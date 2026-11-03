/* snd_nx.c -- sound driver on libnx's audout service.
 *
 * The engine mixes ahead into a ring (shm->buffer), as it does on a sound card.
 * A thread of ours plays the part of the card's DMA: it copies from the ring to
 * the buffers the service plays, and that copying is what moves shm->samplepos.
 *
 * Copyright (C) 2026  HeXen2NX contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */

#include "quakedef.h"
#include "snd_sys.h"

#include <switch.h>

#define NX_SND_RATE		48000	/* all that audout plays */
#define NX_SND_CHANNELS		2
#define NX_SND_BITS		16

#define NX_RING_SAMPLES		(1 << 15)	/* power of two, ~340 ms */
#define NX_CHUNK_FRAMES		1024		/* 21 ms: one page of memory */
#define NX_CHUNK_BYTES		(NX_CHUNK_FRAMES * NX_SND_CHANNELS * (NX_SND_BITS / 8))
#define NX_CHUNKS		3

#define NX_THREAD_STACK		0x4000
#define NX_THREAD_PRIO		0x2B	/* above the engine's 0x2C */
#define NX_THREAD_CORE		2
#define NX_WAIT_NS		20000000ULL

static char		s_nx_driver[] = "NX audout";

static Thread		snd_thread;
static AudioOutBuffer	chunks[NX_CHUNKS];
static void		*chunk_mem;
static s16		*ring;
static volatile bool	snd_running;
static volatile bool	snd_blocked;

static void FillChunk (AudioOutBuffer *chunk)
{
	const int	count = NX_CHUNK_FRAMES * NX_SND_CHANNELS;
	s16		*out = (s16 *) chunk->buffer;
	int		pos = shm->samplepos;
	int		i;

	if (snd_blocked)
	{
		memset(out, 0, NX_CHUNK_BYTES);
	}
	else
	{
		for (i = 0; i < count; i++)
		{
			out[i] = ring[pos];
			pos = (pos + 1) & (NX_RING_SAMPLES - 1);
		}
	}
	shm->samplepos = (shm->samplepos + count) & (NX_RING_SAMPLES - 1);

	armDCacheFlush(chunk->buffer, NX_CHUNK_BYTES);
}

static void SoundThread (void *unused)
{
	(void) unused;

	while (snd_running)
	{
		AudioOutBuffer	*released = NULL;
		u32		count = 0;

		if (R_FAILED(audoutWaitPlayFinish(&released, &count, NX_WAIT_NS)))
			continue;

		/* The wait hands over one buffer, and more may have played. */
		while (count > 0 && released)
		{
			FillChunk(released);
			audoutAppendAudioOutBuffer(released);
			released = NULL;
			count = 0;
			if (R_FAILED(audoutGetReleasedAudioOutBuffer(&released, &count)))
				break;
		}
	}
}

static void FreeMemory (void)
{
	free(chunk_mem);
	chunk_mem = NULL;
	free(ring);
	ring = NULL;
}

static qboolean S_NX_Init (dma_t *dma)
{
	Result	rc;
	int	i;

	rc = audoutInitialize();
	if (R_FAILED(rc))
	{
		Con_Printf("audoutInitialize failed: 0x%x\n", rc);
		return false;
	}

	ring = (s16 *) calloc(NX_RING_SAMPLES, sizeof(s16));
	chunk_mem = aligned_alloc(0x1000, NX_CHUNKS * NX_CHUNK_BYTES);
	if (!ring || !chunk_mem)
	{
		Con_Printf("Failed allocating memory for sound\n");
		FreeMemory();
		audoutExit();
		return false;
	}

	memset ((void *) dma, 0, sizeof(dma_t));
	shm = dma;
	shm->samplebits = NX_SND_BITS;
	shm->speed = NX_SND_RATE;
	shm->channels = NX_SND_CHANNELS;
	shm->samples = NX_RING_SAMPLES;
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char *) ring;

	rc = audoutStartAudioOut();
	if (R_FAILED(rc))
	{
		Con_Printf("audoutStartAudioOut failed: 0x%x\n", rc);
		goto fail;
	}

	for (i = 0; i < NX_CHUNKS; i++)
	{
		chunks[i].next = NULL;
		chunks[i].buffer = (u8 *) chunk_mem + i * NX_CHUNK_BYTES;
		chunks[i].buffer_size = NX_CHUNK_BYTES;
		chunks[i].data_size = NX_CHUNK_BYTES;
		chunks[i].data_offset = 0;
		FillChunk(&chunks[i]);
		audoutAppendAudioOutBuffer(&chunks[i]);
	}

	snd_blocked = false;
	snd_running = true;
	rc = threadCreate(&snd_thread, SoundThread, NULL, NULL, NX_THREAD_STACK, NX_THREAD_PRIO, NX_THREAD_CORE);
	if (R_SUCCEEDED(rc))
		rc = threadStart(&snd_thread);
	if (R_FAILED(rc))
	{
		Con_Printf("Failed starting the sound thread: 0x%x\n", rc);
		audoutStopAudioOut();
		goto fail;
	}

	Con_Printf("NX audout: %d Hz, %d channels, %d ms ring\n", NX_SND_RATE, NX_SND_CHANNELS,
			NX_RING_SAMPLES * 1000 / (NX_SND_RATE * NX_SND_CHANNELS));
	return true;

fail:
	shm = NULL;
	FreeMemory();
	audoutExit();
	return false;
}

static void S_NX_Shutdown (void)
{
	if (!shm)
		return;

	Con_Printf("Shutting down sound\n");
	snd_running = false;
	threadWaitForExit(&snd_thread);
	threadClose(&snd_thread);
	audoutStopAudioOut();
	audoutExit();
	shm->buffer = NULL;
	shm = NULL;
	FreeMemory();
}

static int S_NX_GetDMAPos (void)
{
	return shm->samplepos;
}

static void S_NX_LockBuffer (void)
{
}

static void S_NX_Submit (void)
{
	/* The ring is read by the CPU only: nothing to hand over. */
}

static void S_NX_BlockSound (void)
{
	snd_blocked = true;
}

static void S_NX_UnblockSound (void)
{
	snd_blocked = false;
}

snd_driver_t snddrv_nx =
{
	S_NX_Init,
	S_NX_Shutdown,
	S_NX_GetDMAPos,
	S_NX_LockBuffer,
	S_NX_Submit,
	S_NX_BlockSound,
	S_NX_UnblockSound,
	s_nx_driver,
	SNDDRV_ID_NX,
	false,
	NULL
};

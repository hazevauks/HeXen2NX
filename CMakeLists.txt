cmake_minimum_required(VERSION 3.13)

project(HeXen2NX VERSION 0.1.0 LANGUAGES C ASM)

if(NOT NINTENDO_SWITCH)
	message(FATAL_ERROR "Configure with devkitPro's Switch toolchain: aarch64-none-elf-cmake")
endif()

set(NX_APP_NAME "HeXen II")
set(NX_APP_AUTHOR "hazevauks")
set(NX_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.jpg")

set(UHEXEN2 "${CMAKE_CURRENT_SOURCE_DIR}/third_party/uhexen2")
set(H2 "${UHEXEN2}/engine/hexen2")
set(SHARED "${UHEXEN2}/engine/h2shared")
set(COMMON "${UHEXEN2}/common")

find_library(TREMOR_LIB vorbisidec REQUIRED)
find_library(OGG_LIB ogg REQUIRED)

# The engine's code, the same for any renderer: uHexen2's COMMONOBJS.
set(ENGINE_SOURCES
	${SHARED}/bgmusic.c
	${SHARED}/snd_codec.c
	${SHARED}/snd_wave.c
	${SHARED}/snd_vorbis.c
	${SHARED}/snd_sys.c
	${SHARED}/snd_dma.c
	${SHARED}/snd_mix.c
	${SHARED}/snd_mem.c
	${SHARED}/cd_null.c
	${SHARED}/midi_nul.c
	${H2}/net_bsd.c
	${H2}/net_udp.c
	${H2}/net_dgrm.c
	${H2}/net_loop.c
	${H2}/net_main.c
	${H2}/chase.c
	${H2}/cl_demo.c
	${H2}/cl_effect.c
	${H2}/cl_inlude.c
	${H2}/cl_input.c
	${H2}/cl_main.c
	${H2}/cl_parse.c
	${H2}/cl_string.c
	${H2}/cl_tent.c
	${H2}/cl_cmd.c
	${H2}/console.c
	${H2}/keys.c
	${H2}/menu.c
	${H2}/sbar.c
	${H2}/view.c
	${SHARED}/wad.c
	${SHARED}/cmd.c
	${COMMON}/q_endian.c
	${SHARED}/link_ops.c
	${SHARED}/sizebuf.c
	${COMMON}/strlcat.c
	${COMMON}/strlcpy.c
	${COMMON}/qsnprint.c
	${SHARED}/msg_io.c
	${SHARED}/common.c
	${SHARED}/debuglog.c
	${SHARED}/quakefs.c
	${COMMON}/crc.c
	${SHARED}/cvar.c
	${SHARED}/cfgfile.c
	${H2}/host.c
	${H2}/host_cmd.c
	${SHARED}/host_string.c
	${SHARED}/mathlib.c
	${SHARED}/pr_cmds.c
	${SHARED}/pr_edict.c
	${SHARED}/pr_exec.c
	${H2}/sv_effect.c
	${H2}/sv_main.c
	${H2}/sv_move.c
	${H2}/sv_phys.c
	${H2}/sv_user.c
	${H2}/world.c
	${SHARED}/zone.c
	${SHARED}/hashindex.c
)

# The software renderer: uHexen2's SOFTOBJS.
set(SOFTWARE_SOURCES
	${SHARED}/d_edge.c
	${SHARED}/d_fill.c
	${SHARED}/d_init.c
	${SHARED}/d_modech.c
	${SHARED}/d_part.c
	${SHARED}/d_polyse.c
	${SHARED}/d_scan.c
	${SHARED}/d_sky.c
	${SHARED}/d_sprite.c
	${SHARED}/d_surf.c
	${SHARED}/d_vars.c
	${SHARED}/d_zpoint.c
	${SHARED}/r_aclip.c
	${H2}/r_alias.c
	${SHARED}/r_bsp.c
	${SHARED}/r_draw.c
	${SHARED}/r_edge.c
	${SHARED}/r_efrag.c
	${SHARED}/r_light.c
	${H2}/r_main.c
	${H2}/r_misc.c
	${H2}/r_part.c
	${SHARED}/r_sky.c
	${SHARED}/r_sprite.c
	${SHARED}/r_surf.c
	${SHARED}/r_vars.c
	${SHARED}/screen.c
	${SHARED}/draw.c
	${SHARED}/model.c
)

set(PLATFORM_SOURCES
	source/platform/sys_nx.c
	source/platform/in_nx.c
	source/platform/snd_nx.c
	source/platform/nx_crash.c
	source/platform/nx_data.c
	source/platform/nx_defaults.c
	source/platform/nx_log.c
)

function(hexen2_target name)
	cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES;DEFINES")

	add_executable(${name} ${ENGINE_SOURCES} ${PLATFORM_SOURCES} ${ARG_SOURCES})

	target_include_directories(${name} PRIVATE
		source/platform
		${H2}
		${SHARED}
		${COMMON}
	)
	target_include_directories(${name} SYSTEM PRIVATE ${DEVKITPRO}/portlibs/switch/include)
	target_compile_definitions(${name} PRIVATE
		USE_CODEC_VORBIS
		VORBIS_USE_TREMOR
		USE_CODEC_WAVE
		${ARG_DEFINES}
	)
	target_compile_options(${name} PRIVATE
		-std=gnu11
		-O2
		-g
		-fno-strict-aliasing
		-fno-omit-frame-pointer
		-Wall
		-Wno-trigraphs
		-Wno-unused-result
		-Wno-misleading-indentation
		-Wno-stringop-truncation
		-Wno-format-overflow
		-Wno-format-truncation
		-Wno-array-bounds
		-Wno-maybe-uninitialized
		-Wno-address
		-Wno-dangling-pointer
	)
	target_link_libraries(${name} PRIVATE ${TREMOR_LIB} ${OGG_LIB} m)
	set_target_properties(${name} PROPERTIES OUTPUT_NAME ${name})
endfunction()

# The fallback renderer: boots with nothing but the CPU.
hexen2_target(HeXen2NX_sw
	SOURCES ${SOFTWARE_SOURCES} source/platform/vid_nx.c
)

nx_generate_nacp(HeXen2NX.nacp
	NAME "${NX_APP_NAME}"
	AUTHOR "${NX_APP_AUTHOR}"
	VERSION "${PROJECT_VERSION}"
)

nx_create_nro(HeXen2NX_sw
	NACP HeXen2NX.nacp
	ICON "${NX_ICON}"
)

# What gets copied to the SD card: see tools/package_sd.py for the game's files.
add_custom_target(sd_card ALL
	COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/SD_CARD/switch/HeXen2NX"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"$<TARGET_PROPERTY:HeXen2NX_sw_nro,DKP_FILE>"
		"${CMAKE_CURRENT_SOURCE_DIR}/SD_CARD/switch/HeXen2NX/HeXen2NX.nro"
	DEPENDS HeXen2NX_sw_nro
	COMMENT "Updating SD_CARD/"
	VERBATIM
)

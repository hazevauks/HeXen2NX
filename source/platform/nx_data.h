/* nx_data.h -- puts the player's game files in the layout the engine expects. */

#ifndef NX_DATA_H
#define NX_DATA_H

#include <stdbool.h>

void NX_PrepareGameData (const char *basedir);
bool NX_ConfigIsOurs (const char *path);

#endif	/* NX_DATA_H */

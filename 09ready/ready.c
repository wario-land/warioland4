#include "ready.h"
#include "../gba/gba.h"
#include "../gameutils.h"

int GameReady(void)
{
    // File select screen — returns 1 when done to advance to game
    // For now, auto-advances immediately since save system isn't implemented yet
    return 1;
}

// Scene 11: Angel sparkle sequence — scene11.c
// Post-escape celebration with angel and sparkle effects.
// Scene 11: Angel sparkle sequence
//
// OBJ: Angel sprites, sparkle particles, princess floating

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

extern u16 sLocalSeq;
extern u32 uLocalTime;

void Scene11_Init(void)
{
    REG_DISPCNT = DISPCNT_MODE_0;
}

void Scene11_Exec(int time)
{
    // Angel sequence with sparkles celebrating the escape
    // Simplified: auto-advance after delay

    if (++uLocalTime > 240)
        sGameSeq++;
}

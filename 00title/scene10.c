// Scene 10: Pyramid collapse — scene10.c
// The pyramid crumbles while Wario and cat escape.
// Scene 10: Pyramid collapse
//
// BG: jungle background + light effects
// OBJ: Cat + Wario, treasure chests, smoke, sparkle effects

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

extern u16 sLocalSeq;
extern u32 uLocalTime;

void Scene10_Init(void)
{
    REG_DISPCNT = DISPCNT_MODE_0;
}

void Scene10_Exec(int time)
{
    // Complex scene with treasure chest drop, pyramid collapse,
    // cat and Wario running, sparkle effects.
    //
    // Simplified: auto-advance after delay

    if (++uLocalTime > 300)
        sGameSeq++;
}

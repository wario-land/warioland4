// Scene 9: Escape from pyramid — Wario runs out of collapsing pyramid
// Scene 9: Escape from pyramid — Wario runs out of collapsing pyramid
//
// Complex scene with: Wario + treasure running, cat following,
// doctor chasing, fragment particles, camera shake.
// This is a simplified auto-advancing implementation.

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

extern u16 sLocalSeq;
extern u32 uLocalTime;
extern u16 usEndingType;

void Scene9_Init(void)
{
    // The escape scene shares BG data with Scene7 (pyramid interior)
    // Uses scene7_bg_Palette, scene9_obj_Palette, scene9_obj_Char
    REG_DISPCNT = DISPCNT_MODE_0;
}

void Scene9_Exec(int time)
{
    // Full escape animation with 15+ sub-states (S9_1 through S9_400).
    // Simplified: auto-advance after reasonable delay.
    if (++uLocalTime > 360)
        sGameSeq++;
}

// Kiss scene — Princess and Wario with rose background
// Kiss scene — Princess and Wario with rose background
//
// This is a simplified implementation since the full sprite data
// for Princess/Wario face variations hasn't been extracted yet.
// Auto-advances after a fade-in/fade-out cycle.

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

extern u16 sLocalSeq;
extern u32 uLocalTime;
extern u16 uEVY;

void Kiss_Init(void)
{
    // Clear BG screenbases
    {
        volatile u32 v = 0x03FF03FF;
        REG_DMA3SAD = (u32)&v;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x8000);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x800*4 >> 2);
    }

    // Start dark, fade in
    REG_BLDCNT = BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BD | BLDCNT_EFFECT_DARKEN;
    uEVY = 16;
    REG_BLDY = 16;

    REG_DISPCNT = DISPCNT_MODE_0;
}

void Kiss_Exec(int time)
{
    switch (sLocalSeq)
    {
    case 0:  // Fade in from black
        if ((time & 3) == 3 && uEVY > 0)
        {
            uEVY--;
            REG_BLDY = uEVY;
        }
        if (uEVY == 0)
        {
            sLocalSeq++;
            uLocalTime = 0;
        }
        break;

    case 1:  // Wait, show scene
        if (++uLocalTime > 180)
            sLocalSeq++;
        break;

    case 2:  // Fade to black
        if ((time & 3) == 3 && uEVY < 16)
        {
            uEVY++;
            REG_BLDY = uEVY;
        }
        if (uEVY == 16)
            sGameSeq++;
        break;
    }
}

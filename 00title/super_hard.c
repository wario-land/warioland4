// Super Hard mode message — super_hard.c
// Shows "You can now play Super Hard mode!" after perfect ending.
// Super Hard mode message — shows after perfect ending
//
// Displayed only when all 12 treasures are collected (ucPerfect == 1).

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

extern u16 sLocalSeq;
extern u32 uLocalTime;
extern u16 uEVY;

void SuperHard_Init(void)
{
    // Clear BG screens
    {
        volatile u32 v = 0x03FF03FF;
        REG_DMA3SAD = (u32)&v;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x8000);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x1000 >> 2);
    }

    REG_BLDCNT = BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BD | BLDCNT_EFFECT_DARKEN;
    uEVY = 16;
    REG_BLDY = 16;

    REG_DISPCNT = DISPCNT_MODE_0;
}

void SuperHard_Exec(int time)
{
    // Super Hard sequence:
    // SH_0: Load data, fade in
    // SH_1-3: Show message with animated text

    switch (sLocalSeq)
    {
    case 0:
        // TODO: Load super_hard tiles + message text
        sLocalSeq++;
        break;

    case 1:
        // Fade in from black
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

    case 2:
        // Show message, wait for button press
        if (usTrg & (A_BUTTON | START_BUTTON))
            sGameSeq++;
        break;

    default:
        break;
    }
}

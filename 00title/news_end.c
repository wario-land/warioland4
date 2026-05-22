// Ending newspaper — news_end.c
// Post-credits newspaper with "THE END" message.
// Ending newspaper — post-credits newspaper with "THE END" message
//
// BG: Newspaper with headline text (different from intro newspaper)
// OBJ: "PUSH START" or "THE END" text sprites

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

extern u16 sLocalSeq;
extern u32 uLocalTime;
extern u16  uEVY;

void NewsEnd_Init(void)
{
    // Clear BG screens
    {
        volatile u32 v = 0x03FF03FF;
        REG_DMA3SAD = (u32)&v;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x8000);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x1000 >> 2);
    }

    // Setup dark screen for fade-in
    REG_BLDCNT = BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG1 | BLDCNT_TGT1_BG2
               | BLDCNT_TGT1_OBJ | BLDCNT_TGT1_BD
               | BLDCNT_EFFECT_DARKEN;
    uEVY = 16;
    REG_BLDY = 16;

    REG_DISPCNT = DISPCNT_MODE_0;
}

void NewsEnd_Exec(int time)
{
    // NewsEnd sequence:
    // NEWS_E0: Decompress data
    // NEWS_E1-3: Fade in, show "THE END" text
    // NEWS_E4: "PUSH START" appears for perfect ending
    // NEWS_E5-7: Wait, fade out
    // NEWS_E8: Advance to super hard message or THE_END

    switch (sLocalSeq)
    {
    case 0:
        // TODO: load newspaper ending tiles + OBJ
        sLocalSeq++;
        break;

    case 1:
        // Fade in
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
        // Wait, then advance
        if (++uLocalTime > 120)
            sGameSeq++;
        break;

    default:
        break;
    }
}

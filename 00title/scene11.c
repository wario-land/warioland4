// Scene 11: Angel sparkle sequence — post-escape celebration
//
// Reuses Scene10 BG data (jungle, light, pyramid) plus Scene10 OBJ palette.
// OBJ: Princess floating, angel sprites, sparkle particles
//
// BG0: 256x256 — light effect (scene10_light)
// BG1: 256x256 — jungle (scene10_jungle)
// BG2: 256x256 — pyramid far (pyramid_bg3)

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

// ---- External data (reuses scene10) ----
extern const u16 scene10_obj_Palette[256];
extern const u8  scene10_obj_Char[];
extern const u8  scene10_jungle_Char[];
extern const u8  scene10_light_Char[];
extern const u16 scene10_light[];
extern const u16 scene10_jungle[];

// Reuse pyramid BG
extern const u8  pyramid_bg_Char[];
extern const u16 pyramid_bg3[];

extern u16 S10_GetTreasureY(void);

// ---- Global state ----
extern u16 sLocalSeq;
extern u32 uLocalTime;
extern u32 uLocalTime2;
extern s16 sWork0, sWork1, sWork2, sWork3, sWork4, sWork5, sWork6;
extern s16 ob_pos_x, ob_pos_y;
extern u16 usEndingType;
extern u16 uEVA, uEVB, uEVY;
extern u16 *pObjEnd;
extern u32 uObjSize;

void Scene11_Init(void)
{
    // DMA OBJ palette (reuses scene10)
    REG_DMA3SAD = (u32)scene10_obj_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (256*2 >> 2);

    // Decompress BG tiles
    LZ77UnCompVram((const u32 *)pyramid_bg_Char, (void *)BG_VRAM);
    LZ77UnCompVram((const u32 *)scene10_jungle_Char,
                   (void *)((u8 *)BG_VRAM + 0x220 * 32));
    LZ77UnCompVram((const u32 *)scene10_light_Char,
                   (void *)((u8 *)BG_VRAM + 0x340 * 32));
    LZ77UnCompVram((const u32 *)scene10_obj_Char, (void *)OBJ_VRAM0);

    // Clear tile 0x3FF
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x7FE0);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (32 >> 2);
    }

    // BG tilemaps
    // BG0: light effect (screenbase 16)
    UnPackScreen((const u16 *)scene10_light, (vu16 *)((u8 *)BG_VRAM + 0x8000));
    // BG1: jungle (screenbase 17)
    UnPackScreen((const u16 *)scene10_jungle, (vu16 *)((u8 *)BG_VRAM + 0x8800));
    // BG2: pyramid (screenbase 18)
    UnPackScreen((const u16 *)pyramid_bg3, (vu16 *)((u8 *)BG_VRAM + 0x9000));

    // BG control registers
    REG_BG0CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);
    REG_BG1CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(18) | BGCNT_CHARBASE(0);
    REG_BG2CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(2)
               | BGCNT_SCREENBASE(19) | BGCNT_CHARBASE(0);

    // Initial scroll
    REG_BG0HOFS = 0; REG_BG0VOFS = 0;
    REG_BG1HOFS = 0; REG_BG1VOFS = 0;
    REG_BG2HOFS = 0; REG_BG2VOFS = 0;

    // Princess start position
    ob_pos_x = 136;
    ob_pos_y = 144;
    sWork0 = 96;   // princess X
    sWork1 = 144;  // princess Y
    sWork2 = 184;  // treasure X
    sWork3 = S10_GetTreasureY();  // treasure Y
    sWork4 = sWork3 - 4;  // treasure top Y

    // Treasure fall speed based on ending type
    if (usEndingType == 0 || usEndingType == 1)
        sWork5 = usEndingType;
    else if (usEndingType == 2)
        sWork5 = 3;
    else
        sWork5 = 7;

    uLocalTime2 = 0;
    sWork6 = 0;  // priority flag

    // Start white
    uEVA = 0;
    uEVB = 16;

    V_Wait();
    SetBLDUpMax(BLDCNT_TGT1_BD | BLDCNT_TGT1_BG2);
    REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_BG2_ON;
}

void Scene11_Exec(int time)
{
    u16 *dst = (u16 *)OamBuf;

    switch (sLocalSeq)
    {
    case 0:
        // Fade in, princess descends
        FadeDec(7);

        if (sWork1 > sWork4 && (time & 3) == 3)
            sWork1--;  // princess floats down with treasure

        if (++uLocalTime > 120)
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 1:
        // Princess and treasure visible, sparkle effects
        if (++uLocalTime > 180)
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 2:
        // Fade to white, advance
        if (FadeInc(3))
            sGameSeq++;
        break;

    default:
        break;
    }

    // ---- OBJ rendering ----
    // Princess OBJ
    {
        static const u16 princess[] = { 1, 0x4080, 0x8050, 0x0400 };
        dst = SetObj(princess, dst, sWork0, sWork1);
    }

    // Angel sparkles (8 positions, animated)
    if (sLocalSeq >= 1)
    {
        static const u16 sparkle[] = { 1, 0x40A0, 0x8060, 0x0500 };
        static const s16 sx[] = {16, 24, 80, 184, 224, 216, 72, 152};
        static const s16 sy[] = {16, 72, 152, 136, 112, 24, 44, 96};
        int i;
        for (i = 0; i < 8; i++)
            dst = SetObj(sparkle, dst, sx[i], sy[i]);
    }

    EndObj(dst);
}

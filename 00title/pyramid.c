// Pyramid intro — car drives toward pyramid entrance
// Pyramid intro — car drives toward pyramid entrance
//
// BG layers:
//   BG0: desert ground (repeating pyramid_bg0 tilemap, priority 1)
//   BG1: near pyramid  (256x256, screenbase 17, priority 0)
//   BG2: mid pyramid   (256x256, screenbase 18, priority 2)
//   BG3: far pyramid   (256x256, screenbase 19, priority 3)
//
// OBJ: Wario with treasure (scaled), rocks left+right, grass, smoke

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

extern const u16 pyramid_bg_Palette[64];
extern const u8  pyramid_bg_Char[];
extern const u16 pyramid_bg0[];
extern const u16 pyramid_bg1[];
extern const u16 pyramid_bg2[];
extern const u16 pyramid_bg3[];
extern const u16 pyramid_obj_Palette[48];
extern const u8  pyramid_obj_Char[];

// OBJ sprite patterns — use placeholder since data not yet extracted
static const u16 pyramid_iwa_placeholder[] = { 1, 0x4000, 0x8000, 0x0000 };

extern u16 sLocalSeq;
extern u32 uLocalTime;
extern s16 sWork0, sWork1, sWork2, sWork3, sWork4, sWork5, sWork6, sWork7;
extern s16 bg_scroll_x, bg_scroll_y;
extern s16 bg_offset_x, bg_offset_y;
extern s16 ob_pos_x, ob_pos_y;
extern u16 ob_rotate;
extern u16 ob_scale_x, ob_scale_y;
extern u16 ob_palette;
extern u16 uEVA, uEVB, uEVY;
extern u16 *pObjEnd;
extern u32 uObjSize;

// Local smoke position arrays (not in IWRAM to save space)
static s16 pyramid_smoke_x[8], pyramid_smoke_y[8];

void Pyramid_Init(void)
{
    int i;
    u16 *dst;

    // Copy palettes
    REG_DMA3SAD = (u32)pyramid_bg_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (64*2 >> 2);
    REG_DMA3SAD = (u32)pyramid_obj_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (48*2 >> 2);

    // Clear tile 0x3FC (white tile)
    {
        volatile u32 f = 0xFFFFFFFF;
        REG_DMA3SAD = (u32)&f;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 32*1020);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (32 >> 2);
    }

    LZ77UnCompVram((const u32 *)pyramid_bg_Char, (void *)BG_VRAM);
    LZ77UnCompVram((const u32 *)pyramid_obj_Char, (void *)OBJ_VRAM0);

    // BG0: repeat pyramid_bg0 across 18 rows
    dst = (vu16 *)((u8 *)BG_VRAM + 0x8000);
    for (i = 0; i < 18; i++, dst += 32)
    {
        REG_DMA3SAD = (u32)pyramid_bg0;
        REG_DMA3DAD = (u32)dst;
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (32*2 >> 2);
    }

    UnPackScreen((const u16 *)pyramid_bg1, (vu16 *)((u8 *)BG_VRAM + 0x8800));
    UnPackScreen((const u16 *)pyramid_bg2, (vu16 *)((u8 *)BG_VRAM + 0x9000));
    UnPackScreen((const u16 *)pyramid_bg3, (vu16 *)((u8 *)BG_VRAM + 0x9800));

    // Copy end-of-screenbase for seamless BG3 scroll wrapping
    dst = (vu16 *)((u8 *)BG_VRAM + 0x9800);
    REG_DMA3SAD = (u32)dst;
    REG_DMA3DAD = (u32)(dst + 464);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (30*2 >> 2);
    REG_DMA3SAD = (u32)dst;
    REG_DMA3DAD = (u32)(dst + 480);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (30*2 >> 2);
    REG_DMA3SAD = (u32)dst;
    REG_DMA3DAD = (u32)(dst + 496);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (30*2 >> 2);

    // BG control registers
    REG_BG0CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);
    REG_BG1CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(17) | BGCNT_CHARBASE(0);
    REG_BG2CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(2)
               | BGCNT_SCREENBASE(18) | BGCNT_CHARBASE(0);
    REG_BG3CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(3)
               | BGCNT_SCREENBASE(19) | BGCNT_CHARBASE(0);

    REG_DISPCNT = DISPCNT_MODE_0;
}

void Pyramid_Exec(int time)
{
    u16 *dst;
    int i;

    switch (sLocalSeq)
    {
    case 0:  // S6_1: Setup initial positions
        ob_rotate = 0;
        ob_scale_x = 0x140;
        ob_scale_y = 0x140;
        ob_pos_x = 120;
        ob_pos_y = 180;
        sWork6 = ob_pos_x;  // smoke X
        sWork7 = ob_pos_y + (s16)((s32)16 * ob_scale_y / 256);  // smoke Y
        for (i = 0; i < 4; i++)
        {
            pyramid_smoke_y[i] = ob_pos_y + (s16)((s32)16 * ob_scale_y / 256) + i * 6;
            pyramid_smoke_x[i] = ob_pos_x;
        }

        sWork0 = 0;    // bg1_y
        sWork1 = -24;  // bg2_y
        REG_BG2VOFS = sWork1;
        REG_BG3VOFS = sWork1;
        sWork2 = 120;  // iwaL_x
        sWork3 = 64;   // iwaL_y
        sWork4 = 120;  // iwaR_x
        sWork5 = 64;   // iwaR_y

        uLocalTime = 0;
        sLocalSeq++;
        break;

    case 1:  // S6_2: Blend setup, wait
        if (uLocalTime == 0)
        {
            uEVA = 17;
            uEVB = 0;
            REG_BLDALPHA = 16;  // EVA=16, EVB=0
            REG_BLDCNT = BLDCNT_TGT2_BG2 | BLDCNT_TGT2_BG3
                       | BLDCNT_EFFECT_BLEND | BLDCNT_TGT1_BG0;
            REG_DISPCNT |= DISPCNT_OBJ_ON | DISPCNT_BG0_ON
                        | DISPCNT_BG1_ON | DISPCNT_BG2_ON | DISPCNT_BG3_ON;
        }
        else if (uLocalTime >= 125)
        {
            uLocalTime = 0;
            sLocalSeq++;
            break;
        }
        uLocalTime++;
        break;

    case 2:  // S6_3: Rocks spread, Wario shrinks, BG scrolls
        // Rocks move apart
        if (sWork2 > 64 && (time & 3) == 3)
        {
            sWork2--;  // iwaL_x
            sWork4++;  // iwaR_x
        }
        // Rocks + ground move up
        if (sWork3 < 72 && (time & 31) == 31)
        {
            sWork3++;  // iwaL_y
            sWork5++;  // iwaR_y
            REG_BG1VOFS = --sWork0;  // bg1_y
        }
        // Wario shrinks and moves up
        if (ob_pos_y > 116 && (time & 3) == 3)
        {
            ob_pos_y--;
            ob_scale_x--;
            ob_scale_y--;
        }
        // Smoke
        for (i = 0; i < 4; i++)
        {
            pyramid_smoke_y[i] = ob_pos_y + (s16)((s32)16 * ob_scale_y / 256) + i * 6;
        }

        if (ob_scale_x == 0x100)
        {
            ob_pos_y += 18;
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 3:  // S6_4: Wait pose, BG scroll, blend transition
        if (uLocalTime >= 32 && uLocalTime < 96 && (uLocalTime & 7) == 7)
        {
            REG_BG1VOFS = --sWork0;
            REG_BG2VOFS = ++sWork1;
            REG_BG3VOFS = sWork1;
            sWork3++;
            sWork5++;
            ob_pos_y++;
        }
        uLocalTime++;

        if (uEVA)
        {
            if ((time & 7) == 7)
                REG_BLDALPHA = (++uEVB << 8) | --uEVA;
        }
        else
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 4:  // S6_5: Wario happy pose (pyramid_Anime0)
        uLocalTime++;
        if (uLocalTime == 32)
        {
            // WarioVoiceSet(WV_HAPPY_2);
        }
        if (uLocalTime > 50)
        {
            sLocalSeq++;
        }
        break;

    case 5:  // S6_6: Wait pose, advance
        if (++uLocalTime > 110)
        {
            ob_pos_y -= 18;
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 6:  // S6_7: Exit — shrink out, fade to white
        if ((time & 3) == 3)
        {
            if (ob_pos_y < 160)
            {
                ob_pos_y++;
                ob_scale_x -= 4;
                ob_scale_y -= 4;
            }
            else
            {
                SetBLDUpMin(BLDCNT_TGT1_BD | BLDCNT_TGT1_OBJ
                          | BLDCNT_TGT1_BG3 | BLDCNT_TGT1_BG2
                          | BLDCNT_TGT1_BG1);
                REG_DISPCNT = DISPCNT_OBJ_ON | DISPCNT_BG3_ON
                            | DISPCNT_BG2_ON | DISPCNT_BG1_ON;
                uLocalTime = 0;
                sLocalSeq++;
            }
        }
        sWork7 = ob_pos_y + (s16)((s32)16 * ob_scale_y / 256);
        break;

    case 7:  // S6_8: Fade to white, advance
        FadeInc(7);
        if (uEVY == 16)
            sGameSeq++;
        break;
    }

    // ---- OBJ rendering ----
    dst = (u16 *)OamBuf;

    // Rocks + grass (visible from state 1) — placeholder OBJs
    if (sLocalSeq > 0)
    {
        dst = SetObj(pyramid_iwa_placeholder, dst, sWork2, sWork3);
        dst = SetObj(pyramid_iwa_placeholder, dst, sWork4, sWork5);
    }

    // Smoke particles — placeholder
    if (sLocalSeq >= 2 && sLocalSeq < 6)
    {
        for (i = 0; i < 4; i++)
            dst = SetObj(pyramid_iwa_placeholder, dst, sWork6, pyramid_smoke_y[i]);
    }

    EndObj(dst);
}

// Scene 10: Pyramid collapse — Wario and cat escape the crumbling pyramid
//
// BG layers:
//   BG0: 256x256 16-color — jungle background (scene10_jungle)
//   BG1: 256x256 16-color — mid pyramid (pyramid_bg2, reused)
//   BG2: 256x256 16-color — far pyramid (pyramid_bg3, reused)
//   BG3: 256x256 16-color — texture overlay (scene10_texture)
//
// OBJ: Cat+Wario, treasure, smoke, sparkle, princess sequence
//
// States (sLocalSeq):
//   0: Fade in from white, pyramid rises, Wario runs out
//   1: Cat+Wario run animation loop
//   2: Transition to next scene

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

// ---- External data ----
extern const u16 scene10_obj_Palette[256];
extern const u8  scene10_obj_Char[];
extern const u8  scene10_jungle_Char[];
extern const u8  scene10_light_Char[];
extern const u16 scene10_jungle[];
extern const u16 scene10_texture[];

// Reuse pyramid BG data
extern const u8  pyramid_bg_Char[];
extern const u16 pyramid_bg2[];
extern const u16 pyramid_bg3[];

extern u16 S10_GetTreasureY(void);

// ---- Global state ----
extern u16 sLocalSeq;
extern u32 uLocalTime;
extern u32 uLocalTime2;
extern s16 sWork0, sWork1, sWork2, sWork3, sWork4, sWork5, sWork6;
extern s16 ob_pos_x, ob_pos_y;
extern u16 ob_rotate;
extern u16 ob_scale_x, ob_scale_y;
extern u16 uEVY;
extern u16 *pObjEnd;
extern u32 uObjSize;

void Scene10_Init(void)
{
    // DMA OBJ palette (256 entries = 512 bytes)
    REG_DMA3SAD = (u32)scene10_obj_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (256*2 >> 2);

    // Decompress BG tiles to VRAM:
    // pyramid_bg_Char -> tiles 0-511 (BG_VRAM base)
    LZ77UnCompVram((const u32 *)pyramid_bg_Char, (void *)BG_VRAM);
    // scene10_jungle_Char -> tiles 0x220+ (BG_VRAM + 0x220*32)
    LZ77UnCompVram((const u32 *)scene10_jungle_Char, (void *)((u8 *)BG_VRAM + 0x220 * 32));
    // scene10_light_Char -> tiles 0x340+ (BG_VRAM + 0x340*32)
    LZ77UnCompVram((const u32 *)scene10_light_Char, (void *)((u8 *)BG_VRAM + 0x340 * 32));
    // scene10_obj_Char -> OBJ VRAM
    LZ77UnCompVram((const u32 *)scene10_obj_Char, (void *)OBJ_VRAM0);

    // Clear tile 0x3FF (blank white tile)
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x7FE0);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (32 >> 2);
    }

    // ---- UnPackScreen tilemaps ----
    // BG0: jungle (screenbase 16 = 0x8000)
    UnPackScreen((const u16 *)scene10_jungle, (vu16 *)((u8 *)BG_VRAM + 0x8000));
    // BG1: pyramid mid (screenbase 17 = 0x8800)
    UnPackScreen((const u16 *)pyramid_bg2, (vu16 *)((u8 *)BG_VRAM + 0x8800));
    // BG2: pyramid far (screenbase 18 = 0x9000)
    UnPackScreen((const u16 *)pyramid_bg3, (vu16 *)((u8 *)BG_VRAM + 0x9000));
    // BG3: texture (screenbase 19 = 0x9800)
    UnPackScreen((const u16 *)scene10_texture, (vu16 *)((u8 *)BG_VRAM + 0x9800));

    // ---- BG control registers ----
    // BG0: 256x256 16-color, priority 0, screenbase 16
    REG_BG0CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);
    // BG1: 256x256 16-color, priority 1, screenbase 17
    REG_BG1CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(17) | BGCNT_CHARBASE(0);
    // BG2: 256x256 16-color, priority 2, screenbase 18
    REG_BG2CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(2)
               | BGCNT_SCREENBASE(18) | BGCNT_CHARBASE(0);
    // BG3: 256x256 16-color, priority 3, screenbase 19
    REG_BG3CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(3)
               | BGCNT_SCREENBASE(19) | BGCNT_CHARBASE(0);

    // Start white (fade in from white)
    SetBLDUpMax(BLDCNT_TGT1_BD | BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG1
              | BLDCNT_TGT1_BG2 | BLDCNT_TGT1_BG3 | BLDCNT_TGT1_OBJ);

    // Pyramid starts offscreen (below frame)
    sWork0 = -16;

    // Initial BG scroll
    REG_BG0VOFS = 0;
    REG_BG2VOFS = sWork0;
    REG_BG2HOFS = 0;
    REG_BG3HOFS = 0;

    // Treasure Y position by ending type
    sWork3 = S10_GetTreasureY();

    // Wario+cat start positions
    ob_pos_x = 136;
    ob_pos_y = 144;
    sWork1 = 136;  // cat X
    sWork2 = 144;  // cat Y

    sWork4 = 0;  // accessories done flag
    sWork5 = 0;  // accessories count
    sWork6 = 0;  // Wario surprised flag

    V_Wait();

    // WIN1: screen-wide window for effects
    REG_WIN1H = WIN_RANGE(0, 240);
    REG_WIN1V = WIN_RANGE(8, 63);
    REG_WINOUT = WINOUT_WIN01_BG0 | WINOUT_WIN01_BG1 | WINOUT_WIN01_BG2
               | WINOUT_WIN01_BG3 | WINOUT_WIN01_OBJ;

    // Enable display: Mode 0, all BGs, OBJ, WIN1
    REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_OBJ_ON
                | DISPCNT_BG0_ON | DISPCNT_BG1_ON
                | DISPCNT_BG2_ON | DISPCNT_BG3_ON
                | DISPCNT_WIN1_ON;
}

void Scene10_Exec(int time)
{
    u16 *dst = (u16 *)OamBuf;

    switch (sLocalSeq)
    {
    case 0:
        // Fade in from white (BLD_UP decrease)
        FadeDec(15);

        // Pyramid scrolls up from below (sWork0 = pyramid Y, starts at -16, target 0)
        if (sWork0 <= -120)
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        else if ((time & 7) == 7)
        {
            sWork0--;
            REG_BG2VOFS = sWork0;
        }

        // Horizontal camera shake
        REG_BG1HOFS = (time & 3) - 1;
        break;

    case 1:
        // Main animation: Wario runs right, cat follows
        {
            // Advance scene after ~360 frames
            if (uLocalTime > 360)
            {
                SetBLDDownMin(BLDCNT_TGT1_BD | BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG1
                            | BLDCNT_TGT1_BG2 | BLDCNT_TGT1_BG3 | BLDCNT_TGT1_OBJ);
                sLocalSeq++;
                uLocalTime = 0;
            }

            // Pyramid scrolls into position
            if (sWork0 < 0 && (time & 3) == 3)
            {
                sWork0++;
                REG_BG2VOFS = sWork0;
            }

            // Cat runs right
            if ((time & 1) && sWork1 < 240)
                sWork1++;

            uLocalTime++;
        }
        break;

    case 2:
        // Fade to black, advance
        if (FadeInc(3))
            sGameSeq++;
        break;

    default:
        break;
    }

    // ---- OBJ rendering ----
    // Wario OBJ (always visible)
    {
        static const u16 wario_run[] = { 1, 0x4080, 0x8088, 0x0200 };
        dst = SetObj(wario_run, dst, 120, ob_pos_y);
    }

    // Cat OBJ (visible during animation)
    if (sLocalSeq == 1)
    {
        static const u16 cat_run[] = { 1, 0x4090, 0x8080, 0x0220 };
        dst = SetObj(cat_run, dst, sWork1, sWork2);
    }

    // Treasure OBJ (appears at sWork3 Y position)
    if (sLocalSeq == 1 && uLocalTime > 60)
    {
        static const u16 treasure[] = { 1, 0x4040, 0x80B8, 0x0300 };
        dst = SetObj(treasure, dst, 184, sWork3);
    }

    EndObj(dst);
}

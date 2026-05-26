// Scene 4: Highway + Cat -- Mode 4 bitmap highway with OBJ animations
//
// Mode 4 provides a 240x160 8-bit indexed bitmap background (the highway).
// OBJ sprites overlay the car, cat, paper plane, and buildings.
//
// Animation sequence (corrected from IDA decompilation):
//   S4_0: Decompress bitmap + OBJ tiles to VRAM
//   S4_1: Setup positions, darken, enable MODE_4 + OBJ + BG2
//   S4_2: Cat runs from right (x=280) to center (x=119). Anime0. BG scrolls.
//   S4_3: Cat crouching/idle. Anime2. (30 frames)
//   S4_4: Cat looking around. Anime3. (30 frames)
//   S4_5: Cat SHAKING. Anime10. Car shrinks from 2x -> 0. Car palette fades.
//   S4_6: Cat still shaking. Anime10 + paper floating (Anime8). Paper flies to cat.
//   S4_7: Cat flies up with paper (Anime7, 63-frame loop)
//   S4_8: Cat swooping. Anime9 + Anime5. Paper sinks every 4th frame.
//   S4_9: Cat diving. Anime9 + Anime4.
//   S4_10: Final dive. Anime9 + Anime6. Advances scene on completion.

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

// ---- Scene4 data ----
extern const u16 scene4_bg_Palette[17];
extern const u16 scene4_obj_Palette[208];
extern const u16 scene4_obj_Ending_cat_Palette[16];
extern const u8  scene4_Bitmap[];
extern const u8  scene4_obj_Char[];
extern const u16 scene4_obj_bldg[];

// ---- Global state ----
extern u8   bEnding;
extern u16  sLocalSeq;
extern u32  uLocalTime;
extern u32  uLocalTime2;
extern s16  sWork0, sWork1, sWork2, sWork3;
extern s16  bg_offset_x, bg_offset_y;
extern s16  bg_scale_x, bg_scale_y;
extern u16  ob_pos_x, ob_pos_y;
extern u16  ob_scale_x, ob_scale_y;
extern u16  ob_palette, ob_priority;
extern u16  uEVY;
extern u16 *pObjEnd;
extern u32  uObjSize;

void Scene4_Init(void)
{
    // === BG palette (17 entries) -> BG_PLTT (Mode 4 bitmap palette) ===
    // From IDA disassembly: DMA_16BIT, count=17 halfwords (0x80000011)
    REG_DMA3SAD = (u32)scene4_bg_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_16BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | 17;

    // === OBJ palette (208 entries) -> OBJ_PLTT ===
    // From IDA disassembly: DMA_16BIT, count=208 halfwords (0x800000D0)
    REG_DMA3SAD = (u32)scene4_obj_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_16BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | 208;

    // Ending mode: alternative cat palette at OBJ row 4 (offset 0x40)
    if (bEnding)
    {
        REG_DMA3SAD = (u32)scene4_obj_Ending_cat_Palette;
        REG_DMA3DAD = (u32)((u16 *)OBJ_PLTT + 32);  // 32 halfwords = row 4
        REG_DMA3CNT = ((DMA_ENABLE | DMA_16BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | 16;
    }

    // === Clear Mode 4 framebuffer edges (matching IDA disassembly) ===
    // Clear VRAM+0x0000 (3840 bytes = top 16 lines of frame 0)
    // Clear VRAM+0x8700 (3840 bytes = bottom 16 lines of frame 0)
    // DMA_16BIT, SRC_FIXED, count=1920 halfwords (0x81000780)
    {
        volatile u16 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)BG_VRAM;
        REG_DMA3CNT = ((DMA_ENABLE | DMA_16BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | 1920;
    }
    {
        volatile u16 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x8700);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_16BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | 1920;
    }

    // Car OBJ initial affine params
    ob_pos_x = 88;
    ob_pos_y = 120;
    ob_scale_x = 0x200;   // 2.0x
    ob_scale_y = 0x200;

    REG_DISPCNT = DISPCNT_MODE_4;
}

void Scene4_Exec(int time)
{
    u16 *dst;
    const u16 *cat_pattern = 0;
    const u16 *paper_pattern = 0;
    int done;

    switch (sLocalSeq)
    {
    case 0:
        // === S4_0: Decompress bitmap + OBJ tiles ===
        // Bitmap -> BG_VRAM + 0xF00 (skip 16-line top margin)
        // OBJ tiles -> OBJ_VRAM1 (0x6014000 -- Mode 4 uses VRAM 0-0x13FFF for bitmap)
        LZ77UnCompVram((const u32 *)scene4_Bitmap,
                       (void *)((u8 *)BG_VRAM + 16 * 240));
        LZ77UnCompVram((const u32 *)scene4_obj_Char, (void *)OBJ_VRAM1);
        uLocalTime = 0;
        sLocalSeq++;
        break;

    case 1:
        // === S4_1: Setup, darken, enable display ===
        V_Wait();

        // Cat starts at far right
        sWork0 = 280;
        sWork1 = 144;

        // BG vertical offset starts at -8 (scrolls down during animation)
        bg_offset_y = -8;

        // Max darken: screen starts black
        SetBLDDownMax(BLDCNT_TGT1_BD | BLDCNT_TGT1_OBJ | BLDCNT_TGT1_BG2);

        REG_DISPCNT = DISPCNT_MODE_4 | DISPCNT_OBJ_ON | DISPCNT_BG2_ON;

        sLocalSeq++;
        uLocalTime = 0;
        break;

    case 2:
        // === S4_2: Cat runs from right, BG scrolls (Anime0) ===
        FadeDec(15);
        uLocalTime++;

        scene4_Anime0(time, &cat_pattern);

        // BG scroll at specific frames
        switch (uLocalTime)
        {
        case 150: case 160: case 170: case 180:
        case 190: case 200: case 210: case 220:
            bg_offset_y++;
            break;
        }

        // Cat moves left every odd frame. Stops at x=120 (center).
        // wl4leaks: s4_cat_x-- == LCD_CENTER_X (post-decrement, compare to 120)
        if ((uLocalTime & 1) == 0)
            break;
        if (sWork0-- != 120)
            break;

        // Cat reached center: advance state
        sLocalSeq++;
        uLocalTime = 0;
        uLocalTime2 = 0;
        REG_BLDY = 3;
        REG_BLDCNT = BLDCNT_EFFECT_LIGHTEN | BLDCNT_TGT1_BD
                    | BLDCNT_TGT1_OBJ | BLDCNT_TGT1_BG2;
        break;

    case 3:
        // === S4_3: Cat crouching/idle (Anime2, 30+ frames) ===
        done = scene4_Anime2(uLocalTime++, &cat_pattern);
        if (done) { sLocalSeq++; uLocalTime = 0; }
        break;

    case 4:
        // === S4_4: Cat looking around (Anime3, 30+ frames) ===
        done = scene4_Anime3(uLocalTime++, &cat_pattern);
        if (done) { sLocalSeq++; uLocalTime = 0; }
        break;

    case 5:
        // === S4_5: Cat SHAKING (Anime10), car shrinks, car palette fades ===
        scene4_Anime10(time, &cat_pattern);

        // Car palette decreases every 4th frame
        if ((uLocalTime & 3) == 3 && ob_palette > 4)
            ob_palette--;

        // Car shrinks from 2x to near 0
        if (ob_scale_x > 0x10)
        {
            ob_scale_x -= 0x10;
            ob_scale_y -= 0x10;
        }
        else
        {
            // Car has shrunk: drop OBJ behind BG
            ob_priority = 3;
            // Move car downward if still on screen
            if (ob_pos_y < 128)
            {
                if ((uLocalTime & 7) == 7)
                    ob_pos_y++;
            }
            else
            {
                // Car offscreen: paper plane flies toward cat
                sLocalSeq++;
                uLocalTime = -1;
                sWork2 = sWork0 - 60;
                sWork3 = sWork1 + 60;
            }
        }
        uLocalTime++;
        break;

    case 6:
        // === S4_6: Cat shaking (Anime10) + paper floats (Anime8) toward cat ===
        scene4_Anime10(time, &cat_pattern);
        scene4_Anime8(uLocalTime++, &paper_pattern);

        // Paper flies diagonally toward cat
        if (sWork2 < sWork0) sWork2++;
        if (sWork3 > sWork1) sWork3--;
        if (sWork2 == sWork0 && sWork3 == sWork1)
        {
            sLocalSeq++;
            uLocalTime = 0;
        }
        break;

    case 7:
        // === S4_7: Cat flies up with paper (Anime7, 63-frame cycle) ===
        done = scene4_Anime7(uLocalTime++, &cat_pattern);
        if (done) { sLocalSeq++; uLocalTime = 0; sWork3 -= 2; }
        break;

    case 8:
        // === S4_8: Cat swooping -- Anime9(paper glide) + Anime5(cat flying) ===
        // Paper Y sinks every 4th frame
        {
            int tClamped = (uLocalTime > 54) ? 54 : uLocalTime;
            scene4_Anime9(tClamped, &paper_pattern);
        }
        if ((uLocalTime & 3) == 3) sWork3++;
        done = scene4_Anime5(uLocalTime++, &cat_pattern);
        if (done) { sLocalSeq++; uLocalTime = 0; }
        break;

    case 9:
        // === S4_9: Cat diving -- Anime9(paper glide) + Anime4(cat catching) ===
        scene4_Anime9(54, &paper_pattern);
        done = scene4_Anime4(uLocalTime++, &cat_pattern);
        if (done) { sLocalSeq++; uLocalTime = 0; }
        break;

    case 10:
        // === S4_10: Final dive -- Anime9 + Anime6 ===
        scene4_Anime9(54, &paper_pattern);
        done = scene4_Anime6(uLocalTime++, &cat_pattern);
        if (done) sGameSeq++;
        break;
    }

    // ---- BG zoom-in after car shrinks ----
    if (sLocalSeq > 4 && (time & 15) == 15 && bg_scale_x <= 0x11F)
    {
        bg_scale_x++;
        bg_scale_y++;
    }

    // ---- Brightness sequence (uLocalTime2-based, starts in S4_3+) ----
    if (sLocalSeq > 2)
    {
        switch (uLocalTime2)
        {
        case 15:  REG_BLDY = 4;  break;
        case 30:  REG_BLDY = 5;  break;
        case 45:  REG_BLDY = 6;  break;
        case 61:  REG_BLDY = 7;  break;
        case 77:  REG_BLDY = 8;  break;
        case 78:  REG_BLDY = 16; break;   // Flash!
        case 81:  REG_BLDY = 15; break;
        case 84:  REG_BLDY = 14; break;
        case 87:  REG_BLDY = 13; break;
        case 90:  REG_BLDY = 12; break;
        case 93:  REG_BLDY = 11; break;
        case 96:  REG_BLDY = 10; break;
        case 99:  REG_BLDY = 9;  break;
        case 102: REG_BLDY = 8;  break;
        case 105: REG_BLDY = 7;  break;
        case 107: REG_BLDY = 6;  break;
        case 109: REG_BLDY = 5;  break;
        case 111: REG_BLDY = 4;  break;
        case 113: REG_BLDY = 3;  break;
        case 115: REG_BLDY = 2;  break;
        case 117: REG_BLDY = 1;  break;
        case 119: REG_BLDY = 0;  break;
        }
        uLocalTime2++;
    }

    // ---- OBJ rendering (matching IDA Scene4_Exec) ----
    if (sLocalSeq >= 2)
    {
        dst = (u16 *)OamBuf;

        // Update affine params for car sprite scaling (no rotation)
        SetObjPABCD(0, 0, ob_scale_x, ob_scale_y);

        // Car OBJ: rendered in front of cat when scale > 0x80,
        // behind cat when scale < 0x80
        if (sLocalSeq == 5 && ob_scale_x > 0x80)
            dst = Scene4_SetCarObj(dst);

        // Paper plane (rendered after cat takes off, S4_5+)
        if (sLocalSeq > 5 && paper_pattern)
            dst = SetObj(paper_pattern, dst, sWork2, sWork3);

        // Cat OBJ
        if (cat_pattern)
            dst = SetObj(cat_pattern, dst, sWork0, sWork1 - bg_offset_y);

        // Car OBJ: behind cat when shrunk
        if (sLocalSeq == 5 && ob_scale_x < 0x80)
            dst = Scene4_SetCarObj(dst);

        // Building OBJ overlay (always visible)
        dst = SetObj(scene4_obj_bldg, dst, 120, bg_offset_y + 96);

        EndObj(dst);
    }

    // ---- BG2 vertical scroll ----
    if (sLocalSeq >= 2)
    {
        REG_BG2VOFS = bg_offset_y;
        REG_BG2HOFS = 0;
    }
}

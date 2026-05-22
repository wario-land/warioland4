// Scene 4: Highway + Cat — bitmap mode highway with OBJ animations
// Scene 4: Highway + Cat — bitmap mode highway with OBJ animations
//
// Mode 4 bitmap background (highway scrolling)
// OBJ: cat (multiple animation states), car (affine scaled), paper plane, buildings

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

// ---- Scene4 data references ----
extern const u16 scene4_bg_Palette[17];
extern const u8  scene4_Bitmap[];
extern const u16 scene4_obj_Palette[208];
extern const u16 scene4_obj_Ending_cat_Palette[16];
extern const u8  scene4_obj_Char[];
extern const u16 scene4_obj_bldg[];

// Cat OBJ patterns (from scene4_cat_data.c)
extern const u16 scene4_cat_000[];
extern const u16 scene4_cat_001[];
extern const u16 scene4_cat_002[];
extern const u16 scene4_cat_003[];
extern const u16 scene4_cat_005[];
extern const u16 scene4_cat_006[];
extern const u16 scene4_cat_009[];

// Cat animation function — matches scene4_Anime0 from IDA (0x800BC0C)
// Cycles through cat running frames 0-6 based on time % 72.
// Returns TRUE when animation cycle completes (time % 72 == 71).
static int CatAnime0(int time, const u16 **pattern)
{
    int t = time % 72;
    if (t <= 5)       *pattern = scene4_cat_000;
    else if (t <= 11)  *pattern = scene4_cat_001;
    else if (t <= 17)  *pattern = scene4_cat_002;
    else if (t <= 23)  *pattern = scene4_cat_003;
    else if (t <= 29)  *pattern = scene4_cat_000;
    else if (t <= 35)  *pattern = scene4_cat_005;
    else if (t <= 41)  *pattern = scene4_cat_006;
    else if (t <= 47)  *pattern = scene4_cat_003;
    else if (t <= 53)  *pattern = scene4_cat_000;
    else if (t <= 59)  *pattern = scene4_cat_001;
    else if (t <= 65)  *pattern = scene4_cat_002;
    else               *pattern = scene4_cat_003;
    return (t == 71);
}

// Car OBJ patterns
static const u16 scene4_obj_carL[] = {
    1,
    0x03C0, 0xC1C0, 0x1200,
};
static const u16 scene4_obj_carR[] = {
    1,
    0x03C0, 0xC1C0, 0x1208,
};

// ---- Global state references ----
extern u8   bEnding;
extern u16  sLocalSeq;
extern u32  uLocalTime;
extern u32  uLocalTime2;
extern s16  sWork0, sWork1, sWork2, sWork3;
extern s16  bg_offset_x, bg_offset_y;
extern s16  bg_pos_x, bg_pos_y;
extern s16  bg_scale_x, bg_scale_y;
extern u16  ob_pos_x, ob_pos_y;
extern u16  ob_rotate;
extern u16  ob_scale_x, ob_scale_y;
extern u16  ob_palette, ob_priority;
extern u16  ob_affine;
extern u16  uEVY;
extern u16 *pObjEnd;
extern u32  uObjSize;

void Scene4_Init(void)
{
    // Copy BG palette (17 entries) for mode 4
    REG_DMA3SAD = (u32)scene4_bg_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (17*2 >> 2);

    // Copy OBJ palette (208 entries)
    REG_DMA3SAD = (u32)scene4_obj_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (208*2 >> 2);

    // If ending mode, copy alternative cat palette (16 entries to palette row 2)
    if (bEnding)
    {
        REG_DMA3SAD = (u32)scene4_obj_Ending_cat_Palette;
        REG_DMA3DAD = (u32)((u16 *)OBJ_PLTT + 32);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (16*2 >> 2);
    }

    // Clear bitmap top/bottom margins with zero (index 0)
    // Top: lines 0 to (160-128)/2 * 240 = 16*240 = 3840 pixels
    // Bottom: same area
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)BG_VRAM;
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (3840 >> 2);
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 3840 + 128*240);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (3840 >> 2);
    }

    // Initial OBJ affine position: center-left for car
    ob_pos_x = 88;
    ob_pos_y = 120;
    ob_scale_x = 0x200;  // 2.0x scale
    ob_scale_y = 0x200;

    // m4aMPlayVolumeControl for lower volume in non-ending
    // m4aSongNumStartOrChange(440);  // Wind sound effect, TODO

    // Display: Mode 4, BG2 enabled at init
    REG_DISPCNT = DISPCNT_MODE_4;
}

void Scene4_Exec(int time)
{
    u16 *dst;
    int i;

    switch (sLocalSeq)
    {
    case 0:  // Decompress bitmap to BG_VRAM+0xF00 (skip top 16 lines), OBJ tiles
        // Mode 4 uses OBJ_VRAM1 (0x6014000), not OBJ_VRAM0 (0x6010000)
        LZ77UnCompVram((const u32 *)scene4_Bitmap,
                       (void *)((u8 *)BG_VRAM + 16 * 240));
        LZ77UnCompVram((const u32 *)scene4_obj_Char, (void *)OBJ_VRAM1);
        uLocalTime = 0;
        sLocalSeq++;
        break;

    case 1:  // Setup positions, darken to max, enable MODE_4 + OBJ + BG2
        V_Wait();

        // Cat initial position (walks from right: x=280, y=144)
        sWork0 = 280;
        sWork1 = 144;

        // BG vertical reference offset starts at -8
        bg_offset_y = -8;

        // Max brightness darken (screen black)
        uEVY = 16;
        REG_BLDY = 16;
        REG_BLDCNT = BLDCNT_TGT1_BD | BLDCNT_TGT1_OBJ | BLDCNT_TGT1_BG2
                   | BLDCNT_EFFECT_DARKEN;

        REG_DISPCNT = DISPCNT_MODE_4 | DISPCNT_OBJ_ON | DISPCNT_BG2_ON;

        sLocalSeq++;
        uLocalTime = 0;
        break;

    case 2:  // Fade in, cat runs left, BG scrolls. Matches IDA S4_1
        FadeDec(15);
        uLocalTime++;

        // Cat running animation — uses global 'time' (usFadeTimer), not uLocalTime
        {
            const u16 *pat;
            CatAnime0(time, &pat);
        }

        // BG scroll at specific frames
        switch (uLocalTime)
        {
        case 150: case 160: case 170: case 180:
        case 190: case 200: case 210: case 220:
            bg_offset_y++;
            break;
        }

        // Cat moves left on odd frames (IDA: (uLocalTime & 1) == 0 → skip on even)
        if ((uLocalTime & 1) && (--sWork0 == 119))
        {
            sLocalSeq++;
            uLocalTime = 0;
            // m4aSongNumStartOrChange(441);  // BGM change, TODO
            uLocalTime2 = 0;
            REG_BLDY = 3;
            REG_BLDCNT = BLDCNT_EFFECT_LIGHTEN | BLDCNT_TGT1_BD
                        | BLDCNT_TGT1_OBJ | BLDCNT_TGT1_BG2;
        }
        break;

    case 3:  // Cat pose 2
        uLocalTime++;
        if (uLocalTime > 30)
        {
            sLocalSeq++;
            uLocalTime = 0;
        }
        break;

    case 4:  // Cat pose 3
        uLocalTime++;
        if (uLocalTime > 30)
        {
            sLocalSeq++;
            uLocalTime = 0;
        }
        break;

    case 5:  // Cat shaking, palette change, shrink
        // Adjust car palette toward minimum every 4th frame
        if ((uLocalTime & 3) == 3 && ob_palette > 4)
            ob_palette--;

        // Shrink OBJ from 2x to near 0
        if (ob_scale_x > 0x10)
        {
            ob_scale_x -= 0x10;
            ob_scale_y -= 0x10;
        }
        else
        {
            ob_priority = 3;  // OBJ behind BG
            // Move down every 8th frame
            if (ob_pos_y > 127)
            {
                sLocalSeq++;
                uLocalTime = 0;
                sWork2 = sWork0 - 60;
                sWork3 = sWork1 + 60;
            }
            else if ((uLocalTime & 7) == 7)
            {
                ob_pos_y++;
            }
        }
        uLocalTime++;
        break;

    case 6:  // Paper plane moves toward cat
        uLocalTime++;
        if (sWork2 < sWork0) sWork2++;
        if (sWork3 > sWork1) sWork3--;
        if (sWork2 == sWork0 && sWork3 == sWork1)
        {
            sLocalSeq++;
            uLocalTime = 0;
            // m4aSongNumStartOrChange(442);  // Newspaper sound, TODO
        }
        break;

    case 7:  // Cat flying
        uLocalTime++;
        if (uLocalTime > 40)
        {
            sLocalSeq++;
            uLocalTime = 0;
            sWork3 -= 2;
        }
        break;

    case 8:  // Cat swooping (paper sinks)
        i = (uLocalTime > 54) ? 54 : uLocalTime;
        if ((uLocalTime & 3) == 3)
            sWork3++;
        uLocalTime++;
        if (uLocalTime > 30)
        {
            sLocalSeq++;
            uLocalTime = 0;
        }
        break;

    case 9:  // Cat diving (paper still)
        uLocalTime++;
        if (uLocalTime > 30)
        {
            sLocalSeq++;
            uLocalTime = 0;
        }
        break;

    case 10:  // Cat finale
        uLocalTime++;
        if (uLocalTime > 40)
            sGameSeq++;
        break;
    }

    // ---- BG zoom-in effect ----
    if (sLocalSeq > 4 && (time & 15) == 15 && bg_scale_x <= 0x11F)
    {
        bg_scale_x++;
        bg_scale_y++;
    }

    // ---- Brightness sequence for cat scene (uLocalTime2 based) ----
    if (sLocalSeq > 2)
    {
        switch (uLocalTime2)
        {
        case 15:  REG_BLDY = 4;  break;
        case 30:  REG_BLDY = 5;  break;
        case 45:  REG_BLDY = 6;  break;
        case 61:  REG_BLDY = 7;  break;
        case 77:  REG_BLDY = 8;  break;
        case 78:  REG_BLDY = 16; break;
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

    // ---- OBJ rendering ----
    dst = (u16 *)OamBuf;

    // Only render after display enabled
    if (sLocalSeq > 1)
    {
        // Cat OBJ — running animation with cycle frames
        {
            const u16 *cat_pattern = 0;
            if (sLocalSeq == 5)
            {
                // Cat shaking (scene4_Anime10 in original)
                cat_pattern = scene4_cat_009;
            }
            else if (sLocalSeq >= 2)
            {
                // Cat running animation — use global time (matches IDA scene4_Anime0(a1, ...))
                CatAnime0(time, &cat_pattern);
            }

            if (cat_pattern)
                dst = SetObj(cat_pattern, dst, sWork0, sWork1 - bg_offset_y);
        }
        // Car OBJ: left half (visible before cat when shrinking)
        if (sLocalSeq == 5 && ob_scale_x > 0x80)
        {
            int x, y, offsetLR, offsetX;
            u16 pal = (ob_palette << 12) | (ob_priority << 10);

            offsetLR = (s16)((s32)64 * ob_scale_x / 256);
            offsetX = (64 - offsetLR) / 2;

            x = ob_pos_x + offsetX;
            y = ob_pos_y;

            // Car left half
            dst = SetObj(scene4_obj_carL, dst, x, y);
            dst[-1] = (dst[-1] & 0x3FF) | pal;

            // Car right half
            dst = SetObj(scene4_obj_carR, dst, x + offsetLR, y);
            dst[-1] = (dst[-1] & 0x3FF) | pal;
        }

        // Paper plane OBJ (after cat hitches a ride)
        if (sLocalSeq > 5)
        {
            // Paper OBJ placeholder — would be scene4 paper pattern
            // For now, use building OBJ at paper position
        }

        // Building OBJ (static background overlay)
        dst = SetObj(scene4_obj_bldg, dst, 120, 96 + bg_offset_y);

        // Car OBJ: left half (behind cat when shrunk)
        if (sLocalSeq == 5 && ob_scale_x < 0x80)
        {
            int x, y, offsetLR, offsetX;
            u16 pal = (ob_palette << 12) | (ob_priority << 10);

            offsetLR = (s16)((s32)64 * ob_scale_x / 256);
            offsetX = (64 - offsetLR) / 2;

            x = ob_pos_x + offsetX;
            y = ob_pos_y;

            dst = SetObj(scene4_obj_carL, dst, x, y);
            dst[-1] = (dst[-1] & 0x3FF) | pal;

            dst = SetObj(scene4_obj_carR, dst, x + offsetLR, y);
            dst[-1] = (dst[-1] & 0x3FF) | pal;
        }

        EndObj(dst);
    }

    // ---- BG2 vertical scroll ----
    if (sLocalSeq >= 2)
    {
        REG_BG2VOFS = bg_offset_y;
        REG_BG2HOFS = 0;
    }
}

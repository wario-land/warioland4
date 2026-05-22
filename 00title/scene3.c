// Scene 3: Night road — car driving at night with headlights
// Scene 3: Night road — car driving at night with headlights
//
// BG layers:
//   BG0: car body     (512x512, screenbase 16) — priority 0
//   BG1: car shadow   (512x512, screenbase 20) — priority 1
//   BG2: background   (512x512, screenbase 24) — priority 2
//
// WIN0 used for headlight beam effect.
// Headlight palette cycles through 13 color temperatures.
// Headlights flash off at time ranges 140-151 and 164-175.

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

// ---- Scene3 data references ----
extern const u16 scene3_Palette[256];
extern const u8  scene3_Char[];
extern const u16 scene3_car[];
extern const u16 scene3_shadow[];
extern const u16 scene3_back[];
extern const u16 scene3_obj_Palette[16];
extern const u8  scene3_obj_Char[];

// OBJ animation pattern table (headlight beam sprites)
extern const u16 scene3_000[];

// ---- Global state references ----
extern u16 sLocalSeq;
extern u32 uLocalTime;
extern s16 bg_scroll_x;
extern u16 *pObjEnd;
extern u32 uObjSize;

#define BG0_TOP 0x8000
#define BG1_TOP 0xA000
#define BG2_TOP 0xC000

static const s16 wobble_table[4] = { 0, 2, 3, 1 };

void Scene3_Init(void)
{
    // Copy BG palette (256 entries)
    REG_DMA3SAD = (u32)scene3_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (256*2 >> 2);

    // Copy OBJ palette (16 entries)
    REG_DMA3SAD = (u32)scene3_obj_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (16*2 >> 2);

    // Clear tile 0x3FF to empty
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x7FE0);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (32 >> 2);
    }

    // Clear BG0-BG5 screenbases (0x8000-0xDFFF) with tile 0x3FF
    {
        volatile u32 v = 0x03FF03FF;
        REG_DMA3SAD = (u32)&v;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + BG0_TOP);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x800*6 >> 2);
    }

    // BG0: 512x512, 16-color, priority 0, screenbase 16, charbase 0
    REG_BG0CNT = BGCNT_TXT512x512 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);
    // BG1: 512x512, 16-color, priority 1, screenbase 20, charbase 0
    REG_BG1CNT = BGCNT_TXT512x512 | BGCNT_16COLOR | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(20) | BGCNT_CHARBASE(0);
    // BG2: 512x512, 16-color, priority 2, screenbase 24, charbase 0
    REG_BG2CNT = BGCNT_TXT512x512 | BGCNT_16COLOR | BGCNT_PRIORITY(2)
               | BGCNT_SCREENBASE(24) | BGCNT_CHARBASE(0);

    // Car starts offscreen to the right, scrolls leftward
    bg_scroll_x = 256;
    REG_BG0HOFS = bg_scroll_x;
    REG_BG1HOFS = bg_scroll_x;

    // Display off at init
    REG_DISPCNT = DISPCNT_MODE_0;

    // WIN0: full-width window, vertical range Y=16..144 (headlight beam)
    REG_WIN0H = WIN_RANGE(0, 240);
    REG_WIN0V = WIN_RANGE(16, 144);
    REG_WININ  = WININ_WIN0_BG0 | WININ_WIN0_BG1 | WININ_WIN0_BG2 | WININ_WIN0_OBJ | WININ_WIN0_CLR;
    REG_WINOUT = 0;
}

void Scene3_Exec(int time)
{
    u16 *pattern;
    u16 *dst;
    int i, j;
    s16 y;

    switch (sLocalSeq)
    {
    case 0:  // Decompress BG tiles and tilemaps
        LZ77UnCompVram((const u32 *)scene3_Char, (void *)BG_VRAM);
        UnPackScreen((const u16 *)scene3_car,    (vu16 *)((u8 *)BG_VRAM + BG0_TOP));
        UnPackScreen((const u16 *)scene3_shadow, (vu16 *)((u8 *)BG_VRAM + BG1_TOP));
        UnPackScreen((const u16 *)scene3_back,   (vu16 *)((u8 *)BG_VRAM + BG2_TOP));
        sLocalSeq++;
        break;

    case 1:  // Clear OBJ VRAM, decompress OBJ tiles, enable all layers + blend
        // Clear OBJ VRAM first to prevent leftover tiles from previous scenes
        {
            volatile u32 z = 0;
            REG_DMA3SAD = (u32)&z;
            REG_DMA3DAD = (u32)OBJ_VRAM0;
            REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x4000 >> 2);
        }
        LZ77UnCompVram((const u32 *)scene3_obj_Char, (void *)OBJ_VRAM0);
        sLocalSeq++;
        REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_OBJ_ON
                    | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_BG2_ON
                    | DISPCNT_WIN0_ON;
        SetBLDUpMin(BLDCNT_TGT1_BD | BLDCNT_TGT1_OBJ | BLDCNT_TGT1_BG0
                   | BLDCNT_TGT1_BG1 | BLDCNT_TGT1_BG2);
        break;

    case 2:  // Main animation loop
        // ---- Car deceleration from right side ----
        if (bg_scroll_x > 0)
        {
            if (bg_scroll_x > 80)       bg_scroll_x -= 2;
            if (bg_scroll_x > 40)       bg_scroll_x -= 1;
            if (bg_scroll_x > 16)       bg_scroll_x--;
            else if (bg_scroll_x >  8 && (time & 1))            bg_scroll_x--;
            else if (bg_scroll_x >  4 && (time & 3) == 3)       bg_scroll_x--;
            else if ((time & 7) == 7)   bg_scroll_x--;

            REG_BG0HOFS = bg_scroll_x & 0x1FF;
            REG_BG1HOFS = bg_scroll_x & 0x1FF;
        }

        // ---- BG0/BG1 wobble ----
        y = wobble_table[time & 3];
        REG_BG0VOFS = y;                        // Car body bounces vertically
        REG_BG1HOFS = (bg_scroll_x - y) & 0x1FF; // Shadow bounces horizontally

        // ---- Headlight palette cycling via DMA ----
        // Copy one of 13 palette rows (BG palette rows 1-13) to palette row 1
        {
            int palRow = time % 13;
            REG_DMA3SAD = (u32)((u16 *)BG_PLTT + 16 * (palRow + 1));
            REG_DMA3DAD = (u32)((u16 *)BG_PLTT + 16);
            REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (16*2 >> 2);
        }

        // ---- Headlight brightness based on car position ----
        if      (bg_scroll_x == 240) REG_BLDY = 1;
        else if (bg_scroll_x == 238) REG_BLDY = 2;
        else if (bg_scroll_x == 236) REG_BLDY = 3;
        else if (bg_scroll_x == 120) REG_BLDY = 16;
        else if (bg_scroll_x == 116) REG_BLDY = 15;
        else if (bg_scroll_x == 112) REG_BLDY = 14;
        else if (bg_scroll_x == 108) REG_BLDY = 13;
        else if (bg_scroll_x == 104) REG_BLDY = 12;
        else if (bg_scroll_x == 100) REG_BLDY = 11;
        else if (bg_scroll_x ==  96) REG_BLDY = 10;
        else if (bg_scroll_x ==  92) REG_BLDY = 9;
        else if (bg_scroll_x ==  88) REG_BLDY = 8;
        else if (bg_scroll_x ==  84) REG_BLDY = 7;
        else if (bg_scroll_x ==  80) REG_BLDY = 6;
        else if (bg_scroll_x ==  78) REG_BLDY = 5;
        else if (bg_scroll_x ==  76) REG_BLDY = 4;
        else if (bg_scroll_x ==  74) REG_BLDY = 3;

        // ---- Headlight flash effect ----
        // Turn off headlights (close WIN0) at frame ranges 140-151 and 164-175
        if ((unsigned)(time - 140) <= 11 || (unsigned)(time - 164) <= 11)
        {
            // Headlights off: close window
            REG_WIN0H = 0;
            REG_WIN0V = 0;
            pattern = 0;
        }
        else
        {
            REG_WIN0H = WIN_RANGE(0, 240);
            REG_WIN0V = WIN_RANGE(16, 144);
            pattern = (u16 *)scene3_000;  // Headlight beam OBJ pattern
        }

        // Advance scene at frame 350
        if (time == 350)
            sGameSeq++;

        // ---- Render headlight OBJs ----
        // OBJ pattern table: first u16 is the count of sprites per unit
        // Number of visible beam segments depends on car position
        if (pattern && *pattern)
            i = 128 / (*pattern);  // Max sprites per unit
        else
            i = 0;

        j = bg_scroll_x / 16;  // Visible beam segments based on car position
        if (i > j) i = j;

        dst = (u16 *)OamBuf;
        if (i > 0)
        {
            int offset = 8 * (i - 1);
            while (i-- > 0)
            {
                dst = SetObj(pattern, dst, -bg_scroll_x + 128 - (i << 3), 64 + y);
            }
        }
        else if (pattern)
        {
            dst = SetObj(pattern, dst, -bg_scroll_x + 128, 64 + y);
        }

        EndObj(dst);
        break;
    }
}

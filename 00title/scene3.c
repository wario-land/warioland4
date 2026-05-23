// Scene 3: Night road — car driving at night with headlights
//
// This is the first title cinematic after the waterfall scene.
// A car drives from the right side of the screen toward the center,
// with headlights illuminating a night road background.
//
// BG layers:
//   BG0: Car body (512x512, screenbase 16) — priority 0 (frontmost)
//   BG1: Car shadow (512x512, screenbase 20) — priority 1
//   BG2: Night background (512x512, screenbase 24) — priority 2 (backmost)
//
// Special effects:
//   - WIN0 window masks the headlight beam area
//   - Headlight palette cycles through 13 color temperatures each frame
//   - Headlights flash off at frames 140-151 and 164-175 (lightning/power flicker)
//   - BG0/BG1 wobble simulates bumpy road effect
//   - Car decelerates from right side with staged speed reduction
//   - Headlight brightness increases as car approaches center (BLDY register)
//
// States (sLocalSeq):
//   0: Decompress BG tiles to VRAM, unpack car/shadow/background tilemaps
//   1: Clear OBJ VRAM, decompress OBJ tiles, enable BG+OBJ+WIN0 + blend
//   2: Main animation loop: car deceleration, wobble, headlight effects

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

// ---- Global state references ----
extern u16 sLocalSeq;
extern u32 uLocalTime;
extern s16 bg_scroll_x;
extern u16 *pObjEnd;
extern u32 uObjSize;

#define BG0_TOP 0x8000
#define BG1_TOP 0xA000
#define BG2_TOP 0xC000

// Wobble lookup: vertical bounce offsets for bumpy road effect
// Indexed by (time & 3), applied to BG0VOFS and as horizontal offset to BG1HOFS
static const s16 wobble_table[4] = { 0, 2, 3, 1 };

void Scene3_Init(void)
{
    // ---- Load BG palette (256 entries, 512 bytes) ----
    // Copy the full BG palette from ROM to BG palette RAM.
    // Rows 1-13 contain 13 pre-computed headlight color temperatures.
    // Row 0 is the base car/background colors.
    REG_DMA3SAD = (u32)scene3_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (256*2 >> 2);

    // ---- Load OBJ palette (16 entries, 32 bytes) ----
    // Headlight beam OBJs use palette row 0 (OBJ palette entries 0-15)
    REG_DMA3SAD = (u32)scene3_obj_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (16*2 >> 2);

    // ---- Clear tile 0x3FF (blank tile used for empty screen areas) ----
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x7FE0);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (32 >> 2);
    }

    // ---- Clear BG screenbases with blank tile ----
    // BG0-2 all use 512x512 mode (6 screenbases * 0x800 bytes each = 0x3000 bytes)
    {
        volatile u32 v = 0x03FF03FF;
        REG_DMA3SAD = (u32)&v;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + BG0_TOP);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x800*6 >> 2);
    }

    // ---- BG control registers ----
    // All BGs share charbase 0 (tiles at VRAM base) and use 512x512 text mode
    // for scrolling. Priority decreases from BG0 (front) to BG2 (back).
    REG_BG0CNT = BGCNT_TXT512x512 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);
    REG_BG1CNT = BGCNT_TXT512x512 | BGCNT_16COLOR | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(20) | BGCNT_CHARBASE(0);
    REG_BG2CNT = BGCNT_TXT512x512 | BGCNT_16COLOR | BGCNT_PRIORITY(2)
               | BGCNT_SCREENBASE(24) | BGCNT_CHARBASE(0);

    // ---- Initial scroll: car starts offscreen to the right ----
    // bg_scroll_x = 256 means the visible area starts 256 pixels to the right,
    // so the car (drawn near the left of the tilemap) is offscreen.
    bg_scroll_x = 256;
    REG_BG0HOFS = bg_scroll_x;
    REG_BG1HOFS = bg_scroll_x;

    // ---- WIN0 setup: headlight beam mask ----
    // WIN0 defines a rectangular region where the headlight effect is shown.
    // X: full screen width (0-240)
    // Y: rows 16-144 (the road area, excluding sky and dashboard)
    // WININ: enable all BGs + OBJ + color fill inside window
    // WINOUT: all OFF outside window (creates the "tunnel of light" effect)
    REG_WIN0H = WIN_RANGE(0, 240);
    REG_WIN0V = WIN_RANGE(16, 144);
    REG_WININ  = WININ_WIN0_BG0 | WININ_WIN0_BG1 | WININ_WIN0_BG2 | WININ_WIN0_OBJ | WININ_WIN0_CLR;
    REG_WINOUT = 0;

    // Display off at init — enabled in Exec case 1 after tile decompression
    REG_DISPCNT = DISPCNT_MODE_0;
}

void Scene3_Exec(int time)
{
    const u16 *pattern;
    u16 *dst;
    int headlight_count, visible_segments;
    s16 y;

    switch (sLocalSeq)
    {
    case 0:
        // ---- Decompress BG tiles and unpack tilemaps ----
        // scene3_Char contains all BG tiles (car body, shadow, night background)
        // in LZ77 compressed format. Decompress to VRAM base (charbase 0).
        LZ77UnCompVram((const u32 *)scene3_Char, (void *)BG_VRAM);

        // UnPackScreen: decompress RLE tilemaps to BG screenbases.
        // Each tilemap header format: (position<<5) | count.
        // bit 15=1: write same tile count+1 times (run mode)
        // bit 15=0: write incrementing tiles (fill mode)
        UnPackScreen((const u16 *)scene3_car,    (vu16 *)((u8 *)BG_VRAM + BG0_TOP));
        UnPackScreen((const u16 *)scene3_shadow, (vu16 *)((u8 *)BG_VRAM + BG1_TOP));
        UnPackScreen((const u16 *)scene3_back,   (vu16 *)((u8 *)BG_VRAM + BG2_TOP));
        sLocalSeq++;
        break;

    case 1:
        // ---- Clear OBJ VRAM and decompress OBJ tiles ----
        // OBJ VRAM must be cleared first to prevent leftover sprite tiles
        // from previous scenes bleeding through.
        {
            volatile u32 z = 0;
            REG_DMA3SAD = (u32)&z;
            REG_DMA3DAD = (u32)OBJ_VRAM0;
            REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x4000 >> 2);
        }
        LZ77UnCompVram((const u32 *)scene3_obj_Char, (void *)OBJ_VRAM0);
        sLocalSeq++;

        // ---- Enable display with all layers ----
        // MODE_0: all text-mode BGs, no affine
        // WIN0_ON: headlight window effect active
        // OBJ_ON: headlight beam sprites
        REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_OBJ_ON
                    | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_BG2_ON
                    | DISPCNT_WIN0_ON;

        // ---- Setup blend for headlight brightness control ----
        // BLD_UP with BLDY controls how "bright" the headlight beam appears.
        // BLDY starts at 0 (fully transparent/dark) and increases as car approaches.
        SetBLDUpMin(BLDCNT_TGT1_BD | BLDCNT_TGT1_OBJ | BLDCNT_TGT1_BG0
                   | BLDCNT_TGT1_BG1 | BLDCNT_TGT1_BG2);
        break;

    case 2:
        // ---- Main animation loop ----
        // Handles: car deceleration, BG wobble, headlight palette cycling,
        // headlight brightness, headlight flash effect, OBJ rendering

        // === Car deceleration from right side ===
        // Speed decreases in stages as car approaches center:
        //   >80px: decelerate by 2 per frame (fast approach)
        //   >40px: decelerate by 1 per frame
        //   >16px: decelerate by 1 per frame
        //   >8px:  decelerate every other frame
        //   >4px:  decelerate every 4th frame
        //   >0px:  decelerate every 8th frame
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

        // === BG wobble (bumpy road effect) ===
        // BG0 (car body) bounces vertically
        // BG1 (shadow) shifts horizontally by wobble amount for parallax
        y = wobble_table[time & 3];
        REG_BG0VOFS = y;
        REG_BG1HOFS = (bg_scroll_x - y) & 0x1FF;

        // === Headlight palette cycling ===
        // 13 pre-computed palette rows in BG palette rows 1-13.
        // Each frame copies one row to row 1 (the active headlight palette).
        // This creates a flickering/warm glow effect as "time % 13" rotates.
        {
            int palRow = time % 13;
            REG_DMA3SAD = (u32)((u16 *)BG_PLTT + 16 * (palRow + 1));
            REG_DMA3DAD = (u32)((u16 *)BG_PLTT + 16);
            REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (16*2 >> 2);
        }

        // === Headlight brightness based on car position ===
        // As the car scrolls closer to center (bg_scroll_x decreases from 256→0),
        // BLDY increases making the headlights brighter.
        // Specific scroll positions map to discrete brightness levels.
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

        // === Headlight flash effect (lightning/power flicker) ===
        // WIN0 is closed (zero size) during two time ranges, hiding the
        // headlight beam. This creates dramatic flickering moments.
        if ((unsigned)(time - 140) <= 11 || (unsigned)(time - 164) <= 11)
        {
            // Headlights OFF: close window, hide beam OBJ
            REG_WIN0H = 0;
            REG_WIN0V = 0;
            pattern = 0;
        }
        else
        {
            // Headlights ON: full window, show beam OBJ
            REG_WIN0H = WIN_RANGE(0, 240);
            REG_WIN0V = WIN_RANGE(16, 144);
            scene3_Anime0(time, &pattern);
        }

        // Advance scene at frame 350
        if (time == 350)
            sGameSeq++;

        // === Render headlight beam OBJs ===
        // The beam is composed of multiple OBJ segments, each 8 pixels wide.
        // Number of visible segments depends on car position:
        //   max_segments = 128 / sprite_count (OBJ count per beam unit)
        //   visible_segments = bg_scroll_x / 16 (how many fit on screen)
        // Segments are laid out horizontally from the car position rightward.
        if (pattern && *pattern)
            headlight_count = 128 / (*pattern);     // OBJs per beam unit
        else
            headlight_count = 0;

        visible_segments = bg_scroll_x / 16;
        if (headlight_count > visible_segments)
            headlight_count = visible_segments;

        dst = (u16 *)OamBuf;
        if (headlight_count > 0)
        {
            int offset = 8 * (headlight_count - 1);
            while (headlight_count-- > 0)
            {
                // Each segment is 8 pixels apart, starting from car position
                dst = SetObj(pattern, dst, -bg_scroll_x + 128 - (headlight_count << 3), 64 + y);
            }
        }
        else if (pattern)
        {
            // Even when no segments fit, show a single beam sprite at car position
            dst = SetObj(pattern, dst, -bg_scroll_x + 128, 64 + y);
        }

        EndObj(dst);
        break;
    }
}

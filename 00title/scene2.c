// Scene 2: Swamp — car driving through swamp at dusk
// Scene 2: Swamp — car driving through swamp at dusk
//
// BG layers:
//   BG0: car upper body + door (512x256, screenbase 16)
//   BG1: car lower body + ground + tires (512x256, screenbase 18)
//   BG2: shutter fill (256x256, screenbase 20)
//
// States (sLocalSeq):
//   0: Decompress tiles, max brightness dark, enable display, set white
//   1: Fade in, scroll car from left
//   2: Main animation: tire/ground/door patterns, parallax, BGM

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

// ---- Scene2 data (defined in scene2_data.c) ----
extern const u16 scene2_Palette[48];
extern const u8  scene2_Char[];
extern const u16 scene2_carU[];
extern const u16 scene2_carL[];
extern const u16 *scene2_ground[4];
extern const u16 *scene2_door[4];
extern const u16 scene2_frontTire[4][8];
extern const u16 scene2_rearTire[4][36];

// ---- Globals ----
extern u16 sLocalSeq;
extern u32 uLocalTime;
extern s16 bg_scroll_x;

// BG screenbase definitions
#define BG0_TOP 0x8000  // 512x256 (uses 0x8000 + 0x8800)
#define BG1_TOP 0x9000  // 512x256 (uses 0x9000 + 0x9800)
#define BG2_TOP 0xA000  // 256x256

#define FRONT_TIRE_TOP 423
#define REAR_TIRE_TOP  272

// ---- BlockCopy ----
static void BlockCopy(const u16 *src, vu16 *dst, int w, int h, int stride)
{
    int r, c;
    for (r = 0; r < h; r++)
    {
        for (c = 0; c < w; c++)
            dst[c] = *src++;
        dst += stride;
    }
}

// ---- Scene2_SetPattern ----
// Sets up door (BG0), ground (BG1), tires (BG1), and ground fill row (BG1)
// for the current animation frame.
static void Scene2_SetPattern(int frame)
{
    int f = frame & 3;
    int j;
    u16 *dst;
    u16 data;

    // Ground fill tile per frame: {0x2109, 0x20C7, 0x20DE, 0x20F2}
    static const u16 ground_tile[4] = { 0x2109, 0x20C7, 0x20DE, 0x20F2 };

    // Door → BG0 screenbase 16
    UnPackScreen((const u16 *)scene2_door[f], (vu16 *)((u8 *)BG_VRAM + BG0_TOP));

    // Front tire → BG1 (row offset 423)
    BlockCopy((const u16 *)scene2_frontTire[f],
              (vu16 *)((u8 *)BG_VRAM + BG1_TOP) + FRONT_TIRE_TOP,
              2, 4, 32);

    // Rear tire → BG1 (row offset 272)
    BlockCopy((const u16 *)scene2_rearTire[f],
              (vu16 *)((u8 *)BG_VRAM + BG1_TOP) + REAR_TIRE_TOP,
              4, 9, 32);

    // Ground → BG1 screenbase 18
    UnPackScreen((const u16 *)scene2_ground[f], (vu16 *)((u8 *)BG_VRAM + BG1_TOP));

    // Ground fill row: fill 24 entries at end of BG1 right half (0x9800 + offset)
    // This fills the right-edge ground tiles visible when car scrolls
    data = ground_tile[f];
    u16 *w = (u16 *)((u8 *)BG_VRAM + BG1_TOP + 0x800) + 575;
    for (j = 0; j < 24; j++)
        *w-- = data;
}

// ---- Scene2_Init ----
void Scene2_Init(void)
{
    // Copy palette (48 entries * 2 bytes = 96 bytes)
    REG_DMA3SAD = (u32)scene2_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (48*2 >> 2);

    // Clear tile 0x3FF to empty (32 bytes)
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x7FE0);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (32 >> 2);
    }

    // Clear BG0-BG3 screenbases (0x8000-0x9FFF) with tile 0x3FF
    {
        volatile u32 v = 0x03FF03FF;
        REG_DMA3SAD = (u32)&v;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + BG0_TOP);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x800*4 >> 2);
    }

    // Car upper body → BG0
    UnPackScreen((const u16 *)scene2_carU, (vu16 *)((u8 *)BG_VRAM + BG0_TOP));

    // Car lower body → BG1
    UnPackScreen((const u16 *)scene2_carL, (vu16 *)((u8 *)BG_VRAM + BG1_TOP));

    // Fill ground bottom rows of BG1 with tile 0x0015 (blank) — 96 u16 entries * 2 bytes = 192 bytes
    {
        volatile u32 t15 = 0x00150015;
        REG_DMA3SAD = (u32)&t15;
        REG_DMA3DAD = (u32)((vu16 *)((u8 *)BG_VRAM + BG1_TOP) + 544);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (96*2 >> 2);
        REG_DMA3SAD = (u32)&t15;
        REG_DMA3DAD = (u32)((vu16 *)((u8 *)BG_VRAM + BG1_TOP + 0x800) + 544);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (96*2 >> 2);
    }

    // Init door, ground, tires for frame 0
    Scene2_SetPattern(0);

    // Clear BG2 shutter area with tile 0x0015 — 32*17=544 u16 entries * 2 = 1088 bytes
    {
        volatile u32 t15 = 0x00150015;
        REG_DMA3SAD = (u32)&t15;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + BG2_TOP);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (32*17*2 >> 2);
    }
    // Fill BG2 right area with blank tile 0x03FF — 480 u16 entries * 2 = 960 bytes
    {
        volatile u32 t3FF = 0x03FF03FF;
        REG_DMA3SAD = (u32)&t3FF;
        REG_DMA3DAD = (u32)((vu16 *)((u8 *)BG_VRAM + BG2_TOP) + 544);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (480*2 >> 2);
    }

    // BG0: 512x256, 16-color, priority 0, screenbase 16, charbase 0
    REG_BG0CNT = BGCNT_TXT512x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);
    // BG1: 512x256, 16-color, priority 0, screenbase 18, charbase 0
    REG_BG1CNT = BGCNT_TXT512x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(18) | BGCNT_CHARBASE(0);
    // BG2: 256x256, 16-color, priority 0, screenbase 20, charbase 0
    REG_BG2CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(20) | BGCNT_CHARBASE(0);

    // Car starts offscreen to the left
    bg_scroll_x = -192;
    REG_BG0HOFS = bg_scroll_x;
    REG_BG1HOFS = bg_scroll_x;

    // Display off at init
    REG_DISPCNT = DISPCNT_MODE_0;
}

// ---- Scene2_Exec ----
void Scene2_Exec(int time)
{
    switch (sLocalSeq)
    {
    case 0:  // Decompress tiles, set max brightness to darken screen, enable display
        LZ77UnCompVram((const u32 *)scene2_Char, (void *)BG_VRAM);
        SetBLDDownMax(BLDCNT_TGT1_BD | BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG1 | BLDCNT_TGT1_BG2);
        REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_BG2_ON;
        V_Wait();
        ((vu16 *)BG_PLTT)[0] = 0x7FFF;  // Set first palette color to white for headlights
        sLocalSeq++;
        break;

    case 1:  // Fade in, scroll car in from left
        FadeDec(15);

        if (bg_scroll_x < -24 && (time & 3) == 3)
        {
            bg_scroll_x++;
            REG_BG0HOFS = bg_scroll_x;
            REG_BG1HOFS = bg_scroll_x;
        }

        if (time == 510)
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 2:  // Main animation loop
        {
            // Door, ground, tire pattern changes
            if (uLocalTime == 32)
                Scene2_SetPattern(1);
            else if (uLocalTime == 64)
                Scene2_SetPattern(2);
            else if (uLocalTime == 146)
                Scene2_SetPattern(3);

            // Advance scene at frame 396
            if (uLocalTime == 396)
            {
                sGameSeq++;
                break;
            }

            // BG2 shutter scroll (vertical)
            REG_BG2VOFS = uLocalTime >> 4;

            // Car body horizontal scroll: decelerate from left to target position
            if (bg_scroll_x < 16)
            {
                if (bg_scroll_x < -8 && (time & 7) == 7)
                    bg_scroll_x++;
                else if (bg_scroll_x < -4 && (time & 15) == 15)
                    bg_scroll_x++;
                else if ((time & 31) == 31)
                    bg_scroll_x++;

                REG_BG0HOFS = bg_scroll_x & 0x1FF;
                REG_BG1HOFS = bg_scroll_x & 0x1FF;
            }

            // BG0 vertical wobble (bumpy road effect)
            {
                static const u8 wobble1[] = { 0, 3, 6, 3 };
                static const u8 wobble2[] = { 0, 2, 4, 2 };

                if (uLocalTime > 146)
                    REG_BG0VOFS = wobble2[uLocalTime & 3];
                else if (uLocalTime > 64)
                    REG_BG0VOFS = wobble1[(uLocalTime >> 1) & 3];
            }

            // Background music at frame 184
            if (uLocalTime == 184)
            {
                // m4aSongNumStartOrChange(636);  // BGM TODO
            }

            uLocalTime++;
        }
        break;
    }
}

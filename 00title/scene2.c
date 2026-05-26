// Scene 2: Swamp -- car driving through swamp at dusk
//
// After the waterfall scene (scene1), the title sequence shows Wario's
// car driving through a swampy area at dusk. This scene uses three BG
// layers to create depth and animation.
//
// BG layers:
//   BG0: Car upper body + door       (512x256, screenbase 16, priority 0)
//        -- Car roof, windows, door frame. Door pattern cycles through
//          4 frames to simulate swinging on bumpy terrain.
//   BG1: Car lower body + ground     (512x256, screenbase 18, priority 0)
//        -- Ground strips, tires, car chassis. Front and rear tires
//          animate through 4 frames for rotation effect.
//   BG2: Shutter/grate overlay       (256x256, screenbase 20, priority 0)
//        -- Vertical bars/grille that scroll upward creating a "film reel"
//          transition effect between title segments.
//
// Animation details:
//   - Car scrolls from left side (-192px) to center (0px)
//   - Tires rotate: 4 animation frames cycled through at key times
//   - Door swings: another 4-frame cycle
//   - Ground pattern: changes each tire frame
//   - BG2 shutter: vertical scroll driven by uLocalTime >> 4
//   - BG0 vertical wobble: simulates bumpy road (two wobble intensities)
//
// States (sLocalSeq):
//   0: Decompress tiles to VRAM, max darkness, enable display, set white
//   1: Fade in from black, scroll car from left toward center
//   2: Main animation loop with tire/door/ground pattern changes,
//      car deceleration, BG wobble, shutter scroll

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

// ---- Scene2 data ----
extern const u16 scene2_Palette[48];     // 3 palette rows for car, ground, sky
extern const u8  scene2_Char[];           // LZ77 car tiles
extern const u16 scene2_carU[];           // Car upper body tilemap (RLE)
extern const u16 scene2_carL[];           // Car lower body tilemap (RLE)
extern const u16 *scene2_ground[4];       // Ground strip patterns [0-3]
extern const u16 *scene2_door[4];         // Door animation patterns [0-3]
extern const u16 scene2_frontTire[4][8];  // Front tire: 2x4 tile per frame [4]
extern const u16 scene2_rearTire[4][36];  // Rear tire: 4x9 tile per frame [4]

// ---- Global state ----
extern u16 sLocalSeq;
extern u32 uLocalTime;
extern s16 bg_scroll_x;

// BG screenbase VRAM addresses
#define BG0_TOP 0x8000
#define BG1_TOP 0x9000
#define BG2_TOP 0xA000

// Tire positions within BG1 screenblock (row offsets, matching IDA)
// BG1 is 32 tiles wide. Front tire at row ~13, rear tire at row ~8
#define FRONT_TIRE_TOP 422    // tile offset: 0x600934E - 0x6009000 = 0x34E bytes = 422 tiles
#define REAR_TIRE_TOP  272    // tile offset: 0x6009220 - 0x6009000 = 0x220 bytes = 272 tiles

// ---- Scene2_SetPattern ----
// Updates the door (BG0), ground (BG1), tires (BG1), and ground fill row
// for the current animation frame (0-3). Called at key times during state 2.
static void Scene2_SetPattern(int frame)
{
    int f = frame & 3;
    int j;
    u16 data;

    // Tile values for ground fill row: each frame uses a different tile
    // to create a scrolling ground texture effect
    static const u16 ground_tile[4] = { 0x2109, 0x20C7, 0x20DE, 0x20F2 };

    // === Door -> BG0 (screenbase 16) ===
    // Replace the door area of the car upper body with the current frame
    UnPackScreen((const u16 *)scene2_door[f], (vu16 *)((u8 *)BG_VRAM + BG0_TOP));

    // === Front tire -> BG1 (row offset 423, 2 tiles wide x 4 rows) ===
    // Copy the 2x4 front tire pattern directly to the BG1 screenblock
    {
        int r, c;
        vu16 *dst = (vu16 *)((u8 *)BG_VRAM + BG1_TOP) + FRONT_TIRE_TOP;
        const u16 *src = scene2_frontTire[f];
        for (r = 0; r < 4; r++)
        {
            for (c = 0; c < 2; c++)
                dst[c] = *src++;
            dst += 32;  // next row (32 tiles per row)
        }
    }

    // === Rear tire -> BG1 (row offset 272, 4 tiles wide x 9 rows) ===
    {
        int r, c;
        vu16 *dst = (vu16 *)((u8 *)BG_VRAM + BG1_TOP) + REAR_TIRE_TOP;
        const u16 *src = scene2_rearTire[f];
        for (r = 0; r < 9; r++)
        {
            for (c = 0; c < 4; c++)
                dst[c] = *src++;
            dst += 32;
        }
    }

    // === Ground -> BG1 (screenbase 18) ===
    UnPackScreen((const u16 *)scene2_ground[f], (vu16 *)((u8 *)BG_VRAM + BG1_TOP));

    // === Ground fill row (matching IDA: fill 24 tiles backward from offset 0x93FE) ===
    // Fills 24 tiles at BG1 offset 488-511 (0x1E8-0x1FF tiles from BG1_TOP)
    // with the ground tile for the current frame. This covers the right edge
    // of the left 512x256 half where the car shadow rows end.
    data = ground_tile[f];
    {
        vu16 *w = (vu16 *)((u8 *)BG_VRAM + BG1_TOP) + 511;
        for (j = 0; j < 24; j++)
            *w-- = data;
    }
}

void Scene2_Init(void)
{
    // ---- Load BG palette (48 entries = 3 rows x 16) ----
    // Row 0: car body colors (muted dark tones for swamp scene)
    // Row 1: ground/road colors
    // Row 2: sky/background colors
    REG_DMA3SAD = (u32)scene2_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (48*2 >> 2);

    // ---- Clear tile 0x3FF (blank tile for empty areas) ----
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x7FE0);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (32 >> 2);
    }

    // ---- Clear BG0-BG3 screenbases with blank tile 0x3FF ----
    // 512x256 modes use 2 screenbases each (0x800 bytes x 2)
    // BG0: screenbases 16+17 (0x8000+0x8800)
    // BG1: screenbases 18+19 (0x9000+0x9800)
    // BG2: screenbase 20 (0xA000)
    // Clear 4 screenbases x 0x800 = 0x2000 bytes total
    {
        volatile u32 v = 0x03FF03FF;
        REG_DMA3SAD = (u32)&v;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + BG0_TOP);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x800*4 >> 2);
    }

    // ---- UnPack car body tilemaps ----
    // Car upper -> BG0 left half (screenbase 16 = 0x8000)
    UnPackScreen((const u16 *)scene2_carU, (vu16 *)((u8 *)BG_VRAM + BG0_TOP));
    // Car lower -> BG1 left half (screenbase 18 = 0x9000)
    UnPackScreen((const u16 *)scene2_carL, (vu16 *)((u8 *)BG_VRAM + BG1_TOP));

    // ---- Fill ground bottom rows with blank tile 0x15 ----
    // Both halves of BG1 (512x256 mode uses screenbases 18+19).
    // Tile offset 544 = row 17 (544 / 32 = 17 rows down).
    // 96 tiles = 3 rows x 32 tiles per row.
    {
        volatile u32 t15 = 0x00150015;
        // Left half fill
        REG_DMA3SAD = (u32)&t15;
        REG_DMA3DAD = (u32)((vu16 *)((u8 *)BG_VRAM + BG1_TOP) + 544);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (96*2 >> 2);
        // Right half fill (screenbase 19 = 0x9800)
        REG_DMA3SAD = (u32)&t15;
        REG_DMA3DAD = (u32)((vu16 *)((u8 *)BG_VRAM + BG1_TOP + 0x800) + 544);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (96*2 >> 2);
    }

    // ---- Initialize door, ground, tires for frame 0 ----
    Scene2_SetPattern(0);

    // ---- Clear BG2 shutter area ----
    // Top section: 32x17 tiles with blank tile 0x15
    {
        volatile u32 t15 = 0x00150015;
        REG_DMA3SAD = (u32)&t15;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + BG2_TOP);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (32*17*2 >> 2);
    }
    // Bottom-right: fill 480 tiles with blank tile 0x3FF
    {
        volatile u32 t3FF = 0x03FF03FF;
        REG_DMA3SAD = (u32)&t3FF;
        REG_DMA3DAD = (u32)((vu16 *)((u8 *)BG_VRAM + BG2_TOP) + 544);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (480*2 >> 2);
    }

    // ---- BG control registers ----
    // BG0: 512x256, 16-color, priority 0, screenbase 16 (left) + 17 (right)
    REG_BG0CNT = BGCNT_TXT512x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);
    // BG1: 512x256, 16-color, priority 0, screenbase 18 + 19
    REG_BG1CNT = BGCNT_TXT512x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(18) | BGCNT_CHARBASE(0);
    // BG2: 256x256, 16-color, priority 0, screenbase 20
    REG_BG2CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(20) | BGCNT_CHARBASE(0);

    // ---- Car starts offscreen to the left ----
    // Negative scroll makes the car (drawn near tile 0) appear from left edge
    bg_scroll_x = -192;
    REG_BG0HOFS = bg_scroll_x;
    REG_BG1HOFS = bg_scroll_x;

    // Display off at init -- enabled in Exec case 0
    REG_DISPCNT = DISPCNT_MODE_0;
}

void Scene2_Exec(int time)
{
    switch (sLocalSeq)
    {
    case 0:
        // ---- Decompress car tiles to VRAM, set dark, enable display ----
        // scene2_Char contains all BG tiles (car body, ground, tires, door)
        // in LZ77 compressed 4bpp format. Decompress to VRAM base.
        LZ77UnCompVram((const u32 *)scene2_Char, (void *)BG_VRAM);

        // Set maximum darken blend on all three BG layers + backdrop.
        // Screen starts black and fades in during state 1.
        SetBLDDownMax(BLDCNT_TGT1_BD | BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG1 | BLDCNT_TGT1_BG2);

        // Enable display with all three BG layers
        REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_BG2_ON;

        V_Wait();

        // Set first palette color to white for headlight/reflection effect.
        // Palette entry 0 is the transparent/background color; setting it
        // to white (0x7FFF) provides a bright backdrop for the car reflection.
        ((vu16 *)BG_PLTT)[0] = 0x7FFF;

        sLocalSeq++;
        break;

    case 1:
        // ---- Fade in, scroll car from left toward center ----
        // FadeDec(15): decrease darken every 15th frame (~4 times/sec at 60fps)
        FadeDec(15);

        // Car scrolls right from -192 toward 0 (center of screen)
        // Moving every 4th frame makes the car approach smoothly.
        if (bg_scroll_x < -24 && (time & 3) == 3)
        {
            bg_scroll_x++;
            REG_BG0HOFS = bg_scroll_x;
            REG_BG1HOFS = bg_scroll_x;
        }

        // After 510 frames (~8.5 seconds), transition to main animation.
        // This gives enough time for the fade-in to complete (~16*4=64f for
        // fade) plus several seconds of the car sitting stationary before
        // animation begins.
        if (time == 510)
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 2:
        // ---- Main animation loop ----
        // This state runs until uLocalTime reaches 396, then advances scene.

        // === Door, ground, and tire pattern changes ===
        // The car has 4 animation frames cycled at specific times:
        //   Frame 0 -> 1 at t=32 (first tire rotation, ~0.5 sec)
        //   Frame 1 -> 2 at t=64 (second rotation)
        //   Frame 2 -> 3 at t=146 (third rotation, longer gap for impact effect)
        if (uLocalTime == 32)
            Scene2_SetPattern(1);
        else if (uLocalTime == 64)
            Scene2_SetPattern(2);
        else if (uLocalTime == 146)
            Scene2_SetPattern(3);

        // === End of scene ===
        // After 396 frames (~6.6 seconds of animation), advance to next scene.
        if (uLocalTime == 396)
        {
            sGameSeq++;
            break;
        }

        // === BG2 shutter scroll (vertical film-reel transition) ===
        // The shutter/grate layer scrolls upward. Right-shifting uLocalTime
        // divides by 16, giving a slow, smooth vertical scroll.
        // At t=396, shutter has scrolled 396/16 = 24 pixels = ~3 tiles.
        REG_BG2VOFS = uLocalTime >> 4;

        // === Car body horizontal deceleration ===
        // The car continues drifting right from -24 toward its target at +16.
        // Speed decreases in stages for a smooth stop:
        //   >-8: move every 8th frame (slowest -- approaching final position)
        //   >-4: move every 16th frame
        //   else: move every 32nd frame
        // Final target: bg_scroll_x reaches 16 (car centered with slight offset)
        if (bg_scroll_x < 16)
        {
            if (bg_scroll_x < -8 && (time & 7) == 7)
                bg_scroll_x++;
            else if (bg_scroll_x < -4 && (time & 15) == 15)
                bg_scroll_x++;
            else if ((time & 31) == 31)
                bg_scroll_x++;

            // Apply scroll to BG0 (upper body) and BG1 (lower body)
            // Mask with 0x1FF: horizontal scroll wraps at 512 pixels
            // (the width of the 512x256 BG mode)
            REG_BG0HOFS = bg_scroll_x & 0x1FF;
            REG_BG1HOFS = bg_scroll_x & 0x1FF;
        }

        // === BG0 vertical wobble (bumpy road effect) ===
        // Two wobble tables for different intensities:
        //   Before t=64:  softer wobble (values 0,2,4,2) -- smoother road
        //   After t=146: harder wobble (values 0,3,6,3) -- rougher terrain
        //   Between 64-146: medium wobble (values 0,2,4,2)
        {
            static const u8 wobble_hard[] = { 0, 3, 6, 3 };
            static const u8 wobble_soft[] = { 0, 2, 4, 2 };

            if (uLocalTime > 146)
                REG_BG0VOFS = wobble_hard[uLocalTime & 3];
            else if (uLocalTime > 64)
                REG_BG0VOFS = wobble_soft[(uLocalTime >> 1) & 3];
        }

        uLocalTime++;
        break;
    }
}

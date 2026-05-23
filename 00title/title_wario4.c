// Wario4 Title Logo Scene - "WARIO LAND 4" main title screen
//
// Wario4 Title Logo Scene — "WARIO LAND 4" main title screen
//
// Display layers:
//   BG0: 512x256 16-color - night cityscape / title logo
//   BG1: 256x256 16-color - parallax background (far buildings)
//   BG2: 512x256 16-color - parallax background (mid buildings)
//   BG3: 512x256 16-color - parallax background (near buildings)
//   OBJ: sprites for car, smoke, rocks, PUSH START text
//
// Night->day transition:
//   BG0 starts at Y=80 (night scene visible), blends against BG1-3.
//   As EVA decreases (16->0) and EVB increases (0->16), night fades
//   out and day parallax backgrounds fade in.
//
// Exec state machine (sLocalSeq):
//   0: LZ77 decompress BG tiles -> VRAM
//   1: LZ77 decompress OBJ tiles, enable display
//   2: Night->day scroll + blend transition
//   3: Clear BG0 rows, then UnPackScreen logo tilemap
//   4: Display logo, wait ~5s, blend transition
//   5-6: Brightness clamp, show stable title screen
//   7: Wait for START/A input or auto-demo timeout (~60s)
//   8: Setup exit animation positions
//   9-10: Car moves up/left off screen
//   11: Fade out, advance to TSEQ_PUSH_START

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

// ---- External data references ----
// From wario4_data.c (BG palette + LZ77 tiles)
extern const u16 scene5_bg_Palette[160];
extern const u8  scene5_bg_Char[];
extern const u8  scene5_bg_title_Char[];

// From wario4_scene_data.c (tilemaps + OBJ data + logo + car palette)
extern const u16 wario4_logo[];
extern const u16 warioGBA_logo[];
extern const u16 scene5_bg0_nightL[];
extern const u16 scene5_bg0_nightR[];
extern const u16 scene5_bg0_nightLR[];
extern const u16 scene5_bg1[];
extern const u16 scene5_bg2L[];
extern const u16 scene5_bg2R[];
extern const u16 scene5_bg3L[];
extern const u16 scene5_bg3R[];
extern const u16 scene5_obj_Palette[96];
extern const u8  scene5_obj_Char[];
extern const u16 scene5_car_Palette[256];

// OBJ pattern tables (from wario4_scene_data.c)
extern const u16 scene5_026[], scene5_027[], scene5_028[];  // car frames
extern const u16 scene5_030[], scene5_031[], scene5_032[];  // smoke
extern const u16 scene5_02F[];  // PUSH START text
extern const u16 scene5_00B[], scene5_00C[], scene5_00D[];  // hand cursor

// Global state variables (defined in title.c, IWRAM_DATA)
extern s16 sWork0, sWork1, sWork2, sWork3, sWork4, sWork5;
extern s16 bg_scroll_x, bg_scroll_y;
extern u16 uEVA, uEVB, uEVY;
extern u16 sLocalSeq;
extern u32 uLocalTime;
extern u8  bQuickStart, bMsgJapanese;
extern u16 *pObjEnd;
extern u32 uObjSize;

// ======================================================================
//  Helper functions
// ======================================================================

// SetObj and EndObj are now shared helpers defined in title.c,
// declared in title.h. Only Wario4-specific helpers remain here.

// DMA one row (16 colors = 32 bytes) of car palette to OBJ palette row 4.
// Index 0-15 selects which of the 16 pre-computed palettes to use.
// Called during night->day transition to animate the car's colors.
static void CarPaletteChange(int index)
{
    if ((unsigned)index > 15) return;
    REG_DMA3SAD = (u32)&scene5_car_Palette[index * 16];
    REG_DMA3DAD = (u32)(OBJ_PLTT + 0x20);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (32 >> 2);
}

// ======================================================================
//  Wario4_Init - Set up all BG layers, load tilemaps, configure blending
// ======================================================================
//
// Called once by title.c Initialize() before the Wario4 exec loop.
// Does NOT enable the display; Exec case 1 turns DISPCNT on.
//
// All four BG layers share charbase 0 (tiles at VRAM base).
// The scene5_bg_Char LZ77 data decompresses to tiles 0-357 (4bpp).
// The scene5_bg_title_Char decompresses to tiles 512-935 (4bpp).
//
void Wario4_Init(void)
{
    if (bQuickStart)
    {
        // Clear full BG VRAM and OBJ VRAM: prevents leftovers from previous scene
        {
            volatile u32 z = 0;
            REG_DMA3SAD = (u32)&z;
            REG_DMA3DAD = (u32)PLTT;
            REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (PLTT_SIZE >> 2);
        }
        {
            volatile u32 z = 0;
            REG_DMA3SAD = (u32)&z;
            REG_DMA3DAD = (u32)OBJ_VRAM0;
            REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x4000 >> 2);
        }
        {
            volatile u32 f = 0x03FF03FF;
            REG_DMA3SAD = (u32)&f;
            REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x8000);
            REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x4000 >> 2);
        }
        REG_BLDCNT = 0;
        REG_BLDALPHA = 0;
        REG_DISPCNT = DISPCNT_MODE_0;
        // m4aMPlayFadeOut, m4aSongNumStartOrChange TODO: sound
    }
    else
    {
        // Non-quickstart: clear BG2 left+right screenbases with 0x3FF
        // BG2_TOP = 0xA000, left half = screenbase 20, right = screenbase 21
        {
            volatile u32 v = 0x03FF03FF;
            REG_DMA3SAD = (u32)&v;
            REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0xA000);
            REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x400 >> 2);
            REG_DMA3SAD = (u32)&v;
            REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0xA800);
            REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x400 >> 2);
        }
    }

    // Load OBJ palette (from wario4_data.c: scene5_obj_Palette[96])
    REG_DMA3SAD = (u32)scene5_obj_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (96*2 >> 2);

    // Load BG palette (160 entries: night + parallax + logo colors)
    REG_DMA3SAD = (u32)scene5_bg_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (160*2 >> 2);

    // ---- BG0 tilemap ----
    // Normal intro: load night cityscape (3 overlapping RLE tilemaps)
    // Quick-start:  load title logo directly (skips night scene)
    if (bQuickStart)
    {
        UnPackScreen(wario4_logo, (vu16 *)((u8 *)BG_VRAM + 0x8000));
        CarPaletteChange(15);  // set car to daytime colors
    }
    else
    {
        // Left screen (first 1024 tiles): nightL then nightLR overlay
        UnPackScreen((const u16 *)scene5_bg0_nightL,  (vu16 *)((u8 *)BG_VRAM + 0x8000));
        UnPackScreen((const u16 *)scene5_bg0_nightLR, (vu16 *)((u8 *)BG_VRAM + 0x8000));
        // Right screen (second 1024 tiles): nightR then nightLR overlay
        UnPackScreen((const u16 *)scene5_bg0_nightR,  (vu16 *)((u8 *)BG_VRAM + 0x8800));
        UnPackScreen((const u16 *)scene5_bg0_nightLR, (vu16 *)((u8 *)BG_VRAM + 0x8800));
    }

    // ---- Parallax background tilemaps (BG1-BG3) ----
    // Addresses verified against original IDA decompilation:
    //   BG1:  screenbase 18 = VRAM + 0x9000 (256x256)
    //   BG2L: screenbase 20 = VRAM + 0xA000 (512x256 left half)
    //   BG2R: screenbase 21 = VRAM + 0xA800 (512x256 right half)
    //   BG3L: screenbase 22 = VRAM + 0xB000 (512x256 left half)
    //   BG3R: screenbase 23 = VRAM + 0xB800 (512x256 right half)
    //
    // In 512x256 mode each BG uses 2 consecutive screenbase blocks:
    //   BG2: screenbases 20+21 = VRAM + 0xA000 + 0xA800
    //   BG3: screenbases 22+23 = VRAM + 0xB000 + 0xB800
    UnPackScreen((const u16 *)scene5_bg1,  (vu16 *)((u8 *)BG_VRAM + 0x9000));
    UnPackScreen((const u16 *)scene5_bg2L, (vu16 *)((u8 *)BG_VRAM + 0xA000));
    UnPackScreen((const u16 *)scene5_bg2R, (vu16 *)((u8 *)BG_VRAM + 0xA800));
    UnPackScreen((const u16 *)scene5_bg3L, (vu16 *)((u8 *)BG_VRAM + 0xB000));
    UnPackScreen((const u16 *)scene5_bg3R, (vu16 *)((u8 *)BG_VRAM + 0xB800));

    // ---- BG control registers ----
    // All four BGs use charbase 0 (unified tile pool at VRAM base).
    // BG control register values:
    //   BG0CNT = 0x5000, BG1CNT = 0x1201, BG2CNT = 0x5402, BG3CNT = 0x5603
    REG_BG0CNT = BGCNT_TXT512x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);
    REG_BG1CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(18) | BGCNT_CHARBASE(0);
    REG_BG2CNT = BGCNT_TXT512x256 | BGCNT_16COLOR | BGCNT_PRIORITY(2)
               | BGCNT_SCREENBASE(20) | BGCNT_CHARBASE(0);
    REG_BG3CNT = BGCNT_TXT512x256 | BGCNT_16COLOR | BGCNT_PRIORITY(3)
               | BGCNT_SCREENBASE(22) | BGCNT_CHARBASE(0);

    // ---- Initial scroll ----
    // Quick-start: logo centered at Y=-2 (slight upward offset)
    // Normal:      night scene at Y=80 (show bottom of night cityscape)
    bg_scroll_x = 0;
    bg_scroll_y = bQuickStart ? -2 : 80;
    REG_BG0HOFS = 0;
    REG_BG0VOFS = bg_scroll_y;

    // ---- Blend setup ----
    // BG0 (night cityscape) = 1st target
    // BG1+BG2+BG3 (parallax backgrounds) = 2nd target
    // EVA=16, EVB=0: night fully visible, day backgrounds hidden
    // During transition: EVA decreases (night fades), EVB increases (day appears)
    // Original value: 0x0E41
    REG_BLDCNT = BLDCNT_TGT1_BG0
               | BLDCNT_TGT2_BG1 | BLDCNT_TGT2_BG2 | BLDCNT_TGT2_BG3
               | BLDCNT_EFFECT_BLEND;
    uEVA = 16;
    uEVB = 0;
    REG_BLDALPHA = (uEVB << 8) | uEVA;

    // ---- Car position and timing ----
    // sWork0: brightness target (320->109 during transition, 109=final)
    // sWork1: car Y position (348->148 during scroll, then moves offscreen)
    // sWork2: car X position (constant 150)
    sWork0 = bQuickStart ? 109 : 320;
    sWork1 = bQuickStart ? 148 : 348;
    sWork2 = 150;

    // sWork5: super-hard mode flag (all 12 treasures collected)
    sWork5 = (ucPerfect == 1);

    // Display stays off until Exec case 1 enables it
    REG_DISPCNT = DISPCNT_MODE_0;
}

// ======================================================================
//  Wario4_Exec - Per-frame execution (called each VBlank from GameTitle)
// ======================================================================
//
// The 'time' parameter is usFadeTimer (incremented each frame by GameTitle).
// Returns nothing; modifies sLocalSeq and sGameSeq to drive the state machine.
//
void Wario4_Exec(int time)
{
    switch (sLocalSeq)
    {
    // ================================================================
    // Case 0: Decompress BG tile data from ROM to VRAM
    // ================================================================
    // scene5_bg_Char:       LZ77 -> VRAM+0x0000 (tiles 0-357, 4bpp)
    //   Contains: night cityscape + parallax building tiles
    // scene5_bg_title_Char: LZ77 -> VRAM+0x4000 (tiles 512-935, 4bpp)
    //   Contains: "WARIO LAND 4" logo letter tiles
    // These are done in case 0 (not Init) to spread VRAM load across frames.
    case 0:
        LZ77UnCompVram((const u32 *)scene5_bg_Char, (void *)BG_VRAM);
        LZ77UnCompVram((const u32 *)scene5_bg_title_Char, (void *)((u8 *)BG_VRAM + 0x4000));
        sLocalSeq++;
        break;

    // ================================================================
    // Case 1: Decompress OBJ tiles, enable display hardware
    // ================================================================
    // Quick-start path: skips night->day transition, jumps to case 6.
    // Normal path:      continues to case 2 for the transition.
    // Display is enabled unconditionally (original sets DISPCNT=0x1F00).
    case 1:
        LZ77UnCompVram((const u32 *)scene5_obj_Char, (void *)OBJ_VRAM0);

        if (bQuickStart)
        {
            sLocalSeq = 6;
        }
        else
        {
            // Set up VCOUNT interrupt at scanline 151 for BG0 scroll
            gInterruptHandler.gVCountInterruptHandlerFunc = TitVCountIntr0;
            REG_DISPSTAT = (REG_DISPSTAT & 0xFF) | (151 << 8) | DISPSTAT_VCOUNT_INTR;
            REG_IE |= INTR_FLAG_VCOUNT;
            sLocalSeq++;
        }

        // Clear OBJ VRAM before decompressing new OBJ tiles.
        // Prevents leftover sprite tiles from previous scenes appearing.
        {
            volatile u32 z = 0;
            REG_DMA3SAD = (u32)&z;
            REG_DMA3DAD = (u32)OBJ_VRAM0;
            REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x4000 >> 2);
        }

        REG_DISPCNT = DISPCNT_MODE_0
                    | DISPCNT_OBJ_ON | DISPCNT_BG0_ON
                    | DISPCNT_BG1_ON | DISPCNT_BG2_ON
                    | DISPCNT_BG3_ON;
        V_Wait();
        break;

    // ================================================================
    // Case 2: Night->day scroll transition
    // ================================================================
    // The night scene (BG0) scrolls upward as the parallax backgrounds
    // (BG1-3) become visible through alpha blending.
    //
    // Scroll speed: starts fast, slows as Y approaches 0.
    //   Y>10: decrement every 4th frame
    //   Y>4:  decrement every 8th frame
    //   Y>0:  decrement every 16th frame
    //
    // After 400 frames: begin alpha blend transition.
    //   EVA: 16->0 (1st target = night = fades out)
    //   EVB: 0->16  (2nd target = day buildings = fades in)
    //   Car palette updates each blend step (0->15 = night->day colors)
    case 2:
        // Car scrolls upward with the night scene
        if (sWork1 > 148)
            sWork1--;

        // BG0HOFS is driven by VCOUNT handler TitVCountIntr0 (set up in case 1)
        // BG0VOFS is also driven by VCount (fixed at 95)

        // Scroll BG0 upward (Y decreases from 80 toward 0) via bg_scroll_y
        // This ALSO sets BG0VOFS here, but VCount handler overrides it each scanline
        if (bg_scroll_y > 0)
        {
            if (bg_scroll_y > 10 && (time & 3) == 3)
                bg_scroll_y--;
            else if (bg_scroll_y > 4 && (time & 7) == 7)
                bg_scroll_y--;
            else if ((time & 0xF) == 0xF)
                bg_scroll_y--;
        }
        REG_BG0VOFS = bg_scroll_y;

        // After initial scroll period, start blending from night to day
        if (time > 400 && (time & 0xF) == 0xF)
        {
            if (uEVA > 0) uEVA--;
            if (uEVB < 16) uEVB++;
            REG_BLDALPHA = (uEVB << 8) | uEVA;

            CarPaletteChange(uEVB);  // night->day car colors

            if (uEVB == 16)
            {
                // Blend done: clear VCOUNT interrupt, lock BG0VOFS at 0
                REG_IE &= ~INTR_FLAG_VCOUNT;
                gInterruptHandler.gVCountInterruptHandlerFunc = 0;
                REG_BG0VOFS = 0;
                sLocalSeq++;
            }
        }
        break;

    // ================================================================
    // Case 3: Clear night tilemap from BG0, replace with logo
    // ================================================================
    // Erases 8 rows of BG0 screen per frame (256 entries = 512 bytes each)
    // using DMA fill with blank tile 0x3FF. After all rows cleared,
    // UnPackScreen decompresses the logo tilemap onto BG0.
    case 3:
        if (uLocalTime <= 7)
        {
            u32 dst = (u32)((u8 *)BG_VRAM + 0x8000) + (uLocalTime << 9);
            volatile u32 blank = 0x03FF03FF;
            REG_DMA3SAD = (u32)&blank;
            REG_DMA3DAD = dst;
            REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (512 >> 2);
            uLocalTime++;
        }
        else
        {
            // Logo tilemap: wario4_logo (English) or warioGBA_logo (Japanese)
            UnPackScreen(bMsgJapanese ? (const u16 *)warioGBA_logo : (const u16 *)wario4_logo,
                         (vu16 *)((u8 *)BG_VRAM + 0x8000));

            bg_scroll_x = 0;
            bg_scroll_y = -2;  // small upward offset for logo centering
            REG_BG0HOFS = 0;
            REG_BG0VOFS = -2;

            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    // ================================================================
    // Case 4: Display logo, wait, then fade EVA/EVB
    // ================================================================
    // Shows the logo for ~300 frames (~5 seconds at 60fps).
    // Then reverses the blend: EVA increases (logo solidifies),
    // EVB decreases (parallax fades).
    case 4:
        if (uLocalTime++ <= 0x12C)
            break;

        if ((time & 0xF) == 0xF)
        {
            if (uEVA < 16) uEVA++;
            if (uEVB > 0) uEVB--;
            REG_BLDALPHA = (uEVB << 8) | uEVA;
        }

        if (uEVA == 16)
            sLocalSeq++;
        break;

    // ================================================================
    // Case 5: Rapidly reduce sWork0 toward brightness target
    // ================================================================
    // sWork0 starts at 320 and decreases by 8 per frame to 109.
    // Falls through to case 6 when stable.
    case 5:
        sWork0 -= 8;
        if (sWork0 <= 108)
            sWork0 = 109;

    // ================================================================
    // Case 6: Brightness reached — show stable title screen
    // ================================================================
    // Switch from alpha blend to darken effect on all BGs.
    // This creates the final title screen look with the logo clearly
    // visible against the darkened parallax backgrounds.
    case 6:
        if (sWork0 != 109)
            break;

        REG_BLDCNT = BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG1
                   | BLDCNT_TGT1_BG2 | BLDCNT_TGT1_BG3
                   | BLDCNT_EFFECT_DARKEN;
        uLocalTime = 0;
        sLocalSeq++;
        break;

    // ================================================================
    // Case 7: Wait for player input or auto-demo timeout
    // ================================================================
    // Shows "PUSH START" text. Waits for A or Start button, or
    // auto-advances to demo after ~60 seconds (3600 frames).
    case 7:
        if (usTrg)
        {
            uLocalTime = 0;
            uEVY = 0;
            REG_BLDY = 0;
        }

        if (++uLocalTime > 3600)
        {
            // Auto-demo timeout: fade to black
            if ((usFadeTimer & 3) == 3)
            {
                if (uEVY < 16) uEVY++;
                REG_BLDY = uEVY;
            }
            if (uEVY == 16)
                sLocalSeq++;
        }
        else if (usTrg == A_BUTTON || usTrg == START_BUTTON)
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    // ================================================================
    // Case 8: Short delay before exit animation
    // ================================================================
    // Sets up animation target positions after 16 frames.
    case 8:
        if (uLocalTime++ > 15)
        {
            sWork3 = sWork1 + 40;
            sWork4 = sWork2 - 14;
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    // ================================================================
    // Case 9-10: Car moves up and left off screen
    // ================================================================
    // Each frame: sWork1 (car Y) decreases by 4.
    // When car is fully offscreen (Y < -100), switch to darken effect.
    case 9:
    case 10:
        sWork1 -= 4;
        if (sWork1 < -100)
        {
            REG_BLDCNT = BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG1
                       | BLDCNT_TGT1_BG2 | BLDCNT_TGT1_BG3
                       | BLDCNT_EFFECT_DARKEN;
            sLocalSeq++;
        }
        break;

    // ================================================================
    // Case 11: Fade to black, then advance to TSEQ_PUSH_START
    // ================================================================
    // FadeInc(0): EVY increments every frame (no interval divisor).
    // When fully dark (EVY=16), advance to next game sequence.
    case 11:
        if ((usFadeTimer & 0) == 0)  // always true — fade every frame
        {
            if (uEVY < 16) uEVY++;
            REG_BLDY = uEVY;
        }
        if (uEVY == 16)
            sGameSeq++;
        break;

    default:
        break;
    }

    // ---- BG parallax scrolling (all states, matches IDA) ----
    REG_BG1HOFS = -(time << 2);         // -4 * time  (fastest)
    REG_BG2HOFS = -(s16)(time >> 3);    // -time/8   (medium)
    REG_BG3HOFS = -(s16)(time >> 4);    // -time/16  (slowest)

    // ---- OBJ sprite rendering (matching IDA Wario4_Exec) ----
    // Uses SetObj with position offsets: car at (sWork2, sWork1),
    // smoke at (sWork0, 76), cursor/text at (74, 76).
    // Original calls scene5_car_move (complex multi-part car) and
    // scene5_Anime15-18 for animation patterns.
    {
        u16 *dst = (u16 *)OamBuf;
        const u16 *pattern;

        // Car + smoke: visible during states 2-10
        if (sLocalSeq >= 2 && sLocalSeq <= 10)
        {
            // Car body at (sWork2, sWork1) with affine params
            SetObjPABCD(0, 0, 0x100, 0x100);  // identity matrix

            // Animate car body (scene5_Anime15: 42-frame cycle)
            scene5_Anime15(time, &pattern);
            dst = SetObj(pattern, dst, sWork2, sWork1);

            // Smoke exhaust at (sWork0, 76) — scene5_Anime16 pattern
            scene5_Anime16(&pattern);
            dst = SetObj(pattern, dst, sWork0, 76);
        }

        // PUSH START hand cursor: visible during state 7
        if (sLocalSeq == 7)
        {
            scene5_Anime6(time, &pattern);
            dst = SetObj(pattern, dst, 74, 76);
        }

        // PUSH START text: visible during state 7+
        if (sLocalSeq >= 7)
        {
            scene5_Anime7(time, &pattern);
            dst = SetObj(pattern, dst, 74, 76);
        }

        EndObj(dst);
    }

    // ---- Quick-start check ----
    // A or Start during early states (before case 6) skips to Wario4 title.
    if (sLocalSeq <= 5 && (usTrg & (A_BUTTON | START_BUTTON)))
        sGameSeq = TSEQ_QUICK_START;
}

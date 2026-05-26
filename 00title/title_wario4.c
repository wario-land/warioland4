// Wario4 Title Logo Scene - "WARIO LAND 4" main title screen
//
// Wario4 Title Logo Scene -- "WARIO LAND 4" main title screen
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
extern u16 bg_rotate, bg_scale_x, bg_scale_y;
extern u16 uEVA, uEVB, uEVY;
extern u16 sLocalSeq;
extern u32 uLocalTime;
extern u8  bQuickStart, bMsgJapanese;
extern u16 *pObjEnd;
extern u32 uObjSize;
extern const s16 sin_cos_table[256 + 64];

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
//  Scene5_CarMove -- Multi-part car OBJ rendering
// ======================================================================
// Matches IDA scene5_car_move at 0x80065ec.
// Renders the complete car (10+ OBJs): two affine halves, top, 4 rock
// particles behind, shake effect, anime parts, tires, and other details.
// Returns the next OAM destination pointer.
//
// Parameters: sLocalSeq (current Wario4 state), time (frame counter)
// Global state: sWork1=car Y, sWork2=car X, bg_rotate=shake phase
//
u16 *Scene5_CarMove(int sLocalSeq, int time)
{
    u16 *dst = (u16 *)OamBuf;
    const u16 *pattern;
    int rock_x, shake_y;
    int car_y = sWork1;
    int car_x = sWork2;

    // Affine car halves (only during state 2: night scene)
    if (sLocalSeq == 2)
    {
        scene5_Anime10(time, &pattern);
        dst = SetObj(pattern, dst, car_x, car_y);

        scene5_Anime11(time, &pattern);
        dst = SetObj(pattern, dst, car_x, car_y);

        bg_rotate = 0;
    }

    // Car top detail (states 7+: after night transition)
    if (sLocalSeq > 6)
    {
        scene5_Anime14(&pattern);
        dst = SetObj(pattern, dst, car_x, car_y);
    }

    // Rock/smoke particles behind car + car shake (states 3+)
    if (sLocalSeq > 2)
    {
        // 4 rock particles scroll horizontally with time, Y=400 (below screen)
        // Each particle has a different phase offset
        rock_x = (8 * time + 500) & 0x7FF;
        if (rock_x > 400) rock_x = 400;
        scene5_Rock1(&pattern);
        dst = SetObj(pattern, dst, rock_x, 400);

        rock_x = (8 * time + 800) & 0x7FF;
        if (rock_x > 400) rock_x = 400;
        scene5_Rock2(&pattern);
        dst = SetObj(pattern, dst, rock_x, 400);

        rock_x = (8 * time + 1300) & 0x7FF;
        if (rock_x > 400) rock_x = 400;
        scene5_Rock3(&pattern);
        dst = SetObj(pattern, dst, rock_x, 400);

        rock_x = (8 * time + 2000) & 0x7FF;
        if (rock_x > 400) rock_x = 400;
        scene5_Rock4(&pattern);
        dst = SetObj(pattern, dst, rock_x, 400);

        // Car shake: Y oscillates using sin table at bg_rotate phase
        bg_rotate++;
        shake_y = car_y - (s16)((s32)sin_cos_table[(u8)bg_rotate] * 4 / 256);
        scene5_Anime2(time, &pattern);
        dst = SetObj(pattern, dst, car_x, shake_y);
    }

    // Car detail animations (all states, rendered on top of car body)
    scene5_Anime9(time, &pattern);
    dst = SetObj(pattern, dst, car_x, car_y);

    scene5_Anime1(time, &pattern);
    dst = SetObj(pattern, dst, car_x, car_y);

    // Two tires
    scene5_Tire(&pattern);
    dst = SetObj(pattern, dst, car_x, car_y);
    dst = SetObj(pattern, dst, car_x, car_y);

    // Final car animation layer
    scene5_Anime0(time, &pattern);
    dst = SetObj(pattern, dst, car_x, car_y);

    // Apply affine matrix for car scaling/rotation
    SetObjPABCD(0, bg_rotate, bg_scale_x, bg_scale_y);

    return dst;
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
        // IDA Init at 0x8003d5c: only clears palette RAM, BLDCNT, BLDALPHA, DISPCNT.
        // Does NOT clear VRAM -- preserving previous scene's tile data.
        TitleInit();
        // m4aMPlayFadeOut(&m4a_mplay001, 12);  // TODO: sound
        // m4aSongNumStartOrChange(299);         // TODO: sound
        // m4aSongNumStartOrChange(636);         // TODO: sound
    }
    else
    {
        // Non-quickstart: Fill screenbases 20-21 (BG2 area, VRAM+0xA000/0xA800)
        // with blank tile 0x3FF. Matches IDA Wario4_Init at 0x8005e34:
        // two fills ctx=0x85000100 to 0x600A000 and 0x600A800.
        {
            volatile u32 v = 0x03FF03FF;
            REG_DMA3SAD = (u32)&v;
            REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0xA000);
            REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x800 >> 2);
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
        // IDA: selects warioGBA_logo (Japanese) or wario4_logo (English)
        // based on bMsgJapanese flag.
        UnPackScreen(bMsgJapanese ? (const u16 *)warioGBA_logo : (const u16 *)wario4_logo,
                     (vu16 *)((u8 *)BG_VRAM + 0x8000));
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
    // Clear OBJ VRAM FIRST (before decompression!) to prevent leftover
    // sprite tiles from previous scenes appearing.
    // Original IDA does NOT clear OBJ VRAM at all -- LZ77 overwrites what it needs.
    // We clear here defensively but BEFORE the decompression.
    case 1:
        // Decompress OBJ tiles to VRAM. IDA does NOT clear OBJ VRAM first --
        // the LZ77 decompression writes only the tiles it needs, and leftover
        // tile data from previous scenes may be referenced by shared OBJ patterns.
        LZ77UnCompVram((const u32 *)scene5_obj_Char, (void *)OBJ_VRAM0);

        if (bQuickStart)
        {
            sLocalSeq = 6;
        }
        else
        {
            // Set up VCOUNT interrupt at scanline 151 for BG0 night-scene parallax.
            // TitVCountIntr0 overrides BG0VOFS per scanline to create the
            // horizontal scroll shearing effect during the night->day transition.
            gInterruptHandler.gVCountInterruptHandlerFunc = TitVCountIntr0;
            REG_DISPSTAT = (REG_DISPSTAT & 0xFF) | (151 << 8) | DISPSTAT_VCOUNT_INTR;
            REG_IE |= INTR_FLAG_VCOUNT;
            sLocalSeq++;
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

        // BG0HOFS = -2 * time: horizontal parallax driven per-frame
        // IDA: MEMORY[0x4000010] = -2 * a1
        REG_BG0HOFS = -(time << 1);

        // Scroll BG0 upward (Y decreases from 80 toward 0) via bg_scroll_y
        // VCount handler TitVCountIntr0 overrides BG0VOFS per scanline for
        // the night-scene parallax effect (REG_BG0VOFS=95, HOFS based on usFadeTimer)
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
    // Case 6: Brightness reached -- show stable title screen
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
            // Auto-demo timeout: fade to black, then start demo
            FadeInc(3);
            if (uEVY == 16)
            {
                // AutoDemo_Ready_Run();  // TODO: auto-demo (IDA 0x807504c)
                sLocalSeq++;
            }
        }
        else if (usTrg == A_BUTTON || usTrg == START_BUTTON)
        {
            // m4aSongNumStartOrChange(298);  // TODO: sound
            // WarioVoiceSet(0);              // TODO: sound (IDA 0x8085bdc)
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
        if (FadeInc(0))
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
    // IDA renders in this order:
    //   1. scene5_car_move (multi-part car with affine) -- ALWAYS rendered
    //   2. scene5_Anime16 (smoke exhaust) -- when sLocalSeq > 4
    //   3. scene5_Anime6 (hand cursor) -- when sLocalSeq == 7
    //   4. scene5_Anime7 (PUSH START text) -- when sLocalSeq > 7
    {
        u16 *dst = (u16 *)OamBuf;
        const u16 *pattern;

        // Multi-part car: car body halves, tires, rocks, shake, details
        dst = Scene5_CarMove(sLocalSeq, time);

        // Smoke exhaust at (sWork0, 76) -- state 5+ (matching IDA: sLocalSeq > 4)
        if (sLocalSeq > 4)
        {
            scene5_Anime16(&pattern);
            dst = SetObj(pattern, dst, sWork0, 76);
        }

        // Hand cursor at (74, 76) -- state 7 only
        // TODO: super-hard mode (sWork5!=0) uses scene5_Anime17 (IDA 0x800c340)
        if (sLocalSeq == 7)
        {
            scene5_Anime6(time, &pattern);
            dst = SetObj(pattern, dst, 74, 76);
        }

        // PUSH START text at (74, 76) -- states 8+
        // TODO: super-hard mode (sWork5!=0) uses scene5_Anime18 (IDA 0x800c37c)
        if (sLocalSeq > 7)
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

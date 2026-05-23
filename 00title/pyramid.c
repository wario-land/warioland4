// Pyramid intro scene — Wario's car drives toward pyramid entrance
//
// This is the first scene of the "pyramid" story branch of the title sequence.
// Wario's car arrives at the desert pyramid, rocks slide apart, and Wario
// strikes a "guts pose" before entering.
//
// BG layers:
//   BG0: Desert ground (256x256, screenbase 16) — repeating tilemap
//        Fills 18 rows with pyramid_bg0 pattern for scrolling ground.
//   BG1: Near pyramid (256x256, screenbase 17) — rocks + foreground elements
//   BG2: Mid pyramid (256x256, screenbase 18) — pyramid mid section
//   BG3: Far pyramid (256x256, screenbase 19) — distant background
//
// OBJ: Wario (scaled with affine), rocks left+right (slide apart),
//      grass sprites, smoke particles from rock movement.
//
// Vertical parallax: BG1-3 scroll at different speeds for depth effect.
// BG3 (far) scrolls slowest, BG1 (near) scrolls fastest.
//
// Blend: BG0 (1st target) cross-fades against BG2+BG3 (2nd target)
// for a fog/sand effect as the pyramid approaches.
//
// States (sLocalSeq):
//   S6_1: Setup initial positions, OBJ scale, rock positions
//   S6_2: Blend setup, wait 125 frames (dramatic pause)
//   S6_3: Rocks spread apart, Wario shrinks (from 1.25x to 1.0x), BG scrolls up
//   S6_4: Wait pose, BG continues scrolling, blend transition (uEVA→0, uEVB→16)
//   S6_5: Wario "guts pose" (pyramid_Anime0), voice line
//   S6_6: Wait pose (pyramid_Anime1), then prepare for exit
//   S6_7: Exit — Wario shrinks and moves down, fade to white
//   S6_8: Fade to white complete, advance scene

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

// ---- Pyramid data ----
extern const u16 pyramid_bg_Palette[64];
extern const u8  pyramid_bg_Char[];
extern const u16 pyramid_bg0[];      // Desert ground tilemap (32x1, repeated)
extern const u16 pyramid_bg1[];      // Near pyramid (rocks)
extern const u16 pyramid_bg2[];      // Mid pyramid
extern const u16 pyramid_bg3[];      // Far pyramid
extern const u16 pyramid_obj_Palette[48];
extern const u8  pyramid_obj_Char[];

// ---- Global state ----
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

// Local smoke position arrays (8 particles max)
// Not in IWRAM to save RAM — these are per-scene local state
static s16 pyramid_smoke_x[8], pyramid_smoke_y[8];

void Pyramid_Init(void)
{
    int i;
    vu16 *dst;

    // ---- Load BG palette (64 entries) ----
    REG_DMA3SAD = (u32)pyramid_bg_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (64*2 >> 2);

    // ---- Load OBJ palette (48 entries) ----
    REG_DMA3SAD = (u32)pyramid_obj_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (48*2 >> 2);

    // ---- Fill tile 0x3FC with white (used for edge masking) ----
    {
        volatile u32 f = 0xFFFFFFFF;
        REG_DMA3SAD = (u32)&f;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 32*1020);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (32 >> 2);
    }

    // ---- Decompress BG and OBJ tiles ----
    LZ77UnCompVram((const u32 *)pyramid_bg_Char, (void *)BG_VRAM);
    LZ77UnCompVram((const u32 *)pyramid_obj_Char, (void *)OBJ_VRAM0);

    // ---- BG0: Repeat desert ground strip across 18 rows ----
    // pyramid_bg0 is a 32x1 tilemap (32 u16 entries). DMA it 18 times
    // to fill 18 rows of screenbase 16, creating a scrolling ground.
    dst = (vu16 *)((u8 *)BG_VRAM + 0x8000);
    for (i = 0; i < 18; i++, dst += 32)
    {
        REG_DMA3SAD = (u32)pyramid_bg0;
        REG_DMA3DAD = (u32)dst;
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (32*2 >> 2);
    }

    // ---- Unpack BG tilemaps ----
    // Screenbase 17 (0x8800): BG1 near pyramid
    // Screenbase 18 (0x9000): BG2 mid pyramid
    // Screenbase 19 (0x9800): BG3 far pyramid
    UnPackScreen((const u16 *)pyramid_bg1, (vu16 *)((u8 *)BG_VRAM + 0x8800));
    UnPackScreen((const u16 *)pyramid_bg2, (vu16 *)((u8 *)BG_VRAM + 0x9000));
    UnPackScreen((const u16 *)pyramid_bg3, (vu16 *)((u8 *)BG_VRAM + 0x9800));

    // ---- BG3 scroll wrap setup ----
    // Copy the end of BG3 screenbase to positions 464, 480, 496 so that
    // when scrolling, the tilemap wraps seamlessly without gaps.
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

    // ---- BG control registers ----
    // BG0: ground, priority 1 (behind rocks but in front of pyramid)
    // BG1: near pyramid/rocks, priority 0 (frontmost BG)
    // BG2: mid pyramid, priority 2
    // BG3: far pyramid, priority 3 (backmost)
    REG_BG0CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);
    REG_BG1CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(17) | BGCNT_CHARBASE(0);
    REG_BG2CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(2)
               | BGCNT_SCREENBASE(18) | BGCNT_CHARBASE(0);
    REG_BG3CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(3)
               | BGCNT_SCREENBASE(19) | BGCNT_CHARBASE(0);

    // Display off at init, enabled in Exec
    REG_DISPCNT = DISPCNT_MODE_0;
}

void Pyramid_Exec(int time)
{
    u16 *dst;
    const u16 *wario_pattern = 0;
    int i;

    switch (sLocalSeq)
    {
    case 0:
        // ---- S6_1: Setup initial positions ----
        // Wario starts large (1.25x scale) at bottom center
        ob_rotate = 0;
        ob_scale_x = 0x140;
        ob_scale_y = 0x140;
        ob_pos_x = 120;
        ob_pos_y = 180;

        // Smoke particles originate from Wario's feet
        // Position = Wario Y + (16 * scale / 256) = feet offset at current scale
        sWork6 = ob_pos_x;
        sWork7 = ob_pos_y + (s16)((s32)16 * ob_scale_y / 256);
        for (i = 0; i < 4; i++)
        {
            pyramid_smoke_y[i] = ob_pos_y + (s16)((s32)16 * ob_scale_y / 256) + i * 6;
            pyramid_smoke_x[i] = ob_pos_x;
        }

        // BG scroll positions
        sWork0 = 0;      // bg1_y: near pyramid vertical scroll
        sWork1 = -24;    // bg2_y/mid + bg3/far start above frame
        REG_BG2VOFS = sWork1;
        REG_BG3VOFS = sWork1;

        // Rocks start at center, will slide apart
        sWork2 = 120;    // left rock X
        sWork3 = 64;     // left rock Y
        sWork4 = 120;    // right rock X
        sWork5 = 64;     // right rock Y

        uLocalTime = 0;
        sLocalSeq++;
        break;

    case 1:
        // ---- S6_2: Blend setup, wait 125 frames ----
        // Cross-fade between BG0 (ground) and BG2+BG3 (pyramid) for fog effect.
        // EVA=16, EVB=0: ground fully visible, pyramid hidden
        if (uLocalTime == 0)
        {
            uEVA = 17;    // Will decrement to 16
            uEVB = 0;
            REG_BLDALPHA = 16;  // EVA=16 (ground visible), EVB=0 (pyramid hidden)
            REG_BLDCNT = BLDCNT_TGT2_BG2 | BLDCNT_TGT2_BG3
                       | BLDCNT_EFFECT_BLEND | BLDCNT_TGT1_BG0;

            // Enable display: all BGs + OBJ
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

    case 2:
        // ---- S6_3: Rocks spread, Wario shrinks, BG scrolls up ----
        // Rocks move apart horizontally, revealing the pyramid entrance
        if (sWork2 > 64 && (time & 3) == 3)
        {
            sWork2--;    // left rock moves left
            sWork4++;    // right rock moves right
        }
        // Rocks and ground scroll upward together
        if (sWork3 < 72 && (time & 31) == 31)
        {
            sWork3++;    // rock Y increases (moves down on screen = ground scrolls up)
            sWork5++;
            REG_BG1VOFS = --sWork0;   // BG1 (near) scrolls up
        }
        // Wario: shrink from 1.25x to 1.0x while moving up
        if (ob_pos_y > 116 && (time & 3) == 3)
        {
            ob_pos_y--;
            ob_scale_x--;
            ob_scale_y--;
        }
        // Update smoke positions to follow Wario
        for (i = 0; i < 4; i++)
        {
            pyramid_smoke_y[i] = ob_pos_y + (s16)((s32)16 * ob_scale_y / 256) + i * 6;
        }

        if (ob_scale_x == 0x100)   // Wario reached normal scale
        {
            ob_pos_y += 18;         // Final position adjustment
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 3:
        // ---- S6_4: Wait pose, BG scroll, blend transition ----
        // During frames 32-96: scroll all BGs up for dramatic entrance
        if (uLocalTime >= 32 && uLocalTime < 96 && (uLocalTime & 7) == 7)
        {
            REG_BG1VOFS = --sWork0;    // BG1 scrolls up
            REG_BG2VOFS = ++sWork1;    // BG2 (mid pyramid) scrolls down
            REG_BG3VOFS = sWork1;     // BG3 (far) follows mid
            sWork3++;    // Rocks adjust Y
            sWork5++;
            ob_pos_y++;
        }
        uLocalTime++;

        // Blend transition: fade from ground to pyramid view
        // EVA (ground) decreases, EVB (pyramid) increases
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

    case 4:
        // ---- S6_5: Wario "guts pose" (pyramid_Anime0) ----
        // 72-frame animation loop showing Wario celebrating with treasure
        uLocalTime++;
        pyramid_Anime0(uLocalTime, &wario_pattern);

        if (uLocalTime == 32)
        {
            // WarioVoiceSet(WV_HAPPY_2);  // voice line at frame 32, TODO: sound
        }
        if (uLocalTime > 50)
        {
            sLocalSeq++;
        }
        break;

    case 5:
        // ---- S6_6: Wait pose (pyramid_Anime1) ----
        if (++uLocalTime > 110)
        {
            ob_pos_y -= 18;    // Reset position for exit animation
            uLocalTime = 0;
            sLocalSeq++;
        }
        pyramid_Anime1(uLocalTime, &wario_pattern);
        break;

    case 6:
        // ---- S6_7: Exit — Wario shrinks and moves down ----
        if ((time & 3) == 3)
        {
            if (ob_pos_y < 160)
            {
                // Wario shrinks while falling off screen
                ob_pos_y++;
                ob_scale_x -= 4;
                ob_scale_y -= 4;
            }
            else
            {
                // Switch to white fade out
                SetBLDUpMin(BLDCNT_TGT1_BD | BLDCNT_TGT1_OBJ
                          | BLDCNT_TGT1_BG3 | BLDCNT_TGT1_BG2
                          | BLDCNT_TGT1_BG1);
                // Disable BG0 (ground) — show only pyramid BGs + OBJ
                REG_DISPCNT = DISPCNT_OBJ_ON | DISPCNT_BG3_ON
                            | DISPCNT_BG2_ON | DISPCNT_BG1_ON;
                uLocalTime = 0;
                sLocalSeq++;
            }
        }
        sWork7 = ob_pos_y + (s16)((s32)16 * ob_scale_y / 256);
        break;

    case 7:
        // ---- S6_8: Fade to white, advance to next scene ----
        FadeInc(7);
        if (uEVY == 16)
            sGameSeq++;
        break;
    }

    // ---- OBJ rendering ----
    dst = (u16 *)OamBuf;

    // Wario OBJ (always visible after display enabled)
    if (sLocalSeq > 0)
    {
        if (wario_pattern)
            dst = SetObj(wario_pattern, dst, ob_pos_x, ob_pos_y);
        else
        {
            // Default: use waiting pose for states without explicit pattern
            pyramid_Anime1(time, &wario_pattern);
            if (wario_pattern)
                dst = SetObj(wario_pattern, dst, ob_pos_x, ob_pos_y);
        }
    }

    // Rocks + grass (visible after display enabled)
    if (sLocalSeq > 0)
    {
        // Left rock + grass
        {
            static const u16 rock_l[] = { 1, 0x40A0, 0x8080, 0x0340 };
            dst = SetObj(rock_l, dst, sWork2, sWork3);
        }
        // Right rock + grass
        {
            static const u16 rock_r[] = { 1, 0x40A0, 0x8080, 0x0342 };
            dst = SetObj(rock_r, dst, sWork4, sWork5);
        }
    }

    // Smoke particles from rock movement (S6_3 to S6_5)
    if (sLocalSeq >= 2 && sLocalSeq < 6)
    {
        static const u16 smoke[] = { 1, 0x40A8, 0x8080, 0x0360 };
        for (i = 0; i < 4; i++)
            dst = SetObj(smoke, dst, pyramid_smoke_x[i], pyramid_smoke_y[i]);
    }

    EndObj(dst);
}

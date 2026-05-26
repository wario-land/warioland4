// Pyramid intro scene -- Wario arrives at pyramid, rocks slide apart
//
// BG layers: BG0=ground, BG1=near rocks, BG2=mid pyramid, BG3=far pyramid
// OBJ: Wario (affine scaled), rocks L+R (slide apart), grass, smoke particles
//
// States: 0=setup, 1=blend+wait, 2=rocks spread+shrink, 3=scroll+blend,
//         4=guts pose, 5=wait, 6=shrink+exit, 7=fade to white

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

// ---- Pyramid data ----
extern const u16 pyramid_bg_Palette[64];
extern const u8  pyramid_bg_Char[];
extern const u16 pyramid_bg0[];
extern const u16 pyramid_bg1[];
extern const u16 pyramid_bg2[];
extern const u16 pyramid_bg3[];
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

// Smoke particle Y positions (4 particles, works like saWork1[0-3] in IDA)
static s16 smoke_y[4];
// Smoke particle X positions (works like saWork0[0-3] in IDA)
static s16 smoke_x[4];

// Rock and grass OBJ pattern data -- matching IDA pyramid_iwa_L/R, pyramid_grass_L/R
// These are simple single-OBJ patterns rendered as non-affine sprites
static const u16 rock_L[] = { 1, 0x40A0, 0x8080, 0x0340 };
static const u16 rock_R[] = { 1, 0x40A0, 0x8080, 0x0342 };
static const u16 grass_L[] = { 1, 0x40A8, 0x8090, 0x0344 };
static const u16 grass_R[] = { 1, 0x40A8, 0x8070, 0x0346 };

void Pyramid_Init(void)
{
    int i;
    vu16 *dst;

    // Load BG palette (64 entries)
    REG_DMA3SAD = (u32)pyramid_bg_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (64*2 >> 2);

    // Load OBJ palette (48 entries)
    REG_DMA3SAD = (u32)pyramid_obj_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (48*2 >> 2);

    // Fill tile 0x3FE with white (matches IDA: 0x6007FC0, control 0x85000008)
    {
        volatile u32 f = 0xFFFFFFFF;
        REG_DMA3SAD = (u32)&f;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x7FC0);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (32 >> 2);
    }

    // Decompress BG and OBJ tiles
    LZ77UnCompVram((const u32 *)pyramid_bg_Char, (void *)BG_VRAM);
    LZ77UnCompVram((const u32 *)pyramid_obj_Char, (void *)OBJ_VRAM0);

    // BG0: Repeat desert ground strip across 18 rows (32x1 tilemap -> 32x18)
    dst = (vu16 *)((u8 *)BG_VRAM + 0x8000);
    for (i = 0; i < 18; i++, dst += 32)
    {
        REG_DMA3SAD = (u32)pyramid_bg0;
        REG_DMA3DAD = (u32)dst;
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (32*2 >> 2);
    }

    // Unpack BG tilemaps to screenbases 17, 18, 19
    UnPackScreen((const u16 *)pyramid_bg1, (vu16 *)((u8 *)BG_VRAM + 0x8800));
    UnPackScreen((const u16 *)pyramid_bg2, (vu16 *)((u8 *)BG_VRAM + 0x9000));
    UnPackScreen((const u16 *)pyramid_bg3, (vu16 *)((u8 *)BG_VRAM + 0x9800));

    // BG3 scroll wrap: copy start of BG3 screenbase to offsets for seamless scrolling
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

    // BG control registers (matching IDA: 0x1001, 0x1100, 0x1202, 0x1303)
    REG_BG0CNT = BGCNT_TXT256x256 | BGCNT_256COLOR | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);
    REG_BG1CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(17) | BGCNT_CHARBASE(0);
    REG_BG2CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(2)
               | BGCNT_SCREENBASE(18) | BGCNT_CHARBASE(0) | BGCNT_MOSAIC;
    REG_BG3CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(3)
               | BGCNT_SCREENBASE(19) | BGCNT_CHARBASE(0) | BGCNT_MOSAIC;

    REG_DISPCNT = DISPCNT_MODE_0;
}

void Pyramid_Exec(int time)
{
    u16 *dst;
    const u16 *wario_pattern = 0;
    const u16 *smoke_pattern = 0;
    int i, done;

    dst = (u16 *)OamBuf;

    switch (sLocalSeq)
    {
    case 0:
        // ---- S0: Setup initial positions ----
        // Wario starts at 1.25x scale, bottom center
        ob_rotate = 0;
        ob_scale_x = 0x140;   // 320 = 1.25x
        ob_scale_y = 0x140;
        ob_pos_x = 120;
        ob_pos_y = 180;

        sWork6 = 120;  // smoke base X

        // sWork7 = feet Y = ob_pos_y + FixMul(16, ob_scale_y)
        // FixMul(16, 0x140) = 16*320/256 = 20
        sWork7 = ob_pos_y + (s16)((s32)16 * ob_scale_y / 256);  // = 200

        // Fill smoke particle positions (matching IDA loop: v5=18,12,6,0)
        // saWork0 = smoke_x, saWork1 = smoke_y
        // The loop fills from index 3 down to 0 with v5 decreasing by 6
        {
            int v5 = 18;
            for (i = 3; i >= 0; i--, v5 -= 6)
            {
                smoke_y[i] = ob_pos_y + (s16)((s32)16 * ob_scale_y / 256) + v5;
                smoke_x[i] = ob_pos_x;
            }
        }

        // BG scroll positions
        sWork0 = 0;      // BG1 (near) vertical scroll counter
        sWork1 = -24;    // BG2/BG3 (mid/far) initial Y = -24 (above screen)
        REG_BG2VOFS = -24;
        REG_BG3VOFS = -24;

        // Rocks start at center, will spread apart
        sWork2 = 120;    // left rock X
        sWork3 = 64;     // left rock Y
        sWork4 = 120;    // right rock X
        sWork5 = 64;     // right rock Y

        // MPlayVolumeControl(&m4a_mplay000, 0xFFFF, 200);  // TODO: sound
        uLocalTime = 0;
        sLocalSeq++;
        break;

    case 1:
        // ---- S1: Blend setup + wait 125 frames ----
        if (uLocalTime == 0)
        {
            uEVA = 17;
            uEVB = 0;
            REG_BLDALPHA = (0 << 8) | 16;   // EVA=16 (ground visible), EVB=0 (pyramid hidden)
            REG_BLDCNT = BLDCNT_TGT2_BG2 | BLDCNT_TGT2_BG3
                       | BLDCNT_EFFECT_BLEND | BLDCNT_TGT1_BG0;
            REG_DISPCNT |= DISPCNT_OBJ_ON | DISPCNT_BG0_ON
                        | DISPCNT_BG1_ON | DISPCNT_BG2_ON | DISPCNT_BG3_ON;
        }
        if (uLocalTime == 125)
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        uLocalTime++;
        break;

    case 2:
        // ---- S2: Rocks spread apart, Wario shrinks, BG scrolls up ----
        // Rocks move: left rock goes left, right rock goes right
        if (sWork2 > 64 && (time & 3) == 3)
        {
            sWork2--;
            sWork4++;
            uLocalTime++;
            // pyramid_palette_change(time);  // TODO: palette color cycling (IDA 0x800c598)
        }
        // Rocks + BG1 scroll upward together
        if (sWork3 < 72 && (time & 31) == 31)
        {
            sWork3++;
            sWork5++;
            REG_BG1VOFS = --sWork0;
        }
        // Wario shrinks from 1.25x to 1.0x while moving up
        if (ob_pos_y > 116 && (time & 3) == 3)
        {
            ob_pos_y--;
            ob_scale_x--;
            ob_scale_y--;
        }

        // Render 4 smoke particles using pyramid_Anime3 (matching IDA)
        // Each particle has its own animation phase offset by 6 frames
        {
            int v8 = 3;
            int v10 = time + 18;
            int v11 = 18;
            s16 *sy = &smoke_y[3];
            for (i = 3; i >= 0; i--, v8--, v10 -= 6, v11 -= 6, sy--)
            {
                if (pyramid_Anime3(v10, &smoke_pattern))
                    *sy = ob_pos_y + (s16)((s32)16 * ob_scale_y / 256) + v11;
                dst = SetObj(smoke_pattern, dst,
                             sWork6 + (v8 & 1 ? 4 : -4) * v8,
                             *sy);
            }
        }

        // Wario: pyramid_Anime2 (shrinking pose)
        pyramid_Anime2(time, &wario_pattern);
        dst = SetObj(wario_pattern, dst, ob_pos_x, ob_pos_y);

        // Check if Wario reached normal scale -- advance state
        if (ob_scale_x == 0x100)
        {
            ob_pos_y += 18;    // Final position adjustment
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 3:
        // ---- S3: BG scroll + blend transition ----
        // Wario: pyramid_Anime1 (waiting pose)
        pyramid_Anime1(time, &wario_pattern);
        dst = SetObj(wario_pattern, dst, ob_pos_x, ob_pos_y);

        // BG scroll during frames 32-96
        if (uLocalTime >= 32 && uLocalTime < 96 && (uLocalTime & 7) == 7)
        {
            REG_BG1VOFS = --sWork0;     // BG1 scrolls up
            REG_BG2VOFS = ++sWork1;     // BG2 scrolls down
            REG_BG3VOFS = sWork1;      // BG3 follows BG2
            sWork3++;
            sWork5++;
            ob_pos_y++;
        }
        uLocalTime++;

        // Blend: EVA (ground) decreases, EVB (pyramid) increases
        if (uEVA)
        {
            if ((time & 7) == 7)
            {
                uEVB++;
                uEVA--;
                REG_BLDALPHA = (uEVB << 8) | uEVA;
            }
        }
        else
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 4:
        // ---- S4: Wario "guts pose" (pyramid_Anime0) ----
        done = pyramid_Anime0(uLocalTime++, &wario_pattern);
        dst = SetObj(wario_pattern, dst, ob_pos_x, ob_pos_y);

        if (uLocalTime == 32)
        {
            // WarioVoiceSet(0);  // TODO: sound (IDA 0x8085bdc)
        }
        if (done)
        {
            // Animation cycle complete -- advance after brief hold (uLocalTime=20)
            uLocalTime = 20;
            sLocalSeq++;
        }
        break;

    case 5:
        // ---- S5: Wait pose (pyramid_Anime1), hold for 111 frames ----
        pyramid_Anime1(uLocalTime, &wario_pattern);
        dst = SetObj(wario_pattern, dst, ob_pos_x, ob_pos_y);

        if (++uLocalTime > 110)
        {
            ob_pos_y -= 18;    // Reset position for exit
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 6:
        // ---- S6: Shrink + exit, Wario falls off screen ----
        if ((time & 3) == 3)
        {
            if (ob_pos_y < 160)
            {
                ob_pos_y++;
                ob_scale_x -= 4;
                ob_scale_y -= 4;
            }
            else
            {
                // White fade out
                SetBLDUpMin(BLDCNT_TGT1_BD | BLDCNT_TGT1_OBJ
                          | BLDCNT_TGT1_BG3 | BLDCNT_TGT1_BG2
                          | BLDCNT_TGT1_BG1);
                REG_DISPCNT = DISPCNT_OBJ_ON | DISPCNT_BG3_ON
                            | DISPCNT_BG2_ON | DISPCNT_BG1_ON;
                uLocalTime = 0;
                sLocalSeq++;
            }
        }

        // Top smoke particle (matching IDA: pyramid_Anime3 at sWork6, sWork7)
        if (pyramid_Anime3(time, &smoke_pattern))
            sWork7 = ob_pos_y + (s16)((s32)16 * ob_scale_y / 256);
        dst = SetObj(smoke_pattern, dst, sWork6, sWork7);
        // Set OBJ mosaic effect (bit 10 = OBJ mosaic enable)
        ((u16 *)OamBuf)[2] |= 0x0400;

        // Wario: pyramid_Anime2 (shrinking pose)
        pyramid_Anime2(time, &wario_pattern);
        {
            u16 *warioDst = dst;
            dst = SetObj(wario_pattern, dst, ob_pos_x, ob_pos_y);
            // Set OBJ mosaic effect on Wario's first sprite
            warioDst[2] |= 0x0400;
        }
        break;

    case 7:
        // ---- S7: Fade to white, advance scene ----
        if (FadeInc(7))
            sGameSeq++;
        break;
    }

    // ---- Common OBJ rendering (rocks + grass, all states except 0) ----
    if (sLocalSeq > 0)
    {
        // Left rock + left grass
        dst = SetObj(rock_L, dst, sWork2, sWork3);
        dst = SetObj(grass_L, dst, sWork2 + 16, sWork3);

        // Right rock + right grass
        dst = SetObj(rock_R, dst, sWork4, sWork5);
        dst = SetObj(grass_R, dst, sWork4 - 16, sWork5);
    }

    // Update affine params for Wario sprite scaling
    SetObjPABCD(0, ob_rotate, ob_scale_x, ob_scale_y);
    EndObj(dst);
}

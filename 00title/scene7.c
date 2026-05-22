// Scene 7: Into the pyramid — Wario enters, cat and glitter effects
// Scene 7: Into the pyramid — Wario enters, cat and glitter effects
//
// Multiple sub-scenes:
//   S7_100-102: Pyramid interior with Wario walking
//   S7_200-206: Tunnel scene with cat
//   S7_300-303: Cave scene with cat reveal
//   S7_400:     Fade to black, advance

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

extern const u16 scene7_bg_Palette[256];
extern const u8  scene7_bg_Char[];
extern const u16 scene7_bg1_1[];
extern const u16 scene7_bg1_2[];
extern const u16 scene7_bg1_3[];
extern const u16 scene7_bg2[];
extern const u16 scene7_bg3_1[];
extern const u16 scene7_bg3_2[];
extern const u16 scene7_obj_Palette[48];
extern const u16 scene7_smoke_Palette[16];
extern const u8  scene7_obj_Char[];
extern const u8  scene7_smoke_Char1[];
extern const u8  scene7_smoke_Char2[];

// OBJ animation patterns
extern const u16 scene7_cat_000[], scene7_cat_001[], scene7_cat_002[];
extern const u16 scene7_glitter_000[];
extern const u16 scene7_smoke_02D[], scene7_smoke_054[];

extern u16 sLocalSeq;
extern u32 uLocalTime;
extern u32 uLocalTime2;
extern s16 sWork0, sWork1, sWork2, sWork3, sWork4;
extern s16 saWork0[8], saWork1[8];
extern s16 bg_scroll_x, bg_scroll_y;
extern s16 ob_pos_x, ob_pos_y;
extern u16 uEVY;
extern u16 *pObjEnd;
extern u32 uObjSize;

void Scene7_Init(void)
{
    // Copy palettes to OBJ palette rows 12 and 4
    REG_DMA3SAD = (u32)scene7_bg_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (256*2 >> 2);

    REG_DMA3SAD = (u32)scene7_obj_Palette;
    REG_DMA3DAD = (u32)((u16 *)OBJ_PLTT + 16*12);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (48*2 >> 2);

    REG_DMA3SAD = (u32)scene7_smoke_Palette;
    REG_DMA3DAD = (u32)((u16 *)OBJ_PLTT + 16*4);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (16*2 >> 2);

    // Decompress tiles
    LZ77UnCompVram((const u32 *)scene7_bg_Char, (void *)BG_VRAM);
    LZ77UnCompVram((const u32 *)scene7_obj_Char, (void *)((u8 *)OBJ_VRAM0 + 0x200*32));
    LZ77UnCompVram((const u32 *)scene7_smoke_Char1, (void *)((u8 *)OBJ_VRAM0 + 0x94*32));
    LZ77UnCompVram((const u32 *)scene7_smoke_Char2, (void *)((u8 *)OBJ_VRAM0 + 0xB4*32));

    // Clear and fill BG0+BG1 screenbases
    {
        volatile u32 v = 0x03FF03FF;
        REG_DMA3SAD = (u32)&v;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x8000);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (32*19*2 >> 2);
        REG_DMA3SAD = (u32)&v;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x9000);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (32*19*2 >> 2);
    }
    // Clear BG2 partial area with pattern tile
    {
        volatile u32 p = 0x93A093A0;
        REG_DMA3SAD = (u32)&p;
        REG_DMA3DAD = (u32)((vu16 *)((u8 *)BG_VRAM + 0xA000) + 0x1C0);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (32*6*2 >> 2);
    }

    UnPackScreen((const u16 *)scene7_bg1_1, (vu16 *)((u8 *)BG_VRAM + 0x8000));
    UnPackScreen((const u16 *)scene7_bg1_2, (vu16 *)((u8 *)BG_VRAM + 0x9000));
    UnPackScreen((const u16 *)scene7_bg1_3, (vu16 *)((u8 *)BG_VRAM + 0xA000));

    // Set initial Wario position
    ob_pos_x = -32;
    ob_pos_y = 164;
    sWork2 = ob_pos_x;  // smoke X
    sWork3 = ob_pos_y;  // smoke Y
    sWork4 = 0;         // smoke timer

    // BG registers
    REG_BG0CNT = BGCNT_TXT512x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);
    REG_BG1CNT = BGCNT_TXT512x256 | BGCNT_16COLOR | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(18) | BGCNT_CHARBASE(0);
    REG_BG2CNT = BGCNT_TXT512x256 | BGCNT_16COLOR | BGCNT_PRIORITY(2)
               | BGCNT_SCREENBASE(20) | BGCNT_CHARBASE(0);

    bg_scroll_x = 0;
    bg_scroll_y = 16;

    // White-out max, fade in during Exec
    SetBLDUpMax(BLDCNT_TGT1_BD | BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG1
              | BLDCNT_TGT1_BG2 | BLDCNT_TGT1_OBJ);

    V_Wait();

    REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_OBJ_ON
                | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_BG2_ON;
}

void Scene7_Exec(int time)
{
    u16 *dst, *cat = 0;

    // ---- State machine ----
    switch (sLocalSeq)
    {
    case 0:  // S7_100: Fade in, BG scroll, Wario walks right
        FadeDec(3);

        if (bg_scroll_x < 16 && (time & 3) == 3)
        {
            REG_BG0HOFS = ++bg_scroll_x;
            REG_BG0VOFS = --bg_scroll_y;
            REG_BG1HOFS = bg_scroll_x;
            REG_BG1VOFS = bg_scroll_y;
        }
        if ((time & 7) == 7)
            REG_BG2HOFS = bg_scroll_x >> 1;

        if (ob_pos_y > 112)
            ob_pos_y--;

        if (ob_pos_x < 120)
        {
            if (ob_pos_x < 40)
                ob_pos_x++;
            else
                ob_pos_x += 2;
        }
        else
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 1:  // S7_101: Wario "up face" pose, wait 50 frames
        if (++uLocalTime >= 50)
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 2:  // S7_102: Wario walks right off screen, fade to black
        if (uLocalTime++ == 0)
        {
            // Title_Wario_Pattern(W_NORMAL, WALK, R_MUKI)
        }
        else if (ob_pos_x < 272)
        {
            ob_pos_x += 2;
        }
        else
        {
            SetBLDDownMin(BLDCNT_TGT1_BD | BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG1
                        | BLDCNT_TGT1_BG2 | BLDCNT_TGT1_OBJ);
            sLocalSeq++;
        }
        break;

    case 3:  // S7_200: Fade to black, switch to tunnel scene
        if (FadeInc(1))
        {
            LZ77UnCompVram((const u32 *)scene7_bg2, (void *)((u8 *)BG_VRAM + 0x8000));
            bg_scroll_x = 0;
            bg_scroll_y = 0;
            REG_BG0HOFS = 0;
            REG_BG0VOFS = 0;
            REG_BG1HOFS = 0;
            REG_BG1VOFS = 0;
            ob_pos_x = -32;
            ob_pos_y = 88;
            sWork0 = 424;  // cat_x
            uLocalTime2 = 0;
            sLocalSeq++;
            REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_OBJ_ON
                        | DISPCNT_BG0_ON | DISPCNT_BG1_ON;
        }
        break;

    case 4:  // S7_201: Fade in, Wario walks right
        FadeDec(1);
        if (ob_pos_x < 56)
            ob_pos_x += 2;
        else
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 5:  // S7_202: Wario walks diagonally (tunnel slope)
        {
            int slope_y = uLocalTime * 56 / 112;
            ob_pos_y = 88 + slope_y;
            bg_scroll_x += 2;
            bg_scroll_y = slope_y;
            sWork2 -= 2;  // smoke_x
            sWork3 = ob_pos_y - 8;  // smoke_y
            REG_BG0HOFS = bg_scroll_x;
            REG_BG0VOFS = bg_scroll_y;
            REG_BG1HOFS = bg_scroll_x;
            REG_BG1VOFS = bg_scroll_y;
            uLocalTime += 2;
            if (uLocalTime > 112)
                sLocalSeq++;
        }
        break;

    case 6:  // S7_203: Wario runs horizontally, cat waiting
        bg_scroll_x += 2;
        sWork2 -= 2;
        REG_BG0HOFS = bg_scroll_x;
        REG_BG1HOFS = bg_scroll_x;
        if (bg_scroll_x >= 272)
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 7:  // S7_204: Cat appears
        if (++uLocalTime > 60)
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 8:  // S7_205: Cat runs right
        sWork0 += 2;  // cat_x
        if (++uLocalTime > 80)
        {
            sLocalSeq++;
        }
        break;

    case 9:  // S7_206: Wario follows cat
        if (ob_pos_x < 256)
            ob_pos_x += 2;
        else
            sLocalSeq++;
        break;

    case 10: // S7_300: Fade to black, switch cave scene
        if (FadeInc(1))
        {
            LZ77UnCompVram((const u32 *)scene7_bg3_1, (void *)((u8 *)BG_VRAM + 0x8000));
            LZ77UnCompVram((const u32 *)scene7_bg3_2, (void *)((u8 *)BG_VRAM + 0x9000));
            REG_BG0HOFS = 0;
            REG_BG0VOFS = 0;
            REG_BG1HOFS = 0;
            REG_BG1VOFS = 0;
            ob_pos_x = -32;
            ob_pos_y = 144;
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 11: // S7_301: Fade in, cat drops from ceiling
        FadeDec(1);
        if (ob_pos_x < 48)
            ob_pos_x += 2;
        if (++uLocalTime > 220)
            sLocalSeq++;
        break;

    case 12: // S7_302: Wario walks toward cat
        if (ob_pos_x < 96)
            ob_pos_x += 2;
        else
            sLocalSeq++;
        break;

    case 13: // S7_303: Wario falls into hole
        ob_pos_x++;
        {
            int dx = ob_pos_x - 104;
            ob_pos_y = dx * dx / 8 + 136;
        }
        if (ob_pos_y > 200)
            sLocalSeq++;
        break;

    case 14: // S7_400: Fade to black, advance
        if (FadeInc(1))
            sGameSeq++;
        break;
    }

    // ---- OBJ rendering ----
    dst = (u16 *)OamBuf;

    // Glitter particles (different positions per sub-scene)
    if (sLocalSeq >= 10)  // Cave: 8 fixed sparkles
    {
        static const u16 gx[] = {16,24,80,184,224,216,72,152};
        static const u16 gy[] = {16,72,152,136,112,24,44,96};
        int i;
        for (i = 0; i < 8; i++)
            dst = SetObj(scene7_glitter_000, dst, gx[i], gy[i]);
    }
    else if (sLocalSeq >= 3)  // Tunnel: 12 moving sparkles
    {
        static const u16 gx[] = {12,100,52,184,136,244,284,324,408,448,484,504};
        static const u16 gy[] = {28,48,100,112,188,208,140,204,128,208,132,200};
        int i;
        for (i = 0; i < 12; i++)
            dst = SetObj(scene7_glitter_000, dst, gx[i] - bg_scroll_x, gy[i] - bg_scroll_y);
    }
    else  // Initial: 6 moving sparkles
    {
        static const u16 gx[] = {36,96,164,168,228,248};
        static const u16 gy[] = {144,120,140,44,8,68};
        int i;
        for (i = 0; i < 6; i++)
            dst = SetObj(scene7_glitter_000, dst, gx[i] - bg_scroll_x, gy[i] - bg_scroll_y);
    }

    // Cat OBJ (visible during tunnel/cave scenes)
    if (sLocalSeq >= 6 && sLocalSeq < 10)
        dst = SetObj(scene7_cat_000, dst, sWork0 - bg_scroll_x, 144);
    if (sLocalSeq == 11)
        dst = SetObj(scene7_cat_000, dst, 152, 144);

    // Smoke from Wario's feet
    if (sLocalSeq < 10 && (sLocalSeq == 5 || sLocalSeq == 6))
    {
        sWork4++;
        dst = SetObj(scene7_smoke_02D, dst, sWork2, sWork3);
    }

    EndObj(dst);
}

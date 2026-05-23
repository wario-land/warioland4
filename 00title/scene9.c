// Scene 9: Escape from pyramid — Wario runs out of collapsing pyramid
//
// Reuses Scene7 BG tiles + OBJ palette from scene9.
// BG0: 512x256 16-color - pit background (scene7_bg_pit)
// BG1: 512x256 affine 16-color - unused/wall (screenbase 18)
// BG2: 512x256 affine 16-color - unused (screenbase 20)
// OBJ: Wario (scene9_wario_Anime*), cat, doctor, fragments, smoke
//
// Sequence (sLocalSeq):
//   S9_100-106: Wario runs through pyramid interior
//   S9_200-201: Cat appears behind Wario
//   S9_300-301: Doctor gives chase
//   S9_400:      Fade to black, advance

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

// ---- Scene9 data ----
// OBJ data declarations (palette + tiles extracted from ROM via IDA)
extern const u16 scene9_obj_Palette[112];
extern const u8  scene9_obj_Char[];

// Reuse scene7 BG data (pyramid interior)
extern const u8  scene7_bg_Char[];
extern const u16 scene7_bg_pit[];

// ---- Globals ----
extern u16 sLocalSeq;
extern u32 uLocalTime;
extern u32 uLocalTime2;
extern s16 sWork0, sWork1, sWork2, sWork3, sWork4, sWork5, sWork6;
extern s16 ob_pos_x, ob_pos_y;
extern s16 bg_scroll_x, bg_scroll_y;
extern u16 uEVY;
extern u16 *pObjEnd;
extern u32 uObjSize;

void Scene9_Init(void)
{
    // Copy OBJ palette (112 entries)
    REG_DMA3SAD = (u32)scene9_obj_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (112*2 >> 2);

    // Decompress BG tiles (from Scene7) and OBJ tiles
    LZ77UnCompVram((const u32 *)scene7_bg_Char, (void *)BG_VRAM);
    LZ77UnCompVram((const u32 *)scene9_obj_Char, (void *)OBJ_VRAM0);

    // Clear screenbases 18-21 (0x9000-0xBFFF) with blank tile 0x3FF
    // IDA disasm: DMA3CNT = 0x85000800 → DMA_32BIT | SRC_FIXED, count=0x800 words = 8192 bytes
    {
        volatile u32 v = 0x03FF03FF;
        REG_DMA3SAD = (u32)&v;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x9000);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | 0x800;
    }

    // UnPackScreen: pit background -> BG0
    UnPackScreen((const u16 *)scene7_bg_pit, (vu16 *)((u8 *)BG_VRAM + 0x8000));

    // BG control registers
    REG_BG0CNT = BGCNT_TXT512x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);
    REG_BG1CNT = BGCNT_TXT512x256 | BGCNT_16COLOR | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(18) | BGCNT_CHARBASE(0);
    REG_BG2CNT = BGCNT_TXT512x256 | BGCNT_16COLOR | BGCNT_PRIORITY(2)
               | BGCNT_SCREENBASE(20) | BGCNT_CHARBASE(0);

    // Initial scroll
    bg_scroll_x = 0;
    bg_scroll_y = 0;

    // Wario starts at center, climbing up from bottom
    ob_pos_x = 120;
    ob_pos_y = 220;
    sWork0 = 120;  // cat X
    sWork1 = 204;  // cat Y

    // Darken to max at start (fade in from black)
    SetBLDDownMax(BLDCNT_TGT1_BD | BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG1
                | BLDCNT_TGT1_BG2 | BLDCNT_TGT1_OBJ);

    V_Wait();

    // Enable display: Mode 0, OBJ + all BGs
    REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_OBJ_ON
                | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_BG2_ON;
}

void Scene9_Exec(int time)
{
    u16 *dst;
    int shake_y;

    // Camera shake lookup (simulated)
    shake_y = (time & 4) ? 1 : 0;

    switch (sLocalSeq)
    {
    // ---- S9_100: Fade in, BG scroll ----
    case 0:
        FadeDec(3);

        // Wario climbs upward (moves from bottom toward center)
        if (ob_pos_y > 160 && (time & 1))
            ob_pos_y--;

        // BG scrolls down slowly
        if (bg_scroll_y < 32 && (time & 3) == 3)
        {
            bg_scroll_y++;
            REG_BG0VOFS = bg_scroll_y + shake_y;
            REG_BG1VOFS = bg_scroll_y;
        }

        if (time > 90)
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    // ---- S9_101: Wario runs right ----
    case 1:
        FadeDec(7);

        if (ob_pos_x < 160)
            ob_pos_x++;
        else
            ob_pos_y -= 2;  // Wario climbs upward

        REG_BG0VOFS = bg_scroll_y + shake_y;

        if (++uLocalTime > 60)
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    // ---- S9_102: Wario continues running ----
    case 2:
        if (ob_pos_x < 200)
            ob_pos_x++;
        if (ob_pos_y > 120)
            ob_pos_y--;

        if (bg_scroll_x < 64 && (time & 3) == 3)
        {
            bg_scroll_x++;
            REG_BG0HOFS = bg_scroll_x;
            REG_BG1HOFS = bg_scroll_x;
        }

        REG_BG0VOFS = bg_scroll_y + shake_y;

        if (++uLocalTime > 80)
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    // ---- S9_103: Cat appears ----
    case 3:
        sWork0 = ob_pos_x - 48;  // cat behind Wario
        sWork1 = ob_pos_y - 8;

        if (ob_pos_x < 240)
            ob_pos_x++;
        if (bg_scroll_x < 128 && (time & 3) == 3)
        {
            bg_scroll_x += 2;
            REG_BG0HOFS = bg_scroll_x;
            REG_BG1HOFS = bg_scroll_x;
        }

        REG_BG0VOFS = bg_scroll_y + shake_y;

        if (++uLocalTime > 60)
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    // ---- S9_104-106: Cat follows, Wario leads ----
    case 4:
    case 5:
    case 6:
        sWork0 += 2;  // Cat runs right
        if (ob_pos_x < 280)
            ob_pos_x++;

        if (bg_scroll_x < 200 && (time & 3) == 3)
        {
            bg_scroll_x += 2;
            REG_BG0HOFS = bg_scroll_x;
            REG_BG1HOFS = bg_scroll_x;
        }

        REG_BG0VOFS = bg_scroll_y + shake_y;

        if (++uLocalTime > 180)
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    // ---- S9_400: Fade to black, advance ----
    default:
        FadeInc(3);
        if (uEVY == 16)
            sGameSeq++;
        break;
    }

    // ---- OBJ rendering ----
    dst = (u16 *)OamBuf;

    // Wario OBJ (visible in all states)
    if (ob_pos_y < 200)
    {
        // Simplified single-sprite Wario OBJ
        static const u16 wario_pattern[] = { 1, 0x40C8, 0x8078, 0x0200 };
        dst = SetObj(wario_pattern, dst, ob_pos_x - bg_scroll_x, ob_pos_y);
    }

    // Cat OBJ (visible from state 3)
    if (sLocalSeq >= 3 && sLocalSeq <= 6)
    {
        static const u16 cat_pattern[] = { 1, 0x40C8, 0x8080, 0x0220 };
        dst = SetObj(cat_pattern, dst, sWork0 - bg_scroll_x, sWork1);
    }

    EndObj(dst);
}

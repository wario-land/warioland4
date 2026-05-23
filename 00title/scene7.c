// Scene 7: Into the pyramid — Wario enters, cat and glitter effects
//
// Multiple sub-scenes showing Wario entering the pyramid interior,
// encountering the black cat in a tunnel, and descending into a cave.
//
// Sub-scene flow:
//   S7_100-102: Pyramid interior — Wario walks right through torch-lit halls
//     S7_100: Fade in, BG scrolls, Wario walks from left toward center
//     S7_101: Wario looks up (surprised), waits 50 frames
//     S7_102: Wario continues walking right, exits screen, fade to black
//   S7_200-206: Tunnel — Cat appears and runs away from Wario
//     S7_200: Fade to black, switch to tunnel BG
//     S7_201: Fade in, Wario walks right into tunnel
//     S7_202: Wario walks diagonally down tunnel slope
//     S7_203: Wario runs horizontally through tunnel
//     S7_204: Cat suddenly appears from darkness
//     S7_205: Cat runs right through tunnel
//     S7_206: Wario follows the cat
//   S7_300-303: Cave — Treasure room reveal
//     S7_300: Fade to black, switch to cave BG
//     S7_301: Fade in, Wario enters cave with cat
//     S7_302: Wario walks toward cat near treasure
//     S7_303: Wario steps onto trap, falls into hole (parabolic path)
//   S7_400: Fade to black, advance to scene 8 (treasure room)

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

// ---- Scene7 data ----
extern const u16 scene7_bg_Palette[256];
extern const u8  scene7_bg_Char[];
extern const u16 scene7_bg1_1[];     // BG0 pyramid hall tilemap (main area)
extern const u16 scene7_bg1_2[];     // BG1 wall tilemap (right side)
extern const u16 scene7_bg1_3[];     // BG2 wall tilemap (further right)
extern const u16 scene7_bg2[];       // LZ77 tunnel tilemap
extern const u16 scene7_bg3_1[];     // LZ77 cave tilemap part 1
extern const u16 scene7_bg3_2[];     // LZ77 cave tilemap part 2
extern const u16 scene7_obj_Palette[48];     // OBJ palette rows 12-14
extern const u16 scene7_smoke_Palette[16];   // Smoke palette row 4
extern const u8  scene7_obj_Char[];          // OBJ tiles at offset 0x200
extern const u8  scene7_smoke_Char1[];       // Smoke tiles at offset 0x94
extern const u8  scene7_smoke_Char2[];       // Smoke tiles at offset 0xB4

// ---- Global state ----
extern u16 sLocalSeq;
extern u32 uLocalTime;
extern u32 uLocalTime2;
extern s16 sWork0, sWork1, sWork2, sWork3, sWork4;
extern s16 bg_scroll_x, bg_scroll_y;
extern s16 ob_pos_x, ob_pos_y;
extern u16 uEVY;
extern u16 *pObjEnd;
extern u32 uObjSize;

void Scene7_Init(void)
{
    // ---- Load BG palette (256 entries) ----
    REG_DMA3SAD = (u32)scene7_bg_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (256*2 >> 2);

    // ---- Load OBJ palette row 12 + smoke palette row 4 ----
    // Wario and cat OBJs use palette rows 12-14
    REG_DMA3SAD = (u32)scene7_obj_Palette;
    REG_DMA3DAD = (u32)((u16 *)OBJ_PLTT + 16*12);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (48*2 >> 2);

    // Smoke effects use palette row 4
    REG_DMA3SAD = (u32)scene7_smoke_Palette;
    REG_DMA3DAD = (u32)((u16 *)OBJ_PLTT + 16*4);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (16*2 >> 2);

    // ---- Decompress tiles ----
    // BG tiles: pyramid hall architecture tiles
    LZ77UnCompVram((const u32 *)scene7_bg_Char, (void *)BG_VRAM);
    // OBJ tiles: Wario + cat sprites at offset 0x200*32
    LZ77UnCompVram((const u32 *)scene7_obj_Char, (void *)((u8 *)OBJ_VRAM0 + 0x200*32));
    // Smoke tiles: at offsets 0x94*32 and 0xB4*32
    LZ77UnCompVram((const u32 *)scene7_smoke_Char1, (void *)((u8 *)OBJ_VRAM0 + 0x94*32));
    LZ77UnCompVram((const u32 *)scene7_smoke_Char2, (void *)((u8 *)OBJ_VRAM0 + 0xB4*32));

    // ---- Clear BG screenbases and fill with pattern tile ----
    // BG0 (hall): clear top 19 rows with blank tile
    {
        volatile u32 v = 0x03FF03FF;
        REG_DMA3SAD = (u32)&v;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x8000);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (32*19*2 >> 2);
    }
    // BG1 (wall right): same clear pattern
    {
        volatile u32 v = 0x03FF03FF;
        REG_DMA3SAD = (u32)&v;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x9000);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (32*19*2 >> 2);
    }
    // BG2 (wall further right): fill partial area with pattern tile 0x93A0
    {
        volatile u32 p = 0x93A093A0;
        REG_DMA3SAD = (u32)&p;
        REG_DMA3DAD = (u32)((vu16 *)((u8 *)BG_VRAM + 0xA000) + 0x1C0);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (32*6*2 >> 2);
    }

    // ---- Unpack tilemaps for pyramid hall (initial sub-scene) ----
    UnPackScreen((const u16 *)scene7_bg1_1, (vu16 *)((u8 *)BG_VRAM + 0x8000));
    UnPackScreen((const u16 *)scene7_bg1_2, (vu16 *)((u8 *)BG_VRAM + 0x9000));
    UnPackScreen((const u16 *)scene7_bg1_3, (vu16 *)((u8 *)BG_VRAM + 0xA000));

    // ---- Initial Wario position ----
    // Wario starts offscreen to the left, walks in from x=-32
    ob_pos_x = -32;
    ob_pos_y = 164;
    sWork2 = ob_pos_x;   // smoke X follows Wario
    sWork3 = ob_pos_y;   // smoke Y
    sWork4 = 0;          // smoke timer

    // ---- BG registers ----
    // BG0: main hall, 512x256, priority 0 (frontmost BG layer)
    // BG1: wall right side, 512x256, priority 1
    // BG2: wall further right, 512x256, priority 2
    REG_BG0CNT = BGCNT_TXT512x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);
    REG_BG1CNT = BGCNT_TXT512x256 | BGCNT_16COLOR | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(18) | BGCNT_CHARBASE(0);
    REG_BG2CNT = BGCNT_TXT512x256 | BGCNT_16COLOR | BGCNT_PRIORITY(2)
               | BGCNT_SCREENBASE(20) | BGCNT_CHARBASE(0);

    // Initial scroll: start slightly offset for entrance effect
    bg_scroll_x = 0;
    bg_scroll_y = 16;

    // Start fully white, fade in during Exec
    SetBLDUpMax(BLDCNT_TGT1_BD | BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG1
              | BLDCNT_TGT1_BG2 | BLDCNT_TGT1_OBJ);

    V_Wait();
    REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_OBJ_ON
                | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_BG2_ON;
}

void Scene7_Exec(int time)
{
    u16 *dst;
    const u16 *cat_pattern = 0;
    const u16 *smoke_pattern = 0;

    // ---- State machine: dispatches to one of 15 sub-states ----
    switch (sLocalSeq)
    {
    // ========== S7_100: Fade in, Wario enters hall ==========
    case 0:
        // Decrease white blend (screen fades from white to visible)
        FadeDec(3);

        // BG scrolls right (camera pans into the hall)
        if (bg_scroll_x < 16 && (time & 3) == 3)
        {
            REG_BG0HOFS = ++bg_scroll_x;
            REG_BG0VOFS = --bg_scroll_y;
            REG_BG1HOFS = bg_scroll_x;
            REG_BG1VOFS = bg_scroll_y;
        }
        // BG2 scrolls at half speed (parallax depth)
        if ((time & 7) == 7)
            REG_BG2HOFS = bg_scroll_x >> 1;

        // Wario walks up from bottom
        if (ob_pos_y > 112)
            ob_pos_y--;

        // Wario enters from left, speeds up as he passes x=40
        if (ob_pos_x < 120)
        {
            if (ob_pos_x < 40)
                ob_pos_x++;
            else
                ob_pos_x += 2;   // Wario picks up speed
        }
        else
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    // ========== S7_101: Wario looks up (surprised), waits 50 frames ==========
    case 1:
        if (++uLocalTime >= 50)
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    // ========== S7_102: Wario walks off screen right, fade to black ==========
    case 2:
        if (uLocalTime++ == 0)
        {
            // Title_Wario_Pattern(0, 51, 16) — multi-part Wario OBJ, simplified below
        }
        else if (ob_pos_x < 272)
        {
            ob_pos_x += 2;   // Wario walks right, off screen
        }
        else
        {
            // Wario has exited: set up darken blend for transition
            SetBLDDownMin(BLDCNT_TGT1_BD | BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG1
                        | BLDCNT_TGT1_BG2 | BLDCNT_TGT1_OBJ);
            sLocalSeq++;
        }
        break;

    // ========== S7_200: Fade to black, switch to tunnel BG ==========
    case 3:
        if (FadeInc(1))
        {
            // Load tunnel tilemap into BG0
            LZ77UnCompVram((const u32 *)scene7_bg2, (void *)((u8 *)BG_VRAM + 0x8000));
            // Reset scroll for new scene
            bg_scroll_x = 0;
            bg_scroll_y = 0;
            REG_BG0HOFS = 0;
            REG_BG0VOFS = 0;
            REG_BG1HOFS = 0;
            REG_BG1VOFS = 0;
            // Reset positions
            ob_pos_x = -32;
            ob_pos_y = 88;
            sWork0 = 424;    // cat X (far right, offscreen)
            uLocalTime2 = 0;
            sLocalSeq++;
            // Enable display for tunnel (BG0 + BG1 only, OBJ)
            REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_OBJ_ON
                        | DISPCNT_BG0_ON | DISPCNT_BG1_ON;
        }
        break;

    // ========== S7_201: Fade in, Wario walks right ==========
    case 4:
        FadeDec(1);
        if (ob_pos_x < 56)
            ob_pos_x += 2;
        else
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    // ========== S7_202: Wario walks diagonally down slope ==========
    case 5:
        {
            // Wario Y increases as he descends tunnel slope
            int slope_y = uLocalTime * 56 / 112;
            ob_pos_y = 88 + slope_y;

            // BG scrolls right/down following the slope
            bg_scroll_x += 2;
            bg_scroll_y = slope_y;

            // Smoke follows behind Wario
            sWork2 -= 2;
            sWork3 = ob_pos_y - 8;

            REG_BG0HOFS = bg_scroll_x;
            REG_BG0VOFS = bg_scroll_y;
            REG_BG1HOFS = bg_scroll_x;
            REG_BG1VOFS = bg_scroll_y;

            uLocalTime += 2;
            if (uLocalTime > 112)
                sLocalSeq++;
        }
        break;

    // ========== S7_203: Wario runs horizontally ==========
    case 6:
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

    // ========== S7_204: Cat appears from darkness ==========
    case 7:
        if (++uLocalTime > 60)
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    // ========== S7_205: Cat runs right through tunnel ==========
    case 8:
        sWork0 += 2;    // cat X increases (runs right)
        scene7_cat_Anime0(time, &cat_pattern);
        if (++uLocalTime > 80)
        {
            sLocalSeq++;
        }
        break;

    // ========== S7_206: Wario follows cat ==========
    case 9:
        if (ob_pos_x < 256)
            ob_pos_x += 2;
        else
            sLocalSeq++;
        break;

    // ========== S7_300: Fade to black, switch to cave BG ==========
    case 10:
        if (FadeInc(1))
        {
            // Load cave tilemaps (two parts for wide cave scene)
            LZ77UnCompVram((const u32 *)scene7_bg3_1, (void *)((u8 *)BG_VRAM + 0x8000));
            LZ77UnCompVram((const u32 *)scene7_bg3_2, (void *)((u8 *)BG_VRAM + 0x9000));
            // Reset all scroll/positions for cave
            REG_BG0HOFS = 0; REG_BG0VOFS = 0;
            REG_BG1HOFS = 0; REG_BG1VOFS = 0;
            ob_pos_x = -32;
            ob_pos_y = 144;
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    // ========== S7_301: Fade in, Wario enters cave ==========
    case 11:
        FadeDec(1);
        // Cat drops from ceiling or walks from darkness
        scene7_cat_Anime0(time, &cat_pattern);
        if (ob_pos_x < 48)
            ob_pos_x += 2;
        if (++uLocalTime > 220)
            sLocalSeq++;
        break;

    // ========== S7_302: Wario walks toward cat ==========
    case 12:
        if (ob_pos_x < 96)
            ob_pos_x += 2;
        else
            sLocalSeq++;
        break;

    // ========== S7_303: Wario falls into trap hole (parabolic arc) ==========
    case 13:
        ob_pos_x++;
        {
            // Parabolic fall: y = (x - 104)^2 / 8 + 136
            int dx = ob_pos_x - 104;
            ob_pos_y = dx * dx / 8 + 136;
        }
        if (ob_pos_y > 200)    // Wario has fallen off screen
            sLocalSeq++;
        break;

    // ========== S7_400: Fade to black, advance ==========
    case 14:
        if (FadeInc(1))
            sGameSeq++;    // Move to next title scene
        break;
    }

    // ---- OBJ rendering ----
    dst = (u16 *)OamBuf;

    // === Glitter particles (sparkle effects) ===
    // Different positions per area: cave (8 fixed), tunnel (12 moving), hall (6 moving)
    if (sLocalSeq >= 10)
    {
        // Cave: 8 fixed-position sparkles around the treasure chamber
        static const u16 gx[] = {16, 24, 80, 184, 224, 216, 72, 152};
        static const u16 gy[] = {16, 72, 152, 136, 112, 24, 44, 96};
        int i;
        for (i = 0; i < 8; i++)
        {
            static const u16 glitter[] = { 1, 0x40A0, 0x8080, 0x04C0 };
            dst = SetObj(glitter, dst, gx[i], gy[i]);
        }
    }
    else if (sLocalSeq >= 3)
    {
        // Tunnel: 12 sparkles that move with BG scroll (parallax depth effect)
        static const u16 gx[] = {12, 100, 52, 184, 136, 244, 284, 324, 408, 448, 484, 504};
        static const u16 gy[] = {28, 48, 100, 112, 188, 208, 140, 204, 128, 208, 132, 200};
        int i;
        for (i = 0; i < 12; i++)
        {
            static const u16 glitter[] = { 1, 0x40A0, 0x8080, 0x04C0 };
            dst = SetObj(glitter, dst, gx[i] - bg_scroll_x, gy[i] - bg_scroll_y);
        }
    }
    else
    {
        // Hall: 6 sparkles near torch sconces
        static const u16 gx[] = {36, 96, 164, 168, 228, 248};
        static const u16 gy[] = {144, 120, 140, 44, 8, 68};
        int i;
        for (i = 0; i < 6; i++)
        {
            static const u16 glitter[] = { 1, 0x40A0, 0x8080, 0x04C0 };
            dst = SetObj(glitter, dst, gx[i] - bg_scroll_x, gy[i] - bg_scroll_y);
        }
    }

    // === Cat OBJ ===
    // Visible during tunnel/cave scenes, positioned relative to BG scroll
    if (sLocalSeq >= 6 && sLocalSeq < 10)
        dst = SetObj(cat_pattern, dst, sWork0 - bg_scroll_x, 144);
    if (sLocalSeq == 11)
        dst = SetObj(cat_pattern, dst, 152, 144);

    // === Smoke from Wario's feet ===
    // Visible during running states (S7_202, S7_203) in tunnel
    if (sLocalSeq < 10 && (sLocalSeq == 5 || sLocalSeq == 6))
    {
        sWork4++;
        scene7_smoke_Anime22(sWork4, &smoke_pattern);
        if (smoke_pattern)
            dst = SetObj(smoke_pattern, dst, sWork2, sWork3);
    }

    // === Wario OBJ ===
    // Simplified single-sprite Wario (full multi-part body system not yet ported)
    {
        static const u16 wario[] = { 1, 0x4080, 0x8088, 0x0200 };
        dst = SetObj(wario, dst, ob_pos_x - bg_scroll_x, ob_pos_y);
    }

    EndObj(dst);
}

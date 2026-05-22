// Scene 8: Pyramid treasure room — Wario discovers the treasure
// Scene 8: Pyramid treasure room — Wario discovers the treasure
//
// BG0: 512x256 treasure room background
// OBJ: Wario with treasure, scaling down

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

extern const u16 scene8_bg_Palette[16];
extern const u8  scene8_bg_Char[];
extern const u16 scene8_bg[];
extern const u16 scene8_obj_Palette[16];
extern const u8  scene8_obj_Char[];

extern u16 sLocalSeq;
extern u32 uLocalTime;
extern s16 ob_pos_x, ob_pos_y;
extern u16 ob_rotate;
extern u16 ob_scale_x, ob_scale_y;
extern u16 uEVY;
extern u16 *pObjEnd;
extern u32 uObjSize;

void Scene8_Init(void)
{
    // Copy palettes
    REG_DMA3SAD = (u32)scene8_obj_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (16*2 >> 2);

    REG_DMA3SAD = (u32)scene8_bg_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (16*2 >> 2);

    // Decompress tiles
    LZ77UnCompVram((const u32 *)scene8_bg_Char, (void *)BG_VRAM);
    LZ77UnCompVram((const u32 *)scene8_obj_Char, (void *)OBJ_VRAM0);

    // BG tilemap
    UnPackScreen((const u16 *)scene8_bg, (vu16 *)((u8 *)BG_VRAM + 0x8000));

    // BG0: 512x256, 16-color, priority 0
    REG_BG0CNT = BGCNT_TXT512x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);

    // Wario OBJ: center screen, 3x scale
    ob_pos_x = 120;
    ob_pos_y = 80;
    ob_scale_x = 0x300;
    ob_scale_y = 0x300;
    ob_rotate = 0;

    uEVY = 16;
    REG_BLDY = 16;
    REG_BLDCNT = BLDCNT_TGT1_BG0 | BLDCNT_TGT1_OBJ | BLDCNT_TGT1_BD
               | BLDCNT_EFFECT_DARKEN;

    V_Wait();

    REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_BG0_ON;
}

void Scene8_Exec(int time)
{
    u16 *dst;

    switch (sLocalSeq)
    {
    case 0:  // Fade in from black
        if ((time & 1) == 1 && uEVY > 0)
        {
            uEVY--;
            REG_BLDY = uEVY;
        }
        if (uEVY == 0)
        {
            REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_OBJ_ON | DISPCNT_BG0_ON;
            sLocalSeq++;
        }
        break;

    case 1:  // Wario scales down
        ob_scale_x -= 0x10;
        ob_scale_y -= 0x10;
        if (ob_scale_x < 0x10)
            sLocalSeq++;
        break;

    case 2:  // Fade to black, advance
        if ((time & 1) == 1 && uEVY < 16)
        {
            uEVY++;
            REG_BLDY = uEVY;
        }
        if (uEVY == 16)
            sGameSeq++;
        break;
    }

    // OBJ rendering
    dst = (u16 *)OamBuf;

    if (sLocalSeq < 2)
    {
        // Use a simple dummy OBJ as placeholder for Wario+treasure
        // scene8_Anime0 would generate the OBJ pattern
        static const u16 wario_placeholder[] = { 1, 0x4000, 0x8000, 0x0000 };
        dst = SetObj(wario_placeholder, dst, ob_pos_x, ob_pos_y);
    }

    EndObj(dst);
}

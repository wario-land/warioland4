// Scene 8: Pyramid treasure room — Wario discovers the treasure
//
// After falling through the trap in the cave, Wario lands in a treasure
// chamber deep inside the pyramid. He picks up a treasure chest and
// strikes a victory pose before the scene fades out.
//
// This is a short transitional scene (about 3 seconds).
//
// BG0: 512x256 16-color — treasure room background (walls + treasure shelves)
// OBJ: Wario holding treasure chest above his head (affine scaled)
//
// The scene uses affine scaling on the Wario OBJ:
//   - Starts at 3x scale (0x300), creating a dramatic entrance
//   - Shrinks to 1x scale (0x100) over the animation
//   - Wario rotates slightly as he lands
//
// States (sLocalSeq):
//   S8_0: Fade in from black, enable OBJ display
//   S8_1: Wario scales down from 3x to near 1x, then transitions
//   S8_2: Fade to black, advance to next scene

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

// ---- Scene8 data ----
extern const u16 scene8_bg_Palette[16];   // BG palette (1 row)
extern const u8  scene8_bg_Char[];         // LZ77 BG tiles
extern const u16 scene8_bg[];             // BG tilemap
extern const u16 scene8_obj_Palette[16];  // OBJ palette (1 row)
extern const u8  scene8_obj_Char[];        // LZ77 OBJ tiles

// ---- Global state ----
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
    // ---- Load OBJ palette (16 entries) ----
    // Wario's treasure room palette: gold, bronze, gem colors
    REG_DMA3SAD = (u32)scene8_obj_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (16*2 >> 2);

    // ---- Load BG palette (16 entries) ----
    // Treasure room uses a single monochrome-ish palette row
    REG_DMA3SAD = (u32)scene8_bg_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (16*2 >> 2);

    // ---- Decompress tiles ----
    LZ77UnCompVram((const u32 *)scene8_bg_Char, (void *)BG_VRAM);
    LZ77UnCompVram((const u32 *)scene8_obj_Char, (void *)OBJ_VRAM0);

    // ---- Unpack BG tilemap to screenbase 16 ----
    UnPackScreen((const u16 *)scene8_bg, (vu16 *)((u8 *)BG_VRAM + 0x8000));

    // ---- BG0: 512x256 16-color, priority 0 ----
    // Treasure room is wider than screen, allows slight horizontal scroll
    REG_BG0CNT = BGCNT_TXT512x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);

    // ---- Initial Wario OBJ position and scale ----
    // Wario starts at screen center, 3x scale for dramatic entrance
    ob_pos_x = 120;
    ob_pos_y = 80;
    ob_scale_x = 0x300;   // 3.0x scale
    ob_scale_y = 0x300;
    ob_rotate = 0;

    // Start fully dark (fade in from black)
    uEVY = 16;
    REG_BLDY = 16;
    REG_BLDCNT = BLDCNT_TGT1_BG0 | BLDCNT_TGT1_OBJ | BLDCNT_TGT1_BD
               | BLDCNT_EFFECT_DARKEN;

    V_Wait();

    // Initially BG0 only (OBJ added after fade-in)
    REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_BG0_ON;
}

void Scene8_Exec(int time)
{
    u16 *dst;
    const u16 *wario_pattern = 0;

    switch (sLocalSeq)
    {
    case 0:
        // ---- S8_0: Fade in from black ----
        // Decrease darken brightness every other frame.
        // When fully visible (uEVY==0), enable OBJ and advance state.
        if ((time & 1) == 1 && uEVY > 0)
        {
            uEVY--;
            REG_BLDY = uEVY;
        }
        if (uEVY == 0)
        {
            // Enable OBJ layer now that room is visible
            REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_OBJ_ON | DISPCNT_BG0_ON;
            sLocalSeq++;
        }
        break;

    case 1:
        // ---- S8_1: Wario scales down from 3x to near 1x ----
        // The scale decreases by 16 (0x10) each frame.
        // 3.0x → 1.0x = 0x300 - 0x100 = 0x200 = 512 / 16 = 32 frames total
        ob_scale_x -= 0x10;
        ob_scale_y -= 0x10;

        if (ob_scale_x < 0x10)
        {
            // Scale reached minimum: move to fade-out state
            sLocalSeq++;
        }
        break;

    case 2:
        // ---- S8_2: Fade to black, advance ----
        // Increase darken brightness every other frame.
        // When fully dark, advance to next title scene.
        if ((time & 1) == 1 && uEVY < 16)
        {
            uEVY++;
            REG_BLDY = uEVY;
        }
        if (uEVY == 16)
            sGameSeq++;
        break;
    }

    // ---- OBJ rendering ----
    dst = (u16 *)OamBuf;

    // Render Wario OBJ while scene is active (states 0-1)
    if (sLocalSeq <= 1)
    {
        // Get Wario+treasure OBJ pattern from scene8_Anime0
        scene8_Anime0(&wario_pattern);
        if (wario_pattern)
            dst = SetObj(wario_pattern, dst, ob_pos_x, ob_pos_y);
    }

    // Fill remaining OAM entries with disabled sprites and set DMA size
    EndObj(dst);
}

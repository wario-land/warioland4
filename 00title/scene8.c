// Scene 8: Pyramid treasure room -- Wario discovers the treasure
//
// After falling through the trap in the cave, Wario lands in a treasure
// chamber deep inside the pyramid. He picks up a treasure chest and
// strikes a victory pose before the scene fades out.
//
// This is a short transitional scene (about 3 seconds).
//
// BG0: 512x256 16-color -- treasure room background (walls + treasure shelves)
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
#include "../wario/wario.h"

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
    // ---- Load Wario OBJ palette (matches IDA Title_Wario_Palette) ----
    Title_Wario_Palette();

    // ---- Load scene8-specific OBJ palette at row 12 (16 entries = 32 bytes) ----
    REG_DMA3SAD = (u32)scene8_obj_Palette;
    REG_DMA3DAD = (u32)((u16 *)OBJ_PLTT + 16*12);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (16*2 >> 2);

    // ---- Load BG palette (16 entries) ----
    REG_DMA3SAD = (u32)scene8_bg_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (16*2 >> 2);

    // ---- Decompress tiles ----
    LZ77UnCompVram((const u32 *)scene8_bg_Char, (void *)BG_VRAM);
    LZ77UnCompVram((const u32 *)scene8_obj_Char, (void *)OBJ_VRAM0);

    // ---- Unpack BG tilemap to screenbase 16 ----
    UnPackScreen((const u16 *)scene8_bg, (vu16 *)((u8 *)BG_VRAM + 0x8000));

    // ---- BG0: 512x256 16-color, priority 0 ----
    REG_BG0CNT = BGCNT_TXT512x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);

    // ---- Set up Wario OBJ state and initial position ----
    // IDA calls Title_Wario_Pattern(0, 0, 16) — kind=0 (idle), stat=0, muki=16 (right)
    Title_Wario_Pattern(0, 0, 16);
    ob_pos_x = 120;
    ob_pos_y = 80;
    ob_scale_x = 0x300;   // 3.0x scale for dramatic entrance
    ob_scale_y = 0x300;
    ob_rotate = 0;

    // Apply affine matrix for Wario OBJ scaling
    SetObjPABCD(0, ob_rotate, ob_scale_x, ob_scale_y);

    // Start fully dark (fade in from black), matching IDA SetBLDDownMax(0x31)
    SetBLDDownMax(BLDCNT_TGT1_BG0 | BLDCNT_TGT1_OBJ | BLDCNT_TGT1_BD);

    V_Wait();

    // Initially BG0 only (OBJ added after fade-in, matching IDA: 0x0100)
    REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_BG0_ON;
}

void Scene8_Exec(int time)
{
    u16 *dst;

    switch (sLocalSeq)
    {
    case 0:
        // ---- S8_0: Fade in from black ----
        if (FadeDec(1))
        {
            // Enable OBJ layer (matching IDA: DISPCNT = 0x1100)
            REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_OBJ_ON | DISPCNT_BG0_ON;
            // WarioVoiceSet(0);  // TODO: sound (IDA 0x8085bdc)
            sLocalSeq++;
        }
        break;

    case 1:
        // ---- S8_1: Wario scales down from 3x toward 1x ----
        ob_scale_x -= 0x10;
        ob_scale_y -= 0x10;
        if (ob_scale_x <= 0x0F)
            sLocalSeq++;
        break;

    case 2:
        // ---- S8_2: Fade to black, advance ----
        if (FadeInc(1))
            sGameSeq++;
        break;
    }

    // ---- OBJ rendering ----
    dst = (u16 *)OamBuf;

    // Render Wario OBJ with affine scaling during states 0-1
    // Matching IDA: Title_Wario_Move renders multi-part Wario OBJ
    if (sLocalSeq <= 1)
    {
        dst = Title_Wario_Move(ob_pos_x, ob_pos_y, 0, 0, 0);
        SetObjPABCD(0, ob_rotate, ob_scale_x, ob_scale_y);
    }

    EndObj(dst);
}

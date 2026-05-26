// Scene 1: River scene
// Scene 1: River scene
//
// A simple timed scene that reveals BG layers one by one:
//   BG0: river background  (screenbase 16)
//   BG1: midground        (screenbase 17)
//   BG2: foreground       (screenbase 18)
//
// Timing (uLocalTime frames since Scene1_Init):
//   30: BG0 appears
//   32: BG1 appears
//   34: BG2 appears
//  234: all BGs off
//  236: advance to next scene

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

// ---- Scene1 data (defined in scene1_data.c) ----
extern const u16 scene1_Palette[48];
extern const u8  scene1_Char[];
extern const u16 scene1_bg0[];
extern const u16 scene1_bg1[];
extern const u16 scene1_bg2[];

// ---- Globals ----
extern u32 uLocalTime;

// ---- Scene1_Init ----
// Load palette, decompress tiles, clear + fill tilemaps.
// All BGs off at init (DISPCNT=0), displayed one-by-one in Exec.
// Original at 0x80043B4.
void Scene1_Init(void)
{
    // Load BG palette
    REG_DMA3SAD = (u32)scene1_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (48*2 >> 2);

    // Decompress river scene tiles to VRAM base
    LZ77UnCompVram((const u32 *)scene1_Char, (void *)BG_VRAM);

    // Clear BG0+BG1+BG2 screenblocks (6144 bytes = screenbases 16-18, 0x8000-0x97FF)
    // Matches original DMA: 0->0x6008000, control 0x81000C00 (16-bit, 0xC00 words)
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x8000);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x1800 >> 2);
    }

    // Fill tilemaps for three BG layers
    UnPackScreen((const u16 *)scene1_bg0, (vu16 *)((u8 *)BG_VRAM + 0x8000));
    UnPackScreen((const u16 *)scene1_bg1, (vu16 *)((u8 *)BG_VRAM + 0x8800));
    UnPackScreen((const u16 *)scene1_bg2, (vu16 *)((u8 *)BG_VRAM + 0x9000));

    // BG0: 256x256, 16-color, screenbase 16, charbase 0
    REG_BG0CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);
    // BG1: 256x256, 16-color, screenbase 17, charbase 0
    REG_BG1CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(17) | BGCNT_CHARBASE(0);
    // BG2: 256x256, 16-color, screenbase 18, charbase 0
    REG_BG2CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(18) | BGCNT_CHARBASE(0);

    // No blending, display off
    REG_BLDCNT = 0;
    REG_DISPCNT = DISPCNT_MODE_0;
}

// ---- Scene1_Exec ----
// Simple timer-based scene: turns on BG layers at specific times.
// Original at 0x8004460.
void Scene1_Exec(int time)
{
    // Use uLocalTime as frame counter (auto-incremented)
    // Original game shows ONE BG at a time, not cumulative
    switch (uLocalTime)
    {
    case 30:   // Frame 30: show BG0 only
        REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_BG0_ON;
        break;
    case 32:   // Frame 32: show BG1 only
        REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_BG1_ON;
        break;
    case 34:   // Frame 34: show BG2 only
        REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_BG2_ON;
        break;
    case 234:  // Frame 234: hide all
        REG_DISPCNT = DISPCNT_MODE_0;
        break;
    case 236:  // Frame 236: advance to next scene
        sGameSeq++;
        return;
    }
    uLocalTime++;
}

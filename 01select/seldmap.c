// World map scene (dmap) — Wario walks on the world map between levels
//
// BG layers:
//   BG0: blend overlay (screenbase 24) — windowed swirl/transition effect
//   BG1: base map near layer (screenbase 26) — foreground terrain
//   BG2: base map far layer (screenbase 28) — background terrain
//   BG3: message text (screenbase 30) — "Emerald Passage" etc.
//
// OBJ: Wario cursor, pyramid, shisa enemies, black cat, lid, smoke,
//      light color effects, pyramid panels, sparkle effects

#include "select.h"
#include "../gba/gba.h"
#include "../gameutils.h"
#include "../00title/title.h"

// ---- External data references ----
extern const u16 map_select_bg_Palette[256];
extern const u8  map_select_bg_Char[];
extern const u16 map_select_obj_Palette[256];
extern const u8  map_select_obj_Char[];
extern const u16 map_select_1_Screen[0x400];
extern const u16 map_select_2_Screen[0x400];
extern const u16 map_select_3_Screen[0x400];
extern const u16 map_select_message_Screen[0x400];

// ---- Dmap IWRAM variables ----
IWRAM_DATA static s16  sDmapBgPosY;         // BG Y position (16x fixed point)
IWRAM_DATA static u8   ucRoadKind;          // Which road is selected
IWRAM_DATA static u8   ucDmapMoveCount;     // Movement counter
IWRAM_DATA static s16  sDmapBgScrollSpeedY; // BG scroll speed

typedef struct {
    u16 animTimer;
    u16 animPat;
} DmapObjDef;

IWRAM_DATA static DmapObjDef DmapLight;
IWRAM_DATA static DmapObjDef DmapShisa;
IWRAM_DATA static DmapObjDef DmapCat;
IWRAM_DATA static DmapObjDef DmapSmoke;
IWRAM_DATA static DmapObjDef DmapPanel;

IWRAM_DATA static u8  ucDmapCatStatus;

typedef struct {
    u16 x;
    u16 y;
    u16 animTimer;
    u16 animPat;
} DmapObjDef2;

IWRAM_DATA static DmapObjDef2 DmapPyramid;
IWRAM_DATA static u16  usDmapLidX;
IWRAM_DATA static u16  usDmapLidY;
IWRAM_DATA static u8   ucDmapDemoStatus;
IWRAM_DATA static u8   ucDmapClearStatus;
IWRAM_DATA static u8   ucDmapLightPaletteNo;
IWRAM_DATA u16  usDmapPatternPaletteNo;
IWRAM_DATA static u8   ucDmapSmokeFlag;

// ---- World position tables ----
// X positions for each world (Emerald, Ruby, Topaz, Sapphire, Golden)
static const s16 WorldPosX[6] = { 48, 56, 48, 200, 176, 152 };
static const s16 WorldPosY[6] = { 56, 56, 120, 56, 120, 80 };

// Pyramid position tables (per clear status)
static const s16 PyramidPosX[3] = { 60, 60, 152 };
static const s16 PyramidPosY[3] = { 104, 64, 72 };

// Lid position
#define DMAP_LID_POS_X  56
#define DMAP_LID_POS_Y  (6 * 16)  // 6 * 16 fixed point

// ---- Local functions ----
static void DmapBgScroll(void);
static void DmapWarioMove(void);
static void DmapLightColorChange(void);
static void DmapPatternColorChange(void);
static void DmapWarioAnimLoop(void);
static void DmapShisaAnimLoop(void);

// ---- SelectDmapInit ----
// Sets up BG layers, loads tile data, initializes positions.
void SelectDmapInit(void)
{
    ucStageNum = 6;

    // Clear palette and OBJ VRAM before setting up new scene
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)PLTT;
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (PLTT_SIZE >> 2);
    }
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)OBJ_VRAM0;
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x4000 >> 2);
    }
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)BG_VRAM;
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x18000 >> 2);
    }

    // ---- Load BG palette & tiles ----
    REG_DMA3SAD = (u32)map_select_bg_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (256*2 >> 2);

    REG_DMA3SAD = (u32)map_select_obj_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (256*2 >> 2);

    // Copy BG tiles to VRAM (0x3800 bytes)
    REG_DMA3SAD = (u32)map_select_bg_Char;
    REG_DMA3DAD = (u32)BG_VRAM;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (0x3800 >> 2);

    // Copy OBJ tiles to OBJ VRAM (0x4000 bytes)
    REG_DMA3SAD = (u32)map_select_obj_Char;
    REG_DMA3DAD = (u32)OBJ_VRAM0;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (0x4000 >> 2);

    // Copy screen tilemaps to BG VRAM
    // BG0: blend layer → screenbase 24 (0xC000)
    REG_DMA3SAD = (u32)map_select_1_Screen;
    REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0xC000);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (0x800 >> 2);

    // BG1: base near layer → screenbase 26 (0xD000)
    REG_DMA3SAD = (u32)map_select_2_Screen;
    REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0xD000);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (0x800 >> 2);

    // BG2: base far layer → screenbase 28 (0xE000)
    REG_DMA3SAD = (u32)map_select_3_Screen;
    REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0xE000);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (0x800 >> 2);

    // BG3: message text → screenbase 30 (0xF000)
    REG_DMA3SAD = (u32)map_select_message_Screen;
    REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0xF000);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (0x800 >> 2);

    // BG control registers
    // BG0: 256x256 16-color, priority 1, screenbase 24, charbase 0 — blend/window
    REG_BG0CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(24) | BGCNT_CHARBASE(0);
    // BG1: 256x256 16-color, priority 2, screenbase 26, charbase 0 — near layer
    REG_BG1CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(2)
               | BGCNT_SCREENBASE(26) | BGCNT_CHARBASE(0);
    // BG2: 256x256 16-color, priority 3, screenbase 28, charbase 0 — far layer
    REG_BG2CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(3)
               | BGCNT_SCREENBASE(28) | BGCNT_CHARBASE(0);
    // BG3: 256x256 16-color, priority 0, screenbase 30, charbase 0 — message
    REG_BG3CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(30) | BGCNT_CHARBASE(0);

    // ---- Initialize variables ----
    usSelectCharCount = 0;
    SelectCursor.animTimer = 0;
    SelectCursor.animPat = 0;
    SelectCursor.sX = WorldPosX[ucWorldNum] << 5;
    SelectCursor.sY = WorldPosY[ucWorldNum] << 5;
    ucSelectWarioStatus = MAP_WARIO_DOWN;
    bSelectMovingFlag = 0;

    // Light
    DmapLight.animTimer = 0;
    DmapLight.animPat = 0;
    ucDmapLightPaletteNo = 0;

    // Shisa (pyramid guardian)
    DmapShisa.animTimer = 0;
    DmapShisa.animPat = 0;

    // Black cat
    ucDmapCatStatus = 5;  // hidden by default
    DmapCat.animTimer = 0;
    DmapCat.animPat = 0;

    // Flag
    bSelectMovingFlag = 0;
    ucWorldNumBak = ucWorldNum;

    // Lid
    usDmapLidX = DMAP_LID_POS_X;
    usDmapLidY = DMAP_LID_POS_Y;

    // Demo
    ucDmapDemoStatus = MAP_DEMO_NONE;
    ucSelectDemoSeq = 0;
    usSelectDemoCount = 0;

    // Smoke
    ucDmapSmokeFlag = 0;
    DmapSmoke.animTimer = 0;
    DmapSmoke.animPat = 0;

    // Panel
    DmapPanel.animTimer = 0;
    DmapPanel.animPat = 0;

    // BG speed
    ucDmapMoveCount = 0;

    // Pyramid
    DmapPyramid.animTimer = 0;
    DmapPyramid.animPat = 0;
    DmapPyramid.x = PyramidPosX[ucDmapClearStatus] << 4;
    DmapPyramid.y = PyramidPosY[ucDmapClearStatus] << 4;

    // Scroll position
    sDmapBgPosY = 0;
    REG_BG0VOFS = 0;
    REG_BG0HOFS = 0;
    REG_BG1VOFS = 0;
    REG_BG1HOFS = 0;
    REG_BG2VOFS = 0;
    REG_BG2HOFS = 0;
    REG_BG3VOFS = 0;
    REG_BG3HOFS = 0;

    // Alpha blend: BG0=1st target, BG1+BG2=2nd target
    REG_BLDCNT = BLDCNT_TGT1_BG0
               | BLDCNT_TGT2_BG1 | BLDCNT_TGT2_BG2
               | BLDCNT_EFFECT_BLEND;
    // EVA=8, EVB=8 — 50% blend
    REG_BLDALPHA = 0x0808;

    // Window: initially closed (no tutorial needed → full open)
    if (ucDmapClearStatus == MAP_CLEAR_NONE)
    {
        sDmapWindowLeftX = 80;
        sDmapWindowRightX = 160;
    }
    else
    {
        sDmapWindowLeftX = 0;
        sDmapWindowRightX = 240;
    }

    REG_WIN0H = WIN_RANGE(sDmapWindowLeftX, sDmapWindowRightX);
    REG_WIN0V = WIN_RANGE(0, 160);
    REG_WININ  = WININ_WIN0_BG0 | WININ_WIN0_BG1 | WININ_WIN0_BG2
               | WININ_WIN0_BG3 | WININ_WIN0_OBJ | WININ_WIN0_CLR;
    REG_WINOUT = 0;

    // Enable display: Mode 0, OBJ + all BGs + Window 0
    REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP
                | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_BG2_ON
                | DISPCNT_BG3_ON | DISPCNT_WIN0_ON;
}

// ---- GameSelectDmap ----
// Main per-frame world map logic
int GameSelectDmap(void)
{
    usSelectCharCount = 0;

    switch (ucDmapDemoStatus)
    {
    case MAP_DEMO_NONE:
        // Normal: Wario walks around, player selects levels
        if (bSelectMovingFlag)
        {
            DmapBgScroll();
            DmapWarioMove();
        }
        else
        {
            // Check directional input
            if (usTrg & DPAD_RIGHT)
            {
                bSelectMovingFlag = 1;
                ucRoadKind = 0;
                sSelectMoveSpeedX = 128;  // move right
                sSelectMoveSpeedY = 0;
                ucSelectWarioStatus = MAP_WARIO_RIGHT;
            }
            else if (usTrg & DPAD_LEFT)
            {
                bSelectMovingFlag = 1;
                ucRoadKind = 1;
                sSelectMoveSpeedX = -128;
                sSelectMoveSpeedY = 0;
                ucSelectWarioStatus = MAP_WARIO_LEFT;
            }
            else if (usTrg & DPAD_UP)
            {
                bSelectMovingFlag = 1;
                ucRoadKind = 2;
                sSelectMoveSpeedX = 0;
                sSelectMoveSpeedY = -128;
                ucSelectWarioStatus = MAP_WARIO_UP;
            }
            else if (usTrg & DPAD_DOWN)
            {
                bSelectMovingFlag = 1;
                ucRoadKind = 3;
                sSelectMoveSpeedX = 0;
                sSelectMoveSpeedY = 128;
                ucSelectWarioStatus = MAP_WARIO_DOWN;
            }

            // A button or start: enter level
            if (usTrg & (A_BUTTON | START_BUTTON))
            {
                ucDmapDemoStatus = MAP_DEMO_TO_MMAP;
            }
        }

        // Wario animation loop
        DmapWarioAnimLoop();
        DmapPatternColorChange();
        DmapShisaAnimLoop();
        break;

    case MAP_DEMO_START:
        // Opening demo: Wario walks in, pyramid rises
        // Simplified: skip to normal
        ucDmapDemoStatus = MAP_DEMO_NONE;
        break;

    case MAP_DEMO_TUTORIAL:
        // Tutorial demo after clearing first world
        // Simplified: skip to normal
        ucDmapDemoStatus = MAP_DEMO_NONE;
        break;

    case MAP_DEMO_BOSS:
        // Final boss demo: pyramid opens
        // Simplified: skip to normal
        ucDmapDemoStatus = MAP_DEMO_NONE;
        break;

    case MAP_DEMO_TO_MMAP:
        // Transition to mini map
        DmapWarioAnimLoop();
        if (SelectWindowClose())
        {
            return 1;  // signal to move to mini map
        }
        break;
    }

    DmapLightColorChange();
    return 0;
}

// ---- DmapBgScroll ----
// Scrolls BG layers as Wario moves
static void DmapBgScroll(void)
{
    sDmapBgPosY += sDmapBgScrollSpeedY;

    REG_BG0VOFS = sDmapBgPosY >> 4;
    REG_BG1VOFS = sDmapBgPosY >> 4;
    REG_BG2VOFS = sDmapBgPosY >> 4;
}

// ---- DmapWarioMove ----
// Moves Wario along the selected road
static void DmapWarioMove(void)
{
    // Move toward target at fixed speed
    SelectCursor.sX += sSelectMoveSpeedX;
    SelectCursor.sY += sSelectMoveSpeedY;

    // Check if reached target (simplified: stop after ~16 pixels)
    if (++ucDmapMoveCount >= 16)
    {
        ucDmapMoveCount = 0;
        bSelectMovingFlag = 0;
        sSelectMoveSpeedX = 0;
        sSelectMoveSpeedY = 0;
    }
}

// ---- DmapLightColorChange ----
// Cycles light effect palette for the current area
static void DmapLightColorChange(void)
{
    // Light palette cycling (simplified)
    if ((usFadeTimer & 3) == 3)
    {
        ucDmapLightPaletteNo = (ucDmapLightPaletteNo + 1) & 15;
    }
}

// ---- DmapPatternColorChange ----
// Changes color of cleared area patterns
static void DmapPatternColorChange(void)
{
    // Pattern color shift (simplified)
}

// ---- DmapWarioAnimLoop ----
// Updates Wario's OBJ animation frame
static void DmapWarioAnimLoop(void)
{
    // Wario walk cycle (simplified)
    // Full implementation would use SelectWarioAnimPtr to look up frames
    SelectCursor.animTimer++;
    SelectCursor.animPat = (SelectCursor.animTimer >> 2) & 3;
}

// ---- DmapShisaAnimLoop ----
// Updates shisa (pyramid guardian) animation
static void DmapShisaAnimLoop(void)
{
    DmapShisa.animTimer++;
}

// ---- SelectDmapOamCreate ----
// Renders all OBJs for the world map
void SelectDmapOamCreate(void)
{
    u16 *dst = (u16 *)OamBuf;

    // TODO: Render Wario cursor, pyramid, shisa, cat, lid, smoke, panels
    // For now, render a basic Wario cursor

    // Wario cursor at current position
    {
        int x = (SelectCursor.sX >> 5) - (sDmapBgPosY >> 4);
        int y = (SelectCursor.sY >> 5);
        // Simple placeholder OBJ
        static const u16 wario_cursor[] = { 1, 0x4000, 0x8000, 0x0000 };
        dst = SetObj(wario_cursor, dst, x, y);
    }

    EndObj(dst);
}

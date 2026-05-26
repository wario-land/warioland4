// World map scene (dmap) -- Wario walks on the world map between levels
//
// BG layers:
//   BG0: blend overlay (screenbase 24) -- windowed swirl/transition effect
//   BG1: base map near layer (screenbase 26) -- foreground terrain
//   BG2: base map far layer (screenbase 28) -- background terrain
//   BG3: message text (screenbase 30) -- "Emerald Passage" etc.
//
// OBJ: Wario cursor, pyramid, guardian enemies, black cat, lid, smoke,
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
IWRAM_DATA s16  sDmapBgPosY;         // BG Y position (16x fixed point) -- accessed by select.c VBlank handler
IWRAM_DATA static u8   ucRoadKind;          // Which road is selected
IWRAM_DATA static u8   ucDmapMoveCount;     // Movement counter
IWRAM_DATA static s16  sDmapBgScrollSpeedY; // BG scroll speed

typedef struct {
    u16 animTimer;
    u16 animPat;
} DmapObjDef;

IWRAM_DATA static DmapObjDef DmapLight;
IWRAM_DATA static DmapObjDef DmapGuardian;
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

// ---- World position tables (matching wl4leaks seldmap_data.c) ----
// World enum: TUTORIAL=0, WORLD_1=1, WORLD_2=2, WORLD_3=3, WORLD_4=4, WORLD_BOSS=5, WORLD_SROOM=6
#define DMAP_BG_SCROLL  72
#define DMAP_WORLD_NUM  7

// Wario pixel positions on the map for each world
static const s16 WorldPosX[DMAP_WORLD_NUM] = {120, 193, 191, 49, 47, 120, 120};
static const s16 WorldPosY[DMAP_WORLD_NUM] = {
    186-8-DMAP_BG_SCROLL, 161-8-DMAP_BG_SCROLL, 78-8, 78-8,
    161-8-DMAP_BG_SCROLL, 160-8-DMAP_BG_SCROLL, 36
};

// BG scroll Y positions per world
static const s16 WorldPos[DMAP_WORLD_NUM] = {
    DMAP_BG_SCROLL, DMAP_BG_SCROLL, 0, 0, DMAP_BG_SCROLL, DMAP_BG_SCROLL, 0
};

// World enum for routing (local to DMAP, different from select.h states)
enum { W_TUTO=0, W_W1=1, W_W2=2, W_W3=3, W_W4=4, W_BOSS=5, W_SROOM=6, W_NOWORLD=7 };

// Movement routing table: [clear_status][from_world][direction] -> destination world
// Directions: LEFT=0, RIGHT=1, UP=2, DOWN=3
static const u8 WorldIdo[3][DMAP_WORLD_NUM][4] = {
    // ---- Nothing cleared ----
    {
        {W_NOWORLD, W_NOWORLD, W_NOWORLD, W_NOWORLD},  // Tutorial
        {W_TUTO,    W_W2,      W_W2,      W_TUTO    },  // World 1
        {W_SROOM,   W_W1,      W_SROOM,   W_W1      },  // World 2
        {W_W4,      W_SROOM,   W_SROOM,   W_W4      },  // World 3
        {W_W3,      W_TUTO,    W_W3,      W_TUTO    },  // World 4
        {W_NOWORLD, W_NOWORLD, W_NOWORLD, W_TUTO    },  // Boss
        {W_W3,      W_W2,      W_NOWORLD, W_NOWORLD },  // Sound Room
    },
    // ---- Tutorial cleared ----
    {
        {W_W4,      W_W1,      W_NOWORLD, W_NOWORLD },  // Tutorial
        {W_TUTO,    W_W2,      W_W2,      W_TUTO    },  // World 1
        {W_SROOM,   W_W1,      W_SROOM,   W_W1      },  // World 2
        {W_W4,      W_SROOM,   W_SROOM,   W_W4      },  // World 3
        {W_W3,      W_TUTO,    W_W3,      W_TUTO    },  // World 4
        {W_NOWORLD, W_NOWORLD, W_NOWORLD, W_TUTO    },  // Boss
        {W_W3,      W_W2,      W_NOWORLD, W_NOWORLD },  // Sound Room
    },
    // ---- All bosses cleared ----
    {
        {W_W4,      W_W1,      W_BOSS,    W_NOWORLD },  // Tutorial
        {W_TUTO,    W_W2,      W_W2,      W_TUTO    },  // World 1
        {W_SROOM,   W_W1,      W_SROOM,   W_W1      },  // World 2
        {W_W4,      W_SROOM,   W_SROOM,   W_W4      },  // World 3
        {W_W3,      W_TUTO,    W_W3,      W_TUTO    },  // World 4
        {W_NOWORLD, W_NOWORLD, W_NOWORLD, W_TUTO    },  // Boss
        {W_W3,      W_W2,      W_NOWORLD, W_NOWORLD },  // Sound Room
    },
};

// Road types for each world+direction
enum RoadKind {
    L_U_D_ROAD, L_D_U_ROAD, R_U_D_ROAD, R_D_U_ROAD,
    L_SROOM_ROAD, R_SROOM_ROAD, L_TUTO_ROAD, TUTO_L_ROAD,
    R_TUTO_ROAD, TUTO_R_ROAD, TUTO_BOSS_ROAD, BOSS_TUTO_ROAD,
    SROOM_L_ROAD, SROOM_R_ROAD, NO_ROAD
};

static const u8 WorldRoad[DMAP_WORLD_NUM][4] = {
    {TUTO_L_ROAD,  TUTO_R_ROAD,  TUTO_BOSS_ROAD, NO_ROAD       }, // Tutorial
    {R_TUTO_ROAD,  R_D_U_ROAD,   R_D_U_ROAD,     R_TUTO_ROAD   }, // World 1
    {R_SROOM_ROAD, R_U_D_ROAD,   R_SROOM_ROAD,   R_U_D_ROAD    }, // World 2
    {L_U_D_ROAD,   L_SROOM_ROAD, L_SROOM_ROAD,   L_U_D_ROAD    }, // World 3
    {L_D_U_ROAD,   L_TUTO_ROAD,  L_D_U_ROAD,     L_TUTO_ROAD   }, // World 4
    {NO_ROAD,      NO_ROAD,      NO_ROAD,        BOSS_TUTO_ROAD}, // Boss
    {SROOM_L_ROAD, SROOM_R_ROAD, NO_ROAD,        NO_ROAD       }, // Sound Room
};

// Wario facing direction per road type
static const u8 WarioWalkMuki[14] = {
    MAP_WARIO_DOWN, MAP_WARIO_UP, MAP_WARIO_DOWN, MAP_WARIO_UP,
    MAP_WARIO_RIGHT, MAP_WARIO_LEFT, MAP_WARIO_RIGHT, MAP_WARIO_LEFT,
    MAP_WARIO_LEFT, MAP_WARIO_RIGHT, MAP_WARIO_UP, MAP_WARIO_DOWN,
    MAP_WARIO_LEFT, MAP_WARIO_RIGHT
};

// Frame duration to traverse each road type
static const u8 DmapFrameSpeed[14] = {41, 41, 41, 41, 35, 35, 38, 38, 38, 38, 13, 13, 35, 35};

// Horizontal drift speed for up/down roads
static const s16 DmapWarioShosokudoX[4] = {-50, -50, 50, 50};

// Pyramid position tables (per clear status: none, tutorial, boss)
static const s16 PyramidPosX[3] = {120, 120, 120};
static const s16 PyramidPosY[3] = {278, 178, 148};

// Lid position
#define DMAP_LID_POS_X  120
#define DMAP_LID_POS_Y  120

// ---- Local functions ----
static void DmapBgScroll(void);
static void DmapWarioMove(void);
static void DmapLightColorChange(void);
static void DmapPatternColorChange(void);
static void DmapWarioAnimLoop(void);
static void DmapGuardianAnimLoop(void);

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
    // BG0: blend layer -> screenbase 24 (0xC000)
    REG_DMA3SAD = (u32)map_select_1_Screen;
    REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0xC000);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (0x800 >> 2);

    // BG1: base near layer -> screenbase 26 (0xD000)
    REG_DMA3SAD = (u32)map_select_2_Screen;
    REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0xD000);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (0x800 >> 2);

    // BG2: base far layer -> screenbase 28 (0xE000)
    REG_DMA3SAD = (u32)map_select_3_Screen;
    REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0xE000);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (0x800 >> 2);

    // BG3: message text -> screenbase 30 (0xF000)
    REG_DMA3SAD = (u32)map_select_message_Screen;
    REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0xF000);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (0x800 >> 2);

    // BG control registers
    // BG0: 256x256 16-color, priority 1, screenbase 24, charbase 0 -- blend/window
    REG_BG0CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(24) | BGCNT_CHARBASE(0);
    // BG1: 256x256 16-color, priority 2, screenbase 26, charbase 0 -- near layer
    REG_BG1CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(2)
               | BGCNT_SCREENBASE(26) | BGCNT_CHARBASE(0);
    // BG2: 256x256 16-color, priority 3, screenbase 28, charbase 0 -- far layer
    REG_BG2CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(3)
               | BGCNT_SCREENBASE(28) | BGCNT_CHARBASE(0);
    // BG3: 256x256 16-color, priority 0, screenbase 30, charbase 0 -- message
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

    // Guardian (pyramid guardian)
    DmapGuardian.animTimer = 0;
    DmapGuardian.animPat = 0;

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

    // Clear status: determine how much of the game is cleared
    // TODO: check actual W4ItemStatus for accurate clear status
    ucDmapClearStatus = MAP_CLEAR_NONE;

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

    // Scroll position: start at current world's BG position
    sDmapBgPosY = WorldPos[ucWorldNum] << 4;
    REG_BG0VOFS = sDmapBgPosY >> 4;
    REG_BG0HOFS = 0;
    REG_BG1VOFS = sDmapBgPosY >> 4;
    REG_BG1HOFS = 0;
    REG_BG2VOFS = sDmapBgPosY >> 4;
    REG_BG2HOFS = 0;
    REG_BG3VOFS = 0;
    REG_BG3HOFS = 0;

    // Alpha blend: BG0=1st target, BG1+BG2=2nd target
    REG_BLDCNT = BLDCNT_TGT1_BG0
               | BLDCNT_TGT2_BG1 | BLDCNT_TGT2_BG2
               | BLDCNT_EFFECT_BLEND;
    // EVA=8, EVB=8 -- 50% blend
    REG_BLDALPHA = 0x0808;

    // Window: initially full screen for visibility
    // The narrow window (80-160) is used for demo sequences;
    // during normal gameplay the full map is visible.
    sDmapWindowLeftX = 0;
    sDmapWindowRightX = 240;

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

// ---- SelectDmapInit3 ----
// Entry from file select after pyramid intro (sGameSeq = INIT_DMAP_3 = 5).
// Sets up the "start demo" where Wario slides into the tutorial world.
// Matches the original game flow: Title -> FileSelect -> Pyramid Intro -> DMAP start demo.
// This is also where a "new game" officially begins -- ucSaveFlg is set here.
void SelectDmapInit3(void)
{
    SelectDmapInit();
    ucWorldNum = 0;             // TUTORIAL world
    sDmapBgPosY = 0;
    ucDmapDemoStatus = MAP_DEMO_START;
    ucSelectDemoSeq = 0;
    usSelectDemoCount = 0;

    // Mark save as active -- the new game has officially started
    // This ensures subsequent boots skip the pyramid intro and go to DMAP.
    if (!ucSaveFlg)
    {
        ucSaveFlg = 1;
        ucStageNum = 6;         // World map
        ucWorldNum = 0;         // Tutorial/Emerald Passage
    }

    sGameSeq = FIN_DMAP;
}

// ---- SelectDmapInit4 ----
// Entry for start demo / opening cutscene (sGameSeq = INIT_DMAP_4 = 6).
void SelectDmapInit4(void)
{
    SelectDmapInit();
    ucDmapDemoStatus = MAP_DEMO_START;
    ucSelectDemoSeq = 0;
    usSelectDemoCount = 0;
    sGameSeq = FIN_DMAP;
}

// ---- SelectDmapInit5 ----
// Entry for tutorial/boss clear demo (sGameSeq = INIT_DMAP_5 = 7).
void SelectDmapInit5(void)
{
    SelectDmapInit();
    ucDmapDemoStatus = MAP_DEMO_TUTORIAL;
    ucSelectDemoSeq = 0;
    usSelectDemoCount = 0;
    sGameSeq = FIN_DMAP;
}

// ---- GameSelectDmap ----
// Main per-frame world map logic.
// Returns 1 when transitioning to mini map (MMAP).
int GameSelectDmap(void)
{
    int i;

    usSelectCharCount = 0;

    switch (ucDmapDemoStatus)
    {
    case MAP_DEMO_NONE:
        // ================================================================
        // Normal gameplay: Wario walks around the world map
        // ================================================================
        if (bSelectMovingFlag)
        {
            // Wario is moving along a road between worlds
            DmapBgScroll();
            DmapWarioMove();
        }
        else
        {
            // Check directional input for world-to-world movement
            // Uses WORLD_IDO[clearStatus][currentWorld][direction] to find
            // destination world, and WORLD_ROAD for road type.
            if (usTrg & DPAD_RIGHT)
            {
                i = WorldIdo[ucDmapClearStatus][ucWorldNum][1];  // RIGHT
                if (i != W_NOWORLD)
                {
                    ucRoadKind = WorldRoad[ucWorldNum][1];
                    ucSelectWarioStatus = WarioWalkMuki[ucRoadKind];
                    ucWorldNum = i;
                    bSelectMovingFlag = 1;
                    ucDmapMoveCount = 0;
                    // Calculate movement speed based on road type
                    sSelectMoveSpeedX = ((WorldPosX[i] - (SelectCursor.sX >> 5)) << 5) / DmapFrameSpeed[ucRoadKind];
                    sSelectMoveSpeedY = ((WorldPosY[i] - (SelectCursor.sY >> 5)) << 5) / DmapFrameSpeed[ucRoadKind];
                    sDmapBgScrollSpeedY = ((WorldPos[i] - (sDmapBgPosY >> 4)) << 4) / DmapFrameSpeed[ucRoadKind];
                }
            }
            else if (usTrg & DPAD_LEFT)
            {
                i = WorldIdo[ucDmapClearStatus][ucWorldNum][0];  // LEFT
                if (i != W_NOWORLD)
                {
                    ucRoadKind = WorldRoad[ucWorldNum][0];
                    ucSelectWarioStatus = WarioWalkMuki[ucRoadKind];
                    ucWorldNum = i;
                    bSelectMovingFlag = 1;
                    ucDmapMoveCount = 0;
                    sSelectMoveSpeedX = ((WorldPosX[i] - (SelectCursor.sX >> 5)) << 5) / DmapFrameSpeed[ucRoadKind];
                    sSelectMoveSpeedY = ((WorldPosY[i] - (SelectCursor.sY >> 5)) << 5) / DmapFrameSpeed[ucRoadKind];
                    sDmapBgScrollSpeedY = ((WorldPos[i] - (sDmapBgPosY >> 4)) << 4) / DmapFrameSpeed[ucRoadKind];
                }
            }
            else if (usTrg & DPAD_UP)
            {
                i = WorldIdo[ucDmapClearStatus][ucWorldNum][2];  // UP
                if (i != W_NOWORLD)
                {
                    ucRoadKind = WorldRoad[ucWorldNum][2];
                    ucSelectWarioStatus = WarioWalkMuki[ucRoadKind];
                    ucWorldNum = i;
                    bSelectMovingFlag = 1;
                    ucDmapMoveCount = 0;
                    sSelectMoveSpeedX = ((WorldPosX[i] - (SelectCursor.sX >> 5)) << 5) / DmapFrameSpeed[ucRoadKind];
                    sSelectMoveSpeedY = ((WorldPosY[i] - (SelectCursor.sY >> 5)) << 5) / DmapFrameSpeed[ucRoadKind];
                    sDmapBgScrollSpeedY = ((WorldPos[i] - (sDmapBgPosY >> 4)) << 4) / DmapFrameSpeed[ucRoadKind];
                }
            }
            else if (usTrg & DPAD_DOWN)
            {
                i = WorldIdo[ucDmapClearStatus][ucWorldNum][3];  // DOWN
                if (i != W_NOWORLD)
                {
                    ucRoadKind = WorldRoad[ucWorldNum][3];
                    ucSelectWarioStatus = WarioWalkMuki[ucRoadKind];
                    ucWorldNum = i;
                    bSelectMovingFlag = 1;
                    ucDmapMoveCount = 0;
                    sSelectMoveSpeedX = ((WorldPosX[i] - (SelectCursor.sX >> 5)) << 5) / DmapFrameSpeed[ucRoadKind];
                    sSelectMoveSpeedY = ((WorldPosY[i] - (SelectCursor.sY >> 5)) << 5) / DmapFrameSpeed[ucRoadKind];
                    sDmapBgScrollSpeedY = ((WorldPos[i] - (sDmapBgPosY >> 4)) << 4) / DmapFrameSpeed[ucRoadKind];
                }
            }

            // A button: enter the current world's mini map
            if (usTrg & (A_BUTTON | START_BUTTON))
            {
                ucDmapDemoStatus = MAP_DEMO_TO_MMAP;
                ucSelectDemoSeq = 0;
                usSelectDemoCount = 0;
            }
        }

        // Per-frame animation updates
        DmapWarioAnimLoop();
        DmapPatternColorChange();
        DmapGuardianAnimLoop();
        break;

    case MAP_DEMO_START:
        // Opening demo: Wario slides into the tutorial world
        // Simplified: skip to normal after brief delay
        if (++usSelectDemoCount > 60)
        {
            usSelectDemoCount = 0;
            ucDmapDemoStatus = MAP_DEMO_NONE;
        }
        break;

    case MAP_DEMO_TUTORIAL:
        // Tutorial clear demo: pyramid rises
        if (++usSelectDemoCount > 120)
        {
            usSelectDemoCount = 0;
            ucDmapDemoStatus = MAP_DEMO_NONE;
        }
        break;

    case MAP_DEMO_BOSS:
        // Final boss demo: pyramid opens
        if (++usSelectDemoCount > 180)
        {
            usSelectDemoCount = 0;
            ucDmapDemoStatus = MAP_DEMO_NONE;
        }
        break;

    case MAP_DEMO_TO_MMAP:
        // Transition to mini map: close window then signal
        DmapWarioAnimLoop();
        if (SelectWindowClose())
        {
            return 1;  // Signal to move to mini map (MMAP)
        }
        break;
    }

    DmapLightColorChange();
    return 0;
}

// ---- DmapBgScroll ----
// Scrolls BG layers as Wario moves between worlds.
// Only updates the logical scroll position sDmapBgPosY.
// The VBlank handler (SelectVblankHandler) applies this to hardware registers.
static void DmapBgScroll(void)
{
    sDmapBgPosY += sDmapBgScrollSpeedY;
}

// ---- DmapWarioMove ----
// Moves Wario along the selected road between worlds.
// Uses DMAP_FRAME_SPEED for road traversal duration.
// For up/down roads (L_U_D_ROAD etc.), adds horizontal drift.
static void DmapWarioMove(void)
{
    s16 addSpeedX = 0;
    u8 targetWorld = WorldIdo[ucDmapClearStatus][ucWorldNum][0];  // fallback

    // Apply horizontal drift for vertical roads
    if (ucRoadKind < L_SROOM_ROAD)
    {
        addSpeedX = DmapWarioShosokudoX[ucRoadKind]
                  - ((DmapWarioShosokudoX[ucRoadKind] * 2 * ucDmapMoveCount)
                     / DmapFrameSpeed[ucRoadKind]);
    }

    SelectCursor.sX += sSelectMoveSpeedX + addSpeedX;
    SelectCursor.sY += sSelectMoveSpeedY;

    // Check if movement is complete
    if (++ucDmapMoveCount >= DmapFrameSpeed[ucRoadKind])
    {
        ucDmapMoveCount = 0;
        bSelectMovingFlag = 0;
        // Snap to exact destination position
        SelectCursor.sX = WorldPosX[ucWorldNum] << 5;
        SelectCursor.sY = WorldPosY[ucWorldNum] << 5;
        sDmapBgPosY = WorldPos[ucWorldNum] << 4;
        sDmapBgScrollSpeedY = 0;
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

// Animation tables imported from dmap_wario_data.c
extern const SelectAnimDef *const DmapWarioAnimTable[4];

// ---- DmapWarioAnimLoop ----
// Updates Wario's OBJ animation frame using the AnmDef animation system.
// Matches wl4leaks DmapWarioAnmLoop: cycles through AnmDef frames based
// on per-frame timers, wrapping at the {0,0} terminator.
static void DmapWarioAnimLoop(void)
{
    SelectAnimDef *anim;

    // Set animation pointer based on Wario's current direction
    SelectWarioAnimPtr = (SelectAnimDef *)DmapWarioAnimTable[ucSelectWarioStatus];

    anim = &SelectWarioAnimPtr[SelectCursor.animPat];
    if (++SelectCursor.animTimer >= anim->timer)
    {
        SelectCursor.animTimer = 0;
        SelectCursor.animPat++;
        // Check for terminator (timer == 0) and wrap
        if (SelectWarioAnimPtr[SelectCursor.animPat].timer == 0)
            SelectCursor.animPat = 0;
    }
}

// ---- DmapGuardianAnimLoop ----
// Updates guardian (pyramid guardian) animation
static void DmapGuardianAnimLoop(void)
{
    DmapGuardian.animTimer++;
}

// ---- SelectDmapOamCreate ----
// Renders all OBJs for the world map using the AnmDef animation system.
// Wario is rendered from the current animation pattern (multi-part OBJ data
// from map_select_wario_Obj_XX arrays). Each pattern has [count] sub-sprites
// with pre-computed OAM attributes that include position offsets and tile indices.
void SelectDmapOamCreate(void)
{
    u16 *dst = (u16 *)OamBuf;

    // ---- Wario cursor at current position ----
    // Uses the animation pattern pointed to by SelectWarioAnimPtr.
    // Each pattern is a SetObj-compatible array: [count] [attr0,attr1,attr2]*
    // attr0 and attr1 contain sub-sprite Y/X offsets; SetObj adds the
    // global Wario position (sX>>5, sY>>5) to position each sub-sprite.
    if (SelectWarioAnimPtr)
    {
        const u16 *pattern = (const u16 *)SelectWarioAnimPtr[SelectCursor.animPat].objAddr;
        int wx = SelectCursor.sX >> 5;
        int wy = SelectCursor.sY >> 5;
        dst = SetObj(pattern, dst, wx, wy);
    }

    EndObj(dst);
}

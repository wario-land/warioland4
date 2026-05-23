// Select screen — world map, level select, mini map, room select
#ifndef SELECT_H
#define SELECT_H

#include "../gba/gba.h"

// ---- Select sub-modes (sGameSeq values within GameSelect) ----
enum SelectGameSeq {
    SEL_SEQ_DMAP_INIT = 0,
    SEL_SEQ_DMAP_MAIN,
    SEL_SEQ_DMAP_EXIT,

    SEL_SEQ_DORA_INIT = 6,
    SEL_SEQ_DORA_MAIN,
    SEL_SEQ_DORA_EXIT,

    SEL_SEQ_MMAP_INIT = 12,
    SEL_SEQ_MMAP_MAIN,
    SEL_SEQ_MMAP_EXIT,

    SEL_SEQ_KIME_INIT = 18,
    SEL_SEQ_KIME_MAIN,
    SEL_SEQ_KIME_EXIT,

    SEL_SEQ_ROOM_INIT = 24,
    SEL_SEQ_ROOM_MAIN,
    SEL_SEQ_ROOM_EXIT,

    SEL_SEQ_JUMP_INIT = 30,
    SEL_SEQ_JUMP_MAIN,
    SEL_SEQ_JUMP_EXIT,

    SEL_SEQ_BOSS_DOOR_INIT = 36,
    SEL_SEQ_BOSS_DOOR_MAIN,
    SEL_SEQ_BOSS_DOOR_EXIT,

    SEL_SEQ_STAGE_EXIT = 42,
    SEL_SEQ_SELECT_EXIT = 46,
};

// ---- Directions ----
enum Direction {
    DIR_RIGHT,
    DIR_LEFT,
    DIR_UP,
    DIR_DOWN,
};

// ---- Wario status on world map ----
enum MapWarioStatus {
    MAP_WARIO_DOWN = 0,
    MAP_WARIO_UP   = 1,
    MAP_WARIO_LEFT = 2,
    MAP_WARIO_RIGHT = 3,
};

// ---- Map demo status ----
enum MapDemoStatus {
    MAP_DEMO_NONE = 0,
    MAP_DEMO_START,
    MAP_DEMO_TUTORIAL,
    MAP_DEMO_BOSS,
    MAP_DEMO_TO_MMAP,
};

// ---- Map clear status ----
enum MapClearStatus {
    MAP_CLEAR_NONE = 0,     // Nothing cleared
    MAP_CLEAR_TUTORIAL,     // Tutorial world cleared
    MAP_CLEAR_BOSS,         // Boss cleared (all worlds)
};

// ---- Animation definition ----
typedef struct {
    u32 objAddr;
    u8  timer;
} SelectAnimDef;

// ---- Cursor definition ----
typedef struct {
    u16 animTimer;
    u16 animPat;
    s16 sX;     // Fixed-point X position (<<5)
    s16 sY;     // Fixed-point Y position (<<5)
    u16 rotate;
    u16 scaleX;
    u16 scaleY;
} SelectCursorDef;

// ---- BG affine definition ----
typedef struct {
    s16 sX;     // Fixed-point X position
    s16 sY;     // Fixed-point Y position
    s16 pa, pb, pc, pd;
    s32 startX, startY;
} SelectBgDef;

// ---- IWRAM variables (select) ----
extern u8  ucSelectVector;
extern SelectCursorDef SelectCursor;
extern SelectBgDef SelectBg;
extern u8  ucDoorFlag;

extern u16 usSelectCharCount;
extern u8  ucSelectMessageNo;

extern u8  ucSelectDemoSeq;
extern u16 usSelectDemoCount;

extern s8  cSelectDemoEvb;
extern s8  cSelectDemoEva;
extern s8  cSelectDemoEvy;

extern SelectAnimDef *SelectWarioAnimPtr;
extern u8  ucSelectWarioStatus;
extern u8  bSelectMovingFlag;
extern s16 sSelectMoveSpeedX;
extern s16 sSelectMoveSpeedY;

extern u8  ucSelectWorldKind;

extern s16 sDmapWindowLeftX;
extern s16 sDmapWindowRightX;
extern s16 sDmapWindowUpY;
extern s16 sDmapWindowDownY;

extern u8  ucSelectButtonCount;
extern u8  ucSelectFootstepCount;
extern u8  ucSelectJewelFlag;

// ---- Function declarations ----
void GameSelect(void);

// Helper functions
int  SelectFadeWait(int duration);
int  SelectWindowClose(void);
int  SelectWindowOpen(void);

// Sub-mode functions (defined in seldmap.c)
void SelectDmapInit(void);
int  GameSelectDmap(void);
void SelectDmapOamCreate(void);

// Sub-mode functions (defined in selmmap.c)
void SelectMmapInit(void);
int  GameSelectMmap(void);
void SelectMmapOamCreate(void);

#endif // SELECT_H

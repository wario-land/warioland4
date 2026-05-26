// Select screen -- world map, level select, mini map, room select
#ifndef SELECT_H
#define SELECT_H

#include "../gba/gba.h"

// ---- Select sub-modes (sGameSeq values within GameSelect) ----
// These values match the IDA Pro / wl4leaks select state machine exactly.
// The GameSelect dispatcher uses a jump table indexed by these values.
//
// Flow pattern for each screen: INIT_* -> FIN_* (fade-in) -> MAIN (loop) -> FOUT_* (fade-out)
// Multiple INIT variants exist for different entry contexts:
//   _1 = first time / from title
//   _2 = returning from sub-screen
//   _3 = from ready/file-select (new game)
//   _4 = from demo completion
//   _5 = after special event (tutorial clear, boss clear)
enum SelectGameSeq {
    // ---- DMAP: World Map (dungeon map) ----
    INIT_DMAP_1 = 0,    // First entry from title/intro
    FIN_DMAP = 1,       // Fade-in to world map
    DMAP = 2,           // Main world map loop (Wario walks, player selects)
    FOUT_DMAP = 3,      // Fade-out from world map
    INIT_DMAP_2 = 4,    // Entry from stage map (MMAP) returning
    INIT_DMAP_3 = 5,    // Entry from file select / ready (pyramid intro done)
    INIT_DMAP_4 = 6,    // Entry for start demo (opening cutscene)
    INIT_DMAP_5 = 7,    // Entry for tutorial/boss clear demo

    // ---- SMAP: Stage Map (world overview) ----
    FIN_SMAP = 8,       // Fade-in to stage map
    SMAP = 9,           // Main stage map loop
    FOUT_SMAP = 10,     // Fade-out from stage map
    INIT_SMAP_2 = 11,   // Entry variant 2
    INIT_SMAP_3 = 12,   // Entry variant 3
    INIT_SMAP_4 = 13,   // Entry variant 4
    INIT_SMAP_5 = 14,   // Entry variant 5 (from room, after stage clear)

    // ---- DORA: Door/CD Transition Animation ----
    INIT_DORA = 15,     // Init door animation
    FIN_DORA = 16,      // Fade-in for door
    DORA = 17,          // Main door animation
    SCORE_TALLY = 18,        // Score tally / production screen
    FOUT_DORA = 19,     // Fade-out from door
    INIT_DORA_2 = 20,   // Door entry variant 2 (from stage end type 2)

    // ---- SELOUT: Select-Out Transition ----
    INIT_SELOUT = 21,   // Init select-out
    FIN_SELOUT = 22,    // Fade-in select-out
    SELOUT = 23,        // Main select-out
    FOUT_SELOUT = 24,   // Fade-out select-out

    // ---- SELDEMO: Select Demo ----
    INIT_SELDEMO = 25,  // Init select demo
    FIN_SELDEMO = 26,   // Fade-in select demo
    SELDEMO = 27,       // Main select demo
    FOUT_SELDEMO = 28,  // Fade-out select demo

    // ---- SELPOSE: Character Posing (stage name display) ----
    INIT_SELPOSE = 29,  // Init posing screen
    FIN_SELPOSE = 30,   // Fade-in posing
    SELPOSE = 31,       // Main posing loop
    FOUT_SELPOSE = 32,  // Fade-out posing

    // ---- MMAP: Mini Map (stage select within world) ----
    INIT_MMAP_1 = 33,   // Init mini map (first entry)
    FIN_MMAP = 34,      // Fade-in mini map
    MMAP = 35,          // Main mini map loop
    FOUT_MMAP = 36,     // Fade-out mini map
    INIT_MMAP_2 = 37,   // Mini map entry variant 2
    INIT_MMAP_3 = 38,   // Mini map entry variant 3 (from file select, continue game)
    INIT_MMAP_4 = 39,   // Mini map entry variant 4 (from mini game)
    MMAP_WAIT = 40,     // Mini map wait state

    // ---- BOSS_DOOR: Boss Door Screen ----
    INIT_BOSS_DOOR = 41,    // Init boss door
    FIN_BOSS_DOOR = 42,     // Fade-in boss door
    BOSS_DOOR = 43,         // Main boss door
    FOUT_BOSS_DOOR = 44,    // Fade-out boss door
    INIT_BOSS_DOOR_2 = 45,  // Boss door entry variant 2

    // ---- SROOM: Save Room (after stage complete) ----
    INIT_SROOM = 46,    // Init save room
    FIN_SROOM = 47,     // Fade-in save room
    SROOM = 48,         // Main save room loop
    FOUT_SROOM = 49,    // Fade-out save room
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

// Fade helpers
int  SelectFadeIn(int interval);
int  SelectFadeOut(int interval);
int  SelectFadeWait(int duration);
int  SelectWindowClose(void);
int  SelectWindowOpen(void);

// Sub-mode functions (defined in seldmap.c)
void SelectDmapInit(void);
void SelectDmapInit3(void);   // Entry from file select (pyramid intro done, start demo)
void SelectDmapInit4(void);   // Entry for start demo
void SelectDmapInit5(void);   // Entry for tutorial/boss clear demo
int  GameSelectDmap(void);
void SelectDmapOamCreate(void);

// Sub-mode functions (defined in selmmap.c)
void SelectMmapInit(void);
int  GameSelectMmap(void);
void SelectMmapOamCreate(void);

// DMAP IWRAM variables (accessed by select.c VBlank handler)
extern s16 sDmapBgPosY;

#endif // SELECT_H

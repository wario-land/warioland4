// Select screen: world map, level select, mini map, room select
//
// GameSelect() is the main entry point called from main.c when sMainSeq == MS_SELECT.
// It dispatches between sub-modes based on sGameSeq:
//
//   SEL_SEQ_DMAP: World map — Wario walks on a scrolling map between worlds.
//     The player navigates with D-pad. Press A to enter a world's mini map.
//
//   SEL_SEQ_MMAP: Mini map — Stage select within a world (6 normal stages
//     + 1 bonus stage + boss door). Left/right moves between stages.
//     Press A to select a stage and transition to posing/kime.
//
//   SEL_SEQ_DORA: Door/CD — Transition animation showing Wario entering a door
//     or a CD spinning. Plays when entering/leaving a stage.
//
//   SEL_SEQ_KIME: Posing — Wario poses with stage name and effects (flowers,
//     sparkles, smoke). Shows before entering the actual game stage.
//
//   SEL_SEQ_ROOM: Room — Save room screen with photo, level info, save options.
//
// Navigation flow:
//   Title → Ready → Dmap (world map) → Mmap (mini map) → Kime (posing)
//     → Game (stage)
//   After stage complete → Room (save) → Dmap (world map)

#include "select.h"
#include "../gba/gba.h"
#include "../gameutils.h"

// ---- Select IWRAM variables ----
IWRAM_DATA u8   ucSelectVector;
IWRAM_DATA SelectCursorDef SelectCursor;
IWRAM_DATA SelectBgDef SelectBg;
IWRAM_DATA u8   ucDoorFlag;

IWRAM_DATA u16  usSelectCharCount;
IWRAM_DATA u8   ucSelectMessageNo;

IWRAM_DATA u8   ucSelectDemoSeq;
IWRAM_DATA u16  usSelectDemoCount;

IWRAM_DATA s8   cSelectDemoEvb;
IWRAM_DATA s8   cSelectDemoEva;
IWRAM_DATA s8   cSelectDemoEvy;

IWRAM_DATA SelectAnimDef *SelectWarioAnimPtr;
IWRAM_DATA u8   ucSelectWarioStatus;
IWRAM_DATA u8   bSelectMovingFlag;
IWRAM_DATA s16  sSelectMoveSpeedX;
IWRAM_DATA s16  sSelectMoveSpeedY;

IWRAM_DATA u8   ucSelectWorldKind;

IWRAM_DATA s16  sDmapWindowLeftX;
IWRAM_DATA s16  sDmapWindowRightX;
IWRAM_DATA s16  sDmapWindowUpY;
IWRAM_DATA s16  sDmapWindowDownY;

IWRAM_DATA u8   ucSelectButtonCount;
IWRAM_DATA u8   ucSelectFootstepCount;
IWRAM_DATA u8   ucSelectJewelFlag;

// ---- VBlank handler for select screen ----
// DMA-copies OamBuf to hardware OAM, updates BG scroll registers.
// Called every VBlank while in any select sub-mode.
static void SelectVblankHandler(void)
{
    // OAM DMA: copy OamBuf to hardware OAM if objects were rendered
    if (ucCntObj)
    {
        REG_DMA3SAD = (u32)OamBuf;
        REG_DMA3DAD = (u32)OAM;
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (128 * 8 >> 2);
        ucCntObj = 0;
    }

    // BG scroll register update (used by DMAP sub-mode for parallax)
    REG_BG0HOFS = usBg0Hofs;
    REG_BG0VOFS = usBg0Vofs;
    REG_BG1HOFS = usBg1Hofs;
    REG_BG1VOFS = usBg1Vofs;
    REG_BG2HOFS = usBg2Hofs;
    REG_BG2VOFS = usBg2Vofs;
    REG_BG3HOFS = usBg3Hofs;
    REG_BG3VOFS = usBg3Vofs;

    // Alpha blend register update (used by fade transitions)
    REG_BLDALPHA = (usBgEvb << 8) | usBgEva;
    REG_BLDY = usBgEvy;
}

static void SelectVblkSet(void)
{
    SetVblkFunc(SelectVblankHandler);
}

// ---- Main Select Dispatcher ----
//
// On first entry from ready screen (sGameSeq == 5), we set up VBlank
// and jump to the world map (DMAP) init.
//
// After initialization, the sub-mode functions advance sGameSeq themselves.
//
void GameSelect(void)
{
    // First entry from ready: set up VBlank and jump to world map init
    if (sGameSeq == 5)
    {
        SelectVblkSet();
        ucCntObj = 0;
        sGameSeq = SEL_SEQ_DMAP_INIT;
        ucSelectVector = 0;
    }

    switch (sGameSeq)
    {
    // ================================================================
    //  World Map (DMAP)
    // ================================================================
    case SEL_SEQ_DMAP_INIT:
        SelectDmapInit();
        sGameSeq = SEL_SEQ_DMAP_MAIN;
        break;

    case SEL_SEQ_DMAP_MAIN:
        // GameSelectDmap returns 1 when transitioning to next mode
        if (GameSelectDmap())
            sGameSeq = SEL_SEQ_DMAP_EXIT;
        // Render OBJs for this frame (will be DMA'd to OAM at next VBlank)
        SelectDmapOamCreate();
        ucCntObj = 1;
        break;

    case SEL_SEQ_DMAP_EXIT:
        sGameSeq = SEL_SEQ_MMAP_INIT;
        break;

    // ================================================================
    //  Mini Map / Stage Select (MMAP)
    // ================================================================
    case SEL_SEQ_MMAP_INIT:
        SelectMmapInit();
        sGameSeq = SEL_SEQ_MMAP_MAIN;
        break;

    case SEL_SEQ_MMAP_MAIN:
        // GameSelectMmap handles stage selection and navigation
        // Returns non-zero when transitioning
        if (GameSelectMmap())
        {
            // Transition to posing (KIME) or back to DMAP handled inside
        }
        SelectMmapOamCreate();
        ucCntObj = 1;
        break;

    case SEL_SEQ_MMAP_EXIT:
        // Transition to posing (KIME) or game start
        sGameSeq = SEL_SEQ_KIME_INIT;
        break;

    // ================================================================
    //  Door/CD Animation (DORA)
    // ================================================================
    case SEL_SEQ_DORA_INIT:
        // Transition animation init — simplified: skip to next state
        sGameSeq = SEL_SEQ_DORA_MAIN;
        break;

    case SEL_SEQ_DORA_MAIN:
        // Door transition animation: simple fade/timer
        if (SelectFadeWait(30))
            sGameSeq = SEL_SEQ_DORA_EXIT;
        break;

    case SEL_SEQ_DORA_EXIT:
        sGameSeq = SEL_SEQ_KIME_INIT;
        break;

    // ================================================================
    //  Character Posing (KIME)
    // ================================================================
    case SEL_SEQ_KIME_INIT:
        // Posing screen init — simplified: skip directly to game start
        // Full implementation would show Wario posing with stage name
        sGameSeq = SEL_SEQ_KIME_MAIN;
        break;

    case SEL_SEQ_KIME_MAIN:
        // Posing screen main — wait for animation or button
        // Simplified: auto-advance after short delay
        if (SelectFadeWait(20))
            sGameSeq = SEL_SEQ_KIME_EXIT;
        break;

    case SEL_SEQ_KIME_EXIT:
        // Transition to main game
        sMainSeq = MS_MAIN;
        sGameSeq = 0;
        break;

    // ================================================================
    //  Room / Save Screen (ROOM)
    // ================================================================
    case SEL_SEQ_ROOM_INIT:
        sGameSeq = SEL_SEQ_ROOM_MAIN;
        break;

    case SEL_SEQ_ROOM_MAIN:
        // Room screen main — simplified: auto-advance back to DMAP
        if (SelectFadeWait(10))
            sGameSeq = SEL_SEQ_ROOM_EXIT;
        break;

    case SEL_SEQ_ROOM_EXIT:
        // After room, return to world map
        sGameSeq = SEL_SEQ_DMAP_INIT;
        break;

    // ================================================================
    //  Stage Exit (JUMP sub-mode)
    // ================================================================
    case SEL_SEQ_JUMP_INIT:
        sGameSeq = SEL_SEQ_JUMP_MAIN;
        break;

    case SEL_SEQ_JUMP_MAIN:
        if (SelectFadeWait(16))
            sGameSeq = SEL_SEQ_JUMP_EXIT;
        break;

    case SEL_SEQ_JUMP_EXIT:
        sGameSeq = SEL_SEQ_STAGE_EXIT;
        break;

    // ================================================================
    //  Boss Door
    // ================================================================
    case SEL_SEQ_BOSS_DOOR_INIT:
        sGameSeq = SEL_SEQ_BOSS_DOOR_MAIN;
        break;

    case SEL_SEQ_BOSS_DOOR_MAIN:
        if (SelectFadeWait(20))
            sGameSeq = SEL_SEQ_BOSS_DOOR_EXIT;
        break;

    case SEL_SEQ_BOSS_DOOR_EXIT:
        sGameSeq = SEL_SEQ_STAGE_EXIT;
        break;

    // ================================================================
    //  Exit to game or title
    // ================================================================
    case SEL_SEQ_STAGE_EXIT:
        // Start the actual game stage
        sMainSeq = MS_MAIN;
        sGameSeq = 0;
        break;

    case SEL_SEQ_SELECT_EXIT:
        sMainSeq = MS_TITLE;
        sGameSeq = -1;
        break;

    default:
        break;
    }
}

// ---- Helper functions ----

// Wait for a specific number of frames, returns 1 when done
int SelectFadeWait(int duration)
{
    if (++usSelectDemoCount >= (unsigned)duration)
    {
        usSelectDemoCount = 0;
        return 1;
    }
    return 0;
}

// Close window effect (shrink window borders toward center horizontally)
int SelectWindowClose(void)
{
    if (sDmapWindowLeftX < 120)
    {
        sDmapWindowLeftX += 2;
        sDmapWindowRightX -= 2;
        REG_WIN0H = WIN_RANGE(sDmapWindowLeftX, sDmapWindowRightX);
        return 0;
    }
    return 1;
}

// Open window effect (expand window borders outward from center)
int SelectWindowOpen(void)
{
    if (sDmapWindowLeftX > 0)
    {
        sDmapWindowLeftX -= 2;
        sDmapWindowRightX += 2;
        REG_WIN0H = WIN_RANGE(sDmapWindowLeftX, sDmapWindowRightX);
        return 0;
    }
    return 1;
}

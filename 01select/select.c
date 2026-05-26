// Select screen: world map, level select, mini map, room select
//
// GameSelect() is the main entry point called from main.c when sMainSeq == MS_SELECT.
// It uses a jump-table dispatch on sGameSeq (matching IDA GameSelect at 0x807c1dc).
//
// The state machine values are defined in select.h and match the original game exactly.
// Each screen follows the pattern: INIT_* -> FIN_* (fade-in) -> MAIN (loop) -> FOUT_* (fade-out)
//
// Multiple INIT variants exist for different entry contexts:
//   _1 = first time / from title
//   _2 = returning from sub-screen
//   _3 = from ready/file-select (pyramid intro complete, new game)
//   _4 = from demo completion
//   _5 = after special event (tutorial/boss clear)
//
// Navigation flow:
//   Title -> Ready -> Title Pyramid Intro -> DMAP (world map) -> MMAP (mini map)
//     -> KIME (posing) -> Game (stage)
//   After stage complete -> Room (save) -> DMAP (world map)

#include "select.h"
#include "../gba/gba.h"
#include "../gameutils.h"
#include "../save/save.h"

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
// Matches IDA SelectVblkIntr01 at 0x807cd5c.
// ALWAYS DMA-copies OamBuf to hardware OAM (unconditional, all 128 OBJs).
// Then dispatches to sub-handlers based on sGameSeq.
static void SelectVblankHandler(void)
{
    // OAM DMA: always copy all 128 OBJs from OamBuf to OAM
    // IDA: MEMORY[0x40000DC] = 0x84000100 (256 words = 128 OBJs)
    REG_DMA3SAD = (u32)OamBuf;
    REG_DMA3DAD = (u32)OAM;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (128 * 8 >> 2);

    // Dispatch to sub-handler based on sGameSeq
    switch (sGameSeq)
    {
    case INIT_DMAP_1: case FIN_DMAP: case DMAP: case FOUT_DMAP:
    case INIT_DMAP_2: case INIT_DMAP_3: case INIT_DMAP_4: case INIT_DMAP_5:
        // DMAP: BG scroll + window update (matching IDA SelectDmapVblk)
        REG_BG0VOFS = (sDmapBgPosY >> 4);
        REG_BG0HOFS = 0;
        REG_BG1VOFS = (sDmapBgPosY >> 4);
        REG_BG1HOFS = 0;
        REG_BG2VOFS = (sDmapBgPosY >> 4);
        REG_BG2HOFS = 0;
        REG_BG3VOFS = 0;
        REG_BG3HOFS = 0;
        REG_WIN0H = WIN_RANGE(sDmapWindowLeftX, sDmapWindowRightX);
        REG_WIN0V = WIN_RANGE(0, 160);
        break;

    case INIT_MMAP_1: case FIN_MMAP: case MMAP: case FOUT_MMAP:
    case INIT_MMAP_2: case INIT_MMAP_3: case INIT_MMAP_4: case MMAP_WAIT:
        // MMAP: BG scroll update
        REG_BG0HOFS = usBg0Hofs; REG_BG0VOFS = usBg0Vofs;
        REG_BG1HOFS = usBg1Hofs; REG_BG1VOFS = usBg1Vofs;
        REG_BG2HOFS = usBg2Hofs; REG_BG2VOFS = usBg2Vofs;
        REG_BG3HOFS = usBg3Hofs; REG_BG3VOFS = usBg3Vofs;
        break;

    default:
        // Other states: generic scroll update + blend
        REG_BG0HOFS = usBg0Hofs; REG_BG0VOFS = usBg0Vofs;
        REG_BG1HOFS = usBg1Hofs; REG_BG1VOFS = usBg1Vofs;
        REG_BG2HOFS = usBg2Hofs; REG_BG2VOFS = usBg2Vofs;
        REG_BG3HOFS = usBg3Hofs; REG_BG3VOFS = usBg3Vofs;
        break;
    }

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
// Matches IDA GameSelect at 0x807c1dc.
// On first call each frame: sets VBlank handler, resets OBJ count,
// then dispatches via switch on sGameSeq.
//
// Each sub-mode advances sGameSeq as it transitions through states.
//
void GameSelect(void)
{
    SelectVblkSet();

    switch (sGameSeq)
    {
    // ================================================================
    //  DMAP: World Map (dungeon map)
    // ================================================================
    case INIT_DMAP_1:   // 0: First entry from title/intro
        SelectDmapInit();
        sGameSeq = FIN_DMAP;
        break;

    case FIN_DMAP:      // 1: Fade-in to world map
        if (SelectFadeIn(4))
            sGameSeq = DMAP;
        SelectDmapOamCreate();  // Render OBJs during fade-in too
        ucCntObj = 1;
        break;

    case DMAP:          // 2: Main world map loop
        if (GameSelectDmap())
            sGameSeq = FOUT_DMAP;
        SelectDmapOamCreate();
        ucCntObj = 1;
        break;

    case FOUT_DMAP:     // 3: Fade-out from world map
        if (SelectFadeOut(4))
            sGameSeq = INIT_MMAP_1;
        SelectDmapOamCreate();
        ucCntObj = 1;
        break;

    case INIT_DMAP_2:   // 4: Entry from MMAP returning to DMAP
        SelectDmapInit();
        // Restore previous world position
        sGameSeq = FIN_DMAP;
        break;

    case INIT_DMAP_3:   // 5: Entry from file select (pyramid intro done)
        // Start demo: Wario slides into tutorial world
        SelectDmapInit3();
        break;

    case INIT_DMAP_4:   // 6: Entry for start demo (opening cutscene)
        SelectDmapInit4();
        break;

    case INIT_DMAP_5:   // 7: Entry for tutorial/boss clear demo
        SelectDmapInit5();
        break;

    // ================================================================
    //  SMAP: Stage Map (world overview)
    //  Basic stub: fade-in, wait, fade-out, transition to MMAP.
    //  Full SMAP screen with stage paths will be implemented later.
    // ================================================================
    case FIN_SMAP:      // 8: Fade-in
        if (SelectFadeIn(4))
            sGameSeq = SMAP;
        break;

    case SMAP:          // 9: Main -- brief wait then transition out
        if (SelectFadeWait(30))
            sGameSeq = FOUT_SMAP;
        break;

    case FOUT_SMAP:     // 10: Fade-out to MMAP
        if (SelectFadeOut(4))
            sGameSeq = INIT_MMAP_1;  // Go to mini map for stage selection
        break;

    case INIT_SMAP_2:   // 11: Re-entry variant -- init then fade in
    case INIT_SMAP_3:   // 12: From file select
    case INIT_SMAP_4:   // 13: Demo entry
    case INIT_SMAP_5:   // 14: After stage clear / from SROOM
        sGameSeq = FIN_SMAP;
        break;

    // ================================================================
    //  DORA: Door/CD Transition Animation
    //  Basic stub: fade-in, wait, fade-out, then go to gameplay.
    //  Full door animation with CD will be implemented later.
    // ================================================================
    case INIT_DORA:     // 15: Init door animation
        sGameSeq = FIN_DORA;
        break;

    case FIN_DORA:      // 16: Fade-in door
        if (SelectFadeIn(4))
            sGameSeq = DORA;
        break;

    case DORA:          // 17: Main door -- wait then fade out
        if (SelectFadeWait(30))
            sGameSeq = FOUT_DORA;
        break;

    case SCORE_TALLY:        // 18: Score tally -- skip for now
        sGameSeq = FOUT_DORA;
        break;

    case FOUT_DORA:     // 19: Fade-out -- start gameplay
        if (SelectFadeOut(4))
        {
            sMainSeq = MS_MAIN;
            sGameSeq = 0;
        }
        break;

    case INIT_DORA_2:   // 20: Door variant from stage end type 2
        sGameSeq = FIN_DORA;
        break;

    // ================================================================
    //  SELOUT: Select-Out Transition
    //  Basic stub: fade-in, wait, fade-out to DMAP.
    // ================================================================
    case INIT_SELOUT:   // 21
        sGameSeq = FIN_SELOUT;
        break;

    case FIN_SELOUT:    // 22: Fade-in
        if (SelectFadeIn(4))
            sGameSeq = SELOUT;
        break;

    case SELOUT:        // 23: Main -- brief display then out
        if (SelectFadeWait(30))
            sGameSeq = FOUT_SELOUT;
        break;

    case FOUT_SELOUT:   // 24: Fade-out to DMAP
        if (SelectFadeOut(4))
            sGameSeq = INIT_DMAP_1;
        break;

    // ================================================================
    //  SELDEMO: Select Demo
    //  Basic stub: fade-in, wait, fade-out to DMAP.
    // ================================================================
    case INIT_SELDEMO:  // 25
        sGameSeq = FIN_SELDEMO;
        break;

    case FIN_SELDEMO:   // 26: Fade-in
        if (SelectFadeIn(4))
            sGameSeq = SELDEMO;
        break;

    case SELDEMO:       // 27: Main -- brief display then out
        if (SelectFadeWait(30))
            sGameSeq = FOUT_SELDEMO;
        break;

    case FOUT_SELDEMO:  // 28: Fade-out to DMAP
        if (SelectFadeOut(4))
            sGameSeq = INIT_DMAP_1;
        break;

    // ================================================================
    //  SELPOSE: Character Posing (shows stage name before entering)
    // ================================================================
    case INIT_SELPOSE:  // 29: Init posing screen
        // KIME init -- set up stage name display, Wario pose
        sGameSeq = FIN_SELPOSE;
        break;

    case FIN_SELPOSE:   // 30: Fade-in posing
        if (SelectFadeIn(4))
            sGameSeq = SELPOSE;
        break;

    case SELPOSE:       // 31: Main posing loop -- wait for anim or button
        // TODO: Full posing animation with stage name
        // For now: auto-advance after short delay
        if (SelectFadeWait(30))
            sGameSeq = FOUT_SELPOSE;
        break;

    case FOUT_SELPOSE:  // 32: Fade-out posing
        if (SelectFadeOut(4))
        {
            // Transition to main game
            sMainSeq = MS_MAIN;
            sGameSeq = 0;
        }
        break;

    // ================================================================
    //  MMAP: Mini Map (stage select within a world)
    // ================================================================
    case INIT_MMAP_1:   // 33: Init mini map (first entry from DMAP)
        SelectMmapInit();
        sGameSeq = FIN_MMAP;
        break;

    case FIN_MMAP:      // 34: Fade-in mini map
        if (SelectFadeIn(4))
            sGameSeq = MMAP;
        break;

    case MMAP:          // 35: Main mini map loop
        GameSelectMmap();
        SelectMmapOamCreate();
        ucCntObj = 1;
        break;

    case FOUT_MMAP:     // 36: Fade-out mini map
        if (SelectFadeOut(4))
            sGameSeq = INIT_DMAP_2;  // Back to DMAP
        SelectMmapOamCreate();
        ucCntObj = 1;
        break;

    case INIT_MMAP_2:   // 37: Mini map entry variant 2
        SelectMmapInit();
        sGameSeq = FIN_MMAP;
        break;

    case INIT_MMAP_3:   // 38: From file select (continue game, ucStageNum != 6)
        SelectMmapInit();
        sGameSeq = FIN_MMAP;
        break;

    case INIT_MMAP_4:   // 39: From mini game
        SelectMmapInit();
        sGameSeq = FIN_MMAP;
        break;

    case MMAP_WAIT:     // 40: Mini map wait state
        if (SelectFadeWait(20))
            sGameSeq = FOUT_MMAP;
        SelectMmapOamCreate();
        ucCntObj = 1;
        break;

    // ================================================================
    //  BOSS_DOOR: Boss Door Screen
    //  Basic stub: fade-in, wait, fade-out, start gameplay.
    // ================================================================
    case INIT_BOSS_DOOR:    // 41: Init boss door
        sGameSeq = FIN_BOSS_DOOR;
        break;

    case FIN_BOSS_DOOR:     // 42: Fade-in
        if (SelectFadeIn(4))
            sGameSeq = BOSS_DOOR;
        break;

    case BOSS_DOOR:         // 43: Main -- wait then fade out
        if (SelectFadeWait(30))
            sGameSeq = FOUT_BOSS_DOOR;
        break;

    case FOUT_BOSS_DOOR:    // 44: Fade-out -- start gameplay
        if (SelectFadeOut(4))
        {
            sMainSeq = MS_MAIN;
            sGameSeq = 0;
        }
        break;

    case INIT_BOSS_DOOR_2:  // 45: Boss door variant
        sGameSeq = FIN_BOSS_DOOR;
        break;

    // ================================================================
    //  SROOM: Save Room (after stage complete)
    //  Basic stub: init -> fade-in -> save -> fade-out -> back to world.
    // ================================================================
    case INIT_SROOM:    // 46: Init save room -- switch to game Save VBlank
        sGameSeq = FIN_SROOM;
        break;

    case FIN_SROOM:     // 47: Fade-in -- show save prompt
        if (SelectFadeIn(4))
            sGameSeq = SROOM;
        break;

    case SROOM:         // 48: Save and transition
        // Write save data to cartridge SRAM
        Save_WriteAuto(ucSaveNum);
        ucSaveFlg = 1;  // Mark as saved
        sGameSeq = FOUT_SROOM;
        break;

    case FOUT_SROOM:    // 49: Fade-out -- back to stage map
        if (SelectFadeOut(4))
            sGameSeq = INIT_SMAP_5;  // Re-enter SMAP after save
        break;

    default:
        // sGameSeq > 0x31 (49): invalid state, reset to DMAP
        sGameSeq = INIT_DMAP_1;
        break;
    }
}

// ---- Fade helpers ----

// Fade IN: decreases BLDY to 0 (dark -> visible)
// Returns 1 when fully visible.
int SelectFadeIn(int interval)
{
    if ((usFadeTimer & interval) == interval)
    {
        if (usBgEvy > 0)
            usBgEvy--;
        REG_BLDY = usBgEvy;
    }
    return (usBgEvy == 0);
}

// Fade OUT: increases BLDY to 16 (visible -> dark)
// Returns 1 when fully dark.
int SelectFadeOut(int interval)
{
    if ((usFadeTimer & interval) == interval)
    {
        if (usBgEvy < 16)
            usBgEvy++;
        REG_BLDY = usBgEvy;
    }
    return (usBgEvy == 16);
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

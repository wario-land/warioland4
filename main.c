#include "crt0.h"
#include "gba/gba.h"
#include "main.h"
#include "gameutils.h"
#include "00title/title.h"
#include "01select/select.h"
#include "02game/game.h"
#include "04pause/pause.h"
#include "06mini/mini.h"
#include "07shop/shop.h"
#include "08demo/demo.h"
#include "09ready/ready.h"
#include "10delete/delete.h"
#include "save/save.h"
#include "12ending/ending.h"

static void VBlankIntr(void);
static void HBlankIntr(void);
static void VCountIntr(void);
static void IntrDummy(void);
static void SetIntrFunc(void);
static void DefaultVblkFunc(void);

const IntrFunc gInterruptVectorTable[] =
{
    VBlankIntr,  // 0
    HBlankIntr,  // 1
    VCountIntr,  // 2
    IntrDummy,   // 3
    IntrDummy,   // 4
    IntrDummy,   // 5
    IntrDummy,   // 6
    IntrDummy,   // 7
    IntrDummy,   // 8
    IntrDummy,   // 9
    IntrDummy,   // 10
    IntrDummy,   // 11
    IntrDummy,   // 12
};

IWRAM_DATA struct InterruptHandler gInterruptHandler = {0};

// ---- Key Input ----
void KeyRead()
{
    usTrg = ~REG_KEYINPUT & 0x3FF & ~usCont2;
    usCont = ~REG_KEYINPUT & 0x3FF;
    usCont2 = ~REG_KEYINPUT & 0x3FF;
}

void ResetKey()
{
    if (sMainSeq != MS_RESET && !ucResetStop && (usCont & 0xF) == 0xF)
        sMainSeq = MS_RESET;
}

// ---- KeyReset: Soft reset triggered by A+B+Select+Start ----
// Matches IDA KeyReset at 0x80010ac.
// Reinitializes most of the system (IWRAM, interrupts, save data, sound)
// without going through the full cold-boot path (cartridge init, WAITCNT).
// Returns the game to the title screen.
void KeyReset(void)
{
    // Stop all sound and disable sound VSync
    // SoundVSyncOff_rev01();       // TODO: sound
    // m4aMPlayAllStop();           // TODO: sound

    // Disable VCount and HBlank interrupts
    REG_DISPSTAT &= ~DISPSTAT_VCOUNT_INTR;
    REG_IE &= ~INTR_FLAG_HBLANK;

    // Force darken blend on all layers, display off
    REG_BLDCNT = 0xFF;
    REG_DISPCNT = 0;
    REG_MOSAIC = 0x10;

    // Disable all interrupts during reinit
    REG_IME = 0;

    // Clear IWRAM (same as AllReset: 0x3000000, 0x1F80 32-bit words)
    {
        volatile u32 v = 0;
        REG_DMA3SAD = (u32)&v;
        REG_DMA3DAD = (u32)IWRAM_START;
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x1F80);
    }

    // Reinit interrupt handler code in IWRAM
    SetIntrFunc();
    SetVblkFunc(DefaultVblkFunc);

    // Reinit sound driver
    // m4aSoundInit();              // TODO: sound
    // SoundMode_rev01(0x900000);   // TODO: sound

    REG_IME = 1;

    // SoundVSyncOff_rev01();       // TODO: sound

    // Reinit save data from SRAM
    GroundSave_INIT_Read();
    GameReady_INIT_Read();

    // Reset to title screen
    sMainSeq = MS_TITLE;
    sGameSeq = 0;
    usCont = 0;
    usCont2 = 0;
    usTrg = 0;

    // SoundVSyncOn_rev01();        // TODO: sound
}

// ---- Hardware Initialization ----

// ---- Default VBlank handler (empty, matches IDA fnc at 0x800102c) ----
// Screens replace this with their own handler via SetVblkFunc().
static void DefaultVblkFunc(void)
{
}

// ---- SetIntrFunc: Copy interrupt handler code from ROM to IWRAM ----
// Matches IDA SetIntrFunc at 0x8000258.
// GBA interrupt handlers must run from IWRAM because ROM bus timing
// is incompatible with interrupt latency requirements.
// DMA-copies intr_main (ROM) -> IntrMainBuf (IWRAM), then writes
// IntrMainBuf address to 0x3007FFC for the BIOS interrupt dispatcher.
//
// NOTE: Our interrupt system uses a different approach (direct handler
// table via gInterruptVectorTable). The DMA copy of intr_main is skipped
// for now since we don't use the BIOS interrupt dispatcher path.
// SetIntrFunc -- Stub matching IDA at 0x8000258.
// The original copies intr_main code from ROM to IntrMainBuf via DMA,
// then writes IntrMainBuf address to 0x3007FFC for the BIOS dispatcher.
//
// Our interrupt system uses crt0.s Sub_IRQ_Handler -> gInterruptVectorTable
// instead of the BIOS dispatch path. crt0.s already writes Sub_IRQ_Handler
// to 0x3007FFC at boot. We must NOT overwrite it with an empty buffer.
static void SetIntrFunc(void)
{
    // Intentionally empty -- crt0.s handles interrupt vector setup.
}

void AllReset(void)
{
    REG_DISPCNT = DISPCNT_FORCED_BLANK;
    REG_DISPSTAT = 0;
    REG_IME = 0;

    // Clear IWRAM using DMA3 (matches original game value 0x85001F80)
    {
        volatile u32 clearVal = 0;
        REG_DMA3SAD = (u32)&clearVal;
        REG_DMA3DAD = (u32)IWRAM_START;
        REG_DMA3CNT = ((DMA_ENABLE | DMA_START_NOW | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x1F80);
    }

    ClearGraphicRam();

    // Copy interrupt handler code from ROM to IWRAM (matches IDA SetIntrFunc at 0x8000258)
    SetIntrFunc();

    // Set default (empty) VBlank handler before screens take over (matches IDA fnc at 0x800102c)
    SetVblkFunc(DefaultVblkFunc);

    // Boot-time save initialization (matches IDA AllReset order)
    GroundSave_INIT_Read();
    GameReady_INIT_Read();

    // Sound driver init (TODO: implement sound)
    // m4aSoundInit();
    // SoundMode_rev01(0x900000);

    REG_IME = 1;
    REG_IE = INTR_FLAG_VBLANK;
    REG_DISPSTAT = DISPSTAT_VBLANK_INTR;
    REG_WAITCNT = 0x45B4;

    usCont = 0;
    usCont2 = 0;
    usTrg = 0;
    sGameSeq = 0;
    KeyRead();

    if (usTrg == (L_BUTTON | R_BUTTON))
        sMainSeq = MS_DELETE;
    else
        sMainSeq = MS_TITLE;

    usCont = 0;
    usCont2 = 0;
    usTrg = 0;
    ucResetStop = 0;

    // Exit forced blank so display is visible
    REG_DISPCNT = DISPCNT_MODE_0;
}

// ---- Main Game Loop ----
// Matches IDA AgbMain at 0x80008e0
void AgbMain(void)
{
    int currentState;
    int demoResult;

    AllReset();

    for (;;)
    {
        cVblkFlg = 0;

        // Sound driver update each frame
        if (sMainSeq)
            ; // m4aSoundMain();  // TODO: sound

        if (ucAllReset)
            break;

        KeyRead();
        ResetKey();
        ++ucMainTimer;
        ++usRandomCount;

        switch (sMainSeq)
        {
        // ============================================================
        //  MS_TITLE (0): Title screen
        //  Hold B+L (0x22), press A (0x01) -> debug stage select (MS_TITLE2)
        // ============================================================
        case MS_TITLE:
            if ((usCont & 0x22) == 0x22 && (usTrg & 0x01) != 0)
            {
                sMainSeq = MS_TITLE2;
                sGameSeq = 0;
                cGmStartFlg = 0;
                break;
            }

            currentState = GameTitle();
            switch (currentState)
            {
                case 1:  // TITLE_RESULT_START -> File Select
                    sMainSeq = MS_FILESELECT;
                    sGameSeq = 0;
                    break;
                case 2:  // TITLE_RESULT_END -> Select (skip ready, go to map)
                    sMainSeq = MS_SELECT;
                    sGameSeq = 5;
                    break;
                case 3:  // TITLE_RESULT_ESCAPE -> Credits
                    sMainSeq = MS_CREDITS;
                    sGameSeq = 0;
                    break;
                case 4:  // TITLE_RESULT_ENDING -> Back to title
                    sMainSeq = MS_TITLE;
                    sGameSeq = -1;
                    break;
                default:
                    if (ucAutoDemoNum & 0x80)
                    {
                        sMainSeq = MS_DEMO;
                        sGameSeq = 0;
                    }
                    break;
            }
            break;

        // ============================================================
        //  MS_SELECT (1) + MS_MAIN (2): Select screen / Main game
        //  Original: MS_SELECT falls through to MS_MAIN only when GameSelect
        //  returns non-zero (select is done). We check sMainSeq instead
        //  (GameSelect sets sMainSeq=MS_MAIN internally when done).
        // ============================================================
        case MS_SELECT:
            GameSelect();
            if (sMainSeq != MS_MAIN)
                break;  // Still in select screen, skip MS_MAIN
            // Fall through to MS_MAIN logic below only when select is done

        case MS_MAIN:
            if (!GameMain())
                break;

            // GameMain returned non-zero: game state transition needed
            if (cPauseFlag)
            {
                sMainSeq = MS_PAUSE;
            }
            else if (cShopFlag)
            {
                sMainSeq = MS_SHOP;
            }
            else if (ucTitle2f)
            {
                sMainSeq = MS_TITLE2;
            }
            else
            {
                // Stage end routing based on ucSTEndType
                sMainSeq = MS_SELECT;
                switch (ucSTEndType)
                {
                case 0:
                case 1:
                    sGameSeq = 21;    // INIT_SELOUT (select-out transition)
                    break;
                case 2:
                    sGameSeq = 20;    // INIT_DORA_2 (back to door/posing)
                    break;
                case 3:
                case 4:
                    sGameSeq = 14;    // INIT_SMAP_5 (stage map after room)
                    break;
                case 5:
                    if (!ucWorldNumBak)
                        sGameSeq = 29;     // INIT_SELPOSE (special end)
                    else if (ucWorldNumBak == 5)
                    {
                        sMainSeq = MS_TITLE;
                        sGameSeq = -3;     // escape sequence
                    }
                    else
                        sGameSeq = 25;     // INIT_SELDEMO (boss door)
                    break;
                case 6:
                    sGameSeq = 38;    // INIT_MMAP_3 (mini game end)
                    break;
                default:
                    break;
                }
            }
            break;

        // ============================================================
        //  MS_RESET (3): Soft reset -- wait for keys released
        // ============================================================
        case MS_RESET:
            KeyReset();
            break;

        // ============================================================
        //  MS_PAUSE (4): Pause menu
        // ============================================================
        case MS_PAUSE:
            if (!GamePause())
                break;

            if (cPauseFlag == 2)  // Return to game
            {
                if (ucTitle2f)
                {
                    sMainSeq = MS_TITLE2;
                }
                else
                {
                    ucGateID = 0;
                    sMainSeq = MS_SELECT;
                    sGameSeq = 21;   // door transition
                }
            }
            else if (cPauseFlag == 3)  // Retire
            {
                cPauseFlag = 0;
                sMainSeq = MS_TITLE;
            }
            else if (cPauseFlag == 1)  // Continue
            {
                sMainSeq = MS_MAIN;
            }
            break;

        // ============================================================
        //  MS_SAVEINTERRUPT (5): Interrupted save write
        // ============================================================
        case MS_SAVEINTERRUPT:
            // GameWriteInterrupted();  // TODO: implement save interrupt handler
            sMainSeq = MS_PAUSE;
            break;

        // ============================================================
        //  MS_MINI (6): Mini-game
        // ============================================================
        case MS_MINI:
            if (!GameMini())
                break;

            if (ucTitle2f)
            {
                sMainSeq = MS_TITLE2;
                sGameSeq = 0;
            }
            else
            {
                sMainSeq = MS_SELECT;
                sGameSeq = 39;   // mini game result
            }
            break;

        // ============================================================
        //  MS_SHOP (7): Shop
        // ============================================================
        case MS_SHOP:
            if (!GameShop())
                break;
            sMainSeq = MS_MAIN;
            break;

        // ============================================================
        //  MS_DEMO (8): Auto-demo
        // ============================================================
        case MS_DEMO:
            if (!GameMain() || !cPauseFlag)
                break;

            demoResult = 0;  // AutoDemo_EndRegulation()
            if (!demoResult)
                sGameSeq = -1;
            sMainSeq = MS_TITLE;
            break;

        // ============================================================
        //  MS_FILESELECT (9): File Select / Ready screen
        //  Hold B+L, press A -> debug stage select
        // ============================================================
        case MS_FILESELECT:
            if ((usCont & 0x22) == 0x22 && (usTrg & 0x01) != 0)
            {
                sGameSeq = 0;
                sMainSeq = MS_TITLE2;
            }
            else
            {
                if (GameReady())
                {
                    if (cNextFlg == 1)
                    {
                        if (ucDisConFlg)
                        {
                            sGameSeq = 0;
                            sMainSeq = MS_MAIN;
                        }
                        else
                        {
                            sMainSeq = MS_SELECT;
                            if (ucSaveFlg)
                            {
                                if (ucStageNum == 6)
                                    sGameSeq = 0;       // INIT_DMAP_1: on world map, go to DMAP
                                else
                                    sGameSeq = 38;      // INIT_MMAP_3: in a stage, go to mini map
                            }
                            else
                            {
                                sGameSeq = -2;          // TITLE_ENTRY_PYRAMID: new game -> pyramid intro
                                sMainSeq = MS_TITLE;
                            }
                        }
                    }
                    else
                    {
                        sGameSeq = 0;
                        sMainSeq = MS_TITLE;
                    }
                    cNextFlg = 0;
                }
            }
            cGmStartFlg = 0;
            break;

        // ============================================================
        //  MS_DELETE (10): Delete save data
        // ============================================================
        case MS_DELETE:
            if (!GameDelete())
                break;

            sGameSeq = 0;
            if (cNextFlg == 1)
            {
                ucAllReset = 2;  // full reset after delete
            }
            else
            {
                sMainSeq = MS_TITLE;
            }
            break;

        // ============================================================
        //  MS_TITLE2 (11): Debug stage select / title 2
        // ============================================================
        case MS_TITLE2:
            if (!GameTitle2())
                break;

            switch (cNextFlg)
            {
            case 1:  // Start game from debug select
                sMainSeq = MS_MAIN;
                if (cSelectedNo == 6 || cSelectedNo == 12 ||
                    cSelectedNo == 18 || cSelectedNo == 24)
                {
                    sGameSeq = 0;
                    sMainSeq = MS_MINI;   // mini game
                }
                break;
            case 2:  // Demo
                sMainSeq = MS_DEMO;
                sGameSeq = 0;
                break;
            case 3:  // Credits
                sMainSeq = MS_CREDITS;
                sGameSeq = 0;
                break;
            case 4:  // Escape sequence
                sMainSeq = MS_TITLE;
                sGameSeq = -3;
                break;
            case 5:  // Ending
                sMainSeq = MS_TITLE;
                sGameSeq = -4;
                break;
            default:
                sMainSeq = MS_TITLE;
                if (cNextFlg == 5)
                    sGameSeq = -4;
                else
                    usBgEvy = 0;
                break;
            }
            cNextFlg = 0;
            break;

        // ============================================================
        //  MS_CREDITS (12): Ending / credits
        // ============================================================
        case MS_CREDITS:
            if (!GameEnding1())
                break;
            sMainSeq = MS_TITLE;
            sGameSeq = -4;
            break;

        default:
            break;
        }

        // Sound driver update after VBlank
        // if (cVblkFlg && sMainSeq) m4aSoundMain();

        // Wait for next VBlank
        usIntrCheck &= ~1;
        do {
            asm volatile("SVC #2");
        } while ((usIntrCheck & 1) == 0);
    }

    // Cleanup after main loop exit (full reset, matches IDA AgbMain epilogue)
    // SoundVSyncOff_rev01();  // TODO: implement sound
    if (ucAllReset == 2)
        CartridgeSram_AllClear();
}

// ---- Interrupt Handlers ----
static void VBlankIntr(void)
{
    if (sVblkFunc)
        sVblkFunc();

    if (gInterruptHandler.gVBlankInterruptHandlerFunc)
        gInterruptHandler.gVBlankInterruptHandlerFunc();

    REG_IF |= INTR_FLAG_VBLANK;
    usIntrCheck |= INTR_FLAG_VBLANK;
}

static void HBlankIntr(void)
{
    if (gInterruptHandler.gHBlankInterruptHandlerFunc)
        gInterruptHandler.gHBlankInterruptHandlerFunc();

    REG_IF |= INTR_FLAG_HBLANK;
}

static void VCountIntr(void)
{
    if (gInterruptHandler.gVCountInterruptHandlerFunc)
        gInterruptHandler.gVCountInterruptHandlerFunc();

    REG_IF |= INTR_FLAG_VCOUNT;
}

static void IntrDummy(void)
{
    // No operation
}

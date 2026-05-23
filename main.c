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
#include "12ending/ending.h"

static void VBlankIntr(void);
static void HBlankIntr(void);
static void VCountIntr(void);
static void IntrDummy(void);

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

void KeyReset(void)
{
    // Wait for all keys released, then return to title
    // Called from MS_RESET handler
}

// ---- Hardware Initialization ----

static void ClearGraphicRam(void)
{
    // Clear palette RAM using DMA
    volatile u32 z = 0;
    REG_DMA3SAD = (u32)&z;
    REG_DMA3DAD = (u32)PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (PLTT_SIZE >> 2);
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

    REG_IME = 1;
    REG_IE = INTR_FLAG_VBLANK | INTR_FLAG_GAMEPAK;
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
        //  Hold B+L (0x22), press A (0x01) → debug stage select (MS_TITLE2)
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
                case 1:  // TITLE_RESULT_START → File Select
                    sMainSeq = MS_FILESELECT;
                    sGameSeq = 0;
                    break;
                case 2:  // TITLE_RESULT_END → Select (skip ready, go to map)
                    sMainSeq = MS_SELECT;
                    sGameSeq = 5;
                    break;
                case 3:  // TITLE_RESULT_ESCAPE → Credits
                    sMainSeq = MS_CREDITS;
                    sGameSeq = 0;
                    break;
                case 4:  // TITLE_RESULT_ENDING → Back to title
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
        //  Falls through: Select → check if game should be entered
        // ============================================================
        case MS_SELECT:
            GameSelect();
            // Fall through to MS_MAIN logic below

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
                    sGameSeq = 21;    // SEL_SEQ_DORA_INIT
                    break;
                case 2:
                    sGameSeq = 20;    // back to pose
                    break;
                case 3:
                case 4:
                    sGameSeq = 14;    // SEL_SEQ_ROOM_INIT (save room)
                    break;
                case 5:
                    if (!ucWorldNumBak)
                        sGameSeq = 29;     // special end
                    else if (ucWorldNumBak == 5)
                    {
                        sMainSeq = MS_TITLE;
                        sGameSeq = -3;     // escape sequence
                    }
                    else
                        sGameSeq = 25;     // boss door
                    break;
                case 6:
                    sGameSeq = 38;    // mini game end
                    break;
                default:
                    break;
                }
            }
            break;

        // ============================================================
        //  MS_RESET (3): Soft reset — wait for keys released
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
            // GameWriteTyudan();  // TODO: implement save interrupt handler
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
        //  Hold B+L, press A → debug stage select
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
                                    sGameSeq = 0;
                                else
                                    sGameSeq = 38;  // boss door
                            }
                            else
                            {
                                sGameSeq = -2;
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
                sGameSeq = 0;
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

    // Cleanup after main loop exit (full reset)
    // SoundVSyncOff_rev01();
    // if (ucAllReset == 2) Save_RamAllReset();
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

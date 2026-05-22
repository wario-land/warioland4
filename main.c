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
void AgbMain(void)
{
    int currentState;
    s16 pauseState;
    int demoResult;
    s16 miniGameResult;

    AllReset();

    for (;;)
    {
        cVblkFlg = 0;

        if (sMainSeq)
            ;
            // m4aSoundMain();  // TODO: sound

        if (ucAllReset)
            break;

        KeyRead();
        ResetKey();
        ++ucMainTimer;
        ++usRandomCount;

        switch (sMainSeq)
        {
            case MS_TITLE:
                if ((usCont & (A_BUTTON | L_BUTTON)) == (A_BUTTON | L_BUTTON) && (usTrg & R_BUTTON) != 0)
                {
                    sMainSeq = MS_TITLE2;
                    sGameSeq = 0;
                    cGmStartFlg = 0;
                    break;
                }

                currentState = GameTitle();
                switch (currentState)
                {
                    case TITLE_RESULT_START:
                        sMainSeq = MS_FILESELECT;
                        sGameSeq = 0;
                        break;
                    case TITLE_RESULT_END:
                        sMainSeq = MS_SELECT;
                        sGameSeq = 5;
                        break;
                    case TITLE_RESULT_ESCAPE:
                        sMainSeq = MS_CREDITS;
                        sGameSeq = 0;
                        break;
                    case TITLE_RESULT_ENDING:
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

            case MS_FILESELECT:
                GameReady();
                cGmStartFlg = 0;
                break;

            case MS_SELECT:
                GameSelect();
                // fall through
            case MS_MAIN:
                GameMain();
                break;

            case MS_PAUSE:
                GamePause();
                break;

            case MS_MINI:
                GameMini();
                break;

            case MS_SHOP:
                GameShop();
                break;

            case MS_DEMO:
                GameMain();
                break;

            case MS_DELETE:
                GameDelete();
                break;

            case MS_TITLE2:
                GameTitle2();
                sMainSeq = MS_TITLE;
                sGameSeq = -3;
                break;

            case MS_CREDITS:
                GameEnding1();
                sMainSeq = MS_TITLE;
                sGameSeq = -4;
                break;

            case MS_RESET:
                break;

            default:
                break;
        }

        // Wait for next VBlank
        usIntrCheck &= ~1;
        do {
            asm volatile("SVC #2");
        } while ((usIntrCheck & 1) == 0);
    }
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

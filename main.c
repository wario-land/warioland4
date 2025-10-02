#include "crt0.h"
#include "gba/gba.h"
#include "main.h"
#include "gameutils.h"

static void VBlankIntr(void);
static void HBlankIntr(void);
static void VCountIntr(void);
static void IntrDummy(void);

const IntrFunc gInterruptVectorTable[] =
{
    VBlankIntr, // 0
    HBlankIntr, // 1
    VCountIntr, // 2
    IntrDummy, // 3
    IntrDummy,
    IntrDummy,
    IntrDummy,
    IntrDummy, // 7
    IntrDummy,
    IntrDummy,
    IntrDummy,
    IntrDummy, // 11
    IntrDummy, // 12
};

#define Interrupt_COUNT ((int)(sizeof(gIntrTableTemplate)/sizeof(IntrFunc)))

IWRAM_DATA struct InterruptHandler gInterruptHandler = {0}; // 0x30019FC


void KeyRead()
{
    usTrg = ~REG_KEYINPUT & 0x3FF & ~usCont2;
    usCont = ~REG_KEYINPUT & 0x3FF;
    usCont2 = ~REG_KEYINPUT & 0x3FF;
}

void ResetKey()
{
    if ( sMainSeq != 3 && !ucResetStop && (usCont & 0xF) == 0xF ) sMainSeq = 3;
}

void AllReset(void)
{
    REG_DISPCNT = 0x80;
    REG_DISPSTAT = 0;
    REG_IME = 0;

    int v0 = 0;
    REG_DMA3SAD = &v0;
    REG_DMA3DAD = 0x3000000; // Clear wram
    REG_DMA3CNT = 0x85001F80;

    // TODO
    /*
    ClearGraphicRam();
    SetIntrFunc();
    SetVblkFunc(fnc);
    GroundSave_INIT_Read();
    GameReady_INIT_Read();
    m4aSoundInit();
    SoundMode_rev01(0x900000);
    */
    REG_IME = 1;
    REG_IE = 0x2001;
    REG_DISPSTAT = 8;
    REG_WAITCNT = 0x45B4;
    
    usCont = 0;
    usCont2 = 0;
    usTrg = 0;
    sGameSeq = 0;
    KeyRead();
    if ( usTrg == 0x300 )
        sMainSeq = 10;
    else
        sMainSeq = 0;
    usCont = 0;
    usCont2 = 0;
    usTrg = 0;
    ucResetStop = 0;

    // test stuff
    REG_DISPCNT = DISPCNT_MODE_3 | DISPCNT_BG2_ON; //mode 3 graphics, we aren't actually using bg2 right now
    REG_IME = 1; //enable interrupts
}

#define COL_YELLOW  0x03FF
#define COL_RED     0x001F
#define COL_GREEN   0x03E0
#define COL_BLUE    0x7C00
#define COL_WHITE   0x7FFF
void draw()
{    
    u16 *vid_mem = BG_VRAM;
    int y, x;
    if (usCont & 0x40)
    {
        for (y = 0; y < DISPLAY_HEIGHT; ++y)
        {
            for (x = 0; x < DISPLAY_WIDTH; ++x)
            {
                vid_mem[y * DISPLAY_WIDTH + x] = COL_YELLOW;
            }
        }
    }
    else if (usCont & 0x80)
    {
        for (y = 0; y < DISPLAY_HEIGHT; ++y)
        {
            for (x = 0; x < DISPLAY_WIDTH; ++x)
            {
                vid_mem[y * DISPLAY_WIDTH + x] = COL_RED;
            }
        }
    }
    else if (usCont & 0x20)
    {
        for (y = 0; y < DISPLAY_HEIGHT; ++y)
        {
            for (x = 0; x < DISPLAY_WIDTH; ++x)
            {
                vid_mem[y * DISPLAY_WIDTH + x] = COL_GREEN;
            }
        }
    }
    else if (usCont & 0x10)
    {
        for (y = 0; y < DISPLAY_HEIGHT; ++y)
        {
            for (x = 0; x < DISPLAY_WIDTH; ++x)
            {
                vid_mem[y * DISPLAY_WIDTH + x] = COL_BLUE;
            }
        }
    }
    else
    {
        for (y = 0; y < DISPLAY_HEIGHT; ++y)
        {
            for (x = 0; x < DISPLAY_WIDTH; ++x)
            {
                vid_mem[y * DISPLAY_WIDTH + x] = COL_WHITE;
            }
        }
    }
}

void AgbMain(void)
{
    AllReset();
    for (;;)
    {
        cVblkFlg = 0;
        // if ( sMainSeq ) m4aSoundMain();
        if ( ucAllReset ) break;
        KeyRead();
        ResetKey();
        ++ucMainTimer;
        ++usRandomCount;

        // TODO...

        // test stuff
        draw();
    }
}

static void VBlankIntr(void)
{
    if (gInterruptHandler.gVBlankInterruptHandlerFunc)
        gInterruptHandler.gVBlankInterruptHandlerFunc();

    REG_IF |= INTR_FLAG_VBLANK;
    // gusintrCheck |= INTR_FLAG_VBLANK;
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

static void IntrDummy(void) { /* do nothing */}

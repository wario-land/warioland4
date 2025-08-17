#include "crt0.h"
#include "gba/gba.h"
#include "main.h"

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

#define COL_YELLOW  0x03FF

void AllReset(void)
{
    REG_DISPCNT = DISPCNT_MODE_3 | DISPCNT_BG2_ON; //mode 3 graphics, we aren't actually using bg2 right now
    REG_IME = 1; //enable interrupts
}

void draw()
{    
    u16 *vid_mem = BG_VRAM;
    int y, x;
    for (y = 0; y < 50; ++y)
    {
        for (x = 0; x < 50; ++x)
        {
            vid_mem[(30 + y) * DISPLAY_WIDTH + 30 + x] = COL_YELLOW;
        }
    }
}

void AgbMain(void)
{
    AllReset();
    for (;;)
    {
        // cVblkFlg = 0;

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

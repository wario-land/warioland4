// Kiss scene -- Princess and Wario with rose background
//
// BG0: 512x256 -- princess sprite (varies by ending type)
// BG1: 256x256 -- wario sprite
// BG2: 256x256 -- rose background
// BG3: 256x256 -- background content
//
// Ending type determines which sprites are shown:
//   type 0: princess1 + wario_unhappy (bad ending)
//   type 1: princess2 + wario_unhappy
//   type 2: princess3 + wario_happy
//   type 3: princess4 + wario_happy (best ending)
//
// States (sLocalSeq):
//   KISS_0: Fade in from white, show scene, princess moves toward Wario
//   KISS_1: Kiss pose, message/OBJ animation
//   KISS_2: Wait for button or timeout
//   KISS_3: Fade to black, advance scene

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

// ---- External data (extracted from ROM via IDA) ----
extern const u16 kiss_bg_Palette[32];
extern const u8  kiss_bg_Char[];           // LZ77: rose BG tiles
extern const u8  kiss_bg[];                // LZ77: BG3 content
extern const u16 kiss_bg_rose[];           // UnPackScreen: rose tilemap
extern const u16 kiss_princess_Palette[16];
extern const u16 kiss_wario_Palette[16];
extern const u16 kiss_obj_Palette[16];
extern const u8  kiss_obj_Char[];          // LZ77: OBJ tiles

// Princess data (varies by ending type: 0=bad, 1-2=mid, 3=best)
extern const u8  kiss_princess1_Char[];    // LZ77: princess tiles type 1
extern const u16 kiss_princess1[];         // UnPackScreen: princess tilemap 1
extern const u8  kiss_princess2_Char[];    // LZ77: princess tiles type 2
extern const u16 kiss_princess2[];         // UnPackScreen: princess tilemap 2
extern const u8  kiss_princess3_Char[];    // LZ77: princess tiles type 3
extern const u16 kiss_princess3[];         // UnPackScreen: princess tilemap 3
extern const u8  kiss_princess4_Char[];    // LZ77: princess tiles type 4
extern const u16 kiss_princess4[];         // UnPackScreen: princess tilemap 4

// Wario data (happy = good ending, unhappy = bad ending)
extern const u8  kiss_wario_happy_Char[];  // LZ77: wario tiles (happy)
extern const u16 kiss_wario_happy[];       // UnPackScreen: wario tilemap (happy)
extern const u8  kiss_wario_unhappy_Char[];// LZ77: wario tiles (unhappy)
extern const u16 kiss_wario_unhappy[];     // UnPackScreen: wario tilemap (unhappy)

// ---- Global state ----
extern u16 sLocalSeq;
extern u32 uLocalTime;
extern u32 uLocalTime2;
extern s16 sWork0, sWork1, sWork2, sWork3, sWork4, sWork5;
extern u16 usEndingType;
extern u16 uEVA, uEVB, uEVY;
extern u16 *pObjEnd;
extern u32 uObjSize;

// VRAM layout constants matching IDA:
// BG0: screenbase 16 (0x8000) -- princess -- tiles at VRAM+0x780*32 (tile 0x780)
// BG1: screenbase 18 (0x9000) -- wario -- tiles at VRAM base (charbase 0)
// BG2: screenbase 19 (0x9800) -- rose
// BG3: screenbase 20 (0xA000) -- background
// BG tiles: princess at VRAM+0x780*32, wario at VRAM+0x0000
void Kiss_Init(void)
{
    // 1) Load Wario palette to BG palette row 0 (BG_PLTT + 0*16)
    // Matches IDA: kiss_wario_Palette -> BG_PLTT, control 0x80000010
    REG_DMA3SAD = (u32)kiss_wario_Palette;
    REG_DMA3DAD = (u32)(BG_PLTT + 0 * 16);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (16*2 >> 2);

    // 2) Load princess palette to BG palette row 1 (BG_PLTT + 1*16 = BG_PLTT + 0x20)
    // Matches IDA: DMA to MEMORY[0x5000020] = BG_PLTT + 32 bytes = row 1
    REG_DMA3SAD = (u32)kiss_princess_Palette;
    REG_DMA3DAD = (u32)(BG_PLTT + 1 * 16);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (16*2 >> 2);

    // 3) Fill screenbases 16-20 with blank tile 0x3FF (10240 bytes = 5 screenblocks)
    // Matches IDA: 0x3FF03FF -> 0x6008000, control 0x85000A00
    {
        volatile u32 p = 0x03FF03FF;
        REG_DMA3SAD = (u32)&p;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x8000);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x2800 >> 2);
    }

    // 4) Decompress princess/Wario tiles based on ending type
    // Princess tiles go to VRAM+0x780*32 (BG0 uses charbase 0, tiles from 0x780)
    // Wario tiles go to VRAM base (BG1 uses charbase 0)
    // Wario tilemap goes to screenbase 18 (0x9000)
    if (usEndingType == 0)
    {
        // Bad ending: princess1 + unhappy Wario
        LZ77UnCompVram((const u32 *)kiss_princess1_Char, (void *)((u8 *)BG_VRAM + 0x780 * 32));
        UnPackScreen((const u16 *)kiss_princess1, (vu16 *)((u8 *)BG_VRAM + 0x8000));
        LZ77UnCompVram((const u32 *)kiss_wario_unhappy_Char, (void *)BG_VRAM);
        UnPackScreen((const u16 *)kiss_wario_unhappy, (vu16 *)((u8 *)BG_VRAM + 0x9000));
    }
    else if (usEndingType == 1)
    {
        // Mid ending 1: princess2 + unhappy Wario
        LZ77UnCompVram((const u32 *)kiss_princess2_Char, (void *)((u8 *)BG_VRAM + 0x780 * 32));
        UnPackScreen((const u16 *)kiss_princess2, (vu16 *)((u8 *)BG_VRAM + 0x8000));
        LZ77UnCompVram((const u32 *)kiss_wario_unhappy_Char, (void *)BG_VRAM);
        UnPackScreen((const u16 *)kiss_wario_unhappy, (vu16 *)((u8 *)BG_VRAM + 0x9000));
    }
    else if (usEndingType == 2)
    {
        // Mid ending 2: princess3 + happy Wario
        LZ77UnCompVram((const u32 *)kiss_princess3_Char, (void *)((u8 *)BG_VRAM + 0x780 * 32));
        UnPackScreen((const u16 *)kiss_princess3, (vu16 *)((u8 *)BG_VRAM + 0x8000));
        LZ77UnCompVram((const u32 *)kiss_wario_happy_Char, (void *)BG_VRAM);
        UnPackScreen((const u16 *)kiss_wario_happy, (vu16 *)((u8 *)BG_VRAM + 0x9000));
    }
    else
    {
        // Best ending (type 3+): princess4 + happy Wario
        LZ77UnCompVram((const u32 *)kiss_princess4_Char, (void *)((u8 *)BG_VRAM + 0x780 * 32));
        UnPackScreen((const u16 *)kiss_princess4, (vu16 *)((u8 *)BG_VRAM + 0x8000));
        LZ77UnCompVram((const u32 *)kiss_wario_happy_Char, (void *)BG_VRAM);
        UnPackScreen((const u16 *)kiss_wario_happy, (vu16 *)((u8 *)BG_VRAM + 0x9000));
    }

    // 5) Load rose BG palette to BG palette row 14 (BG_PLTT + 14*16 = BG_PLTT + 0x1C0)
    // Matches IDA: DMA to MEMORY[0x50001C0] = BG_PLTT + 448 bytes = row 14
    REG_DMA3SAD = (u32)kiss_bg_Palette;
    REG_DMA3DAD = (u32)(BG_PLTT + 14 * 16);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (32*2 >> 2);

    // 4) Rose BG tiles -> VRAM+0x780*32 (same charblock as princess)
    LZ77UnCompVram((const u32 *)kiss_bg_Char, (void *)((u8 *)BG_VRAM + 0x780 * 32));

    // 5) Rose tilemap -> BG2 screenbase 19 (0x9800)
    UnPackScreen((const u16 *)kiss_bg_rose, (vu16 *)((u8 *)BG_VRAM + 0x9800));

    // 6) Decompress BG3 background content -> BG3 screenbase 20 (0xA000)
    LZ77UnCompVram((const u32 *)kiss_bg, (void *)((u8 *)BG_VRAM + 0xA000));

    // 7) Load OBJ palette (petals/sparkles)
    REG_DMA3SAD = (u32)kiss_obj_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (16*2 >> 2);

    // 8) Decompress OBJ tiles (petals/sparkles)
    LZ77UnCompVram((const u32 *)kiss_obj_Char, (void *)OBJ_VRAM0);

    // 9) BG control registers (matching IDA values)
    // BG0: 512x256, 16-color, priority 0, screenbase 16, charbase 0
    REG_BG0CNT = BGCNT_TXT512x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);
    // BG1: 256x256, 16-color, priority 1, screenbase 18, charbase 0
    REG_BG1CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(18) | BGCNT_CHARBASE(0);
    // BG2: 256x256, 16-color, priority 2, screenbase 19, charbase 0
    REG_BG2CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(2)
               | BGCNT_SCREENBASE(19) | BGCNT_CHARBASE(0);
    // BG3: 256x256, 16-color, priority 3, screenbase 20, charbase 0
    REG_BG3CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(3)
               | BGCNT_SCREENBASE(20) | BGCNT_CHARBASE(0);

    // 10) Initial positions (matching IDA Kiss_princess_move/Kiss_wario_move)
    // These set BG0/BG1 scroll positions for princess and Wario
    // sWork0,sWork1 = princess BG0 scroll; sWork2,sWork3 = wario BG1 scroll
    sWork0 = 48;   // princess BG0 x (left side)
    sWork1 = 72;   // princess BG0 y
    sWork2 = 168;  // wario BG1 x (right side)
    sWork3 = 72;   // wario BG1 y

    REG_BG0HOFS = sWork0;
    REG_BG0VOFS = sWork1;
    REG_BG1HOFS = sWork2;
    REG_BG2VOFS = sWork3;

    sWork4 = 0;       // animation counter
    sWork5 = 0;       // sub-state
    uLocalTime2 = 0;  // secondary timer

    // Start with white screen (lighten max), then fade to normal
    uEVA = 0;
    uEVB = 16;

    V_Wait();
    SetBLDUpMax(BLDCNT_TGT1_BD | BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG1
              | BLDCNT_TGT1_BG2 | BLDCNT_TGT1_BG3 | BLDCNT_TGT1_OBJ);
    REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_OBJ_ON
                | DISPCNT_BG0_ON | DISPCNT_BG1_ON
                | DISPCNT_BG2_ON | DISPCNT_BG3_ON;
}

void Kiss_Exec(int time)
{
    u16 *dst = (u16 *)OamBuf;

    switch (sLocalSeq)
    {
    case 0:
        // Fade in from white
        FadeDec(7);

        // Princess slides right toward Wario (BG0 scroll right)
        if (sWork0 < 104 && (time & 3) == 3)
        {
            sWork0++;
            REG_BG0HOFS = sWork0;
        }

        if (++uLocalTime > 90)
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 1:
        // Kiss pose: princess at destination, message/text appears
        // Wait for animation timing or button press
        if (++uLocalTime > 120)
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 2:
        // Message display: wait for button or auto-timeout
        if (++uLocalTime > 180 || (usTrg & (A_BUTTON | START_BUTTON)))
        {
            // Begin fade to black
            SetBLDDownMin(BLDCNT_TGT1_BD | BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG1
                        | BLDCNT_TGT1_BG2 | BLDCNT_TGT1_BG3 | BLDCNT_TGT1_OBJ);
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 3:
        // Fade to black, advance to next scene
        if (FadeInc(3))
            sGameSeq++;
        break;

    default:
        break;
    }

    // ---- OBJ rendering (rose petals on screen) ----
    // Petals float around as decorative OBJ overlay
    {
        static const u16 petal[] = { 1, 0x4080, 0x8070, 0x0600 };
        dst = SetObj(petal, dst, 60, 80);
        dst = SetObj(petal, dst, 180, 40);
    }

    EndObj(dst);
}

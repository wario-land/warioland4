// Super Hard mode message -- shown after perfect ending
//
// Displayed only when all 12 treasures + hidden items collected.
// Message varies by ucEndingMessageKind:
//   0: Normal message (bosses returned, get 12 treasures)
//   1: All-get message (bosses returned)
//   2: S-Hard message (you can now play Super Hard mode)
//
// BG0: screenbase 16 -- message text overlay
// BG1: screenbase 17 -- message board background
// BG2: screenbase 18 -- super_hard_bg

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

// ---- Data references (extracted from ROM via IDA) ----
extern const u8  super_hard_Char[];              // LZ77 BG tiles
extern const u16 super_hard_bg[];                 // BG2 tilemap
extern const u16 super_hard_board[];              // Board tilemap (normal)
extern const u16 super_hard_board_00[];           // Board tilemap (message kind 1)
extern const u16 super_hard_board_E12[];          // Board tilemap (message kind 2, English)
extern const u16 super_hard_board_J12[];          // Board tilemap (message kind 2, Japanese)
extern const u16 super_hard_E[];                  // English message text (normal)
extern const u16 super_hard_J[];                  // Japanese message text (normal)
extern const u16 super_hard_E00[];                // English message text (kind 1)
extern const u16 super_hard_J00[];                // Japanese message text (kind 1)
extern const u16 super_hard_E12[];                // English message text (kind 2)
extern const u16 super_hard_J12[];                // Japanese message text (kind 2)
extern const u16 newspaper_END_obj_Palette[];     // OBJ palette (reuse from news_end)
extern const u8  newspaper_END_obj_Char[];        // OBJ tiles (reuse from news_end)

// ---- Global state ----
extern u16 sLocalSeq;
extern u32 uLocalTime;
extern u16 uEVY;
extern s16 sWork0;       // OBJ animation completion flag
extern u8  bMsgJapanese;
extern u8  ucEndingMessageKind;
extern u8  ucPerfect;

void SuperHard_Init(void)
{
    // Load OBJ palette (64 entries = 128 bytes, matching IDA DMA: 0x80000040)
    REG_DMA3SAD = (u32)newspaper_END_obj_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (64 * 2 >> 2);

    // Decompress BG tiles to VRAM base (charblock 0)
    LZ77UnCompVram((const u32 *)super_hard_Char, (void *)BG_VRAM);

    // Decompress OBJ tiles to OBJ_VRAM0
    LZ77UnCompVram((const u32 *)newspaper_END_obj_Char, (void *)OBJ_VRAM0);

    // BG2: super_hard_bg background -> screenbase 18 (0x9000)
    // Matching IDA: UnPackScreen(super_hard_bg, 0x6009000)
    UnPackScreen((const u16 *)super_hard_bg, (vu16 *)((u8 *)BG_VRAM + 0x9000));

    // BG1: message board -> screenbase 17 (0x8800)
    // BG0: message text -> screenbase 16 (0x8000)
    // Matching IDA: content varies by ucEndingMessageKind + language
    if (ucEndingMessageKind == 1)
    {
        // kind 1: All-get message
        UnPackScreen((const u16 *)super_hard_board_00, (vu16 *)((u8 *)BG_VRAM + 0x8800));
        UnPackScreen(bMsgJapanese ? (const u16 *)super_hard_J00
                                  : (const u16 *)super_hard_E00,
                     (vu16 *)((u8 *)BG_VRAM + 0x8000));
    }
    else if (ucEndingMessageKind == 2)
    {
        // kind 2: S-Hard mode unlock message
        UnPackScreen(bMsgJapanese ? (const u16 *)super_hard_board_J12
                                  : (const u16 *)super_hard_board_E12,
                     (vu16 *)((u8 *)BG_VRAM + 0x8800));
        UnPackScreen(bMsgJapanese ? (const u16 *)super_hard_J12
                                  : (const u16 *)super_hard_E12,
                     (vu16 *)((u8 *)BG_VRAM + 0x8000));
    }
    else
    {
        // kind 0: Normal message (bosses returned)
        UnPackScreen((const u16 *)super_hard_board, (vu16 *)((u8 *)BG_VRAM + 0x8800));
        UnPackScreen(bMsgJapanese ? (const u16 *)super_hard_J
                                  : (const u16 *)super_hard_E,
                     (vu16 *)((u8 *)BG_VRAM + 0x8000));
    }

    // BG control registers (matching IDA: 0x1000, 0x1101, 0x1202)
    // BG0: screenbase 16, priority 0
    REG_BG0CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);
    // BG1: screenbase 17, priority 1
    REG_BG1CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(17) | BGCNT_CHARBASE(0);
    // BG2: screenbase 18, priority 2
    REG_BG2CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(2)
               | BGCNT_SCREENBASE(18) | BGCNT_CHARBASE(0);

    sWork0 = 0;  // animation done flag
    V_Wait();
    SetBLDDownMax(BLDCNT_TGT1_BD | BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG1
                | BLDCNT_TGT1_BG2 | BLDCNT_TGT1_OBJ);
    // DISPCNT = 0x1700 = MODE_0 | OBJ_ON | BG0_ON | BG1_ON | BG2_ON
    REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_OBJ_ON
                | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_BG2_ON;
}

void SuperHard_Exec(int time)
{
    u16 *dst;

    switch (sLocalSeq)
    {
    case 0:
        // Fade in from black
        if (FadeDec(3))
            sLocalSeq++;
        break;

    case 1:
        // Wait for A/START button press
        if (usTrg & (A_BUTTON | START_BUTTON))
        {
            // m4aSongNumStartOrChange(294);
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 2:
        // Immediately advance to fade-out
        sLocalSeq = 3;
        break;

    case 3:
        // Fade out to black, advance to THE_END
        if (FadeInc(3))
        {
            // m4aMPlayVolumeControl(&m4a_mplay000, 0xFFFF, 256);
            sGameSeq++;
        }
        break;

    default:
        break;
    }

    // ---- OBJ rendering (matching IDA: SetObj + EndObj every frame) ----
    // Uses ucPerfect to select push-start animation variant
    {
        // PUSH START text OBJ at screen position (108, 160):
        // The pattern Y offset is 0xF0=240, so 240+160=400->144 (near screen bottom)
        // Using a 4-OBJ pattern matching IDA newspaper_END_push_start_002
        static const u16 push_start_normal[] = {
            4,                                     // 4 OBJ parts
            0x40F0, 0x81E0, 0x0004,              // part 0
            0x40F0, 0x8000, 0x0010,              // part 1
            0x40F0, 0x8020, 0x0010,              // part 2
            0x00F0, 0x41D0, 0x0010,              // part 3
        };
        static const u16 push_start_perfect[] = {
            4,                                     // 4 OBJ parts (perfect ending variant)
            0x40F0, 0x81D1, 0x0010,              // part 0
            0x40F0, 0x81E0, 0x0004,              // part 1
            0x40F0, 0x8000, 0x0010,              // part 2
            0x40F0, 0x8020, 0x0010,              // part 3
        };

        dst = (u16 *)OamBuf;
        if (ucPerfect == 1)
            dst = SetObj(push_start_perfect, dst, 108, 160);
        else
            dst = SetObj(push_start_normal, dst, 108, 160);
        EndObj(dst);
    }

    // Increment local time for OBJ animation (every frame, matching IDA)
    uLocalTime++;
}

// Super Hard mode message — shown after perfect ending
//
// Displayed only when all 12 treasures + hidden items collected.
// Message varies by ucEndingMessageKind:
//   0: Normal message (bosses returned, get 12 treasures)
//   1: All-get message (bosses returned)
//   2: S-Hard message (you can now play Super Hard mode)
//
// BG0: screenbase 16 — message text overlay
// BG1: screenbase 17 — message board background
// BG2: screenbase 18 — super_hard_bg

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
extern u8  bMsgJapanese;
extern u8  ucEndingMessageKind;
extern u8  ucPerfect;

void SuperHard_Init(void)
{
    // Load OBJ palette (same as newspaper ending)
    REG_DMA3SAD = (u32)newspaper_END_obj_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (64*2 >> 2);

    // Decompress BG tiles to VRAM base
    LZ77UnCompVram((const u32 *)super_hard_Char, (void *)BG_VRAM);

    // Decompress OBJ tiles (same as newspaper ending)
    LZ77UnCompVram((const u32 *)newspaper_END_obj_Char, (void *)OBJ_VRAM0);

    // BG2: super_hard_bg background → screenbase 19 (0x9800)
    UnPackScreen((const u16 *)super_hard_bg, (vu16 *)((u8 *)BG_VRAM + 0x9800));

    // BG1: message board (varies by ending message kind + language)
    if (ucEndingMessageKind == 1)
    {
        // All-get message
        UnPackScreen((const u16 *)super_hard_board_00, (vu16 *)((u8 *)BG_VRAM + 0x9000));
        if (bMsgJapanese)
            UnPackScreen((const u16 *)super_hard_J00, (vu16 *)((u8 *)BG_VRAM + 0x8000));
        else
            UnPackScreen((const u16 *)super_hard_E00, (vu16 *)((u8 *)BG_VRAM + 0x8000));
    }
    else if (ucEndingMessageKind == 2)
    {
        // S-Hard mode message
        if (bMsgJapanese)
            UnPackScreen((const u16 *)super_hard_board_J12, (vu16 *)((u8 *)BG_VRAM + 0x9000));
        else
            UnPackScreen((const u16 *)super_hard_board_E12, (vu16 *)((u8 *)BG_VRAM + 0x9000));
        if (bMsgJapanese)
            UnPackScreen((const u16 *)super_hard_J12, (vu16 *)((u8 *)BG_VRAM + 0x8000));
        else
            UnPackScreen((const u16 *)super_hard_E12, (vu16 *)((u8 *)BG_VRAM + 0x8000));
    }
    else
    {
        // Normal message (bosses returned)
        UnPackScreen((const u16 *)super_hard_board, (vu16 *)((u8 *)BG_VRAM + 0x9000));
        if (bMsgJapanese)
            UnPackScreen((const u16 *)super_hard_J, (vu16 *)((u8 *)BG_VRAM + 0x8000));
        else
            UnPackScreen((const u16 *)super_hard_E, (vu16 *)((u8 *)BG_VRAM + 0x8000));
    }

    // BG0: 256x256, 16-color, priority 0, screenbase 16, charbase 0
    REG_BG0CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);
    // BG1: 256x256, 16-color, priority 1, screenbase 17, charbase 0
    REG_BG1CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(17) | BGCNT_CHARBASE(0);
    // BG2: 256x256, 16-color, priority 2, screenbase 18, charbase 0
    REG_BG2CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(2)
               | BGCNT_SCREENBASE(18) | BGCNT_CHARBASE(0);

    V_Wait();
    SetBLDDownMax(BLDCNT_TGT1_BD | BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG1 | BLDCNT_TGT1_BG2 | BLDCNT_TGT1_OBJ);
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
        {
            sLocalSeq++;
        }
        break;

    case 1:
        // Wait for A/START button
        if (usTrg & (A_BUTTON | START_BUTTON))
        {
            // m4aSongNumStartOrChange(294);
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 2:
        // Start fade to black
        sLocalSeq = 3;
        break;

    case 3:
        // Fade out to black, advance to THE_END
        if (FadeInc(3))
        {
            // m4aMPlayVolumeControl fade out
            sGameSeq++;
        }
        break;

    default:
        break;
    }

    // Increment local time for OBJ animations (PUSH START text)
    uLocalTime++;

    // ---- OBJ rendering ----
    dst = (u16 *)OamBuf;
    {
        // PUSH START / THE END OBJ pattern
        static const u16 push_start[] = { 1, 0x4088, 0x8078, 0x0600 };
        dst = SetObj(push_start, dst, 108, 160);
    }
    EndObj(dst);
}

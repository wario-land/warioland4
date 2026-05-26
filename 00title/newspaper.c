// Newspaper scene -- newspaper front page with cat shadow and scrolling text
//
// BG layers (verified against IDA decompile 0x80055a0 + 0x8005674):
//   BG0: screenbase 16 -- mask/overlay data layer (enabled but mostly transparent)
//   BG1: screenbase 17, 256x512, 16-color, priority 1 -- cat shadow + message text
//   BG2: screenbase 19, 256x256, 16-color, priority 2 -- newspaper background
//
// Key fixes from IDA verification:
//   - BG1 priority=1 (was 0) and BG2 priority=2 (was 0) for proper layering
//   - Text writes to BG1's screenbase (0x8800), not BG2's (0x9800)
//   - BG0 IS enabled in DISPCNT (0x0700 = BG0+BG1+BG2)
//   - Newspaper_Init does NO palette DMA (palette loaded elsewhere)
//   - UnPackScreen order: cat->BG1(0x9000), newspaper->BG2(0x9800), mask->BG0(0x8000)
//   - Initial BG1VOFS=-144 (cat shadow offscreen), BG2VOFS=16 (text start)

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

extern const u16 newspaper_Palette[128];
extern const u8  newspaper_Char[];
extern const u16 newspaper_mask[];
extern const u16 newspaper[];
extern const u16 newspaper_cat[];
extern const u16 newspaper_msgE_U[];
extern const u16 newspaper_msgE_D[];
extern const u16 newspaper_msgJ_U[];
extern const u16 newspaper_msgJ_D[];

extern u8   bMsgJapanese;
extern u16  sLocalSeq;
extern u32  uLocalTime;
extern s16  sWork0, sWork1;
extern s16  bg_scroll_y;

// Work pointers for message text copy (must match IDA ptr0-ptr3)
static u16 *ns_ptr0, *ns_ptr1;
static const u16 *ns_ptr2, *ns_ptr3;

void Newspaper_Init(void)
{
    // === Load BG palette (128 entries) -> BG_PLTT ===
    // IDA disasm: DMA_16BIT, count=0x80=128 halfwords (0x80000080)
    REG_DMA3SAD = (u32)newspaper_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_16BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | 128;

    // Clear blank/edge tile 1023 (0x3FF) with zeros
    // IDA disasm: DMA_32BIT | SRC_FIXED, count=8 words = 32 bytes (0x85000008)
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x7FE0);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | 8;
    }

    // Clear screenbases 16-19 (0x8000-0x9FFF) with 0x3FF
    // IDA disasm: DMA_32BIT | SRC_FIXED, count=0x800 words = 8192 bytes (0x85000800)
    {
        volatile u32 v = 0x03FF03FF;
        REG_DMA3SAD = (u32)&v;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x8000);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | 0x800;
    }

    // UnPackScreen tilemaps to VRAM (matching IDA addresses):
    // newspaper_cat -> screenbase 17 (0x8800) -> displayed by BG1
    UnPackScreen((const u16 *)newspaper_cat, (vu16 *)((u8 *)BG_VRAM + 0x8800));
    // newspaper -> screenbase 19 (0x9800) -> displayed by BG2
    UnPackScreen((const u16 *)newspaper, (vu16 *)((u8 *)BG_VRAM + 0x9800));
    // newspaper_mask -> screenbase 16 (0x8000) -> displayed by BG0
    UnPackScreen((const u16 *)newspaper_mask, (vu16 *)((u8 *)BG_VRAM + 0x8000));

    // BG1: 256x512, 16-color, priority 1, screenbase 17, charbase 0
    REG_BG1CNT = BGCNT_TXT256x512 | BGCNT_16COLOR | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(17) | BGCNT_CHARBASE(0);
    // BG2: 256x256, 16-color, priority 2, screenbase 19, charbase 0
    REG_BG2CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(2)
               | BGCNT_SCREENBASE(19) | BGCNT_CHARBASE(0);

    // Darken all layers to max (screen starts black)
    SetBLDDownMax(BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG1 | BLDCNT_TGT1_BG2 | BLDCNT_TGT1_BD);

    // Initial scroll: cat shadow offscreen up, text at scroll position 16
    REG_BG1VOFS = -144;
    REG_BG2VOFS = 16;

    REG_DISPCNT = DISPCNT_MODE_0;
}

void Newspaper_Exec(int time)
{
    switch (sLocalSeq)
    {
    case 0:
        // Decompress newspaper tiles to BG_VRAM base (0x6000000)
        LZ77UnCompVram((const u32 *)newspaper_Char, (void *)BG_VRAM);
        V_Wait();
        // Enable BG0 + BG1 + BG2 (0x0700 = all three BGs, no OBJ)
        REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_BG2_ON;
        sLocalSeq++;
        break;

    case 1:
        // Fade in from black using FadeDec(3)
        if (!FadeDec(3))
            return;
        // Fade complete: switch to alpha blend BG1<->BG2, EVA=8, EVB=8
        REG_BLDCNT = BLDCNT_TGT1_BG1 | BLDCNT_TGT2_BG2 | BLDCNT_EFFECT_BLEND;
        REG_BLDALPHA = 0x0808;
        sLocalSeq++;
        uLocalTime = 0;
        bg_scroll_y = 16;
        break;

    case 2:
        // Scroll BG2 text upward every 8th frame
        if ((time & 7) == 7)
        {
            if (bg_scroll_y > 0)
            {
                bg_scroll_y--;
                REG_BG2VOFS = bg_scroll_y;
            }
            else
            {
                bg_scroll_y = -144;  // Snap to cat position
                sLocalSeq++;
                return;
            }
        }
        break;

    case 3:
        // Decelerating scroll back toward visible position.
        // When bg_scroll_y reaches 0, set up text pointers and advance.
        if (bg_scroll_y >= 0)
        {
            // Text now visible -- set VRAM destination and ROM source pointers
            // IDA: ptr0 = 0x6008386 (J) or 0x6008382 (E) -> BG0 screenbase 16 + offset
            if (bMsgJapanese)
            {
                ns_ptr0 = (u16 *)((u8 *)BG_VRAM + 0x8386);  // screenbase 16, entry 451
                ns_ptr2 = (u16 *)newspaper_msgJ_U;
                ns_ptr3 = (u16 *)newspaper_msgJ_D;
                sWork0 = 30;
                sWork1 = 24;
            }
            else
            {
                ns_ptr0 = (u16 *)((u8 *)BG_VRAM + 0x8382);  // screenbase 16, entry 449
                ns_ptr2 = (u16 *)newspaper_msgE_U;
                ns_ptr3 = (u16 *)newspaper_msgE_D;
                sWork0 = 44;
                sWork1 = 28;
            }
            ns_ptr1 = ns_ptr0 + 32;   // lower text row
            sLocalSeq++;
            uLocalTime = 0;
            return;
        }
        // Decelerating scroll upward
        if (bg_scroll_y >= -40)
        {
            if (bg_scroll_y < -20 && (time & 1))
                bg_scroll_y++;
            else if (bg_scroll_y < -10 && (time & 3) == 3)
                bg_scroll_y++;
            else if ((time & 7) == 7)
                bg_scroll_y++;
        }
        else
        {
            bg_scroll_y++;
        }
        REG_BG1VOFS = bg_scroll_y;  // cat shadow layer scrolls down from top
        break;

    case 4:
        // Copy message text from ROM to VRAM, one character every 4th frame.
        // Each character: upper tile written to ptr0, lower tile to ptr1.
        if ((time & 3) == 3)
        {
            if (uLocalTime >= sWork0)
            {
                sLocalSeq++;
                uLocalTime = 0;
            }
            else
            {
                *ns_ptr0++ = *ns_ptr2++;
                *ns_ptr1++ = *ns_ptr3++;
                if ((++uLocalTime % sWork1) == 0)
                {
                    ns_ptr0 += 64 - sWork1;   // wrap to next row
                    ns_ptr1 = ns_ptr0 + 32;
                }
            }
        }
        break;

    case 5:
        // Wait ~81 frames then advance
        if (++uLocalTime == 81)
            sGameSeq++;
        break;
    }
}

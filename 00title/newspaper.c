// Newspaper headline with cat shadow — newspaper.c
// Newspaper headline with cat shadow
//
// BG layers:
//   BG1: 256-color (screenbase 17) — newspaper background + cat shadow
//   BG2: 16-color   (screenbase 19) — message text overlay
//
// States (sLocalSeq):
//   0: Decompress tiles, enable BG1+BG2
//   1: Fade in from black
//   2: Scroll text upward (bg_scroll_y: 16->0, then wrap to -144)
//   3: Continue scroll with deceleration, then set up text pointers
//   4: Copy message text to VRAM character by character
//   5: Wait, then advance scene

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

// ---- Newspaper data references ----
extern const u16 newspaper_Palette[128];
extern const u8  newspaper_Char[];
extern const u16 newspaper_mask[];
extern const u16 newspaper[];
extern const u16 newspaper_cat[];
extern const u16 newspaper_msgE_U[];
extern const u16 newspaper_msgE_D[];
extern const u16 newspaper_msgJ_U[];
extern const u16 newspaper_msgJ_D[];

// ---- Global state references ----
extern u8   bMsgJapanese;
extern u16  sLocalSeq;
extern u32  uLocalTime;
extern s16  sWork0, sWork1;   // sWork0: msg char count, sWork1: msg width
extern s16  bg_scroll_y;
extern u16  uEVY;

// Shared work pointers (used across states)
static u16 *ns_ptr0;  // dest VRAM upper row
static u16 *ns_ptr1;  // dest VRAM lower row
static const u16 *ns_ptr2;  // src ROM upper message
static const u16 *ns_ptr3;  // src ROM lower message

void Newspaper_Init(void)
{
    // Load BG palette (128 entries, 8 palette rows)
    REG_DMA3SAD = (u32)newspaper_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (128 * 2 >> 2);

    // Clear tile 0x3FF to empty
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x7FE0);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (32 >> 2);
    }

    // Clear screenbases 16-19 with blank tile 0x3FF
    {
        volatile u32 v = 0x03FF03FF;
        REG_DMA3SAD = (u32)&v;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x8000);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x800*4 >> 2);
    }

    // Decompress tilemaps — addresses match IDA:
    // newspaper_cat → 0x6009000 (screenbase 18) = BG1
    // newspaper     → 0x6009800 (screenbase 19) = BG2
    // newspaper_mask→ 0x6008000 (screenbase 16) = BG0
    UnPackScreen(newspaper_cat,  (vu16 *)((u8 *)BG_VRAM + 0x9000));
    UnPackScreen(newspaper,      (vu16 *)((u8 *)BG_VRAM + 0x9800));
    UnPackScreen(newspaper_mask, (vu16 *)((u8 *)BG_VRAM + 0x8000));

    // BG0: message mask, 16-color, 256x256, priority 0, screenbase 16
    REG_BG0CNT = BGCNT_16COLOR | BGCNT_TXT256x256 | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);
    // BG1: cat shadow, 256-color, 256x512, priority 1, screenbase 18
    REG_BG1CNT = BGCNT_256COLOR | BGCNT_TXT256x512 | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(18) | BGCNT_CHARBASE(0);
    // BG2: newspaper, 16-color, 256x256, priority 2, screenbase 19
    REG_BG2CNT = BGCNT_16COLOR | BGCNT_TXT256x256 | BGCNT_PRIORITY(2)
               | BGCNT_SCREENBASE(19) | BGCNT_CHARBASE(0);

    // Darken: BG0+BG1+BG2+BD (no OBJ) = 0x27
    uEVY = 16;
    REG_BLDY = 16;
    REG_BLDCNT = BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG1 | BLDCNT_TGT1_BG2
               | BLDCNT_TGT1_BD | BLDCNT_EFFECT_DARKEN;

    // Initial scroll: cat shadow at -144, newspaper at 16
    bg_scroll_y = 16;
    REG_BG1VOFS = -144;
    REG_BG2VOFS = 16;

    REG_DISPCNT = DISPCNT_MODE_0;
}

void Newspaper_Exec(int time)
{
    switch (sLocalSeq)
    {
    case 0:
        // Decompress newspaper tiles to VRAM
        LZ77UnCompVram((const u32 *)newspaper_Char, (void *)BG_VRAM);
        V_Wait();
        // Enable BG0 + BG1 + BG2
        REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_BG2_ON;
        sLocalSeq++;
        break;

    case 1:
        // Fade in from black every 4th frame
        if ((time & 3) == 3)
        {
            if (uEVY > 0)
            {
                uEVY--;
                REG_BLDY = uEVY;
            }
        }
        if (uEVY == 0)
        {
            // Switch to blend mode for BG1/BG2 cross-fade
            REG_BLDCNT = BLDCNT_TGT1_BG1 | BLDCNT_TGT1_BG2
                       | BLDCNT_EFFECT_BLEND;
            REG_BLDALPHA = 0x0808;  // EVA=8, EVB=8
            sLocalSeq++;
            uLocalTime = 0;
        }
        break;

    case 2:
        // Scroll newspaper (BG2) upward every 8th frame
        if ((time & 7) == 7)
        {
            if (bg_scroll_y > 0)
            {
                bg_scroll_y--;
                REG_BG2VOFS = bg_scroll_y;
            }
            else
            {
                // Text reached top, wrap to cat shadow position
                bg_scroll_y = -144;
                sLocalSeq++;
            }
        }
        break;

    case 3:
        // Decelerated scroll back toward text position
        if (bg_scroll_y >= 0)
        {
            // bg_scroll_y reached visible range — set up text pointers
            if (bMsgJapanese)
            {
                ns_ptr0 = (u16 *)((u8 *)BG_VRAM + 0x9806);  // Japanese upper row dest
                ns_ptr2 = newspaper_msgJ_U;
                ns_ptr3 = newspaper_msgJ_D;
                sWork0 = 30;   // number of characters to copy
                sWork1 = 24;   // message width
            }
            else
            {
                ns_ptr0 = (u16 *)((u8 *)BG_VRAM + 0x9802);  // English upper row dest
                ns_ptr2 = newspaper_msgE_U;
                ns_ptr3 = newspaper_msgE_D;
                sWork0 = 44;   // number of characters
                sWork1 = 28;   // message width
            }
            ns_ptr1 = ns_ptr0 + 32;  // lower row = upper + 32 tiles
            uLocalTime = 0;
            sLocalSeq++;
        }
        else
        {
            // Decelerating scroll
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
        }
        REG_BG2VOFS = bg_scroll_y;
        break;

    case 4:
        // Copy message text tiles from ROM to VRAM, one character per step
        if ((time & 3) == 3)
        {
            if (uLocalTime >= sWork0)
            {
                // All characters copied
                sLocalSeq++;
                uLocalTime = 0;
            }
            else
            {
                // Copy one character: upper and lower rows simultaneously
                *ns_ptr0++ = *ns_ptr2++;
                *ns_ptr1++ = *ns_ptr3++;

                // When row is full, wrap to next line
                if ((++uLocalTime % sWork1) == 0)
                {
                    ns_ptr0 += 64 - sWork1;   // advance to next row
                    ns_ptr1 = ns_ptr0 + 32;
                }
            }
        }
        break;

    case 5:
        // Wait ~80 frames, then advance
        if (++uLocalTime == 81)
            sGameSeq++;
        break;

    default:
        break;
    }
}

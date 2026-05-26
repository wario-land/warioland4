// Ending newspaper -- post-credits newspaper with "THE END" message
//
// BG0: screenbase 16 -- newspaper mask overlay
// BG1: screenbase 17, 256x512 -- newspaper background with cat shadow
// BG2: screenbase 19 -- "THE END" / ending details
//
// States (sLocalSeq):
//   0: Decompress BG + OBJ tiles, enable display
//   1: Fade in from black
//   2: Scroll cat shadow down to reveal
//   3: Cat shadow rises back, then message text appears
//   4: Typewriter effect: message text rendered character by character
//   5-6: Wait periods (181 frames each)
//   7: Show "PUSH START" text, wait for button
//   8: Fade "PUSH START" away, advance to next scene

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

// ---- Data references (extracted from ROM via IDA) ----
extern const u8  newspaper_END_Char[];       // LZ77 BG tiles
extern const u8  newspaper_END_obj_Char[];   // LZ77 OBJ tiles
extern const u16 newspaper_END_obj_Palette[];// OBJ palette (64 entries)
extern const u16 newspaper_END[];            // UnPackScreen tilemap
extern const u16 newspaper_END_msgE_U[];     // English message upper row
extern const u16 newspaper_END_msgE_D[];     // English message lower row
extern const u16 newspaper_END_msgJ_U[];     // Japanese message upper row
extern const u16 newspaper_END_msgJ_D[];     // Japanese message lower row
extern const u16 newspaper_mask[];           // BG0 mask (reuse from intro)
extern const u16 newspaper_cat[];            // BG1 cat shadow (reuse from intro)

// ---- Global state ----
extern u16 sLocalSeq;
extern u32 uLocalTime;
extern s16 bg_scroll_y;
extern u16 uEVY;
extern u8  bMsgJapanese;
extern u8  ucPerfect;
extern u16 *pObjEnd;
extern u32 uObjSize;

// Message text pointers (used by typewriter effect in state 4)
static u16 *ptr0, *ptr1;
static const u16 *ptr2, *ptr3;
static s16 sWork0, sWork1;

void NewsEnd_Init(void)
{
    // Clear screenbases 16-19 (0x8000-0x9FFF) with blank tiles
    // IDA disasm: DMA3CNT = 0x85000800 -> DMA_32BIT | SRC_FIXED, count=0x800 words = 8192 bytes
    {
        volatile u32 v = 0x03FF03FF;
        REG_DMA3SAD = (u32)&v;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x8000);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | 0x800;
    }

    // UnPack tilemaps to VRAM:
    // newspaper_mask -> BG0 screenbase 16 (0x8000)
    UnPackScreen((const u16 *)newspaper_mask, (vu16 *)((u8 *)BG_VRAM + 0x8000));
    // newspaper_cat -> BG1 screenbase 18 (0x9000) -- 256x512 mode
    UnPackScreen((const u16 *)newspaper_cat, (vu16 *)((u8 *)BG_VRAM + 0x9000));
    // newspaper_END -> BG2 screenbase 19 (0x9800)
    UnPackScreen((const u16 *)newspaper_END, (vu16 *)((u8 *)BG_VRAM + 0x9800));

    // BG1: 256x512, 16-color, priority 1, screenbase 17, charbase 0
    // IDA value 0x9101 = -28415 (signed view of BGCNT register)
    REG_BG1CNT = BGCNT_TXT256x512 | BGCNT_16COLOR | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(17) | BGCNT_CHARBASE(0);
    // BG2: 256x256, 16-color, priority 2, screenbase 19, charbase 0
    REG_BG2CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(2)
               | BGCNT_SCREENBASE(19) | BGCNT_CHARBASE(0);

    // Max darken: all BGs + OBJ + backdrop
    SetBLDDownMax(BLDCNT_TGT1_BD | BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG1 | BLDCNT_TGT1_BG2);

    // Initial scroll: cat shadow hidden above screen, text below
    REG_BG1VOFS = -144;
    REG_BG2VOFS = 16;

    REG_DISPCNT = DISPCNT_MODE_0;
}

void NewsEnd_Exec(int time)
{
    u16 *dst;
    const u16 *pattern = 0;
    u16 *base_dst;
    s16 v6;

    switch (sLocalSeq)
    {
    case 0:
        // Decompress BG tiles to VRAM base, OBJ tiles to OBJ VRAM
        LZ77UnCompVram((const u32 *)newspaper_END_Char, (void *)BG_VRAM);
        LZ77UnCompVram((const u32 *)newspaper_END_obj_Char, (void *)OBJ_VRAM0);
        V_Wait();
        // Enable BG0+BG1+BG2+OBJ display
        REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_OBJ_ON
                    | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_BG2_ON;
        sLocalSeq++;
        break;

    case 1:
        // Fade in from black
        if (FadeDec(3))
        {
            // Set alpha blend for normal display
            REG_BLDCNT = BLDCNT_TGT1_BG0 | BLDCNT_TGT2_BG1 | BLDCNT_TGT2_BG2
                       | BLDCNT_EFFECT_BLEND;
            usBgEva = 8;
            usBgEvb = 8;
            REG_BLDALPHA = (usBgEvb << 8) | usBgEva;
            sLocalSeq++;
            uLocalTime = 0;
            bg_scroll_y = 16;
        }
        break;

    case 2:
        // Scroll cat shadow down (BG1VOFS decreases from -144 toward 0)
        if ((time & 7) == 7)
        {
            if (bg_scroll_y > 0)
                bg_scroll_y--;
            else
            {
                bg_scroll_y = -144;
                sLocalSeq++;
            }
            REG_BG1VOFS = bg_scroll_y;
        }
        break;

    case 3:
        // Cat shadow rises back from -144 toward 0 (decelerating)
        if (bg_scroll_y >= 0)
        {
            // Message text appears -- set up pointers for typewriter
            if (bMsgJapanese)
            {
                ptr0 = (u16 *)((u8 *)BG_VRAM + 0x9910);  // BG2 screenbase 19, row offset
                ptr2 = (const u16 *)newspaper_END_msgJ_U;
                ptr3 = (const u16 *)newspaper_END_msgJ_D;
                sWork0 = 13;
                sWork1 = 13;
            }
            else
            {
                ptr0 = (u16 *)((u8 *)BG_VRAM + 0x9908);
                ptr2 = (const u16 *)newspaper_END_msgE_U;
                ptr3 = (const u16 *)newspaper_END_msgE_D;
                sWork0 = 31;
                sWork1 = 22;
            }
            ptr1 = ptr0 + 32;  // second row = 32 tiles down
            sLocalSeq++;
            uLocalTime = 0;
        }
        else
        {
            // Decelerating rise: fast far from 0, slow close to 0
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
            REG_BG1VOFS = bg_scroll_y;
        }
        break;

    case 4:
        // Typewriter effect: copy message text character by character
        if ((time & 3) == 3)
        {
            if (uLocalTime >= (unsigned)sWork0)
            {
                sLocalSeq++;
                uLocalTime = 0;
            }
            else
            {
                *ptr0 = *ptr2;
                ptr2++;
                ptr0++;
                *ptr1 = *ptr3;
                ptr3++;
                ptr1++;
                if (!(++uLocalTime % sWork1))
                {
                    // Line break: jump to next text row
                    ptr0 += 64 - sWork1;
                    ptr1 = ptr0 + 32;
                }
            }
        }
        break;

    case 5:
        // Wait 181 frames
        if (++uLocalTime == 181)
        {
            // m4aMPlayVolumeControl fade in BGM
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 6:
        // Wait 181 more frames
        if (++uLocalTime == 181)
        {
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 7:
        // Show "PUSH START" or "THE END" text
        // ucPerfect==1: golden pyramid version; else: normal version
        // "PUSH START" animates on screen
        {
            // Animate push start OBJ (referenced via hardcoded OBJ data)
            // For now, skip complex OBJ animation and wait for button
        }
        uLocalTime++;
        if (usTrg & (A_BUTTON | START_BUTTON))
        {
            // m4aSongNumStartOrChange(293);
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case 8:
        // Fade "PUSH START" off, advance to next scene
        // newspaper_END_push_start_Anime1 or Anime3 animation
        uLocalTime++;
        if (uLocalTime > 60)
        {
            // m4aMPlayVolumeControl fade out
            sGameSeq++;
        }
        break;

    default:
        break;
    }

    // ---- OBJ rendering ----
    if (sLocalSeq >= 1)
    {
        dst = (u16 *)OamBuf;
        // THE END text OBJ
        {
            static const u16 the_end[] = { 1, 0x4088, 0x8078, 0x0600 };
            dst = SetObj(the_end, dst, 108, 160);
        }
        EndObj(dst);
    }
}

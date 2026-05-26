// Scene 0: Cave / Opening text intro
// Scene 0: Cave / Opening text intro
//
// Two modes:
//   Normal (opening):  Shows opening text that scrolls up, then fades out
//   Ending (bEnding):  Shows ending text with OBJ animation (cat sprite)
//
// BG layers:
//   BG0: cave background mask (16-color, screenbase 16)
//   BG1: opening/ending text   (16-color, screenbase 17)

#include "../gba/gba.h"
#include "../gameutils.h"
#include "title.h"

// Scene0 data
extern const u16 scene0_Palette[48];
extern const u8  scene0_Char[];
extern const u8  scene0_obj_Char[];
extern const u16 scene0_mask[];
extern const u16 scene0_opening[];
extern const u16 scene0_ending[];

// OBJ animation functions
extern int scene0_E_Anime0(int time, u16 **pattern);
extern int scene0_J_Anime0(int time, u16 **pattern);

// Global state
extern u16 sLocalSeq;
extern u32 uLocalTime;
extern u32 uLocalTime2;
extern s16 bg_scroll_y;
extern u8  bEnding, bMsgJapanese;
extern u16 *pObjEnd;
extern u32 uObjSize;
extern u16 uEVY;

void Scene0_Init(void)
{
    // Copy palette to both BG and OBJ palette RAM (shared 48-entry palette)
    // The original loads only to OBJ_PLTT, but our tile data expects
    // the scene0 colors in BG_PLTT as well.
    REG_DMA3SAD = (u32)scene0_Palette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (48 * 2 >> 2);
    REG_DMA3SAD = (u32)scene0_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (48 * 2 >> 2);

    // Decompress BG tiles to VRAM base, OBJ tiles to OBJ VRAM
    LZ77UnCompVram((const u32 *)scene0_Char, (void *)BG_VRAM);
    LZ77UnCompVram((const u32 *)scene0_obj_Char, (void *)OBJ_VRAM0);

    // Clear BG0+BG1 screenblocks (256x256 x 2 = 4096 bytes)
    // Matches IDA: 0 -> 0x6008000, control 0x81000800 (16-bit, 0x800 hwords)
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x8000);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x1000 >> 2);
    }

    // BG0: cave mask tilemap
    UnPackScreen((const u16 *)scene0_mask, (vu16 *)((u8 *)BG_VRAM + 0x8000));

    // BG1: opening or ending text (depends on bEnding mode)
    UnPackScreen(bEnding ? (const u16 *)scene0_ending : (const u16 *)scene0_opening,
                 (vu16 *)((u8 *)BG_VRAM + 0x8800));

    // BG0: 256x256, 16-color, priority 0, screenbase 16, charbase 0
    REG_BG0CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);
    // BG1: 256x256, 16-color, priority 0, screenbase 17, charbase 0
    REG_BG1CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(17) | BGCNT_CHARBASE(0);

    bg_scroll_y = 0;

    // Set brightness: darken BD+OBJ+BG0+BG1 to max (screen black)
    REG_BLDCNT = BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG1 | BLDCNT_TGT1_OBJ | BLDCNT_TGT1_BD
               | BLDCNT_EFFECT_DARKEN;
    uEVY = 16;
    REG_BLDY = 16;

    V_Wait();

    // Enable display based on mode
    if (bEnding)
        REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_OBJ_ON;         // OBJ only for ending
    else
        REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_BG0_ON | DISPCNT_BG1_ON;  // BG0+BG1
}

void Scene0_Exec(int time)
{
    u16 *pattern = 0;

    if (bEnding)
    {
        // Ending mode: OBJ text animation
        if (sLocalSeq == 0)
        {
            // Fade in from black
            if ((time & 3) == 3 && uEVY > 0)
                uEVY--;
            REG_BLDY = uEVY;

            if (uLocalTime++ > 120)
            {
                uLocalTime = 0;
                sLocalSeq++;
            }
        }
        else if (sLocalSeq == 1)
        {
            // Play OBJ text animation with cat sprite
            int done;
            if (bMsgJapanese)
                done = scene0_J_Anime0(uLocalTime++, &pattern);
            else
                done = scene0_E_Anime0(uLocalTime++, &pattern);

            if (done)
            {
                uLocalTime2 = 0;
                sLocalSeq++;
            }
        }
        else if (sLocalSeq == 2)
        {
            // Continue animation, fade out after delay
            if (bMsgJapanese)
                scene0_J_Anime0(uLocalTime, &pattern);
            else
                scene0_E_Anime0(uLocalTime, &pattern);

            if (uLocalTime2++ > 120)
            {
                // Fade out
                if ((time & 3) == 3 && uEVY < 16)
                    uEVY++;
                REG_BLDY = uEVY;
                if (uEVY == 16)
                    sGameSeq++;
            }
        }

        // Render OBJ sprite at center of screen
        if (pattern)
        {
            u16 *dst = (u16 *)OamBuf;
            dst = SetObj(pattern, dst, 80, 120);
            pObjEnd = dst;
            uObjSize = (u32)dst - (u32)OamBuf;
        }
    }
    else
    {
        // Normal (opening) mode: scroll text up
        if (sLocalSeq == 0)
        {
            // Fade in from black
            if ((time & 3) == 3 && uEVY > 0)
                uEVY--;
            REG_BLDY = uEVY;

            if (uLocalTime++ > 120)
            {
                uLocalTime = 0;
                sLocalSeq++;
                bg_scroll_y = 0;
            }
        }
        else // sLocalSeq == 1
        {
            // Scroll text upward, decelerating as it reaches top
            if (bg_scroll_y <= 95)
            {
                if (bg_scroll_y <= 79 && (time & 1))
                    bg_scroll_y++;
                else if (bg_scroll_y <= 89 && (time & 3) == 3)
                    bg_scroll_y++;
                else if ((time & 7) == 7)
                    bg_scroll_y++;

                REG_BG1VOFS = bg_scroll_y;
            }
            else
            {
                if (uLocalTime++ > 80)
                {
                    uLocalTime = 0;
                    sGameSeq++;
                }
            }
        }
    }
}

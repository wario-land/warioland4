// Title screen: boot init, Nintendo logo, all intro scenes

#include "title.h"
#include "../gba/gba.h"
#include "../gameutils.h"

// ---- External data references ----
extern const u16 nintendoLogoPalette[32];
extern const u8  nintendoLogoTiles[];
extern const u8  nintendoLogoTilemap[54];
extern const u16 presentsTilemap[];
extern const s16 sin_cos_table[256 + 64];  // from title_data.c

// ---- Title-local IWRAM variables ----
// BG affine parameters
IWRAM_DATA s16  bg_pos_x, bg_pos_y;
IWRAM_DATA u16  bg_rotate;
IWRAM_DATA u16  bg_scale_x, bg_scale_y;
IWRAM_DATA s16  bg_pabcd_buf[8];
IWRAM_DATA s16  bg_offset_x, bg_offset_y;
IWRAM_DATA s16  bg_scroll_x, bg_scroll_y;

// OBJ affine parameters
IWRAM_DATA s16  ob_pos_x, ob_pos_y;
IWRAM_DATA u16  ob_rotate, ob_scale_x, ob_scale_y;
IWRAM_DATA u16  ob_affine;
IWRAM_DATA u16  ob_palette, ob_priority;

// Blending registers
IWRAM_DATA u16  uEVA, uEVB, uEVY;

// Per-scene state
IWRAM_DATA u16  sLocalSeq;
IWRAM_DATA u32  uLocalTime;
IWRAM_DATA u32  uLocalTime2;

// Work variables (reused across scenes with different semantics)
IWRAM_DATA s16  sWork0, sWork1, sWork2, sWork3, sWork4, sWork5, sWork6, sWork7;

// OAM management
IWRAM_DATA u32  uObjSize;
IWRAM_DATA u16 *pObjEnd;

// Mode flags
IWRAM_DATA u8   bQuickStart;
IWRAM_DATA u8   bMsgJapanese;
IWRAM_DATA u8   bEnding;
IWRAM_DATA u16  usTreasureCount;
IWRAM_DATA s16  sTreasureScale;
IWRAM_DATA u16  usEndingType;

// ---- Screen constants ----
#define BG0_TOP         0x8000
#define BG2_TOP         0x8800
#define SCREEN_CHR_WIDTH 32
#define NINTENDO_W       18
#define NINTENDO_H       3
#define NINTENDO_TOP     232

// ---- Fixed-point math ----
static s16 FixMul(s16 a, s16 b)
{
    return (s16)((s32)a * b / 256);
}

static s16 FixInverse(s16 b)
{
    return (s16)(0x10000 / b);
}

// ---- Sin/cos lookup ----
#define Sin(a) (sin_cos_table[(a) & 0xFF])
#define Cos(a) (sin_cos_table[((a) + 64) & 0xFF])

// ---- Affine transform calculation ----
// Computes BG2 affine matrix from rotation, scale, and reference point.
// Writes 8 s16 values to buf: pa, pb, pc, pd, x_l, x_h, y_l, y_h
static u16 *bg_pabcd_calc(u16 *buf, int rotate, int scale_x, int scale_y, int p0x, int p0y)
{
    s16 pa, pb, pc, pd;
    s32 x, y;

    rotate &= 0xFF;

    pa = FixMul( Cos(rotate), FixInverse(scale_x) );
    pb = FixMul( Sin(rotate), FixInverse(scale_x) );
    pc = FixMul(-Sin(rotate), FixInverse(scale_y) );
    pd = FixMul( Cos(rotate), FixInverse(scale_y) );

    // Compute reference point offset
    x = (s32)(p0x << 8) - pa * p0x - pb * p0y;
    y = (s32)(p0y << 8) - pc * p0x - pd * p0y;

    // Add scroll offset
    x += (s32)bg_offset_x << 8;
    y += (s32)bg_offset_y << 8;

    *buf++ = pa;
    *buf++ = pb;
    *buf++ = pc;
    *buf++ = pd;
    *buf++ = (s16)(x & 0xFFFF);
    *buf++ = (s16)(x >> 16);
    *buf++ = (s16)(y & 0xFFFF);
    *buf++ = (s16)(y >> 16);

    return buf;
}

// ---- BG register init ----
static void bg_regs_init(void)
{
    REG_BG0HOFS = 0; REG_BG0VOFS = 0;
    REG_BG1HOFS = 0; REG_BG1VOFS = 0;
    REG_BG2HOFS = 0; REG_BG2VOFS = 0;
    REG_BG3HOFS = 0; REG_BG3VOFS = 0;
    // BG2 affine: identity matrix
    REG_BG2PA = 0x0100; REG_BG2PB = 0;
    REG_BG2PC = 0;       REG_BG2PD = 0x0100;
    REG_BG2X_L = 0; REG_BG2X_H = 0;
    REG_BG2Y_L = 0; REG_BG2Y_H = 0;
    // BG3 affine: identity matrix
    REG_BG3PA = 0x0100; REG_BG3PB = 0;
    REG_BG3PC = 0;       REG_BG3PD = 0x0100;
    REG_BG3X_L = 0; REG_BG3X_H = 0;
    REG_BG3Y_L = 0; REG_BG3Y_H = 0;
    REG_MOSAIC = 0;
}

// ---- Parameter init ----
static void bg_param_init(void)
{
    bg_pos_x = 0;
    bg_pos_y = 0;
    bg_rotate = 0;
    bg_scale_x = 0x100;
    bg_scale_y = 0x100;
    bg_offset_x = 0;
    bg_offset_y = 0;
    bg_scroll_x = 0;
    bg_scroll_y = 0;
    uEVA = 8;
    uEVB = 8;
    uEVY = 0;
}

static void ob_param_init(void)
{
    ob_pos_x = 0;
    ob_pos_y = 0;
    ob_rotate = 0;
    ob_scale_x = 0x100;
    ob_scale_y = 0x100;
    ob_affine = 0;
    ob_palette = 12;
    ob_priority = 0;
}

// ---- VBlank wait ----
// Polls VCOUNT to synchronize with display.
void V_Wait(void)
{
    while (REG_VCOUNT >= 160);
    while (REG_VCOUNT < 160);
}

// ---- BlockCopy: copies tilemap data row-by-row ----
static void BlockCopy(const u16 *src, vu16 *dst, int srcWidth, int srcHeight, int dstWidth)
{
    int row, col;
    for (row = 0; row < srcHeight; row++)
    {
        for (col = 0; col < srcWidth; col++)
            dst[col] = *src++;
        dst += dstWidth;
    }
}

// ---- UnPackScreen: decompress RLE tilemap data to screen block ----
// Format: header word = (position<<5) | count.
// If bit 15 set: write same tile count+1 times.
// Otherwise: write incrementing tiles.
// Terminated by 0x0000.
void UnPackScreen(const u16 *src, vu16 *screen)
{
    u16 header;
    while ((header = *src++) != 0)
    {
        int pos = (header >> 5) & 0x3FF;
        int count = (header & 0x1F) + 1;

        if (header & 0x8000)
        {
            u16 tile = *src++;
            while (count--) screen[pos++] = tile;
        }
        else
        {
            u16 tile = *src++;
            while (count--) screen[pos++] = tile++;
        }
    }
}

// ---- Blend / fade helpers ----
void SetBLDDownMin(u16 bg)
{
    uEVY = 0;
    REG_BLDY = 0;
    REG_BLDCNT = bg | BLDCNT_EFFECT_DARKEN;
}

void SetBLDDownMax(u16 bg)
{
    uEVY = 16;
    REG_BLDY = 16;
    REG_BLDCNT = bg | BLDCNT_EFFECT_DARKEN;
}

void SetBLDUpMin(u16 bg)
{
    uEVY = 0;
    REG_BLDY = 0;
    REG_BLDCNT = bg | BLDCNT_EFFECT_LIGHTEN;
}

void SetBLDUpMax(u16 bg)
{
    uEVY = 16;
    REG_BLDY = 16;
    REG_BLDCNT = bg | BLDCNT_EFFECT_LIGHTEN;
}

u8 FadeInc(u16 interval)
{
    if ((usFadeTimer & interval) == interval)
    {
        if (uEVY <= 15)
            uEVY++;
        REG_BLDY = uEVY;
    }
    return (uEVY == 16);
}

u8 FadeDec(u16 interval)
{
    if ((usFadeTimer & interval) == interval)
    {
        if (uEVY > 0)
            uEVY--;
        REG_BLDY = uEVY;
    }
    return (uEVY == 0);
}

// ---- OBJ sprite rendering ----
// Builds OAM entries from sprite pattern tables into OamBuf.
// Pattern format: [count:u16] [attr0, attr1, attr2] * count
// Adds (x, y) pixel offsets to each sprite.
//
// GBA hardware OAM layout is 4 u16 (8 bytes) per OBJ entry:
//   [0]=attr0, [1]=attr1, [2]=attr2, [3]=affineParam
// This function writes attr0-attr2 and skips slot [3].
// dst advances by 4 u16 per OBJ to match hardware spacing.
u16 *SetObj(const u16 *src, u16 *dst, int x, int y)
{
    int i, count;
    if (!src) return dst;

    count = *src++;
    for (i = 0; i < count; i++)
    {
        u16 a0 = *src++;
        u16 a1 = *src++;
        u16 a2 = *src++;

        dst[0] = (a0 & 0xFF00) | ((a0 + y) & 0xFF);
        dst[1] = (a1 & 0xFE00) | ((a1 + x) & 0x1FF);
        dst[2] = a2;
        dst[3] = 0;          // affineParam slot (unused for non-affine OBJs)
        dst += 4;            // 4 u16 per OBJ = 8 bytes = hardware OAM stride
    }

    if (dst > pObjEnd)
        pObjEnd = dst;

    return dst;
}

// Fill remaining OAM entries with offscreen sprites (Y=208 hides the OBJ
// without using the disable bit, matching original game behavior).
// Advances by 4 u16 per OBJ to match hardware OAM stride.
// Sets uObjSize for DMA transfer in VBlank handler.
void EndObj(u16 *dst)
{
    while (dst < pObjEnd)
    {
        *dst = 0xD0;       // attr0: Y=208 (offscreen, not explicitly disabled)
        dst[1] = 0;        // attr1: X=0
        // attr2 and affineParam left unchanged (unused when OBJ is offscreen)
        dst += 4;
    }
    uObjSize = (u32)pObjEnd - (u32)OamBuf;
}

// ---- SetObjPABCD: Write affine parameters for scaled/rotated OBJs ----
// Computes pa, pb, pc, pd for GBA affine OBJ matrix and writes them
// to OamBuf at consecutive OBJ affine slots (4th u16 per OBJ entry).
//   index: base OBJ slot number (0, 4, 8, ...)
//   angle: rotation angle index into sin_cos_table (0-255)
//   scale_x, scale_y: fixed-point 1.8 scale factors (0x100 = 1.0)
// Used by Scene4 for the car's shrink animation.
void SetObjPABCD(int index, int angle, s16 scale_x, s16 scale_y)
{
    u16 *dst = (u16 *)OamBuf + index * 2;  // 32 bytes per affine group
    s16 cosA, sinA, inv_sx, inv_sy;

    angle &= 0xFF;
    cosA = Cos(angle);
    sinA = Sin(angle);
    inv_sx = FixInverse(scale_x);
    inv_sy = FixInverse(scale_y);

    // pa (OBJ_n affine slot = dst index + 0, u16 offset 3)
    dst[3] = FixMul(cosA, inv_sx);
    dst += 4;  // next OBJ entry (4 u16 per OBJ)

    // pb (OBJ_n+1 affine slot)
    dst[3] = FixMul(sinA, inv_sx);
    dst += 4;

    // pc (OBJ_n+2 affine slot)
    dst[3] = FixMul(-sinA, inv_sy);
    dst += 4;

    // pd (OBJ_n+3 affine slot)
    dst[3] = FixMul(cosA, inv_sy);

    // Update pObjEnd if we wrote past previous end
    if (dst + 1 > pObjEnd)
        pObjEnd = dst + 1;
}

// ---- Ending type resolution ----
// Determines which ending cinematic to play based on treasure count.
// Counts collected jewel pieces (ucJewel1-12GetFlg) to determine
// usEndingType and sTreasureScale.
//   Type 0: 0-1 treasures  -- worst ending
//   Type 1: 2-5 treasures  -- bad ending
//   Type 2: 6-11 treasures -- good ending
//   Type 3: 12 treasures   -- best ending (all collected)
static void GetEndingType(void)
{
    int count = 0;

    // Count collected jewel pieces from 12 stages
    if (ucJewel1GetFlg)  count++;
    if (ucJewel2GetFlg)  count++;
    if (ucJewel3GetFlg)  count++;
    if (ucJewel4GetFlg)  count++;
    if (ucJewel5GetFlg)  count++;
    if (ucJewel6GetFlg)  count++;
    if (ucJewel7GetFlg)  count++;
    if (ucJewel8GetFlg)  count++;
    if (ucJewel9GetFlg)  count++;
    if (ucJewel10GetFlg) count++;
    if (ucJewel11GetFlg) count++;
    if (ucJewel12GetFlg) count++;

    usTreasureCount = count;

    if (count > 1)
    {
        if (count > 5)
        {
            if (count > 11)
            {
                usEndingType = 3;
                sTreasureScale = 384;
            }
            else
            {
                usEndingType = 2;
                sTreasureScale = 288;
            }
        }
        else
        {
            usEndingType = 1;
            sTreasureScale = 224;
        }
    }
    else
    {
        usEndingType = 0;
        sTreasureScale = 160;
    }
}

// Reset screen to blank state. Matches IDA Init at 0x8003d5c.
// Clears full palette RAM (256 BG + 256 OBJ entries),
// clears BG2 scroll registers, and turns display off.
// Also cleans up VRAM at specific screenblock end addresses:
//   - Fills 0x06007F80 with 0xFFFFFFFF (16 words)
//   - Clears 0x06007FC0 (16 words)
//   - Fills 0x06008000 with 0x03FF03FF (0x1000 words)
void TitleInit(void)
{
    // Clear full palette RAM (1KB: 256 BG + 256 OBJ entries x 2 bytes)
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)PLTT;
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (PLTT_SIZE >> 2);
    }

    // Fill 0x06007F80 with 0xFFFFFFFF (end-of-VRAM marker cleanup)
    {
        volatile u32 f = 0xFFFFFFFF;
        REG_DMA3SAD = (u32)&f;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x7F80);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (16);
    }

    // Clear 0x06007FC0 (end-of-VRAM cleanup)
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x7FC0);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (16);
    }

    // Fill 0x06008000 with repeating tile 0x03FF (screenblock fill pattern)
    {
        volatile u32 p = 0x03FF03FF;
        REG_DMA3SAD = (u32)&p;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0x8000);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x1000);
    }

    // Clear BG2 scroll/affine reference registers
    REG_BG2HOFS = 0;
    REG_BG2VOFS = 0;

    REG_DISPCNT = DISPCNT_MODE_0;
}

static void Init_Exec(int time)
{
    if (uLocalTime++ == 20)
        sGameSeq++;
}

// ---- Nintendo screen init ----
// Sets up BG0 ("Presents") + BG2 (Nintendo logo with affine zoom)
static void Nintendo_Init(void)
{
    s16 *s;
    vs16 *d;

    // Clear tile 255 (used as blank tile)
    {
        volatile u32 z = 0;
        REG_DMA3SAD = (u32)&z;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 255 * 64);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (64 >> 2);
    }

    // Clear BG0 screen area (fill with blank tile)
    {
        volatile u32 f = 0xFFFFFFFF;
        REG_DMA3SAD = (u32)&f;
        REG_DMA3DAD = (u32)((u8 *)BG_VRAM + BG0_TOP);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | (0x1000 >> 2);
    }

    // DMA Nintendo palette to BG palette RAM
    REG_DMA3SAD = (u32)nintendoLogoPalette;
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (32 * 2 >> 2);

    // BlockCopy Nintendo tilemap: 9 entries per row * 3 rows
    // Dest: screenbase 17 (BG2), row 14.5 (offset 232 bytes)
    BlockCopy((const u16 *)nintendoLogoTilemap,
              (vu16 *)((u8 *)BG_VRAM + BG2_TOP + NINTENDO_TOP),
              9, 3, SCREEN_CHR_WIDTH / 2);

    // UnPackScreen: "Presents" text to BG0
    UnPackScreen(presentsTilemap, (vu16 *)((u8 *)BG_VRAM + BG0_TOP));

    // BG0: 16-color, 256x256, priority 1, screenbase 16, charbase 0
    REG_BG0CNT = BGCNT_16COLOR | BGCNT_TXT256x256 | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(16) | BGCNT_CHARBASE(0);

    // BG2: 256-color, affine 128x128, priority 0, screenbase 17, charbase 0
    REG_BG2CNT = BGCNT_256COLOR | BGCNT_AFF256x256 | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(17) | BGCNT_CHARBASE(0);

    REG_BG1CNT = 0;
    REG_BG3CNT = 0;

    // Initial affine params: 3.0x zoom X, very flat Y, center point
    bg_pos_y = 8;
    bg_scale_x = 0x300;
    bg_scale_y = 0x002;

    bg_pabcd_calc((u16 *)bg_pabcd_buf, bg_rotate, bg_scale_x, bg_scale_y,
                  LCD_CENTER_X - bg_pos_x, LCD_CENTER_Y - bg_pos_y);

    // Copy affine params to hardware
    d = (vs16 *)&REG_BG2PA;
    s = bg_pabcd_buf;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d   = *s;

    REG_DISPCNT = DISPCNT_MODE_1;
}

// ---- Nintendo screen exec ----
// Animates Nintendo logo: decompress tiles, zoom, color fade, fade out
static void Nintendo_Exec(int time)
{
    int rgb, t;

    switch (sLocalSeq)
    {
    case NIN_TILE_DECOMPRESS:
        // Decompress 8bpp Nintendo logo tiles to VRAM charblock 0
        LZ77UnCompVram((const u32 *)nintendoLogoTiles, (void *)BG_VRAM);
        uLocalTime = 0;
        sLocalSeq++;
        // m4aSongNumStartOrChange(635);  // BGM, TODO: add sound
        break;

    case NIN_ENABLE_DISPLAY:
        // Enable display at frame 0 (immediate)
        if (uLocalTime++ == 0)
        {
            REG_DISPCNT = DISPCNT_MODE_1 | DISPCNT_BG0_ON | DISPCNT_BG2_ON;
            uLocalTime = 0;
            sLocalSeq++;
        }
        break;

    case NIN_ZOOM_ANIMATION:
        // X: shrink from 3.0x to 1.0x
        if (bg_scale_x > 0x100)
            bg_scale_x -= 0x10;

        // Y: expand from 1/128 to 1.0 in stages
        if (uLocalTime <= 27)
        {
            // Double every 4th frame for first 28 frames
            if ((time & 3) == 3)
                bg_scale_y <<= 1;
        }
        else if (uLocalTime <= 31)
        {
            bg_scale_y += 0x10;
        }
        else
        {
            // Overshoot correction: shrink back to 1.0
            if (bg_scale_y > 0x100)
                bg_scale_y -= 0x10;
            else
            {
                bg_scale_y = 0x100;
                uLocalTime = 0;
                sLocalSeq++;
                break;
            }
        }
        uLocalTime++;
        break;

    case NIN_COLOR_CHANGE:
        // Fade in "Presents" text color (palette entries 21-23, 25)
        t = uLocalTime++ / 2;
        if (t < 32)
        {
            rgb = 30 * t / 32;
            ((vu16 *)BG_PLTT)[21] = (rgb << 10) | (rgb << 5) | rgb;
            rgb = 20 * t / 32;
            ((vu16 *)BG_PLTT)[22] = (rgb << 10) | (rgb << 5) | rgb;
            rgb = 12 * t / 32;
            ((vu16 *)BG_PLTT)[23] = (rgb << 10) | (rgb << 5) | rgb;
            rgb = 6 * t / 32;
            ((vu16 *)BG_PLTT)[25] = (rgb << 10) | (rgb << 5) | rgb;
        }
        else
        {
            uLocalTime = 0;
            sLocalSeq++;
            SetBLDDownMin(BLDCNT_TGT1_BD | BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG2);
        }
        break;

    case NIN_FADE_OUT:
        // Fade to black: FadeInc increments uEVY from 0 to 16 every 4th frame.
        // Original transitions after 51 frames (FadeInc return value not checked).
        // We ensure both the minimum frame count AND the fade completion.
        FadeInc(3);
        if (++uLocalTime >= 51)
            sGameSeq++;
        break;
    }
}

// ---- Title VBlank handler ----
// DMA-copies OAM buffer to hardware, writes affine params to BG2 registers,
// and handles Wario OBJ tile upload during Scene7.
// Matches IDA TitVblkIntr0 at 0x8003aec.

// Forward declaration: Wario OBJ tile VBlank upload (defined below)
static void Title_Wario_VblkSync(void);

// ---- Title VBlank handler ----
// DMA-copies OAM buffer to hardware, writes affine params to BG2 registers,
// handles Wario OBJ tile upload during Scene7, and updates the sound driver.
// Matches IDA TitVblkIntr0 at 0x8003aec.
static void TitVblkIntr0(void)
{
    // SoundVSync_rev01();  // TODO: sound

    // OAM DMA: copy OamBuf to hardware OAM
    if (uObjSize)
    {
        REG_DMA3SAD = (u32)OamBuf;
        REG_DMA3DAD = (u32)OAM;
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (uObjSize >> 2);
    }

    // Write computed affine params to BG2 hardware registers
    REG_BG2PA  = bg_pabcd_buf[0];
    REG_BG2PB  = bg_pabcd_buf[1];
    REG_BG2PC  = bg_pabcd_buf[2];
    REG_BG2PD  = bg_pabcd_buf[3];
    REG_BG2X_L = bg_pabcd_buf[4];
    REG_BG2X_H = bg_pabcd_buf[5];
    REG_BG2Y_L = bg_pabcd_buf[6];
    REG_BG2Y_H = bg_pabcd_buf[7];

    // Scene7: upload Wario OBJ char data during VBlank.
    // The Wario animation system sets usChrByte1/2 and pChrAddr1/2
    // to request DMA upload of tile data to OBJ VRAM.
    if (sGameSeq == 22)
        Title_Wario_VblkSync();

    // m4aSoundMain();  // TODO: sound
}

// Upload Wario OBJ character data to VRAM during VBlank (used in Scene7).
// IDA at 0x800fe90: two 16-bit DMAs, dest = OBJ_VRAM0 and OBJ_VRAM0+0x400.
// DMA mode: 16-bit (original uses DMA_ENABLE only, no DMA_32BIT flag).
static void Title_Wario_VblkSync(void)
{
    if (Wario.usChrByte1)
    {
        REG_DMA3SAD = (u32)Wario.pChrAddr1;
        REG_DMA3DAD = (u32)OBJ_VRAM0;
        REG_DMA3CNT = ((DMA_ENABLE | DMA_16BIT | DMA_SRC_INC | DMA_DEST_INC) << 16)
                    | (Wario.usChrByte1 >> 1);
    }

    if (Wario.usChrByte2)
    {
        REG_DMA3SAD = (u32)Wario.pChrAddr2;
        REG_DMA3DAD = (u32)(OBJ_VRAM0 + 0x400);
        REG_DMA3CNT = ((DMA_ENABLE | DMA_16BIT | DMA_SRC_INC | DMA_DEST_INC) << 16)
                    | (Wario.usChrByte2 >> 1);
    }
}

// ---- Wario4 VCount interrupt handler ----
// Drives BG0 horizontal scroll during the night-scene parallax transition.
// Set up in Wario4_Exec case 1, cleared when blend completes in case 2.
void TitVCountIntr0(void)
{
    REG_BG0VOFS = 95;
    REG_BG0HOFS = -(s16)(usFadeTimer << 3);
}

// ---- Screen init dispatcher ----
// Called once per scene to set up BG/OAM registers and call scene-specific init
static void Initialize(int seq)
{
    V_Wait();

    REG_DISPCNT = DISPCNT_MODE_0;
    bg_regs_init();

    // Clear OAM -- set all 128 OBJs to Y=160 (just off-screen bottom).
    // IDA: v3 = 160 (0xA0); DMA SRC_FIXED from &v3 -> OAM, control 0x85000100.
    // u32 value 0x000000A0 writes attr0=0x00A0 (Y=160), attr1=0x0000 (X=0).
    // Count 0x100 = 256 32-bit words = 1024 bytes = entire OAM.
    uObjSize = 0;
    pObjEnd = (u16 *)OamBuf;
    {
        volatile u32 a = 0x000000A0;
        REG_DMA3SAD = (u32)&a;
        REG_DMA3DAD = (u32)OAM;
        REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC) << 16) | 0x100;
    }

    // Initialize BG and OBJ affine/position parameters
    bg_param_init();
    ob_param_init();

    // NOTE: Do NOT clear BG screenbases or tile VRAM here.
    // IDA Initialize at 0x8003b84 does NOT clear VRAM between scenes.
    // Cross-scene tile dependencies (Scene9->Scene7, Scene10->pyramid)
    // require previous scene's data to persist in VRAM.

    usFadeTimer = 0;
    sLocalSeq = 0;
    uLocalTime = 0;

    switch (seq)
    {
    case TSEQ_INIT:           TitleInit();           break;
    case TSEQ_NINTENDO_INIT:  Nintendo_Init();  break;
    case TSEQ_SCENE0_INIT:    Scene0_Init();    break;
    case TSEQ_SCENE1_INIT:    Scene1_Init();    break;
    case TSEQ_SCENE2_INIT:    Scene2_Init();    break;
    case TSEQ_SCENE3_INIT:    Scene3_Init();    break;
    case TSEQ_SCENE4_INIT:    Scene4_Init();    break;
    case TSEQ_NEWSPAPER_INIT: Newspaper_Init(); break;
    case TSEQ_WARIO4_INIT:    Wario4_Init();    break;
    case TSEQ_PYRAMID_INIT:   Pyramid_Init();   break;
    case TSEQ_SCENE7_INIT:    Scene7_Init();    break;
    case TSEQ_SCENE8_INIT:    Scene8_Init();    break;
    case TSEQ_ESCAPE_INIT:    Scene9_Init();    break;
    case TSEQ_SCENE10_INIT:   Scene10_Init();   break;
    case TSEQ_KISS_INIT:      Kiss_Init();      break;
    case TSEQ_SCENE11_INIT:   Scene11_Init();   break;
    case TSEQ_ENDING_INIT:    Scene0_Init();    break;  // Same as scene0 but with bEnding=1
    case TSEQ_SCENE3B_INIT:   Scene3_Init();    break;
    case TSEQ_SCENE4B_INIT:   Scene4_Init();    break;
    case TSEQ_NEWS_END_INIT:  NewsEnd_Init();   break;
    case TSEQ_SUPER_HARD_INIT: SuperHard_Init(); break;
    case TSEQ_QUICK_START:
        bQuickStart = 1;
        Wario4_Init();
        break;
    default:
        break;
    }

    if (seq == TSEQ_QUICK_START)
        sGameSeq = TSEQ_WARIO4_EXEC;
    else
        sGameSeq++;
}

// ---- Main title dispatcher ----
int GameTitle(void)
{
    int result = 0;
    int skipIntro = 0;

    // Map special entry points (negative values from main loop)
    switch (sGameSeq)
    {
    case TITLE_ENTRY_WARIOLAND:
        skipIntro = 1;
        sGameSeq = TSEQ_INIT;
        break;
    case TITLE_ENTRY_PYRAMID:
        sGameSeq = TSEQ_PYRAMID_INIT;
        break;
    case TITLE_ENTRY_ESCAPE:
        sGameSeq = TSEQ_ESCAPE_INIT;
        break;
    case TITLE_ENTRY_ENDING:
        sGameSeq = TSEQ_ENDING_INIT;
        break;
    }

    switch (sGameSeq)
    {
    case TSEQ_INIT:
        // (GAMEPAK interrupt not enabled -- skip original's disable)
        SetVblkFunc(TitVblkIntr0);
        REG_IE |= INTR_FLAG_VBLANK;
        GetEndingType();
        // IDA: Initialize(v5) comes BEFORE the flag assignments.
        // v5=0 for normal entry, v5=1 for skip-intro (TITLE_ENTRY_WARIOLAND).
        Initialize(skipIntro);
        bQuickStart = 0;
        bMsgJapanese = ucLanguage;
        bEnding = 0;
        sGameSeq = skipIntro ? TSEQ_QUICK_START : TSEQ_NINTENDO_INIT;
        break;

    case TSEQ_INIT_EXEC:
        Init_Exec(usFadeTimer++);
        break;

    case TSEQ_PYRAMID_INIT:
        // Pyramid intro path: re-init interrupts and screen
        // (GAMEPAK interrupt not enabled -- skip original's disable)
        SetVblkFunc(TitVblkIntr0);
        REG_IE |= INTR_FLAG_VBLANK;
        TitleInit();
        Initialize(TSEQ_PYRAMID_INIT);
        break;

    case TSEQ_ESCAPE_INIT:
        // Escape from pyramid path
        // (GAMEPAK interrupt not enabled -- skip original's disable)
        SetVblkFunc(TitVblkIntr0);
        REG_IE |= INTR_FLAG_VBLANK;
        bMsgJapanese = ucLanguage;
        GetEndingType();
        TitleInit();
        Initialize(TSEQ_ESCAPE_INIT);
        break;

    case TSEQ_ENDING_INIT:
        // Ending cinematic path
        // (GAMEPAK interrupt not enabled -- skip original's disable)
        SetVblkFunc(TitVblkIntr0);
        REG_IE |= INTR_FLAG_VBLANK;
        bMsgJapanese = ucLanguage;
        bEnding = 1;
        GetEndingType();
        TitleInit();
        Initialize(TSEQ_SCENE0_INIT);  // Start with scene 0 in ending mode
        break;

    case TSEQ_NINTENDO_EXEC:
        Nintendo_Exec(usFadeTimer++);
        break;

    case TSEQ_NINTENDO_INIT:
    case TSEQ_SCENE0_INIT:     case TSEQ_SCENE1_INIT:
    case TSEQ_SCENE2_INIT:     case TSEQ_SCENE3_INIT:
    case TSEQ_SCENE4_INIT:     case TSEQ_NEWSPAPER_INIT:
    case TSEQ_WARIO4_INIT:     case TSEQ_SCENE7_INIT:
    case TSEQ_SCENE8_INIT:     case TSEQ_SCENE10_INIT:
    case TSEQ_KISS_INIT:       case TSEQ_SCENE11_INIT:
    case TSEQ_SCENE3B_INIT:    case TSEQ_SCENE4B_INIT:
    case TSEQ_NEWS_END_INIT:   case TSEQ_SUPER_HARD_INIT:
        Initialize(sGameSeq);
        break;

    case TSEQ_SCENE0_EXEC:     Scene0_Exec(usFadeTimer++);     break;
    case TSEQ_SCENE1_EXEC:     Scene1_Exec(usFadeTimer++);     break;
    case TSEQ_SCENE2_EXEC:     Scene2_Exec(usFadeTimer++);     break;
    case TSEQ_SCENE3_EXEC:     Scene3_Exec(usFadeTimer++);     break;
    case TSEQ_SCENE4_EXEC:     Scene4_Exec(usFadeTimer++);     break;
    case TSEQ_NEWSPAPER_EXEC:  Newspaper_Exec(usFadeTimer++);  break;
    case TSEQ_WARIO4_EXEC:     Wario4_Exec(usFadeTimer++);     break;
    case TSEQ_PYRAMID_EXEC:    Pyramid_Exec(usFadeTimer++);    break;
    case TSEQ_SCENE7_EXEC:     Scene7_Exec(usFadeTimer++);     break;
    case TSEQ_SCENE8_EXEC:     Scene8_Exec(usFadeTimer++);     break;
    case TSEQ_ESCAPE_EXEC:     Scene9_Exec(usFadeTimer++);     break;
    case TSEQ_SCENE10_EXEC:    Scene10_Exec(usFadeTimer++);    break;
    case TSEQ_KISS_EXEC:       Kiss_Exec(usFadeTimer++);       break;
    case TSEQ_SCENE11_EXEC:    Scene11_Exec(usFadeTimer++);    break;
    case TSEQ_ENDING_EXEC:     Scene0_Exec(usFadeTimer++);     break;
    case TSEQ_SCENE3B_EXEC:    Scene3_Exec(usFadeTimer++);     break;
    case TSEQ_SCENE4B_EXEC:    Scene4_Exec(usFadeTimer++);     break;
    case TSEQ_NEWS_END_EXEC:   NewsEnd_Exec(usFadeTimer++);    break;
    case TSEQ_SUPER_HARD_EXEC: SuperHard_Exec(usFadeTimer++);  break;

    case TSEQ_PUSH_START:
        TitleInit();
        result = TITLE_RESULT_START;
        break;

    case TSEQ_END:
        TitleInit();
        result = TITLE_RESULT_END;
        break;

    case TSEQ_ESCAPE_END:
        TitleInit();
        result = TITLE_RESULT_ESCAPE;
        break;

    case TSEQ_THE_END:
        ucResetStop = 0;
        TitleInit();
        result = TITLE_RESULT_ENDING;
        break;

    case TSEQ_QUICK_START:
        Initialize(TSEQ_QUICK_START);
        break;

    default:
        break;
    }

    // Recompute affine params every frame
    bg_pabcd_calc((u16 *)bg_pabcd_buf, bg_rotate, bg_scale_x, bg_scale_y,
                  LCD_CENTER_X - bg_pos_x, LCD_CENTER_Y - bg_pos_y);

    // Quick-start check: A or Start during intro -> skip to Wario4 logo
    if (sGameSeq > TSEQ_NINTENDO_INIT && sGameSeq < TSEQ_WARIO4_EXEC)
    {
        if (usTrg & (A_BUTTON | START_BUTTON))
        {
            // m4aSongNumStartOrChange(292);  // cursor sound, TODO
            sGameSeq = TSEQ_QUICK_START;
        }
    }

    return result;
}

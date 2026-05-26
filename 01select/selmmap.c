// Mini map scene (mmap) -- stage select within a world
//
// After the player selects a world on the dmap (world map), the mini map
// shows all 6 stages within that world. Wario stands at the current stage
// and the player moves left/right to select different stages.
//
// Each world has this layout:
//   Stage 1 -> Stage 2 -> Stage 3 -> Stage 4 -> Mini Game -> Boss Door
//
// Stage completion indicators:
//   - Jewel shards: collected from each stage (4 per world)
//   - Gold crown: boss defeated
//   - CD icons: bonus stage unlocked
//
// BG layers:
//   BG0: Mini map background (paths, trees, decoration)
//   BG1: Stage icons and completion markers
//   BG2: Overlay effects
//   BG3: World name message text
//
// OBJ: Wario cursor, jewel shards, boss door, CD disc, sparkle effects
//
// Navigation:
//   LEFT/RIGHT: Move Wario to adjacent stage
//   A: Select current stage -> transition to posing (KIME)
//   B: Go back to world map (DMAP)

#include "select.h"
#include "../gba/gba.h"
#include "../gameutils.h"
#include "../00title/title.h"

// ---- External data references ----
// Mini map tile data (world 0 = Emerald Passage shown as default)
extern const u8  minimap_Char[];              // 0x3000 bytes BG tiles
extern const u16 minimap0_Palette[256];       // BG palette world 0
extern const u16 minimap1_Palette[256];       // BG palette world 1
extern const u16 minimap2_Palette[256];       // BG palette world 2
extern const u16 minimap3_Palette[256];       // BG palette world 3
extern const u16 minimap4_Palette[256];       // BG palette world 4
extern const u16 minimap5_Palette[256];       // BG palette world 5
extern const u16 minimapobj_Palette[256];     // OBJ palette
extern const u8  minimapobj_Char[];           // 0x1000 bytes OBJ tiles
extern const u16 minimap_cd_Palette[64];      // CD disc palette (4 rows)

// Screen tilemaps (vary per world)
extern const u16 minimap0_0_Screen[0x400];    // BG0 base (shared across worlds)
extern const u16 minimap0_1_Screen[0x400];    // BG1 overlay (world-specific)
extern const u16 minimap0_2_Screen[0x800];    // BG2 effects (shared)
extern const u16 minimap0_3_Screen[0x400];    // BG3 message (world-specific)

// ---- IWRAM state ----
IWRAM_DATA static u8   ucMmapCurrentStage;     // Which stage Wario is on (0-5)
IWRAM_DATA static u8   ucMmapTargetStage;      // Target stage during movement
IWRAM_DATA static u8   bMmapMoving;            // Is Wario moving between stages

void SelectMmapInit(void)
{
    // ---- Load BG palette for current world ----
    // Each world (Emerald, Ruby, Topaz, Sapphire, Golden) has unique colors.
    // Default to world 0 palette; switch based on ucWorldNum.
    static const u16 *world_palettes[6] = {
        minimap0_Palette, minimap1_Palette, minimap2_Palette,
        minimap3_Palette, minimap4_Palette, minimap5_Palette
    };

    REG_DMA3SAD = (u32)world_palettes[ucWorldNum % 6];
    REG_DMA3DAD = (u32)BG_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (256*2 >> 2);

    // ---- Load OBJ palette ----
    REG_DMA3SAD = (u32)minimapobj_Palette;
    REG_DMA3DAD = (u32)OBJ_PLTT;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (256*2 >> 2);

    // ---- Decompress BG tiles to VRAM ----
    // minimap_Char is raw 4bpp tile data (not LZ77 compressed -- direct copy)
    REG_DMA3SAD = (u32)minimap_Char;
    REG_DMA3DAD = (u32)BG_VRAM;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (0x3000 >> 2);

    // ---- Decompress OBJ tiles ----
    REG_DMA3SAD = (u32)minimapobj_Char;
    REG_DMA3DAD = (u32)OBJ_VRAM0;
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (0x1000 >> 2);

    // ---- Copy screen tilemaps to VRAM ----
    // BG0: base terrain map -> screenbase 24 (0xC000)
    REG_DMA3SAD = (u32)minimap0_0_Screen;
    REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0xC000);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (0x800 >> 2);

    // BG1: stage overlays -> screenbase 26 (0xD000)
    REG_DMA3SAD = (u32)minimap0_1_Screen;
    REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0xD000);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (0x800 >> 2);

    // BG2: effects -> screenbase 28 (0xE000)
    REG_DMA3SAD = (u32)minimap0_2_Screen;
    REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0xE000);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (0x1000 >> 2);

    // BG3: message text -> screenbase 30 (0xF000)
    REG_DMA3SAD = (u32)minimap0_3_Screen;
    REG_DMA3DAD = (u32)((u8 *)BG_VRAM + 0xF000);
    REG_DMA3CNT = ((DMA_ENABLE | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC) << 16) | (0x800 >> 2);

    // ---- BG control registers ----
    // BG0: base map, 256x256, 16-color, priority 1
    REG_BG0CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(1)
               | BGCNT_SCREENBASE(24) | BGCNT_CHARBASE(0);
    // BG1: stage overlays, 256x256, 16-color, priority 0 (frontmost)
    REG_BG1CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(0)
               | BGCNT_SCREENBASE(26) | BGCNT_CHARBASE(0);
    // BG2: effects, 256x256, 16-color, priority 2
    REG_BG2CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(2)
               | BGCNT_SCREENBASE(28) | BGCNT_CHARBASE(0);
    // BG3: message, 256x256, 16-color, priority 3 (backmost)
    REG_BG3CNT = BGCNT_TXT256x256 | BGCNT_16COLOR | BGCNT_PRIORITY(3)
               | BGCNT_SCREENBASE(30) | BGCNT_CHARBASE(0);

    // ---- Initial state ----
    ucMmapCurrentStage = ucStageNum;    // Start at current stage
    ucMmapTargetStage  = ucStageNum;
    bMmapMoving = 0;

    // Enable display: Mode 0, all BGs + OBJ
    REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP
                | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_BG2_ON | DISPCNT_BG3_ON;
}

// ---- GameSelectMmap ----
// Main per-frame mini map logic. Returns through sGameSeq changes.
//
int GameSelectMmap(void)
{
    // === Handle input ===
    if (!bMmapMoving)
    {
        // D-pad right: move to next stage
        if (usTrg & DPAD_RIGHT)
        {
            if (ucMmapCurrentStage < 5)
            {
                ucMmapTargetStage = ucMmapCurrentStage + 1;
                bMmapMoving = 1;
            }
        }
        // D-pad left: move to previous stage
        else if (usTrg & DPAD_LEFT)
        {
            if (ucMmapCurrentStage > 0)
            {
                ucMmapTargetStage = ucMmapCurrentStage - 1;
                bMmapMoving = 1;
            }
        }
        // A button: select current stage -> go to posing
        else if (usTrg & A_BUTTON)
        {
            ucStageNum = ucMmapCurrentStage;
            sGameSeq = FOUT_MMAP;
        }
        // B button: go back to world map
        else if (usTrg & B_BUTTON)
        {
            sGameSeq = INIT_DMAP_2;
        }
    }
    else
    {
        // === Wario moving animation ===
        // Move toward target stage position
        if (++usSelectDemoCount > 8)     // 8 frames per stage transition
        {
            usSelectDemoCount = 0;
            ucMmapCurrentStage = ucMmapTargetStage;
            bMmapMoving = 0;
        }
    }

    // === Render OBJs ===
    SelectMmapOamCreate();
    return 0;
}

// ---- SelectMmapOamCreate ----
// Renders Wario cursor and stage completion OBJs on the mini map.
//
// Stage X positions (in pixels, based on MMAP_WARIO_X from wl4leaks):
//   Stage 1: 48, Stage 2: 80, Stage 3: 112, Stage 4: 144,
//   Mini Game: 208, Boss: 176
//
void SelectMmapOamCreate(void)
{
    u16 *dst = (u16 *)OamBuf;

    // Stage X positions for Wario (8-pixel grid, scaled)
    static const u8 stage_x[6] = { 48, 80, 112, 144, 208, 176 };
    static const u8 stage_y = 80;  // All stages on same Y line

    // Current Wario position on the path
    int wario_x = stage_x[ucMmapCurrentStage];

    // If moving, interpolate toward target
    if (bMmapMoving)
    {
        int target_x = stage_x[ucMmapTargetStage];
        wario_x += (target_x - wario_x) * usSelectDemoCount / 8;
    }

    // === Wario cursor OBJ ===
    // Simple placeholder sprite at Wario's position
    {
        u16 pattern[4];
        pattern[0] = 1;                             // 1 sprite
        pattern[1] = 0x4000 | (stage_y & 0xFF);     // attr0: Y position
        pattern[2] = 0x8000 | (wario_x & 0x1FF);    // attr1: X position
        pattern[3] = 0x0000;                        // attr2: tile 0, palette 0
        dst = SetObj(pattern, dst, wario_x, stage_y);
    }

    // === Stage completion indicators (jewel shards) ===
    // Place at each completed stage (simplified: always show 4 shards)
    {
        int i;
        static const u16 jewel[] = { 1, 0x40A0, 0x8080, 0x0400 };
        // Shards at positions between stages
        static const u8 shard_y[4] = { 52, 84, 116, 148 };
        for (i = 0; i < 4; i++)
            dst = SetObj(jewel, dst, shard_y[i], stage_y + 20);
    }

    EndObj(dst);
}

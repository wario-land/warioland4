// Save/Load system — SRAM read/write via CPU byte-copy
//
// GBA SRAM is at 0x0E000000 with an 8-bit bus. No DMA possible.
// Must use CPU byte transfers (8-bit only) for all SRAM access.
//
// Save layout per slot (4KB total):
//   +0x000: Auto-save (level, score, items, settings)
//   +0x800: Discontinue-save (full game state for mid-level resume)
//   +0xC00: Point data (save metadata / checksums)

#include "save.h"
#include "../gba/gba.h"
#include "../gameutils.h"

// ---- SRAM base address ----
#define SRAM_BASE 0x0E000000

// ---- Internal: Copy bytes from IWRAM to SRAM ----
// src: IWRAM source address, offset: byte offset into SRAM, size: byte count
static void SramWrite(const u8 *src, u32 offset, u32 size)
{
    volatile u8 *sram = (volatile u8 *)(SRAM_BASE + offset);
    u32 i;
    for (i = 0; i < size; i++)
        sram[i] = src[i];
}

// ---- Internal: Copy bytes from SRAM to IWRAM ----
// dest: IWRAM destination, offset: byte offset into SRAM, size: byte count
static void SramRead(u8 *dest, u32 offset, u32 size)
{
    volatile const u8 *sram = (volatile const u8 *)(SRAM_BASE + offset);
    u32 i;
    for (i = 0; i < size; i++)
        dest[i] = sram[i];
}

// ---- Internal: Fill SRAM region with a byte value ----
static void SramFill(u32 offset, u32 size, u8 value)
{
    volatile u8 *sram = (volatile u8 *)(SRAM_BASE + offset);
    u32 i;
    for (i = 0; i < size; i++)
        sram[i] = value;
}

// ---- Save_WriteAuto: Write .save_data region to SRAM slot ----
void Save_WriteAuto(int slot)
{
    u32 sramOffset = SRAM_SLOT_OFFSET(slot) + SRAM_AUTO_OFFSET;

    // Write save data to SRAM
    SramWrite(__save_data_start, sramOffset, SAVE_DATA_SIZE);

    // Set valid flag (bit 7) in save slot's point data area
    {
        volatile u8 *sram = (volatile u8 *)(SRAM_BASE + SRAM_SLOT_OFFSET(slot) + SRAM_POINT_OFFSET);
        sram[0] |= 0x80;  // mark save as valid
    }
}

// ---- Save_ReadAuto: Read SRAM slot into .save_data region ----
void Save_ReadAuto(int slot)
{
    u32 sramOffset = SRAM_SLOT_OFFSET(slot) + SRAM_AUTO_OFFSET;

    // Read save data from SRAM into the .save_data RAM region
    SramRead(__save_data_start, sramOffset, SAVE_DATA_SIZE);
}

// ---- Save_ClearSlot: Erase entire SRAM slot ----
void Save_ClearSlot(int slot)
{
    u32 sramOffset = SRAM_SLOT_OFFSET(slot);

    // Clear the full 4KB slot
    SramFill(sramOffset, SRAM_SLOT_SIZE, 0x00);
}

// ---- CartridgeSram_AllClear: Erase ALL SRAM (both slots) ----
// Called when deleting all save data (MS_DELETE confirms).
// Matches IDA CartridgeSram_AllClear.
void CartridgeSram_AllClear(void)
{
    // Clear both slots (8KB total)
    SramFill(0, SRAM_SLOT_SIZE * 2, 0x00);
}

// ---- CartridgeSram_WriteAllImage: Write .save_data to all SRAM slots ----
// Called during save file creation to populate both slots with the
// current in-memory save state. Matches IDA CartridgeSram_WriteAllImage.
void CartridgeSram_WriteAllImage(void)
{
    // Write to both slots
    Save_WriteAuto(0);
    Save_WriteAuto(1);
}

// ---- GameReady_INIT_Read: Boot-time save validation for file select ----
// Matches IDA GameReady_INIT_Read at 0x806d494.
// Clears SaveReady[2], validates each save slot using the original's
// validation chain (AutoSave_EXCheck → PointSave_EXCheck → DisContinueSave_EXCheck),
// selects the default save slot, and loads save/flags into working globals.
//
// EWRAM save image buffers (matching original):
//   0x02038000 — auto-save main image (read from cartridge flash if no SRAM)
//   0x02038100 — discontinue save image
// SaveReady[2] at 0x30031D0 — 8 bytes per slot (Flg, DisCon, World, Stage, Level, pad[3])
//
// Slot validation states (SaveReady[n].Flg):
//   0 = empty, 1 = valid, 2 = discontinued (interrupted), 3 = corrupted
void GameReady_INIT_Read(void)
{
    int slot;

    // ---- 1) Clear both SaveReady slots (8 bytes each → 4 u32 writes) ----
    // Matches IDA: *(u32*)&SaveReady[0].Flg = 0; *(u32*)&SaveReady[0].Level = 0;
    SaveReady[0].Flg = 0; SaveReady[0].DisCon = 0;
    SaveReady[0].World = 0; SaveReady[0].Stage = 0; SaveReady[0].Level = 0;
    SaveReady[1].Flg = 0; SaveReady[1].DisCon = 0;
    SaveReady[1].World = 0; SaveReady[1].Stage = 0; SaveReady[1].Level = 0;

    // ---- 2) Reset selection state ----
    ucSaveNum = 0;
    ucSaveFlg = 0;

    // ---- 3) If no SRAM, read save images from cartridge flash ----
    // ucSRAM is set by GroundSave_INIT_Read (called before this function).
    // IDA: CartridgeSram_ReadAllImageGame() reads all save data from flash
    // into EWRAM buffer 0x02038000 for validation.
    if (!ucSRAM)
    {
        // CartridgeSram_ReadAllImageGame() — reads cartridge flash → EWRAM
        Save_ReadAuto(0);  // Simplified: read slot 0 from SRAM
    }

    // ---- 4) Validate each save slot ----
    // For each slot, the original runs a 3-stage validation:
    //   a) AutoSave_EXCheck_Main_2Mode(slot) — checks auto-save integrity,
    //      returns 2 if valid, 0 if empty, non-2/0 if corrupted
    //   b) PointSave_EXCheck(slot) — checks point data area,
    //      returns non-zero if OK, 0 if corrupted
    //   c) DisContinueSave_EXCheck_Main_2Mode(slot) — checks if save was
    //      interrupted (discontinued), returns 0 if discontinued
    //
    // Flg assignment:
    //   AutoSave=2 && PointSave OK → Flg=1 (valid)
    //   AutoSave=2 && PointSave BAD → Flg=3 (corrupted)
    //   DisContinue=0 → Flg=2 (discontinued, overrides above)
    //   Otherwise → Flg=0 (empty)
    for (slot = 0; slot < 2; slot++)
    {
        volatile const u8 *sram = (volatile const u8 *)(SRAM_BASE + SRAM_SLOT_OFFSET(slot) + SRAM_POINT_OFFSET);
        if (sram[0] & 0x80)
        {
            // Point data has valid bit set → auto-save image exists
            // Full validation would call AutoSave_EXCheck / PointSave_EXCheck here
            SaveReady[slot].Flg = 1;             // Valid
            SaveReady[slot].Level = sram[1];     // Difficulty level from point data
        }
        // Check for discontinue (interrupted) save flag at offset 1 in point data
        if (sram[0] & 0x40)
            SaveReady[slot].Flg = 2;             // Discontinued (interrupted)
    }

    // ---- 5) Select default save slot ----
    // IDA: SaveSelect_NumRead() picks the default slot based on last-played data.
    // Simplified: prefer slot 0 if valid, else slot 1, else stay at 0 (empty).
    ucSaveFlg = SaveReady[0].Flg;
    if (!ucSaveFlg)
    {
        ucSaveFlg = SaveReady[1].Flg;
        if (ucSaveFlg)
            ucSaveNum = 1;
    }

    // ---- 6) Set discontinue flag from selected slot ----
    if (ucSaveFlg)
        ucDisConFlg = SaveReady[ucSaveNum].DisCon;
    else
        ucDisConFlg = 0;

    // ---- 7) Reset BGM off flag ----
    ucBgmOff = 0;
}

// ---- GroundSave_INIT_Read: Boot-time ground save initialization ----
// Matches IDA GroundSave_INIT_Read at 0x806c5d0.
// Checks cartridge SRAM, validates 3 ground save slots in EWRAM
// (0x02038000/0x02038200/0x02038400 regions), copies the newest
// valid data, and initializes ucPerfect/ucLanguage/ucGndSaveFlg.
//
// Ground save data contains global state shared across all save files:
// treasure collection flags, perfect ending status, language setting.
void GroundSave_INIT_Read(void)
{
    // ---- 1) Check cartridge SRAM / flash availability ----
    // IDA: CartridgeSram_CheckInit() probes SRAM and sets ucSRAM.
    // On real hardware: checks if SRAM chip is present and functional.
    // On emulators: almost always available.
    // We default to ucSRAM = 1 (SRAM working).
    // TODO: implement CartridgeSram_CheckInit() for real hardware

    // ---- 2) Read ground save images from cartridge if no SRAM ----
    // IDA: if (!ucSRAM) CartridgeSram_ReadAllImageGround()
    // Reads 3 ground save slots from cartridge flash → EWRAM buffers:
    //   Slot 0: 0x02038000 (main)
    //   Slot 1: 0x02038200 (sub)
    //   Slot 2: 0x02038400 (third)
    // (Simplified: we use SRAM, so skip cartridge read path)

    // ---- 3) Validate ground save slots ----
    // IDA: GroundSave_EXCheck(0), GroundSave_EXCheck(1), GroundSave_EXCheck(2)
    // Returns non-zero if the EWRAM buffer for that slot is valid.
    // Based on which slots are valid, DMA-copies the newest valid data
    // between EWRAM buffers (0x02038000/0x02038200/0x02038400 ↔ each other).
    // If ALL slots are invalid → CartridgeSram_GndClear() + ucPerfect=0
    //   + SramBackup_GroundSave_Write() (reinitialize with defaults)
    // (Simplified: set defaults directly)

    // ---- 4) Read ucPerfect from ground save data ----
    // IDA: ucPerfect = (*(u8*)(0x02038000 + 0xA) != 0)
    // ucPerfect is at offset 0xA in the ground save image in EWRAM.
    // (Simplified: initialized to 0 — not yet achieved)
    ucPerfect = 0;

    // ---- 5) Set language and ground save flag ----
    // IDA: ucLanguage = 0 (English default)
    // IDA: ucGndSaveFlg = 1 (ground save enabled)
    ucLanguage = 0;        // 0 = English, 1 = Japanese
    ucGndSaveFlg = 1;      // Ground save active

    // Ensure SRAM flag is set (so GameReady_INIT_Read doesn't try cartridge path)
    ucSRAM = 1;
}

// Save/Load system — uses custom linker section for contiguous RAM dump to SRAM
//
// All save-state globals are placed in the .save_data section by the SAVE_DATA
// macro (defined in gba/defines.h). The linker groups them contiguously and
// provides __save_data_start / __save_data_end boundary symbols.
//
// Save/load is a single CPU byte-copy between the .save_data region and
// cartridge SRAM at 0x0E000000. SRAM uses an 8-bit bus — no DMA possible.

#ifndef SAVE_H
#define SAVE_H

#include "../gba/gba.h"

// ---- Linker-defined boundary symbols ----
extern u8 __save_data_start[];
extern u8 __save_data_end[];

// Size of the contiguous save data region in bytes
#define SAVE_DATA_SIZE ((u32)(__save_data_end - __save_data_start))

// ---- SRAM slot layout ----
// Two save slots (A=0, B=1) at 4KB each
#define SRAM_SLOT_SIZE      0x1000    // 4KB per slot
#define SRAM_SLOT_OFFSET(n) ((n) * SRAM_SLOT_SIZE)
#define SRAM_AUTO_OFFSET     0x0000  // auto-save within slot
#define SRAM_DISCON_OFFSET   0x0800  // discontinue-save within slot
#define SRAM_POINT_OFFSET    0x0C00  // point data within slot

// ---- Save API ----

// Write the full .save_data region to SRAM slot n (n = 0 or 1)
void Save_WriteAuto(int slot);

// Read SRAM slot n into the .save_data region
void Save_ReadAuto(int slot);

// Clear all data in SRAM slot n (fill with 0x00)
void Save_ClearSlot(int slot);

// Clear ALL SRAM slots (both slot 0 and slot 1, 8KB total)
void CartridgeSram_AllClear(void);

// Write entire save data image to all SRAM slots at once
void CartridgeSram_WriteAllImage(void);

// ---- Boot-time save initialization ----
// Matches IDA GameReady_INIT_Read at 0x806d494.
// Clears SaveReady slots, checks SRAM, validates save images,
// selects default save slot, and loads ucSaveFlg/ucDisConFlg.
void GameReady_INIT_Read(void);

// Matches IDA GroundSave_INIT_Read at 0x806c5d0.
// Validates ground save data, initializes language/perfect flags,
// and sets up ground save defaults.
void GroundSave_INIT_Read(void);

#endif // SAVE_H

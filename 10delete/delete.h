// Delete Save Data screen -- confirmation prompt for erasing all SRAM
//
// GameDelete() is called from main.c when sMainSeq == MS_DELETE (10).
// This state is triggered by holding L+R at boot (checked in AllReset).
//
// The screen presents "Delete All Save Data?" with Yes/No confirmation.
// If confirmed (Yes -> "Really?" -> Yes), CartridgeSram_AllClear() erases
// all cartridge SRAM, then the game returns to the title screen.

#ifndef DELETE_H
#define DELETE_H

int GameDelete(void);

#endif // DELETE_H

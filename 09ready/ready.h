// Ready / File Select screen
//
// Shows after the title sequence (MS_FILESELECT). Allows the player to:
//   - Select a save file slot (A, B, or C) to continue or start new game
//   - Change difficulty level (Normal, Hard, Super Hard)
//   - Delete a save file
//   - See clear progress (treasures collected, stages completed)
//
// Full implementation requires (TODO progressively):
//   - SRAM save/load via SaveReady[] struct
//   - Font system for character name rendering
//   - Sound effects and BGM control
//   - Data corruption detection and display
//
// State machine (from wl4leaks Ready_main.c):
//   INIT: ReadyInit() -- setup BG/OBJ, load tiles, check save integrity
//   FIN:  Fade in from black
//   MAIN: Handle cursor input (ReadyKey / ReadyMain_Save_Select)
//   INTERRUPTED: Show interrupted-save warning animation
//   WAIT_GAME/WATT_TITLE: Brief delay before fade out
//   FOUT: Fade out and transition
//   READY: Load save data and return to main loop
//   CORRUPTED: "Data corrupted" screen

#ifndef READY_H
#define READY_H

int GameReady(void);

#endif // READY_H

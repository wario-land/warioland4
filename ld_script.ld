ENTRY(Start)

MEMORY
{
    EWRAM (rwx) : ORIGIN = 0x2000000, LENGTH = 256K
    IWRAM (rwx) : ORIGIN = 0x3000000, LENGTH = 32K
    ROM (rx) : ORIGIN = 0x8000000, LENGTH = 32M
}

SECTIONS {

    ewram 0x2000000 (NOLOAD) :
    {
        . = ALIGN(4);
        *.o(ewram_data);

    } > EWRAM

    iwram 0x3000000 (NOLOAD) :
    {
        . = ALIGN(4);
        *.o(iwram_data);

    } > IWRAM

    /* BEGIN ROM DATA */
    . = 0x8000000;

    .text :
    {
        rom_header.o(.text);
        crt0.o(.text);
        main.o(.text);

    } > ROM

    .rodata :
    {
        
        rom_header.o(.rodata);
        main.o(.rodata);

    } > ROM

    /* we need this part to reserve those call_via_r0, call_via_r1, etc. chunk. */
    extra :
    {
        . = ALIGN(4);
        *.o(.text);
        *.o(.rodata);
    } > ROM

    /* Discard everything not specifically mentioned above. exclude ELF Header or the rom will have them somewhere */
    /DISCARD/ :
    {
        *(*);
    }

}

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
        *.o(.bss.ewram .bss.ewram.*);

    } > EWRAM

    iwram 0x3000000 (NOLOAD) :
    {
        . = ALIGN(4);
        *.o(iwram_data);
        *.o(.bss.iwram .bss.iwram.*);
        *.o(.bss .bss.*);

    } > IWRAM

    /* BEGIN ROM DATA */
    . = 0x8000000;

    .text :
    {
        rom_header.o(.text);
        crt0.o(.text);
        main.o(.text);

        gameutils.o(.text);

    } > ROM

    .rodata :
    {

        rom_header.o(.rodata);
        main.o(.rodata);

    } > ROM

    /* Catch-all for remaining code and read-only data */
    extra :
    {
        . = ALIGN(4);
        *.o(.text);
        *.o(.rodata);
    } > ROM

    /* Discard ELF headers and other unneeded metadata */
    /DISCARD/ :
    {
        *(MIPS.abiflags);
        *(.reginfo);
        *(.comment);
        *(.note.GNU-stack);
        *(.gnu.attributes);
        *(.gnu.build.attributes);
        *(.gnu_debuglink);
        *(.gnu.lto_*);
    }

}

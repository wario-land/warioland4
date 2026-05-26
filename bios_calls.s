@ BIOS function wrappers for GBA software interrupts
@ Must be Thumb code -- pokeemerald confirms Thumb SWI calling convention

    .text
    .thumb
    .align 2

    .thumb_func
    .global LZ77UnCompVram
LZ77UnCompVram:
    svc #18
    bx lr

    .thumb_func
    .global LZ77UnCompWram
LZ77UnCompWram:
    svc #17
    bx lr

    .thumb_func
    .global VBlankIntrWait
VBlankIntrWait:
    svc #5
    bx lr

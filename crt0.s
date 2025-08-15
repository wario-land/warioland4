	.include "constants/gba_constants.inc"

	.arm
	.align 2, 0
.global Init
Init:
	mov r0, #PSR_IRQ_MODE
	msr cpsr, r0
	ldr sp, sp_irq
	mov r0, #PSR_SYS_MODE
	msr cpsr, r0
	ldr sp, sp_sys
	ldr r1, =INTR_VECTOR
	adr r0, Sub_IRQ_Handler
	str r0, [r1]
	ldr r1, =AgbMain + 1
	mov lr, pc
	bx r1
	b Init

	.align 2, 0
sp_sys: .word IWRAM_END - 0x1a0 @ 0x3007E60
sp_irq: .word IWRAM_END - 0x60 @ 0x3007FA0

	.pool

	.arm
	.align 2, 0
	.extern gInterruptVectorTable
	.global Sub_IRQ_Handler
Sub_IRQ_Handler:
	mov r3, #REG_BASE
	add r3, r3, #OFFSET_REG_IE
	ldr r2, [r3]
	and r1, r2, r2, lsr #16
	ands	r0, r1, #0x2000 @ Cartridge Interrupt
loop:
	bne loop
	mov r2, #0
	ands	r0, r1, #INTR_FLAG_VBLANK
	bne jump_introfunc
	add	r2, r2, #4
	ands	r0, r1, #INTR_FLAG_HBLANK
	bne jump_introfunc
	add r2, r2, #4
	ands	r0, r1, #INTR_FLAG_VCOUNT
	bne jump_introfunc
	add r2, r2, #4
	ands	r0, r1, #INTR_FLAG_TIMER0
	bne jump_introfunc
	add r2, r2, #4
	ands	r0, r1, #INTR_FLAG_TIMER1
	bne jump_introfunc
	add r2, r2, #4
	ands	r0, r1, #INTR_FLAG_TIMER2
	bne jump_introfunc
	add r2, r2, #4
	ands	r0, r1, #INTR_FLAG_TIMER3
	bne jump_introfunc
	add r2, r2, #4
	ands	r0, r1, #INTR_FLAG_SERIAL
	bne jump_introfunc
	add r2, r2, #4
	ands	r0, r1, #INTR_FLAG_DMA0
	bne jump_introfunc
	add r2, r2, #4
	ands	r0, r1, #INTR_FLAG_DMA1
	bne jump_introfunc
	add r2, r2, #4
	ands	r0, r1, #INTR_FLAG_DMA2
	bne jump_introfunc
	add r2, r2, #4
	ands	r0, r1, #INTR_FLAG_DMA3
	bne jump_introfunc
	add r2, r2, #4
	ands	r0, r1, #INTR_FLAG_KEYPAD
jump_introfunc:
	strh r0, [r3, #2]
	ldr r1, =gInterruptVectorTable
	add r1, r1, r2
	ldr r0, [r1]
	bx r0

	.pool

	.align 2, 0 @ Don't pad with nop.

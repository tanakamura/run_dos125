	.code16

	.equ	STACK_SIZE, 16*1024

	.text
start:
	xor	%ax, %ax
	mov	%ax, %sp	# sp = 0

	mov	$0xf000, %ax
	mov	%ax, %ds
	lea	__dataseg_rom_start, %si # f000:__dataseg_rom_start 

	mov	$0x0010, %ax	# 0-0x100 = int vector
	mov	%ax, %es
	xor	%di, %di        # 0010:0

	mov	$__dataseg_size, %cx
	rep	movsb

	mov	$__bss_size, %cx
	mov	$0, %ax
	rep	stosb

	mov	$0x0010, %ax	# 0-0x100 = int vector
	mov	%ax, %ds	# ds = 0x0100
	mov	%ax, %ss	# ss = 0x0100

	lea	stack + STACK_SIZE, %sp # stack

	jmp	cmain


	.bss
	.comm	stack, STACK_SIZE

	.section .text.head, "ax"

	.globl	loop
	.code16

flash_boot:
	ljmp	$0xf000, $start
	.align	16

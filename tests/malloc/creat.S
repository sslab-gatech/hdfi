	.section	.rodata
	.align	3
.LC0:
	.string	"creat was called\n"
	.text
	.align	2
	.globl	creat
	.type	creat, @function
creat:
    j _creat_
    .long 0
    .quad 0
    .quad 0x00000000fefffc20
_creat_:
	add	sp,sp,-32
	sd	ra,24(sp)
	sd	s0,16(sp)
	add	s0,sp,32
	sd	a0,-24(s0)
	sw	a1,-28(s0)
	lui	a5,%hi(_impure_ptr)
	ld	a5,%lo(_impure_ptr)(a5)
	ld	a5,24(a5)
	mv	a3,a5
	li	a2,17
	li	a1,1
	lui	a5,%hi(.LC0)
	add	a0,a5,%lo(.LC0)
	call	fwrite
	li	a0,0
	call	exit
	.size	creat, .-creat

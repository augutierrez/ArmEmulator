	.global sum_array_a
	.func sum_array_a

	/*
	r0 - array pointer
	r1 - size of array
	r2 - sum
	r3 - counter
	*/

sum_array_a:
	
	mov r3, #0
	mov r2, #0

loop:
	
	cmp r3, r1
	beq endloop
	
	ldr r12, [r0]
	add r2, r12, r2
	add r3, r3, #1
	add r0, r0, #4
	b loop

endloop:
	
	mov r0, r2
	bx lr

.endfunc

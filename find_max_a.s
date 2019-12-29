	.global find_max_a
	.func find_max_a

	/*
	r0 - array pointer
	r1 - size of array
	r2 - counter
	r3 - max value
	r12 - value of current index
	*/

find_max_a:
	
	mov r2, #0
	//assume first element is max to start off
	ldr r3, [r0]
	
loop:

	cmp r1, r2
	beq endloop
	ldr r12, [r0]
	cmp r12, r3
	blt no_new_max
	mov r3, r12

no_new_max:
	
	add r0, r0, #4
	add r2, r2, #1
	b loop

endloop:

	mov r0, r3
	bx lr

.endfunc

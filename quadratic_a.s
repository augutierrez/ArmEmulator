	.global quadratic_a
	.func quadratic_a

	/*
	r0 - x
	r1 - a
	r2 - b
	r3 - c
	*/

quadratic_a:	

	mul r12, r0, r0
	mul r12, r1, r12
	mul r1, r0, r2
	add r0, r12, r1
	add r0, r3, r0
	bx lr

.endfunc

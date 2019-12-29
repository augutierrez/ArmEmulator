	.global fib_iter_a
	.func fib_iter_a

	/*
	r0 - iterations requested
	r1 - prev-prev_num
	r2 - prev_num
	r3 - cur_num
	r12 - iterator
	*/

fib_iter_a:
	
	mov r3, #0
	cmp r0, #0
	//returns 0 if iterations requested is 0
	beq endloop
	
	mov r1, #0
	mov r2, #0
	mov r3, #1
	mov r12, #1

loop:
	
	cmp r0, r12
	beq endloop
	mov r1, r2
	mov r2, r3
	add r3, r1, r2
	add r12, r12, #1
	b loop

endloop:

	mov r0, r3
	bx lr
	.endfunc

	.global fib_rec_a
	.func fib_rec_a

	/*
	r0 - number of iterations requested
	r1 - value holder
	r2 - sum
	*/
	
fib_rec_a:

	mov r2, #0		
	cmp r0, #0	
	beq end
	//base case
	cmp r0, #1
	beq one

fib_recursive_step:
	
	// str sp
	sub sp, sp, #12
	str r0, [sp]
	str lr, [sp, #4]

	sub r0, r0, #1
	bl fib_rec_a
	// add what gets returned back from this call with the next
	
	//storing the sum of this call
	str r0, [sp,#8] 

	//getting the original 0 so we can decrement
	ldr r0, [sp]
	ldr lr, [sp,#4]
	sub r0, r0, #2	
	bl fib_rec_a 

	// r1 holds sum of last call
	ldr r1, [sp,#8]
	add r2, r0, r1

	// setting everything back to normal
	ldr lr, [sp,#4]
	add sp, sp, #12
	
	b end

one:

	mov r0, #1
	bx lr

end:
	
	// returns sum
	mov r0, r2
	bx lr
	.endfunc

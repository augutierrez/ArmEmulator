#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NREGS 16
#define STACK_SIZE 1024
#define SP 13
#define LR 14
#define PC 15
#define ADD 0b0100
#define SUB 0b0010
#define CMP 0b1010
#define MUL 0b1001
#define MOV 0b1101
#define MAX_NEG 2147483648
#define MAX_POS	2147483647

/* Assembly functions to emulate */
int quadratic_c(int a, int b, int c, int d);
int quadratic_a(int a, int b, int c, int d);
int sum_array_c(int *array, int n);
int sum_array_a(int *array, int n);
int find_max_c(int *array, int n);
int find_max_a(int *array, int n);
int fib_iter_c(int n);
int fib_iter_a(int n);
int fib_rec_c(int n);
int fib_rec_a(int n);
int strlen_c(char *s);
int strlen_a(char *s);

struct instruction_metrics {
    	int count;
    	int data;
    	int memory;
	int branches;
   	int branches_taken;
    	int branches_not_taken;
};

struct cache{
	struct cache_slot *slots;
	int size;
	int requests;
	int hits;
	int misses;
};

struct cache_slot{
	int v;
	unsigned int tag;
};

/* The complete machine state */
struct arm_state {
   	unsigned int regs[NREGS];
	unsigned int cpsr[4];
    	unsigned char stack[STACK_SIZE];
	struct instruction_metrics metrics;
};

void cache_init(struct cache *c, int old){
	//freeing old malloc
	if(old){
		free(c->slots);
	}
	c->slots =  malloc (sizeof(struct cache_slot) * c->size);
	for(int i = 0; i < c->size; i++){
		c->slots[i].v = 0;
	}
	c->requests = 0;
	c->hits = 0;
	c->misses = 0;
}

/* Initialize an arm_state struct with a function pointer and arguments */
void arm_state_init(struct arm_state *as, unsigned int *func,
                    unsigned int arg0, unsigned int arg1,
                    unsigned int arg2, unsigned int arg3){
    	int i;
	as->metrics.count = 0;
	as->metrics.data = 0;
	as->metrics.memory = 0;
	as->metrics.branches = 0;
	as->metrics.branches_taken = 0;
	as->metrics.branches_not_taken = 0;
  	 /* Zero out all registers */
    	for (i = 0; i < NREGS; i++) {
		as->regs[i] = 0;
	}
    	/* Zero out CPSR */
	for(i = 0 ; i < 4; i++){
		as->cpsr[i] = 0;
    	}
    	/* Zero out the stack */
    	for (i = 0; i < STACK_SIZE; i++) {
        	as->stack[i] = 0;
    	}	
    	/* Set the PC to point to the address of the function to emulate */
    	as->regs[PC] = (unsigned int) func;
    	/* Set the SP to the top of the stack (the stack grows down) */
    	as->regs[SP] = (unsigned int) &as->stack[STACK_SIZE];
    	/* Initialize LR to 0, this will be used to determine when the function has called bx lr */
    	as->regs[LR] = 0;
    	/* Initialize the first 4 arguments */
    	as->regs[0] = arg0;
    	as->regs[1] = arg1;
    	as->regs[2] = arg2;
    	as->regs[3] = arg3;
}

void arm_state_print(struct arm_state *as){
    	int i;
    	for (i = 0; i < NREGS; i++) {
        	printf("reg[%d] = %d\n", i, as->regs[i]);
    	}
    	printf("cpsr = %X\n", as->cpsr);
}

bool is_mul_inst(unsigned int iw){
	unsigned int first;
	unsigned int second;
	unsigned int third;
	first = (iw >> 4) & 0b1111;
	second = (iw >> 21) & 0b1;
	third = (iw >> 22) & 0b111111;
	return (first == 0b1001 && second == 0 && third == 0);
}

void armemu_mul(struct arm_state *state){
	unsigned int iw;
	unsigned int rd, rm, rs;
	//mul counts as 
	state->metrics.data++;	
	iw = *((unsigned int *) state->regs[PC]);
	rd = (iw >> 16) & 0xF;
	rm = iw & 0xF;
	rs = (iw >> 8) & 0xF;
	state->regs[rd] = state->regs[rs] * state->regs[rm];
	if(rd != PC){
		state->regs[PC] = state->regs[PC]+4;
	}
}

bool is_bx_inst(unsigned int iw){
    unsigned int bx_code;

    bx_code = (iw >> 4) & 0x00FFFFFF;

    return (bx_code == 0b000100101111111111110001);
}

void armemu_bx(struct arm_state *state){
    unsigned int iw;
    unsigned int rn;
	state->metrics.branches++;
	state->metrics.branches_taken++;

    iw = *((unsigned int *) state->regs[PC]);
    rn = iw & 0b1111;

	state->regs[PC] = state->regs[rn];
	if(state->regs[LR] > 0){
		state->regs[PC] = state->regs[LR];
		state->regs[LR] = 0;
	}	
   }

bool is_data_processing_inst(unsigned int iw){
	unsigned int op;
 	op = (iw >> 26) & 0b11;
	return (op == 0);
}

int get_bit(unsigned int num, int position){
//	printf("the bit: %u", ((num >> position)&0b1));
	return ((num >> position)&0b1);
}

unsigned int sign_extend(unsigned int iw, int position){
	unsigned int a = 0;
	if(position == 23){
		if(get_bit(iw,23) == 1){
			a = 0xFF000000|(iw & 0xFFFFFF);
		}
		else{
			a = (iw & 0xFFFFFF);
		}
	}
	if(position == 7){
		if(get_bit(iw,7) == 1){
			a = 0xFFFFFFF0|(iw & 0xFF);
		}
		else{
			a = (iw & 0xFF);
		}
	}
	return (a);
}

void armemu_cmp(struct arm_state *state){
	unsigned int iw = *((unsigned int *) state->regs[PC]);
	unsigned int I = (iw >> 25) & 0b1;
	unsigned int rn = (iw >> 16) & 0xF;
	unsigned int rm = sign_extend(iw, 7);
    	if(I == 0){	
		rm = state->regs[iw & 0xF];
	}
	int a = (int)state->regs[rn];
	int b = (int)rm;
	long long al = (long long) state->regs[rn];
	long long bl = (long long) rm;
	unsigned int result = state->regs[rn] - rm;
	int r = a - b;	
	state->cpsr[0] = (r < 0);	
	state->cpsr[1] = (result == 0);
	state->cpsr[2] = (rm > state->regs[rn]);
	long long rl = al - bl;
	state->cpsr[3] = (rl > MAX_POS); // or less than max neg
	if(rl >> 31 == 0b1){
		state->cpsr[3] = (rl < MAX_NEG); // less because neg
	}	
}

void armemu_data_processing(struct arm_state *state){
   	unsigned int iw;
   	unsigned int rd, rn, rm, opCode, I;
	state->metrics.data = state->metrics.data + 1;
	iw = *((unsigned int *) state->regs[PC]);
	I = (iw >> 25) & 0b1;
	rd = (iw >> 12) & 0xF;
    	rn = (iw >> 16) & 0xF;
	rm = sign_extend(iw, 7);
    	if(I == 0){	
		rm = state->regs[iw & 0xF]; //so that it's compatible with I
	}
	opCode = (iw >> 21) & 0xF;
	if(opCode == ADD){
    		state->regs[rd] = state->regs[rn] + rm;
	}
	else if(opCode == SUB){
		state->regs[rd] = state->regs[rn] - rm;
	}
	else if(opCode == MOV){
		state->regs[rd] = rm;	
	}
	else if(opCode == CMP){	
		unsigned int *p = state->cpsr;
		armemu_cmp(state);
	}
	else{
		printf("data_processing opCode not found");
	}
    	if (rd != PC) {
        	state->regs[PC] = state->regs[PC] + 4;
    	}
}

bool is_branch_inst(unsigned int iw){
	unsigned int op;
 	op = (iw >> 25) & 0b111;
	return (op == 0b101);
}

void armemu_branch(struct arm_state *state){
	unsigned int iw, cond, link, offset;
	state->metrics.branches = state->metrics.branches + 1;
	iw = *((unsigned int *) state->regs[PC]);
	link = (iw >> 24) & 0b1;
	cond = (iw >> 28) & 0xF;
	int b = 0;
	if(cond == 0b1110){
		b = 1;
	} 
	else if(cond == 0b0000){
		if(state->cpsr[1] == 1){
			b = 1;
		}
	} 
	else if(cond == 0b001){
		if(state->cpsr[1] == 0){
			b = 1;
		}
	}
	else if(cond == 0b1100){
		if(state->cpsr[0] == state->cpsr[3]){
			b = 1;
		}
	}
	else if(cond == 0b1011){
		if(state->cpsr[0] != state->cpsr[3]){
			b = 1;
		}
	}
	//link if true
	if(link == 1){
		state->regs[LR] = state->regs[PC] + 4;
	}
	//branch
	if(b){
		state->metrics.branches_taken = state->metrics.branches_taken + 1;
		offset = sign_extend(iw, 23);
		unsigned int byte_address = offset*4;
		state->regs[PC] = state->regs[PC]+8+ byte_address;
	}
	else{
		state->metrics.branches_not_taken = state->metrics.branches_not_taken + 1;        	
		state->regs[PC] = state->regs[PC] + 4;
	}
}

bool is_memory_inst(unsigned int iw){
	unsigned int op;
 	op = (iw >> 26) & 0b11;
	return (op == 0b01);
}

void armemu_memory(struct arm_state *state){
	unsigned int iw, I, B, L, U, rm, rn, rd, target;
	state->metrics.memory = state->metrics.memory + 1;  
	iw = *((unsigned int *) state->regs[PC]);
	I = (iw >> 25) & 0b1;
	U = (iw >> 23) & 0b1;
	B = (iw >> 22) & 0b1;
	L = (iw >> 20) & 0b1;
	rn = (iw >> 16) & 0xF;
	rd = (iw >> 12) & 0xF;
	rm = iw & 0xFFF;
	if(I){
		rm = state->regs[iw & 0xF];
	}

	if(U){
		target = (state->regs[rn] + rm);
	}
	else{
		target = (state->regs[rn] - rm);
	}

	if(L){//load
		if(B){
			int b = 0;
			state->regs[rd] = *((unsigned char*)target);
		}
		else{
			
			state->regs[rd] = *((unsigned int*)target);		}
	}
	else{//store
		*(unsigned int*)target= state->regs[rd];	
	}
	if(rd != PC){
		state->regs[PC] = state->regs[PC]+4;
	}
}

int get_slot_size(int size){
	int log2 = 0;
	int size1 = size;
	while((size1>>1) > 0){
		size1 = size1>>1;
		log2++;
	}
	return log2;
}

unsigned int get_slot(int size, unsigned int address){
	return (address/4)&size;
}

unsigned int get_tag(int log2, unsigned int address){
	return(address >> (log2+2));
}

void cache_request(struct arm_state *state, struct cache *c){
	unsigned int address;
	address = state->regs[PC];
	unsigned int slot, log2, tag;
	//unsigned int tag;
	get_slot_size(8);
	log2 = get_slot_size(c->size);
	slot = get_slot(log2, address);
	tag = get_tag(log2, address);
	c->requests++;
	if(c->slots[slot].v == 1){
		if(tag == c->slots[slot].tag){
			c->hits++;
		}
		else{
			c->misses++;
		}
	}
	else{
		c->misses++;
		c->slots[slot].v = 1;
		c->slots[slot].tag = tag;
	}	
} 

void print_cache(struct cache *c){
	printf("cache report\n");
	printf("Size: %d\nRequests : %d\nHits : %d\nMisses : %d\n", c->size, c->requests, c->hits, c->misses);
}

void armemu_one(struct arm_state *state, struct cache *c){
    	unsigned int iw;
	state->metrics.count++;
    	iw = *((unsigned int *) state->regs[PC]);
	cache_request(state,c);
    	if (is_bx_inst(iw)) {
      		armemu_bx(state);
    	}
	else if(is_mul_inst(iw)){
		armemu_mul(state);
	}
	else if(is_memory_inst(iw)){
		armemu_memory(state);
	} 
	else if(is_data_processing_inst(iw)){
		armemu_data_processing(state);
	}
	else if(is_branch_inst(iw)){
		armemu_branch(state);
	}
	else{
	printf("not found");
	}
}

unsigned int armemu(struct arm_state *state, struct cache *c){
    /* Execute instructions until PC = 0 */
    /* This happens when bx lr is issued and lr is 0 */
    while (state->regs[PC] != 0) {
        armemu_one(state, c);
    }
    return state->regs[0];
}      

void print_stats(struct arm_state *state){
	int count = state->metrics.count;
	int data = state->metrics.data;
	double data_p = (double)data/count*100;
	int memory = state->metrics.memory;
	double memory_p = (double)memory/count*100;
	int branches = state->metrics.branches;
	double branches_p = (double)branches/count*100;
	int branches_taken = state->metrics.branches_taken;
	double branches_taken_p = (double)branches_taken/count*100;
	int branches_not_taken = state->metrics.branches_not_taken;
	double branches_not_taken_p = (double)branches_not_taken/count*100;
	printf("\nDynamic Analysis:\n");
	printf("Number of instructions: %d\nData instructions: %d (%.0f%)\nMemory instructions: %d (%.0f%)\nBranch instructions: %d (%.0f%)\nBranches taken: %d (%.0f%)\nBranches not taken: %d (%.0f%)\n", count, data, data_p, memory, memory_p, branches, branches_p, branches_taken, branches_taken_p, branches_not_taken, branches_not_taken_p);
}          
  
int main(int argc, char **argv){
    	struct arm_state state;
    	unsigned int r, n;
	//default cache size	
	int c_size = 8;
	//parsing main arguments to check for requested cache size (-c #)
	if(argc > 2 && strlen(argv[1])> 1 && argv[1][0] == '-' && argv[1][1] == 'c'){
		c_size = atoi(argv[2]);
	}

	//4 examples for quad
	struct cache c;
	c.size=c_size;
	cache_init(&c, 0);
	printf("Quadratic Tests: \n\n");
	r = quadratic_c(2,2,2,2);
	printf("Quadratic_c : %d\n", r);
	r = quadratic_a(2,2,2,2);
	printf("Quadratic_a : %d\n", r);
	arm_state_init(&state, (unsigned int*) quadratic_a, 2, 2, 2, 2);
	r = armemu(&state,&c);
	printf("armemu(Quadratic_a) : %d\n", r);
	print_stats(&state);
	print_cache(&c);

	cache_init(&c,1);
	r = quadratic_c(2,3,4,5);
	printf("Quadratic_c : %d\n", r);
	r = quadratic_a(2,3,4,5);
	printf("Quadratic_a : %d\n", r);
	arm_state_init(&state, (unsigned int*) quadratic_a, 2, 3, 4, 5);
	r = armemu(&state,&c);
	printf("armemu(Quadratic_a) : %d\n", r);
	print_stats(&state);
	print_cache(&c);

	cache_init(&c,1);
	r = quadratic_c(-3,-2,-5,-9);
	printf("Quadratic_c : %d\n", r);
	r = quadratic_a(-3,-2,-5,-9);
	printf("Quadratic_a : %d\n", r);
	arm_state_init(&state, (unsigned int*) quadratic_a, -3, -2, -5, -9);
	r = armemu(&state,&c);
	printf("armemu(Quadratic_a) : %d\n", r);
	print_stats(&state);
	print_cache(&c);
	
	cache_init(&c,1);
	r = quadratic_c(-2,-2,-2,-2);
	printf("Quadratic_c : %d\n", r);
	r = quadratic_a(-2,-2,-2,-2);
	printf("Quadratic_a : %d\n", r);
	arm_state_init(&state, (unsigned int*) quadratic_a, -2, -2, -2, -2);
	r = armemu(&state,&c);
	printf("armemu(Quadratic_a) : %d\n", r);
	print_stats(&state);
	print_cache(&c);

	cache_init(&c,1);
	r = quadratic_c(0,0,0,0);
	printf("Quadratic_c : %d\n", r);
	r = quadratic_a(0,0,0,0);
	printf("Quadratic_a : %d\n", r);
	arm_state_init(&state, (unsigned int*) quadratic_a, 0, 0, 0, 0);
	r = armemu(&state,&c);
	printf("armemu(Quadratic_a) : %d\n", r);
	print_stats(&state);
	print_cache(&c);


	printf("\n");
	printf("Sum Array Tests: \n\n");
	
	int array[] = {1,2,3,4,5};
	n = sizeof(array)/sizeof(int);
	cache_init(&c,1);
	r = sum_array_c(array, n);
	printf("Sum_array_c : %d\n", r);
	r = sum_array_a(array, n);
	printf("Sum_array_a : %d\n", r);
	arm_state_init(&state, (unsigned int*) sum_array_a, (unsigned int) array, n, 0, 0);
	r = armemu(&state,&c);
	printf("armemu(Sum_array_a) : %d\n", r);
	print_stats(&state);
	print_cache(&c);

	
	int array1[] = {4,3,2,31,4,3,2,4,2,1,3};
	n = sizeof(array1)/sizeof(int);
	cache_init(&c,1);
	r = sum_array_c(array1, n);
	printf("Sum_array_c : %d\n", r);
	r = sum_array_a(array1, n);
	printf("Sum_array_a : %d\n", r);
	arm_state_init(&state, (unsigned int*) sum_array_a, (unsigned int) array1, n, 0, 0);
	r = armemu(&state,&c);
	printf("armemu(Sum_array_a) : %d\n", r);
	print_stats(&state);
	print_cache(&c);


	int array2[] = {-4,-3,-2,-31,-4,-3,-2,-4,-2,-1,-3};
	n = sizeof(array2)/sizeof(int);
	cache_init(&c,1);
	r = sum_array_c(array2, n);
	printf("Sum_array_c : %d\n", r);
	r = sum_array_a(array2, n);
	printf("Sum_array_a : %d\n", r);
	arm_state_init(&state, (unsigned int*) sum_array_a, (unsigned int) array2, n, 0, 0);
	r = armemu(&state,&c);
	printf("armemu(Sum_array_a) : %d\n", r);
	print_stats(&state);
	print_cache(&c);


	int array3[] = {-2,-2,-4};
	n = sizeof(array3)/sizeof(int);
	cache_init(&c,1);
	r = sum_array_c(array3, n);
	printf("Sum_array_c : %d\n", r);
	r = sum_array_a(array3, n);
	printf("Sum_array_a : %d\n", r);
	arm_state_init(&state, (unsigned int*) sum_array_a, (unsigned int) array3, n, 0, 0);
	r = armemu(&state,&c);
	printf("armemu(Sum_array_a) : %d\n", r);
	print_stats(&state);
	print_cache(&c);

	int array4[1000];
	for(int i = 0; i < 1000; i = i + 2){
		array4[i] = 1;
		array4[i+1] = -1;
	}
	n= sizeof(array4)/sizeof(int);
	cache_init(&c,1);
	r = sum_array_c(array4, n);
	printf("Sum_array_c : %d\n", r);
	r = sum_array_a(array4, n);
	printf("Sum_array_a : %d\n", r);
	arm_state_init(&state, (unsigned int*) sum_array_a, (unsigned int) array4, n, 0, 0);
	r = armemu(&state,&c);
	printf("armemu(Sum_array_a) : %d\n", r);
	print_stats(&state);
	print_cache(&c);

	printf("\n");
	printf("Find Max Tests: \n\n");

	cache_init(&c,1);
	int array9[1000];
	for(int i = 0; i < 1000; i = i + 2){
		array9[i] = i;
		array9[i+1] = i+1; 
	}
	n = sizeof(array9)/sizeof(int);

	r= find_max_c(array9,n);
	printf("Find_max_c : %d\n", r); 
	r= find_max_a(array9,n);
	printf("Find_max_a : %d\n", r);
	arm_state_init(&state, (unsigned int*) find_max_a, (unsigned int) array9, n, 0, 0);
	r = armemu(&state,&c);
	printf("armemu(Find_max_a) : %d\n", r);
	print_stats(&state);
	print_cache(&c);


	cache_init(&c,1);
	int array6[] = {1,2,3,4,5,10,22,33};
	n = sizeof(array6)/sizeof(int);

	r= find_max_c(array6,n);
	printf("Find_max_c : %d\n", r); 
	r= find_max_a(array6,n);
	printf("Find_max_a : %d\n", r);
	arm_state_init(&state, (unsigned int*) find_max_a, (unsigned int) array6, n, 0, 0);
	r = armemu(&state,&c);
	printf("armemu(Find_max_a) : %d\n", r);
	print_stats(&state);
	print_cache(&c);


	cache_init(&c,1);	
	int array7[] = {-1,-2,-3,-4,-5,-10,-22,-33};
	n = sizeof(array7)/sizeof(int);

	r= find_max_c(array7,n);
	printf("Find_max_c : %d\n", r); 
	r= find_max_a(array7,n);
	printf("Find_max_a : %d\n", r);
	arm_state_init(&state, (unsigned int*) find_max_a, (unsigned int) array7, n, 0, 0);
	r = armemu(&state,&c);
	printf("armemu(Find_max_a) : %d\n", r);
	print_stats(&state);
	print_cache(&c);


	cache_init(&c,1);
	int array8[] = {-10,-22,-33};
	n = sizeof(array8)/sizeof(int);

	r= find_max_c(array8,n);
	printf("Find_max_c : %d\n", r); 
	r= find_max_a(array8,n);
	printf("Find_max_a : %d\n", r);
	arm_state_init(&state, (unsigned int*) find_max_a, (unsigned int) array8, n, 0, 0);
	r = armemu(&state,&c);
	printf("armemu(Find_max_a) : %d\n", r);
	print_stats(&state);
	print_cache(&c);
	

	cache_init(&c,1);
	int array5[] = {0,0,0,0,0};
	n = sizeof(array5)/sizeof(int);

	r= find_max_c(array5,n);
	printf("Find_max_c : %d\n", r); 
	r= find_max_a(array5,n);
	printf("Find_max_a : %d\n", r);
	arm_state_init(&state, (unsigned int*) find_max_a, (unsigned int) array5, n, 0, 0);
	r = armemu(&state,&c);
	printf("armemu(Find_max_a) : %d\n", r);
	print_stats(&state);
	print_cache(&c);


	printf("\n");
	printf("Fibonacci Iteration Tests: \n\n");

	n = 20;
	r = fib_iter_c(n);
	printf("Fib_iter_c : %d\n", r);
	
	r = fib_iter_a(n);
	printf("Fib_iter_a : %d\n", r);
	cache_init(&c,1);
	arm_state_init(&state, (unsigned int*) fib_iter_a, n, 0, 0, 0);
	r = armemu(&state,&c);
	printf("armemu(Fib_iter_a) : %d\n", r);
	print_stats(&state);
	print_cache(&c);
	
	printf("\n");
	printf("Fibonacci Recursive Tests: \n\n");
	
	
	n = 20;
	r = fib_rec_c(n);
	printf("Fib_rec_c : %d\n", r);
	
	r = fib_rec_a(n);
	printf("Fib_rec_a : %d\n", r);

	cache_init(&c,1);
	arm_state_init(&state, (unsigned int*) fib_rec_a, n, 0, 0, 0);
	r = armemu(&state,&c);
	printf("armemu(Fib_rec_a) : %d\n", r);
	print_stats(&state);
	print_cache(&c);	

	printf("\n");
	printf("Strlen Tests \n\n");

	char s[] = {'a','b','c', 'd', 'e', 'f', 'g'};
	r = strlen_c(s);
	printf("Strlen_c : %d\n", r);
		
	r = strlen_a(s);
	printf("Strlen_a : %d\n", r);
	
	arm_state_init(&state, (unsigned int*) strlen_a, (unsigned int) s, 0, 0, 0);
	cache_init(&c,1);
	r = armemu(&state,&c);
	printf("armemu(Strlen_a) : %d\n", r);
	print_stats(&state);
	print_cache(&c);


	char s2[] = {'h','e','l', 'l', 'o'};
	r = strlen_c(s2);
	printf("Strlen_c : %d\n", r);
	
		
	r = strlen_a(s2);
	printf("Strlen_a : %d\n", r);
	
	arm_state_init(&state, (unsigned int*) strlen_a, (unsigned int) s2, 0, 0, 0);
	cache_init(&c,1);
	r = armemu(&state,&c);
	printf("armemu(Strlen_a) : %d\n", r);
	print_stats(&state);
	print_cache(&c);


	char s3[] = {'a','b','c'};
	r = strlen_c(s3);
	printf("Strlen_c : %d\n", r);
		
	r = strlen_a(s3);
	printf("Strlen_a : %d\n", r);
	
	arm_state_init(&state, (unsigned int*) strlen_a, (unsigned int) s3, 0, 0, 0);
	cache_init(&c,1);
	r = armemu(&state,&c);
	printf("armemu(Strlen_a) : %d\n", r);
	print_stats(&state);
	print_cache(&c);


	char s4[] = {'a','b','c', 'd', 'e', 'f', 'g', 'f', 'f', 'f', 'f'};
	r = strlen_c(s4);
	printf("Strlen_c : %d\n", r);
		
	r = strlen_a(s4);
	printf("Strlen_a : %d\n", r);

	arm_state_init(&state, (unsigned int*) strlen_a, (unsigned int) s4, 0, 0, 0);
	cache_init(&c,1);
	r = armemu(&state,&c);
	printf("armemu(Strlen_a) : %d\n", r);
	print_stats(&state);
	print_cache(&c);



	char s5[] = {'a'};
	r = strlen_c(s5);
	printf("Strlen_c : %d\n", r);
		
	r = strlen_a(s5);
	printf("Strlen_a : %d\n", r);

	arm_state_init(&state, (unsigned int*) strlen_a, (unsigned int) s5, 0, 0, 0);
	cache_init(&c,1);
	r = armemu(&state,&c);
	printf("armemu(Strlen_a) : %d\n", r);
	print_stats(&state);
	print_cache(&c);
    	return 0;
}

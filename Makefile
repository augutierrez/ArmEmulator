PROGS = armemu

OBJS_ARMEMU = quadratic_a.o sum_array_a.o strlen_a.o find_max_a.o fib_iter_a.o fib_rec_a.s quadratic_c.o sum_array_c.o find_max_c.o fib_iter_c.o fib_rec_c.o strlen_c.o  

CFLAGS = -g

%.o : %.s
	as -o $@ $<

%.o : %.c
	gcc -c ${CFLAGS} -o $@ $<

all : ${PROGS}

armemu : armemu.c ${OBJS_ARMEMU}
	gcc ${CFLAGS} -o $@ $^

clean :
	rm -rf ${PROGS} ${OBJS_ARMEMU}

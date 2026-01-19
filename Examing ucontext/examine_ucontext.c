#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>

#ifndef __x86_64__
#error "Do this project on a 64-bit x86-64 linux machine"
#endif /* __x86_64__ */

#if __WORDSIZE != 64
#error "word size should be 64 bits"
#endif

/* zero out the context */
ucontext_t my_context = { 0 };

static int
get_start_end(long *startp, long *endp)
{
	char filename[128];
	char line[256];
	FILE *f;
	char ex = 0;
        int matches = 0;
	
	sprintf(filename, "/proc/%d/maps", getpid());
	f = fopen(filename, "r");
	if (f == NULL) {
		fprintf(stderr, "Could not open file %s.\n", filename);
		return -1;
	}

	do {
		/* read line */
		if (fgets(line, sizeof(line), f) != NULL) {
		        matches = sscanf(line, "%lx-%lx %*c%*c%c%*c ", startp, endp, &ex);
		} else {
			matches = EOF;
		}
	} while (ex != 'x' && matches > 0);

	if (ex != 'x') {
		fprintf(stderr, "Did not find an executable region in the process maps.\n");
		*startp = *endp = 0;

		return -1;
	}
	
	fclose(f);
	return 0;
}

static void
call_setcontext(ucontext_t * context)
{
	int err = setcontext(context);
	assert(!err);
}

int
main(int argc, char **argv)
{
	long start, end;
	int err;
	
	/* we declare setcontext_called to be volatile so that the compiler will
	 * make sure to store it on the stack and not in a register. This is
	 * ESSENTIAL, or else the code in this function may run in an infinite
	 * loop (you can try this by removing the volatile keyword, and
	 * compiling this file with the "-O2" flag to the gcc compiler, by
	 * changing CFLAGS in the Makefile).
	 *
	 * QUESTION: why must setcontext_called be stored on the stack, and not
	 * in a register? You will need to look at the code below, and
	 * understand how getcontext and setcontext work to answer this
	 * question. */
	volatile int setcontext_called = 0;


	/* Get context: make sure to read the man page of getcontext in detail,
	 * or else you will not be able to answer the questions below. */
	err = getcontext(&my_context);
	assert(!err);

	/* QUESTION: which fields of my_context changed due to the getcontext call
	 * above? Hint: It will help to run the program using gdb and put a
	 * breakpoint at entry to main and before and after the calls to
	 * getcontext().
	 * - Use "info registers" to see the values of the registers.
	 * - Use "next"/"step" to advance to the next line of code.
	 * - Use "print my_context" to see the values stored in my_context.
	 *   Compare them with the output of "info registers".
	 * - Use "ptype my_context" so see the type/fields of my_context */

	printf("%s: setcontext_called = %d\n", __FUNCTION__, setcontext_called);
	if (setcontext_called == 1) {
		/* QUESTION: will we get here? Why or why not? */
		exit(0);
	}

	get_start_end(&start, &end);
	printf("start = 0x%lx\n", start);
	printf("end = 0x%lx\n", end);

	/*
	 * DO NOT CHANGE/ADD ANY CODE ABOVE THIS POINT.
	 */

	/*
	 * Replace the -1 in each printf() with a variable or expression that
	 * will print the expected value. Do not use constants!
	 */


	/* 1. Show size of ucontext_t structure. Hint: use sizeof(). */
	printf("ucontext_t size = %ld bytes\n", (long int)sizeof(ucontext_t));

	/* now, look inside of the context you just saved. */

	/* First, think about code. 
	 * Note that the program counter is called RIP in x86-64 
	 */
	
	/* 2. Show the memory address of the main() function. */
	printf("memory address of main() = 0x%lx\n", (unsigned long)&main);
	
	/* 3. Show the memory address of the program counter saved in my_context. 
	 * Hint: the uc_mcontext field of the ucontext_t struct holds
	 * machine-specific saved context, including the machine registers. 
	 * See /usr/include/x86_64-linux-gnu/sys/ucontext.h on teach.cs machines
	 * for details. You will find defined constants that you can use to index
	 * into the gregs array (the saved general CPU registers) to get the saved
	 * state of specific registers. 
	 */
	printf("memory address of the program counter (RIP) saved "
	       "in my_context = 0x%lx\n",
	       (unsigned long)my_context.uc_mcontext.gregs[REG_RIP]);

	/* What is the distance (i.e., number of bytes) between the address of 
	 * main and the address of the program counter saved in my_context?
	 * (do not add a print statement for this - use gdb to do the math)
	 */
	
	/* Now, think about parameters. */

	/* (Not marked) Show the value of argc passed to main(). */
	printf("argc = %d\n", argc);

	/* 4. Show the memory address of the argv array passed to main(). */
	printf("argv = %p\n", (void *)&argv);
	
	/* QUESTIONS: How are the parameters argc and argv passed into the main 
	 * function? 
	 * Are there any saved registers in my_context that store the parameter
	 * values above. Why or why not? Hint: Use gdb, and then run
	 * "disassemble main" in gdb, and then scroll up to see the beginning of
	 * the main function. 
	 */ 

	/* Now, think about the stack. */
	/* QUESTIONS: Are setcontext_called and err stored on the stack? Does the
	 * stack grow up or down? What stack-related data is stored in
	 * my_context.uc_mcontext.gregs[]? 
	 */

	/* 5. Show the memory address of the variable setcontext_called. */
	printf("memory address of the variable setcontext_called = %p\n",
	       (void *)&setcontext_called);

	/* 6. Show the memory address of the variable err. */
	printf("memory address of the variable err = %p\n",
	       (void *)&err);

	/* 7. Show the distance (in bytes) between setcontext_called and err. */
	printf("number of bytes pushed to the stack between setcontext_called "
	       "and err = %ld\n", (unsigned long)((char *)&err - (char *)&setcontext_called));

	/* 8. Show the value of the stack pointer register saved in my_context. */
	printf("stack pointer register (RSP) stored in my_context = 0x%lx\n",
	       (unsigned long)my_context.uc_mcontext.gregs[REG_RSP]);

	/* 9. Show the distance (in bytes) between err and the saved RSP. */
	printf("number of bytes between err and the saved stack in my_context "
	       "= %ld\n", (unsigned long)((char *)&err - (char *)my_context.uc_mcontext.gregs[REG_RSP]));

	/* QUESTION: What is the value of the uc_stack field in my_context?
	 * Note that this field is used to store an alternate stack for use
	 * during signal handling, and is NOT the stack of the running thread. 
	 */

	/* 10. Show the value of the uc_stack field in my_context. */
	printf("value of uc_stack.ss_sp = 0x%lx\n",
	       (unsigned long)my_context.uc_stack.ss_sp);


	/* Now we will try to understand how setcontext works. */
	setcontext_called = 1;
	call_setcontext(&my_context);
	
	/* QUESTION: why does the program not fail at the assert below? */
	assert(0);
}


#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include <signal.h>

/* we will use this signal type for delivering "interrupts". */
#define SIG_TYPE SIGALRM

/* in preemptive mode, the interrupt will be delivered every 200 usec */
#define SIG_INTERVAL 200

void interrupt_init(int verbose);
void interrupt_end(void);

int interrupt_on(void);
int interrupt_off(void);
int interrupt_set(int enabled);

/* check if interrupt is enabled */
int interrupt_enabled(void);

/* turn off interrupts while printing */
int unintr_printf(const char *fmt, ...);

/* disable diagnostic messages for interrupts */
void interrupt_quiet(void);

/* waste CPU cycle for usecs (in microseconds) */
void spin(int usecs);

#endif

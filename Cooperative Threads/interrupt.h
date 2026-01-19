#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include <stdbool.h>

/*
 * placeholder for ppremptive thread assignment
 */

static inline bool interrupt_off(void) { return false; }
#define interrupt_on() interrupt_off()
#define interrupt_set(enable) (void)(enable)
#define interrupt_enabled() interrupt_off()
#define interrupt_init(preemptive) (void)(preemptive)
#define interrupt_end() 

#endif

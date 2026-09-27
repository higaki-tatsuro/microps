#ifndef PLATFORM_H
#define PLATFORM_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

extern int platform_init(void);
extern int platform_run(void);
extern int platform_shutdown(void);

/*
 * Memory
 */

extern void* memory_alloc(size_t size);
extern void memory_free(void* ptr);

/*
 * Lock
 */

typedef pthread_mutex_t lock_t;

#define LOCK_INITIALIZER PTHREAD_MUTEX_INITIALIZER

extern int lock_init(lock_t* lock);
extern int lock_acquire(lock_t* lock);
extern int lock_release(lock_t* lock);

/*
 * Random
 */

extern uint16_t random16(void);

#include "intr.h"
#include "sched.h"
#include "timer.h"

#endif

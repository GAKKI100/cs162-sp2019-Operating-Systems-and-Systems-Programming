#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include "lib/kernel/hash.h"

#include "threads/synch.h"
#include "threads/palloc.h"

/* Functions for frame manipulation */

void vm_frame_init(void);
void *vm_frame_allocate(enum palloc_flags, void *);
void vm_frame_free(void *);
void vm_frame_unpin(void *);

#endif /* vm/frame.h */

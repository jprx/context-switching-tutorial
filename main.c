#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

typedef uint64_t u64;
struct thread_t;

extern void cswitch(struct thread_t *old, struct thread_t *new);

// All callee-saved registers:
// Need to save any callee-saved registers plus the return address
typedef struct regs_t {
  // SW pushes these:
  u64 rbx;
  u64 rbp;
  u64 r12;
  u64 r13;
  u64 r14;
  u64 r15;

  // HW pushes this (from call inst):
  u64 rip;
} regs_t;

typedef struct thread_t {
  u64               saved_sp; // points to a regs_t on the stack
  uintptr_t         stack_page;
  struct thread_t  *next;
} thread_t;

struct thread_t *cur_thread;

thread_t *create_thread(void (*entrypoint)(void)) {
  regs_t *init_regs;
  thread_t *t;

  // 1. Allocate a thread_t and its stack
  t = malloc(sizeof(*t));
  t->stack_page = (uintptr_t)malloc(0x1000);

  // 2. Initialize all thread vars
  t->next = NULL;
  t->saved_sp = (uintptr_t)t->stack_page + (0x1000 - 0x10);

  // 3. Push a regs_t to the new stack and initialize it
  //    (recall the stack grows towards lower addresses)
  t->saved_sp -= sizeof(regs_t);
  init_regs = (regs_t*)t->saved_sp;
  init_regs->rip = (uintptr_t)entrypoint;

  printf("Created thread at 0x%lX with entrypoint 0x%lX and stack at 0x%lX\n", (u64)t, init_regs->rip, t->stack_page);
  return t;
}

void scheduler() {
  thread_t *prev = cur_thread;
  thread_t *next = cur_thread->next;
  cur_thread = cur_thread->next;
  cswitch(prev, next);
}

void func1() {
  int local_var = 0x1111;
  while (true) {
    printf("in function 1 (0x%X)\n", local_var);
    scheduler();
  }
}

void func2() {
  int local_var = 0x2222;
  while (true) {
    printf("in function 2 (0x%X)\n", local_var);
    scheduler();
  }
}

void start_scheduler(thread_t *init_thread) {
  // cswitch will push a regs_t to the current stack, and then write the stack
  // pointer into offset 0 of the previous thread_t. We are just starting the
  // scheduler so there isn't one of our thread_t's currently running. We still
  // need a thread_t to save the "previous state" into, but we don't need to
  // restore it since we never need to return here. So, just setup a thread_t
  // on the stack here and allow cswitch to save the old sp into it; after the
  // first context switch this stack frame is never touched again so it doesn't
  // matter.
  thread_t scratch_thread;
  cur_thread = init_thread;
  cswitch(&scratch_thread, init_thread);
}

int main() {
  thread_t *threads[4];
  threads[0] = create_thread(func1);
  threads[1] = create_thread(func2);
  threads[2] = create_thread(func1);
  threads[3] = create_thread(func2);

  threads[0]->next = threads[1];
  threads[1]->next = threads[2];
  threads[2]->next = threads[3];
  threads[3]->next = threads[0];
  start_scheduler(threads[0]);
}

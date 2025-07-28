.section .text
.code64
.intel_syntax noprefix

# void cswitch(thread_t *prev, thread_t *next)
# rdi: current thread struct
# rsi: next thread struct
# thread_t.saved_sp assumed to be first field of a thread_t (at offset 0)
.global cswitch
cswitch:
  # 1. Push state to current stack
  # (recall RIP was pushed by the call instruction that brought us here)
  push r15
  push r14
  push r13
  push r12
  push rbp
  push rbx

  # 2. Store stack pointer in prev thread's struct
  mov [rdi], rsp

  # 3. Load stack pointer from next thread's struct
  mov rsp, [rsi]

  # 4. Pop state from new stack
  pop rbx
  pop rbp
  pop r12
  pop r13
  pop r14
  pop r15
  ret

# Context Switching Demo

A simple userspace scheduler that demonstrates the "right way" to implement
context switching. This implementation is written for `x86_64`, but this
approach works for any ISA. See below for a description of what this code is
doing.

"Context switching" allows one CPU core to run multiple threads. A context
switch is the process of switching from one thread to another.

**Context switching** is as easy as 1, 2, 3, 4:
1. Push the callee saved registers and return address to current thread's stack.
2. Save the current thread's stack pointer (where we just pushed the registers) to the thread's struct.
3. Load the next thread's stack pointer from its struct. It points to the regs for the next thread.
4. Pop the callee saved registers from the new stack, and return.

You can see the example context switch code in `cswitch.s`. `main.c` contains
the logic that creates threads and starts a scheduler that uses `cswitch` to
switch between them.

## Usage

- Build: `make`
- Run: `./demo`

## Details

A **thread** is a unit of execution. Its state consists of a stack plus the
current values in all CPU registers. Each thread needs its own stack- threads
can't share a stack. This is because the stack holds local variables and the
local function call path. A thread is usually represented by a C struct- see
`thread_t` in `main.c`.

The **return address** is the location a function jumps to after it concludes.
The `ret` instruction jumps to the return address. Later, you'll see how we use
the return address to direct where threads go when they start running. We can
set the initial return address of a thread's saved registers to point to
whatever function we want the thread to run when it starts up.

The CPU can only run one thread at a time. The **scheduler**'s job is to switch
which thread the CPU is running. When a thread is put to sleep, we need to save
the thread's state so that it can be restored later. Specifically, we need to
save the CPU registers and the stack. We use a struct called `regs_t` that
holds all the register values, and we write it to the thread's stack. Saving
the stack just means saving the current stack pointer, which we put in the
thread's struct rather than on the stack, because we need to be able to find it
later. Then, we need to load the CPU registers and stack of the next thread,
which we can find via the next thread's saved stack pointer. Later you will see
that `regs_t` only needs to hold a subset of the CPU registers (specifically
just the callee saved ones).

We will implement a context switch using a single "magic" function called
`schedule()`. `schedule()` puts this thread to sleep and selects a new one to
run. Eventually the original thread will be woken up. It will look like
`schedule()` just returned and the original thread will continue running right
after the `schedule()` call. All of the original state will have been restored,
just like magic.

The `schedule()` function picks a new thread to schedule and then calls a lower
level context switch method called `cswitch` which actually performs the
context switch. `cswitch` must be written in pure assembly- **NOT** inline
assembly- as we need complete control over the stack layout.

The **calling convention** for each CPU specifies which registers are saved by
the caller and which are saved by the callee during a function call. When a
function is called, it only needs to preserve any callee saved registers it
changes. Since `cswitch` is a function, it needs to follow the same rules.

We can't know which registers the next thread will change, so we assume they
all will change. Therefore, `cswitch` should save every callee saved register.
If a caller saved register's value is important, then whatever function called
us will have saved it, so we don't have to. But we do need to save the callee
saved registers.

In addition to all callee saved registers, we also need to save the return
address / link register. On X86, the return address is pushed to the stack by
hardware during a `call` instruction (yet another reason every thread needs its
own stack) so we don't need to manually save it. However, on other ISAs like
ARM or RISCV you'd need to save the return address register yourself, as the
next thread will definitely change it and we need its original value to get
back to whatever called `cswitch` when we restore this thread. Let's call the
callee saved registers plus return address the "saved registers" (even if the
return address isn't a register, like on X86).

It's up to you where you save the saved registers. You could put them in the
thread's struct for instance. However, I feel a very natural place to put them
is just on the thread's stack before you switch to the next thread, so that's
how I do it. We just need to save the stack pointer value in the thread struct
so we can find the stack again later.

Our `cswitch` does the following:
1. Save callee saved regs + return address to the current stack.
2. Save the current stack pointer in the previous thread's `thread_t` struct.
3. Restore the next stack pointer from the new thread's `thread_t` struct.
4. Restore callee saved regs from new stack; `ret` jumps to the return address (read from the stack).

We assume every sleeping thread's stack is the result of a `cswitch`. Namely,
the saved stack pointer points to a `regs_t` as the result of `cswitch`'s step
2. This makes sense if the thread was put to sleep by a `cswitch` call, but how
do we create a new thread from scratch that `cswitch` can run?

### Creating a New Thread
`cswitch` is simple: its only job is to save the current registers to the stack
and load the next registers from the next stack. If we want to create a new
thread that `cswitch` can run, we need to setup a `regs_t` on the new thread's
stack with initial values for the registers. That way, when `cswitch` picks up
our new thread, the CPU registers will be set correctly to allow the next
thread to begin running.

The only `regs_t` value that we really care about is the return address, which
is where the thread will begin running from when it starts up. The rest of the
callee saved registers don't matter (see the ISA's calling convention). So, all
we need to do is make sure the saved return address points to where we want the
new thread to go when it starts.

When `cswitch` schedules this new thread, it'll pop the `regs_t` we setup off
the stack. When `ret` runs at the end of `cswitch`, it'll jump to whatever
return address we specified. Since we set the return address to point to the
start of a function, it will begin running the thread at that function.

### Special Case: Starting the First Thread

Starting the very first thread requires special consideration because there
isn't a running thread to save the state of, but `cswitch` always tries to save
the current state into a thread struct. Since there isn't a thread struct to
use, we make a fake one that `cswitch` uses as the previous thread. We only
have to do this for the very first context switch that starts the scheduler.

Let's call the stack we entered on the "bootstrap" stack. Once we start the
first thread we actually never want to return to the bootstrap context, so we
don't actually need to save any state. However, the very first context switch
(`cswitch`) doesn't know that. It will still try to save the state somewhere
thinking that we are already running a thread.

There are two options. First, you could modify `cswitch` to account for the
case where the previous thread is NULL, and skip steps 1 and 2 where it saves
the old register state and the old stack pointer. However, this adds complexity
to `cswitch` just to handle this single special case. `cswitch` should be fast
since it runs a lot. A cleaner approach is to just setup a fake `thread_t` for
`cswitch` to save into just for the first context switch.

Let's revisit the context switch steps:
1. Save old regs to current stack
2. Save current stack to `thread_t`
3. Restore next stack from `thread_t`
4. Restore old regs from new stack

In step 1, we will just push the registers to the bootstrap stack. That's no
problem; the bootstrap stack isn't needed after this context switch anyways
since we never come back here, so it's fine. We can write whatever here!

In step 2, we write whatever the stack pointer is into the previous `thread_t`.
We can't make the previous `thread_t` NULL because this write would cause a
segfault. Instead, we can just put a `thread_t` on the bootstrap stack and use
that as the previous `thread_t`. `cswitch` will write the stack pointer into
it, and then after we swap to the stack for the first thread we never return
here.

This `thread_t` is a local variable so it'll be on the bootstrap stack, which
once again, we don't care about after the first `cswitch`. So, it's fine!

You can see the logic for this in `start_scheduler()` in the code.

### Thread Saved State

The only state you really need to save for each thread is its stack pointer.
The stack pointer points to a `regs_t` that will be popped off by `cswitch` the
next time this thread is scheduled. This `regs_t` could have been placed there
by us, giving the thread an initial location to begin running via the return
address, or could have been placed by `cswitch` the last time this thread was
run.

If you decided to save the registers to the thread's struct rather than on the
stack, then you'd obviously need to save the callee saved registers and return
address in the thread's struct too. But since the return address is already on
the stack (at least for X86) and we need to save the stack pointer anyways, why
not just push all the registers to the stack and keep it simple?

You can see the saved state of a `thread_t` in `main.c`.

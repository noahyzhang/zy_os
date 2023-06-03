#include "thread/thread.h"
#include "lib/string.h"
#include "kernel/global.h"
#include "kernel/memory.h"

// 由 kernel_thread 去执行 func(func_arg)
static void kernel_thread(thread_func* func, void* func_arg) {
    func(func_arg);
}

void thread_create(struct task_struct* pthread, thread_func func, void* func_arg) {
    // 预留中断使用栈的空间
    pthread->self_kernel_stack -= sizeof(struct intr_stack);
    // 在留出线程栈空间
    pthread->self_kernel_stack -= sizeof(struct thread_stack);
    struct thread_stack* kernel_thread_stack = (struct thread_stack*)pthread->self_kernel_stack;
    kernel_thread_stack->eip = kernel_thread;
    kernel_thread_stack->function = func;
    kernel_thread_stack->func_arg = func_arg;
    kernel_thread_stack->ebp = 0;
    kernel_thread_stack->ebx = 0;
    kernel_thread_stack->esi = 0;
    kernel_thread_stack->edi = 0;
}

// 初始化线程基本信息
void init_thread(struct task_struct* pthread, char* name, int prio) {
    memset(pthread, 0, sizeof(*pthread));
    strncpy(pthread->name, name, strlen(name));
    pthread->status = TASK_RUNNING;
    pthread->priority = prio;
    // self_kernel_stack 是线程自己在内核态下使用的栈顶空间
    pthread->self_kernel_stack = (uint32_t*)((uint32_t)pthread + PAGE_SIZE);
    // 自定义的魔数
    pthread->stack_magic = 0x19971216;
}

struct task_struct* thread_start(char* name, int prio, thread_func func, void* func_arg) {
    struct task_struct* thread = get_kernel_pages(1);
    init_thread(thread, name, prio);
    thread_create(thread, func, func_arg);
    asm volatile("movl %0, %%esp; pop %%ebp; pop %%ebx; \
        pop %%edi; pop %%esi; ret"::"g"(thread->self_kernel_stack) : "memory");
    return thread;
}

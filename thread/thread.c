#include "thread/thread.h"
#include "lib/string.h"
#include "kernel/global.h"
#include "kernel/memory.h"
#include "lib/kernel/print.h"
#include "kernel/interrupt.h"
#include "kernel/debug.h"
#include "user_process/process.h"
#include "thread/sync.h"

#define MAIN_THREAD_PRIO_VALUE (31)

// 主线程 PCB
struct task_struct* main_thread;
// idle 线程 PCB
struct task_struct* idle_thread;
// 就绪队列，只存储准备运行的线程
struct list thread_ready_list;
// 所有任务队列，存储包括就绪的、阻塞的、正在执行的的线程
struct list thread_all_list;
// 分配 pid 锁
struct lock pid_lock;
// 用于保存队列中的线程节点
static struct list_elem* thread_tag;

extern void switch_to(struct task_struct* cur, struct task_struct* next);
extern void init(void);

static void idle(void* arg) {
    (void)arg;
    for (;;) {
        thread_block(TASK_BLOCKED);
        // 执行 hlt 时必须要保证目前处于开中断的情况下
        asm volatile("sti; hlt" : : : "memory");
    }
}

struct task_struct* running_thread(void) {
    uint32_t esp;
    asm ("mov %%esp, %0" : "=g" (esp));
    // 取 esp 整数部分，即 PCB 的起始地址
    return (struct task_struct*)(esp & 0xfffff000);
}

// 由 kernel_thread 去执行 func(func_arg)
static void kernel_thread(thread_func* func, void* func_arg) {
    // 执行 function 前要开中断，避免后面的时钟中断被屏蔽，而无法调度其他线程
    intr_enable();
    func(func_arg);
}

/**
 * @brief 分配 pid
 * 
 * @return pid_t 
 */
static pid_t allocate_pid(void) {
    static pid_t next_pid = 0;
    lock_acquire(&pid_lock);
    next_pid++;
    lock_release(&pid_lock);
    return next_pid;
}

/**
 * @brief fork 进程时为其分配 pid
 * 
 * @return pid_t 
 */
pid_t fork_pid(void) {
    return allocate_pid();
}

/**
 * @brief 创建线程，填充内容
 * 
 * @param pthread 
 * @param func 
 * @param func_arg 
 */
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
    pthread->pid = allocate_pid();
    strncpy(pthread->name, name, strlen(name));
    if (pthread == main_thread) {
        // main 函数也封装成一个线程，并且他一直是运行的，故将其直接设置为 TASK_RUNNING 的状态
        pthread->status = TASK_RUNNING;
    } else {
        pthread->status = TASK_READY;
    }
    pthread->priority = prio;
    // self_kernel_stack 是线程自己在内核态下使用的栈顶空间
    pthread->self_kernel_stack = (uint32_t*)((uint32_t)pthread + PAGE_SIZE);
    pthread->ticks = prio;
    pthread->elapsed_ticks = 0;
    pthread->pg_dir = NULL;
    // 预留标准输入输出
    pthread->fd_table[0] = 0;
    pthread->fd_table[1] = 1;
    pthread->fd_table[2] = 2;
    // 其余的全置为 -1
    for (uint8_t fd_idx = 3; fd_idx < MAX_FILES_OPEN_PER_PROC; fd_idx++) {
        pthread->fd_table[fd_idx] = -1;
    }
    // 任务的父进程默认为 -1（-1 表示没有父进程）
    pthread->parent_pid = -1;
    // 自定义的魔数，用于检测是否有栈溢出
    pthread->stack_magic = TASK_STACK_MAGIC_VALUE;
}

struct task_struct* thread_start(char* name, int prio, thread_func func, void* func_arg) {
    // asm volatile ("xchg %%bx, %%bx" ::);
    // put_str("thread_start\n");
    // asm volatile ("xchg %%bx, %%bx" ::);
    // put_str(name);
    // put_str("\n");
    // put_str(func_arg);
    // put_str("\n");
    // PCB 都位于内核空间，包括用户进程的 PCB 也是在内核空间
    struct task_struct* thread = get_kernel_pages(1);
    init_thread(thread, name, prio);
    thread_create(thread, func, func_arg);
    // 确保之前不在就绪队列中
    ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
    // 加入到就绪队列中
    list_append(&thread_ready_list, &thread->general_tag);

    // 确保之前不在所有任务队列中
    ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
    // 加入到所有任务队列中
    list_append(&thread_all_list, &thread->all_list_tag);

    // 不需要通过 ret 指令主动去更改 eip 的值了
    /*
    asm volatile("movl %0, %%esp; pop %%ebp; pop %%ebx; \
        pop %%edi; pop %%esi; ret" : : "g" (thread->self_kernel_stack) : "memory");
    */
    return thread;
}

/**
 * @brief 使 kernel 中的 main 函数变成主线程
 * 
 */
static void make_main_thread(void) {
    // main 线程早已经运行了，我们在 load.s 中进入内核时的 "mov esp, 0xc009f000"
    // 就是为 main 线程预留的栈，地址为 0xc009e000, 因此不需要通过 get_kernel_page 分配一页
    main_thread = running_thread();
    init_thread(main_thread, "main", MAIN_THREAD_PRIO_VALUE);
    // main 函数是当前线程，当前线程不在 thread_ready_list 中，所以只将其加到 thread_all_list 中
    ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));
    list_append(&thread_all_list, &main_thread->all_list_tag);
}

void schedule(void) {
    ASSERT(intr_get_status() == INTR_OFF);
    struct task_struct* cur = running_thread();
    if (cur->status == TASK_RUNNING) {
        // 如果此线程只是 CPU 时间片到了，将其加入到就绪队列尾部
        ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
        list_append(&thread_ready_list, &cur->general_tag);
        // 重置当前线程的 ticks
        cur->ticks = cur->priority;
        cur->status = TASK_READY;
    } else {
        // 如果此线程是非 RUNNING 状态，需要等待某个事件发生后才能上 CPU 运行
        // 那么就不需要将其加入到队列，让他等待后面的调度吧
    }
    // 如果就绪队列中没有可运行的任务，就唤醒 idle
    if (list_empty(&thread_ready_list)) {
        thread_unblock(idle_thread);
    }
    ASSERT(!list_empty(&thread_ready_list));
    thread_tag = NULL;
    // 将 thread_ready_list 队列中第一个就绪线程弹出，准备将其调度上 CPU
    thread_tag = list_pop(&thread_ready_list);
    struct task_struct* next = elem2entry(struct task_struct, general_tag, thread_tag);
    next->status = TASK_RUNNING;
    // 激活任务页表
    process_activate(next);
    switch_to(cur, next);
    return;
}

/**
 * @brief 当前线程将自己阻塞，并更新状态为 stat
 * 
 * @param stat 
 */
void thread_block(enum task_status stat) {
    // 只有 stat 取如下三种状态才不会被调度
    ASSERT(((stat == TASK_BLOCKED) || (stat == TASK_WAITING) || (stat == TASK_HANGING)));
    enum intr_status old_status = intr_disable();
    struct task_struct* cur_thread = running_thread();
    // 置其状态为 stat
    cur_thread->status = stat;
    // 将当前线程换下处理器，由于不会将此线程加入到就绪队列，所以不会被调度
    schedule();
    // 待当前线程被解除阻塞后才继续运行下面的 intr_set_status
    intr_set_status(old_status);
}

/**
 * @brief 将线程 pthread 解除阻塞
 * 
 * @param pthread 
 */
void thread_unblock(struct task_struct* pthread) {
    enum intr_status old_status = intr_disable();
    ASSERT(((pthread->status == TASK_BLOCKED) || (pthread->status == TASK_WAITING)
        || (pthread->status == TASK_HANGING)));
    if (pthread->status != TASK_READY) {
        // ASSERT 是调试阶段用的，运行阶段我们用 PANIC 返回错误信息
        ASSERT(!elem_find(&thread_ready_list, &pthread->general_tag));
        if (elem_find(&thread_ready_list, &pthread->general_tag)) {
            PANIC("thread_unlock: blocked thread in ready_list\n");
        }
        // 放到队列的最前面，让他尽快得到调度
        list_push(&thread_ready_list, &pthread->general_tag);
        pthread->status = TASK_READY;
    }
    intr_set_status(old_status);
}

/**
 * @brief 主动让出 CPU，换其他线程运行
 * 
 */
void thread_yield(void) {
    struct task_struct* cur = running_thread();
    enum intr_status old_status = intr_disable();
    ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
    list_append(&thread_ready_list, &cur->general_tag);
    cur->status = TASK_READY;
    schedule();
    intr_set_status(old_status);
}

void thread_init(void) {
    put_str("thread_init start\n");
    list_init(&thread_ready_list);
    list_init(&thread_all_list);
    lock_init(&pid_lock);
    // 先创建第一个用户进程：init
    // 放在第一个初始化，这是第一个进程，init 进程的 pid 为 1
    process_execute(init, "init");
    // 将当前 main 函数创建为线程
    make_main_thread();
    // 创建 idle 线程
    idle_thread = thread_start("idle", 10, idle, NULL);
    put_str("thread_init end\n");
}

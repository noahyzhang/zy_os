/**
 * @file thread.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-03
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef THREAD_THREAD_H_
#define THREAD_THREAD_H_

#include "lib/stdint.h"
#include "lib/kernel/list.h"
#include "kernel/memory.h"

// task_struct 中 stack_magic 的魔数值
#define TASK_STACK_MAGIC_VALUE (0x19971216)
// 任务名的长度
#define TASK_NAME_LEN 16
// 每个线程可以打开的文件数
#define MAX_FILES_OPEN_PER_PROC 8

// 函数类型
typedef void thread_func(void* arg);
typedef int16_t pid_t;

// 进程或者线程的状态
enum task_status {
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_WAITING,
    TASK_HANGING,
    TASK_DIED
};

/**
 * @brief 中断栈
 * 用于中断发生时保护程序（线程或者进程）的上下文环境
 * 进程或者线程被外部中断或软中断打断时，会按照此结构压入上下文、寄存器
 * intr_exit 中的出栈操作是此结构的逆操作
 * 此栈在线程自己的内核栈中位置固定，所在页的最顶端
 */
struct intr_stack {
    uint32_t vec_no;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    // 虽然 pushad 把 esp 也压入，但 esp 是不断变化的，所以会被 popad 忽略
    uint32_t esp_dummy;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;
    // 如下由 CPU 从低特权级进入高特权级时压入
    uint32_t err_code;
    void (*eip)(void);
    uint32_t cs;
    uint32_t eflags;
    void* esp;
    uint32_t ss;
};

/**
 * @brief 线程栈
 * 线程自己的栈，用于存储线程中待执行的函数
 * 此结构在线程自己的内核栈中位置不固定
 * 仅用在 switch_to 时保存线程环境
 * 实际位置取决于实际运行情况
 * 
 * ABI：Application Binary Interface，即应用程序二进制接口，ABI 规定的是更加底层的一套规则，属于编译方面的约定
 * 比如参数如何传递，返回值如何存储，系统调用的实现方式，目标文件格式和数据类型等
 * C 编译器就是按照这套 ABI 规则来编译 C 程序的，倘若我们全是用 C 语言来写程序，那就不用考虑 ABI 规则，这些是编译器考虑的事
 * C 语言和汇编语言是用不同的编译器来编译的，C 语言代码要先被编译成汇编代码，此汇编代码便是按照 ABI 规则生成的
 * 因此如果要手动写汇编函数，并且此函数要供 C 语言调用的话，必须按照 ABI 规则去写汇编才行
 * 我们的切换线程上 CPU 的函数 switch_to 就是使用的汇编语言，因此我们需要在汇编代码中保存这 5 个寄存器(ebp、ebx、edi、esi、esp)
 * 保存的位置就是在线程栈中
 * 
 * 线程栈有两个作用：
 * 1. 线程首次运行时，线程栈用于存储创建线程所需的相关数据。和线程有关的数据应该都在该线程的 PCB 中
 *    这样便于线程管理，避免为他们再单独维护数据空间。创建线程之初，要指定在线程中运行的函数及参数
 *    因此，把他们放在位于 PCB 所在页的高地址处的 0 级栈中比较合适，该处就是线程栈的所在地址
 * 2. 用在在任务切换函数 switch_to 中，这是线程已经处于正常运行后线程栈所体现的作用
 *    switch_to 是使用汇编实现，他是被内核调度器函数调用，因此这里涉及到主调函数寄存器的保护
 *    就是 ebp、ebx、edi 和 esi 这 4 个寄存器
 */
struct thread_stack {
    /**
     * 位于 Intel 386 硬件体系上的所有寄存器都具有全局性，因此在函数调用时，这些寄存器对主调函数和被调函数都可见
     * 这 5 个寄存器 ebp、ebx、edi、esi 和 esp 归主调函数所用，其余的寄存器归被调函数所用
     * 也就是说，不管被调函数中是否使用了这 5 个寄存器，在被调函数执行完后，这 5 个寄存器的值不该被改变
     * 因此被调函数必须为主调函数保护好这 5 个寄存器的值，在被调函数运行完之后，这 5 个寄存器的值必须和运行前一样
     * 它必须在自己的栈中存储这些寄存器的值
     * 其中 esp 会由调用约定(cdecl、fastcall)来保证，这里不用保护 esp
     */
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;
    // 线程第一次执行时，eip 指向待调用的函数 kernel_thread
    // 其他时候，eip 是指向 switch_to 的返回地址
    void (*eip)(thread_func* func, void* func_arg);

    /* ---- 以下仅供第一次被调度上 CPU 时使用 ---- */
    // 参数 unused_ret_addr 为返回地址，用于占位，没有实际使用
    void(*unused_ret_addr);
    // 由 kernel_thread 所调用的函数名
    thread_func* function;
    // 由 kernel_thread 所调用的函数所需的参数
    void* func_arg;
};

/**
 * @brief 进程或者线程的 PCB，程序控制块
 * 
 */
struct task_struct {
    // 内核线程所用的内核栈
    uint32_t* self_kernel_stack;
    pid_t pid;
    // 状态
    enum task_status status;
    char name[TASK_NAME_LEN];
    // 优先级
    uint8_t priority;
    // 每次在处理器上执行的时钟滴答数
    uint8_t ticks;
    // 此任务自从上 CPU 运行后至今占用了多少 cpu 滴答数
    // 任务运行了多久
    uint32_t elapsed_ticks;
    // 用于线程在一般队列中的节点（比如：就绪队列或者其他队列）
    struct list_elem general_tag;
    // 用于线程队列 thread_all_list 中的节点
    struct list_elem all_list_tag;
    // 进程自己页表的虚拟地址
    // 如果是线程，则此字段为 NULL
    uint32_t* pg_dir;
    // 用户进程的虚拟地址
    struct virtual_addr user_process_vaddr;
    // 用户进程内存块描述符
    struct mem_block_desc u_block_desc[DESC_CNT];
    // 已打开文件数组
    int32_t fd_table[MAX_FILES_OPEN_PER_PROC];
    // 进程所在的工作目录的 inode 编号
    uint32_t cwd_inode_nr;
    // 父进程 pid
    int16_t parent_pid;
    // 栈的边界标记，用于检测栈的溢出
    // 这个字段因为要作为边界标记，所以必须放在结构体的末尾
    // 我们的 PCB 和 0 级栈是在同一页中，栈位于页的顶端并向下发展
    // 因此担心压栈过程中会把 PCB 中的信息给覆盖，所以每次在线程或进程调度时要判断是否触及到了进程信息的边界
    // 也就是判断 stack_magic 的值是否为初始化的内容。其实 stack_magic 是一个魔数
    uint32_t stack_magic;
};

extern struct list thread_ready_list;
extern struct list thread_all_list;

void thread_create(struct task_struct* pthread, thread_func func, void* func_arg);
void init_thread(struct task_struct* pthread, char* name, int prio);
struct task_struct* thread_start(char* name, int prio, thread_func func, void* func_arg);
struct task_struct* running_thread(void);
void schedule(void);
void thread_block(enum task_status stat);
void thread_unblock(struct task_struct* pthread);
void thread_yield(void);
void thread_init(void);
pid_t fork_pid(void);
void sys_ps(void);

#endif  // THREAD_THREAD_H_

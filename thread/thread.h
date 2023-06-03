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

// 函数类型
typedef void thread_func(void* arg);

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
 */
struct thread_stack {
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;
    // 线程第一次执行时，eip 指向待调用的函数 kernel_thread
    // 其他时候，eip 是指向 switch_to 的返回地址
    void (*eip)(thread_func* func, void* func_arg);

    /* ---- 以下仅供第一次被调度上 CPU 时使用 ---- */
    // 参数 unused_ret_addr 为返回地址，用于占位，没有实际使用
    void (*unused_ret_addr);
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
    // 状态
    enum task_status status;
    // 优先级
    uint8_t priority;
    char name[16];
    // 栈的边界标记，用于检测栈的溢出
    uint32_t stack_magic;
};

void thread_create(struct task_struct* pthread, thread_func func, void* func_arg);
void init_thread(struct task_struct* pthread, char* name, int prio);
struct task_struct* thread_start(char* name, int prio, thread_func func, void* func_arg);

#endif  // THREAD_THREAD_H_

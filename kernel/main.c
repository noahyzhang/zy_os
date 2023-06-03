#include "lib/kernel/print.h"
#include "kernel/init.h"
#include "kernel/debug.h"
#include "kernel/memory.h"
#include "thread/thread.h"

void kernel_thread_func(void* arg);

/**
 * 注意 main 函数一定要是 main.c 文件的第一个函数，因为我们设定的从 0xc0001500 开始执行
 * 一定要让 main 函数位于 0xc0001500 地址处
 * 
 */

int main(void) {
    // 测试 print 函数
    put_str("I am kernel\n");
    // put_str("hello world 2023-05-20 11:52\n");
    // put_int(0);
    // put_char('\n');
    // put_int(9);
    // put_char('\n');
    // put_int(0x00021a3f);
    // put_char('\n');
    // put_int(0x12345678);
    // put_char('\n');
    // put_int(0x00000000);

    // 测试中断
    // init_all();
    // 打开中断，使用 sti 指令，他会将标志寄存器 eflags 中的 IF 位置 1
    // 这样来自中断代理 8259A 的中断信号便被处理器受理
    // 这里主要为了验证时钟中断
    // asm volatile("sti");

    // 测试 ASSERT
    // init_all();
    // ASSERT(1 == 2);

    // 测试申请内存
    // init_all();
    // void* addr = get_kernel_pages(3);
    // put_str("get_kernel_page start vaddr is ");
    // put_int((uint32_t)addr);
    // put_str("\n");

    // 测试创建线程
    init_all();
    // asm volatile ("xchg %%bx, %%bx" ::);
    // put_str("kernel_thread_func\n");
    thread_start("kernel_thread_func", 100, kernel_thread_func, "arg1");

    for (;;) {}

    return 0;
}

void kernel_thread_func(void* arg) {
    char* para = (char*)arg;
    // put_str("run kernel_thread_func start\n");
    // put_str(para);
    // put_str("\n");
    // put_str("end run kernel_thread_func\n");
    for (;;) {
        put_str(para);
        put_str("\n");
    }
}


#include "print.h"
#include "init.h"

void main(void) {
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
    init_all();
    // 打开中断，使用 sti 指令，他会将标志寄存器 eflags 中的 IF 位置 1
    // 这样来自中断代理 8259A 的中断信号便被处理器受理
    // 这里主要为了验证时钟中断
    asm volatile("sti");

    for (;;) {}
}

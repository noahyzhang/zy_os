#include "kernel/init.h"
#include "lib/kernel/print.h"
#include "kernel/interrupt.h"
#include "../device/timer.h"
#include "kernel/memory.h"
#include "thread/thread.h"
#include "device/console.h"
#include "device/keyboard.h"
#include "user_process/tss.h"
#include "user_process/syscall-init.h"
#include "device/ide.h"

/*负责初始化所有模块 */
void init_all() {
    put_str("init_all\n");
    idt_init();   //初始化中断
    timer_init();  // 初始化 PIT
    mem_init();  // 初始化内存
    thread_init();  // 初始化线程
    console_init();  // 初始化终端
    keyboard_init();  // 初始化键盘
    tss_init();  // 初始化 TSS
    syscall_init();  // 初始化系统调用

    intr_enable();  // 后面的 ide_init 需要打开中断
    ide_init();  // 初始化硬盘
    // asm volatile ("xchg %%bx, %%bx" ::);
}

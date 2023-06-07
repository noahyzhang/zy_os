#include "device/keyboard.h"
#include "lib/kernel/print.h"
#include "kernel/interrupt.h"
#include "lib/kernel/io.h"
#include "kernel/global.h"

// 键盘 buffer 寄存器端口号为 0x60
#define KBD_BUF_PORT 0x60

/**
 * @brief 键盘中断处理程序
 * 
 */
static void intr_keyboard_handler(void) {
    put_str("receive keyboard interrupt\n");
    // 注意：必须要读取输出缓冲区寄存器，否则 8042 不再继续相应键盘中断
    inb(KBD_BUF_PORT);
    return;
}

/**
 * @brief 键盘初始化
 * 
 */
void keyboard_init(void) {
    put_str("keyboard init start\n");
    // 注册键盘中断处理程序
    register_handler(0x21, intr_keyboard_handler);
    put_str("keyboard init end\n");
}


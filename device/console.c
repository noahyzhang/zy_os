#include "lib/kernel/print.h"
#include "thread/sync.h"
#include "thread/thread.h"
#include "device/console.h"

// 控制台锁
static struct lock console_lock;

/**
 * @brief 初始化终端
 * 
 */
void console_init() {
    lock_init(&console_lock);
}

/**
 * @brief 获取终端
 * 
 */
void console_acquire() {
    lock_acquire(&console_lock);
}

/**
 * @brief 释放终端
 * 
 */
void console_release() {
    lock_release(&console_lock);
}

/**
 * @brief 向终端中输出字符串
 * 
 * @param str 
 */
void console_put_str(char* str) {
    console_acquire();
    put_str(str);
    console_release();
}

/**
 * @brief 向终端中输出字符
 * 
 * @param c 
 */
void console_put_char(uint8_t char_asci) {
    console_acquire();
    put_char(char_asci);
    console_release();
}

/**
 * @brief 向终端中输出 16 进制整数
 * 
 * @param num 
 */
void console_put_int(uint32_t num) {
    console_acquire();
    put_int(num);
    console_release();
}

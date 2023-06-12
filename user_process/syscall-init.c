#include "user_process/syscall-init.h"
#include "lib/user/syscall.h"
#include "lib/kernel/print.h"
#include "thread/thread.h"
#include "device/console.h"
#include "lib/string.h"

#define syscall_nr 32

void* syscall_table[syscall_nr];

/**
 * @brief 返回当前任务的 PID
 * 
 * @return uint32_t 
 */
uint32_t sys_getpid(void) {
    return running_thread()->pid;
}

/**
 * @brief 简易版的 write 系统调用
 * 
 * @param str 
 * @return uint32_t 
 */
uint32_t sys_write(char* str) {
    console_put_str(str);
    return strlen(str);
}

/**
 * @brief 初始化系统调用
 * 
 */
void syscall_init(void) {
    put_str("syscall_init start\n");
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_WRITE] = sys_write;
    put_str("syscall_init done\n");
}
#include "shell/shell.h"
#include "lib/stdint.h"
#include "fs/fs.h"
#include "fs/file.h"
#include "lib/user/syscall.h"
#include "lib/stdio.h"
#include "kernel/global.h"
#include "lib/string.h"
#include "lib/user/assert.h"

// 最大支持键入128个字符的命令行输入
#define cmd_len 128
// 加上命令名外,最多支持15个参数
#define MAX_ARG_NR 16

// 存储输入的命令
static char cmd_line[cmd_len] = {0};

// 用来记录当前目录, 是当前目录的缓存, 每次执行 cd 命令时会更新此内容
char cwd_cache[64] = {0};

/**
 * @brief 输出提示符
 * 
 */
void print_prompt(void) {
    printf("[noahyzhang@localhost %s]$ ", cwd_cache);
}

/**
 * @brief 从键盘缓冲区中最多读入 count 个字节到 buf
 * 
 * @param buf 
 * @param count 
 */
static void readline(char* buf, int32_t count) {
    assert(buf != NULL && count > 0);
    char* pos = buf;
    // 在不出错情况下, 直到找到回车符才返回
    while (read(stdin_no, pos, 1) != -1 && (pos - buf) < count) {
        switch (*pos) {
        // 找到回车或换行符后认为键入的命令结束, 直接返回
        case '\n':
        case '\r':
            // 添加 cmd_line 的终止字符 0
            *pos = 0;
            putchar('\n');
            return;
        case '\b':
            // 阻止删除非本次输入的信息
            if (buf[0] != '\b') {
                // 退回到缓冲区 cmd_line 中上一个字符
                --pos;
                putchar('\b');
            }
            break;
        // ctrl+l 清屏
        case 'l' - 'a':
            // 1. 先将当前的字符 'l'-'a' 置为 0
            *pos = 0;
            // 2. 再将屏幕清空
            clear();
            // 3. 打印提示符
            print_prompt();
            // 4. 将之前键入的内容再次打印
            printf("%s", buf);
            break;
        // ctrl+u 清理输入
        case 'u'-'a':
            while (buf != pos) {
                putchar('\b');
                *(pos--) = 0;
            }
            break;
        // 非控制键则输出字符
        default:
            putchar(*pos);
            pos++;
            break;
        }
    }
    printf("readline: can`t find enter_key in the cmd_line, max num of char is 128\n");
}

/**
 * @brief 简易 shell
 * 
 */
void my_shell(void) {
    cwd_cache[0] = '/';
    while (1) {
        print_prompt();
        memset(cmd_line, 0, cmd_len);
        readline(cmd_line, cmd_len);
        if (cmd_line[0] == 0) {  // 若只键入了一个回车
            continue;
        }
    }
    panic("my_shell: should not be here");
}

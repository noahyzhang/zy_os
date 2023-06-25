#include "shell/shell.h"
#include "lib/stdint.h"
#include "fs/fs.h"
#include "fs/file.h"
#include "lib/user/syscall.h"
#include "lib/stdio.h"
#include "kernel/global.h"
#include "lib/string.h"
#include "lib/user/assert.h"
#include "shell/buildin_cmd.h"
#include "user_process/exec.h"

// 加上命令名外,最多支持15个参数
#define MAX_ARG_NR 16

// 存储输入的命令
static char cmd_line[MAX_PATH_LEN] = {0};
// 路径的缓冲
char final_path[MAX_PATH_LEN] = {0};
// 用来记录当前目录, 是当前目录的缓存, 每次执行 cd 命令时会更新此内容
char cwd_cache[64] = {0};
// argv 为全局变量，为了 exec 程序可以访问参数
char* argv[MAX_ARG_NR];
int32_t argc = -1;

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
 * @brief 分析字符串 cmd_str 中以 token 为分隔符的单词, 将各单词的指针存入 argv 数组
 * 
 * @param cmd_str 
 * @param argv 
 * @param token 
 * @return int32_t 
 */
static int32_t cmd_parse(char* cmd_str, char** argv, char token) {
    assert(cmd_str != NULL);
    int32_t arg_idx = 0;
    while (arg_idx < MAX_ARG_NR) {
        argv[arg_idx] = NULL;
        arg_idx++;
    }
    char* next = cmd_str;
    int32_t argc = 0;
    // 外层循环处理整个命令行
    while (*next) {
        // 去除命令字或参数之间的空格
        while (*next == token) {
            next++;
        }
        // 处理最后一个参数后接空格的情况, 如 "ls dir2 "
        if (*next == 0) {
            break;
        }
        argv[argc] = next;
        // 内层循环处理命令行中的每个命令字及参数
        while (*next && *next != token) {  // 在字符串结束前找单词分隔符
            next++;
        }
        // 如果未结束(是token字符), 使 token 变成 0
        if (*next) {
            // 将 token 字符替换为字符串结束符 0, 做为一个单词的结束, 并将字符指针 next 指向下一个字符
            *next++ = 0;
        }
        // 避免 argv 数组访问越界, 参数过多则返回 0
        if (argc > MAX_ARG_NR) {
            return -1;
        }
        argc++;
    }
    return argc;
}

/**
 * @brief 简易 shell
 * 
 */
void my_shell(void) {
    cwd_cache[0] = '/';
    while (1) {
        print_prompt();
        memset(final_path, 0, MAX_PATH_LEN);
        memset(cmd_line, 0, MAX_PATH_LEN);
        readline(cmd_line, MAX_PATH_LEN);
        if (cmd_line[0] == 0) {  // 若只键入了一个回车
            continue;
        }
        argc = -1;
        argc = cmd_parse(cmd_line, argv, ' ');
        if (argc == -1) {
            printf("num of arguments exceed %d\n", MAX_ARG_NR);
            continue;
        }
        if (!strncmp("ls", argv[0], 2)) {
            buildin_ls(argc, argv);
        } else if (!strncmp("cd", argv[0], 2)) {
            if (buildin_cd(argc, argv) != NULL) {
                memset(cwd_cache, 0, MAX_PATH_LEN);
                strncpy(cwd_cache, final_path, sizeof(cwd_cache));
            }
        } else if (!strncmp("pwd", argv[0], 3)) {
            buildin_pwd(argc, argv);
        } else if (!strncmp("ps", argv[0], 2)) {
            buildin_ps(argc, argv);
        } else if (!strncmp("clear", argv[0], 5)) {
            buildin_clear(argc, argv);
        } else if (!strncmp("mkdir", argv[0], 5)) {
            buildin_mkdir(argc, argv);
        } else if (!strncmp("rmdir", argv[0], 5)) {
            buildin_rmdir(argc, argv);
        } else if (!strncmp("rm", argv[0], 2)) {
            buildin_rm(argc, argv);
        } else {  // 如果是外部命令，需要从磁盘上加载
            int32_t pid = fork();
            if (pid) {  // 父进程
                // 下面这个 while 必须要加上, 否则父进程一般情况下会比子进程先执行,
                // 因此会进行下一轮循环将 final_path 清空, 这样子进程将无法从 final_path 中获得参数
                for (;;) {}
            } else {  // 子进程
                make_clear_abs_path(argv[0], final_path);
                argv[0] = final_path;
                // 先判断下文件是否存在
                struct stat file_stat;
                memset(&file_stat, 0, sizeof(struct stat));
                if (stat(argv[0], &file_stat) == -1) {
                    printf("my_shell: cannot access %s: No such file or directory\n", argv[0]);
                } else {
                    execv(argv[0], argv);
                }
                for (;;) {}
            }
        }
        for (int32_t arg_idx = 0; arg_idx < MAX_ARG_NR; arg_idx++) {
            argv[arg_idx] = NULL;
        }

        // char buf[MAX_PATH_LEN] = {0};
        // for (int32_t arg_idx = 0; arg_idx < argc; arg_idx++) {
        //     make_clear_abs_path(argv[arg_idx], buf);
        //     printf("%s -> %s\n", argv[arg_idx], buf);
        // }
    }
    panic("my_shell: should not be here");
}

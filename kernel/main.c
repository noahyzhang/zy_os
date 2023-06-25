#include "lib/kernel/print.h"
#include "kernel/init.h"
#include "kernel/debug.h"
#include "kernel/memory.h"
#include "thread/thread.h"
#include "kernel/interrupt.h"
#include "device/console.h"
#include "device/io_queue.h"
#include "device/keyboard.h"
#include "user_process/process.h"
#include "lib/user/syscall.h"
#include "user_process/syscall-init.h"
#include "lib/stdio.h"
#include "fs/fs.h"
#include "lib/string.h"

/**
 * 注意 main 函数一定要是 main.c 文件的第一个函数，因为我们设定的从 0xc0001500 开始执行
 * 一定要让 main 函数位于 0xc0001500 地址处
 * 
 */

void kernel_thread_func(void*);
void k_thread_a(void*);
void k_thread_b(void*);
void thread_consumer(void*);
void u_process_a(void);
void u_process_b(void);
int test_var_a = 0, test_var_b = 0;
int process_a_pid = 0, process_b_pid = 0;

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
    // init_all();
    // asm volatile ("xchg %%bx, %%bx" ::);
    // put_str("kernel_thread_func\n");
    // thread_start("kernel_thread_func", 100, kernel_thread_func, "arg1");

    // 测试线程调度
    // init_all();
    // thread_start("k_thread_a", 50, k_thread_a, "argA");
    // thread_start("k_thread_b", 10, k_thread_b, "argB");
    // // 打开中断，使时钟中断起作用
    // intr_enable();
    // for (;;) {
    //     put_str("main thread\n");
    // }

    // 测试加锁后的线程打印
    // init_all();
    // thread_start("k_thread_a", 50, k_thread_a, "arg A");
    // thread_start("k_thread_b", 10, k_thread_b, "argB ");
    // intr_enable();
    // for (;;) {
    //     console_put_str("main thread ");
    // }

    // 测试键盘中断
    // 任意按键，即可触发通路和断路
    // init_all();
    // intr_enable();

    // 测试键盘的环形缓冲区
    // init_all();
    // thread_start("consumer_a", 100, thread_consumer, " A_");
    // thread_start("consumer_b", 100, thread_consumer, " B_");
    // intr_enable();

    // 测试用户进程
    // init_all();
    // thread_start("k_thread_a", 31, k_thread_a, "argA ");
    // thread_start("k_thread_b", 31, k_thread_b, "argB ");
    // process_execute(u_process_a, "user_prog_a");
    // process_execute(u_process_b, "user_prog_b");
    // intr_enable();

    // 测试第一个系统调用 getpid()
    // init_all();
    // process_execute(u_process_a, "user_prog_a");
    // process_execute(u_process_b, "user_prog_b");

    // intr_enable();
    // console_put_str(" main_pid: 0x");
    // console_put_int(sys_getpid());
    // console_put_str("\n");
    // thread_start("k_thread_a", 30, k_thread_a, "argA ");
    // thread_start("t_thread_b", 31, k_thread_b, "argB ");

    // 测试 sys_malloc
    // init_all();
    // intr_enable();
    // thread_start("thread_a", 30, k_thread_a, "argA ");
    // thread_start("thread_b", 31, k_thread_b, "argB ");

    // 测试系统调用 malloc、free
    // init_all();
    // intr_enable();
    // process_execute(u_process_a, "user_prog_a");
    // process_execute(u_process_b, "user_prog_b");
    // thread_start("thread_a", 30, k_thread_a, "argA ");
    // thread_start("thread_b", 31, k_thread_b, "argB ");

    // 测试文件系统
    // init_all();
    // sys_open("/file_1", O_CREAT);
    // 测试文件的打开、关闭
    // uint32_t fd = sys_open("/file2", O_RDONLY | O_CREAT);
    // printf("fd: %d\n", fd);
    // int32_t res = sys_close(fd);
    // printf("%d closed, res: %d\n", fd, res);

    // 测试文件的写入
    // uint32_t fd = sys_open("/file1", O_RDWR | O_CREAT);
    // printf("fd: %d\n", fd);
    // int32_t res = sys_write(fd, "hello world\n", 12);
    // printf("sys_write res: %d\n", res);
    // res = sys_close(fd);
    // printf("%d closed, res: %d\n", fd, res);

    // 测试文件的读取
    // 如果刚写完文件就去读的话，读不到期望内容，因为文件指针此时被指向文件尾。
    // 因此需要重新打开文件，就可以读到期望内容
    // uint32_t fd = sys_open("/file1", O_RDWR | O_CREAT);
    // printf("fd: %d\n", fd);
    // int32_t res = sys_write(fd, "hello world\n", 12);
    // printf("sys_write res: %d\n", res);
    // res = sys_close(fd);
    // printf("%d closed, res: %d\n", fd, res);

    // uint32_t fd2 = sys_open("/file1", O_RDWR);
    // printf("fd: %d\n", fd2);
    // char buf[64] = {0};
    // int32_t read_bytes = sys_read(fd2, buf, 12);
    // printf("sys_read read_bytes: %d, content: %s\n", read_bytes, buf);
    // memset(buf, 0, sizeof(buf));
    // read_bytes = sys_read(fd2, buf, 8);
    // printf("sys_read read_bytes: %d, content: %s\n", read_bytes, buf);
    // res = sys_close(fd2);
    // printf("fd: %d closed, res: %d\n", fd2, res);

    // 测试文件读写指针偏移功能
    // uint32_t fd = sys_open("/file1", O_RDWR | O_CREAT);
    // printf("fd: %d\n", fd);
    // int32_t res = sys_write(fd, "hello world\n", 12);
    // printf("sys_write res: %d\n", res);
    // res = sys_lseek(fd, 0, SEEK_SET);
    // printf("sys_lseek res: %d\n", res);
    // char buf[64] = {0};
    // int32_t read_bytes = sys_read(fd, buf, 11);
    // printf("sys_read read_bytes: %d, content: %s\n", read_bytes, buf);
    // res = sys_close(fd);
    // printf("sys_close res: %d\n", res);

    // 测试文件的删除
    // 删除文件必须要等到文件关闭才能删除
    // uint32_t fd = sys_open("/file1", O_RDWR | O_CREAT);
    // printf("fd: %d\n", fd);
    // int32_t res = sys_close(fd);
    // printf("sys_close res: %d\n", res);
    // res = sys_unlink("/file1");
    // printf("sys_unlink res: %d\n", res);
    // res = sys_open("/file1", O_RDWR);
    // printf("sys_open res: %d\n", res);

    // 目录功能测试
    // int32_t res = sys_mkdir("/dir1");
    // printf("sys_mkdir res: %d\n", res);
    // res = sys_mkdir("/dir1/subdir1");
    // printf("sys_mkdir res: %d\n", res);
    // int32_t fd = sys_open("/dir1/subdir1/file1", O_CREAT | O_RDWR);
    // printf("sys_open fd: %d\n", fd);
    // res = sys_close(fd);
    // printf("sys_close res: %d\n", res);

    // struct dir* p_dir = sys_opendir("/dir1/subdir1");
    // if (p_dir) {
    //     printf("/dir1/subdir1 open done\n");
    //     res = sys_closedir(p_dir);
    //     printf("sys_closedir res: %d\n", res);
    // } else {
    //     printf("/dir1/subdir1 open fail\n");
    // }
    // res = sys_unlink("/dir1/subdir1/file1");
    // printf("sys_unlink res: %d\n", res);
    // res = sys_rmdir("/dir1/subdir1");
    // printf("sys_rmdir res: %d\n", res);

    // 获取任务的工作目录
    // char cwd_buf[32] = {0};
    // char* cwd = sys_getcwd(cwd_buf, 32);
    // printf("sys_getcwd cwd: %s, res: %s\n", cwd_buf, cwd);
    // int32_t res = sys_mkdir("/dir1");
    // printf("sys_mkdir res: %d\n", res);
    // res = sys_chdir("/dir1");
    // printf("sys_chdir res: %d\n", res);
    // memset(cwd_buf, 0, 32);
    // sys_getcwd(cwd_buf, 32);
    // printf("sys_getcwd cwd: %s\n", cwd_buf);

    // 获取文件属性
    // int32_t res = sys_mkdir("/dir1");
    // printf("sys_mkdir res: %d\n", res);
    // struct stat obj_stat;
    // sys_stat("/", &obj_stat);
    // printf("i_no: %d, size: %d, filetype: %s\n",
    //     obj_stat.st_ino, obj_stat.st_size, obj_stat.st_filetype == 2 ? "dir" : "file");
    // struct stat obj_stat2;
    // sys_stat("/dir1", &obj_stat2);
    // printf("i_no: %d, size: %d, filetype: %s\n",
    //     obj_stat2.st_ino, obj_stat2.st_size, obj_stat2.st_filetype == 2 ? "dir" : "file");

    for (;;) {}

    return 0;
}

void thread_consumer(void* arg) {
    for (;;) {
        enum intr_status old_status = intr_disable();
        if (!ioq_empty(&kbd_buf)) {
            console_put_str(arg);
            char byte = ioq_getchar(&kbd_buf);
            console_put_char(byte);
        }
        intr_set_status(old_status);
    }
}

void kernel_thread_func(void* arg) {
    char* para = (char*)arg;
    // put_str("run kernel_thread_func start\n");
    // put_str(para);
    // put_str("\n");
    // put_str("end run kernel_thread_func\n");
    for (;;) {
        put_str(para);
        // put_str("\n");
    }
}

void k_thread_a(void* arg) {
    (void)(arg);
    // char* para = (char*)arg;
    // console_put_str(" thread_a_pid: 0x");
    // console_put_int(sys_getpid());
    // console_put_char('\n');
    // console_put_str(" process_a_pid: 0x");
    // console_put_int(process_a_pid);
    // console_put_char('\n');
    void* addr1 = sys_malloc(33);
    void* addr2 = sys_malloc(255);
    void* addr3 = sys_malloc(254);

    console_put_str("I am thread_a malloc addr1 is 0x");
    console_put_int((int)addr1);
    console_put_char(',');
    console_put_int((int)addr2);
    console_put_char(',');
    console_put_int((int)addr3);
    console_put_char('\n');
    int cpu_delay = 100000;
    for (; cpu_delay-- > 0;) {}
    sys_free(addr1);
    sys_free(addr2);
    sys_free(addr3);
    // printf("I am thread_a, sys_malloc(33), addr is 0x%x \n", (int)addr);
    for (;;) {}
    // for (;;) {
        // put_str("k_thread_a: ");
        // put_str(para);
        // put_str("\n");
        // console_put_str("v_a: 0x");
        // console_put_int(test_var_a);
        // console_put_str(para);
    // }
}

void k_thread_b(void* arg) {
    (void)(arg);
    // char* para = (char*)arg;
    // console_put_str(" thread_b_pid: 0x");
    // // 内核线程通过 sys_getpid() 获取 PID
    // console_put_int(sys_getpid());
    // console_put_char('\n');
    // console_put_str(" process_b_pid: 0x");
    // console_put_int(process_b_pid);
    // console_put_char('\n');
    // void* addr = sys_malloc(63);
    // console_put_str("I am thread_b, sys_malloc(63), addr is 0x");
    // console_put_int((int)addr);
    // console_put_char('\n');
    // printf("I am thread_b, sys_malloc(63), addr is 0x%x \n", (int)addr);

    void* addr1 = sys_malloc(33);
    void* addr2 = sys_malloc(255);
    void* addr3 = sys_malloc(254);

    console_put_str("I am thread_b malloc addr1 is 0x");
    console_put_int((int)addr1);
    console_put_char(',');
    console_put_int((int)addr2);
    console_put_char(',');
    console_put_int((int)addr3);
    console_put_char('\n');
    int cpu_delay = 100000;
    for (; cpu_delay-- > 0;) {}
    sys_free(addr1);
    sys_free(addr2);
    sys_free(addr3);
    for (;;) {}
    // for (;;) {
        // put_str("k_thread_b: ");
        // put_str(para);
        // put_str("\n");
        // console_put_str(" v_b: 0x");
        // console_put_int(test_var_b);
        // console_put_str(para);
    // }
}

void u_process_a(void) {
    // 用户进程通过 getpid() 来获取进程 PID
    // process_a_pid = getpid();
    // char* name = "process_a";
    // 测试 printf
    // printf("I am %s, pid: 0x%x%c\n", name, getpid(), '\n');

    void* addr1 = malloc(256);
    void* addr2 = malloc(255);
    void* addr3 = malloc(254);
    printf("u_process_a malloc addr: 0x%x, 0x%x, 0x%x\n", (int)addr1, (int)addr2, (int)addr3);
    int cpu_delay = 100000;
    for (; cpu_delay-- > 0;) {}
    free(addr1);
    free(addr2);
    free(addr3);
    for (;;) {}
    // for (;;) {
    //     test_var_a++;
    // }
}

void u_process_b(void) {
    // process_b_pid = getpid();
    // 测试 printf
    // printf(" u_process_b: 0x%x\n", getpid());

    void* addr1 = malloc(256);
    void* addr2 = malloc(255);
    void* addr3 = malloc(254);
    printf("u_process_b malloc addr: 0x%x, 0x%x, 0x%x\n", (int)addr1, (int)addr2, (int)addr3);
    int cpu_delay = 100000;
    for (; cpu_delay-- > 0;) {}
    free(addr1);
    free(addr2);
    free(addr3);
    for (;;) {}
    // for (;;) {
    //     test_var_b++;
    // }
}

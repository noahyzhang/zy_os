#include "user_process/fork.h"
#include "user_process/process.h"
#include "kernel/memory.h"
#include "kernel/interrupt.h"
#include "kernel/debug.h"
#include "fs/file.h"
#include "lib/string.h"

extern void intr_exit(void);

/**
 * @brief 将父进程的 pcb、虚拟地址位图拷贝给子进程
 * 
 * @param child_thread 
 * @param parent_thread 
 * @return int32_t 
 */
static int32_t copy_pcb_vaddrbitmap_stack0(struct task_struct* child_thread, struct task_struct* parent_thread) {
    // 1. 复制 pcb 所在的整个页，里面包含进程 pcb 信息及特权级 0 级的栈，里面包含了返回地址，然后再单独修改个别部分
    memcpy(child_thread, parent_thread, PAGE_SIZE);
    child_thread->pid = fork_pid();
    child_thread->elapsed_ticks = 0;
    child_thread->status = TASK_READY;
    // 为新进程填充时间片
    child_thread->ticks = child_thread->priority;
    child_thread->parent_pid = parent_thread->pid;
    child_thread->general_tag.prev = child_thread->general_tag.next = NULL;
    child_thread->all_list_tag.prev = child_thread->all_list_tag.next = NULL;
    block_desc_init(child_thread->u_block_desc);
    // 2. 复制父进程的虚拟地址池的位图
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PAGE_SIZE / 8, PAGE_SIZE);
    void* vaddr_btmp = get_kernel_pages(bitmap_pg_cnt);
    if (vaddr_btmp == NULL) {
        return -1;
    }
    // 将 child_thread->user_process_vaddr.vaddr_bitmap.bits 指向自己的位图 vaddr_btmp
    memcpy(vaddr_btmp, child_thread->user_process_vaddr.vaddr_bitmap.bits, bitmap_pg_cnt * PAGE_SIZE);
    child_thread->user_process_vaddr.vaddr_bitmap.bits = vaddr_btmp;
    // 用来调试
    // pcb.name 的长度是 16，为避免下面的 stract 越界
    ASSERT(strlen(child_thread->name) < 11);
    strcat(child_thread->name, "_fork");
    return 0;
}

/**
 * @brief 复制子进程的进程体（代码和数据）以及用户栈
 * 
 * @param child_thread 
 * @param parent_thread 
 * @param buf_page 
 */
static void copy_body_stack3(struct task_struct* child_thread, struct task_struct* parent_thread, void* buf_page) {
    uint8_t* vaddr_btmp = parent_thread->user_process_vaddr.vaddr_bitmap.bits;
    uint8_t* btmp_bytes_len = parent_thread->user_process_vaddr.vaddr_bitmap.bmap_bytes_len;
    uint32_t vaddr_start = parent_thread->user_process_vaddr.vaddr_start;
    uint32_t idx_byte = 0;
    uint32_t idx_bit = 0;
    uint32_t process_vaddr = 0;
    // 在父进程的用户空间中查找已有数据的页
    for (; idx_byte < btmp_bytes_len; idx_byte++) {
        if (vaddr_btmp[idx_byte]) {
            for (idx_bit = 0; idx_bit < 8; idx_bit++) {
                if ((BITMAP_MASK << idx_bit) & vaddr_btmp[idx_byte]) {
                    process_vaddr = (idx_byte * 8 + idx_bit) * PAGE_SIZE + vaddr_start;
                    // 下面的操作是将父进程用户空间中的数据通过内核空间做中转，最终复制到子进程的用户空间
                    // 1. 将父进程在用户空间中的数据复制到内核缓冲区 buf_page
                    // 目的是下面切换到子进程的页表后，还能访问到父进程的数据
                    memcpy(buf_page, (void*)process_vaddr, PAGE_SIZE);
                    // 2. 将页表切换到子进程，目的是避免下面申请内存的函数将 pte 及 pde 安装在父进程的页表中
                    page_dir_activate(child_thread);
                    // 3. 申请虚拟地址 process_vaddr
                    get_a_page_without_opvaddrbitmap(PF_USER, process_vaddr);
                    // 4. 从内核缓冲区中将父进程数据复制到子进程的用户空间
                    memcpy((void*)process_vaddr, buf_page, PAGE_SIZE);
                    // 5. 恢复父进程页表
                    page_dir_activate(parent_thread);
                }
            }
        }
    }
}

/**
 * @brief 为子进程构造 thread_stack 和修改返回值
 * 
 * @param child_thread 
 * @return int32_t 
 */
static int32_t build_child_stack(struct task_struct* child_thread) {
    // 1. 使子进程 pid 返回值为 0
    // 获取子进程 0 级栈栈顶
    struct intr_stack* intr_0_stack = (struct intr_stack*)(
        (uint32_t)child_thread + PAGE_SIZE - sizeof(struct intr_stack));
    // 修改子进程的返回值为 0
    intr_0_stack->eax = 0;
    // 2. 为 switch_to 构建 struct thread_stack，将其构建在紧临 intr_stack 之下的空间
    uint32_t* ret_addr_in_thread_stack = (uint32_t*)intr_0_stack - 1;
    // 为了梳理 thread_stack 中的关系
    uint32_t* esi_ptr_in_thread_stack = (uint32_t*)intr_0_stack - 2;
    uint32_t* edi_ptr_in_thread_stack = (uint32_t*)intr_0_stack - 3;
    uint32_t* ebx_ptr_in_thread_stack = (uint32_t*)intr_0_stack - 4;

    // ebp 在 thread_stack 中的地址便是当时的 esp（0级栈的栈顶）
    uint32_t* ebp_ptr_in_thread_stack = (uint32_t*)intr_0_stack - 5;
    // switch_to 的返回地址更新为 intr_exit，直接从中断返回
    *ret_addr_in_thread_stack = (uint32_t)intr_exit;
    // 下面这两行赋值只是为了使构建的 thread_stack 更加清晰, 其实也不需要,
    // 因为在进入 intr_exit 后一系列的 pop 会把寄存器中的数据覆盖
    *ebp_ptr_in_thread_stack = *ebx_ptr_in_thread_stack = 0;
    *edi_ptr_in_thread_stack = *esi_ptr_in_thread_stack = 0;
    // 把构建的 thread_stack 的栈顶作为 switch_to 恢复数据时的栈顶
    child_thread->self_kernel_stack = ebp_ptr_in_thread_stack;
    return 0;
}


/**
 * @brief 更新 inode 打开数
 * 
 * @param thread 
 */
static void update_inode_open_cnts(struct task_struct* thread) {
    int32_t local_fd = 3, global_fd = 0;
    for (; local_fd < MAX_FILES_OPEN_PER_PROC; local_fd++) {
        global_fd = thread->fd_table[local_fd];
        ASSERT(global_fd < MAX_FILE_OPEN);
        if (global_fd != -1) {
            file_table[global_fd].fd_inode->i_open_cnts++;
        }
    }
}

/**
 * @brief 拷贝父进程本身所占资源给子进程
 * 
 * @param child_thread 
 * @param parent_thread 
 * @return int32_t 
 */
static int32_t copy_process(struct task_struct* child_thread, struct task_struct* parent_thread) {
    // 内核缓冲区，作为父进程用户空间的数据复制到子进程用户空间的中转
    void* buf_page = get_kernel_pages(1);
    if (buf_page == NULL) {
        return -1;
    }
    // 1. 复制父进程的 pcb、虚拟地址位图、内核栈 到子进程
    if (copy_pcb_vaddrbitmap_stack0(child_thread, parent_thread) == -1) {
        return -2;
    }
    // 2. 为子进程创建页表，此页表仅包含内核空间
    child_thread->pg_dir = create_page_dir();
    if (child_thread->pg_dir == NULL) {
        return -3;
    }
    // 3. 复制父进程进程体及用户栈给子进程
    copy_body_stack3(child_thread, parent_thread, buf_page);
    // 4. 构建子进程 thread_stack 和修改返回 pid
    build_child_stack(child_thread);
    // 5. 更新文件 inode 的打开数
    mfree_page(PF_KERNEL, buf_page, 1);
    return 0;
}

/**
 * @brief fork 子进程，内核线程不可直接调用
 * 
 * @return pid_t 
 */
pid_t sys_fork(void) {
    struct task_struct* parent_thread = running_thread();
    // 为子进程创建 pcb（task_struct 结构）
    struct task_struct* child_thread = get_kernel_pages(1);
    if (child_thread == NULL) {
        return -1;
    }
    ASSERT(INTR_OFF == intr_get_status() && parent_thread->pg_dir != NULL);
    if (copy_process(child_thread, parent_thread) == -1) {
        return -2;
    }
    // 新创建的线程添加到就绪队列和所有线程队列，子进程由调试器安排运行
    ASSERT(!elem_find(&thread_ready_list, &child_thread->general_tag));
    list_append(&thread_ready_list, &child_thread->general_tag);
    ASSERT(!elem_find(&thread_all_list, &child_thread->all_list_tag));
    list_append(&thread_all_list, &child_thread->all_list_tag);
    // 返回子进程的 pid
    return child_thread->pid;
}

#include "kernel/global.h"
#include "kernel/debug.h"
#include "kernel/memory.h"
#include "lib/kernel/list.h"
#include "user_process/tss.h"
#include "kernel/interrupt.h"
#include "lib/string.h"
#include "device/console.h"
#include "user_process/process.h"

extern void intr_exit(void);

/**
 * @brief 创建用户进程
 * 
 * @param process_name 
 * @param name 
 */
void process_execute(void* process_name, char* name) {
    // pcb 内核的数据结构，由内核来维护进程信息，因此要在内核内存池中申请
    struct task_struct* thread = get_kernel_pages(1);
    init_thread(thread, name, DEFAULT_PRIO);
    create_user_vaddr_bitmap(thread);
    thread_create(thread, start_process, process_name);
    thread->pg_dir = create_page_dir();
    block_desc_init(thread->u_block_desc);

    // 关中断，将创建出来的 thread 插入到就绪队列中
    enum intr_status old_status = intr_disable();
    ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
    list_append(&thread_ready_list, &thread->general_tag);
    // 同时也将此 thread 插入到所有线程队列中
    ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
    list_append(&thread_all_list, &thread->all_list_tag);
    // 不要忘记恢复中断状态
    intr_set_status(old_status);
}

/**
 * @brief 构建用户进程初始上下文信息
 * 
 * @param process_name 
 */
void start_process(void* process_name) {
    void* function = process_name;
    struct task_struct* cur = running_thread();
    // 跨过 thread_stack，指向 intr_stack
    cur->self_kernel_stack += sizeof(struct thread_stack);
    // 获取到中断栈
    struct intr_stack* proc_stack = (struct intr_stack*)cur->self_kernel_stack;
    // 初始化中断栈
    proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp_dummy = 0;
    proc_stack->ebx = proc_stack->edx = proc_stack->ecx = proc_stack->eax = 0;
    // 不太允许用户直接访问显存资源，用户态用不上，直接初始化为 0
    proc_stack->gs = 0;
    proc_stack->ds = proc_stack->es = proc_stack->fs = SELECTOR_U_DATA;
    // 待执行的用户程序地址
    proc_stack->eip = function;
    proc_stack->cs = SELECTOR_U_CODE;
    proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);
    proc_stack->esp = (void*)((uint32_t)get_a_page(PF_USER, USER_STACK3_VADDR) + PAGE_SIZE);
    proc_stack->ss = SELECTOR_U_DATA;
    asm volatile("movl %0, %%esp; jmp intr_exit" : : "g" (proc_stack) : "memory");
}

/**
 * @brief 激活线程或进程的页表，更新 TSS 中的 esp0 为进程的特权级 0 的栈
 * 
 * @param p_thread 
 */
void process_activate(struct task_struct* p_thread) {
    ASSERT(p_thread != NULL);
    // 激活进程或者线程的页表
    page_dir_activate(p_thread);
    // 内核线程特权级本身就是 0 特权级，处理器进入中断时并不会从 TSS 中获取 0 特权栈地址，故不需要更新 esp0
    if (p_thread->pg_dir) {
        // 更新该进程的 esp0，用于此进程被中断时保留上下文
        update_tss_esp(p_thread);
    }
}

/**
 * @brief 激活页表
 * 
 * @param p_thread 
 */
void page_dir_activate(struct task_struct* p_thread) {
    /********************************************************
     * 执行此函数时,当前任务可能是线程。
     * 之所以对线程也要重新安装页表, 原因是上一次被调度的可能是进程,
     * 否则不恢复页表的话,线程就会使用进程的页表了。
     ********************************************************/

    // 若为内核线程，需要重新填充页表为 0x100000
    // 默认为内核的页目录物理地址,也就是内核线程所用的页目录表
    uint32_t page_dir_phy_addr = 0x100000;
    // 用户态进程有自己的页目录表
    if (p_thread->pg_dir != NULL) {
        page_dir_phy_addr = addr_v2p((uint32_t)p_thread->pg_dir);
    }
    // 更新页目录寄存器 cr3，使新页表生效
    asm volatile("movl %0, %%cr3" : : "r" (page_dir_phy_addr) : "memory");
}

/**
 * @brief 创建页目录表，将当前页表中表示内核空间的 pde 复制
 *        成功则返回页目录的虚拟地址，否则返回 -1
 * 
 * @return uint32_t* 
 */
uint32_t* create_page_dir(void) {
    /* 用户进程的页表不能让用户直接访问到,所以在内核空间来申请 */
    uint32_t* page_dir_vaddr = get_kernel_pages(1);
    if (page_dir_vaddr == NULL) {
        console_put_str("create_page_dir: get_kernel_page failed!");
        return NULL;
    }

    /************************** 1  先复制页表  *************************************/
    /*  page_dir_vaddr + 0x300*4 是内核页目录的第768项 */
    memcpy((uint32_t*)((uint32_t)page_dir_vaddr + 0x300*4), (uint32_t*)(0xfffff000+0x300*4), 1024);
    /*****************************************************************************/

    /************************** 2  更新页目录地址 **********************************/
    uint32_t new_page_dir_phy_addr = addr_v2p((uint32_t)page_dir_vaddr);
    /* 页目录地址是存入在页目录的最后一项,更新页目录地址为新页目录的物理地址 */
    page_dir_vaddr[1023] = new_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_1;
    /*****************************************************************************/
    return page_dir_vaddr;
}

void create_user_vaddr_bitmap(struct task_struct* user_prog) {
    user_prog->user_process_vaddr.vaddr_start = USER_VADDR_START;
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PAGE_SIZE / 8 , PAGE_SIZE);
    user_prog->user_process_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);
    user_prog->user_process_vaddr.vaddr_bitmap.bmap_bytes_len = (0xc0000000 - USER_VADDR_START) / PAGE_SIZE / 8;
    bitmap_init(&user_prog->user_process_vaddr.vaddr_bitmap);
}

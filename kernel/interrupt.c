#include "kernel/interrupt.h"
#include "lib/stdint.h"
#include "kernel/global.h"
#include "lib/kernel/io.h"
#include "lib/kernel/print.h"

// 这里用的可编程中断控制器是 8259A, 主片的控制端口是 0x20
#define PIC_M_CTRL 0x20
// 主片的数据端口是 0x21
#define PIC_M_DATA 0x21
// 从片的控制端口是 0xa0
#define PIC_S_CTRL 0xa0
// 从片的数据端口是 0xa1
#define PIC_S_DATA 0xa1

#define IDT_DESC_CNT 0x81      // 目前总共支持的中断数

#define EFLAGS_IF   0x00000200       // eflags寄存器中的if位为1
#define GET_EFLAGS(EFLAG_VAR) asm volatile("pushfl; popl %0" : "=g" (EFLAG_VAR))

extern uint32_t syscall_handler(void);

/*中断门描述符结构体*/
struct gate_desc {
    uint16_t    func_offset_low_word;
    uint16_t    selector;
    uint8_t     dcount;  // 此项为双字计数字段，是门描述符中的第4字节。此项固定值，不用考虑
    uint8_t     attribute;
    uint16_t    func_offset_high_word;
};

// 静态函数声明,非必须
static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function);
// idt是中断描述符表,本质上就是个中断门描述符数组
static struct gate_desc idt[IDT_DESC_CNT];

// 用于保存异常的名字
char* intr_name[IDT_DESC_CNT];
// 定义中断处理程序数组
// 在 kernel.s 中定义的 intr_xxx_entry 只是中断处理程序的入口，最终调用的是 ide_table 中的处理程序
intr_handler idt_table[IDT_DESC_CNT];
// 声明引用定义在 kernel.s 中的中断处理函数入口数组
extern intr_handler intr_entry_table[IDT_DESC_CNT];

/**
 * @brief 初始化可编程中断控制器8259A
 * 
 */
static void pic_init(void) {
    // 初始化主片
    // ICW1: 边沿触发, 级联 8259, 需要 ICW4
    outb(PIC_M_CTRL, 0x11);
    // ICW2: 起始中断向量号为 0x20, 也就是 IR[0-7] 为 0x20 ~ 0x27
    outb(PIC_M_DATA, 0x20);
    // ICW3: IR2 接从片
    outb(PIC_M_DATA, 0x04);
    // ICW4: 8086 模式, 其中 AEOI 位为 0，则表示非自动，即手动结束中断
    outb(PIC_M_DATA, 0x01);

    // 初始化从片
    // ICW1: 边沿触发, 级联8259, 需要ICW4
    outb(PIC_S_CTRL, 0x11);
    // ICW2: 起始中断向量号为 0x28, 也就是 IR[8-15] 为 0x28 ~ 0x2F
    outb(PIC_S_DATA, 0x28);
    // ICW3: 设置从片连接到主片的 IR2 引脚
    outb(PIC_S_DATA, 0x02);
    // ICW4: 8086模式, 其中 AEOI 位为 0，则表示非自动，即手动结束中断
    outb(PIC_S_DATA, 0x01);

    // 打开主片上 IR0, 也就是目前只接受时钟产生的中断
    // OCW1: 主片，只打开 IR0，其他位都屏蔽
    // outb(PIC_M_DATA, 0xfe);
    // OCW1: 从片，全部位都屏蔽
    // outb(PIC_S_DATA, 0xff);

    // 测试键盘，只打开键盘中断，其他全部关闭
    // OCW1: 主片，键盘中断是 IRQ1，其他位都屏蔽
    // outb(PIC_M_DATA, 0xfd);

    // 打开时钟中断和键盘中断
    // outb(PIC_M_DATA, 0xfc);
    // outb(PIC_S_DATA, 0xff);

    // IRQ2 用于级联从片，必须打开，否则无法响应从片上的中断
    // 主片上打开的中断有 IRQ0 的时钟，IRQ1 的键盘和级联从片的 IRQ2，其他全部关闭
    outb(PIC_M_DATA, 0xf8);
    // 打开从片上的 IRQ14，此引脚接收硬盘控制器的中断
    outb(PIC_S_DATA, 0xbf);

    put_str("pic_init done\n");
}

// 创建中断门描述符
static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function) {
    p_gdesc->func_offset_low_word = (uint32_t)function & 0x0000FFFF;
    p_gdesc->selector = SELECTOR_K_CODE;
    p_gdesc->dcount = 0;
    p_gdesc->attribute = attr;
    p_gdesc->func_offset_high_word = ((uint32_t)function & 0xFFFF0000) >> 16;
}

// 初始化中断描述符表
static void idt_desc_init(void) {
    int last_index = IDT_DESC_CNT - 1;
    for (int i = 0; i < IDT_DESC_CNT; i++) {
        make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
    }
    // 系统调用的中断号是 0x80
    // 单独处理系统调用，系统调用对应的中断门 dpl 为 3
    // 中断处理程序为单独的 syscall_handler
    make_idt_desc(&idt[last_index], IDT_DESC_ATTR_DPL3, syscall_handler);
    put_str("   idt_desc_init done\n");
}

/**
 * @brief 通用的中断处理函数, 一般用在异常出现时的处理
 * 
 * @param vec_nr 
 */
static void general_intr_handler(uint8_t vec_nr) {
    // 0x2f 是从片 8259A 上的最后一个 irq 引脚，保留
    if (vec_nr == 0x27 || vec_nr == 0x2f) {
        // IRQ7 和 IRQ15 会产生伪中断(spurious interrupt), 无须处理。
        // 一般是是那种硬件中断，比如中断线路上电气信号异常，或者中断请求设备本身有问题
        //由于 IRQ7和 IRQ15 无法屏蔽，所以在这里单独处理他们
        return;
    }
    // 将光标置为 0，从屏幕左上角清出一片打印异常情况的区域，方便查看
    set_cursor(0);
    int cursor_pos = 0;
    // 320 表示每行 80 个字符，一共 4 行
    while (cursor_pos < 320) {
        put_char(' ');
        cursor_pos++;
    }
    // 重置光标为屏幕左上角
    set_cursor(0);
    put_str("!!!   exception message begin   !!!\n");
    // 从第 2 行的第 8 个字符开始打印
    set_cursor(88);
    put_str(intr_name[vec_nr]);
    put_str("\n");
    // 如果为 PageFault，将缺失的地址打印出来并悬停
    if (vec_nr == 14) {
        int page_fault_vaddr = 0;
        asm("movl %%cr2, %0" : "=r" (page_fault_vaddr));
        put_str("page fault addr is ");
        put_int(page_fault_vaddr);
        put_str("\n");
    }
    put_str("!!!   exception message end   !!!\n");
    // 能进入中断处理程序就表示已经处于关中断情况下，不会出现调度进程的情况。因此如下死循环不会被中断
    while (true) {}

    // put_str("int vector: 0x");
    // put_int(vec_nr);
    // put_char('\n');
}

/**
 * @brief 完成一般中断处理函数注册及异常名称注册
 * 
 */
static void exception_init(void) {
    for (int i = 0; i < IDT_DESC_CNT; i++) {
        // idt_table 数组中的函数是在进入中断后根据中断向量号调用的,
        // 见 kernel/kernel.S的call [idt_table + %1*4]
        // 默认为 general_intr_handler。以后会由 register_handler 来注册具体处理函数。
        idt_table[i] = general_intr_handler;
        // 先统一赋值为 unknown
        intr_name[i] = "unknown";
    }
    intr_name[0] = "#DE Divide Error";
    intr_name[1] = "#DB Debug Exception";
    intr_name[2] = "NMI Interrupt";
    intr_name[3] = "#BP Breakpoint Exception";
    intr_name[4] = "#OF Overflow Exception";
    intr_name[5] = "#BR BOUND Range Exceeded Exception";
    intr_name[6] = "#UD Invalid Opcode Exception";
    intr_name[7] = "#NM Device Not Available Exception";
    intr_name[8] = "#DF Double Fault Exception";
    intr_name[9] = "Coprocessor Segment Overrun";
    intr_name[10] = "#TS Invalid TSS Exception";
    intr_name[11] = "#NP Segment Not Present";
    intr_name[12] = "#SS Stack Fault Exception";
    intr_name[13] = "#GP General Protection Exception";
    intr_name[14] = "#PF Page-Fault Exception";
    // intr_name[15] 第15项是intel保留项，未使用
    intr_name[16] = "#MF x87 FPU Floating-Point Error";
    intr_name[17] = "#AC Alignment Check Exception";
    intr_name[18] = "#MC Machine-Check Exception";
    intr_name[19] = "#XF SIMD Floating-Point Exception";
}

/**
 * @brief 开中断并返回开中断前的状态
 * 
 * @return enum intr_status 
 */
enum intr_status intr_enable() {
    enum intr_status old_status;
    if (INTR_ON == intr_get_status()) {
        old_status = INTR_ON;
        return old_status;
    } else {
        old_status = INTR_OFF;
        // 开中断, sti 指令将 eflags 寄存器的 IF 位置 1
        asm volatile("sti");
        return old_status;
    }
}

/**
 * @brief 关中断, 并且返回关中断前的状态
 * 
 * @return enum intr_status 
 */
enum intr_status intr_disable() {
    enum intr_status old_status;
    if (INTR_ON == intr_get_status()) {
        old_status = INTR_ON;
        // 关中断, cli 指令将 eflags 寄存器的 IF 位置 0
        asm volatile("cli" : : : "memory");
        return old_status;
    } else {
        old_status = INTR_OFF;
        return old_status;
    }
}

/**
 * @brief 将中断状态设置为 status
 * 
 * @param status 
 * @return enum intr_status 
 */
enum intr_status intr_set_status(enum intr_status status) {
    return status & INTR_ON ? intr_enable() : intr_disable();
}

/**
 * @brief 获取当前中断状态
 * 
 * @return enum intr_status 
 */
enum intr_status intr_get_status() {
    uint32_t eflags = 0;
    GET_EFLAGS(eflags);
    return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF;
}

/**
 * @brief 注册中断处理程序
 * 
 * @param vector_no 
 * @param function 
 */
void register_handler(uint8_t vector_no, intr_handler function) {
    // idt_table 数组中的函数是在进入中断后根据中断向量号调用的
    // kernel/kernel.s 中的 "call[idt_table + %1*4]"
    idt_table[vector_no] = function;
}

/**
 * @brief 完成有关中断的所有初始化工作
 * 
 */
void idt_init() {
    put_str("idt_init start\n");
    // 初始化中断描述符表
    idt_desc_init();
    // 异常名初始化并注册通常的中断处理函数
    exception_init();
    // 初始化 8259A
    pic_init();

    // 加载 idt
    // lidt 48位内存数据 ==> 前 32 位是基址，后 16 位是界限 limit
    // 注意：32 位地址经过左移操作后，高位将被丢弃，万一原地址高 16 位不是 0，这样会造成数据错误
    // 因此，需要将 idt 地址转换成 64 位整型后再进行左移操作，这样高 32 位都是 0，经过左移依然可以保证精度
    // 因为我们先将指针转换成 uint32_t，然后再转换成 uint64_t
    uint64_t idt_operand = ((sizeof(idt) - 1) | ((uint64_t)(uint32_t)idt << 16));
    asm volatile("lidt %0" : : "m" (idt_operand));
    put_str("idt_init done\n");
}

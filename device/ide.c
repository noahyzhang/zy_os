#include "device/ide.h"
#include "lib/stdio.h"
#include "lib/kernel/stdio_kernel.h"
#include "kernel/interrupt.h"
#include "kernel/memory.h"
#include "kernel/debug.h"
#include "lib/string.h"
#include "device/timer.h"
#include "lib/kernel/io.h"
#include "lib/string.h"

// 定义硬盘各寄存器的端口号
#define reg_data(channel)     (channel->port_base + 0)
#define reg_error(channel)    (channel->port_base + 1)
#define reg_sect_cnt(channel) (channel->port_base + 2)
#define reg_lba_l(channel)    (channel->port_base + 3)
#define reg_lba_m(channel)    (channel->port_base + 4)
#define reg_lba_h(channel)    (channel->port_base + 5)
#define reg_dev(channel)      (channel->port_base + 6)
#define reg_status(channel)   (channel->port_base + 7)
#define reg_cmd(channel)      (reg_status(channel))
#define reg_alt_status(channel)  (channel->port_base + 0x206)
#define reg_ctl(channel)      reg_alt_status(channel)

// reg_alt_status 寄存器的一些关键位
// 硬盘忙
#define BIT_STAT_BSY    0x80
// 驱动器准备好
#define BIT_STAT_DRDY   0x40
// 数据传输准备好了
#define BIT_STAT_DRQ    0x8

// device寄存器的一些关键位
// 第7位和第5位固定为 1
#define BIT_DEV_MBS  0xa0
// device 寄存器的 LBA 位，表示寻址模式使用 LBA 还是 CHS
#define BIT_DEV_LBA  0x40
// device 寄存器的 dev 位，0 代表主盘，1 代表从盘
#define BIT_DEV_DEV  0x10

// 一些硬盘操作的指令
// identify指令
#define CMD_IDENTIFY       0xec
// 读扇区指令
#define CMD_READ_SECTOR    0x20
// 写扇区指令
#define CMD_WRITE_SECTOR   0x30

// 定义可读写的最大扇区数,调试用的
// 只支持80MB硬盘
#define max_lba ((80*1024*1024/512) - 1)

// 按硬盘数计算的通道数
uint8_t channel_cnt;
// 有两个ide通道
struct ide_channel channels[2];
// 用于记录总扩展分区的起始 lba, 初始为0, partition_scan 时以此为标记
int32_t ext_lba_base = 0;
// 用来记录硬盘主分区和逻辑分区的下标
uint8_t p_no = 0, l_no = 0;
// 分区队列
struct list partition_list;

/**
 * @brief 构建一个 16 字节大小的结构体,用来存分区表项
 * 
 */
struct partition_table_entry {
    // 是否可引导
    uint8_t  bootable;
    // 起始磁头号
    uint8_t  start_head;
    // 起始扇区号
    uint8_t  start_sec;
    // 起始柱面号
    uint8_t  start_chs;
    // 分区类型
    uint8_t  fs_type;
    // 结束磁头号
    uint8_t  end_head;
    // 结束扇区号
    uint8_t  end_sec;
    // 结束柱面号
    uint8_t  end_chs;
    // 本分区起始扇区的lba地址
    uint32_t start_lba;
    // 本分区的扇区数目
    uint32_t sec_cnt;
} __attribute__((packed));  // 保证此结构是16字节大小

/**
 * @brief 引导扇区,mbr或ebr所在的扇区
 * 
 */
struct boot_sector {
    // 引导代码
    uint8_t  other[446];
    // 分区表中有4项, 一项 16 字节，共64字节
    struct   partition_table_entry partition_table[4];
    // 启动扇区的结束标志是 0x55,0xaa
    // 注意：x86 是小端存储，所以此处变量的实际值是 0xaa55
    uint16_t signature;
} __attribute__((packed));

/**
 * @brief 选择读写的硬盘
 * 
 * @param hd 
 */
static void select_disk(struct disk* hd) {
    uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
    // 若是从盘就置DEV位为1
    if (hd->dev_no == 1) {
        reg_device |= BIT_DEV_DEV;
    }
    outb(reg_dev(hd->my_channel), reg_device);
}

/**
 * @brief 向硬盘控制器写入起始扇区地址及要读写的扇区数
 * 
 * @param hd 
 * @param lba 
 * @param sec_cnt 
 */
static void select_sector(struct disk* hd, uint32_t lba, uint8_t sec_cnt) {
    ASSERT(lba <= max_lba);
    struct ide_channel* channel = hd->my_channel;

    // 写入要读写的扇区数
    // 如果sec_cnt为0,则表示写入256个扇区
    outb(reg_sect_cnt(channel), sec_cnt);

    // 写入lba地址(即扇区号)
    // lba 地址的低8位, 不用单独取出低8位. outb 函数中的汇编指令 outb %b0, %w1会只用 al
    outb(reg_lba_l(channel), lba);
    // lba地址的 8~15 位
    outb(reg_lba_m(channel), lba >> 8);
    // lba地址的 16~23 位
    outb(reg_lba_h(channel), lba >> 16);

    // 因为 lba 地址的 24~27 位要存储在 device 寄存器的 0～3 位,
    // 无法单独写入这 4 位, 所以在此处把 device 寄存器再重新写入一次
    outb(reg_dev(channel), BIT_DEV_MBS | BIT_DEV_LBA | (hd->dev_no == 1 ? BIT_DEV_DEV : 0) | lba >> 24);
}

/**
 * @brief 向通道 channel 发命令 cmd
 * 
 * @param channel 
 * @param cmd 
 */
static void cmd_out(struct ide_channel* channel, uint8_t cmd) {
    // 只要向硬盘发出了命令便将此标记置为true,硬盘中断处理程序需要根据它来判断
    channel->expecting_intr = true;
    outb(reg_cmd(channel), cmd);
}

/**
 * @brief 硬盘读入 sec_cnt 个扇区的数据到 buf
 * 注意，读写扇区数端口 0x1f2 和 0x172 是 8 位寄存器
 * 所以，每次读写最多是 255 个扇区
 * 当写入端口值为 0 时，则表示读写 256 个扇区
 * 因此当读写的端口数超过 256 时，必须拆分成多次读写操作
 * 
 * @param hd 
 * @param buf 
 * @param sec_cnt 
 */
static void read_from_sector(struct disk* hd, void* buf, uint8_t sec_cnt) {
    uint32_t size_in_byte;
    if (sec_cnt == 0) {
        // 因为sec_cnt是8位变量,由主调函数将其赋值时,若为256则会将最高位的1丢掉变为0
        size_in_byte = 256 * 512;
    } else {
        size_in_byte = sec_cnt * 512;
    }
    // (size_in_byte/2): 一次读 2 字节，所以这里除以 2
    insw(reg_data(hd->my_channel), buf, size_in_byte / 2);
}

/**
 * @brief 将buf中sec_cnt扇区的数据写入硬盘
 * 
 * @param hd 
 * @param buf 
 * @param sec_cnt 
 */
static void write2sector(struct disk* hd, void* buf, uint8_t sec_cnt) {
    uint32_t size_in_byte;
    if (sec_cnt == 0) {
        // 因为 sec_cnt 是 8 位变量, 由主调函数将其赋值时, 若为 256 则会将最高位的 1 丢掉变为 0
        size_in_byte = 256 * 512;
    } else {
        size_in_byte = sec_cnt * 512;
    }
    outsw(reg_data(hd->my_channel), buf, size_in_byte / 2);
}

/**
 * @brief 最多等待30秒, 直到硬盘数据准备就绪，可以读取
 * 为什么要等待硬盘 30 秒呢？
 * 在 ata 手册中说：All actions required in this state shall be completed within 31 s
 * 也就是说，所有的操作都应该在 31 秒内完成，所以我们在 30 秒内等待硬盘响应
 * @param hd 
 * @return true 
 * @return false 
 */
static bool busy_wait(struct disk* hd) {
    struct ide_channel* channel = hd->my_channel;
    // 可以等待30000毫秒
    uint16_t time_limit = 30 * 1000;
    while (time_limit -= 10 >= 0) {
        if (!(inb(reg_status(channel)) & BIT_STAT_BSY)) {
            return (inb(reg_status(channel)) & BIT_STAT_DRQ);
        } else {
            // 睡眠10毫秒
            mtime_sleep(10);
        }
    }
    return false;
}

/**
 * @brief 从硬盘读取 sec_cnt 个扇区到 buf
 * 
 * @param hd 
 * @param lba 
 * @param buf 
 * @param sec_cnt 此处的sec_cnt为32位大小
 */
void ide_read(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt) {
    ASSERT(lba <= max_lba);
    ASSERT(sec_cnt > 0);
    // 这里加锁保护，保证硬盘的中断和硬盘的操作是相互对应的
    // 也就是保证一次只操作同一通道上的一块硬盘
    lock_acquire(&hd->my_channel->lock);
    // 1. 先选择操作的硬盘
    select_disk(hd);
    // 每次操作的扇区数
    uint32_t secs_op;
    // 已完成的扇区数
    uint32_t secs_done = 0;
    while (secs_done < sec_cnt) {
        // 设置单次操作的扇区数为 256 个扇区
        if ((secs_done + 256) <= sec_cnt) {
            secs_op = 256;
        } else {
            secs_op = sec_cnt - secs_done;
        }
        // 2. 写入待读入的扇区数和起始扇区号
        select_sector(hd, lba + secs_done, secs_op);
        // 3. 执行的命令写入reg_cmd寄存器
        // 准备开始读数据
        cmd_out(hd->my_channel, CMD_READ_SECTOR);
        /*********************   阻塞自己的时机  ***********************/
        // 在硬盘已经开始工作(开始在内部读数据或写数据)后才能阻塞自己,现在硬盘已经开始忙了,
        // 将自己阻塞,等待硬盘完成读操作后通过中断处理程序唤醒自己
        sema_down(&hd->my_channel->disk_done);
        // 4. 检测硬盘状态是否可读
        // 醒来后开始执行下面代码
        // 如果失败，即等待了 30 秒磁盘依然在繁忙或者磁盘未准备好数据，直接报错出来
        if (!busy_wait(hd)) {
            char error[64];
            snprintf(error, sizeof(error) - 1, "%s read sector %d failed!!!!!!\n", hd->name, lba);
            PANIC(error);
        }
        // 5. 把数据从硬盘的缓冲区中读出
        read_from_sector(hd, (void*)((uint32_t)buf + secs_done * 512), secs_op);
        secs_done += secs_op;
    }
    lock_release(&hd->my_channel->lock);
}

/**
 * @brief 将 buf 中 sec_cnt 扇区数据写入硬盘
 * 
 * @param hd 
 * @param lba 
 * @param buf 
 * @param sec_cnt 
 */
void ide_write(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt) {
    ASSERT(lba <= max_lba);
    ASSERT(sec_cnt > 0);
    lock_acquire(&hd->my_channel->lock);
    // 1. 先选择操作的硬盘
    select_disk(hd);
    // 每次操作的扇区数
    uint32_t secs_op;
    // 已完成的扇区数
    uint32_t secs_done = 0;
    while (secs_done < sec_cnt) {
        if ((secs_done + 256) <= sec_cnt) {
        secs_op = 256;
        } else {
        secs_op = sec_cnt - secs_done;
        }
        // 2. 写入待写入的扇区数和起始扇区号
        // 先将待读的块号lba地址和待读入的扇区数写入lba寄存器
        select_sector(hd, lba + secs_done, secs_op);

        // 3. 执行的命令写入reg_cmd寄存器
        // 准备开始写数据
        cmd_out(hd->my_channel, CMD_WRITE_SECTOR);

        // 4. 检测硬盘状态是否可读
        if (!busy_wait(hd)) {
            char error[64];
            snprintf(error, sizeof(error) - 1, "%s write sector %d failed!!!!!!\n", hd->name, lba);
            PANIC(error);
        }
        // 5. 将数据写入硬盘
        write2sector(hd, (void*)((uint32_t)buf + secs_done * 512), secs_op);
        // 在硬盘响应期间阻塞自己
        sema_down(&hd->my_channel->disk_done);
        secs_done += secs_op;
    }
    /* 醒来后开始释放锁*/
    lock_release(&hd->my_channel->lock);
}

/**
 * @brief 将 dst 中 len 个相邻字节交换位置后存入 buf
 * 用于处理 identify 命令的返回信息，因为硬盘参数信息是以字为单位的
 * 所以要处理小端的字节序
 * 
 * @param dst 
 * @param buf 
 * @param len 
 */
static void swap_pairs_bytes(const char* dst, char* buf, uint32_t len) {
    uint8_t idx;
    for (idx = 0; idx < len; idx += 2) {
        // buf中存储dst中两相邻元素交换位置后的字符串
        buf[idx + 1] = *dst++;
        buf[idx] = *dst++;
    }
    buf[idx] = '\0';
}

/**
 * @brief 获得硬盘参数信息
 * 
 * @param hd 
 */
static void identify_disk(struct disk* hd) {
    char id_info[512];
    select_disk(hd);
    cmd_out(hd->my_channel, CMD_IDENTIFY);
    // 向硬盘发送指令后便通过信号量阻塞自己,
    // 待硬盘处理完成后,通过中断处理程序将自己唤醒
    sema_down(&hd->my_channel->disk_done);

    /* 醒来后开始执行下面代码*/
    if (!busy_wait(hd)) {     //  若失败
        char error[64];
        snprintf(error, sizeof(error)-1, "%s identify failed!!!!!!\n", hd->name);
        PANIC(error);
    }
    // 读取一个扇区就够了
    read_from_sector(hd, id_info, 1);

    char buf[64];
    uint8_t sn_start = 10 * 2, sn_len = 20, md_start = 27 * 2, md_len = 40;
    // [10, 19] 是硬盘序列号，长度为 20 的字符串
    swap_pairs_bytes(&id_info[sn_start], buf, sn_len);
    printk("   disk %s info:\n      SN: %s\n", hd->name, buf);
    memset(buf, 0, sizeof(buf));
    // [27, 46] 是硬盘型号，长度为 40 的字符串
    swap_pairs_bytes(&id_info[md_start], buf, md_len);
    printk("      MODULE: %s\n", buf);
    // [60, 61] 是可供用户使用的扇区数，长度为 2 的整型
    uint32_t sectors = *(uint32_t*)&id_info[60 * 2];
    printk("      SECTORS: %d\n", sectors);
    printk("      CAPACITY: %dMB\n", sectors * 512 / 1024 / 1024);
}

/**
 * @brief 扫描硬盘 hd 中地址为 ext_lba 的扇区中的所有分区
 * 
 * @param hd 
 * @param ext_lba 
 */
static void partition_scan(struct disk* hd, uint32_t ext_lba) {
    // 因为需要一个扇区大小的内存空间，使用栈空间可能会爆栈，因为下面还要递归
    struct boot_sector* bs = sys_malloc(sizeof(struct boot_sector));
    ide_read(hd, ext_lba, bs, 1);
    uint8_t part_idx = 0;
    // 获取分区表地址
    struct partition_table_entry* p = bs->partition_table;
    // 遍历分区表4个分区表项
    while (part_idx++ < 4) {
        // 若为扩展分区
        if (p->fs_type == 0x5) {
            if (ext_lba_base != 0) {
                // 子扩展分区的start_lba是相对于主引导扇区中的总扩展分区地址
                partition_scan(hd, p->start_lba + ext_lba_base);
            } else {  // ext_lba_base为0表示是第一次读取引导块,也就是主引导记录所在的扇区
                // 记录下扩展分区的起始lba地址,后面所有的扩展分区地址都相对于此
                ext_lba_base = p->start_lba;
                partition_scan(hd, p->start_lba);
            }
        } else if (p->fs_type != 0) {  // 若是有效的分区类型
            if (ext_lba == 0) {  // 此时全是主分区
                hd->prim_parts[p_no].start_lba = ext_lba + p->start_lba;
                hd->prim_parts[p_no].sec_cnt = p->sec_cnt;
                hd->prim_parts[p_no].my_disk = hd;
                list_append(&partition_list, &hd->prim_parts[p_no].part_tag);
                uint32_t name_len = sizeof(hd->prim_parts[p_no].name);
                snprintf(hd->prim_parts[p_no].name, name_len-1, "%s%d", hd->name, p_no + 1);
                p_no++;
                ASSERT(p_no < 4);  // 0,1,2,3
            } else {
                hd->logic_parts[l_no].start_lba = ext_lba + p->start_lba;
                hd->logic_parts[l_no].sec_cnt = p->sec_cnt;
                hd->logic_parts[l_no].my_disk = hd;
                list_append(&partition_list, &hd->logic_parts[l_no].part_tag);
                // 逻辑分区数字是从 5 开始,主分区是 1～4
                uint32_t name_len = sizeof(hd->logic_parts[l_no].name);
                snprintf(hd->logic_parts[l_no].name, name_len-1, "%s%d", hd->name, l_no + 5);
                l_no++;
                if (l_no >= 8)    // 只支持8个逻辑分区,避免数组越界
                    return;
            }
        }
        p++;
    }
    sys_free(bs);
}

/**
 * @brief 打印分区信息
 * 
 * @param pelem 
 * @param UNUSED 
 * @return true 
 * @return false 
 */
static bool partition_info(struct list_elem* pelem, int arg) {
    (void)arg;
    struct partition* part = elem2entry(struct partition, part_tag, pelem);
    printk("   %s start_lba:0x%x, sec_cnt:0x%x\n", part->name, part->start_lba, part->sec_cnt);

    /* 在此处return false与函数本身功能无关,
    * 只是为了让主调函数list_traversal继续向下遍历元素 */
    return false;
}

/**
 * @brief 硬盘中断处理程序
 * 注意：硬盘控制器的中断在下列情况下会被清掉
 * 1. 读取了 status 寄存器
 * 2. 发出了 reset 命令
 * 3. 或者又向 reg_cmd 写了新的命令
 * 
 * @param irq_no 
 */
void intr_hd_handler(uint8_t irq_no) {
    // 0x2e 表示 8259A 的 IRQ14 接口，代表第一个 ATA 通道
    // 0x2f 表示 8259A 的 IRQ15 接口，代表第二个 ATA 通道
    ASSERT(irq_no == 0x2e || irq_no == 0x2f);
    uint8_t ch_no = irq_no - 0x2e;
    struct ide_channel* channel = &channels[ch_no];
    ASSERT(channel->irq_no == irq_no);
    // 不必担心此中断是否对应的是这一次的expecting_intr,
    // 每次读写硬盘时会申请锁,从而保证了同步一致性
    if (channel->expecting_intr) {
        channel->expecting_intr = false;
        sema_up(&channel->disk_done);

        // 读取状态寄存器使硬盘控制器认为此次的中断已被处理,
        // 从而硬盘可以继续执行新的读写
        inb(reg_status(channel));
    }
}

/**
 * @brief 硬盘数据结构初始化
 * 
 */
void ide_init() {
    printk("ide_init start\n");
    // 获取硬盘的数量
    // 注意：低端 1M 以内的虚拟地址和物理地址相同。所以虚拟地址 0x475 可以映射到物理地址 0x475 中
    uint8_t hd_cnt = *((uint8_t*)(0x475));
    printk("    ide_init hd_cnt: %d\n", hd_cnt);
    ASSERT(hd_cnt > 0);
    list_init(&partition_list);
    // 一个ide通道上有两个硬盘,根据硬盘数量反推有几个ide通道
    channel_cnt = DIV_ROUND_UP(hd_cnt, 2);
    struct ide_channel* channel;
    uint8_t channel_no = 0;
    uint8_t dev_no = 0;

    // 处理每个通道上的硬盘
    while (channel_no < channel_cnt) {
        channel = &channels[channel_no];
        snprintf(channel->name, strlen(channel->name), "ide%d", channel_no);

        // 为每个ide通道初始化端口基址及中断向量
        switch (channel_no) {
        case 0:
            // ide0通道的起始端口号是0x1f0
            channel->port_base = 0x1f0;
            // 从片8259a上倒数第二的中断引脚,温盘,也就是ide0通道的的中断向量号
            channel->irq_no = 0x20 + 14;
            break;
        case 1:
            // ide1通道的起始端口号是0x170
            channel->port_base = 0x170;
            // 从8259A上的最后一个中断引脚,我们用来响应ide1通道上的硬盘中断
            channel->irq_no = 0x20 + 15;
            break;
        }
        // 未向硬盘写入指令时不期待硬盘的中断
        channel->expecting_intr = false;
        lock_init(&channel->lock);

        // 初始化为0,目的是向硬盘控制器请求数据后,硬盘驱动sema_down此信号量会阻塞线程,
        // 直到硬盘完成后通过发中断,由中断处理程序将此信号量sema_up,唤醒线程
        sema_init(&channel->disk_done, 0);
        register_handler(channel->irq_no, intr_hd_handler);
        //分别获取两个硬盘的参数及分区信息
        for (; dev_no < 2;) {
            struct disk* hd = &channel->devices[dev_no];
            hd->my_channel = channel;
            hd->dev_no = dev_no;
            snprintf(hd->name, sizeof(hd->name)-1, "sd%c", 'a' + channel_no * 2 + dev_no);
            // 获取硬盘参数
            identify_disk(hd);
            // 内核本身的裸硬盘（hd60M.img）不处理
            if (dev_no != 0) {
                // 扫描该硬盘上的分区
                partition_scan(hd, 0);
            }
            p_no = 0, l_no = 0;
            dev_no++;
        }
        // 将硬盘驱动器号置 0，为下一个 channel 的两个硬盘初始化
        dev_no = 0;
        // 下一个channel
        channel_no++;
    }
    printk("\n  all partition info\n");
    // 打印所有分区信息
    list_traversal(&partition_list, partition_info, (int)NULL);
    printk("ide_init done\n");
}

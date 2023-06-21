/**
 * @file ide.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-20
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef DEVICE_IDE_H_
#define DEVICE_IDE_H_

#include "lib/stdint.h"
#include "thread/sync.h"
#include "lib/kernel/bitmap.h"

/**
 * @brief 分区结构
 * 
 */
struct partition {
    // 起始扇区
    uint32_t start_lba;
    // 扇区数
    uint32_t sec_cnt;
    // 分区所属的硬盘
    struct disk* my_disk;
    // 用于队列中的标记
    struct list_elem part_tag;
    // 分区名称
    char name[8];
    // 本分区的超级块
    struct super_block* sb;
    // 块位图
    struct bitmap block_bitmap;
    // i结点位图
    struct bitmap inode_bitmap;
    // 本分区打开的i结点队列
    struct list open_inodes;
};

/**
 * @brief 硬盘结构
 * 
 */
struct disk {
    // 本硬盘的名称，如sda等
    char name[8];
    // 此块硬盘归属于哪个ide通道
    struct ide_channel* my_channel;
    // 本硬盘是主0还是从1
    uint8_t dev_no;
    // 主分区顶多是4个
    struct partition prim_parts[4];
    // 逻辑分区数量无限,但总得有个支持的上限,那就支持8个
    struct partition logic_parts[8];
};

/**
 * @brief ata通道结构
 * 
 */
struct ide_channel {
    // 本ata通道名称
    char name[8];
    // 本通道的起始端口号
    uint16_t port_base;
    // 本通道所用的中断号
    uint8_t irq_no;
    // 通道锁
    struct lock lock;
    // 表示等待硬盘的中断
    bool expecting_intr;
    // 用于阻塞、唤醒驱动程序
    struct semaphore disk_done;
    // 一个通道上连接两个硬盘，一主一从
    struct disk devices[2];
};

extern uint8_t channel_cnt;
extern struct ide_channel channels[];
extern struct list partition_list;

void ide_init(void);
void intr_hd_handler(uint8_t irq_no);
void ide_read(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt);
void ide_write(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt);

#endif  // DEVICE_IDE_H_

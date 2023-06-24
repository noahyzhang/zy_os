#include "fs/inode.h"
#include "fs/fs.h"
#include "fs/file.h"
#include "kernel/global.h"
#include "kernel/debug.h"
#include "kernel/memory.h"
#include "kernel/interrupt.h"
#include "lib/kernel/list.h"
#include "lib/kernel/stdio_kernel.h"
#include "lib/string.h"
#include "fs/super_block.h"

/**
 * @brief 用来存储 inode 位置
 * 
 */
struct inode_position {
    // inode 是否跨扇区
    bool two_sec;
    // inode 所在的扇区号
    uint32_t sec_lba;
    // inode 在扇区内的字节偏移量
    uint32_t off_size;
};

/**
 * @brief 获取 inode 所在的扇区和扇区内的偏移量
 * 
 * @param part 
 * @param inode_no 
 * @param inode_pos 
 */
static void inode_locate(struct partition* part, uint32_t inode_no, struct inode_position* inode_pos) {
    // inode_table 在硬盘上是连续的
    ASSERT(inode_no < 4096);
    uint32_t inode_table_lba = part->sb->inode_table_lba;

    uint32_t inode_size = sizeof(struct inode);
    // 第 inode_no 号 I 结点相对于 inode_table_lba 的字节偏移量
    uint32_t off_size = inode_no * inode_size;
    // 第 inode_no 号 I结点相对于 inode_table_lba 的扇区偏移量
    uint32_t off_sec  = off_size / 512;
    // 待查找的 inode 所在扇区中的起始地址
    uint32_t off_size_in_sec = off_size % 512;

    // 判断此 i 结点是否跨越 2 个扇区
    uint32_t left_in_sec = 512 - off_size_in_sec;
    // 若扇区内剩下的空间不足以容纳一个 inode, 必然是 I 结点跨越了 2 个扇区
    if (left_in_sec < inode_size ) {
        inode_pos->two_sec = true;
    } else {  // 否则,所查找的 inode 未跨扇区
        inode_pos->two_sec = false;
    }
    inode_pos->sec_lba = inode_table_lba + off_sec;
    inode_pos->off_size = off_size_in_sec;
}

/**
 * @brief 将 inode 写入到分区 part
 * 
 * @param part 
 * @param inode 
 * @param io_buf 是用于硬盘 io 的缓冲区
 */
void inode_sync(struct partition* part, struct inode* inode, void* io_buf) {
    uint8_t inode_no = inode->i_no;
    struct inode_position inode_pos;
    // inode位置信息会存入inode_pos
    inode_locate(part, inode_no, &inode_pos);
    ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));

    // 硬盘中的 inode 中的成员 inode_tag 和 i_open_cnts 是不需要的,
    // 它们只在内存中记录链表位置和被多少进程共享
    struct inode pure_inode;
    memcpy(&pure_inode, inode, sizeof(struct inode));

    // 以下 inode 的三个成员只存在于内存中,现在将 inode 同步到硬盘, 清掉这三项即可
    pure_inode.i_open_cnts = 0;
    // 置为 false, 以保证在硬盘中读出时为可写
    pure_inode.write_deny = false;
    pure_inode.inode_tag.prev = pure_inode.inode_tag.next = NULL;

    char* inode_buf = (char*)io_buf;
    // 若是跨了两个扇区, 就要读出两个扇区再写入两个扇区
    if (inode_pos.two_sec) {
        // 读写硬盘是以扇区为单位,若写入的数据小于一扇区,要将原硬盘上的内容先读出来再和新数据拼成一扇区后再写入
        // inode 在 format 中写入硬盘时是连续写入的, 所以读入 2 块扇区
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);

        // 开始将待写入的 inode 拼入到这 2 个扇区中的相应位置
        memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));

        // 将拼接好的数据再写入磁盘
        ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
    } else {  // 若只是一个扇区
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
        memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
        ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
    }
}

/**
 * @brief 根据 i 结点号返回相应的 i 结点
 * 
 * @param part 
 * @param inode_no 
 * @return struct inode* 
 */
struct inode* inode_open(struct partition* part, uint32_t inode_no) {
    // 先在已打开 inode 链表中找 inode, 此链表是为提速创建的缓冲区
    struct list_elem* elem = part->open_inodes.head.next;
    struct inode* inode_found;
    while (elem != &part->open_inodes.tail) {
        inode_found = elem2entry(struct inode, inode_tag, elem);
        if (inode_found->i_no == inode_no) {
            inode_found->i_open_cnts++;
            return inode_found;
        }
        elem = elem->next;
    }

    // 由于 open_inodes 链表中找不到, 下面从硬盘上读入此 inode 并加入到此链表
    struct inode_position inode_pos;

    // inode 位置信息会存入 inode_pos, 包括 inode 所在扇区地址和扇区内的字节偏移量
    inode_locate(part, inode_no, &inode_pos);

    // 为使通过 sys_malloc 创建的新 inode 被所有任务共享,
    // 需要将 inode 置于内核空间,故需要临时将 cur_pbc->pg_dir 置为 NULL
    struct task_struct* cur = running_thread();
    uint32_t* cur_pagedir_bak = cur->pg_dir;
    cur->pg_dir = NULL;
    // 以上三行代码完成后下面分配的内存将位于内核区
    inode_found = (struct inode*)sys_malloc(sizeof(struct inode));
    // 恢复 pg_dir
    cur->pg_dir = cur_pagedir_bak;

    char* inode_buf;
    if (inode_pos.two_sec) {  // 考虑跨扇区的情况
        inode_buf = (char*)sys_malloc(1024);
        // i结点表是被 partition_format 函数连续写入扇区的, 所以下面可以连续读出来
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
    } else {  // 否则,所查找的inode未跨扇区,一个扇区大小的缓冲区足够
        inode_buf = (char*)sys_malloc(512);
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
    }
    memcpy(inode_found, inode_buf + inode_pos.off_size, sizeof(struct inode));

    /* 因为一会很可能要用到此inode,故将其插入到队首便于提前检索到 */
    list_push(&part->open_inodes, &inode_found->inode_tag);
    inode_found->i_open_cnts = 1;

    sys_free(inode_buf);
    return inode_found;
}

/**
 * @brief 关闭 inode 或减少 inode 的打开数
 * 
 * @param inode 
 */
void inode_close(struct inode* inode) {
    // 若没有进程再打开此文件,将此 inode 去掉并释放空间
    enum intr_status old_status = intr_disable();
    if (--inode->i_open_cnts == 0) {
        // 将 I 结点从 part->open_inodes 中去掉
        list_remove(&inode->inode_tag);
        // inode_open 时为实现 inode 被所有进程共享,
        // 已经在 sys_malloc 为 inode 分配了内核空间,
        // 释放inode时也要确保释放的是内核内存池
        struct task_struct* cur = running_thread();
        uint32_t* cur_pagedir_bak = cur->pg_dir;
        cur->pg_dir = NULL;
        sys_free(inode);
        cur->pg_dir = cur_pagedir_bak;
    }
    intr_set_status(old_status);
}

/**
 * @brief 将硬盘分区 part 上的 inode 清空
 * 
 * @param part 
 * @param inode_no 
 * @param io_buf 
 */
void inode_delete(struct partition* part, uint32_t inode_no, void* io_buf) {
    ASSERT(inode_no < 4096);
    struct inode_position inode_pos;
    // inode 位置信息会存入 inode_pos
    inode_locate(part, inode_no, &inode_pos);
    ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));

    char* inode_buf = (char*)io_buf;
    if (inode_pos.two_sec) {   // inode跨扇区,读入2个扇区
        // 将原硬盘上的内容先读出来
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
        // 将 inode_buf 清 0
        memset((inode_buf + inode_pos.off_size), 0, sizeof(struct inode));
        // 用清 0 的内存数据覆盖磁盘
        ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
    } else {  // 未跨扇区, 只读入 1 个扇区就好
        // 将原硬盘上的内容先读出来
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
        // 将 inode_buf 清 0
        memset((inode_buf + inode_pos.off_size), 0, sizeof(struct inode));
        // 用清 0 的内存数据覆盖磁盘
        ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
    }
}

/**
 * @brief 回收 inode 的数据块和 inode 本身
 * 
 * @param part 
 * @param inode_no 
 */
void inode_release(struct partition* part, uint32_t inode_no) {
    struct inode* inode_to_del = inode_open(part, inode_no);
    ASSERT(inode_to_del->i_no == inode_no);

    // 1. 回收 inode 占用的所有块
    uint8_t block_idx = 0, block_cnt = 12;
    uint32_t block_bitmap_idx;
    // 12 个直接块 + 128 个间接块
    uint32_t all_blocks[140] = {0};
    // a. 先将前 12 个直接块存入 all_blocks
    for (; block_idx < 12; block_idx++) {
        all_blocks[block_idx] = inode_to_del->i_sectors[block_idx];
    }
    // b. 如果一级间接块表存在,将其 128 个间接块读到 all_blocks[12~], 并释放一级间接块表所占的扇区
    if (inode_to_del->i_sectors[12] != 0) {
        ide_read(part->my_disk, inode_to_del->i_sectors[12], all_blocks + 12, 1);
        block_cnt = 140;
        // 回收一级间接块表占用的扇区
        block_bitmap_idx = inode_to_del->i_sectors[12] - part->sb->data_start_lba;
        ASSERT(block_bitmap_idx > 0);
        bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
    }
    // c. inode 所有的块地址已经收集到 all_blocks 中,下面逐个回收
    block_idx = 0;
    while (block_idx < block_cnt) {
        if (all_blocks[block_idx] != 0) {
            block_bitmap_idx = 0;
            block_bitmap_idx = all_blocks[block_idx] - part->sb->data_start_lba;
            ASSERT(block_bitmap_idx > 0);
            bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
            bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
        }
        block_idx++;
    }

    // 2. 回收该 inode 所占用的 inode
    bitmap_set(&part->inode_bitmap, inode_no, 0);
    bitmap_sync(cur_part, inode_no, INODE_BITMAP);

    /******     以下inode_delete是调试用的    ******
     * 此函数会在inode_table中将此inode清0,
     * 但实际上是不需要的,inode分配是由inode位图控制的,
     * 硬盘上的数据不需要清0,可以直接覆盖*/
    void* io_buf = sys_malloc(1024);
    inode_delete(part, inode_no, io_buf);
    sys_free(io_buf);
    /***********************************************/

    inode_close(inode_to_del);
}

/**
 * @brief 初始化 new_inode
 * 
 * @param inode_no 
 * @param new_inode 
 */
void inode_init(uint32_t inode_no, struct inode* new_inode) {
    new_inode->i_no = inode_no;
    new_inode->i_size = 0;
    new_inode->i_open_cnts = 0;
    new_inode->write_deny = false;

    /* 初始化块索引数组i_sector */
    uint8_t sec_idx = 0;
    while (sec_idx < 13) {
    /* i_sectors[12]为一级间接块地址 */
        new_inode->i_sectors[sec_idx] = 0;
        sec_idx++;
    }
}

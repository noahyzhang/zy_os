/**
 * @file inode.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-21
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef FS_INODE_H_
#define FS_INODE_H_

#include "lib/stdint.h"
#include "lib/string.h"
#include "lib/kernel/list.h"
#include "device/ide.h"

/**
 * @brief inode 结构
 * 
 */
struct inode {
    // inode编号
    uint32_t i_no;

    // 当此 inode 是文件时, i_size 是指文件大小,
    // 若此 inode 是目录, i_size 是指该目录下所有目录项大小之和
    // 单位是字节
    uint32_t i_size;

    // 记录此文件被打开的次数
    // 在关闭文件时，回收与之相关的资源
    uint32_t i_open_cnts;
    // 写文件不能并行, 进程写文件前检查此标识
    // 不能让多个用户同时写一个文件，否则就会引起数据混乱
    // 因此必须保证文件的写操作是串行的
    // 当 write_deny 为 true 时表示已经有任务在写文件里，此文件的其他写操作应该被拒绝
    bool write_deny;

    // 数据块的指针，因为我们一个数据块是一个扇区，因此使用 sectors 命名
    // i_sectors[0-11] 是直接块, i_sectors[12] 用来存储一级间接块指针
    // 我们只支持一级间接块
    // 扇区大小 512 字节，块地址用 4 字节表示，所以支持的一级间接块是 128 个
    // 因此一共支持: 12+128=140 个块(扇区)
    uint32_t i_sectors[13];
    // 存储已打开的 inode 列表，充当一个磁盘与内存之间的缓冲区
    // 由于 inode 是从硬盘上保存的，文件被打开时，肯定是先要从硬盘上载入其 inode，硬盘较慢
    // 为了避免下次再打开该文件时还要从硬盘上重复载入 inode，应该在该文件第一次被打开时就将其 inode 加入到内存缓冲中
    // 每次打开一个文件时，先在此缓冲中查找相关的 inode，如果有就直接使用，否则再从硬盘上读取 inode
    struct list_elem inode_tag;
};

struct inode* inode_open(struct partition* part, uint32_t inode_no);
void inode_sync(struct partition* part, struct inode* inode, void* io_buf);
void inode_init(uint32_t inode_no, struct inode* new_inode);
void inode_close(struct inode* inode);
void inode_release(struct partition* part, uint32_t inode_no);
void inode_delete(struct partition* part, uint32_t inode_no, void* io_buf);

#endif  // FS_INODE_H_

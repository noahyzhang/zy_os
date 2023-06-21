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

/**
 * @brief inode 结构
 * 
 */
struct inode {
    // inode编号
    uint32_t i_no;

    // 当此 inode 是文件时, i_size 是指文件大小,
    // 若此 inode 是目录, i_size 是指该目录下所有目录项大小之和
    uint32_t i_size;

    // 记录此文件被打开的次数
    uint32_t i_open_cnts;
    // 写文件不能并行,进程写文件前检查此标识
    bool write_deny;

    // i_sectors[0-11] 是直接块, i_sectors[12] 用来存储一级间接块指针
    uint32_t i_sectors[13];
    struct list_elem inode_tag;
};

#endif  // FS_INODE_H_

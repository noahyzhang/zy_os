/**
 * @file super_block.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-21
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef FS_SUPER_BLOCK_H_
#define FS_SUPER_BLOCK_H_

#include "lib/stdint.h"

/**
 * @brief 超级块
 * 
 */
struct super_block {
    // 用来标识文件系统类型, 支持多文件系统的操作系统通过此标志来识别文件系统类型
    uint32_t magic;
    // 本分区总共的扇区数
    uint32_t sec_cnt;
    // 本分区中 inode 数量
    uint32_t inode_cnt;
    // 本分区的起始 lba 地址
    uint32_t part_lba_base;

    // 块位图本身起始扇区地址
    uint32_t block_bitmap_lba;
    // 扇区位图本身占用的扇区数量
    uint32_t block_bitmap_sects;

    // i 结点位图起始扇区 lba 地址
    uint32_t inode_bitmap_lba;
    // i 结点位图占用的扇区数量
    uint32_t inode_bitmap_sects;

    // i 结点表起始扇区 lba 地址
    uint32_t inode_table_lba;
    // i 结点表占用的扇区数量
    uint32_t inode_table_sects;

    // 数据区开始的第一个扇区号
    uint32_t data_start_lba;
    // 根目录所在的I结点号
    uint32_t root_inode_no;
    // 目录项大小
    uint32_t dir_entry_size;

    // 加上 460 字节,凑够 512 字节 1 扇区大小
    uint8_t  pad[460];
} __attribute__((packed));

#endif  // FS_SUPER_BLOCK_H_

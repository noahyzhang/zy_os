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

#define SUPER_BLOCK_MAGIC 0x19970719

/**
 * @brief 超级块
 * 我们的文件系统中，1 块数据块等于 1 个扇区
 * 磁盘操作要以扇区为单位，因此让我们的超级块凑足一个扇区，最后 pad 就是填充扇区用的
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

    // 空闲块位图本身起始扇区地址
    uint32_t block_bitmap_lba;
    // 空闲块扇区位图本身占用的扇区数量
    uint32_t block_bitmap_sects;

    // inode 位图起始扇区 lba 地址
    uint32_t inode_bitmap_lba;
    // inode 位图占用的扇区数量
    uint32_t inode_bitmap_sects;

    // inode 数组起始扇区 lba 地址
    uint32_t inode_table_lba;
    // inode 数组占用的扇区数量
    uint32_t inode_table_sects;

    // 数据区开始的地址
    uint32_t data_start_lba;
    // 根目录所在的 inode 号
    uint32_t root_inode_no;
    // 目录项大小
    uint32_t dir_entry_size;

    // 以上所有变量加起来有 52 字节
    // 加上 460 字节,凑够 512 字节 1 扇区大小
    uint8_t pad[460];
} __attribute__((packed));

#endif  // FS_SUPER_BLOCK_H_

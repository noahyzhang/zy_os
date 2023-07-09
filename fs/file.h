/**
 * @file file.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-21
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef FS_FILE_H_
#define FS_FILE_H_

#include "lib/stdint.h"
#include "device/ide.h"
#include "fs/dir.h"
#include "kernel/global.h"

/**
 * 每个进程的 PCB 中有文件描述符数组
 * 文件描述符数组元素的信息指向文件表中的某个文件结构
 * 而打开一个文件会产生一个文件结构，需要占用空间。
 * 因此进程可打开的最大文件数是有上限的
 * 
 * 1. 每个进程都必须有独立的、大小完全一样的一套文件描述符数组
 * 2. 文件结构所组成的数组（文件表）占用的空间是比较大的，不可能放在 PCB 中（PCB 只占用 1 页内存）
 * 因此文件描述符数组元素存储的是文件表中文件结构的下标
 * 
 */

/**
 * @brief 文件结构
 * 模仿 Linux 的文件结构，用于记录与文件操作相关的信息
 * 每次打开一个文件就会产生一个文件结构
 * 从而实现，即使同一个文件被同时多次打开，各自操作的偏移量也互不影响
 * 
 * Linux 会把所有的 “文件结构” 组织到一起形成数组统一管理，该数组称为文件表
 */
struct file {
    // 记录当前文件操作的偏移地址, 以 0 为起始, 最大为文件大小减1
    uint32_t fd_pos;
    uint32_t fd_flag;
    struct inode* fd_inode;
};

/**
 * @brief 标准输入输出描述符
 * 
 */
enum std_fd {
    // 0 标准输入
    stdin_no,
    // 1 标准输出
    stdout_no,
    // 2 标准错误
    stderr_no
};

/**
 * @brief 位图类型
 * 
 */
enum bitmap_type {
    // inode 位图
    INODE_BITMAP,
    // 块位图
    BLOCK_BITMAP
};

// 系统可打开的最大文件数
#define MAX_FILE_OPEN 32

extern struct file file_table[MAX_FILE_OPEN];
int32_t inode_bitmap_alloc(struct partition* part);
int32_t block_bitmap_alloc(struct partition* part);
int32_t file_create(struct dir* parent_dir, char* filename, uint8_t flag);
void bitmap_sync(struct partition* part, uint32_t bit_idx, uint8_t btmp);
int32_t get_free_slot_in_global(void);
int32_t pcb_fd_install(int32_t globa_fd_idx);
int32_t file_open(uint32_t inode_no, uint8_t flag);
int32_t file_close(struct file* file);
int32_t file_write(struct file* file, const void* buf, uint32_t count);
int32_t file_read(struct file* file, void* buf, uint32_t count);

#endif  // FS_FILE_H_

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
 * @brief 文件结构
 * 
 */
struct file {
    // 记录当前文件操作的偏移地址, 以 0 为起始,最大为文件大小 -1
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

#endif  // FS_FILE_H_

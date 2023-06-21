/**
 * @file dir.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-21
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef FS_DIR_H_
#define FS_DIR_H_

#include "lib/stdint.h"
#include "fs/inode.h"
#include "device/ide.h"
#include "kernel/global.h"

// 最大文件名长度
#define MAX_FILE_NAME_LEN 16

/**
 * @brief 目录结构
 * 
 */
struct dir {
    struct inode* inode;
    // 记录在目录内的偏移
    uint32_t dir_pos;
    // 目录的数据缓存
    uint8_t dir_buf[512];
};

/**
 * @brief 目录项结构
 * 
 */
struct dir_entry {
    // 普通文件或目录名称
    char filename[MAX_FILE_NAME_LEN];
    // 普通文件或者目录对应的 inode 编号
    uint32_t i_no;
    // 文件类型
    enum file_types f_type;
};

#endif  // FS_DIR_H_

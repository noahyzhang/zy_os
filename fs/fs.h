/**
 * @file fs.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-21
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef FS_FS_H_
#define FS_FS_H_

#include "lib/stdint.h"
#include "device/ide.h"

// 每个分区所支持最大创建的文件数
#define MAX_FILES_PER_PART 4096
//每扇区的位数
#define BITS_PER_SECTOR 4096
// 扇区字节大小
#define SECTOR_SIZE 512
// 块字节大小
#define BLOCK_SIZE SECTOR_SIZE

/**
 * @brief 文件类型
 * 
 */
enum file_types {
    // 不支持的文件类型
    FT_UNKNOWN,
    // 普通文件
    FT_REGULAR,
    // 目录
    FT_DIRECTORY
};

void filesys_init(void);

#endif  // FS_FS_H_

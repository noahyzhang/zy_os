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
// 路径最大长度
#define MAX_PATH_LEN 512

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

/**
 * @brief 打开文件的选项
 * 
 */
enum oflags {
    O_RDONLY,  // 只读
    O_WRONLY,  // 只写
    O_RDWR,  // 读写
    O_CREAT = 4  // 创建
};

/**
 * @brief 用来记录查找文件过程中已找到的上级路径, 也就是查找文件过程中"走过的地方"
 * 
 */
struct path_search_record {
    char searched_path[MAX_PATH_LEN];  // 查找过程中的父路径
    struct dir* parent_dir;  // 文件或目录所在的直接父目录
    enum file_types file_type;  // 找到的是普通文件还是目录,找不到将为未知类型(FT_UNKNOWN)
};

extern struct partition* cur_part;


void filesys_init(void);
int32_t path_depth_cnt(char* pathname);
int32_t sys_open(const char* pathname, uint8_t flags);
int32_t sys_write(int32_t fd, const void* buf, uint32_t count);
int32_t sys_read(int32_t fd, void* buf, uint32_t count);

#endif  // FS_FS_H_

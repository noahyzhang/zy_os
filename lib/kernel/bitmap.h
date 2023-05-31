/**
 * @file bitmap.h
 * @author noahyzhang
 * @brief 
 * @version 0.1
 * @date 2023-05-31
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef LIB_KERNEL_BITMAP_H_
#define LIB_KERNEL_BITMAP_H_

#include "kernel/global.h"

#define BITMAP_MASK 1

struct bitmap {
    uint32_t bmap_bytes_len;
    uint8_t* bits;
};

/**
 * @brief 初始化位图，全部置 0
 * 
 * @param bitmap 
 */
void bitmap_init(struct bitmap* bitmap);

/**
 * @brief 判断 bit_idx 位是否为 1，若为 1，则返回 true，否则返回 false
 * 
 * @param bmap 
 * @param bit_idx 
 * @return true 
 * @return false 
 */
bool bitmap_scan_test(struct bitmap* bmap, uint32_t bit_idx);

/**
 * @brief 在位图上申请连续 cnt 个位，成功，则返回其起始位下标；失败，返回 -1
 * 
 * @param bmap 
 * @param cnt 
 * @return int 
 */
int bitmap_scan(struct bitmap* bmap, uint32_t cnt);

/**
 * @brief 将位图的 bit_idx 位设置为 value
 * 
 * @param bmap 
 * @param bit_idx 
 * @param value 
 */
void bitmap_set(struct bitmap* bmap, uint32_t bit_idx, int8_t value);

#endif  // LIB_KERNEL_BITMAP_H_

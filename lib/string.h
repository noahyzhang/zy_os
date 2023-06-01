/**
 * @file string.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-05-31
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef LIB_STRING_H_
#define LIB_STRING_H_

#include "lib/stdint.h"

/**
 * @brief 将 dst 起始的 size 个字节设置为 value
 * 
 * @param dst 
 * @param value 
 * @param size 
 */
void memset(void* dst, uint8_t value, uint32_t size);

/**
 * @brief 将 src 起始的 size 个字节复制到 dst
 * 
 * @param dst 
 * @param src 
 * @param size 
 */
void memcpy(void* dst, const void* src, uint32_t size);

/**
 * @brief 连续比较以地址 a 和地址 b 开头的 size 个字节
 *        若相等则返回 0，如果 a 大于 b，返回 1， 否则返回 -1
 * 
 * @param a 
 * @param b 
 * @param size 
 * @return int 
 */
int memcmp(const void* a, const void* b, uint32_t size);

/**
 * @brief 将 src 起始的 size 个字节复制到 dst
 * 
 * @param dst 
 * @param src 
 * @return char* 
 */
char* strncpy(char* dst, const char* src, uint32_t size);

/**
 * @brief 字符串的长度
 * 
 * @param str 
 * @return uint32_t 
 */
uint32_t strlen(const char* str);

/**
 * @brief 比较两个字符串
 *        若 a 中字符串大于 b 中字符串，返回 1；相等返回 0； 否则返回 -1
 * 
 * @param a 
 * @param b 
 * @param size 
 * @return int8_t 
 */
int8_t strncmp(const char *a, const char *b, uint32_t size);

/**
 * @brief 从左到右查找字符串 str 中首次出现字符 ch 的地址
 * 
 * @param str 
 * @param ch 
 * @return char* 
 */
char* strchr(const char* str, const uint8_t ch);

/**
 * @brief 从后往前查找字符串 str 中首次出现字符 ch 的地址
 * 
 * @param str 
 * @param ch 
 * @return char* 
 */
char* strrchr(const char* str, const uint8_t ch);

/**
 * @brief 将字符串 str 拼接到 dst 后，返回拼接后的字符串
 * 
 * @param dst 
 * @param src 
 * @return char* 
 */
char* strcat(char* dst, const char* src);

/**
 * @brief 在字符串 str 中查找字符 ch 出现的次数
 * 
 * @param str 
 * @param ch 
 * @return uint32_t 
 */
uint32_t strchrs(const char* str, uint8_t ch);

#endif  // LIB_STRING_H_

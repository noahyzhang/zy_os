/**
 * @file string.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-05-31
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "lib/string.h"
#include "kernel/global.h"
#include "kernel/debug.h"

void memset(void* dst, uint8_t value, uint32_t size) {
    ASSERT(dst != NULL);
    uint8_t* dst_dummy = (uint8_t*)dst;
    for (; size-- > 0;) {
        *dst_dummy++ = value;
    }
}

void memcpy(void* dst, const void* src, uint32_t size) {
    ASSERT(dst != NULL && src != NULL);
    uint8_t* dst_dummy = (uint8_t*)dst;
    const uint8_t* src_dummy = (const uint8_t*)src;
    for (; size-- > 0;) {
        *dst_dummy++ = *src_dummy++;
    }
}

int memcmp(const void* a, const void* b, uint32_t size) {
    ASSERT(a != NULL || b != NULL);
    const char* a_dummy = (const char*)a;
    const char* b_dummy = (const char*)b;
    for (; size-- > 0;) {
        if (*a_dummy != *b_dummy) {
            return *a_dummy > *b_dummy ? 1 : -1;
        }
        a_dummy++;
        b_dummy++;
    }
    return 0;
}

char* strncpy(char* dst, const char* src, uint32_t size) {
    ASSERT(dst != NULL && src != NULL);
    char* res = dst;
    for (; (size-- > 0) && (*dst++ = *src++);) {}
    return res;
}

uint32_t strlen(const char* str) {
    ASSERT(str != NULL);
    const char* p = str;
    for (; *p++;) {}
    return (p - str - 1);
}

int8_t strncmp(const char *a, const char *b, uint32_t size) {
    ASSERT(a != NULL && b != NULL);
    for (; (size-- > 0) && (*a != 0) && (*a == *b);) {
        a++;
        b++;
    }
    // 如果 *a 小于 *b，就返回 -1
    // 否则就属于 *a 大于等于 *b 的情况。
    // 若 *a 大于 *b，则表达式为 true，返回 1；否则表达式不成立，也就是为 false，返回 0
    return *a < *b ? -1 : *a > *b;
}

char* strchr(const char* str, const uint8_t ch) {
    ASSERT(str != NULL);
    for (; *str != 0;) {
        if (*str == ch) {
            return (char*)str;
        }
        str++;
    }
    return NULL;
}

char* strrchr(const char* str, const uint8_t ch) {
    ASSERT(str != NULL);
    const char* last_char = NULL;
    // 从头到尾遍历，若存在 ch 字符，last_char 会记录该字符的最后一次出现在 str 中的地址
    for (; *str != 0;) {
        if (*str == ch) {
            last_char = str;
        }
        str++;
    }
    return (char*)last_char;
}

char* strcat(char* dst, const char* src) {
    ASSERT(dst != NULL && src != NULL);
    char* str_dummy = dst;
    for (; *str_dummy++;) {}
    --str_dummy;
    // 当 str_dummy 被赋值为 0 时，循环结束
    for (; (*str_dummy++ = *src++);) {}
    return dst;
}

uint32_t strchrs(const char* str, uint8_t ch) {
    ASSERT(str != NULL);
    uint32_t ch_cnt = 0;
    const char* p = str;
    for (; *p != 0;) {
        if (*p == ch) {
            ch_cnt++;
        }
        p++;
    }
    return ch_cnt;
}

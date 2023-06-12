#include "lib/stdio.h"
#include "kernel/global.h"
#include "lib/string.h"
#include "lib/user/syscall.h"
#include "lib/kernel/print.h"
#include "kernel/interrupt.h"

// 把 ap 指向第一个固定参数 v
#define va_start(ap, v) ap = (va_list)&v
// ap 指向下一个参数并返回其值
#define va_arg(ap, t) *((t*)(ap += 4))
// 清除 ap
#define va_end(ap) ap = NULL

/**
 * @brief 将整形转换成字符（ASCII）
 * 
 * @param value 
 * @param buf_ptr_addr 
 * @param base 
 */
static void itoa(uint32_t value, char** buf_ptr_addr, uint8_t base) {
    // 求模，最先掉下来的是最低位
    uint32_t m = value % base;
    // 取整
    uint32_t i = value / base;
    // 如果倍数不为 0 则递归调用
    if (i) {
        itoa(i, buf_ptr_addr, base);
    }
    // 如果余数是 0-9
    if (m < 10) {
        // 将数字 0-9 转换为字符 '0'-'9'
        *((*buf_ptr_addr)++) = m + '0';
    } else {  // 否则余数是 A-F
        // 将数字 A-F 转换为字符 'A'-'F'
        *((*buf_ptr_addr)++) = m - 10 + 'A';
    }
}

/**
 * @brief 将参数 ap 按照格式 format 输出到字符串 str，并返回替换后 str 的长度
 * 
 * @param str 
 * @param format 
 * @param ap 
 * @return uint32_t 
 */
uint32_t vsprintf(char* str, const char* format, va_list ap) {
    char* buf_ptr = str;
    const char* index_ptr = format;
    char index_char = *index_ptr;
    int32_t arg_int;
    for (; index_char;) {
        if (index_char != '%') {
            *(buf_ptr++) = index_char;
            index_char = *(++index_ptr);
            continue;
        }
        // 得到 % 后面的字符
        index_char = *(++index_ptr);
        switch (index_char) {
        case 'x':
            arg_int = va_arg(ap, int);
            itoa(arg_int, &buf_ptr, 16);
            // 跳过格式字符并更新 index_char
            index_char = *(++index_ptr);
            break;
        }
    }
    return strlen(str);
}

/**
 * @brief 格式化输出字符串 format
 * 
 * @param format 
 * @param ... 
 * @return uint32_t 
 */
uint32_t printf(const char* format, ...) {
    va_list args;
    // 使 args 指向 format
    va_start(args, format);
    // 存储拼接后的字符串
    char buf[1024] = {0};
    vsprintf(buf, format, args);
    va_end(args);
    return write(buf);
}

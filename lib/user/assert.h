/**
 * @file assert.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-25
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef LIB_USER_ASSERT_H_
#define LIB_USER_ASSERT_H_

#define NULL ((void*)0)

void user_spin(char* filename, int line, const char* func, const char* condition);

#define panic(...) user_spin(__FILE__, __LINE__, __func__, __VA_ARGS__)

#ifdef NDEBUG
    #define assert(CONDITION) ((void)0)
#else
    #define assert(CONDITION) \
        if (!(CONDITION)) { \
            panic(#CONDITION); \
        }
#endif  // NDEBUG

#endif  // LIB_USER_ASSERT_H_

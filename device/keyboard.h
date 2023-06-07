/**
 * @file keyboard.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-07
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef DEVICE_KEYBOARD_H_
#define DEVICE_KEYBOARD_H_

#include "device/io_queue.h"

extern struct io_queue_t kbd_buf;

void keyboard_init(void);

#endif  // DEVICE_KEYBOARD_H_

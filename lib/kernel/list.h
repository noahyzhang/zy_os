/**
 * @file list.h
 * @author your name (you@domain.com)
 * @brief 双向链表
 * @version 0.1
 * @date 2023-06-03
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef LIB_KERNEL_LIST_H_
#define LIB_KERNEL_LIST_H_

#include "kernel/global.h"

// elem_ptr 是待转换的地址
// struct_member_name 是 elem_ptr 所属结构体的类型
// elem2entry 的作用是将指针 elem_ptr 转换成 struct_type 类型的指针
// 原理是用 elem_ptr 的地址减去 elem_ptr 在结构体 struct_type 中的偏移量，此地址差便是 struct_type 的起始地址
// 最后再将此地址差转换为 struct_type 指针类型
#define offset(struct_type, member) (int)(&((struct_type*)0)->member)
#define elem2entry(struct_type, struct_member_name, elem_ptr) \
        (struct_type*)((int)elem_ptr - offset(struct_type, struct_member_name))

/**********   定义链表结点成员结构   ***********
*结点中不需要数据成元,只要求前驱和后继结点指针*/
struct list_elem {
    // 前躯结点
    struct list_elem* prev;
    // 后继结点
    struct list_elem* next;
};

/* 链表结构,用来实现队列 */
struct list {
    /* head是队首,是固定不变的，不是第1个元素,第1个元素为head.next */
    struct list_elem head;
    /* tail是队尾,同样是固定不变的 */
    struct list_elem tail;
};

/* 自定义函数类型function,用于在list_traversal中做回调函数 */
typedef bool (function)(struct list_elem*, int arg);

void list_init(struct list*);
void list_insert_before(struct list_elem* before, struct list_elem* elem);
void list_push(struct list* plist, struct list_elem* elem);
void list_iterate(struct list* plist);
void list_append(struct list* plist, struct list_elem* elem);
void list_remove(struct list_elem* pelem);
struct list_elem* list_pop(struct list* plist);
bool list_empty(struct list* plist);
uint32_t list_len(struct list* plist);
struct list_elem* list_traversal(struct list* plist, function func, int arg);
bool elem_find(struct list* plist, struct list_elem* obj_elem);

#endif  // LIB_KERNEL_LIST_H_

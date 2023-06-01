#include "kernel/memory.h"
#include "lib/kernel/print.h"

#define PAGE_SIZE (4096)

// 因为 0xc009f000 是内核主线程栈顶，0xc009e000 是内核主线程的 pcb
// 使用一页 4096 字节来保存一个位图，那么此位图一定有 (4096 * 8 = 32768) 位
// 又因为位图中一位表示一页，那么此位图有 32768 位，可以表示 32768 个页，也即(32768 * 4096 = 134217728)字节，也即 128M
// 我们一共有 4 个这样的位图，总共可以表示 512M 虚拟内存
// 位图地址
#define MEM_BITMAP_BASE 0xc009a000

// 0xc0000000 是内核从虚拟地址 3G 起
// 0x100000 跨过低端 1MB 内存，使虚拟地址在逻辑上连续
#define K_HEAP_START 0xc0100000

/**
 * @brief 内存池结构
 *        有两个实例，用于管理内核内存池和用户内存池
 */
struct pool {
    // 本内存池用到的位图结构，用于管理物理内存
    struct bitmap pool_bitmap;
    // 本内存池所管理物理内存的起始地址
    uint32_t phy_addr_start;
    // 本内存池字节容量
    uint32_t pool_size;
};

// 生成内核内存池和用户内存池
struct pool kernel_pool, user_pool;

// 此结构用来给内核分配虚拟地址
struct virtual_addr kernel_vaddr;

/**
 * @brief 初始化内存池
 * 
 * @param all_mem 
 */
static void mem_pool_init(uint32_t all_mem) {
    put_str("mem_pool_init start\n");
    // 页表大小 = 1页的页目录表 + 第 0 和第 768 个页目录项指向同一个页表 + 第 769 - 1022 个页目录项共指向 254 个页表
    // 一共 256 个页面
    uint32_t page_table_size = PAGE_SIZE * 256;
    // 0x100000 为低端 1MB 内存
    uint32_t used_mem = page_table_size + 0x100000;
    uint32_t free_mem = all_mem - used_mem;
    // 1 页为 4KB，不管总内存是不是 4K 的倍数
    // 对于以页为单位的内存分配策略，不足 1 页的内存不用考虑
    uint16_t all_free_pages = free_mem / PAGE_SIZE;
    uint16_t kernel_free_pages = all_free_pages / 2;
    uint16_t user_free_pages = all_free_pages - kernel_free_pages;

    // 为了简化位图操作，余数不做处理，可能会导致丢内存。但是不用做内存的越界检查，因为位图表示的内存少于实际物理内存
    // kernel bitmap 的长度，位图中一位表示一页，以字节为单位
    uint32_t kbm_length = kernel_free_pages / 8;
    // user bitmap 的长度
    uint32_t ubm_length = user_free_pages / 8;

    // kernel pool start, 内核内存池的起始地址
    uint32_t kp_start = used_mem;
    // user pool start, 用户内存池的起始地址
    uint32_t up_start = kp_start + kernel_free_pages * PAGE_SIZE;

    kernel_pool.phy_addr_start = kp_start;
    user_pool.phy_addr_start = up_start;
    kernel_pool.pool_size = kernel_free_pages * PAGE_SIZE;
    user_pool.pool_size = user_free_pages * PAGE_SIZE;
    kernel_pool.pool_bitmap.bmap_bytes_len = kbm_length;
    user_pool.pool_bitmap.bmap_bytes_len = ubm_length;

    kernel_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE;
    user_pool.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length);

    put_str("kernel_pool_bitmap_start:");
    put_int((int)kernel_pool.pool_bitmap.bits);
    put_str("  kernel_pool_phy_addr_start:");
    put_int(kernel_pool.phy_addr_start);
    put_str("\n");
    put_str("user_pool_bitmap_start:");
    put_int((int)user_pool.pool_bitmap.bits);
    put_str("  user_pool_phy_addr_start:");
    put_int(user_pool.phy_addr_start);
    put_str("\n");

    // 将位图置 0
    bitmap_init(&kernel_pool.pool_bitmap);
    bitmap_init(&user_pool.pool_bitmap);

    kernel_vaddr.vaddr_bitmap.bmap_bytes_len = kbm_length;
    kernel_vaddr.vaddr_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);

    kernel_vaddr.vaddr_start = K_HEAP_START;
    bitmap_init(&kernel_vaddr.vaddr_bitmap);
    put_str("mem_pool_init done\n");
}

void mem_init() {
    put_str("mem_init start\n");
    uint32_t mem_bytes_total = (*(uint32_t*)(0xb00));
    mem_pool_init(mem_bytes_total);
    put_str("mem_init done\n");
}


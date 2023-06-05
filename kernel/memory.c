#include "kernel/memory.h"
#include "lib/kernel/print.h"
#include "lib/kernel/bitmap.h"
#include "lib/stdint.h"
#include "kernel/global.h"
#include "kernel/debug.h"
#include "lib/string.h"

// 因为 0xc009f000 是内核主线程栈顶，0xc009e000 是内核主线程的 pcb
// 使用一页 4096 字节来保存一个位图，那么此位图一定有 (4096 * 8 = 32768) 位
// 又因为位图中一位表示一页，那么此位图有 32768 位，可以表示 32768 个页，也即(32768 * 4096 = 134217728)字节，也即 128M
// 我们一共有 4 个这样的位图，总共可以表示 512M 虚拟内存
// 位图地址
#define MEM_BITMAP_BASE 0xc009a000

// 0xc0000000 是内核从虚拟地址 3G 起
// 0x100000 跨过低端 1MB 内存，使虚拟地址在逻辑上连续
#define K_HEAP_START 0xc0100000

// 页目录项索引
#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
// 页表项索引
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

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
 * @brief 在 pf 表示的虚拟内存池中申请 pg_cnt 个虚拟页
 *        成功时则返回虚拟页的起始地址，失败则返回 NULL
 * 
 * @param pf 
 * @param pg_cnt 
 * @return void* 
 */
static void* vaddr_get(enum pool_flags pf, uint32_t pg_cnt) {
    int vaddr_start = 0, bit_idx_start = -1;
    uint32_t cnt = 0;
    if (pf == PF_KERNEL) {
        put_str("vaddr_get kernel_vaddr.vaddr_bitmap.bits: ");
        put_int((int)kernel_vaddr.vaddr_bitmap.bits);
        put_str("\n");

        put_str("vaddr_get kernel_vaddr.vaddr_bitmap.bmap_bytes_len :");
        put_int(kernel_vaddr.vaddr_bitmap.bmap_bytes_len);
        put_str("\n");

        bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);
        if (bit_idx_start == -1) {
            return NULL;
        }
        // asm volatile ("xchg %%bx, %%bx" ::);
        for (; cnt < pg_cnt;) {
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
        }
        vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PAGE_SIZE;
    } else {
        // TODO(noahyzhang): 用户内存池
    }
    return (void*)vaddr_start;
}

/**
 * @brief 得到虚拟地址 vaddr 对应的 PTE 指针
 * 
 * @param vaddr 
 * @return uint32_t* 
 */
uint32_t* pte_ptr(uint32_t vaddr) {
    // 先访问到页表自己
    // 在用页目录项 PDE（页目录内页表的索引）作为 PTE 的索引访问到页表
    // 再用 PTE 的索引作为页内偏移
    uint32_t* pte = (uint32_t*)(0xffc00000 + ((vaddr & 0xffc00000) >> 10) + PTE_IDX(vaddr) * 4);
    return pte;
}

/**
 * @brief 得到虚拟地址 vaddr 对应的 PDE 指针
 * 
 * @param vaddr 
 * @return uint32_t* 
 */
uint32_t* pde_ptr(uint32_t vaddr) {
    // 0xfffff 用来访问页表本身所在的地址
    uint32_t* pde = (uint32_t*)((0xfffff000) + PDE_IDX(vaddr) * 4);
    return pde;
}

static void* palloc(struct pool* m_pool) {
    int bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1);
    if (bit_idx == -1) {
        return NULL;
    }
    bitmap_set(&m_pool->pool_bitmap, bit_idx, 1);
    uint32_t page_phyaddr = ((bit_idx * PAGE_SIZE) + m_pool->phy_addr_start);
    return (void*)page_phyaddr;
}

/* 页表中添加虚拟地址_vaddr与物理地址_page_phyaddr的映射 */
static void page_table_add(void* _vaddr, void* _page_phyaddr) {
   uint32_t vaddr = (uint32_t)_vaddr, page_phyaddr = (uint32_t)_page_phyaddr;
   uint32_t* pde = pde_ptr(vaddr);
   uint32_t* pte = pte_ptr(vaddr);

/************************   注意   *************************
 * 执行*pte,会访问到空的pde。所以确保pde创建完成后才能执行*pte,
 * 否则会引发page_fault。因此在*pde为0时,*pte只能出现在下面else语句块中的*pde后面。
 * *********************************************************/
   /* 先在页目录内判断目录项的P位，若为1,则表示该表已存在 */
   if (*pde & 0x00000001) {	 // 页目录项和页表项的第0位为P,此处判断目录项是否存在
      ASSERT(!(*pte & 0x00000001));

      if (!(*pte & 0x00000001)) {   // 只要是创建页表,pte就应该不存在,多判断一下放心
	 *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);    // US=1,RW=1,P=1
      } else {			    //应该不会执行到这，因为上面的ASSERT会先执行。
	 PANIC("pte repeat");
	 *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);      // US=1,RW=1,P=1
      }
   } else {			    // 页目录项不存在,所以要先创建页目录再创建页表项.
      /* 页表中用到的页框一律从内核空间分配 */
      uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);

      *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);

      /* 分配到的物理页地址pde_phyaddr对应的物理内存清0,
       * 避免里面的陈旧数据变成了页表项,从而让页表混乱.
       * 访问到pde对应的物理地址,用pte取高20位便可.
       * 因为pte是基于该pde对应的物理地址内再寻址,
       * 把低12位置0便是该pde对应的物理页的起始*/
      memset((void*)((int)pte & 0xfffff000), 0, PAGE_SIZE);
         
      ASSERT(!(*pte & 0x00000001));
      *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);      // US=1,RW=1,P=1
   }
}

/* 分配pg_cnt个页空间,成功则返回起始虚拟地址,失败时返回NULL */
void* malloc_page(enum pool_flags pf, uint32_t pg_cnt) {
   ASSERT(pg_cnt > 0 && pg_cnt < 3840);
/***********   malloc_page的原理是三个动作的合成:   ***********
      1通过vaddr_get在虚拟内存池中申请虚拟地址
      2通过palloc在物理内存池中申请物理页
      3通过page_table_add将以上得到的虚拟地址和物理地址在页表中完成映射
***************************************************************/
   void* vaddr_start = vaddr_get(pf, pg_cnt);
   if (vaddr_start == NULL) {
      return NULL;
   }

   uint32_t vaddr = (uint32_t)vaddr_start, cnt = pg_cnt;
   struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;

   /* 因为虚拟地址是连续的,但物理地址可以是不连续的,所以逐个做映射*/
   while (cnt-- > 0) {
      void* page_phyaddr = palloc(mem_pool);
      if (page_phyaddr == NULL) {  // 失败时要将曾经已申请的虚拟地址和物理页全部回滚，在将来完成内存回收时再补充
	 return NULL;
      }
      page_table_add((void*)vaddr, page_phyaddr); // 在页表中做映射
      vaddr += PAGE_SIZE;		 // 下一个虚拟页
   }
   return vaddr_start;
}

/* 从内核物理内存池中申请pg_cnt页内存,成功则返回其虚拟地址,失败则返回NULL */
void* get_kernel_pages(uint32_t pg_cnt) {
   void* vaddr =  malloc_page(PF_KERNEL, pg_cnt);
   if (vaddr != NULL) {	   // 若分配的地址不为空,将页框清0后返回
      memset(vaddr, 0, pg_cnt * PAGE_SIZE);
   }
   return vaddr;
}

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

    // put_str("kernel_pool_bitmap_start:");
    // put_int((int)kernel_pool.pool_bitmap.bits);
    // put_str("  kernel_pool_phy_addr_start:");
    // put_int(kernel_pool.phy_addr_start);
    // put_str("\n");
    // put_str("user_pool_bitmap_start:");
    // put_int((int)user_pool.pool_bitmap.bits);
    // put_str("  user_pool_phy_addr_start:");
    // put_int(user_pool.phy_addr_start);
    // put_str("\n");

    // 将位图置 0
    bitmap_init(&kernel_pool.pool_bitmap);
    bitmap_init(&user_pool.pool_bitmap);

    kernel_vaddr.vaddr_bitmap.bmap_bytes_len = kbm_length;
    kernel_vaddr.vaddr_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);
    kernel_vaddr.vaddr_start = K_HEAP_START;

    put_str("kernel_vaddr.vaddr_bitmap.bits: ");
    put_int((int)kernel_vaddr.vaddr_bitmap.bits);
    put_str("\n");

    put_str("kernel_vaddr.vaddr_bitmap.bmap_bytes_len :");
    put_int(kernel_vaddr.vaddr_bitmap.bmap_bytes_len);
    put_str("\n");

    put_str("kernel_vaddr.vaddr_start:");
    put_int(kernel_vaddr.vaddr_start);
    put_str("\n");

    bitmap_init(&kernel_vaddr.vaddr_bitmap);
    put_str("mem_pool_init done\n");

    // asm volatile("xchg %%bx, %%bx"::);
}

void mem_init(void) {
    put_str("mem_init start\n");
    uint32_t mem_bytes_total = (*(uint32_t*)(0xb00));
    mem_pool_init(mem_bytes_total);
    put_str("mem_init done\n");
}

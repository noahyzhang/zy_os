#include "kernel/memory.h"
#include "lib/kernel/print.h"
#include "lib/kernel/bitmap.h"
#include "lib/stdint.h"
#include "kernel/global.h"
#include "kernel/debug.h"
#include "lib/string.h"
#include "thread/sync.h"
#include "kernel/interrupt.h"

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
    // 申请内存时互斥
    struct lock lock;
};

/**
 * @brief 内存 arena 的元信息
 * 
 */
struct arena {
    // 此 arena 关联的 mem_block_desc
    struct mem_block_desc* desc;
    // large 为 true 时，cnt 表示页数
    // 否则 cnt 表示空闲 mem_block 数量
    uint32_t cnt;
    bool large;
};

// 内核内存块描述符数组
struct mem_block_desc k_block_descs[DESC_CNT];
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
    // 内核内存池
    if (pf == PF_KERNEL) {
        // put_str("vaddr_get kernel_vaddr.vaddr_bitmap.bits: ");
        // put_int((int)kernel_vaddr.vaddr_bitmap.bits);
        // put_str("\n");

        // put_str("vaddr_get kernel_vaddr.vaddr_bitmap.bmap_bytes_len :");
        // put_int(kernel_vaddr.vaddr_bitmap.bmap_bytes_len);
        // put_str("\n");

        bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);
        if (bit_idx_start == -1) {
            return NULL;
        }
        // asm volatile ("xchg %%bx, %%bx" ::);
        for (; cnt < pg_cnt;) {
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
        }
        vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PAGE_SIZE;
    } else { // 用户内存池
        struct task_struct* cur = running_thread();
        bit_idx_start = bitmap_scan(&cur->user_process_vaddr.vaddr_bitmap, pg_cnt);
        if (bit_idx_start == -1) {
            return NULL;
        }
        for (; cnt < pg_cnt;) {
            bitmap_set(&cur->user_process_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
        }
        vaddr_start = cur->user_process_vaddr.vaddr_start + bit_idx_start * PAGE_SIZE;
        // (0xc0000000 - PAGE_SIZE) 作为用户 3 级栈已经在 start_process 被分配
        ASSERT((uint32_t)vaddr_start < (0xc0000000 - PAGE_SIZE));

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
    lock_acquire(&kernel_pool.lock);
    void* vaddr =  malloc_page(PF_KERNEL, pg_cnt);
    if (vaddr != NULL) {	   // 若分配的地址不为空,将页框清0后返回
        memset(vaddr, 0, pg_cnt * PAGE_SIZE);
    }
    lock_release(&kernel_pool.lock);
    return vaddr;
}

/**
 * @brief 在用户空间中申请 4K 内存，并返回其虚拟地址
 * 
 * @param pg_cnt 
 * @return void* 
 */
void* get_user_pages(uint32_t pg_cnt) {
    lock_acquire(&user_pool.lock);
    void* vaddr = malloc_page(PF_USER, pg_cnt);
    if (vaddr != NULL) {
        memset(vaddr, 0, pg_cnt * PAGE_SIZE);
    }
    lock_release(&user_pool.lock);
    return vaddr;
}

/**
 * @brief 将地址 vaddr 与 pf 池中的物理地址关联
 *        仅支持一页空间分配
 * 
 * @param pf 
 * @param vaddr 
 * @return void* 
 */
void* get_a_page(enum pool_flags pf, uint32_t vaddr) {
    struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
    lock_acquire(&mem_pool->lock);
    // 先将虚拟地址对应的位图置 1
    struct task_struct* cur = running_thread();
    int32_t bit_idx = -1;
    // 若当前是用户进程申请用户内存，就修改用户进程自己的虚拟地址位图
    if (cur->pg_dir != NULL && pf == PF_USER) {
        bit_idx = (vaddr - cur->user_process_vaddr.vaddr_start) / PAGE_SIZE;
        ASSERT(bit_idx > 0);
        bitmap_set(&cur->user_process_vaddr.vaddr_bitmap, bit_idx, 1);
    } else if (cur->pg_dir == NULL && pf == PF_KERNEL) {
        // 如果是内核线程申请内核内存,就修改kernel_vaddr.
        bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PAGE_SIZE;
        ASSERT(bit_idx > 0);
        bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx, 1);
    } else {
        PANIC("get_a_page:not allow kernel alloc userspace or user alloc kernel space by get_a_page");
    }
    void* page_phyaddr = palloc(mem_pool);
    if (page_phyaddr == NULL) {
        return NULL;
    }
    page_table_add((void*)vaddr, page_phyaddr);
    lock_release(&mem_pool->lock);
    return (void*)vaddr;
}

/**
 * @brief 获取物理地址（通过虚拟地址映射所得）
 * 
 * @param vaddr 
 * @return uint32_t 
 */
uint32_t addr_v2p(uint32_t vaddr) {
    uint32_t* pte = pte_ptr(vaddr);
    // (*pte)的值是页表所在的物理页框地址,
    // 去掉其低12位的页表项属性+虚拟地址vaddr的低12位 */
    return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
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

    lock_init(&kernel_pool.lock);
    lock_init(&user_pool.lock);

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

/**
 * @brief 初始化内存块元数据
 *        因此我们的内存块规格：16，32，64，128，256，512，1024
 * @param desc_array 
 */
void block_desc_init(struct mem_block_desc* desc_array) {
    uint16_t desc_idx, block_size = 16;
    // 初始化每个 mem_block_desc 描述符
    for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++) {
        desc_array[desc_idx].block_size = block_size;
        // 初始化 arena 中的内存块数量
        desc_array[desc_idx].blocks_per_arena = (PAGE_SIZE - sizeof(struct arena)) / block_size;
        list_init(&desc_array[desc_idx].free_list);
        // 更新为下一个规格内存块
        block_size *= 2;
    }
}

/**
 * @brief 返回 arena 中第 idx 个内存块的地址
 * 
 * @param a 
 * @param idx 
 * @return struct mem_block* 
 */
static struct mem_block* arena2block(struct arena* a, uint32_t idx) {
    return (struct mem_block*)((uint32_t)a + sizeof(struct arena) + idx * a->desc->block_size);
}

/**
 * @brief 返回内存块所在的 arena 地址
 * 
 * @param b 
 * @return struct arena* 
 */
static struct arena* block2arena(struct mem_block* b) {
    return (struct arena*)((uint32_t)b & 0xfffff000);
}

/**
 * @brief 在堆中申请 size 字节内存
 * 
 * @param size 
 * @return void* 
 */
void* sys_malloc(uint32_t size) {
    enum pool_flags PF;
    struct pool* mem_pool;
    uint32_t pool_size;
    struct mem_block_desc* descs;
    struct task_struct* cur_thread = running_thread();
    // 判断用那个内存池
    // 如果是内核线程
    if (cur_thread->pg_dir == NULL) {
        PF = PF_KERNEL;
        pool_size = kernel_pool.pool_size;
        mem_pool = &kernel_pool;
        descs = k_block_descs;
    } else {  // 用户进程 PCB 中的 pgdir 会在为其分配页表时创建
        PF = PF_USER;
        pool_size = user_pool.pool_size;
        mem_pool = &user_pool;
        descs = cur_thread->u_block_desc;
    }
    // 校验参数
    // 如果申请的内存不在内存池容量范围内则直接返回 NULL
    if (!(size > 0 && size < pool_size)) {
        return NULL;
    }
    struct arena* a;
    struct mem_block* b;
    lock_acquire(&mem_pool->lock);
    if (size > 1024) {
        // 向上取整需要的页面数
        uint32_t page_cnt = DIV_ROUND_UP(size + sizeof(struct arena), PAGE_SIZE);
        a = malloc_page(PF, page_cnt);
        if (a != NULL) {
            // 将分配的内存清零
            memset(a, 0, page_cnt * PAGE_SIZE);
            // 对于分配的大块页，将 desc 置为 NULL，cnt 置为页数，large 置为 true
            a->desc = NULL;
            a->cnt = page_cnt;
            a->large = true;
            lock_release(&mem_pool->lock);
            // 跨过 arena 大小，把剩下的内存返回
            return (void*)(a + 1);
        } else {
            lock_release(&mem_pool->lock);
            return NULL;
        }
    } else {  // 如果申请的内存小于 1024，可在各种规格的 mem_block_desc 中去适配
        uint8_t desc_idx;
        // 从内存块描述符中匹配合适的内存块规格
        for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++) {
            // 从小往大找，找到后退出
            if (size <= descs[desc_idx].block_size) {
                break;
            }
        }
        // 如果 mem_block_desc 的 free_list 中已经没有可用的 mem_block
        // 就创建新的 arena 提供 mem_block
        if (list_empty(&descs[desc_idx].free_list)) {
            // 分配一页作为 arena
            a = malloc_page(PF, 1);
            if (a == NULL) {
                lock_release(&mem_pool->lock);
                return NULL;
            }
            memset(a, 0, PAGE_SIZE);
            // asm volatile ("xchg %%bx, %%bx" ::);
            // 对于分配的小块内存，将 desc 置为相应内存块描述符
            // cnt 置为此 arena 可用的内存块数，large 置为 false
            a->desc = &descs[desc_idx];
            a->large = false;
            a->cnt = descs[desc_idx].blocks_per_arena;
            uint32_t block_idx;
            // 开始将 arena 拆分成内存块，并添加到内存块描述符的 free_list 中
            // 在做这个操作前记得暂停中断
            enum intr_status old_status = intr_disable();
            for (block_idx = 0; block_idx < descs[desc_idx].blocks_per_arena; block_idx++) {
                b = arena2block(a, block_idx);
                ASSERT(!elem_find(&a->desc->free_list, &b->free_elem));
                list_append(&a->desc->free_list, &b->free_elem);
            }
            intr_set_status(old_status);
        }
        // 开始分配内存块
        b = elem2entry(struct mem_block, free_elem, list_pop(&(descs[desc_idx].free_list)));
        memset(b, 0, descs[desc_idx].block_size);
        // 获取内存块所在的 arena
        a = block2arena(b);
        // 将此 arena 中的空闲内存块数减一
        a->cnt--;
        lock_release(&mem_pool->lock);
        return (void*)b;
    }
}

/**
 * @brief 将物理地址 pg_phy_addr 回收到物理内存池
 * 
 * @param pg_phy_addr 
 */
void pfree(uint32_t pg_phy_addr) {
    struct pool* mem_pool;
    uint32_t bit_idx = 0;
    // 用户物理内存池
    if (pg_phy_addr >= user_pool.phy_addr_start) {
        mem_pool = &user_pool;
        bit_idx = (pg_phy_addr - user_pool.phy_addr_start) / PAGE_SIZE;
    } else {  // 内核物理内存池
        mem_pool = &kernel_pool;
        bit_idx = (pg_phy_addr - kernel_pool.phy_addr_start) / PAGE_SIZE;
    }
    // 将位图中该位清 0
    bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0);
}

static void page_table_pte_remove(uint32_t vaddr) {
    uint32_t* pte = pte_ptr(vaddr);
    // 将页表项 pte 的 P 位置 0
    *pte &= ~PG_P_1;
    // 更新 tlb
    asm volatile("invlpg %0" :: "m" (vaddr):"memory");
}

static void vaddr_remove(enum pool_flags pf, void* p_vaddr, uint32_t pg_cnt) {
    uint32_t bit_idx_start = 0;
    uint32_t i_vaddr = (uint32_t)p_vaddr;
    uint32_t cnt = 0;
    // 内核虚拟内存池
    if (pf == PF_KERNEL) {
        bit_idx_start = (i_vaddr - kernel_vaddr.vaddr_start) / PAGE_SIZE;
        for (; cnt < pg_cnt;) {
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
        }
    } else {  // 用户虚拟内存池
        struct task_struct* cur_thread = running_thread();
        bit_idx_start = (i_vaddr - cur_thread->user_process_vaddr.vaddr_start) / PAGE_SIZE;
        for (; cnt < pg_cnt;) {
            bitmap_set(&cur_thread->user_process_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
        }
    }
}

void mfree_page(enum pool_flags pf, void* p_vaddr, uint32_t pg_cnt) {
    uint32_t pg_phy_addr;
    uint32_t i_vaddr = (uint32_t)p_vaddr;
    uint32_t page_cnt = 0;
    ASSERT((pg_cnt >= 1) && ((i_vaddr % PAGE_SIZE) == 0));
    // 获取虚拟地址 p_vaddr 对应的物理地址
    pg_phy_addr = addr_v2p(i_vaddr);
    // 确保待释放的物理内存在低端 1M+1K 大小的页目录+1k大小的页表地址范围外
    ASSERT((pg_phy_addr % PAGE_SIZE) == 0 && pg_phy_addr >= 0x102000);
    // 判断 pg_phy_addr 属于用户物理内存池还是内核物理内存池
    if (pg_phy_addr >= user_pool.phy_addr_start) {   // 位于user_pool内存池
        i_vaddr -= PAGE_SIZE;
        for (; page_cnt < pg_cnt;) {
            i_vaddr += PAGE_SIZE;
            pg_phy_addr = addr_v2p(i_vaddr);
            // 确保物理地址属于用户物理内存池
            ASSERT((pg_phy_addr % PAGE_SIZE) == 0 && pg_phy_addr >= user_pool.phy_addr_start);
            // 先将对应的物理页框归还到内存池
            pfree(pg_phy_addr);
            // 再从页表中清除此虚拟地址所在的页表项 pte
            page_table_pte_remove(i_vaddr);

            page_cnt++;
        }
        // 清空虚拟地址的位图中的相应位 */
        vaddr_remove(pf, p_vaddr, pg_cnt);
    } else {  // 位于kernel_pool内存池
        i_vaddr -= PAGE_SIZE;
        for (; page_cnt < pg_cnt;) {
            i_vaddr += PAGE_SIZE;
            pg_phy_addr = addr_v2p(i_vaddr);
            // 确保待释放的物理内存只属于内核物理内存池
            ASSERT((pg_phy_addr % PAGE_SIZE) == 0 && \
                pg_phy_addr >= kernel_pool.phy_addr_start && \
                pg_phy_addr < user_pool.phy_addr_start);
            // 先将对应的物理页框归还到内存池
            pfree(pg_phy_addr);
            // 再从页表中清除此虚拟地址所在的页表项 pte
            page_table_pte_remove(i_vaddr);
            page_cnt++;
        }
        // 清空虚拟地址的位图中的相应位
        vaddr_remove(pf, p_vaddr, pg_cnt);
    }
}

/**
 * @brief 回收内存
 * 
 * @param ptr 
 */
void sys_free(void* ptr) {
    ASSERT(ptr != NULL);
    if (ptr == NULL) {
        return;
    }
    // 如下是 ptr 不为 NULL 的情况
    enum pool_flags PF;
    struct pool* mem_pool;

    // 判断是线程还是进程
    if (running_thread()->pg_dir == NULL) {
        ASSERT((uint32_t)ptr >= K_HEAP_START);
        PF = PF_KERNEL;
        mem_pool = &kernel_pool;
    } else {
        PF = PF_USER;
        mem_pool = &user_pool;
    }

    lock_acquire(&mem_pool->lock);
    struct mem_block* b = ptr;
    struct arena* a = block2arena(b);  // 把mem_block转换成arena,获取元信息
    ASSERT(a->large == 0 || a->large == 1);
    if (a->desc == NULL && a->large == true) {  // 大于1024的内存
        mfree_page(PF, a, a->cnt);
    } else {  // 小于等于1024的内存块
        // 先将内存块回收到free_list
        list_append(&a->desc->free_list, &b->free_elem);
        // 再判断此arena中的内存块是否都是空闲,如果是就释放 arena
        if (++a->cnt == a->desc->blocks_per_arena) {
            uint32_t block_idx;
            for (block_idx = 0; block_idx < a->desc->blocks_per_arena; block_idx++) {
                struct mem_block*  b = arena2block(a, block_idx);
                ASSERT(elem_find(&a->desc->free_list, &b->free_elem));
                list_remove(&b->free_elem);
            }
            mfree_page(PF, a, 1);
        }
    }
    lock_release(&mem_pool->lock);
}

void mem_init(void) {
    put_str("mem_init start\n");
    // 这里的 0x920 来自于 boot/loader.s 中计算出来的系统总内存的存放地址
    uint32_t mem_bytes_total = (*(uint32_t*)(0x920));
    mem_pool_init(mem_bytes_total);
    // 初始化 k_block_descs 数组
    block_desc_init(k_block_descs);
    put_str("mem_init done\n");
}


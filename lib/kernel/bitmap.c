#include "lib/kernel/bitmap.h"
#include "lib/stdint.h"
#include "lib/string.h"
#include "lib/kernel/print.h"
#include "kernel/interrupt.h"
#include "kernel/debug.h"

void bitmap_init(struct bitmap* bitmap) {
    memset(bitmap->bits, 0, bitmap->bmap_bytes_len);
}

bool bitmap_scan_test(struct bitmap* bmap, uint32_t bit_idx) {
    uint32_t byte_idx = bit_idx / 8;
    uint32_t bit_odd = bit_idx % 8;
    return (bmap->bits[byte_idx] & (BITMAP_MASK << bit_odd));
}

int bitmap_scan(struct bitmap* bmap, uint32_t cnt) {
    uint32_t idx_byte = 0;
    // 1 表示该位已分配，若为 0xff，则表示该字节内已无空闲位，向下一字节继续找
    // 逐字节比较
    while ((0xff == bmap->bits[idx_byte]) && (idx_byte < bmap->bmap_bytes_len)) {
        idx_byte++;
    }
    asm volatile ("xchg %%bx, %%bx" ::);
    put_str("idx_byte: ");
    put_int(idx_byte);
    put_str("\n");
    put_str("bmap->bmap_bytes_len: ");
    put_int(bmap->bmap_bytes_len);
    put_str("\n");
    ASSERT(idx_byte < bmap->bmap_bytes_len);
    if (idx_byte == bmap->bmap_bytes_len) {
        return -1;
    }
    // 若在位图数组范围内的某字节内找到了空闲位
    // 在该字节内逐位比对，返回空闲位的索引
    int idx_bit = 0;
    while ((uint8_t)(BITMAP_MASK << idx_bit) & bmap->bits[idx_byte]) {
        idx_bit++;
    }
    int bit_idx_start = idx_byte * 8 + idx_bit;
    if (cnt == 1) {
        return bit_idx_start;
    }
    uint32_t bit_left = (bmap->bmap_bytes_len * 8 - bit_idx_start);
    uint32_t next_bit = bit_idx_start + 1;
    uint32_t count = 1;
    // 先将其置为 -1，若找不到连续的位就直接返回
    bit_idx_start = -1;
    while (bit_left-- > 0) {
        if (!bitmap_scan_test(bmap, next_bit)) {
            // 如果 next_bit 为 0
            count++;
        } else {
            count = 0;
        }
        // 找到了连续的 cnt 个空位
        if (count == cnt) {
            bit_idx_start = next_bit - cnt + 1;
            break;
        }
        next_bit++;
    }
    return bit_idx_start;
}

void bitmap_set(struct bitmap* bmap, uint32_t bit_idx, int8_t value) {
    ASSERT((value == 0) || (value == 1));
    uint32_t byte_idx = bit_idx / 8;
    uint32_t bit_odd = bit_idx % 8;
    if (value) {
        bmap->bits[byte_idx] |= (BITMAP_MASK << bit_odd);
    } else {
        bmap->bits[byte_idx] &= ~(BITMAP_MASK << bit_odd);
    }
}

#include "device/keyboard.h"
#include "lib/kernel/print.h"
#include "kernel/interrupt.h"
#include "lib/kernel/io.h"
#include "kernel/global.h"
#include "device/io_queue.h"

// 键盘 buffer 寄存器端口号为 0x60
#define KBD_BUF_PORT 0x60

// 用转义字符定义部分控制字符
#define esc '\x1b'
#define backspace '\b'
// #define tab '\t'
#define tab '\x09'
#define enter '\r'
#define delete '\x7f'

// 对于不可见字符统一定义成 0
#define char_invisible 0
#define ctrl_l_char char_invisible
#define ctrl_r_char char_invisible
#define shift_l_char char_invisible
#define shift_r_char char_invisible
#define alt_l_char char_invisible
#define alt_r_char char_invisible
#define caps_lock_char char_invisible

// 定义控制字符的通码和断码
#define shift_l_make 0x2a
#define shift_r_make 0x36
#define alt_l_make 0x38
#define alt_r_make 0xe038
#define alt_r_break 0xe0b8
#define ctrl_l_make 0x1d
#define ctrl_r_make 0xe01d
#define ctrl_r_break 0xe09d
#define caps_lock_make 0x3a

// 记录相应键是否按下的状态
// ext_scan_code 用于记录 make_code 是否以 0xe0 开头
static bool ctrl_status, shift_status, alt_status, caps_lock_status, ext_scan_code;

// 键盘缓冲区
struct io_queue_t kbd_buf;

// 以通码 make_code 为索引的二维数组
static char keymap[][2] = {
    /* 扫描码   未与shift组合  与shift组合*/
    /* ---------------------------------- */
    /* 0x00 */  {0,	0},
    /* 0x01 */	{esc,	esc},
    /* 0x02 */	{'1',	'!'},
    /* 0x03 */	{'2',	'@'},
    /* 0x04 */	{'3',	'#'},
    /* 0x05 */	{'4',	'$'},		
    /* 0x06 */	{'5',	'%'},		
    /* 0x07 */	{'6',	'^'},		
    /* 0x08 */	{'7',	'&'},		
    /* 0x09 */	{'8',	'*'},		
    /* 0x0A */	{'9',	'('},		
    /* 0x0B */	{'0',	')'},		
    /* 0x0C */	{'-',	'_'},		
    /* 0x0D */	{'=',	'+'},		
    /* 0x0E */	{backspace, backspace},	
    /* 0x0F */	{tab,	tab},		
    /* 0x10 */	{'q',	'Q'},		
    /* 0x11 */	{'w',	'W'},		
    /* 0x12 */	{'e',	'E'},		
    /* 0x13 */	{'r',	'R'},		
    /* 0x14 */	{'t',	'T'},		
    /* 0x15 */	{'y',	'Y'},		
    /* 0x16 */	{'u',	'U'},		
    /* 0x17 */	{'i',	'I'},		
    /* 0x18 */	{'o',	'O'},		
    /* 0x19 */	{'p',	'P'},		
    /* 0x1A */	{'[',	'{'},		
    /* 0x1B */	{']',	'}'},		
    /* 0x1C */	{enter,  enter},
    /* 0x1D */	{ctrl_l_char, ctrl_l_char},
    /* 0x1E */	{'a',	'A'},		
    /* 0x1F */	{'s',	'S'},		
    /* 0x20 */	{'d',	'D'},		
    /* 0x21 */	{'f',	'F'},		
    /* 0x22 */	{'g',	'G'},		
    /* 0x23 */	{'h',	'H'},		
    /* 0x24 */	{'j',	'J'},		
    /* 0x25 */	{'k',	'K'},		
    /* 0x26 */	{'l',	'L'},		
    /* 0x27 */	{';',	':'},		
    /* 0x28 */	{'\'',	'"'},		
    /* 0x29 */	{'`',	'~'},		
    /* 0x2A */	{shift_l_char, shift_l_char},	
    /* 0x2B */	{'\\',	'|'},		
    /* 0x2C */	{'z',	'Z'},		
    /* 0x2D */	{'x',	'X'},		
    /* 0x2E */	{'c',	'C'},		
    /* 0x2F */	{'v',	'V'},		
    /* 0x30 */	{'b',	'B'},		
    /* 0x31 */	{'n',	'N'},		
    /* 0x32 */	{'m',	'M'},		
    /* 0x33 */	{',',	'<'},		
    /* 0x34 */	{'.',	'>'},		
    /* 0x35 */	{'/',	'?'},
    /* 0x36	*/	{shift_r_char, shift_r_char},
    /* 0x37 */	{'*',	'*'},    	
    /* 0x38 */	{alt_l_char, alt_l_char},
    /* 0x39 */	{' ',	' '},
    /* 0x3A */	{caps_lock_char, caps_lock_char}
    /*其它按键暂不处理*/
};

/**
 * @brief 键盘中断处理程序
 * 
 */
static void intr_keyboard_handler(void) {
    // put_str("receive keyboard interrupt\n");
    // 这次中断发生前的上一次中断，以下三个键是否有按下
    bool ctrl_down_last = ctrl_status;
    bool shift_down_last = shift_status;
    bool caps_lock_last = caps_lock_status;

    bool break_code;
    // 注意：必须要读取输出缓冲区寄存器，否则 8042 不再继续相应键盘中断
    // inb 返回的是从端口读取的数据
    uint16_t scan_code = inb(KBD_BUF_PORT);
    // put_str("receive keyboard scan_code:|");
    // put_int(scan_code);
    // put_str("|\n");
    // 若扫描码是 e0 开头的，表示此键的按下将产生多个扫描码
    // 所以马上结束此次中断处理函数，等待下一个扫描码进来
    if (scan_code == 0xe0) {
        // 打开 e0 标记
        ext_scan_code = true;
        return;
    }
    // 如果上次是以 0xe0 开头，将扫描码合并
    if (ext_scan_code) {
        scan_code = ((0xe000) | scan_code);
        // 关闭 e0 标记
        ext_scan_code = false;
    }
    // 获取 break_code
    break_code = ((scan_code & 0x0080) != 0);
    // 若是断码 break_code（按键弹起时产生的扫描码）
    if (break_code) {
        // 由于 ctrl_r 和 alt_r 的 make_code 和 break_code 都是两字节
        // 所以如下方法取 make_code，多字节的扫描码暂不处理
        // 得到 make_code（按键按下时产生的扫描码）
        uint16_t make_code = (scan_code &= 0xff7f);
        // 若是任意以下三个键弹起了，将状态置为 false
        if (make_code == ctrl_l_make || make_code == ctrl_r_make) {
            ctrl_status = false;
        } else if (make_code == shift_l_make || make_code == shift_r_make) {
            shift_status = false;
        } else if (make_code == alt_l_make || make_code == alt_r_make) {
            alt_status = false;
        }
        // 由于 caps_lock 不是弹起后关闭，所以需要单独处理
        // 这里直接返回结束此次中断处理程序
        return;
    } else if ((scan_code > 0x00 && scan_code < 0x3b) \
        || (scan_code == alt_r_make) || (scan_code == ctrl_r_make)) {
        // 如果是通码，只处理数组中定义的键以及 alt_right 和 ctrl 键，全是 make_code

        // 判断是否与 shift 组合，用来在一维数组中索引对应的字符
        bool shift = false;
        if ((scan_code < 0x0e) || (scan_code == 0x29) ||
            (scan_code == 0x1a) || (scan_code == 0x1b) ||
            (scan_code == 0x2b) || (scan_code == 0x27) ||
            (scan_code == 0x28) || (scan_code == 0x33) ||
            (scan_code == 0x34) || (scan_code == 0x35)) {
            // 如上判断是代表两个字母的键
            // 0x0e 数字 '0' - '9'，字符 '-', 字符 '='
            // 0x29 字符 '`'
            // 0x1a 字符 '['
            // 0x1b 字符 ']'
            // 0x2b 字符 '\\'
            // 0x27 字符 ';'
            // 0x28 字符 '\'
            // 0x33 字符 ','
            // 0x34 字符 '.'
            // 0x35 字符 '/'

            // 如果同时按下了 shift 键
            if (shift_down_last) {
                shift = true;
            }
        } else {
            // 默认为字母键

            // 如果 shift 和 caps_lock 同时按下
            if (shift_down_last && caps_lock_last) {
                shift = false;
            } else if (shift_down_last || caps_lock_last) {
                // 如果 shift 和 caps_lock 任意被按下
                shift = true;
            } else {
                shift = false;
            }
        }
        // 将扫描码的高字节置 0，主要是针对高字节是 e0 的扫描码
        uint8_t index = (scan_code &= 0x00ff);
        // 在数组中找到对应的字符
        char cur_char = keymap[index][shift];

        // 只处理 ASCII 码不为 0 的键
        if (cur_char) {
            // TODO(noahyzhang): tab 键为什么输出的是字符 'o'
            // TODO(noahyzhang): esc 键输出的 '<-' 符号
            // if (0x0f == index) {
            //     put_char(tab);
            //     put_str("\n");
            // }
            // put_char(cur_char);
            // 如果 kbd_buf 中未满并且待加入的 cur_char 不为 0，则将其加入到缓冲区 kbd_buf 中
            if (!ioq_full(&kbd_buf)) {
                // 用来调试
                // put_char(cur_char);
                ioq_putchar(&kbd_buf, cur_char);
            }
            return;
        }

        // 记录本次是否按下了下面几类控制键之一，供下次键入时判断组合键
        if (scan_code == ctrl_l_make || scan_code == ctrl_r_make) {
            ctrl_status = true;
        } else if (scan_code == shift_l_make || scan_code == shift_r_make) {
            shift_status = true;
        } else if (scan_code == alt_l_make || scan_code == alt_r_make) {
            alt_status = true;
        } else if (scan_code == caps_lock_make) {
            // put_str("push caps_lock\n");
            // 不管之前是否有按下 caps_lock 键，当再次按下时则状态取反
            caps_lock_status = !caps_lock_status;
        }
    } else {
        put_str("unkown key\n");
    }
    return;
}

/**
 * @brief 键盘初始化
 * 
 */
void keyboard_init(void) {
    put_str("keyboard init start\n");
    io_queue_init(&kbd_buf);
    // 注册键盘中断处理程序
    register_handler(0x21, intr_keyboard_handler);
    put_str("keyboard init end\n");
}


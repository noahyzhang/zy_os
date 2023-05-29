#include "print.h"

void main(void) {
    put_str("I am kernel\n");
    put_str("hello world 2023-05-20 11:52\n");
    put_int(0);
    put_char('\n');
    put_int(9);
    put_char('\n');
    put_int(0x00021a3f);
    put_char('\n');
    put_int(0x12345678);
    put_char('\n');
    put_int(0x00000000);
    for (;;) {}
}

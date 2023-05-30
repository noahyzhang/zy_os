#!/bin/bash
if [ ! -d "./out" ]; then
      mkdir out
fi
if [ -e "hd60M.img" ]; then
     rm -rf hd60M.img  
fi
if [ ! -d "./out/boot" ];then 
    mkdir out/boot
fi 
if [ ! -d "./out/kernel" ];then 
    mkdir out/kernel 
fi 

echo -e "================================================"
echo -e "生成 hd60M.img 文件"
bximage -q -func="create" -hd=60 -imgmode="flat" -sectsize=512 hd60M.img

echo -e "生成 mbr.bin 文件"
nasm -I ./boot/include/ -o ./out/boot/mbr.bin ./boot/mbr.s && dd if=./out/boot/mbr.bin of=./hd60M.img bs=512 count=1  conv=notrunc

echo -e "生成 loader.bin 文件"
nasm -I ./boot/include/ -o ./out/boot/loader.bin ./boot/loader.s && dd if=./out/boot/loader.bin of=./hd60M.img bs=512 count=4 seek=2 conv=notrunc

echo -e "生成 print.o 文件"
nasm -f elf -o out/kernel/print.o lib/kernel/print.s

echo -e "生成 kernel.o 文件"
nasm -f elf -o out/kernel/kernel.o kernel/kernel.s

echo -e "生成 interrupt.o 文件"
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -fstack-protector -o out/interrupt.o kernel/interrupt.c

echo -e "生成 init.o 文件"
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o out/init.o kernel/init.c

echo -e "生成 main.o 文件"
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o out/kernel/main.o kernel/main.c

echo -e "生成 kernel.bin 并且拷贝到硬盘"
ld -melf_i386  -Ttext 0xc0001500 -e main -o ./out/kernel/kernel.bin \
    out/kernel/main.o out/kernel/print.o out/init.o out/interrupt.o out/kernel/kernel.o \
    &&  dd if=./out/kernel/kernel.bin of=./hd60M.img bs=512 count=200 seek=9 conv=notrunc 
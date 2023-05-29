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
bximage -q -func="create" -hd=60 -imgmode="flat" -sectsize=512 hd60M.img
nasm -I ./boot/include/ -o ./out/boot/mbr.bin ./boot/mbr.s && dd if=./out/boot/mbr.bin of=./hd60M.img bs=512 count=1  conv=notrunc
nasm -I ./boot/include/ -o ./out/boot/loader.bin ./boot/loader.s && dd if=./out/boot/loader.bin of=./hd60M.img bs=512 count=4 seek=2 conv=notrunc
nasm -f elf -o out/kernel/print.o lib/kernel/print.s
gcc -m32 -I lib/kernel/ -c -o out/kernel/main.o kernel/main.c
ld -melf_i386  -Ttext 0xc0001500 -e main -o ./out/kernel/kernel.bin out/kernel/main.o out/kernel/print.o && \
    dd if=./out/kernel/kernel.bin of=./hd60M.img bs=512 count=200 seek=9 conv=notrunc
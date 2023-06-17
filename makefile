BUILD_DIR = ./build
DISK_IMG = hd60M.img
ENTRY_POINT = 0xc0001500

AS = nasm
CC = gcc
LD = ld
# LIB = -I lib/ -I lib/kernel/ -I kernel/ -I device/
LIB = -I ./
ASFLAGS = -f elf
ASBINLIB = -I boot/include/
CFLAGS = -m32 -Wall $(LIB) -c -fno-builtin -fno-stack-protector -W -Wstrict-prototypes -Wmissing-prototypes
LDFLAGS = -melf_i386 -Ttext $(ENTRY_POINT) -e main
OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/init.o $(BUILD_DIR)/interrupt.o \
	$(BUILD_DIR)/timer.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/print.o \
	$(BUILD_DIR)/debug.o $(BUILD_DIR)/memory.o $(BUILD_DIR)/bitmap.o \
	$(BUILD_DIR)/string.o $(BUILD_DIR)/thread.o $(BUILD_DIR)/list.o  \
	$(BUILD_DIR)/switch.o $(BUILD_DIR)/console.o $(BUILD_DIR)/sync.o  \
	$(BUILD_DIR)/keyboard.o $(BUILD_DIR)/io_queue.o $(BUILD_DIR)/tss.o \
	$(BUILD_DIR)/process.o $(BUILD_DIR)/syscall.o $(BUILD_DIR)/syscall-init.o \
	$(BUILD_DIR)/stdio.o 

##############     MBR代码编译     ############### 
$(BUILD_DIR)/mbr.bin: boot/mbr.s
	$(AS) $(ASBINLIB) $< -o $@

##############     bootloader代码编译     ###############
$(BUILD_DIR)/loader.bin: boot/loader.s
	$(AS) $(ASBINLIB) $< -o $@

##############     c代码编译     ###############
$(BUILD_DIR)/main.o: kernel/main.c lib/kernel/print.h lib/stdint.h kernel/init.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/init.o: kernel/init.c kernel/init.h lib/kernel/print.h \
        lib/stdint.h kernel/interrupt.h device/timer.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/interrupt.o: kernel/interrupt.c kernel/interrupt.h \
        lib/stdint.h kernel/global.h lib/kernel/io.h lib/kernel/print.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/timer.o: device/timer.c device/timer.h lib/stdint.h\
         lib/kernel/io.h lib/kernel/print.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/debug.o: kernel/debug.c kernel/debug.h \
        lib/kernel/print.h lib/stdint.h kernel/interrupt.h
	$(CC) $(CFLAGS) $< -o $@

${BUILD_DIR}/string.o: lib/string.c lib/string.h \
		lib/stdint.h kernel/global.h kernel/debug.h
	$(CC) $(CFLAGS) $< -o $@

${BUILD_DIR}/bitmap.o: lib/kernel/bitmap.c lib/kernel/bitmap.h \
		lib/stdint.h kernel/global.h lib/string.c lib/kernel/print.h kernel/interrupt.h kernel/debug.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/memory.o: kernel/memory.c kernel/memory.h \
		lib/kernel/bitmap.h lib/stdint.h lib/kernel/print.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/thread.o: thread/thread.c thread/thread.h \
		lib/stdint.h lib/string.c kernel/global.h kernel/memory.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/list.o: lib/kernel/list.c lib/kernel/list.h \
		kernel/global.h kernel/interrupt.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/console.o: device/console.c device/console.h \
		lib/stdint.h lib/kernel/print.h thread/thread.h thread/sync.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/sync.o: thread/sync.c thread/sync.h \
		lib/stdint.h lib/kernel/list.h thread/thread.h kernel/interrupt.h kernel/debug.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/keyboard.o: device/keyboard.c device/keyboard.h \
		lib/kernel/print.h kernel/interrupt.h lib/kernel/io.h kernel/global.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/io_queue.o: device/io_queue.c device/io_queue.h \
		thread/thread.h lib/stdint.h thread/sync.h kernel/debug.h kernel/interrupt.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/tss.o: user_process/tss.c user_process/tss.h \
		thread/thread.h lib/stdint.h kernel/global.h lib/kernel/print.h lib/string.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/process.o: user_process/process.c user_process/process.h \
		thread/thread.h lib/stdint.h kernel/global.h kernel/debug.h kernel/memory.h \
		lib/kernel/list.h user_process/tss.h kernel/interrupt.h lib/string.h device/console.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/syscall.o: lib/user/syscall.c lib/user/syscall.h lib/stdint.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/syscall-init.o: user_process/syscall-init.c user_process/syscall-init.h \
		thread/thread.h lib/stdint.h lib/kernel/print.h lib/user/syscall.h \
		lib/string.h device/console.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/stdio.o: lib/stdio.c lib/stdio.h lib/stdint.h kernel/global.h \
		lib/kernel/print.h lib/user/syscall.h lib/string.h kernel/interrupt.h
	$(CC) $(CFLAGS) $< -o $@

##############    汇编代码编译    ###############
$(BUILD_DIR)/kernel.o: kernel/kernel.s
	$(AS) $(ASFLAGS) $< -o $@
$(BUILD_DIR)/print.o: lib/kernel/print.s
	$(AS) $(ASFLAGS) $< -o $@
$(BUILD_DIR)/switch.o: thread/switch.s
	$(AS) $(ASFLAGS) $< -o $@

##############    链接所有目标文件    #############
$(BUILD_DIR)/kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@

.PHONY : mk_dir hd clean all

mk_dir:
	if [ ! -d $(BUILD_DIR) ];then mkdir $(BUILD_DIR);fi

mk_img:
	if [ ! -e $(DISK_IMG) ];then bximage -q -func="create" -hd=60 -imgmode="flat" -sectsize=512 $(DISK_IMG);fi
	# if [ ! -e $(DISK_IMG) ];then qemu-img create -u $(DISK_IMG) 60M;fi

hd:
	dd if=$(BUILD_DIR)/mbr.bin of=hd60M.img bs=512 count=1  conv=notrunc
	dd if=$(BUILD_DIR)/loader.bin of=hd60M.img bs=512 count=4 seek=2 conv=notrunc
	dd if=$(BUILD_DIR)/kernel.bin \
           of=hd60M.img \
           bs=512 count=200 seek=6 conv=notrunc

clean:
	cd $(BUILD_DIR) && rm -f ./* && rm ../$(DISK_IMG)

build: $(BUILD_DIR)/kernel.bin $(BUILD_DIR)/mbr.bin $(BUILD_DIR)/loader.bin

all: mk_dir mk_img build hd

qemug:
	# qemu-system-x86_64 -m 32M -hda ./hd60M.img -S -s -d int -D qemu.log 
	# qemu-system-i386 -m 32M -hda ./hd60M.img -S -s -d int -D qemu.log
	qemu-system-i386 -drive file=hd60M.img,index=0,media=disk,format=raw -S -s
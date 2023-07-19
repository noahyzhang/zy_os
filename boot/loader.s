; 进入保护模式，加载内核，初始化内存相关
; ============================================================

%include "boot.inc"
section loader vstart=LOADER_BASE_ADDR

;构建 gdt 及其内部的描述符
GDT_BASE: dd    0x00000000 
          dd    0x00000000

CODE_DESC: dd    0x0000FFFF 
           dd    DESC_CODE_HIGH4

DATA_STACK_DESC: dd    0x0000FFFF
                 dd    DESC_DATA_HIGH4

VIDEO_DESC: dd    0x80000007	       ; limit=(0xbffff-0xb8000)/4k=0x7
            dd    DESC_VIDEO_HIGH4  ; 此时dpl为0

GDT_SIZE   equ   $ - GDT_BASE
GDT_LIMIT  equ   GDT_SIZE -	1 

; 本来打算留 60 个描述符空位，但是在使用 qemu 调试的时候，发现貌似对 times 有上限限制，暂时不留
; 仅为猜想，并不能作为结论。先不留，后面再来深入看看 qemu 
; times 60 dq 0					 ; 此处预留60个描述符的空位(slot)

SELECTOR_CODE equ (0x0001<<3) + TI_GDT + RPL0    ; 相当于(CODE_DESC - GDT_BASE)/8 + TI_GDT + RPL0
SELECTOR_DATA equ (0x0002<<3) + TI_GDT + RPL0	 ; 同上
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0	 ; 同上 

; total_mem_bytes 用于保存内存容量, 以字节为单位, 此位置比较好记。
; 当前偏移 loader.bin 文件头 0x20 字节（4个 GDT共占用 32字节，也就是 0x20 字节）
; loader.bin 的加载地址是0x900, 因此 total_mem_bytes 内存中的地址是 0x920
; 将来在内核中会引用此地址作为系统内存总大小
; 0x920 
total_mem_bytes dd 0					 

; 以下是定义 gdt 的指针，前 2 字节是 gdt 界限，后 4 字节是 gdt 起始地址
; 此处地址是 0x924
gdt_ptr dw  GDT_LIMIT 
        dd  GDT_BASE

; 人工对齐: total_mem_bytes 4字节 + gdt_ptr 6字节 + ards_buf 212字节 + ards_nr 2字节
; 一共 256 字节，刚好占用 0x100 字节
; 此处地址是 0x92a
ards_buf times 212 db 0
ards_nr dw 0  ;用于记录ards结构体数量

; 以上是 0x900 - 0xa00，用来存储数据
; ============================================================


; ============================================================
; 以下从 0xa00 开始，是代码，mbr 会跳到 0xa00 处，执行指令
; loader.bin 目前占用 4 个扇区
; ============================================================


; 开始执行代码
; 此处地址是 0xa00 
loader_start:

; ---------------------------------------------------------------------------
; 如下是获取当前系统总内存
; ---------------------------------------------------------------------------

;-------  int 15h eax = 0000E820h ,edx = 534D4150h ('SMAP') 获取内存布局  -------
xor ebx, ebx		      ;第一次调用时，ebx值要为0
mov edx, 0x534d4150	      ;edx只赋值一次，循环体中不会改变
mov di, ards_buf	      ;ards结构缓冲区

.e820_mem_get_loop:	      ;循环获取每个ARDS内存范围描述结构
    mov eax, 0x0000e820	  ;执行int 0x15后,eax值变为0x534d4150,所以每次执行int前都要更新为子功能号。
    mov ecx, 20		      ;ARDS地址范围描述符结构大小是20字节
    int 0x15
    jc .e820_failed_so_try_e801   ;若cf位为1则有错误发生，尝试0xe801子功能
    add di, cx		              ;使di增加20字节指向缓冲区中新的ARDS结构位置
    inc word [ards_nr]	          ;记录ARDS数量
    cmp ebx, 0		              ;若ebx为0且cf不为1,这说明ards全部返回，当前已是最后一个
    jnz .e820_mem_get_loop

;在所有ards结构中，找出(base_add_low + length_low)的最大值，即内存的容量。
mov cx, [ards_nr]	      ;遍历每一个ARDS结构体,循环次数是ARDS的数量
mov ebx, ards_buf 
xor edx, edx		      ;edx为最大的内存容量,在此先清0

.find_max_mem_area:	      ;无须判断type是否为1,最大的内存块一定是可被使用
    mov eax, [ebx]	      ;base_add_low
    add eax, [ebx+8]	  ;length_low
    add ebx, 20		      ;指向缓冲区中下一个ARDS结构
    cmp edx, eax		  ;冒泡排序，找出最大,edx寄存器始终是最大的内存容量
    jge .next_ards
    mov edx, eax		  ;edx为总内存大小

.next_ards:
    loop .find_max_mem_area
    jmp .mem_get_ok

; ------  int 15h ax = E801h 获取内存大小,最大支持4G  ------
; 返回后, ax cx 值一样,以KB为单位,bx dx值一样,以64KB为单位
; 在ax和cx寄存器中为低16M,在bx和dx寄存器中为16MB到4G。
.e820_failed_so_try_e801:
    mov ax,0xe801
    int 0x15
    jc .e801_failed_so_try88   ;若当前e801方法失败,就尝试0x88方法

; 1. 先算出低15M的内存,ax和cx中是以KB为单位的内存数量,将其转换为以byte为单位
mov cx,0x400	     ;cx和ax值一样,cx用做乘数
mul cx 
shl edx,16
and eax,0x0000FFFF
or edx,eax
add edx, 0x100000 ;ax只是15MB,故要加1MB
mov esi,edx	     ;先把低15MB的内存容量存入esi寄存器备份

; 2. 再将16MB以上的内存转换为byte为单位,寄存器bx和dx中是以64KB为单位的内存数量
xor eax,eax
mov ax,bx		
mov ecx, 0x10000	;0x10000十进制为64KB
mul ecx		;32位乘法,默认的被乘数是eax,积为64位,高32位存入edx,低32位存入eax.
add esi,eax		;由于此方法只能测出4G以内的内存,故32位eax足够了,edx肯定为0,只加eax便可
mov edx,esi		;edx为总内存大小
jmp .mem_get_ok

;-----------------  int 15h ah = 0x88 获取内存大小,只能获取64M之内  ----------
.e801_failed_so_try88: 
    ;int 15后，ax存入的是以kb为单位的内存容量
    mov  ah, 0x88
    int  0x15
    jc .error_hlt
    and eax,0x0000FFFF
        
    ; 16位乘法，被乘数是ax,积为32位.积的高16位在dx中，积的低16位在ax中
    mov cx, 0x400     ;0x400等于1024,将ax中的内存容量换为以byte为单位
    mul cx
    shl edx, 16	     ;把dx移到高16位
    or edx, eax	     ;把积的低16位组合到edx,为32位的积
    add edx,0x100000  ;0x88子功能只会返回1MB以上的内存,故实际内存大小要加上1MB

.mem_get_ok:
    mov [total_mem_bytes], edx	 ;将内存换为byte单位后存入total_mem_bytes处。


; ---------------------------------------------------------------------------
; 如下是进入保护保护
; ---------------------------------------------------------------------------

;-----------------   准备进入保护模式   -------------------
; 0. 关中断
; 1. 打开A20
; 2. 加载gdt
; 3. 将cr0的pe位置1

; 一定不要忘记了关中断，曾经在这里调试了好久。
; 调试的现象为：不关中断，bochs 调试没有问题，但是使用 qemu 调试的时候，会出现指令乱跳
; 后来发现 qemu 调试的时候，由于中断的影响，导致在从磁盘加载 kernel.bin 到内存的时候
; 出现执行流与预期不符合，中断的影响使指令乱跳。
; 更近一步的原因还在详查

; -----------------  关中断  ----------------
cli 

; -----------------  打开A20  ----------------
in al,0x92
or al,0000_0010B
out 0x92,al

;-----------------  加载GDT  ----------------
lgdt [gdt_ptr]

;-----------------  cr0第0位置1  ----------------
mov eax, cr0
or eax, 0x00000001
mov cr0, eax

; 刷新流水线，避免分支预测的影响, 这种 cpu 优化策略，最怕 jmp 跳转
; 这将导致之前做的预测失效，从而起到了刷新的作用
; 通过 bochs 调试的时候就可以发现，还未进入 32 位模式时，如下的 32 位的指令和代码全部被翻译成 16 位指令+代码
jmp dword SELECTOR_CODE:p_mode_start	     

; 出错则挂起		     
.error_hlt:
    hlt


; ---------------------------------------------------------------------------
; 如下是进入了 32 位的世界了
; ---------------------------------------------------------------------------
[BITS 32]
p_mode_start:

mov ax, SELECTOR_DATA
mov ds, ax
mov es, ax
mov ss, ax
mov esp,LOADER_STACK_TOP
mov ax, SELECTOR_VIDEO
mov gs, ax

; -------------------------   加载kernel  ----------------------
mov edi, KERNEL_BIN_BASE_ADDR  ; 从磁盘读出后，写入到 edi 指定的地址
mov ecx, KERNEL_START_SECTOR   ; kernel.bin 所在的扇区号
mov ebx, 200 ; 读入的扇区数 
call read_hd

; 创建页目录及页表并初始化页内存位图
call setup_page

; 要将描述符表地址及偏移量写入内存 gdt_ptr, 一会用新地址重新加载
sgdt [gdt_ptr]	      ; 存储到原来gdt所有的位置

; 将gdt描述符中视频段描述符中的段基址 + 0xc0000000
mov ebx, [gdt_ptr + 2]  
; 视频段是第3个段描述符,每个描述符是8字节,故0x18
; 段描述符的高4字节的最高位是段基址的31~24位
or dword [ebx + 0x18 + 4], 0xc0000000 
					      

; 将gdt的基址加上0xc0000000使其成为内核所在的高地址
add dword [gdt_ptr + 2], 0xc0000000

; 将栈指针同样映射到内核地址
add esp, 0xc0000000 

; 把页目录地址赋给cr3
mov eax, PAGE_DIR_TABLE_POS
mov cr3, eax

; 打开cr0的pg位(第31位)
mov eax, cr0
or eax, 0x80000000
mov cr0, eax

; 在开启分页后,用gdt新的地址重新加载
lgdt [gdt_ptr]       ; 重新加载

;;;;;;;;;;;;;;;;;;;;;;;;;;;;  此时不刷新流水线也没问题  ;;;;;;;;;;;;;;;;;;;;;;;;
; 由于一直处在32位下, 原则上不需要强制刷新, 经过实际测试没有以下这两句也没问题
; 但以防万一，还是加上啦，免得将来出来莫句奇妙的问题
jmp SELECTOR_CODE:enter_kernel	  ; 强制刷新流水线,更新 gdt


; ---------------------------------------------------------------------------
; 正式进入内核，可以开始调试 C 语言了
; ---------------------------------------------------------------------------
enter_kernel:    
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ; call kernel_init
    mov esp, 0xc009f000
    jmp KERNEL_ENTRY_POINT  ; 内核起始地址存储在 0xc0001500


; -----------------   将 kernel.bin 中的 segment 拷贝到编译的地址   -----------
; kernel.bin 是 ELF 结构，如下也是对 ELF 文件的解析
kernel_init:
    xor eax, eax
    xor ebx, ebx		; ebx 记录程序头表地址
    xor ecx, ecx		; cx 记录程序头表中的 program header 数量
    xor edx, edx		; dx 记录 program header 尺寸,即 e_phentsize

    ; 偏移文件 42 字节处的属性是 e_phentsize, 表示 program header 大小
    mov dx, [KERNEL_BIN_BASE_ADDR + 42]
    ; 偏移文件开始部分 28 字节的地方是 e_phoff, 表示第 1 个 program header 在文件中的偏移量
    ; 其实该值是 0x34, 不过还是谨慎一点，这里来读取实际值
    mov ebx, [KERNEL_BIN_BASE_ADDR + 28] 
                        
    add ebx, KERNEL_BIN_BASE_ADDR
    ; 偏移文件开始部分 44 字节的地方是 e_phnum, 表示有几个 program header
    mov cx, [KERNEL_BIN_BASE_ADDR + 44]

.each_segment:
    cmp byte [ebx + 0], PT_NULL		  ; 若 p_type 等于 PT_NULL, 说明此 program header 未使用。
    je .PTNULL

    ; 为函数 memcpy 压入参数,参数是从右往左依然压入，函数原型类似于 memcpy(dst,src,size)
    push dword [ebx + 16]		      ; program header 中偏移 16 字节的地方是 p_filesz, 压入函数 memcpy 的第三个参数: size
    mov eax, [ebx + 4]			      ; 距程序头偏移量为4字节的位置是 p_offset
    add eax, KERNEL_BIN_BASE_ADDR	  ; 加上 kernel.bin 被加载到的物理地址, eax 为该段的物理地址
    push eax				          ; 压入函数 memcpy 的第二个参数: 源地址
    push dword [ebx + 8]			  ; 压入函数 memcpy 的第一个参数: 目的地址, 偏移程序头8字节的位置是 p_vaddr，这就是目的地址
    call mem_cpy				      ; 调用 mem_cpy 完成段复制
    add esp,12				          ; 清理栈中压入的三个参数

.PTNULL:
    add ebx, edx		 ; edx为 program header 大小,即 e_phentsize, 在此 ebx 指向下一个 program header 
    loop .each_segment
    ret

; ----------  逐字节拷贝 mem_cpy(dst,src,size) ------------
; 输入:栈中三个参数(dst,src,size)
;---------------------------------------------------------
mem_cpy:		      
    cld
    push ebp
    mov ebp, esp
    push ecx		       ; rep指令用到了 ecx，但 ecx 对于外层段的循环还有用，故先入栈备份
    mov edi, [ebp + 8]	   ; dst
    mov esi, [ebp + 12]	   ; src
    mov ecx, [ebp + 16]	   ; size
    rep movsb		       ; 逐字节拷贝

    ;恢复环境
    pop ecx		
    pop ebp
    ret


;-------------   创建页目录及页表   ---------------
setup_page:

;先把页目录占用的空间逐字节清 0
mov ecx, 4096
mov esi, 0

.clear_page_dir:
    mov byte [PAGE_DIR_TABLE_POS + esi], 0
    inc esi
    loop .clear_page_dir

; 开始创建页目录项(PDE)
.create_pde:				     ; 创建 Page Directory Entry
    mov eax, PAGE_DIR_TABLE_POS
    add eax, 0x1000 			     ; 此时 eax 为第一个页表的位置及属性
    mov ebx, eax				     ; 此处为 ebx 赋值，是为 .create_pte 做准备，ebx 为基址。

; 下面将页目录项 0 和 0xc00 都存为第一个页表的地址，
; 一个页表可表示 4MB 内存,这样 0xc03fffff 以下的地址和 0x003fffff 以下的地址都指向相同的页表，
; 这是为将地址映射为内核地址做准备
    or eax, PG_US_U | PG_RW_W | PG_P	   ; 页目录项的属性 RW 和 P 位为1, US 为 1, 表示用户属性,所有特权级别都可以访问.
    mov [PAGE_DIR_TABLE_POS + 0x0], eax    ; 第1个目录项,在页目录表中的第1个目录项写入第一个页表的位置(0x101000)及属性(3)
    mov [PAGE_DIR_TABLE_POS + 0xc00], eax  ; 一个页表项占用4字节,0xc00表示第768个页表占用的目录项,0xc00以上的目录项用于内核空间,
                                           ; 也就是页表的0xc0000000~0xffffffff共计1G属于内核,0x0~0xbfffffff共计3G属于用户进程.
    sub eax, 0x1000
    mov [PAGE_DIR_TABLE_POS + 4092], eax   ; 使最后一个目录项指向页目录表自己的地址

;下面创建页表项(PTE)
    mov ecx, 256				         ; 1M 低端内存 / 每页大小4k = 256
    mov esi, 0
    mov edx, PG_US_U | PG_RW_W | PG_P	 ; 属性为7,US=1,RW=1,P=1

.create_pte:				     ; 创建Page Table Entry
    mov [ebx+esi*4],edx			     ; 此时的ebx已经在上面通过eax赋值为0x101000,也就是第一个页表的地址 
    add edx,4096
    inc esi
    loop .create_pte

; 创建内核其它页表的PDE
mov eax, PAGE_DIR_TABLE_POS
add eax, 0x2000 		          ; 此时eax为第二个页表的位置
or eax, PG_US_U | PG_RW_W | PG_P  ; 页目录项的属性RW和P位为1,US为0
mov ebx, PAGE_DIR_TABLE_POS
mov ecx, 254			          ; 范围为第769~1022的所有目录项数量
mov esi, 769

.create_kernel_pde:
    mov [ebx+esi*4], eax
    inc esi
    add eax, 0x1000
    loop .create_kernel_pde
    ret


;-------------   读取磁盘中的数据写到内存   ---------------
read_hd:
    ; 0x1f2 8bit 指定读取或写入的扇区数
    mov dx, 0x1f2
    mov al, bl
    out dx, al

    ; 0x1f3 8bit iba地址的第八位 0-7
    inc dx
    mov al, cl
    out dx, al

    ; 0x1f4 8bit iba地址的中八位 8-15
    inc dx
    mov al, ch
    out dx, al

    ; 0x1f5 8bit iba地址的高八位 16-23
    inc dx
    shr ecx, 16
    mov al, cl
    out dx, al

    ; 0x1f6 8bit
    ; 0-3 位iba地址的24-27
    ; 4 0表示主盘 1表示从盘
    ; 5、7位固定为1
    ; 6 0表示CHS模式，1表示LAB模式
    inc dx
    shr ecx, 8
    and cl, 0b1111
    mov al, 0b1110_0000     ; LBA模式
    or al, cl
    out dx, al

    ; 0x1f7 8bit  命令或状态端口
    inc dx
    mov al, 0x20
    out dx, al

    ; 设置loop次数，读多少个扇区要loop多少次
    mov cl, bl

.start_read:
    push cx     ; 保存loop次数，防止被下面的代码修改破坏

    call .wait_hd_prepare
    call read_hd_data

    pop cx      ; 恢复loop次数

    loop .start_read

.return:
    ret

; 一直等待，直到硬盘的状态是：不繁忙，数据已准备好
; 即第7位为0，第3位为1，第0位为0
.wait_hd_prepare:
    mov dx, 0x1f7

.check:
    in al, dx
    and al, 0b1000_1000
    cmp al, 0b0000_1000
    jnz .check

    ret

; 读硬盘，一次读两个字节，读256次，刚好读一个扇区
read_hd_data:
    mov dx, 0x1f0
    mov cx, 256

.read_word:
    in ax, dx
    mov [edi], ax
    add edi, 2
    loop .read_word

    ret


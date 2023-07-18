[bits 32]
%define ERROR_CODE nop		 ; 若在相关的异常中cpu已经自动压入了错误码,为保持栈中格式统一,这里不做操作.
%define ZERO push 0		 ; 若在相关的异常中cpu没有压入错误码,为了统一栈中格式,就手工压入一个0

extern idt_table		 ;idt_table是C中注册的中断处理程序数组

section .data
global intr_entry_table
intr_entry_table:

; 定义 VECTOR 宏
%macro VECTOR 2
section .text
; 每个中断处理程序都要压入中断向量号,所以一个中断类型一个中断处理程序，自己知道自己的中断向量号是多少
intr%1entry:
   ; 中断若有错误码会压在eip后面 
   %2			
   ; 以下是保存上下文环境
   ; 保存 4 个段寄存器
   push ds
   push es
   push fs
   push gs
   ; pushad: push all double word register 压入 8 个通用寄存器
   ; PUSHAD 指令压入32位寄存器,其入栈顺序是: EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI
   pushad

   ; 因为我们设置了手动向 8259A 发送中断结束标识（也就是 ICW4 中的 AEOI 位为 0）
   ; 因此这里需要通过 OCW2 来设置中断结束
   ; 如果是从片上进入的中断, 除了往从片上发送 EOI 外, 还要往主片上发送 EOI 
   ; OCW2 格式，设置 EOI 位为 1，发送 EOI 信号结束中断
   mov al,0x20                   ; 中断结束命令 EOI
   out 0xa0,al                   ; 向从片发送
   out 0x20,al                   ; 向主片发送

   ; 不管 idt_table 中的目标程序是否需要参数, 都一律压入中断向量号, 用于调试时很方便
   push %1
   ; 调用 idt_table 中的 C 语言版本中断处理函数
   call [idt_table + %1*4]
   ; 该恢复上下文了
   jmp intr_exit

section .data
   dd    intr%1entry	 ; 存储各个中断入口程序的地址，形成intr_entry_table数组
%endmacro

section .text
global intr_exit
intr_exit:	     
   ; 以下是恢复上下文环境
   ; 跳过中断号, 当前压入中断号是为了调试方便
   add esp, 4
   ; popad: pop all double word register
   popad
   pop gs
   pop fs
   pop es
   pop ds
   ; 跳过 error_code
   add esp, 4
   ; iret 指令有两个功能，一是从中断返回，二是返回到调用自己执行的那个旧任务
   iretd

VECTOR 0x00,ZERO
VECTOR 0x01,ZERO
VECTOR 0x02,ZERO
VECTOR 0x03,ZERO 
VECTOR 0x04,ZERO
VECTOR 0x05,ZERO
VECTOR 0x06,ZERO
VECTOR 0x07,ZERO 
VECTOR 0x08,ERROR_CODE
VECTOR 0x09,ZERO
VECTOR 0x0a,ERROR_CODE
VECTOR 0x0b,ERROR_CODE 
VECTOR 0x0c,ZERO
VECTOR 0x0d,ERROR_CODE
VECTOR 0x0e,ERROR_CODE
VECTOR 0x0f,ZERO 
VECTOR 0x10,ZERO
VECTOR 0x11,ERROR_CODE
VECTOR 0x12,ZERO
VECTOR 0x13,ZERO 
VECTOR 0x14,ZERO
VECTOR 0x15,ZERO
VECTOR 0x16,ZERO
VECTOR 0x17,ZERO 
VECTOR 0x18,ERROR_CODE
VECTOR 0x19,ZERO
VECTOR 0x1a,ERROR_CODE
VECTOR 0x1b,ERROR_CODE 
VECTOR 0x1c,ZERO
VECTOR 0x1d,ERROR_CODE
VECTOR 0x1e,ERROR_CODE
VECTOR 0x1f,ZERO 
VECTOR 0x20, ZERO  ; 时钟中断对应的入口
VECTOR 0x21, ZERO  ; 键盘中断对应的入口
VECTOR 0x22, ZERO  ; 级联时使用
VECTOR 0x23, ZERO  ; 串口 2 对应的入口
VECTOR 0x24, ZERO  ; 串口 1 对应的入口
VECTOR 0x25, ZERO  ; 并口 2 对应的入口 
VECTOR 0x26, ZERO  ; 软盘对应的入口
VECTOR 0x27, ZERO  ; 并口 1 对应的入口
VECTOR 0x28, ZERO  ; 实时时钟对应的入口
VECTOR 0x29, ZERO  ; 重定向
VECTOR 0x2a, ZERO  ; 保留
VECTOR 0x2b, ZERO  ; 保留
VECTOR 0x2c, ZERO  ; ps/2 鼠标
VECTOR 0x2d, ZERO  ; fpu 浮点单元异常
VECTOR 0x2e, ZERO  ; 硬盘
VECTOR 0x2f, ZERO  ; 保留

;;;;;;;;;;;;;;;;   0x80号中断   ;;;;;;;;;;;;;;;;
[bits 32]
extern syscall_table
section .text
global syscall_handler
syscall_handler:
;1 保存上下文环境
   push 0			    ; 压入0, 使栈中格式统一

   push ds
   push es
   push fs
   push gs
   pushad			    ; PUSHAD指令压入32位寄存器，其入栈顺序是:
				          ; EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI 
				 
   push 0x80			 ; 此位置压入0x80也是为了保持统一的栈格式

;2 为系统调用子功能传入参数
   push edx			    ; 系统调用中第3个参数
   push ecx			    ; 系统调用中第2个参数
   push ebx			    ; 系统调用中第1个参数

;3 调用子功能处理函数
   call [syscall_table + eax*4]	    ; 编译器会在栈中根据C函数声明匹配正确数量的参数
   add esp, 12			                ; 跨过上面的三个参数

;4 将call调用后的返回值存入待当前内核栈中eax的位置
   mov [esp + 8*4], eax	
   jmp intr_exit		    ; intr_exit返回,恢复上下文

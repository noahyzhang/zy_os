[bits 32]
section .text


; 此函数接受两个参数，第一个参数是当前线程 cur，第二个参数是下一个上处理器的线程
; 此函数的功能是保存 cur 线程的寄存器映像，将下一个线程 next 的寄存器映像装载到处理器
global switch_to
switch_to:
    ; 栈中此处是返回地址
    ; 根据 ABI 原则，保护内核环境上下文，保护如下 4 个寄存器
    push esi
    push edi
    push ebx
    push ebp
    ; 得到栈中的参数 cur，cur = [esp + 20]
    mov eax, [esp + 20]
    ; 保存栈顶指针 esp。task_struct 的 self_kernel_stack 字段
    ; self_kernel_stack 在 task_struct 中的偏移为 0
    ; 所以直接往 thread 开头处存 4 字节即可
    mov [eax], esp

    ; --- 以上是备份当前线程的环境，以下是恢复下一个线程的环境 ---

    ; 得到栈中参数 next, next = [esp + 24]
    mov eax, [esp + 24]
    ; pcb 的第一个成员是 self_kernel_stack
    ; 用来记录 0 级栈顶指针，被换上 cpu 时用来恢复 0 级栈
    ; 0 级栈中保存了进程或者线程所有信息，包括 3 级栈指针
    mov esp, [eax]
    pop ebp
    pop ebx 
    pop edi 
    pop esi 
    ; 如果未由中断进入，第一次执行时会返回到 kernel_thread
    ret

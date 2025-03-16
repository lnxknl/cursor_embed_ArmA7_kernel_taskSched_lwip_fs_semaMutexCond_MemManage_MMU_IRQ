.section .vectors
.align 4

.global _start
.global _vectors

_vectors:
    b _start              @ Reset
    b _undefined          @ Undefined instruction
    b _svc               @ Software interrupt
    b _prefetch          @ Prefetch abort
    b _data              @ Data abort
    b _unused            @ Not used
    b _irq               @ IRQ interrupt
    b _fiq               @ FIQ interrupt

.section .text
.align 4

_start:
    @ 设置处理器模式为SVC模式，禁用所有中断
    mrs r0, cpsr
    bic r0, r0, #0x1F
    orr r0, r0, #0x13
    orr r0, r0, #0xC0    @ 禁用 IRQ 和 FIQ
    msr cpsr, r0

    @ 初始化栈指针
    ldr r0, =_stack_top
    mov sp, r0

    @ 复制 .data 段到 RAM
    ldr r0, =_data_loadaddr
    ldr r1, =_data_start
    ldr r2, =_data_end
copy_data:
    cmp r1, r2
    ldrlt r3, [r0], #4
    strlt r3, [r1], #4
    blt copy_data

    @ 清零 BSS 段
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    mov r2, #0
bss_loop:
    cmp r0, r1
    strlt r2, [r0], #4
    blt bss_loop

    @ 跳转到 main 函数
    bl main

    @ 如果 main 返回，进入死循环
hang:
    b hang

@ 异常向量处理程序
_undefined:
    b _undefined

_svc:
    b _svc

_prefetch:
    b _prefetch

_data:
    b _data

_unused:
    b _unused

_irq:
    b _irq

_fiq:
    b _fiq

.end 
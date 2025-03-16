.section .vectors
.align 4

.global _start
.global _vectors

_vectors:
    ldr pc, =_start          @ Reset
    ldr pc, =_undefined      @ Undefined instruction
    ldr pc, =_svc           @ Software interrupt
    ldr pc, =_prefetch      @ Prefetch abort
    ldr pc, =_data          @ Data abort
    ldr pc, =_unused        @ Not used
    ldr pc, =_irq          @ IRQ interrupt
    ldr pc, =_fiq          @ FIQ interrupt

_vectors_data:
    .word _start
    .word _undefined
    .word _svc
    .word _prefetch
    .word _data
    .word _unused
    .word _irq
    .word _fiq

.section .text
.align 4

@ ARM架构相关的常量定义
.equ MODE_USR, 0x10
.equ MODE_FIQ, 0x11
.equ MODE_IRQ, 0x12
.equ MODE_SVC, 0x13
.equ MODE_ABT, 0x17
.equ MODE_UND, 0x1B
.equ MODE_SYS, 0x1F

.equ I_BIT, 0x80
.equ F_BIT, 0x40

_start:
    @ 禁用所有中断
    cpsid if

    @ 设置各种模式的栈指针
    @ FIQ 模式
    msr cpsr_c, #(MODE_FIQ | I_BIT | F_BIT)
    ldr sp, =_fiq_stack_top

    @ IRQ 模式
    msr cpsr_c, #(MODE_IRQ | I_BIT | F_BIT)
    ldr sp, =_irq_stack_top

    @ Abort 模式
    msr cpsr_c, #(MODE_ABT | I_BIT | F_BIT)
    ldr sp, =_abt_stack_top

    @ Undefined 模式
    msr cpsr_c, #(MODE_UND | I_BIT | F_BIT)
    ldr sp, =_und_stack_top

    @ System 模式
    msr cpsr_c, #(MODE_SYS | I_BIT | F_BIT)
    ldr sp, =_sys_stack_top

    @ 最后设置为 SVC 模式
    msr cpsr_c, #(MODE_SVC | I_BIT | F_BIT)
    ldr sp, =_svc_stack_top

    @ 初始化CP15协处理器
    @ 禁用指令缓存和数据缓存
    mrc p15, 0, r0, c1, c0, 0
    bic r0, r0, #(1 << 12)   @ 禁用指令缓存
    bic r0, r0, #(1 << 2)    @ 禁用数据缓存
    bic r0, r0, #(1 << 0)    @ 禁用MMU
    mcr p15, 0, r0, c1, c0, 0

    @ 使无效TLB
    mcr p15, 0, r0, c8, c7, 0

    @ 使无效指令缓存和数据缓存
    mcr p15, 0, r0, c7, c7, 0

    @ 复制异常向量表到0地址
    ldr r0, =_vectors
    mov r1, #0
    ldmia r0!, {r2-r9}
    stmia r1!, {r2-r9}
    ldmia r0!, {r2-r9}
    stmia r1!, {r2-r9}

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

    @ 调用MMU初始化
    bl mmu_init

    @ 调用中断初始化
    bl interrupt_init

    @ 调用定时器初始化
    bl timer_init

    @ 启用MMU
    mrc p15, 0, r0, c1, c0, 0
    orr r0, r0, #1
    mcr p15, 0, r0, c1, c0, 0

    @ 启用指令缓存和数据缓存
    mrc p15, 0, r0, c1, c0, 0
    orr r0, r0, #(1 << 12)   @ 启用指令缓存
    orr r0, r0, #(1 << 2)    @ 启用数据缓存
    mcr p15, 0, r0, c1, c0, 0

    @ 启用中断
    cpsie i

    @ 跳转到 main 函数
    bl main

    @ 如果 main 返回，进入死循环
hang:
    b hang

@ 异常向量处理程序
_undefined:
    stmfd sp!, {r0-r12, lr}
    bl undefined_handler
    ldmfd sp!, {r0-r12, pc}^

_svc:
    stmfd sp!, {r0-r12, lr}
    bl svc_handler
    ldmfd sp!, {r0-r12, pc}^

_prefetch:
    stmfd sp!, {r0-r12, lr}
    bl prefetch_handler
    ldmfd sp!, {r0-r12, pc}^

_data:
    stmfd sp!, {r0-r12, lr}
    bl data_abort_handler
    ldmfd sp!, {r0-r12, pc}^

_unused:
    b .

_irq:
    sub lr, lr, #4           @ 调整返回地址
    stmfd sp!, {r0-r12, lr}  @ 保存寄存器
    bl irq_handler           @ 调用C语言中断处理函数
    ldmfd sp!, {r0-r12, pc}^ @ 恢复寄存器并返回

_fiq:
    sub lr, lr, #4
    stmfd sp!, {r0-r12, lr}
    bl fiq_handler
    ldmfd sp!, {r0-r12, pc}^

.end 
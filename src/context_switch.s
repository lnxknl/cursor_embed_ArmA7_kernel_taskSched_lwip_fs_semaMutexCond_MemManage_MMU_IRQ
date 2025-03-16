.section .text
.global context_switch
.align 4

@ 上下文切换函数
@ r0: 前一个任务的任务控制块指针
@ r1: 下一个任务的任务控制块指针
context_switch:
    @ 保存当前任务的上下文
    stmfd sp!, {r0-r12, lr}    @ 保存通用寄存器和返回地址
    mrs r2, cpsr               @ 保存CPSR
    stmfd sp!, {r2}           @ 保存CPSR到栈

    @ 如果前一个任务不为空，保存其栈指针
    cmp r0, #0
    beq 1f
    str sp, [r0]              @ 保存栈指针到任务控制块

1:  @ 加载新任务的上下文
    ldr sp, [r1]              @ 加载新任务的栈指针
    ldmfd sp!, {r2}           @ 恢复CPSR
    msr spsr, r2              @ 设置SPSR
    ldmfd sp!, {r0-r12, lr}   @ 恢复通用寄存器和返回地址
    movs pc, lr               @ 返回到新任务

.end 
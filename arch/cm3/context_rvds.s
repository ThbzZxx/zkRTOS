;******************************************************************************
; File: context_rvds.s
; Brief: ZK-RTOS Cortex-M3 context switch & interrupt handlers (ARM Keil)
; Version: 1.0.0
; Date: 2025-01-08
; Note: 
;------------------------------------------------------------------------------

    EXTERN  g_current_tcb
    EXTERN  g_switch_next_tcb
    EXTERN  scheduler_increment_tick
    EXTERN  task_update_runtime_stats

    AREA |.text|, CODE, READONLY, ALIGN=2
    THUMB
    REQUIRE8
    PRESERVE8

;******************************************************************************
; Function: zk_asm_start_first_task
; Brief: 启动第一个任务
;******************************************************************************
zk_asm_start_first_task    PROC
    EXPORT zk_asm_start_first_task

    LDR     r0, =0xE000ED08         ; NVIC向量表偏移寄存器VTOR地址
    LDR     r0, [r0]                ; 从VTOR寄存器中读取向量表基地址
    LDR     r0, [r0]                ; 读取主栈指针初始值

    MSR     msp, r0                 ; 初始化主栈指针

    CPSIE   i                       ; 使能IRQ中断
    CPSIE   f                       ; 使能Fault中断
    DSB
    ISB

    SVC     0                       ; 触发svc异常
    NOP
    NOP
    ENDP

;******************************************************************************
; Function: zk_asm_svc_handler
; Brief: SVC异常处理函数（启动第一个任务）
;******************************************************************************
zk_asm_svc_handler   PROC
    EXPORT zk_asm_svc_handler

    LDR     r3, =g_current_tcb      ; 获取当前任务的TCB地址
    LDR     r1, [r3]                ; 读取TCB指针
    LDR     r0, [r1]                ; 获取任务栈顶地址
    LDMIA   r0!, {r4-r11}           ; 从任务栈恢复寄存器R4-R11
    MSR     psp, r0                 ; 将任务栈地址加载到PSP
    ISB
    MOV     r0, #0                  ; 清除basepri寄存器
    MSR     basepri, r0
    ORR     r14, #0xd               ; 设置EXC_RETURN=0xFFFFFFFD
    BX      r14                     ; 触发异常返回，执行任务函数
    ENDP

;******************************************************************************
; Function: zk_asm_pendsv_handler
; Brief: PendSV异常处理函数（任务切换）
;******************************************************************************
zk_asm_pendsv_handler   PROC
    EXPORT zk_asm_pendsv_handler

    ; 保存即将切换出去任务的上下文
    MRS     r0, psp                     ; 从PSP寄存器中读取软件保存帧的顶部
    ISB
    LDR     r3, =g_current_tcb          ; 获取当前任务的TCB地址
    LDR     r2, [r3]                    ; r2 = 当前TCB指针

    STMDB   r0!, {r4-r11}               ; 软件保存寄存器R4-R11到任务栈
    STR     r0, [r2]                    ; 将最终的栈指针保存回TCB

    ; P1: 更新任务运行时统计
    PUSH    {r2, lr}                    ; 保存r2(old_tcb)和返回地址
    LDR     r3, =g_switch_next_tcb      ; 获取新任务TCB
    LDR     r1, [r3]                    ; r1 = new_tcb
    MOV     r0, r2                      ; r0 = old_tcb (r2已保存)
    BL      task_update_runtime_stats   ; 调用C函数更新统计
    POP     {r2, lr}                    ; 恢复r2和lr

    ; 恢复即将切换进来任务的上下文
    LDR     r3, =g_switch_next_tcb      ; 获取下一个任务的TCB地址
    LDR     r1, [r3]                    ; r1 = 下一个任务的TCB指针
    LDR     r0, [r1]                    ; r0 = 新任务保存的栈指针

    LDMIA   r0!, {r4-r11}               ; 软件恢复寄存器R4-R11
    MSR     psp, r0                     ; 更新进程栈指针
    ISB
    LDR     R0, =g_current_tcb          ; R0 = &g_current_tcb
    STR     r1, [R0]                    ; g_current_tcb = r1
    BX      r14                         ; 触发异常返回
    NOP
    ENDP

;******************************************************************************
; Function: zk_asm_systick_handler
; Brief: SysTick异常处理函数
;******************************************************************************
zk_asm_systick_handler   PROC
    EXPORT zk_asm_systick_handler

    ; 保存上下文并调用C函数
    PUSH    {lr}

    ; 提升BASEPRI以屏蔽低优先级中断
    MOV     r0, #191                    ; ZK_MAX_SYSCALL_INTERRUPT_PRIORITY
    MSR     basepri, r0
    DSB
    ISB

    ; 调用调度器时钟增量函数
    BL      scheduler_increment_tick

    ; 清除BASEPRI
    MOV     r0, #0
    MSR     basepri, r0

    POP     {pc}
    ENDP

;******************************************************************************
; STM32 固件库需要的辅助函数
;******************************************************************************

; void __SETPRIMASK(void) - 禁用中断
__SETPRIMASK    PROC
    EXPORT __SETPRIMASK
    CPSID   i
    BX      lr
    ENDP

; void __RESETPRIMASK(void) - 使能中断
__RESETPRIMASK  PROC
    EXPORT __RESETPRIMASK
    CPSIE   i
    BX      lr
    ENDP

; void __SETFAULTMASK(void) - 禁用Fault中断
__SETFAULTMASK  PROC
    EXPORT __SETFAULTMASK
    CPSID   f
    BX      lr
    ENDP

; void __RESETFAULTMASK(void) - 使能Fault中断
__RESETFAULTMASK PROC
    EXPORT __RESETFAULTMASK
    CPSIE   f
    BX      lr
    ENDP

; void __BASEPRICONFIG(uint32_t priority) - 设置BASEPRI
__BASEPRICONFIG PROC
    EXPORT __BASEPRICONFIG
    MSR     basepri, r0
    BX      lr
    ENDP

; uint32_t __GetBASEPRI(void) - 获取BASEPRI
__GetBASEPRI    PROC
    EXPORT __GetBASEPRI
    MRS     r0, basepri
    BX      lr
    ENDP

    ALIGN   4
    END

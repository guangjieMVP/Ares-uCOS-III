


NVIC_INT_CTRL   EQU     0xE000ED04    ; 中断控制及状态寄存器 SCB_ICSR。 
NVIC_SYSPRI14   EQU     0xE000ED22    ; 系统优先级寄存器 SCB_SHPR3： 
                                       ; bit16~23 
NVIC_PENDSV_PRI EQU           0xFF    ; PendSV 优先级的值(最低)。 
NVIC_PENDSVSET  EQU     0x10000000    ; 触发 PendSV 异常的值 Bit28：PENDSVSET 
	
	IMPORT  OSTCBCurPtr                                         ; 外部文件引人的参考
    IMPORT  OSTCBHighRdyPtr
	IMPORT  OSPrioHighRdy
	IMPORT  OSPrioCur
		
    EXPORT  OSStartHighRdy                                      ; 该文件定义的函数
	EXPORT  PendSV_Handler
	
	PRESERVE8
	THUMB
		
	AREA CODE, CODE, READONLY
		
OSStartHighRdy PROC
	LDR R0, =  NVIC_SYSPRI14        ;设置PendSV异常优先级为最低
	LDR R1, =  NVIC_PENDSV_PRI
	STRB  R1, [R0]                  ;少了这句汇编就不行了，没有把PendSV异常设为最低优先级
	
	MOVS R0, #0           
	MSR  PSP,R0
	
	LDR R0, = NVIC_INT_CTRL     ;触发PendSV异常
	LDR R1, = NVIC_PENDSVSET
	STR R1,[R0]
	
	CPSIE I             ;使能总中断

OSStartHang 
	B   OSStartHang 
	
	ENDP
	
PendSV_Handler PROC
;	CPSID   I    
	; 任务的保存，即把CPU寄存器的值存储到任务的堆栈中	
	CPSID   I                                 ; 关中断，NMI和HardFault除外，防止上下文切换被中断	
	MRS     R0, PSP                           ; 将psp的值加载到R0
	CBZ     R0, OS_CPU_PendSVHandler_nosave   ; 判断R0，如果值为0则跳转到OS_CPU_PendSVHandler_nosave
	                                          ; 进行第一次任务切换的时候，R0肯定为0
	
	; 在进入PendSV异常的时候，当前CPU的xPSR，PC（任务入口地址），R14，R12，R3，R2，R1，R0会自动存储到当前任务堆栈，同时递减PSP的值
	STMDB   R0!, {R4-R11}                     ; 手动存储CPU寄存器R4-R11的值到当前任务的堆栈
	
	LDR     R1, = OSTCBCurPtr                 ; 加载 OSTCBCurPtr 指针的地址到R1，这里LDR属于伪指令
	LDR     R1, [R1]                          ; 加载 OSTCBCurPtr 指针到R1，这里LDR属于ARM指令
	STR     R0, [R1]                          ; 存储R0的值到	OSTCBCurPtr->OSTCBStkPtr，这个时候R0存的是任务空闲栈的栈顶

; 任务的切换，即把下一个要运行的任务的堆栈内容加载到CPU寄存器中
OS_CPU_PendSVHandler_nosave  
	;OSPrioCur = OSPrioHighRdy  第一次切换找到任务最高优先级
	LDR  R0 ,= OSPrioCur
	LDR  R1 ,= OSPrioHighRdy
	LDRB R2 , [R1]
	STRB R2 , [R0]
	
	; OSTCBCurPtr = OSTCBHighRdyPtr;
	LDR     R0, = OSTCBCurPtr                 ; 加载 OSTCBCurPtr 指针的地址到R0，这里LDR属于伪指令
	LDR     R1, = OSTCBHighRdyPtr             ; 加载 OSTCBHighRdyPtr 指针的地址到R1，这里LDR属于伪指令
	LDR     R2, [R1]                          ; 加载 OSTCBHighRdyPtr 指针到R2，这里LDR属于ARM指令
	STR     R2, [R0]                          ; 存储 OSTCBHighRdyPtr 到 OSTCBCurPtr
	
	LDR     R0, [R2]                          ; 加载 OSTCBHighRdyPtr 到 R0
	LDMIA   R0!, {R4-R11}                     ; 加载需要手动保存的信息到CPU寄存器R4-R11
	
	MSR     PSP, R0                           ; 更新PSP的值，这个时候PSP指向下一个要执行的任务的堆栈的栈底（这个栈底已经加上刚刚手动加载到CPU寄存器R4-R11的偏移）
	ORR     LR, LR, #0x04                     ; 确保异常返回使用的堆栈指针是PSP，即LR寄存器的位2要为1
	CPSIE   I                                 ; 开中断
	BX      LR                                ; 异常返回，这个时候任务堆栈中的剩下内容将会自动加载到xPSR，PC（任务入口地址），R14，R12，R3，R2，R1，R0（任务的形参）
	                                          ; 同时PSP的值也将更新，即指向任务堆栈的栈顶。在STM32中，堆栈是由高地址向低地址生长的。
	
	NOP                                       ; 为了汇编指令对齐，不然会有警告
	
	
	END                                       ; 汇编文件结束
	
	
	END                                       ; 汇编文件结束
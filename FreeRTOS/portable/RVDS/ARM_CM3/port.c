/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： LM_port.c
 * 文件标识： 
 * 内容摘要： M3-M4移植模块声明
 * 其它说明： 无
 * 当前版本： FreeRTOS V9.0.0
 * 作    者： Qiguo_Cui
 * 完成日期： 2025年09月01日
 *
 *******************************************************************************/
/*-----------------------------------------------------------
 * 此实现针对带FPU的Cortex-M4内核，包含内存管理、上下文切换和低功耗支持
 *
 *
 *----------------------------------------------------------*/

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"

/* Private define ------------------------------------------------------------
 *
 * 1、硬件依赖检查：确保目标平台支持硬件浮点单元(VFP)         M4内核定义
 * 2、中断优先级验证：防止系统调用中断优先级配置错误
 * 3、SysTick时钟配置：提供系统节拍定时器的时钟源选择
 * 4、NVIC寄存器定义：直接映射Cortex-M内核的系统控制寄存器
 * 5、芯片识别：特殊处理Cortex-M7 r0p1版本的兼容性
 * 6、优先级设置：配置PendSV和SysTick中断的优先级
 * 7、VFP支持：定义浮点上下文控制相关寄存器和位
 * 8、堆栈初始化：设置任务初始上下文的关键值
 * 9、无空闲节拍支持：提供低功耗模式所需的各种常量和补偿因子
 *
 *-----------------------------------------------------------------------------*/
 
/*1、 检查是否定义了目标FPU类型，如果不是VFP则报错   
      此端口仅当项目配置中启用硬件浮点支持时才能使用*/
#ifndef __TARGET_FPU_VFP
	#error This port can only be used when the project options are configured to enable hardware floating point support.
#endif

/* 2、确保系统调用中断优先级不为0 */
#if configMAX_SYSCALL_INTERRUPT_PRIORITY == 0 
	#error configMAX_SYSCALL_INTERRUPT_PRIORITY must not be set to 0.  See http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html
#endif

/* 3、如果没有定义系统时钟频率，则使用CPU时钟频率 */
#ifndef configSYSTICK_CLOCK_HZ
	#define configSYSTICK_CLOCK_HZ configCPU_CLOCK_HZ
	/* 确保SysTick以与内核相同的频率时钟工作 */
	#define portNVIC_SYSTICK_CLK_BIT	( 1UL << 2UL )   /* SysTick控制寄存器时钟源选择位(使用内核时钟) */
#else
	/* 如果SysTick时钟与内核时钟不同，则不修改时钟方式 */
	#define portNVIC_SYSTICK_CLK_BIT	( 0 )            /* 使用外部参考时钟 */
#endif

/* 在Keil工具中__weak属性可能无法按预期工作，因此如果应用程序编写者想要提供自己的
vPortSetupTimerInterrupt()实现，必须将configOVERRIDE_DEFAULT_TICK_CONFIGURATION常量设置为1。
确保定义了configOVERRIDE_DEFAULT_TICK_CONFIGURATION。 */
#ifndef configOVERRIDE_DEFAULT_TICK_CONFIGURATION
	#define configOVERRIDE_DEFAULT_TICK_CONFIGURATION 0   /* 默认使用端口提供的定时器配置 */
#endif

/* 4、操作内核所需的常量。首先是寄存器地址... */
#define portNVIC_SYSTICK_CTRL_REG			( * ( ( volatile uint32_t * ) 0xe000e010 ) )          /* SysTick控制和状态寄存器 */
#define portNVIC_SYSTICK_LOAD_REG			( * ( ( volatile uint32_t * ) 0xe000e014 ) )          /* SysTick重载值寄存器 */
#define portNVIC_SYSTICK_CURRENT_VALUE_REG	( * ( ( volatile uint32_t * ) 0xe000e018 ) )    /* SysTick当前值寄存器 */
#define portNVIC_SYSPRI2_REG				( * ( ( volatile uint32_t * ) 0xe000ed20 ) )            /* 系统优先级寄存器2 */  
  
/* ...然后是寄存器中的位定义。 */
#define portNVIC_SYSTICK_INT_BIT			( 1UL << 1UL )            /* SysTick中断使能位 */
#define portNVIC_SYSTICK_ENABLE_BIT			( 1UL << 0UL )          /* SysTick计数器使能位 */
#define portNVIC_SYSTICK_COUNT_FLAG_BIT		( 1UL << 16UL )       /* SysTick计数标志位(计数到0时置位) */
#define portNVIC_PENDSVCLEAR_BIT 			( 1UL << 27UL )           /* PendSV清除挂起位 */
#define portNVIC_PEND_SYSTICK_CLEAR_BIT		( 1UL << 25UL )       /* SysTick清除挂起位 */

/* 5、用于检测Cortex-M7 r0p1内核的常量，应使用ARM_CM7 r0p1端口 */
#define portCPUID							( * ( ( volatile uint32_t * ) 0xE000ed00 ) )     /* CPUID寄存器 */
#define portCORTEX_M7_r0p1_ID				( 0x410FC271UL )                           /* Cortex-M7 r0p1芯片标识 */
#define portCORTEX_M7_r0p0_ID				( 0x410FC270UL )                           /* Cortex-M7 r0p0芯片标识 */

/* 6、优先级设置：配置PendSV和SysTick中断的优先级 */
#define portNVIC_PENDSV_PRI					( ( ( uint32_t ) configKERNEL_INTERRUPT_PRIORITY ) << 16UL )   /* PendSV中断优先级设置 */
#define portNVIC_SYSTICK_PRI				( ( ( uint32_t ) configKERNEL_INTERRUPT_PRIORITY ) << 24UL )   /* SysTick中断优先级设置 */

/* 检查中断优先级有效性所需的常量 */
#define portFIRST_USER_INTERRUPT_NUMBER		( 16 )                                /* 第一个用户可用的中断编号 */
#define portNVIC_IP_REGISTERS_OFFSET_16 	( 0xE000E3F0 )                        /* 中断优先级寄存器偏移量(从第16个中断开始) */
#define portAIRCR_REG						( * ( ( volatile uint32_t * ) 0xE000ED0C ) )    /* 应用程序中断和复位控制寄存器 */ 
#define portMAX_8_BIT_VALUE					( ( uint8_t ) 0xff )                        /* 8位最大值 */
#define portTOP_BIT_OF_BYTE					( ( uint8_t ) 0x80 )                        /* 字节的最高位 */
#define portMAX_PRIGROUP_BITS				( ( uint8_t ) 7 )                           /* 最大优先级组位数 */        
#define portPRIORITY_GROUP_MASK				( 0x07UL << 8UL )                         /* 优先级组别掩码 */
#define portPRIGROUP_SHIFT					( 8UL )                                     /* 优先级组别偏移量 */


/* 屏蔽ICSR寄存器中除VECTACTIVE位外的所有位 */
#define portVECTACTIVE_MASK					( 0xFFUL )                                  /* 当前活动中断号掩码 */


/* 7、操作VFP所需的常量 */
#define portFPCCR					( ( volatile uint32_t * ) 0xe000ef34 )                /* 浮点上下文控制寄存器地址 */
#define portASPEN_AND_LSPEN_BITS	( 0x3UL << 30UL )                             /* 自动状态保存和惰性状态保存使能位 */


/* 8、设置初始堆栈所需的常量 */
#define portINITIAL_XPSR			( 0x01000000 )                                    /* 初始程序状态寄存器值(Thumb状态) */
#define portINITIAL_EXEC_RETURN		( 0xfffffffd )                                /* 初始异常返回值(返回到线程模式，使用PSP) */


/* SysTick是一个24位计数器 */
#define portMAX_24_BIT_NUMBER		( 0xffffffUL )                                  /* 24位最大值 */

/* 一个估算因子，用于估算在无空闲节拍计算期间SysTick计数器停止时可能发生的SysTick计数次数 */
#define portMISSED_COUNTS_FACTOR	( 45UL )                                      /* 错过的计数补偿因子 */

/* 为了严格符合Cortex-M规范，任务起始地址应清除位0，因为它会在退出ISR时加载到PC中 */
#define portSTART_ADDRESS_MASK		( ( StackType_t ) 0xfffffffeUL )              /* 任务地址掩码(确保地址对齐) */


/* Private variables ---------------------------------------------------------*/

/* 临界区嵌套计数器，初始化为0xaaaaaaaa用于调试（通常为易识别的模式值，如0xaaaaaaaa在调试时容易识别内存状态） */
static UBaseType_t uxCriticalNesting = 0xaaaaaaaa;                      


/*
 * 组成一个节拍周期所需的SysTick增量计数。
 * 用于无空闲节拍(tickless idle)模式的计算。
 */
#if configUSE_TICKLESS_IDLE == 1
	static uint32_t ulTimerCountsForOneTick = 0;           /* 一个节拍周期对应的定时器计数值 */
#endif /* configUSE_TICKLESS_IDLE */

/*
 * 可抑制的最大节拍周期数受SysTick定时器24位分辨率的限制。 
 * 用于无空闲节拍模式，确定最大可睡眠时间。
 */
#if configUSE_TICKLESS_IDLE == 1
	static uint32_t xMaximumPossibleSuppressedTicks = 0;   /* 最大可抑制的节拍数 */
#endif /* configUSE_TICKLESS_IDLE */

/*
 * 补偿SysTick停止时传递的CPU周期数（仅用于低功耗功能）。
 * 用于无空闲节拍模式，校正因定时器停止造成的时间误差。
 */
#if configUSE_TICKLESS_IDLE == 1
	static uint32_t ulStoppedTimerCompensation = 0;       /* 停止计时器补偿值 */
#endif /* configUSE_TICKLESS_IDLE */

/*
 * 由portASSERT_IF_INTERRUPT_PRIORITY_INVALID()宏使用，确保FreeRTOS API函数
 * 不会从已被分配高于configMAX_SYSCALL_INTERRUPT_PRIORITY优先级的中断中调用。
 * 仅在调试模式下(configASSERT_DEFINED == 1)有效。
 */
#if ( configASSERT_DEFINED == 1 )
	 static uint8_t ucMaxSysCallPriority = 0;   /* 最大系统调用优先级值 */
	 static uint32_t ulMaxPRIGROUPValue = 0;    /* 最大优先级组值 */
	                                            /* 指向中断优先级寄存器的常量指针，从第16个中断开始 */
	 static const volatile uint8_t * const pcInterruptPriorityRegisters = ( uint8_t * ) portNVIC_IP_REGISTERS_OFFSET_16;
#endif /* configASSERT_DEFINED */

/* Private function prototypes -----------------------------------------------*/

/*
 * 设置定时器以生成节拍中断。此文件中的实现为弱定义(__weak)，
 * 允许应用程序编写者更改用于生成节拍中断的定时器。
 * 输入参数：无
 * 输出参数：无
 */
void vPortSetupTimerInterrupt( void );


/*
 * 异常处理程序。
 */
void xPortPendSVHandler( void );
void xPortSysTickHandler( void );
void vPortSVCHandler( void );

/*
 * 启动第一个任务作为一个独立函数，以便可以单独测试。
 * 输入参数：无
 * 输出参数：无
 */
static void prvStartFirstTask( void );

/*
 * 在portasm.s中定义的函数，用于启用VFP(向量浮点单元)。
 * 输入参数：无
 * 输出参数：无
 */
static void prvEnableVFP( void );

/*
 * 用于捕获试图从其实现函数返回的任务（任务函数不应返回，应无限循环或自行删除）。
 * 输入参数：无
 * 输出参数：无
 */
static void prvTaskExitError( void );


/* functions -------------------------------------------------------------------*/

/*******************************************************************************
 函数名称：    StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters )
 功能描述：    初始化任务堆栈帧 - 模拟上下文切换中断时硬件自动保存的寄存器状态
 输入参数：    pxTopOfStack: 指向任务堆栈顶部的指针（堆栈从高地址向低地址增长）
               pxCode: 任务函数的入口地址（任务要执行的代码）
               pvParameters: 传递给任务函数的参数指针
 输出参数：    无
 返 回 值：    指向初始化后堆栈帧底部的指针（任务上下文开始的位置）
 其它说明：    此函数专门用于ARM Cortex-M架构，按照异常进入时硬件自动保存寄存器的顺序构建堆栈帧
 
 高地址
+-------------------+  ← 初始 pxTopOfStack
|   对齐预留空间     |  (pxTopOfStack--)
+-------------------+
|      xPSR         |  (初始程序状态寄存器)
+-------------------+
|        PC         |  (任务入口地址)
+-------------------+
|        LR         |  (指向 prvTaskExitError)
+-------------------+
|      R12          |  (未初始化)
+-------------------+
|       R3          |  (未初始化)
+-------------------+
|       R2          |  (未初始化)
+-------------------+
|       R1          |  (未初始化)
+-------------------+
|   R0 (参数指针)    |  (pvParameters)
+-------------------+
|   EXC_RETURN      |  (异常返回值)
+-------------------+
|       R11         |  (未初始化)
+-------------------+
|       R10         |  (未初始化)
+-------------------+
|        R9         |  (未初始化)
+-------------------+
|        R8         |  (未初始化)
+-------------------+
|        R7         |  (未初始化)
+-------------------+
|        R6         |  (未初始化)
+-------------------+
|        R5         |  (未初始化)
+-------------------+
|        R4         |  (未初始化)
+-------------------+  ← 返回的 pxTopOfStack (堆栈帧底部)
低地址
               
 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2025/09/01     V1.0          Qiguo_Cui          创建
 *******************************************************************************/
StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters )
{
  /* 模拟上下文切换中断时创建的堆栈帧结构 */

  
  pxTopOfStack--;                    /* 调整栈指针偏移：考虑MCU在中断进入/退出时使用堆栈的方式，并确保对齐要求 */
  
	*pxTopOfStack = portINITIAL_XPSR;  /* 设置初始程序状态寄存器值 (xPSR) - 包含Thumb状态位 */
  pxTopOfStack--;
	
  *pxTopOfStack = ( ( StackType_t ) pxCode ) & portSTART_ADDRESS_MASK;  /* 设置程序计数器 (PC) - 指向任务入口地址并确保对齐 */
  pxTopOfStack--;
	
  *pxTopOfStack = ( StackType_t ) prvTaskExitError;  /* 设置链接寄存器 (LR) - 指向任务退出错误处理函数 */
  pxTopOfStack -= 5;  /* 跳过部分寄存器初始化以节省代码空间 - 为R12、R3、R2和R1预留空间 */
	
  *pxTopOfStack = ( StackType_t ) pvParameters;  /* 设置R0寄存器 - 存储任务参数指针 */
  pxTopOfStack--;
	                                         /* 使用一种保存方法，要求每个任务维护自己的异常返回值 */
  *pxTopOfStack = portINITIAL_EXEC_RETURN; /* 设置异常返回值 - 控制任务首次执行时的处理器模式和堆栈指针选择 */

  
  pxTopOfStack -= 8;       /* 为剩余寄存器预留空间 - R11, R10, R9, R8, R7, R6, R5 and R4 */
  
  return pxTopOfStack;    /* 返回初始化后的堆栈指针位置（指向堆栈帧底部） */
}



/*******************************************************************************
函数名称：prvTaskExitError
功能描述：任务退出错误处理函数，用于捕获任务函数非法返回的错误情况。
          当任务函数试图返回时会调用此函数，触发断言并禁用中断，进入死循环。
输入参数：无
输出参数：无
返 回 值：无
其它说明：1.任务函数必须永不返回，应通过调用vTaskDelete(NULL)来终止任务
          2.此函数会强制触发断言(如果configASSERT已定义)并禁用所有中断
          3.设计用于帮助开发者捕获任务返回错误

修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          创建
*******************************************************************************/

static void prvTaskExitError( void )
{
    /* 实现任务的函数不得退出或尝试返回其调用者，因为没有可返回的地方。
       如果任务想要退出，它应该调用vTaskDelete(NULL)。

       如果定义了configASSERT()，则人工强制触发assert()，
       然后在此停止，以便应用程序编写者可以捕获错误。 */
    configASSERT( uxCriticalNesting == ~0UL );  /* 触发断言，检查临界区嵌套计数是否为最大值 */
    portDISABLE_INTERRUPTS();                    /* 禁用所有中断，防止进一步执行 */
    for( ;; );                                   /* 无限循环，停止系统运行 */
}



/*******************************************************************************
函数名称：vPortSVCHandler
功能描述：FreeRTOS SVC中断服务例程，用于任务上下文切换和启动调度器。在首次启动调度器时，
          通过SVC异常实现从特权模式到任务用户模式的切换，并恢复任务上下文。
输入参数：无（硬件自动触发）
输出参数：无
返 回 值：无
其它说明：1.使用进程栈指针(PSP)进行任务上下文恢复
          2.通过ldmia指令从任务堆栈恢复寄存器
          3.清除basepri寄存器以启用所有中断

修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          创建
*******************************************************************************/

__asm void vPortSVCHandler( void )
{
    PRESERVE8          /* 指定8字节栈对齐 */

    /* 获取当前任务控制块(TCB)地址 */
    ldr    r3, =pxCurrentTCB   /* 加载pxCurrentTCB全局变量地址到r3 */
    ldr    r1, [r3]            /* 获取pxCurrentTCB指针值（即TCB地址）到r1 */
    ldr    r0, [r1]            /* 从TCB首成员获取任务栈顶指针到r0 */

    /* 从堆栈恢复内核寄存器 */
    ldmia r0!, {r4-r11, r14}   /* 从r0指向的堆栈依次弹出r4-r11和r14，同时r0递增 */

    msr psp, r0                /* 将更新后的堆栈指针值存入进程栈指针(PSP) */
    isb                        /* 指令同步屏障，确保psp写入完成 */

    mov r0, #0                 /* 将0存入r0寄存器 */
    msr    basepri, r0         /* 清除basepri寄存器（启用所有中断） */

    bx r14                     /* 返回到任务代码（通过lr寄存器） */
}

/*******************************************************************************
函数名称：prvStartFirstTask
功能描述：启动FreeRTOS的第一个任务，初始化主堆栈指针(MSP)，全局启用中断，
          并通过SVC调用触发第一次任务切换。这是调度器启动的关键初始化函数。
输入参数：无
输出参数：无
返 回 值：无
其它说明：1.从NVIC的向量表偏移寄存器获取初始堆栈值
          2.设置主堆栈指针(MSP)为复位值
          3.启用所有中断(IRQ和FIQ)
          4.使用SVC 0指令触发第一个任务的启动

修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          创建
*******************************************************************************/

__asm void prvStartFirstTask( void )
{
    PRESERVE8          /* 指定8字节栈对齐 */

    /* 使用NVIC偏移寄存器定位主堆栈起始位置 */
    ldr r0, =0xE000ED08   /* 加载VTOR寄存器地址(向量表偏移寄存器)到r0 */
    ldr r0, [r0]          /* 获取向量表基地址(存储MSP初始值的位置) */
    ldr r0, [r0]          /* 获取主堆栈指针(MSP)的初始值 */

    /* 将MSP重置为堆栈起始位置 */
    msr msp, r0           /* 将获取的初始值设置到主堆栈指针寄存器 */

    /* 全局启用中断 */
    cpsie i               /* 启用IRQ中断(清除PRIMASK) */
    cpsie f               /* 启用FIQ中断(清除FAULTMASK) */
    dsb                   /* 数据同步屏障，确保所有内存访问完成 */
    isb                   /* 指令同步屏障，确保所有指令执行完成 */

    /* 调用SVC指令启动第一个任务 */
    svc 0                 /* 触发SVC异常，参数0表示启动第一个任务 */
    nop                   /* 空操作，用于对齐或占位 */
    nop                   /* 空操作，用于对齐或占位 */
}


/*******************************************************************************
函数名称：prvEnableVFP
功能描述：启用ARM Cortex-M处理器的浮点单元(FPU)，通过设置CPACR寄存器的CP10和CP11位，
          允许处理器执行浮点运算指令。此函数通常在系统初始化时调用一次。
输入参数：无
输出参数：无
返 回 值：无
其它说明：1.修改协处理器访问控制寄存器(CPACR)使能浮点单元
          2.适用于Cortex-M4/M7等带有FPU的处理器
          3.使用ldr.w指令确保32位地址加载

修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          创建
*******************************************************************************/

__asm void prvEnableVFP( void )
{
    PRESERVE8          /* 指定8字节栈对齐 */

    /* FPU使能位位于CPACR寄存器中 */
    ldr.w r0, =0xE000ED88   /* 使用宽指令加载CPACR寄存器地址到r0 */
    ldr    r1, [r0]         /* 读取CPACR寄存器的当前值到r1 */

    /* 启用CP10和CP11协处理器，然后保存回寄存器 */
    orr    r1, r1, #( 0xf << 20 )  /* 设置CP10和CP11字段为全访问权限(0b11) */
    str r1, [r0]            /* 将修改后的值写回CPACR寄存器 */
    bx    r14               /* 返回到调用者(使用连接寄存器r14) */
    nop                     /* 空操作，用于对齐或占位 */
}
/*******************************************************************************
函数名称：xPortStartScheduler
功能描述：启动FreeRTOS调度器，进行系统初始化配置，包括中断优先级设置、系统节拍定时器初始化、
          浮点单元启用，并启动第一个任务。这是FreeRTOS启动的核心函数。
输入参数：无
输出参数：无
返 回 值：BaseType_t类型，理论上应返回0(但正常情况下不会返回)
其它说明：1.包含多个配置断言检查，确保系统配置正确
          2.设置PendSV和SysTick中断为最低优先级
          3.初始化系统节拍定时器
          4.启用浮点单元并配置惰性堆栈保存
          5.最终通过prvStartFirstTask启动第一个任务

修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          创建
*******************************************************************************/

BaseType_t xPortStartScheduler( void )
{
    /* configMAX_SYSCALL_INTERRUPT_PRIORITY不能设置为0。
    参见http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html */
    configASSERT( configMAX_SYSCALL_INTERRUPT_PRIORITY );

    /* 此端口可用于除r0p1版本之外的所有Cortex-M7内核。
    r0p1部件应使用/source/portable/GCC/ARM_CM7/r0p1目录中的端口。 */
    configASSERT( portCPUID != portCORTEX_M7_r0p1_ID );
    configASSERT( portCPUID != portCORTEX_M7_r0p0_ID );

    #if( configASSERT_DEFINED == 1 )
    {
        volatile uint32_t ulOriginalPriority;
        volatile uint8_t * const pucFirstUserPriorityRegister = ( uint8_t * ) ( portNVIC_IP_REGISTERS_OFFSET_16 + portFIRST_USER_INTERRUPT_NUMBER );
        volatile uint8_t ucMaxPriorityValue;

        /* 确定可以从中调用FreeRTOS API安全中断函数(以"FromISR"结尾的函数)的最高优先级。
        FreeRTOS维护独立的线程和ISR API函数，以确保中断入口尽可能快速和简单。

        保存即将被修改的中断优先级值。 */
        ulOriginalPriority = *pucFirstUserPriorityRegister;

        /* 确定可用的优先级位数。首先向所有可能的位写入值。 */
        *pucFirstUserPriorityRegister = portMAX_8_BIT_VALUE;

        /* 读回值以查看有多少位被保留。 */
        ucMaxPriorityValue = *pucFirstUserPriorityRegister;

        /* 内核中断优先级应设置为最低优先级。 */
        configASSERT( ucMaxPriorityValue == ( configKERNEL_INTERRUPT_PRIORITY & ucMaxPriorityValue ) );

        /* 对最大系统调用优先级使用相同的掩码。 */
        ucMaxSysCallPriority = configMAX_SYSCALL_INTERRUPT_PRIORITY & ucMaxPriorityValue;

        /* 根据读回的位数计算最大可接受的优先级组值。 */
        ulMaxPRIGROUPValue = portMAX_PRIGROUP_BITS;
        while( ( ucMaxPriorityValue & portTOP_BIT_OF_BYTE ) == portTOP_BIT_OF_BYTE )
        {
            ulMaxPRIGROUPValue--;
            ucMaxPriorityValue <<= ( uint8_t ) 0x01;
        }

        /* 将优先级组值移回其在AIRCR寄存器中的位置。 */
        ulMaxPRIGROUPValue <<= portPRIGROUP_SHIFT;
        ulMaxPRIGROUPValue &= portPRIORITY_GROUP_MASK;

        /* 将修改过的中断优先级寄存器恢复为其原始值。 */
        *pucFirstUserPriorityRegister = ulOriginalPriority;
    }
    #endif /* conifgASSERT_DEFINED */

    /* 将PendSV和SysTick设置为最低优先级中断。 */
    portNVIC_SYSPRI2_REG |= portNVIC_PENDSV_PRI;
    portNVIC_SYSPRI2_REG |= portNVIC_SYSTICK_PRI;

    /* 启动生成滴答定时器中断的定时器。此处中断已被禁用。 */
    vPortSetupTimerInterrupt();

    /* 初始化关键嵌套计数，准备第一个任务。 */
    uxCriticalNesting = 0;

    /* 确保VFP已启用 - 它应该是已经启用的。 */
    prvEnableVFP();

    /* 始终启用惰性保存。 */
    *( portFPCCR ) |= portASPEN_AND_LSPEN_BITS;

    /* 启动第一个任务。 */
    prvStartFirstTask();

    /* 不应执行到这里！ */
    return 0;
}


/*******************************************************************************
函数名称：vPortEndScheduler
功能描述：停止FreeRTOS调度器。在大多数嵌入式端口中未实现此功能，因为一旦调度器启动，
          通常没有可返回的环境。此函数主要用于调试或特殊情况下停止调度器运行。
输入参数：无
输出参数：无
返 回 值：无
其它说明：1.在大多数端口实现中，此函数实际上未完整实现
          2.通过断言检查确保不会在意外情况下调用
          3.设计上用于支持调试或特殊应用场景

修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          创建
*******************************************************************************/

void vPortEndScheduler( void )
{
    /* 在无可返回环境的端口中未实现此功能。
    人工强制触发断言。 */
    configASSERT( uxCriticalNesting == 1000UL );  /* 检查关键嵌套计数是否为1000，如果不是则触发断言错误 */
}


/*******************************************************************************
函数名称：vPortEnterCritical
功能描述：进入临界区，禁用中断并增加临界区嵌套计数。确保代码段在执行时不会被中断打断，
          用于保护共享资源的原子操作。非中断安全版本，不能在中断服务例程中调用。
输入参数：无
输出参数：无
返 回 值：无
其它说明：1.此函数会禁用全局中断
          2.维护临界区嵌套计数器uxCriticalNesting
          3.通过断言检查防止在中断上下文中错误调用
          4.只有以"FromISR"结尾的API函数才能在中断中使用

修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          创建
*******************************************************************************/

void vPortEnterCritical( void )
{
    portDISABLE_INTERRUPTS();  /* 禁用所有中断，防止临界区代码被打断 */
    uxCriticalNesting++;       /* 增加临界区嵌套计数器，记录嵌套深度 */

    /* 这不是进入临界区函数的中断安全版本，因此如果从中断上下文中调用，则触发断言()。
    只有以"FromISR"结尾的API函数才能在中断中使用。仅在临界嵌套计数为1时进行断言，
    以防止如果断言函数也使用临界区时的递归调用。 */
    if( uxCriticalNesting == 1 )  /* 只在第一次进入临界区时检查，避免递归调用问题 */
    {
        /* 检查当前是否处于中断上下文(portVECTACTIVE_MASK非零表示处于中断中) */
        configASSERT( ( portNVIC_INT_CTRL_REG & portVECTACTIVE_MASK ) == 0 );
    }
}

/*******************************************************************************
函数名称：vPortExitCritical
功能描述：退出临界区函数，减少临界区嵌套计数并在嵌套计数为零时重新启用中断。
          此函数必须与vPortEnterCritical配对使用，用于结束受保护的代码段。
输入参数：无
输出参数：无
返 回 值：无
其它说明：1.使用断言确保不会出现未配对的退出调用
          2.只在所有嵌套临界区都退出后才重新启用中断
          3.遵循"后进先出"(LIFO)的嵌套原则

修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          创建
*******************************************************************************/
void vPortExitCritical( void )
{
	configASSERT( uxCriticalNesting );  /* 断言检查：确保临界区嵌套计数不为零，防止未配对的退出调用 */
	uxCriticalNesting--;                /* 减少临界区嵌套计数器，表示退出一个临界区 */
	if( uxCriticalNesting == 0 )        /* 检查是否已退出所有嵌套的临界区 */
	{
		portENABLE_INTERRUPTS();        /* 重新启用中断，恢复系统的正常中断响应 */
	}
}


/*******************************************************************************
函数名称：xPortPendSVHandler
功能描述：PendSV(可挂起的系统调用)中断处理程序，负责FreeRTOS的任务上下文切换。
          保存当前任务上下文，调用任务切换函数，然后恢复下一个任务的上下文。
输入参数：无（由硬件自动触发）
输出参数：无
返 回 值：无
其它说明：1.使用进程栈指针(PSP)进行任务上下文保存和恢复
          2.支持浮点单元(FPU)上下文的高效保存和恢复
          3.通过basepri寄存器实现临界区保护
          4.包含XMC4000系列特定 errata 的工作around

修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          创建
*******************************************************************************/
__asm void xPortPendSVHandler( void )
{
    /* 声明外部变量和函数 */
    extern uxCriticalNesting;        /* 临界区嵌套计数器 */
    extern pxCurrentTCB;             /* 当前任务控制块指针 */
    extern vTaskSwitchContext;       /* 任务上下文切换函数 */

    PRESERVE8                        /* 指定8字节栈对齐 */

    mrs r0, psp                      /* 将进程栈指针(PSP)的值读取到r0寄存器 */
    isb                              /* 指令同步屏障，确保PSP读取完成 */

    /* 获取当前任务控制块(TCB)的位置 */
    ldr    r3, =pxCurrentTCB         /* 加载pxCurrentTCB的地址到r3 */
    ldr    r2, [r3]                  /* 获取当前TCB的指针到r2 */

    /* 检查任务是否使用FPU上下文？如果是，推送高VFP寄存器。 */
    tst r14, #0x10                   /* 测试EXC_RETURN值的第4位(0x10) */
    it eq                            /* 如果相等(使用FPU)，执行下一条指令 */
    vstmdbeq r0!, {s16-s31}          /* 以递减方式将FPU寄存器s16-s31保存到任务堆栈 */

    /* 保存核心寄存器。 */
    stmdb r0!, {r4-r11, r14}         /* 以递减方式将寄存器r4-r11和r14保存到任务堆栈 */

    /* 将新的栈顶保存到TCB的第一个成员中。 */
    str r0, [r2]                     /* 将更新后的堆栈指针保存到TCB的第一个字段 */

    stmdb sp!, {r3}                  /* 将r3(pxCurrentTCB地址)保存到主堆栈 */
    mov r0, #configMAX_SYSCALL_INTERRUPT_PRIORITY  /* 加载最大系统调用中断优先级 */
    msr basepri, r0                  /* 设置basepri寄存器，屏蔽低于此优先级的中断 */
    dsb                              /* 数据同步屏障 */
    isb                              /* 指令同步屏障 */
    bl vTaskSwitchContext            /* 调用任务上下文切换函数 */
    mov r0, #0                       /* 将0存入r0 */
    msr basepri, r0                  /* 清除basepri寄存器(启用所有中断) */
    ldmia sp!, {r3}                  /* 从主堆栈恢复r3(pxCurrentTCB地址) */

    /* pxCurrentTCB中的第一项是任务的栈顶。 */
    ldr r1, [r3]                     /* 获取新的当前TCB指针 */
    ldr r0, [r1]                     /* 从TCB获取新任务的栈顶指针 */

    /* 弹出核心寄存器。 */
    ldmia r0!, {r4-r11, r14}         /* 从新任务堆栈恢复寄存器r4-r11和r14 */

    /* 检查任务是否使用FPU上下文？如果是，弹出高VFP寄存器。 */
    tst r14, #0x10                   /* 再次测试EXC_RETURN值的第4位 */
    it eq                            /* 如果相等(使用FPU)，执行下一条指令 */
    vldmiaeq r0!, {s16-s31}          /* 从任务堆栈恢复FPU寄存器s16-s31 */

    msr psp, r0                      /* 将更新后的堆栈指针设置回PSP */
    isb                              /* 指令同步屏障 */

    #ifdef WORKAROUND_PMU_CM001 /* XMC4000特定 errata 处理 */
        #if WORKAROUND_PMU_CM001 == 1
            push { r14 }             /* 将r14压入堆栈 */
            pop { pc }               /* 弹出到PC，实现返回 */
            nop                      /* 空操作 */
        #endif
    #endif

    bx r14                           /* 使用EXC_RETURN值返回到任务模式 */
}


/*******************************************************************************
函数名称：xPortSysTickHandler
功能描述：SysTick系统节拍定时器中断服务程序，用于FreeRTOS的心跳节拍处理。
          递增系统节拍计数器，并在需要时触发任务上下文切换。
输入参数：无（由硬件自动触发）
输出参数：无
返 回 值：无
其它说明：1.SysTick运行在最低中断优先级，执行时所有中断已取消屏蔽
          2.使用vPortRaiseBASEPRI()替代portSET_INTERRUPT_MASK_FROM_ISR()以提高效率
          3.通过设置PendSV中断来请求上下文切换
          4.遵循FreeRTOS中断服务程序的最佳实践

修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          创建
*******************************************************************************/
void xPortSysTickHandler( void )
{
    /* SysTick运行在最低中断优先级，因此当此中断执行时所有中断必须已取消屏蔽。
    因此不需要保存然后恢复中断屏蔽值，因为它的值已经是已知的 - 因此使用稍快的
    vPortRaiseBASEPRI()函数替代portSET_INTERRUPT_MASK_FROM_ISR()。 */
    vPortRaiseBASEPRI();  /* 临时提升中断优先级，创建临界区保护 */
    {
        /* 递增RTOS节拍计数器。 */
        if( xTaskIncrementTick() != pdFALSE )  /* 递增系统节拍并检查是否需要上下文切换 */
        {
            /* 需要上下文切换。上下文切换在PendSV中断中执行。
            挂起PendSV中断。 */
            portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;  /* 设置PendSV挂起位，请求上下文切换 */
        }
    }
    vPortClearBASEPRIFromISR();  /* 清除中断优先级掩码，退出临界区 */
}

/*******************************************************************************
函数名称：vPortSuppressTicksAndSleep
功能描述：FreeRTOS无空闲节拍(tickless idle)模式实现函数，用于在系统空闲时进入低功耗状态。
          通过动态调整SysTick定时器来延长睡眠时间，减少不必要的定时器中断，从而降低功耗。
输入参数：xExpectedIdleTime - 预期的空闲时间（以系统节拍数为单位）
输出参数：无
返 回 值：无
其它说明：1.此函数为弱定义(__weak)，允许用户自定义实现
          2.仅在configUSE_TICKLESS_IDLE为1时编译
          3.通过精确计算SysTick重载值实现长时间睡眠
          4.支持睡眠前和睡眠后的用户自定义处理

修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          创建
*******************************************************************************/
#if configUSE_TICKLESS_IDLE == 1

    __weak void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime )
    {
        uint32_t ulReloadValue, ulCompleteTickPeriods, ulCompletedSysTickDecrements, ulSysTickCTRL;
        TickType_t xModifiableIdleTime;

        /* 确保SysTick重载值不会使计数器溢出。 */
        if( xExpectedIdleTime > xMaximumPossibleSuppressedTicks )
        {
            xExpectedIdleTime = xMaximumPossibleSuppressedTicks;  /* 限制最大可抑制的节拍数 */
        }

        /* 暂时停止SysTick。SysTick停止的时间会尽可能被考虑进去，但使用无空闲节拍模式
        将不可避免地导致内核维护的时间与日历时间之间存在微小漂移。 */
        portNVIC_SYSTICK_CTRL_REG &= ~portNVIC_SYSTICK_ENABLE_BIT;  /* 禁用SysTick定时器 */

        /* 计算等待xExpectedIdleTime个节拍周期所需的重新加载值。
        使用-1是因为此代码将在其中一个节拍周期的一部分时间内执行。 */
        ulReloadValue = portNVIC_SYSTICK_CURRENT_VALUE_REG + ( ulTimerCountsForOneTick * ( xExpectedIdleTime - 1UL ) );  /* 计算重载值 */
        if( ulReloadValue > ulStoppedTimerCompensation )  /* 如果重载值大于停止计时器补偿值 */
        {
            ulReloadValue -= ulStoppedTimerCompensation;  /* 减去补偿值 */
        }

        /* 进入临界区，但不使用taskENTER_CRITICAL()方法，因为该方法会屏蔽应退出睡眠模式的中断。 */
        __disable_irq();  /* 禁用中断 */
        __dsb( portSY_FULL_READ_WRITE );  /* 数据同步屏障 */
        __isb( portSY_FULL_READ_WRITE );  /* 指令同步屏障 */

        /* 如果上下文切换挂起或任务正在等待调度程序恢复，则放弃低功耗进入。 */
        if( eTaskConfirmSleepModeStatus() == eAbortSleep )  /* 检查是否应中止睡眠 */
        {
            /* 从计数寄存器中剩余的任意值重新启动，以完成此节拍周期。 */
            portNVIC_SYSTICK_LOAD_REG = portNVIC_SYSTICK_CURRENT_VALUE_REG;  /* 设置重载值为当前值 */

            /* 重新启动SysTick。 */
            portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;  /* 启用SysTick定时器 */

            /* 将重载寄存器重置为正常节拍周期所需的值。 */
            portNVIC_SYSTICK_LOAD_REG = ulTimerCountsForOneTick - 1UL;  /* 设置正常节拍的重载值 */

            /* 重新启用中断 - 参见上面__disable_irq()调用的注释。 */
            __enable_irq();  /* 启用中断 */
        }
        else
        {
            /* 设置新的重载值。 */
            portNVIC_SYSTICK_LOAD_REG = ulReloadValue;  /* 设置计算得到的重载值 */

            /* 清除SysTick计数标志并将计数值重置为零。 */
            portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;  /* 重置当前计数值 */

            /* 重新启动SysTick。 */
            portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;  /* 启用SysTick定时器 */

            /* 睡眠直到发生某些事情。configPRE_SLEEP_PROCESSING()可以将其参数设置为0，
            表示其实现包含自己的等待中断或等待事件指令，因此不应再次执行wfi。
            但是，必须保持原始的预期空闲时间变量不变，因此需要获取副本。 */
            xModifiableIdleTime = xExpectedIdleTime;  /* 创建可修改的副本 */
            configPRE_SLEEP_PROCESSING( xModifiableIdleTime );  /* 调用睡眠前处理钩子函数 */
            if( xModifiableIdleTime > 0 )  /* 如果还需要睡眠 */
            {
                __dsb( portSY_FULL_READ_WRITE );  /* 数据同步屏障 */
                __wfi();  /* 等待中断指令，进入低功耗状态 */
                __isb( portSY_FULL_READ_WRITE );  /* 指令同步屏障 */
            }
            configPOST_SLEEP_PROCESSING( xExpectedIdleTime );  /* 调用睡眠后处理钩子函数 */

            /* 再次停止SysTick。SysTick停止的时间会尽可能被考虑进去，但使用无空闲节拍模式
            将不可避免地导致内核维护的时间与日历时间之间存在微小漂移。 */
            ulSysTickCTRL = portNVIC_SYSTICK_CTRL_REG;  /* 保存控制寄存器值 */
            portNVIC_SYSTICK_CTRL_REG = ( ulSysTickCTRL & ~portNVIC_SYSTICK_ENABLE_BIT );  /* 禁用SysTick定时器 */

            /* 重新启用中断 - 参见上面__disable_irq()调用的注释。 */
            __enable_irq();  /* 启用中断 */

            if( ( ulSysTickCTRL & portNVIC_SYSTICK_COUNT_FLAG_BIT ) != 0 )  /* 检查计数标志是否置位 */
            {
                uint32_t ulCalculatedLoadValue;

                /* 节拍中断已经执行，并且SysTick计数已使用ulReloadValue重新加载。
                使用此节拍周期剩余的任何值重置portNVIC_SYSTICK_LOAD_REG。 */
                ulCalculatedLoadValue = ( ulTimerCountsForOneTick - 1UL ) - ( ulReloadValue - portNVIC_SYSTICK_CURRENT_VALUE_REG );  /* 计算剩余的重载值 */

                /* 不允许使用微小值，或者由于睡眠后钩子函数执行了某些耗时操作而导致下溢的值。 */
                if( ( ulCalculatedLoadValue < ulStoppedTimerCompensation ) || ( ulCalculatedLoadValue > ulTimerCountsForOneTick ) )  /* 检查计算值是否在有效范围内 */
                {
                    ulCalculatedLoadValue = ( ulTimerCountsForOneTick - 1UL );  /* 使用默认值 */
                }

                portNVIC_SYSTICK_LOAD_REG = ulCalculatedLoadValue;  /* 设置计算得到的重载值 */

                /* 节拍中断处理程序已经在内核中挂起了节拍处理。由于挂起的节拍将在
                此函数退出后立即处理，因此节拍维护的节拍值将向前步进等待时间减一。 */
                ulCompleteTickPeriods = xExpectedIdleTime - 1UL;  /* 计算完整的节拍周期数 */
            }
            else
            {
                /* 节拍中断以外的其他事件结束了睡眠。
                计算睡眠持续时间，四舍五入到完整的节拍周期（不是考虑部分节拍的ulReload值）。 */
                ulCompletedSysTickDecrements = ( xExpectedIdleTime * ulTimerCountsForOneTick ) - portNVIC_SYSTICK_CURRENT_VALUE_REG;  /* 计算已完成的SysTick递减次数 */

                /* 处理器等待时经过了多少完整的节拍周期？ */
                ulCompleteTickPeriods = ulCompletedSysTickDecrements / ulTimerCountsForOneTick;  /* 计算完整的节拍周期数 */

                /* 重载值设置为剩余的单节拍周期的分数部分。 */
                portNVIC_SYSTICK_LOAD_REG = ( ( ulCompleteTickPeriods + 1UL ) * ulTimerCountsForOneTick ) - ulCompletedSysTickDecrements;  /* 计算并设置重载值 */
            }

            /* 重新启动SysTick，使其从portNVIC_SYSTICK_LOAD_REG再次运行，
            然后将portNVIC_SYSTICK_LOAD_REG设置回其标准值。
            使用临界区确保在重载寄存器接近零的情况下，节拍中断只能执行一次。 */
            portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;  /* 重置当前计数值 */
            portENTER_CRITICAL();  /* 进入临界区 */
            {
                portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;  /* 启用SysTick定时器 */
                vTaskStepTick( ulCompleteTickPeriods );  /* 步进系统节拍计数器 */
                portNVIC_SYSTICK_LOAD_REG = ulTimerCountsForOneTick - 1UL;  /* 恢复正常的重载值 */
            }
            portEXIT_CRITICAL();  /* 退出临界区 */
        }
    }

#endif /* #if configUSE_TICKLESS_IDLE */

/*******************************************************************************
函数名称：vPortSetupTimerInterrupt
功能描述：配置SysTick定时器中断，设置系统节拍频率和相关参数。
          根据配置计算定时器重载值，并初始化SysTick控制寄存器。
输入参数：无
输出参数：无
返 回 值：无
其它说明：1.仅在configOVERRIDE_DEFAULT_TICK_CONFIGURATION为0时编译
          2.支持无空闲节拍模式(configUSE_TICKLESS_IDLE)的相关计算
          3.配置SysTick定时器以指定的频率产生中断

修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          创建
*******************************************************************************/
#if configOVERRIDE_DEFAULT_TICK_CONFIGURATION == 0

    void vPortSetupTimerInterrupt( void )
    {
        /* 计算配置节拍中断所需的常量。 */
        #if configUSE_TICKLESS_IDLE == 1  /* 如果启用无空闲节拍模式 */
        {
            ulTimerCountsForOneTick = ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ );  /* 计算一个节拍所需的定时器计数 */
            xMaximumPossibleSuppressedTicks = portMAX_24_BIT_NUMBER / ulTimerCountsForOneTick;  /* 计算最大可抑制的节拍数 */
            ulStoppedTimerCompensation = portMISSED_COUNTS_FACTOR / ( configCPU_CLOCK_HZ / configSYSTICK_CLOCK_HZ );  /* 计算停止计时器补偿值 */
        }
        #endif /* configUSE_TICKLESS_IDLE */

        /* 配置SysTick以请求的速率中断。 */
        portNVIC_SYSTICK_LOAD_REG = ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ ) - 1UL;  /* 设置SysTick重载值 */
        portNVIC_SYSTICK_CTRL_REG = ( portNVIC_SYSTICK_CLK_BIT | portNVIC_SYSTICK_INT_BIT | portNVIC_SYSTICK_ENABLE_BIT );  /* 配置并启用SysTick */
    }

#endif /* configOVERRIDE_DEFAULT_TICK_CONFIGURATION */

/*******************************************************************************
函数名称：vPortGetIPSR
功能描述：获取当前IPSR(中断程序状态寄存器)的值，用于确定当前正在执行的中断或异常编号。
          该函数通过汇编指令直接读取IPSR寄存器的值并返回。
输入参数：无
输出参数：无
返 回 值：uint32_t类型，返回当前IPSR寄存器的值，包含正在处理的中断或异常编号
其它说明：1.使用MRS指令直接读取IPSR寄存器
          2.遵循ARM AAPCS调用约定，返回值通过r0寄存器传递
          3.使用PRESERVE8保证8字节栈对齐

修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          创建
*******************************************************************************/
__asm uint32_t vPortGetIPSR( void )
{
    PRESERVE8          /* 指定8字节栈对齐，符合ARM EABI标准 */

    mrs r0, ipsr       /* 将中断程序状态寄存器(IPSR)的值移动到r0寄存器 */
    bx r14             /* 使用连接寄存器r14(即lr)返回到调用者，返回值在r0中 */
}

/*******************************************************************************
函数名称：vPortValidateInterruptPriority
功能描述：验证中断优先级配置是否正确，确保使用FreeRTOS API的中断具有合适的优先级。
          检查当前中断的优先级是否低于或等于configMAX_SYSCALL_INTERRUPT_PRIORITY，
          并验证优先级分组设置是否正确。
输入参数：无
输出参数：无
返 回 值：无
其它说明：1.仅在configASSERT_DEFINED为1时编译(调试模式下)
          2.使用断言检查中断优先级配置的正确性
          3.确保FreeRTOS API从中断安全函数正确调用

修改日期      版本号          修改人            修改内容
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          创建
*******************************************************************************/
#if( configASSERT_DEFINED == 1 )

    void vPortValidateInterruptPriority( void )
    {
        uint32_t ulCurrentInterrupt;
        uint8_t ucCurrentPriority;

        /* 获取当前正在执行的中断编号。 */
        ulCurrentInterrupt = vPortGetIPSR();  /* 调用vPortGetIPSR函数获取当前中断号 */

        /* 中断编号是否为用户定义的中断？ */
        if( ulCurrentInterrupt >= portFIRST_USER_INTERRUPT_NUMBER )  /* 检查是否为用户中断(非内核异常) */
        {
            /* 查找中断的优先级。 */
            ucCurrentPriority = pcInterruptPriorityRegisters[ ulCurrentInterrupt ];  /* 从中断优先级寄存器数组获取当前优先级 */

            /* 如果为已被分配高于configMAX_SYSCALL_INTERRUPT_PRIORITY优先级的中断的服务例程(ISR)
            调用了ISR安全的FreeRTOS API函数，以下断言将失败。ISR安全的FreeRTOS API函数必须*仅*
            从已被分配优先级等于或低于configMAX_SYSCALL_INTERRUPT_PRIORITY的中断中调用。

            数值低的中断优先级编号表示逻辑上的高中断优先级，因此中断的优先级必须设置为等于或数值上
            *高于*configMAX_SYSCALL_INTERRUPT_PRIORITY的值。

            使用FreeRTOS API的中断不能保留其默认优先级零，因为这是最高可能的优先级，
            保证高于configMAX_SYSCALL_INTERRUPT_PRIORITY，因此也保证无效。

            FreeRTOS维护独立的线程和ISR API函数，以确保中断入口尽可能快速和简单。

            以下链接提供详细信息：
            http://www.freertos.org/RTOS-Cortex-M3-M4.html
            http://www.freertos.org/FAQHelp.html */
            configASSERT( ucCurrentPriority >= ucMaxSysCallPriority );  /* 断言检查当前中断优先级是否符合要求 */
        }

        /* 优先级分组：中断控制器(NVIC)允许将定义每个中断优先位的位拆分为定义中断的抢占优先级位
        和定义中断的子优先级的位。为简单起见，所有位必须定义为抢占优先级位。如果不是这种情况
        (如果某些位表示子优先级)，以下断言将失败。

        如果应用程序仅使用CMSIS库进行中断配置，则可以通过在启动调度程序之前调用
        NVIC_SetPriorityGrouping(0);在所有Cortex-M设备上实现正确设置。但请注意，
        某些供应商特定的外设库假定非零优先级组设置，在这种情况下，使用零值将导致不可预测的行为。 */
        configASSERT( ( portAIRCR_REG & portPRIORITY_GROUP_MASK ) <= ulMaxPRIGROUPValue );  /* 断言检查优先级分组设置 */
    }

#endif /* configASSERT_DEFINED */




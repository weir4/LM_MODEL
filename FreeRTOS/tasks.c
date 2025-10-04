/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： LM_task.c
 * 文件标识： 
 * 内容摘要： 任务模块定义
 * 其它说明： 无
 * 当前版本： FreeRTOS V9.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年09月13日
 *
 *******************************************************************************/


/* Define to prevent recursive inclusion -------------------------------------*/

/* Includes ------------------------------------------------------------------*/
/* Standard includes. */
#include <stdlib.h>
#include <string.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "StackMacros.h"

/* Exported types ------------------------------------------------------------*/
/*
 * Task control block.  A task control block (TCB) is allocated for each task,
 * and stores task state information, including a pointer to the task's context
 * (the task's run time environment, including register values)
 */
typedef struct tskTaskControlBlock
{
	volatile StackType_t	*pxTopOfStack;	/*< 指向任务栈顶的位置，必须是TCB结构的第一个成员 */

	#if ( portUSING_MPU_WRAPPERS == 1 )
		xMPU_SETTINGS	xMPUSettings;		/*< MPU设置，必须是TCB结构的第二个成员 */
	#endif

	ListItem_t			xStateListItem;	/*< 状态列表项，用于表示任务状态（就绪、阻塞、挂起） */
	ListItem_t			xEventListItem;		/*< 事件列表项，用于从事件列表中引用任务 */
	UBaseType_t			uxPriority;			/*< 任务优先级，0为最低优先级 */
	StackType_t			*pxStack;			/*< 指向堆栈起始位置 */
	char				pcTaskName[ configMAX_TASK_NAME_LEN ];/*< 任务创建时给定的描述性名称，便于调试 */

	#if ( portSTACK_GROWTH > 0 )
		StackType_t		*pxEndOfStack;		/*< 在堆栈从低内存向上增长的架构中指向堆栈末端 */
	#endif

	#if ( portCRITICAL_NESTING_IN_TCB == 1 )
		UBaseType_t		uxCriticalNesting;	/*< 保存关键段嵌套深度 */
	#endif

	#if ( configUSE_TRACE_FACILITY == 1 )
		UBaseType_t		uxTCBNumber;		/*< 存储TCB创建时递增的数字，便于调试器检测任务删除和重建 */
		UBaseType_t		uxTaskNumber;		/*< 专门供第三方跟踪代码使用的数字 */
	#endif

	#if ( configUSE_MUTEXES == 1 )
		UBaseType_t		uxBasePriority;		/*< 最后分配给任务的优先级 - 用于优先级继承机制 */
		UBaseType_t		uxMutexesHeld;      /*< 持有的互斥量数量 */
	#endif

	#if ( configUSE_APPLICATION_TASK_TAG == 1 )
		TaskHookFunction_t pxTaskTag;       /*< 任务标签函数 */
	#endif

	#if( configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0 )
		void *pvThreadLocalStoragePointers[ configNUM_THREAD_LOCAL_STORAGE_POINTERS ]; /*< 线程本地存储指针 */
	#endif

	#if( configGENERATE_RUN_TIME_STATS == 1 )
		uint32_t		ulRunTimeCounter;	/*< 存储任务在运行状态下花费的时间量 */
	#endif

	#if ( configUSE_NEWLIB_REENTRANT == 1 )
		struct	_reent xNewLib_reent;       /*< Newlib重入结构 */
	#endif

	#if( configUSE_TASK_NOTIFICATIONS == 1 )
		volatile uint32_t ulNotifiedValue;  /*< 通知值 */
		volatile uint8_t ucNotifyState;     /*< 通知状态 */
	#endif

	#if( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 )
		uint8_t	ucStaticallyAllocated; 		/*< 如果任务是静态分配的，则设置为pdTRUE，确保不会尝试释放内存 */
	#endif

	#if( INCLUDE_xTaskAbortDelay == 1 )
		uint8_t ucDelayAborted;             /*< 延迟中止标志 */
	#endif

} tskTCB;

/* The old tskTCB name is maintained above then typedefed to the new TCB_t name
below to enable the use of older kernel aware debuggers. */
typedef tskTCB TCB_t;  /*< TCB类型定义，兼容旧版内核感知调试器 */

/* Exported constants --------------------------------------------------------*/
/* Values that can be assigned to the ucNotifyState member of the TCB. */
#define taskNOT_WAITING_NOTIFICATION	( ( uint8_t ) 0 )  /*< 任务未等待通知 */
#define taskWAITING_NOTIFICATION		( ( uint8_t ) 1 )  /*< 任务正在等待通知 */
#define taskNOTIFICATION_RECEIVED		( ( uint8_t ) 2 )  /*< 任务已收到通知 */

/*
 * The value used to fill the stack of a task when the task is created.  This
 * is used purely for checking the high water mark for tasks.
 */
#define tskSTACK_FILL_BYTE	( 0xa5U )  /*< 任务栈填充字节，用于检测栈使用高水位线 */

/*
 * Macros used by vListTask to indicate which state a task is in.
 */
#define tskBLOCKED_CHAR		( 'B' )  /*< 阻塞状态字符表示 */
#define tskREADY_CHAR		( 'R' )  /*< 就绪状态字符表示 */
#define tskDELETED_CHAR		( 'D' )  /*< 已删除状态字符表示 */
#define tskSUSPENDED_CHAR	( 'S' )  /*< 挂起状态字符表示 */

/* Exported macro ------------------------------------------------------------*/
#if( configUSE_PREEMPTION == 0 )
	/* If the cooperative scheduler is being used then a yield should not be
	performed just because a higher priority task has been woken. */
	#define taskYIELD_IF_USING_PREEMPTION()  /*< 协作调度器下不进行任务切换 */
#else
	#define taskYIELD_IF_USING_PREEMPTION() portYIELD_WITHIN_API()  /*< 抢占式调度器下进行任务切换 */
#endif

/* Exported functions --------------------------------------------------------*/
/* 注：此处列出的是在头文件中声明的外部可见函数，实际定义在文件后面 */

/* Private types -------------------------------------------------------------*/
/* 注：此处没有额外的私有类型定义，所有类型已在导出类型中定义 */

/* Private variables ---------------------------------------------------------*/

PRIVILEGED_DATA TCB_t * volatile pxCurrentTCB = NULL;  /*< 指向当前运行任务的TCB */

/* Lists for ready and blocked tasks. --------------------*/
PRIVILEGED_DATA static List_t pxReadyTasksLists[ configMAX_PRIORITIES ];  /*< 优先级就绪任务列表 */
PRIVILEGED_DATA static List_t xDelayedTaskList1;                         /*< 延迟任务列表1 */
PRIVILEGED_DATA static List_t xDelayedTaskList2;                         /*< 延迟任务列表2（用于处理计数器溢出的延迟） */
PRIVILEGED_DATA static List_t * volatile pxDelayedTaskList;              /*< 指向当前使用的延迟任务列表 */
PRIVILEGED_DATA static List_t * volatile pxOverflowDelayedTaskList;      /*< 指向处理计数器溢出的延迟任务列表 */
PRIVILEGED_DATA static List_t xPendingReadyList;                         /*< 调度器挂起时已就绪的任务列表 */

#if( INCLUDE_vTaskDelete == 1 )
	PRIVILEGED_DATA static List_t xTasksWaitingTermination;              /*< 等待终止的任务列表 */
	PRIVILEGED_DATA static volatile UBaseType_t uxDeletedTasksWaitingCleanUp = ( UBaseType_t ) 0U;  /*< 等待清理的已删除任务计数 */
#endif

#if ( INCLUDE_vTaskSuspend == 1 )
	PRIVILEGED_DATA static List_t xSuspendedTaskList;                    /*< 挂起的任务列表 */
#endif

/* Other file private variables. --------------------------------*/
PRIVILEGED_DATA static volatile UBaseType_t uxCurrentNumberOfTasks = ( UBaseType_t ) 0U;  /*< 当前任务数量 */
PRIVILEGED_DATA static volatile TickType_t xTickCount = ( TickType_t ) 0U;                /*< 系统滴答计数器 */
PRIVILEGED_DATA static volatile UBaseType_t uxTopReadyPriority = tskIDLE_PRIORITY;       /*< 最高就绪优先级 */
PRIVILEGED_DATA static volatile BaseType_t xSchedulerRunning = pdFALSE;                   /*< 调度器运行标志 */
PRIVILEGED_DATA static volatile UBaseType_t uxPendedTicks = ( UBaseType_t ) 0U;          /*< 挂起的滴答计数 */
PRIVILEGED_DATA static volatile BaseType_t xYieldPending = pdFALSE;                      /*< 待处理的任务切换标志 */
PRIVILEGED_DATA static volatile BaseType_t xNumOfOverflows = ( BaseType_t ) 0;           /*< 计数器溢出次数 */
PRIVILEGED_DATA static UBaseType_t uxTaskNumber = ( UBaseType_t ) 0U;                    /*< 任务编号计数器 */
PRIVILEGED_DATA static volatile TickType_t xNextTaskUnblockTime = ( TickType_t ) 0U;     /*< 下一个任务解除阻塞的时间 */
PRIVILEGED_DATA static TaskHandle_t xIdleTaskHandle = NULL;                              /*< 空闲任务句柄 */

PRIVILEGED_DATA static volatile UBaseType_t uxSchedulerSuspended = ( UBaseType_t ) pdFALSE;  /*< 调度器挂起标志 */

#if ( configGENERATE_RUN_TIME_STATS == 1 )
	PRIVILEGED_DATA static uint32_t ulTaskSwitchedInTime = 0UL;  /*< 上次任务切换时的时间/计数器值 */
	PRIVILEGED_DATA static uint32_t ulTotalRunTime = 0UL;        /*< 运行时间计数器时钟定义的总执行时间 */
#endif

/* Private constants ---------------------------------------------------------*/
/* 注：此处没有额外的私有常量定义，所有常量已在导出常量中定义 */

/* Private macros ------------------------------------------------------------*/

#ifdef portREMOVE_STATIC_QUALIFIER
	#define static  /*< 移除静态限定符以满足某些调试器的需求 */
#endif

#if ( configUSE_PORT_OPTIMISED_TASK_SELECTION == 0 )
	/* 非端口优化的任务选择实现 */
	#define taskRECORD_READY_PRIORITY( uxPriority )                      \
	{                                                                   \
		if( ( uxPriority ) > uxTopReadyPriority )                       \
		{                                                               \
			uxTopReadyPriority = ( uxPriority );                        \
		}                                                               \
	} /* taskRECORD_READY_PRIORITY */

	#define taskSELECT_HIGHEST_PRIORITY_TASK()                          \
	{                                                                   \
	UBaseType_t uxTopPriority = uxTopReadyPriority;                     \
                                                                        \
		while( listLIST_IS_EMPTY( &( pxReadyTasksLists[ uxTopPriority ] ) ) ) \
		{                                                               \
			configASSERT( uxTopPriority );                              \
			--uxTopPriority;                                            \
		}                                                               \
                                                                        \
		listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &( pxReadyTasksLists[ uxTopPriority ] ) ); \
		uxTopReadyPriority = uxTopPriority;                             \
	} /* taskSELECT_HIGHEST_PRIORITY_TASK */

	#define taskRESET_READY_PRIORITY( uxPriority )
	#define portRESET_READY_PRIORITY( uxPriority, uxTopReadyPriority )

#else /* configUSE_PORT_OPTIMISED_TASK_SELECTION */
	/* 端口优化的任务选择实现 */
	#define taskRECORD_READY_PRIORITY( uxPriority )	portRECORD_READY_PRIORITY( uxPriority, uxTopReadyPriority )

	#define taskSELECT_HIGHEST_PRIORITY_TASK()                          \
	{                                                                   \
	UBaseType_t uxTopPriority;                                          \
                                                                        \
		portGET_HIGHEST_PRIORITY( uxTopPriority, uxTopReadyPriority );  \
		configASSERT( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ uxTopPriority ] ) ) > 0 ); \
		listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &( pxReadyTasksLists[ uxTopPriority ] ) ); \
	} /* taskSELECT_HIGHEST_PRIORITY_TASK() */

	#define taskRESET_READY_PRIORITY( uxPriority )                      \
	{                                                                   \
		if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ ( uxPriority ) ] ) ) == ( UBaseType_t ) 0 ) \
		{                                                               \
			portRESET_READY_PRIORITY( ( uxPriority ), ( uxTopReadyPriority ) ); \
		}                                                               \
	}

#endif /* configUSE_PORT_OPTIMISED_TASK_SELECTION */

/* pxDelayedTaskList and pxOverflowDelayedTaskList are switched when the tick
count overflows. */
#define taskSWITCH_DELAYED_LISTS()                                      \
{                                                                       \
	List_t *pxTemp;                                                     \
                                                                        \
	configASSERT( ( listLIST_IS_EMPTY( pxDelayedTaskList ) ) );         \
                                                                        \
	pxTemp = pxDelayedTaskList;                                         \
	pxDelayedTaskList = pxOverflowDelayedTaskList;                      \
	pxOverflowDelayedTaskList = pxTemp;                                 \
	xNumOfOverflows++;                                                  \
	prvResetNextTaskUnblockTime();                                      \
}


#define prvAddTaskToReadyList( pxTCB )                                  \
	traceMOVED_TASK_TO_READY_STATE( pxTCB );                            \
	taskRECORD_READY_PRIORITY( ( pxTCB )->uxPriority );                 \
	vListInsertEnd( &( pxReadyTasksLists[ ( pxTCB )->uxPriority ] ), &( ( pxTCB )->xStateListItem ) ); \
	tracePOST_MOVED_TASK_TO_READY_STATE( pxTCB )


#define prvGetTCBFromHandle( pxHandle ) ( ( ( pxHandle ) == NULL ) ? ( TCB_t * ) pxCurrentTCB : ( TCB_t * ) ( pxHandle ) )


#if( configUSE_16_BIT_TICKS == 1 )
	#define taskEVENT_LIST_ITEM_VALUE_IN_USE	0x8000U      /*< 事件列表项值使用中标志(16位) */
#else
	#define taskEVENT_LIST_ITEM_VALUE_IN_USE	0x80000000UL /*< 事件列表项值使用中标志(32位) */
#endif

/* Private functions ---------------------------------------------------------*/
/* Callback function prototypes. --------------------------*/
#if( configCHECK_FOR_STACK_OVERFLOW > 0 )
	extern void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName );  /*< 栈溢出钩子函数 */
#endif

#if( configUSE_TICK_HOOK > 0 )
	extern void vApplicationTickHook( void );  /*< 滴答钩子函数 */
#endif

#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	extern void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );  /*< 获取空闲任务内存 */
#endif


#if ( INCLUDE_vTaskSuspend == 1 )
	static BaseType_t prvTaskIsTaskSuspended( const TaskHandle_t xTask ) PRIVILEGED_FUNCTION;  /*< 检查任务是否挂起 */
#endif /* INCLUDE_vTaskSuspend */


static void prvInitialiseTaskLists( void ) PRIVILEGED_FUNCTION;  /*< 初始化任务列表 */


static portTASK_FUNCTION_PROTO( prvIdleTask, pvParameters );  /*< 空闲任务函数 */


#if ( INCLUDE_vTaskDelete == 1 )
	static void prvDeleteTCB( TCB_t *pxTCB ) PRIVILEGED_FUNCTION;  /*< 删除TCB */
#endif


static void prvCheckTasksWaitingTermination( void ) PRIVILEGED_FUNCTION;  /*< 检查等待终止的任务 */


static void prvAddCurrentTaskToDelayedList( TickType_t xTicksToWait, const BaseType_t xCanBlockIndefinitely ) PRIVILEGED_FUNCTION;  /*< 将当前任务添加到延迟列表 */


#if ( configUSE_TRACE_FACILITY == 1 )
	static UBaseType_t prvListTasksWithinSingleList( TaskStatus_t *pxTaskStatusArray, List_t *pxList, eTaskState eState ) PRIVILEGED_FUNCTION;  /*< 列出单个列表中的任务 */
#endif


#if ( INCLUDE_xTaskGetHandle == 1 )
	static TCB_t *prvSearchForNameWithinSingleList( List_t *pxList, const char pcNameToQuery[] ) PRIVILEGED_FUNCTION;  /*< 在列表中按名称搜索任务 */
#endif


#if ( ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) )
	static uint16_t prvTaskCheckFreeStackSpace( const uint8_t * pucStackByte ) PRIVILEGED_FUNCTION;  /*< 检查任务栈空闲空间 */
#endif


#if ( configUSE_TICKLESS_IDLE != 0 )
	static TickType_t prvGetExpectedIdleTime( void ) PRIVILEGED_FUNCTION;  /*< 获取预期空闲时间 */
#endif


static void prvResetNextTaskUnblockTime( void );  /*< 重置下一个任务解除阻塞时间 */

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )
	static char *prvWriteNameToBuffer( char *pcBuffer, const char *pcTaskName ) PRIVILEGED_FUNCTION;  /*< 将任务名称写入缓冲区 */
#endif

static void prvInitialiseNewTask( TaskFunction_t pxTaskCode,
									const char * const pcName,
									const uint32_t ulStackDepth,
									void * const pvParameters,
									UBaseType_t uxPriority,
									TaskHandle_t * const pxCreatedTask,
									TCB_t *pxNewTCB,
									const MemoryRegion_t * const xRegions ) PRIVILEGED_FUNCTION;  /*< 初始化新任务 */

static void prvAddNewTaskToReadyList( TCB_t *pxNewTCB ) PRIVILEGED_FUNCTION;  /*< 将新任务添加到就绪列表 */

/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xTaskCreateStatic
 * 功能描述：静态创建新任务并添加到就绪列表，使用预分配的TCB和栈内存
 *           适用于不支持动态内存分配或需要精确控制内存布局的嵌入式系统
 * 输入参数：
 *   - pxTaskCode:     任务函数指针，指向任务的具体实现函数
 *   - pcName:         任务名称字符串，用于调试和识别任务
 *   - ulStackDepth:   任务栈深度（以字为单位，不是字节数）
 *   - pvParameters:   传递给任务函数的参数指针
 *   - uxPriority:     任务优先级（0为最低优先级，configMAX_PRIORITIES-1为最高）
 *   - puxStackBuffer: 指向预分配的栈内存缓冲区的指针
 *   - pxTaskBuffer:   指向预分配的静态任务控制块(TCB)内存的指针
 * 输出参数：无
 * 返 回 值：
 *   - TaskHandle_t:   成功创建时返回任务句柄，失败时返回NULL
 * 其它说明：
 *   - 此函数仅在启用静态分配时编译（configSUPPORT_STATIC_ALLOCATION == 1）
 *   - 任务的TCB和栈空间都由用户预先分配，不涉及动态内存分配
 *   - 适用于内存受限或需要精确内存管理的场景
 *   - 创建的任务会自动添加到就绪列表，等待调度器调度
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
#if( configSUPPORT_STATIC_ALLOCATION == 1 )

TaskHandle_t xTaskCreateStatic( TaskFunction_t pxTaskCode,
                                const char * const pcName,
                                const uint32_t ulStackDepth,
                                void * const pvParameters,
                                UBaseType_t uxPriority,
                                StackType_t * const puxStackBuffer,
                                StaticTask_t * const pxTaskBuffer ) /*lint !e971 允许未限定的char类型用于字符串和单个字符 */
{
    TCB_t *pxNewTCB;          /* 指向新任务控制块的指针 */
    TaskHandle_t xReturn;     /* 函数返回值（任务句柄） */

    /* 断言检查：确保栈缓冲区和任务缓冲区指针不为NULL */
    configASSERT( puxStackBuffer != NULL );
    configASSERT( pxTaskBuffer != NULL );

    /* 再次检查栈缓冲区和任务缓冲区指针是否有效 */
    if( ( pxTaskBuffer != NULL ) && ( puxStackBuffer != NULL ) )
    {
        /* 任务的TCB和栈内存由此函数传入 - 直接使用它们 */
        pxNewTCB = ( TCB_t * ) pxTaskBuffer; /*lint !e740 不寻常的转换是可以的，因为结构设计为具有相同的对齐方式，并且大小由断言检查 */
        pxNewTCB->pxStack = ( StackType_t * ) puxStackBuffer;

        /* 如果同时支持静态和动态分配，标记任务的分配方式 */
        #if( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 )
        {
            /* 任务可以静态或动态创建，因此标记此任务是静态创建的，
               以便后续删除时正确处理 */
            pxNewTCB->ucStaticallyAllocated = tskSTATICALLY_ALLOCATED_STACK_AND_TCB;
        }
        #endif /* configSUPPORT_DYNAMIC_ALLOCATION */

        /* 初始化新任务：设置任务函数、名称、栈深度、参数、优先级等 */
        prvInitialiseNewTask( pxTaskCode,      /* 任务函数指针 */
                              pcName,          /* 任务名称 */
                              ulStackDepth,    /* 栈深度 */
                              pvParameters,    /* 任务参数 */
                              uxPriority,      /* 任务优先级 */
                              &xReturn,        /* 返回任务句柄 */
                              pxNewTCB,        /* 新任务的TCB */
                              NULL );          /* 无MPU区域配置 */

        /* 将新任务添加到就绪列表，使其可被调度器调度 */
        prvAddNewTaskToReadyList( pxNewTCB );
    }
    else
    {
        /* 设置返回值为NULL表示创建失败 */
        xReturn = NULL;
    }

    /* 返回任务句柄或NULL */
    return xReturn;
}

#endif /* SUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xTaskCreateRestricted
 * 功能描述：创建受MPU（内存保护单元）保护的任务，使用静态分配的栈缓冲区
 *           该函数在启用MPU包装器时可用，用于创建具有内存保护特性的任务
 * 输入参数：
 *   - pxTaskDefinition: 指向任务参数结构体的指针，包含任务的所有配置信息
 *   - pxCreatedTask:    用于返回任务句柄的指针变量地址
 * 输出参数：
 *   - pxCreatedTask:    成功创建任务后，将任务句柄写入此地址
 * 返 回 值：
 *   - pdPASS:           任务创建成功
 *   - errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY: 内存分配失败，任务创建失败
 * 其它说明：
 *   - 此函数仅在启用MPU包装器时编译（portUSING_MPU_WRAPPERS == 1）
 *   - 任务使用静态分配的栈缓冲区，TCB（任务控制块）动态分配
 *   - 创建的任务会自动添加到就绪列表，等待调度
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
#if( portUSING_MPU_WRAPPERS == 1 )

BaseType_t xTaskCreateRestricted( const TaskParameters_t * const pxTaskDefinition, TaskHandle_t *pxCreatedTask )
{
    TCB_t *pxNewTCB;                                                /* 指向新任务控制块的指针 */
    BaseType_t xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;     /* 返回值，初始化为内存分配错误 */

    /* 断言检查：确保任务定义中的栈缓冲区指针不为NULL */
    configASSERT( pxTaskDefinition->puxStackBuffer );

    /* 再次检查栈缓冲区指针是否有效 */
    if( pxTaskDefinition->puxStackBuffer != NULL )
    {
        /* 为TCB分配内存。内存来源取决于端口malloc函数的实现以及是否使用静态分配 */
        pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) );

        /* 检查TCB内存是否分配成功 */
        if( pxNewTCB != NULL )
        {
            /* 在TCB中存储栈位置（使用静态分配的栈缓冲区） */
            pxNewTCB->pxStack = pxTaskDefinition->puxStackBuffer;

            /* 任务可以静态或动态创建，此任务使用静态分配的栈，但TCB是动态分配的，
               标记此任务的分配方式以便后续删除时正确处理 */
            pxNewTCB->ucStaticallyAllocated = tskSTATICALLY_ALLOCATED_STACK_ONLY;

            /* 初始化新任务：设置任务函数、名称、栈深度、参数、优先级等 */
            prvInitialiseNewTask( pxTaskDefinition->pvTaskCode,          /* 任务函数指针 */
                                  pxTaskDefinition->pcName,              /* 任务名称 */
                                  ( uint32_t ) pxTaskDefinition->usStackDepth, /* 栈深度 */
                                  pxTaskDefinition->pvParameters,        /* 任务参数 */
                                  pxTaskDefinition->uxPriority,          /* 任务优先级 */
                                  pxCreatedTask,                         /* 返回任务句柄 */
                                  pxNewTCB,                              /* 新任务的TCB */
                                  pxTaskDefinition->xRegions );          /* MPU内存区域配置 */

            /* 将新任务添加到就绪列表，使其可被调度器调度 */
            prvAddNewTaskToReadyList( pxNewTCB );

            /* 设置返回值为成功 */
            xReturn = pdPASS;
        }
    }

    /* 返回任务创建结果 */
    return xReturn;
}

#endif /* portUSING_MPU_WRAPPERS */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xTaskCreate
 * 功能描述：动态创建新任务并添加到就绪列表，是FreeRTOS中最常用的任务创建函数
 *           根据栈增长方向自动处理内存分配顺序，支持动态内存分配方式
 * 输入参数：
 *   - pxTaskCode:   任务函数指针，指向任务的具体实现函数
 *   - pcName:       任务名称字符串，用于调试和识别任务
 *   - usStackDepth: 任务栈深度（以字为单位，不是字节数）
 *   - pvParameters: 传递给任务函数的参数指针
 *   - uxPriority:   任务优先级（0为最低优先级，configMAX_PRIORITIES-1为最高）
 *   - pxCreatedTask: 用于返回任务句柄的指针变量地址
 * 输出参数：
 *   - pxCreatedTask: 成功创建任务后，将任务句柄写入此地址
 * 返 回 值：
 *   - pdPASS:       任务创建成功
 *   - errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY: 内存分配失败，任务创建失败
 * 其它说明：
 *   - 此函数仅在启用动态分配时编译（configSUPPORT_DYNAMIC_ALLOCATION == 1）
 *   - 任务的TCB和栈空间都是动态分配的
 *   - 根据端口的不同栈增长方向（向上或向下），分配顺序有所不同
 *   - 创建的任务会自动添加到就绪列表，等待调度器调度
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

BaseType_t xTaskCreate( TaskFunction_t pxTaskCode,
                        const char * const pcName,
                        const uint16_t usStackDepth,
                        void * const pvParameters,
                        UBaseType_t uxPriority,
                        TaskHandle_t * const pxCreatedTask ) /*lint !e971 允许未限定的char类型用于字符串和单个字符 */
{
    TCB_t *pxNewTCB;          /* 指向新任务控制块的指针 */
    BaseType_t xReturn;       /* 函数返回值 */

    /* 如果栈向下增长，则先分配栈空间再分配TCB，这样栈不会增长到TCB中。
       同样，如果栈向上增长，则先分配TCB再分配栈空间。 */
    #if( portSTACK_GROWTH > 0 )
    {
        /* 为TCB分配空间。内存来源取决于端口malloc函数的实现以及是否使用静态分配 */
        pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) );

        /* 检查TCB是否分配成功 */
        if( pxNewTCB != NULL )
        {
            /* 为正在创建的任务分配栈空间。
               栈内存的基地址存储在TCB中，以便后续需要时可以删除任务 */
            pxNewTCB->pxStack = ( StackType_t * ) pvPortMalloc( ( ( ( size_t ) usStackDepth ) * sizeof( StackType_t ) ) ); /*lint !e961 MISRA异常，因为转换仅对某些端口是冗余的 */

            /* 检查栈空间是否分配成功 */
            if( pxNewTCB->pxStack == NULL )
            {
                /* 无法分配栈空间，删除已分配的TCB */
                vPortFree( pxNewTCB );
                pxNewTCB = NULL;
            }
        }
    }
    #else /* portSTACK_GROWTH */
    {
        StackType_t *pxStack;  /* 栈指针 */

        /* 为正在创建的任务分配栈空间 */
        pxStack = ( StackType_t * ) pvPortMalloc( ( ( ( size_t ) usStackDepth ) * sizeof( StackType_t ) ) ); /*lint !e961 MISRA异常，因为转换仅对某些端口是冗余的 */

        /* 检查栈空间是否分配成功 */
        if( pxStack != NULL )
        {
            /* 为TCB分配空间 */
            pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) ); /*lint !e961 MISRA异常，因为转换仅对某些路径是冗余的 */

            /* 检查TCB是否分配成功 */
            if( pxNewTCB != NULL )
            {
                /* 在TCB中存储栈位置 */
                pxNewTCB->pxStack = pxStack;
            }
            else
            {
                /* 由于TCB未创建，栈空间无法使用，需要再次释放 */
                vPortFree( pxStack );
            }
        }
        else
        {
            pxNewTCB = NULL;
        }
    }
    #endif /* portSTACK_GROWTH */

    /* 检查TCB和栈空间是否都分配成功 */
    if( pxNewTCB != NULL )
    {
        #if( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 )
        {
            /* 任务可以静态或动态创建，因此标记此任务是动态创建的，
               以便后续删除时正确处理 */
            pxNewTCB->ucStaticallyAllocated = tskDYNAMICALLY_ALLOCATED_STACK_AND_TCB;
        }
        #endif /* configSUPPORT_STATIC_ALLOCATION */

        /* 初始化新任务：设置任务函数、名称、栈深度、参数、优先级等 */
        prvInitialiseNewTask( pxTaskCode,      /* 任务函数指针 */
                              pcName,          /* 任务名称 */
                              ( uint32_t ) usStackDepth, /* 栈深度 */
                              pvParameters,    /* 任务参数 */
                              uxPriority,      /* 任务优先级 */
                              pxCreatedTask,   /* 返回任务句柄 */
                              pxNewTCB,        /* 新任务的TCB */
                              NULL );          /* 无MPU区域配置 */

        /* 将新任务添加到就绪列表，使其可被调度器调度 */
        prvAddNewTaskToReadyList( pxNewTCB );

        /* 设置返回值为成功 */
        xReturn = pdPASS;
    }
    else
    {
        /* 设置返回值为内存分配错误 */
        xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    /* 返回任务创建结果 */
    return xReturn;
}

#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvInitialiseNewTask
 * 功能描述：初始化新任务的TCB（任务控制块）和栈空间，设置任务的各种属性和状态
 *           这是FreeRTOS任务创建的核心内部函数，负责任务控制块的全面初始化
 * 输入参数：
 *   - pxTaskCode:     任务函数指针，指向任务的具体实现函数
 *   - pcName:         任务名称字符串，用于调试和识别任务
 *   - ulStackDepth:   任务栈深度（以字为单位，不是字节数）
 *   - pvParameters:   传递给任务函数的参数指针
 *   - uxPriority:     任务优先级（0为最低优先级，configMAX_PRIORITIES-1为最高）
 *   - pxCreatedTask:  用于返回任务句柄的指针变量地址
 *   - pxNewTCB:       指向新任务控制块(TCB)的指针
 *   - xRegions:       MPU内存区域配置指针（如果使用MPU）
 * 输出参数：
 *   - pxCreatedTask:  成功初始化任务后，将任务句柄写入此地址
 * 返 回 值：无（静态函数，不对外部可见）
 * 其它说明：
 *   - 此函数是静态函数，仅在FreeRTOS内核内部使用
 *   - 负责任务的全面初始化，包括栈设置、优先级设置、列表项初始化等
 *   - 根据不同的配置选项（MPU、栈检查、跟踪设施等）执行不同的初始化操作
 *   - 最终通过pxPortInitialiseStack函数初始化任务栈，使其看起来像已经被中断过
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
static void prvInitialiseNewTask( TaskFunction_t pxTaskCode,
                                  const char * const pcName,
                                  const uint32_t ulStackDepth,
                                  void * const pvParameters,
                                  UBaseType_t uxPriority,
                                  TaskHandle_t * const pxCreatedTask,
                                  TCB_t *pxNewTCB,
                                  const MemoryRegion_t * const xRegions ) /*lint !e971 允许未限定的char类型用于字符串和单个字符 */
{
    StackType_t *pxTopOfStack;  /* 栈顶指针 */
    UBaseType_t x;              /* 循环计数器 */

    /* 如果使用MPU包装器，检查任务是否应该在特权模式下创建 */
    #if( portUSING_MPU_WRAPPERS == 1 )
        BaseType_t xRunPrivileged;  /* 是否以特权模式运行的标志 */
        
        /* 检查优先级中是否设置了特权位 */
        if( ( uxPriority & portPRIVILEGE_BIT ) != 0U )
        {
            xRunPrivileged = pdTRUE;  /* 设置为特权模式 */
        }
        else
        {
            xRunPrivileged = pdFALSE; /* 设置为非特权模式 */
        }
        
        /* 清除优先级中的特权位，只保留纯优先级值 */
        uxPriority &= ~portPRIVILEGE_BIT;
    #endif /* portUSING_MPU_WRAPPERS == 1 */

    /* 如果不需要memset()，则避免依赖它 */
    #if( ( configCHECK_FOR_STACK_OVERFLOW > 1 ) || ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) )
    {
        /* 用已知值填充栈，以辅助调试和栈溢出检测 */
        ( void ) memset( pxNewTCB->pxStack, ( int ) tskSTACK_FILL_BYTE, ( size_t ) ulStackDepth * sizeof( StackType_t ) );
    }
    #endif /* 栈检查或跟踪设施相关配置 */

    /* 计算栈顶地址。这取决于栈是从高内存向低内存增长（如80x86）还是相反。
       portSTACK_GROWTH用于根据需要使结果为正或负 */
    #if( portSTACK_GROWTH < 0 )  /* 栈向下增长 */
    {
        /* 计算栈顶地址（栈向下增长时，栈顶在栈缓冲区的末尾） */
        pxTopOfStack = pxNewTCB->pxStack + ( ulStackDepth - ( uint32_t ) 1 );
        
        /* 对齐栈顶地址到要求的字节对齐 */
        pxTopOfStack = ( StackType_t * ) ( ( ( portPOINTER_SIZE_TYPE ) pxTopOfStack ) & ( ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) ) ); /*lint !e923 MISRA异常。避免在指针和整数之间进行转换不实际。使用portPOINTER_SIZE_TYPE类型处理大小差异 */
        
        /* 检查计算出的栈顶地址的对齐是否正确 */
        configASSERT( ( ( ( portPOINTER_SIZE_TYPE ) pxTopOfStack & ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) == 0UL ) );
    }
    #else /* portSTACK_GROWTH */  /* 栈向上增长 */
    {
        /* 栈向上增长时，栈顶就是栈缓冲区的起始地址 */
        pxTopOfStack = pxNewTCB->pxStack;

        /* 检查栈缓冲区的对齐是否正确 */
        configASSERT( ( ( ( portPOINTER_SIZE_TYPE ) pxNewTCB->pxStack & ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) == 0UL ) );

        /* 如果执行栈检查，需要知道栈空间的另一端 */
        pxNewTCB->pxEndOfStack = pxNewTCB->pxStack + ( ulStackDepth - ( uint32_t ) 1 );
    }
    #endif /* portSTACK_GROWTH */

    /* 将任务名称存储到TCB中 */
    for( x = ( UBaseType_t ) 0; x < ( UBaseType_t ) configMAX_TASK_NAME_LEN; x++ )
    {
        /* 复制任务名称字符 */
        pxNewTCB->pcTaskName[ x ] = pcName[ x ];

        /* 如果字符串比configMAX_TASK_NAME_LEN字符短，不要复制所有configMAX_TASK_NAME_LEN个字符，
           以防字符串之后的内存不可访问（极不可能） */
        if( pcName[ x ] == 0x00 )
        {
            break;  /* 遇到字符串结束符，退出循环 */
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
        }
    }

    /* 确保名称字符串在字符串长度大于或等于configMAX_TASK_NAME_LEN的情况下被正确终止 */
    pxNewTCB->pcTaskName[ configMAX_TASK_NAME_LEN - 1 ] = '\0';

    /* 优先级用作数组索引，因此必须确保它不会太大。首先移除可能存在的特权位 */
    if( uxPriority >= ( UBaseType_t ) configMAX_PRIORITIES )
    {
        /* 如果优先级超过最大值，将其设置为最大优先级 */
        uxPriority = ( UBaseType_t ) configMAX_PRIORITIES - ( UBaseType_t ) 1U;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
    }

    /* 设置任务优先级 */
    pxNewTCB->uxPriority = uxPriority;
    
    /* 如果使用互斥量，设置基本优先级和互斥量持有计数 */
    #if ( configUSE_MUTEXES == 1 )
    {
        pxNewTCB->uxBasePriority = uxPriority;  /* 设置基本优先级 */
        pxNewTCB->uxMutexesHeld = 0;            /* 初始化互斥量持有计数为0 */
    }
    #endif /* configUSE_MUTEXES */

    /* 初始化任务的状态列表项和事件列表项 */
    vListInitialiseItem( &( pxNewTCB->xStateListItem ) );
    vListInitialiseItem( &( pxNewTCB->xEventListItem ) );

    /* 设置从ListItem_t返回到pxNewTCB的链接。这样我们可以从列表中的通用项返回到包含的TCB */
    listSET_LIST_ITEM_OWNER( &( pxNewTCB->xStateListItem ), pxNewTCB );

    /* 事件列表总是按优先级顺序排列 */
    listSET_LIST_ITEM_VALUE( &( pxNewTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) uxPriority ); /*lint !e961 MISRA异常，因为转换仅对某些端口是冗余的 */
    listSET_LIST_ITEM_OWNER( &( pxNewTCB->xEventListItem ), pxNewTCB );

    /* 如果TCB中包含关键段嵌套计数，初始化为0 */
    #if ( portCRITICAL_NESTING_IN_TCB == 1 )
    {
        pxNewTCB->uxCriticalNesting = ( UBaseType_t ) 0U;
    }
    #endif /* portCRITICAL_NESTING_IN_TCB */

    /* 如果使用应用程序任务标签，初始化为NULL */
    #if ( configUSE_APPLICATION_TASK_TAG == 1 )
    {
        pxNewTCB->pxTaskTag = NULL;
    }
    #endif /* configUSE_APPLICATION_TASK_TAG */

    /* 如果生成运行时统计信息，初始化运行时间计数器为0 */
    #if ( configGENERATE_RUN_TIME_STATS == 1 )
    {
        pxNewTCB->ulRunTimeCounter = 0UL;
    }
    #endif /* configGENERATE_RUN_TIME_STATS */

    /* 如果使用MPU包装器，存储任务的MPU设置 */
    #if ( portUSING_MPU_WRAPPERS == 1 )
    {
        vPortStoreTaskMPUSettings( &( pxNewTCB->xMPUSettings ), xRegions, pxNewTCB->pxStack, ulStackDepth );
    }
    #else
    {
        /* 避免编译器关于未引用参数的警告 */
        ( void ) xRegions;
    }
    #endif

    /* 如果配置了线程本地存储指针，初始化为NULL */
    #if( configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0 )
    {
        for( x = 0; x < ( UBaseType_t ) configNUM_THREAD_LOCAL_STORAGE_POINTERS; x++ )
        {
            pxNewTCB->pvThreadLocalStoragePointers[ x ] = NULL;
        }
    }
    #endif

    /* 如果使用任务通知，初始化通知值和状态 */
    #if ( configUSE_TASK_NOTIFICATIONS == 1 )
    {
        pxNewTCB->ulNotifiedValue = 0;                           /* 初始化通知值为0 */
        pxNewTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION;  /* 初始化通知状态为不等待通知 */
    }
    #endif

    /* 如果使用Newlib可重入，初始化Newlib可重入结构 */
    #if ( configUSE_NEWLIB_REENTRANT == 1 )
    {
        /* 初始化此任务的Newlib可重入结构 */
        _REENT_INIT_PTR( ( &( pxNewTCB->xNewLib_reent ) ) );
    }
    #endif

    /* 如果包含任务中止延迟功能，初始化延迟中止标志 */
    #if( INCLUDE_xTaskAbortDelay == 1 )
    {
        pxNewTCB->ucDelayAborted = pdFALSE;  /* 初始化延迟中止标志为FALSE */
    }
    #endif

    /* 初始化TCB栈，使其看起来像任务已经在运行但被调度器中断。
       返回地址设置为任务函数的起始地址。一旦栈被初始化，栈顶变量就会被更新 */
    #if( portUSING_MPU_WRAPPERS == 1 )
    {
        /* 使用MPU时，需要传递特权模式信息 */
        pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxTaskCode, pvParameters, xRunPrivileged );
    }
    #else /* portUSING_MPU_WRAPPERS */
    {
        /* 不使用MPU时，不需要传递特权模式信息 */
        pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxTaskCode, pvParameters );
    }
    #endif /* portUSING_MPU_WRAPPERS */

    /* 如果提供了任务句柄指针，将新任务的TCB指针赋给它 */
    if( ( void * ) pxCreatedTask != NULL )
    {
        /* 以匿名方式传递句柄。该句柄可用于更改创建任务的优先级、删除创建的任务等 */
        *pxCreatedTask = ( TaskHandle_t ) pxNewTCB;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
    }
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvAddNewTaskToReadyList
 * 功能描述：将新创建的任务添加到就绪列表，并处理相关初始化和调度逻辑
 *           此函数是任务创建过程的最后一步，负责将任务纳入调度系统管理
 * 输入参数：
 *   - pxNewTCB: 指向新任务控制块(TCB)的指针，包含任务的所有属性和状态信息
 * 输出参数：无
 * 返 回 值：无（静态函数，不对外部可见）
 * 其它说明：
 *   - 此函数是静态函数，仅在FreeRTOS内核内部使用
 *   - 在临界区内操作任务列表，防止中断访问导致的数据不一致
 *   - 处理第一个任务创建的特殊情况，初始化任务列表
 *   - 更新任务计数和任务编号
 *   - 根据任务优先级决定是否立即进行任务切换
 *   - 支持跟踪设施，为调试提供任务编号
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
static void prvAddNewTaskToReadyList( TCB_t *pxNewTCB )
{
    /* 确保在更新任务列表时中断不会访问这些列表，进入临界区 */
    taskENTER_CRITICAL();
    {
        /* 增加当前任务数量计数器 */
        uxCurrentNumberOfTasks++;
        
        /* 检查当前是否有运行中的任务 */
        if( pxCurrentTCB == NULL )
        {
            /* 没有其他任务，或者所有其他任务都处于挂起状态 - 使此任务成为当前任务 */
            pxCurrentTCB = pxNewTCB;

            /* 检查是否是创建的第一个任务 */
            if( uxCurrentNumberOfTasks == ( UBaseType_t ) 1 )
            {
                /* 这是创建的第一个任务，因此执行所需的初步初始化。
                   如果此调用失败，我们将无法恢复，但会报告失败 */
                prvInitialiseTaskLists();  /* 初始化任务列表 */
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
            }
        }
        else
        {
            /* 如果调度程序尚未运行，且此任务是迄今为止创建的优先级最高的任务，则使其成为当前任务 */
            if( xSchedulerRunning == pdFALSE )
            {
                /* 比较新任务和当前任务的优先级 */
                if( pxCurrentTCB->uxPriority <= pxNewTCB->uxPriority )
                {
                    pxCurrentTCB = pxNewTCB;  /* 新任务优先级更高，设为当前任务 */
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
            }
        }

        /* 增加全局任务编号计数器 */
        uxTaskNumber++;

        /* 如果启用跟踪设施，为TCB添加一个计数器用于跟踪 */
        #if ( configUSE_TRACE_FACILITY == 1 )
        {
            /* 在TCB中添加一个用于跟踪的计数器 */
            pxNewTCB->uxTCBNumber = uxTaskNumber;
        }
        #endif /* configUSE_TRACE_FACILITY */
        
        /* 跟踪任务创建事件 */
        traceTASK_CREATE( pxNewTCB );

        /* 将新任务添加到就绪列表 */
        prvAddTaskToReadyList( pxNewTCB );

        /* 端口特定的TCB设置（如果有的话） */
        portSETUP_TCB( pxNewTCB );
    }
    /* 退出临界区 */
    taskEXIT_CRITICAL();

    /* 检查调度程序是否已经在运行 */
    if( xSchedulerRunning != pdFALSE )
    {
        /* 如果创建的任务优先级高于当前任务，则它应该立即运行 */
        if( pxCurrentTCB->uxPriority < pxNewTCB->uxPriority )
        {
            /* 如果使用抢占式调度，执行任务让步 */
            taskYIELD_IF_USING_PREEMPTION();
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
        }
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
    }
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vTaskDelete
 * 功能描述：删除指定任务，将其从所有列表中移除并释放相关资源
 *           支持删除其他任务或任务自我删除，处理任务终止的完整流程
 * 输入参数：
 *   - xTaskToDelete: 要删除的任务句柄，如果为NULL则删除调用任务自身
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数仅在启用任务删除功能时编译（INCLUDE_vTaskDelete == 1）
 *   - 可以删除其他任务或任务自我删除
 *   - 对于自我删除的任务，需要空闲任务来清理资源
 *   - 从就绪列表、事件列表等中移除任务
 *   - 更新任务计数和任务编号
 *   - 可能触发任务调度
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
#if ( INCLUDE_vTaskDelete == 1 )

void vTaskDelete( TaskHandle_t xTaskToDelete )
{
    TCB_t *pxTCB;  /* 指向要删除的任务控制块的指针 */

    /* 进入临界区，保护任务列表操作 */
    taskENTER_CRITICAL();
    {
        /* 如果传入NULL，则表示要删除调用此函数的任务自身 */
        pxTCB = prvGetTCBFromHandle( xTaskToDelete );

        /* 从就绪列表中移除任务 */
        if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
        {
            /* 如果就绪列表变为空，重置该优先级的就绪位 */
            taskRESET_READY_PRIORITY( pxTCB->uxPriority );
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
        }

        /* 检查任务是否也在事件列表中等待 */
        if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
        {
            /* 从事件列表中移除任务 */
            ( void ) uxListRemove( &( pxTCB->xEventListItem ) );
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
        }

        /* 增加任务编号，以便内核感知调试器可以检测到任务列表需要重新生成。
           在portPRE_TASK_DELETE_HOOK()之前完成此操作，因为在Windows端口中该宏不会返回 */
        uxTaskNumber++;

        /* 检查是否要删除当前正在运行的任务 */
        if( pxTCB == pxCurrentTCB )
        {
            /* 任务正在删除自身。这不能在任务本身内完成，因为需要切换到另一个任务。
               将任务放在终止列表中。空闲任务将检查终止列表并释放调度器为已删除任务的TCB和栈分配的所有内存 */
            vListInsertEnd( &xTasksWaitingTermination, &( pxTCB->xStateListItem ) );

            /* 增加ucTasksDeleted变量，以便空闲任务知道有任务已被删除，因此应该检查xTasksWaitingTermination列表 */
            ++uxDeletedTasksWaitingCleanUp;

            /* 预删除钩子主要用于Windows模拟器，在其中执行Windows特定的清理操作，
               之后无法从此任务让步 - 因此使用xYieldPending来锁定需要上下文切换 */
            portPRE_TASK_DELETE_HOOK( pxTCB, &xYieldPending );
        }
        else
        {
            /* 减少当前任务数量 */
            --uxCurrentNumberOfTasks;
            
            /* 直接删除任务的TCB（任务控制块） */
            prvDeleteTCB( pxTCB );

            /* 重置下一个预期的解除阻塞时间，以防它引用了刚刚删除的任务 */
            prvResetNextTaskUnblockTime();
        }

        /* 跟踪任务删除事件 */
        traceTASK_DELETE( pxTCB );
    }
    /* 退出临界区 */
    taskEXIT_CRITICAL();

    /* 如果刚刚删除的是当前正在运行的任务，强制重新调度 */
    if( xSchedulerRunning != pdFALSE )
    {
        if( pxTCB == pxCurrentTCB )
        {
            /* 断言确保调度器没有被挂起 */
            configASSERT( uxSchedulerSuspended == 0 );
            
            /* 在API内部进行任务让步，触发上下文切换 */
            portYIELD_WITHIN_API();
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
        }
    }
}

#endif /* INCLUDE_vTaskDelete */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskDelayUntil == 1 ) /* 条件编译：只有当FreeRTOS配置中启用了vTaskDelayUntil功能时，此函数代码才被包含编译 */

/*******************************************************************************
 函数名称： vTaskDelayUntil
 功能描述：    实现精确的周期性任务延迟，确保任务以固定频率执行。此函数基于绝对时间而非相对时间，
               能够自动补偿任务执行时间的变化，避免累积误差，提供比vTaskDelay更高的时间精度。
 输入参数：   pxPreviousWakeTime - 指向TickType_t变量的指针，用于存储/更新上次唤醒时间。
               首次调用前必须初始化为当前时间（如xTaskGetTickCount()）。
               xTimeIncrement - 任务执行的固定周期，以时钟节拍数为单位。
 输出参数：    pxPreviousWakeTime - 函数会自动更新此变量为下一次唤醒的时间。
 返 回 值：    无
 其它说明：    此函数为FreeRTOS核心API，必须在宏INCLUDE_vTaskDelayUntil定义为1时可用。
               函数内部会处理系统时钟计数器溢出的特殊情况，确保在任何情况下都能正确计算延迟。

 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2024/06/05     V1.00          AI Assistant       创建并添加详细注释
 *******************************************************************************/

	void vTaskDelayUntil( TickType_t * const pxPreviousWakeTime, const TickType_t xTimeIncrement )
	{
	TickType_t xTimeToWake; /* 计算出的下一次任务应该唤醒的绝对时间点 */
	BaseType_t xAlreadyYielded, xShouldDelay = pdFALSE; /* xAlreadyYielded: 记录xTaskResumeAll是否已触发调度; xShouldDelay: 标记是否需要延迟 */

		/* 参数断言检查: 确保pxPreviousWakeTime指针有效 */
		configASSERT( pxPreviousWakeTime );
		/* 参数断言检查: 确保时间增量大于0 */
		configASSERT( ( xTimeIncrement > 0U ) );
		/* 断言检查: 确保调度器未被挂起 */
		configASSERT( uxSchedulerSuspended == 0 );

		/* 挂起所有任务，防止在计算过程中被其他任务或中断干扰 */
		vTaskSuspendAll();
		{
			/* 小优化: 在此代码块内，时钟节拍计数不会变化，将其保存为常量以提高效率和确保一致性 */
			const TickType_t xConstTickCount = xTickCount;

			/* 计算任务下一次希望唤醒的绝对时间: 上次唤醒时间 + 周期 */
			xTimeToWake = *pxPreviousWakeTime + xTimeIncrement;

			/* 处理时钟计数器溢出的复杂情况 */
			if( xConstTickCount < *pxPreviousWakeTime )
			{
				/* 情况1: 自上次调用此函数后，时钟节拍计数器已经溢出。
				   在这种情况下，只有当唤醒时间也已溢出，并且唤醒时间大于当前节拍计数时，才需要延迟。
				   这种情况就好像两个时间都没有溢出一样处理。 */
				if( ( xTimeToWake < *pxPreviousWakeTime ) && ( xTimeToWake > xConstTickCount ) )
				{
					xShouldDelay = pdTRUE; /* 需要延迟 */
				}
				else
				{
					/* 代码覆盖测试标记: 不需要延迟的情况 */
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				/* 情况2: 时钟节拍计数器没有溢出。
				   在这种情况下，如果唤醒时间已溢出，和/或当前节拍时间小于唤醒时间，则需要延迟。 */
				if( ( xTimeToWake < *pxPreviousWakeTime ) || ( xTimeToWake > xConstTickCount ) )
				{
					xShouldDelay = pdTRUE; /* 需要延迟 */
				}
				else
				{
					/* 代码覆盖测试标记: 不需要延迟的情况 */
					mtCOVERAGE_TEST_MARKER();
				}
			}

			/* 更新唤醒时间，为下一次调用做好准备 */
			*pxPreviousWakeTime = xTimeToWake;

			/* 根据判断决定是否需要进行延迟 */
			if( xShouldDelay != pdFALSE )
			{
				/* 调用trace宏，输出任务延迟调试信息 */
				traceTASK_DELAY_UNTIL( xTimeToWake );

				/* prvAddCurrentTaskToDelayedList()需要的是相对阻塞时间，而不是绝对唤醒时间，
				   因此需要减去当前节拍计数，计算出需要延迟的节拍数 */
				prvAddCurrentTaskToDelayedList( xTimeToWake - xConstTickCount, pdFALSE );
			}
			else
			{
				/* 代码覆盖测试标记: 不需要延迟的情况 */
				mtCOVERAGE_TEST_MARKER();
			}
		}
		/* 恢复所有任务，并获取返回值（是否已触发任务调度） */
		xAlreadyYielded = xTaskResumeAll();

		/* 如果xTaskResumeAll没有已经触发调度，并且我们可能已经将自己置为睡眠状态，则强制进行一次调度 */
		if( xAlreadyYielded == pdFALSE )
		{
			/* 在API内部发起一次任务调度 */
			portYIELD_WITHIN_API();
		}
		else
		{
			/* 代码覆盖测试标记: 已经触发过调度的情况 */
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* INCLUDE_vTaskDelayUntil */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskDelay == 1 ) /* 条件编译：只有当FreeRTOS配置中启用了vTaskDelay功能时，此函数代码才被包含编译 */

/*******************************************************************************
 函数名称： vTaskDelay
 功能描述：    将当前任务延迟（阻塞）指定的时钟节拍数。这是一种相对延迟，从调用vTaskDelay的时刻开始计算延迟时间。
               任务在延迟期间处于阻塞状态，不会占用CPU时间，允许其他任务运行。
 输入参数：   xTicksToDelay - 要延迟的时钟节拍数。如果为0，则强制进行一次任务切换（即让出CPU）。
 输出参数：    无
 返 回 值：    无
 其它说明：    此函数为FreeRTOS核心API，必须在宏INCLUDE_vTaskDelay定义为1时可用。
               与vTaskDelayUntil不同，此函数提供的是相对延迟，不适合需要精确周期的应用。

 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2024/06/05     V1.00          AI Assistant       创建并添加详细注释
 *******************************************************************************/

	void vTaskDelay( const TickType_t xTicksToDelay )
	{
	BaseType_t xAlreadyYielded = pdFALSE; /* 标记xTaskResumeAll是否已经触发了任务调度 */

		/* 如果延迟时间为0，只需强制进行一次重新调度（任务切换） */
		if( xTicksToDelay > ( TickType_t ) 0U )
		{
			/* 断言检查：确保调度器没有被挂起 */
			configASSERT( uxSchedulerSuspended == 0 );
			/* 挂起所有任务，防止在操作过程中被其他任务或中断干扰 */
			vTaskSuspendAll();
			{
				/* 调用trace宏，输出任务延迟调试信息 */
				traceTASK_DELAY();

				/* 当调度器被挂起时从事件列表中移除的任务，在调度器恢复之前不会被放入就绪列表或从阻塞列表中移除。
				
				当前执行的任务不可能在事件列表中，因为它是当前正在执行的任务。 */
				/* 将当前任务添加到延迟列表，指定延迟的节拍数 */
				prvAddCurrentTaskToDelayedList( xTicksToDelay, pdFALSE );
			}
			/* 恢复所有任务，并获取返回值（是否已触发任务调度） */
			xAlreadyYielded = xTaskResumeAll();
		}
		else
		{
			/* 代码覆盖测试标记：延迟时间为0的情况 */
			mtCOVERAGE_TEST_MARKER();
		}

		/* 如果xTaskResumeAll没有已经触发调度，并且我们可能已经将自己置为睡眠状态，则强制进行一次调度 */
		if( xAlreadyYielded == pdFALSE )
		{
			/* 在API内部发起一次任务调度 */
			portYIELD_WITHIN_API();
		}
		else
		{
			/* 代码覆盖测试标记：已经触发过调度的情况 */
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* INCLUDE_vTaskDelay */
/*-----------------------------------------------------------*/

#if( ( INCLUDE_eTaskGetState == 1 ) || ( configUSE_TRACE_FACILITY == 1 ) )

/*******************************************************************************
 * 函数名称: eTaskGetState
 * 功能描述: 获取指定任务的状态信息。该函数通过查询任务所在的状态列表来确定任务当前状态，
 *           包括运行态、就绪态、阻塞态、挂起态或已删除状态。
 * 输入参数: xTask - 要查询状态的任务句柄（任务控制块指针）
 * 输出参数: 无
 * 返 回 值: eTaskState枚举类型，表示任务的当前状态：
 *           eRunning   - 任务正在运行
 *           eReady     - 任务处于就绪状态
 *           eBlocked   - 任务处于阻塞状态
 *           eSuspended - 任务处于挂起状态
 *           eDeleted   - 任务已被删除
 * 其它说明: 1.此函数需要在FreeRTOS配置中启用INCLUDE_eTaskGetState或configUSE_TRACE_FACILITY
 *          2.函数内部使用临界区保护状态查询操作
 *          3.如果查询的是当前正在运行的任务，直接返回eRunning
 *          4.任务状态通过检查任务所在链表确定
 * 修改日期      版本号          修改人            修改内容
 * ----------------------------------------------------------------------------
 * 2024/06/02     V1.00          ChatGPT            创建并添加详细注释
 *******************************************************************************/
eTaskState eTaskGetState( TaskHandle_t xTask )
{
eTaskState eReturn;                         /* 定义返回值变量 */
List_t *pxStateList;                        /* 指向任务所在状态链表的指针 */
const TCB_t * const pxTCB = ( TCB_t * ) xTask;  /* 将任务句柄转换为任务控制块指针 */

    configASSERT( pxTCB );                  /* 验证任务控制块指针有效性 */

    if( pxTCB == pxCurrentTCB )             /* 检查是否为当前运行任务 */
    {
        /* 任务正在查询自身状态，直接返回运行状态 */
        eReturn = eRunning;
    }
    else
    {
        taskENTER_CRITICAL();               /* 进入临界区保护状态查询 */
        {
            /* 获取任务状态列表项所在的链表 */
            pxStateList = ( List_t * ) listLIST_ITEM_CONTAINER( &( pxTCB->xStateListItem ) );
        }
        taskEXIT_CRITICAL();                /* 退出临界区 */

        if( ( pxStateList == pxDelayedTaskList ) || ( pxStateList == pxOverflowDelayedTaskList ) )
        {
            /* 任务处于延迟阻塞或溢出延迟阻塞列表中 */
            eReturn = eBlocked;
        }

        #if ( INCLUDE_vTaskSuspend == 1 )   /* 如果启用了任务挂起功能 */
            else if( pxStateList == &xSuspendedTaskList )  /* 任务处于挂起任务列表 */
            {
                /* 检查任务是真正挂起还是无限期阻塞 */
                if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) == NULL )
                {
                    /* 事件列表项不在任何链表中，表示任务真正挂起 */
                    eReturn = eSuspended;
                }
                else
                {
                    /* 事件列表项在链表中，表示任务处于阻塞状态 */
                    eReturn = eBlocked;
                }
            }
        #endif  /* INCLUDE_vTaskSuspend */

        #if ( INCLUDE_vTaskDelete == 1 )    /* 如果启用了任务删除功能 */
            else if( ( pxStateList == &xTasksWaitingTermination ) || ( pxStateList == NULL ) )
            {
                /* 任务处于待终止列表或未在任何列表中（已删除状态） */
                eReturn = eDeleted;
            }
        #endif  /* INCLUDE_vTaskDelete */

        else /*lint !e525 忽略缩进警告，使预处理结构更清晰 */
        {
            /* 任务不处于上述任何状态，则处于就绪状态（包括待就绪状态） */
            eReturn = eReady;
        }
    }

    return eReturn;  /* 返回任务状态查询结果 */
} /*lint !e818 xTask不能为const指针，因为它是typedef定义 */

#endif /* INCLUDE_eTaskGetState或configUSE_TRACE_FACILITY启用检查 */
/*-----------------------------------------------------------*/

#if ( INCLUDE_uxTaskPriorityGet == 1 ) /* 条件编译：只有当FreeRTOS配置中启用了uxTaskPriorityGet功能时，此函数代码才被包含编译 */

/*******************************************************************************
 函数名称： uxTaskPriorityGet
 功能描述：    获取指定任务的当前优先级。可以查询任意任务的优先级，包括任务自身。
 输入参数：   xTask - 要查询优先级的任务句柄。如果传入NULL，则表示查询调用任务自身的优先级。
 输出参数：    无
 返 回 值：    UBaseType_t类型 - 返回指定任务的当前优先级值。
 其它说明：    此函数为FreeRTOS核心API，必须在宏INCLUDE_uxTaskPriorityGet定义为1时可用。
               函数内部使用临界区保护，确保在读取任务优先级时不会被其他任务或中断干扰。

 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2024/06/05     V1.00          AI Assistant       创建并添加详细注释
 *******************************************************************************/

	UBaseType_t uxTaskPriorityGet( TaskHandle_t xTask )
	{
	TCB_t *pxTCB; /* 指向任务控制块(TCB)的指针 */
	UBaseType_t uxReturn; /* 存储要返回的优先级值 */

		/* 进入临界区，保护对任务控制块的访问 */
		taskENTER_CRITICAL();
		{
			/* 如果传入NULL，则表示查询调用uxTaskPriorityGet()的任务自身的优先级 */
			pxTCB = prvGetTCBFromHandle( xTask ); /* 通过任务句柄获取对应的任务控制块 */
			uxReturn = pxTCB->uxPriority; /* 从任务控制块中读取优先级字段 */
		}
		/* 退出临界区 */
		taskEXIT_CRITICAL();

		/* 返回获取到的优先级值 */
		return uxReturn;
	}

#endif /* INCLUDE_uxTaskPriorityGet */
/*-----------------------------------------------------------*/

#if ( INCLUDE_uxTaskPriorityGet == 1 ) /* 条件编译：只有当FreeRTOS配置中启用了uxTaskPriorityGet功能时，此函数代码才被包含编译 */

/*******************************************************************************
 函数名称： uxTaskPriorityGetFromISR
 功能描述：    从中断服务例程(ISR)中获取指定任务的当前优先级。这是uxTaskPriorityGet()的中断安全版本。
               可以查询任意任务的优先级，专为在中断上下文中调用而设计。
 输入参数：   xTask - 要查询优先级的任务句柄。如果传入NULL，则表示查询当前正在运行的任务的优先级。
 输出参数：    无
 返 回 值：    UBaseType_t类型 - 返回指定任务的当前优先级值。
 其它说明：    此函数为FreeRTOS中断安全API，只能在中断服务例程中调用。
               函数使用中断屏蔽而非任务临界区来保护关键代码段，确保在中断上下文中的安全访问。

 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2024/06/05     V1.00          AI Assistant       创建并添加详细注释
 *******************************************************************************/

	UBaseType_t uxTaskPriorityGetFromISR( TaskHandle_t xTask )
	{
	TCB_t *pxTCB; /* 指向任务控制块(TCB)的指针 */
	UBaseType_t uxReturn, uxSavedInterruptState; /* uxReturn: 存储要返回的优先级值; uxSavedInterruptState: 保存中断状态 */

		/* 检查中断优先级有效性：
		   FreeRTOS支持中断嵌套的端口有最大系统调用中断优先级的概念。
		   高于此优先级的中断即使在内核临界区也保持启用，但不能调用任何FreeRTOS API函数。
		   只有以FromISR结尾的FreeRTOS函数可以从优先级等于或低于最大系统调用中断优先级的中断中调用。
		   此宏会验证当前中断优先级是否有效，如果无效则会触发断言失败。 */
		portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

		/* 保存当前中断状态并屏蔽中断（中断安全版本），保护后续关键代码段 */
		uxSavedInterruptState = portSET_INTERRUPT_MASK_FROM_ISR();
		{
			/* 如果传入NULL，则表示查询当前正在运行的任务的优先级 */
			pxTCB = prvGetTCBFromHandle( xTask ); /* 通过任务句柄获取对应的任务控制块 */
			uxReturn = pxTCB->uxPriority; /* 从任务控制块中读取优先级字段 */
		}
		/* 恢复之前保存的中断状态 */
		portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptState );

		/* 返回获取到的优先级值 */
		return uxReturn;
	}

#endif /* INCLUDE_uxTaskPriorityGet */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskPrioritySet == 1 ) /* 条件编译：只有当FreeRTOS配置中启用了vTaskPrioritySet功能时，此函数代码才被包含编译 */

/*******************************************************************************
 函数名称： vTaskPrioritySet
 功能描述：    设置指定任务的优先级。可以动态改变任务的优先级，包括任务自身。
               函数会根据新的优先级重新调整任务在就绪列表中的位置，并在必要时触发任务调度。
 输入参数：   xTask - 要设置优先级的任务句柄。如果传入NULL，则表示设置调用任务自身的优先级。
              uxNewPriority - 新的优先级值，必须小于configMAX_PRIORITIES。
 输出参数：    无
 返 回 值：    无
 其它说明：    此函数为FreeRTOS核心API，必须在宏INCLUDE_vTaskPrioritySet定义为1时可用。
               函数内部使用临界区保护，确保在修改任务优先级时不会被其他任务或中断干扰。
               当使用互斥量时，函数会正确处理优先级继承机制。

 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2024/06/05     V1.00          AI Assistant       创建并添加详细注释
 *******************************************************************************/

	void vTaskPrioritySet( TaskHandle_t xTask, UBaseType_t uxNewPriority )
	{
	TCB_t *pxTCB; /* 指向任务控制块(TCB)的指针 */
	UBaseType_t uxCurrentBasePriority, uxPriorityUsedOnEntry; /* uxCurrentBasePriority: 当前基础优先级; uxPriorityUsedOnEntry: 进入时的优先级 */
	BaseType_t xYieldRequired = pdFALSE; /* 标记是否需要触发任务调度 */

		/* 断言检查：确保新优先级值有效（小于配置的最大优先级） */
		configASSERT( ( uxNewPriority < configMAX_PRIORITIES ) );

		/* 确保新优先级有效，如果超出范围则设置为最大允许值 */
		if( uxNewPriority >= ( UBaseType_t ) configMAX_PRIORITIES )
		{
			uxNewPriority = ( UBaseType_t ) configMAX_PRIORITIES - ( UBaseType_t ) 1U;
		}
		else
		{
			/* 代码覆盖测试标记：优先级值有效的情况 */
			mtCOVERAGE_TEST_MARKER();
		}

		/* 进入临界区，保护对任务控制块的访问 */
		taskENTER_CRITICAL();
		{
			/* 如果传入NULL，则表示要改变调用任务的优先级 */
			pxTCB = prvGetTCBFromHandle( xTask ); /* 通过任务句柄获取对应的任务控制块 */

			/* 调用trace宏，输出任务优先级设置调试信息 */
			traceTASK_PRIORITY_SET( pxTCB, uxNewPriority );

			/* 根据是否使用互斥量，获取当前的基础优先级 */
			#if ( configUSE_MUTEXES == 1 )
			{
				uxCurrentBasePriority = pxTCB->uxBasePriority; /* 获取基础优先级（考虑优先级继承） */
			}
			#else
			{
				uxCurrentBasePriority = pxTCB->uxPriority; /* 直接获取任务优先级 */
			}
			#endif

			/* 检查优先级是否确实需要改变 */
			if( uxCurrentBasePriority != uxNewPriority )
			{
				/* 优先级改变可能会导致一个比调用任务优先级更高的任务变为就绪状态 */

				/* 处理优先级提高的情况 */
				if( uxNewPriority > uxCurrentBasePriority )
				{
					/* 如果修改的不是当前运行的任务 */
					if( pxTCB != pxCurrentTCB )
					{
						/* 正在提高一个非当前运行任务的优先级。
						   检查这个优先级是否提高到等于或高于当前运行任务的优先级？ */
						if( uxNewPriority >= pxCurrentTCB->uxPriority )
						{
							/* 需要触发任务调度，因为可能有更高优先级的任务就绪 */
							xYieldRequired = pdTRUE;
						}
						else
						{
							/* 代码覆盖测试标记：新优先级仍低于当前运行任务的情况 */
							mtCOVERAGE_TEST_MARKER();
						}
					}
					else
					{
						/* 正在提高当前运行任务的优先级，但当前运行任务已经是最高优先级任务，
						   所以不需要触发调度 */
					}
				}
				/* 处理优先级降低的情况 */
				else if( pxTCB == pxCurrentTCB )
				{
					/* 降低当前运行任务的优先级意味着现在可能有另一个更高优先级的任务就绪可执行 */
					xYieldRequired = pdTRUE;
				}
				else
				{
					/* 降低任何其他任务的优先级不需要触发调度，因为运行任务的优先级
					   必须高于被修改任务的新优先级 */
				}

				/* 在修改uxPriority成员之前，记住任务可能被引用的就绪列表，
				   以便taskRESET_READY_PRIORITY()宏能正确运行 */
				uxPriorityUsedOnEntry = pxTCB->uxPriority;

				/* 根据是否使用互斥量，设置优先级 */
				#if ( configUSE_MUTEXES == 1 )
				{
					/* 只有当任务当前没有使用继承优先级时，才更改正在使用的优先级 */
					if( pxTCB->uxBasePriority == pxTCB->uxPriority )
					{
						pxTCB->uxPriority = uxNewPriority; /* 设置当前优先级 */
					}
					else
					{
						/* 代码覆盖测试标记：任务正在使用继承优先级的情况 */
						mtCOVERAGE_TEST_MARKER();
					}

					/* 无论何种情况，基础优先级都要设置 */
					pxTCB->uxBasePriority = uxNewPriority; /* 设置基础优先级 */
				}
				#else
				{
					/* 不使用互斥量时，直接设置优先级 */
					pxTCB->uxPriority = uxNewPriority;
				}
				#endif

				/* 只有当事件列表项的值没有被用于其他用途时，才重置它 */
				if( ( listGET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ) ) & taskEVENT_LIST_ITEM_VALUE_IN_USE ) == 0UL )
				{
					/* 设置事件列表项的值，用于在事件列表中按优先级排序 */
					listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), ( ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) uxNewPriority ) ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
				}
				else
				{
					/* 代码覆盖测试标记：事件列表项的值正在被使用的情况 */
					mtCOVERAGE_TEST_MARKER();
				}

				/* 如果任务在阻塞或挂起列表中，我们只需要更改其优先级变量。
				   但是，如果任务在就绪列表中，需要将其移除并放入与其新优先级对应的列表中 */
				if( listIS_CONTAINED_WITHIN( &( pxReadyTasksLists[ uxPriorityUsedOnEntry ] ), &( pxTCB->xStateListItem ) ) != pdFALSE )
				{
					/* 任务当前在其就绪列表中 - 在将其添加到新的就绪列表之前先移除。
					   由于我们在临界区内，即使调度器被挂起也可以执行此操作 */
					if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
					{
						/* 已知任务在其就绪列表中，因此无需再次检查，
						   可以直接调用端口级别的重置宏 */
						portRESET_READY_PRIORITY( uxPriorityUsedOnEntry, uxTopReadyPriority );
					}
					else
					{
						/* 代码覆盖测试标记：列表移除后仍有其他任务的情况 */
						mtCOVERAGE_TEST_MARKER();
					}
					/* 将任务添加到新的就绪列表中 */
					prvAddTaskToReadyList( pxTCB );
				}
				else
				{
					/* 代码覆盖测试标记：任务不在就绪列表中的情况 */
					mtCOVERAGE_TEST_MARKER();
				}

				/* 如果需要，触发任务调度 */
				if( xYieldRequired != pdFALSE )
				{
					taskYIELD_IF_USING_PREEMPTION();
				}
				else
				{
					/* 代码覆盖测试标记：不需要调度的情况 */
					mtCOVERAGE_TEST_MARKER();
				}

				/* 当端口优化的任务选择未使用时，移除关于未使用变量的编译器警告 */
				( void ) uxPriorityUsedOnEntry;
			}
		}
		/* 退出临界区 */
		taskEXIT_CRITICAL();
	}

#endif /* INCLUDE_vTaskPrioritySet */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskSuspend == 1 ) /* 条件编译：只有当FreeRTOS配置中启用了任务挂起功能时，此函数代码才被包含编译 */

/*******************************************************************************
 函数名称： vTaskSuspend
 功能描述：    将一个任务置于挂起状态。被挂起的任务将不再被执行，直到通过`vTaskResume()`或`xTaskResumeFromISR()`将其恢复。
               此函数可以挂起任意任务，包括当前正在运行的任务（通过传入NULL参数）。
 输入参数：   xTaskToSuspend - 要挂起的任务的句柄（TaskHandle_t类型）。如果传入NULL，则表示挂起当前正在运行的任务本身。
 输出参数：    无
 返 回 值：    无
 其它说明：    此函数为FreeRTOS核心API，必须在宏`INCLUDE_vTaskSuspend`定义为1时可用。
               函数内部包含临界区操作和可能的任务调度。

 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2024/06/05     V1.00          AI Assistant       创建并添加详细注释
 *******************************************************************************/

	void vTaskSuspend( TaskHandle_t xTaskToSuspend )
	{
	TCB_t *pxTCB; /* 声明一个指针，用于指向要挂起任务的任务控制块(TCB) */

		/* 进入临界区，保护后续操作不被中断 */
		taskENTER_CRITICAL();
		{
			/* 通过任务句柄获取对应的任务控制块(TCB)地址。
			   如果传入的参数是NULL，则内部函数prvGetTCBFromHandle会返回当前运行任务的TCB */
			pxTCB = prvGetTCBFromHandle( xTaskToSuspend );

			/* 调用 trace 宏，用于输出任务挂起调试信息（如果启了跟踪功能） */
			traceTASK_SUSPEND( pxTCB );

			/* 将任务从就绪列表或延迟列表中移除。
			   uxListRemove 返回值表示原列表剩余任务数，如果为0，说明该优先级就绪列表中已无其他任务 */
			if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
			{
				/* 如果该优先级的就绪列表为空，则重置调度器中的就绪优先级位图，
				   清除该优先级对应的位，表示此优先级没有就绪任务 */
				taskRESET_READY_PRIORITY( pxTCB->uxPriority );
			}
			else
			{
				/* 代码覆盖测试标记，表示 else 分支存在，但通常无实际功能代码 */
				mtCOVERAGE_TEST_MARKER();
			}

			/* 检查任务是否也在事件列表中等待（例如等待队列、信号量等） */
			if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
			{
				/* 如果任务正在等待事件，则将其从事件列表中移除 */
				( void ) uxListRemove( &( pxTCB->xEventListItem ) );
			}
			else
			{
				/* 代码覆盖测试标记 */
				mtCOVERAGE_TEST_MARKER();
			}

			/* 将任务的状态列表项插入到已挂起任务列表（xSuspendedTaskList）的末尾 */
			vListInsertEnd( &xSuspendedTaskList, &( pxTCB->xStateListItem ) );
		}
		/* 退出临界区 */
		taskEXIT_CRITICAL();

		/* 检查调度器是否已经启动运行 */
		if( xSchedulerRunning != pdFALSE )
		{
			/* 由于一个任务被挂起，它可能原本是下一个要解除阻塞的任务。
			   因此需要重置下一个任务解除阻塞时间，重新计算下一个最快到期的时间 */
			taskENTER_CRITICAL();
			{
				prvResetNextTaskUnblockTime();
			}
			taskEXIT_CRITICAL();
		}
		else
		{
			/* 代码覆盖测试标记（调度器未运行的情况） */
			mtCOVERAGE_TEST_MARKER();
		}

		/* 判断被挂起的任务是否是当前正在运行的任务 */
		if( pxTCB == pxCurrentTCB )
		{
			/* 如果挂起的确实是当前任务 */
			if( xSchedulerRunning != pdFALSE )
			{
				/* 确保在挂起当前任务时，调度器没有被挂起（处于正常状态） */
				configASSERT( uxSchedulerSuspended == 0 );
				/* 主动发起一次任务调度（portYIELD_WITHIN_API），
				   因为当前任务即将被挂起，CPU需要切换到其他就绪任务 */
				portYIELD_WITHIN_API();
			}
			else
			{
				/* 如果调度器还未启动，但当前任务TCB指向的任务被挂起了，
				   需要调整pxCurrentTCB指向一个不同的任务 */

				/* 检查已挂起任务列表中的任务数是否等于总任务数 */
				if( listCURRENT_LIST_LENGTH( &xSuspendedTaskList ) == uxCurrentNumberOfTasks )
				{
					/* 如果所有任务都被挂起了，没有其他任务就绪，
					   则将pxCurrentTCB设置为NULL。
					   这样当下一个任务创建时，pxCurrentTCB会自动指向它 */
					pxCurrentTCB = NULL;
				}
				else
				{
					/* 如果还有其他任务就绪，则执行任务切换，选择下一个要运行的任务 */
					vTaskSwitchContext();
				}
			}
		}
		else
		{
			/* 代码覆盖测试标记（挂起的不是当前任务的情况） */
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* INCLUDE_vTaskSuspend */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskSuspend == 1 ) /* 条件编译：只有当FreeRTOS配置中启用了任务挂起功能时，此函数代码才被包含编译 */

/*******************************************************************************
 函数名称： prvTaskIsTaskSuspended
 功能描述：    静态辅助函数，用于检查指定任务是否真正处于挂起状态。
               此函数会验证任务是否在挂起列表中且未被ISR恢复，并确保任务不是因为无超时阻塞而处于挂起列表。
 输入参数：   xTask - 要检查的任务句柄（TaskHandle_t类型），不能为NULL且不能是调用任务自身
 输出参数：    无
 返 回 值：    BaseType_t类型 
               - pdTRUE: 任务确实处于挂起状态
               - pdFALSE: 任务不处于挂起状态
 其它说明：    此函数为FreeRTOS内部静态函数，仅在挂起功能启用时可用。
               必须在临界区内调用，因为它会访问xPendingReadyList等共享资源。
               使用configASSERT确保传入的任务句柄不为NULL。

 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2024/06/05     V1.00          AI Assistant       创建并添加详细注释
 *******************************************************************************/

	static BaseType_t prvTaskIsTaskSuspended( const TaskHandle_t xTask )
	{
	BaseType_t xReturn = pdFALSE; /* 初始化返回值为pdFALSE（表示任务未挂起） */
	const TCB_t * const pxTCB = ( TCB_t * ) xTask; /* 将任务句柄转换为任务控制块(TCB)指针 */

		/* 此函数会访问xPendingReadyList等共享资源，因此必须从临界区内调用 */
		/* 注释：调用此函数前应确保已进入临界区 */

		/* 检查传入的任务句柄不能为NULL，因为检查调用任务自身是否挂起没有意义 */
		configASSERT( xTask );

		/* 检查要恢复的任务是否确实在挂起任务列表(xSuspendedTaskList)中 */
		if( listIS_CONTAINED_WITHIN( &xSuspendedTaskList, &( pxTCB->xStateListItem ) ) != pdFALSE )
		{
			/* 进一步检查：任务是否已经从中断服务程序(ISR)中被恢复（即是否在待处理就绪列表xPendingReadyList中） */
			if( listIS_CONTAINED_WITHIN( &xPendingReadyList, &( pxTCB->xEventListItem ) ) == pdFALSE )
			{
				/* 再次检查：任务是否因为真正处于挂起状态而在挂起列表中，
				   而不是因为无超时阻塞（事件列表项容器为NULL表示真正挂起） */
				if( listIS_CONTAINED_WITHIN( NULL, &( pxTCB->xEventListItem ) ) != pdFALSE )
				{
					/* 通过所有检查，确认任务确实处于挂起状态 */
					xReturn = pdTRUE;
				}
				else
				{
					/* 代码覆盖测试标记：任务在挂起列表中但事件列表项有容器，
					   表示可能是因为无超时阻塞而非真正挂起 */
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				/* 代码覆盖测试标记：任务已在ISR中被恢复（位于待处理就绪列表中） */
				mtCOVERAGE_TEST_MARKER();
			}
		}
		else
		{
			/* 代码覆盖测试标记：任务不在挂起列表中 */
			mtCOVERAGE_TEST_MARKER();
		}

		/* 返回检查结果 */
		return xReturn;
	} /*lint !e818 xTask cannot be a pointer to const because it is a typedef. */
    /* Lint注释：忽略lint警告，xTask不能是指向const的指针，因为它是一个类型定义 */

#endif /* INCLUDE_vTaskSuspend */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskSuspend == 1 ) /* 条件编译：只有当FreeRTOS配置中启用了任务挂起功能时，此函数代码才被包含编译 */

/*******************************************************************************
 函数名称： vTaskResume
 功能描述：    恢复一个被挂起的任务，使其重新进入就绪状态，能够被调度器选择执行。
               此函数不能用于恢复当前正在运行的任务本身，也不能恢复因延迟、阻塞等原因而暂停的任务。
 输入参数：   xTaskToResume - 要恢复的任务句柄（TaskHandle_t类型），不能为NULL且不能指向当前任务
 输出参数：    无
 返 回 值：    无
 其它说明：    此函数为FreeRTOS核心API，必须在宏`INCLUDE_vTaskSuspend`定义为1时可用。
               函数内部会检查任务是否真正处于挂起状态，并处理可能的优先级抢占。
               如果恢复的任务优先级高于当前任务，可能会触发任务切换。

 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2024/06/05     V1.00          AI Assistant       创建并添加详细注释
 *******************************************************************************/

	void vTaskResume( TaskHandle_t xTaskToResume )
	{
	/* 将任务句柄转换为任务控制块(TCB)指针，使用const确保指针指向的内容不被修改 */
	TCB_t * const pxTCB = ( TCB_t * ) xTaskToResume;

		/* 断言检查：恢复调用任务自身是没有意义的，因此传入参数不能为NULL */
		configASSERT( xTaskToResume );

		/* 参数检查：任务句柄不能为NULL，且不能是当前正在执行的任务
		   因为无法恢复当前正在运行的任务（它本身就不是挂起状态） */
		if( ( pxTCB != NULL ) && ( pxTCB != pxCurrentTCB ) )
		{
			/* 进入临界区，保护对任务列表的访问 */
			taskENTER_CRITICAL();
			{
				/* 使用内部函数检查任务是否真正处于挂起状态 */
				if( prvTaskIsTaskSuspended( pxTCB ) != pdFALSE )
				{
					/* 调用 trace 宏，用于输出任务恢复调试信息（如果启用了跟踪功能） */
					traceTASK_RESUME( pxTCB );

					/* 由于在临界区内，即使调度器被挂起，我们也可以安全地访问就绪列表 */
					
					/* 将任务从挂起列表中移除 */
					( void ) uxListRemove(  &( pxTCB->xStateListItem ) );
					/* 将任务添加到就绪列表中，使其能够被调度器选择执行 */
					prvAddTaskToReadyList( pxTCB );

					/* 检查恢复的任务优先级是否高于或等于当前运行任务的优先级 */
					if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
					{
						/* 恢复的任务优先级不低于当前任务，可能需要进行任务切换 */
						
						/* 此Yield操作可能不会立即使刚恢复的任务运行，
						   但会使任务列表处于正确状态，为下一次Yield做好准备 */
						taskYIELD_IF_USING_PREEMPTION();
					}
					else
					{
						/* 代码覆盖测试标记：恢复的任务优先级低于当前任务，不需要立即切换 */
						mtCOVERAGE_TEST_MARKER();
					}
				}
				else
				{
					/* 代码覆盖测试标记：任务不处于挂起状态（可能是已经恢复或从未挂起） */
					mtCOVERAGE_TEST_MARKER();
				}
			}
			/* 退出临界区 */
			taskEXIT_CRITICAL();
		}
		else
		{
			/* 代码覆盖测试标记：参数无效（为NULL或是当前任务） */
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* INCLUDE_vTaskSuspend */

/*-----------------------------------------------------------*/

#if ( ( INCLUDE_xTaskResumeFromISR == 1 ) && ( INCLUDE_vTaskSuspend == 1 ) )
/* 条件编译：只有当FreeRTOS配置中同时启用了从中断恢复任务功能和任务挂起功能时，此函数代码才被包含编译 */

/*******************************************************************************
 函数名称： xTaskResumeFromISR
 功能描述：    从中断服务例程(ISR)中恢复一个被挂起的任务，是vTaskResume()的中断安全版本。
               此函数可以在中断上下文调用，用于将挂起的任务重新置为就绪状态。
 输入参数：   xTaskToResume - 要恢复的任务句柄（TaskHandle_t类型），不能为NULL
 输出参数：    无
 返 回 值：    BaseType_t类型 
               - pdTRUE: 需要在中断退出前执行上下文切换
               - pdFALSE: 不需要在中断退出前执行上下文切换
 其它说明：    此函数为FreeRTOS中断安全API，只能在中断服务例程中调用。
               如果恢复的任务优先级高于当前任务，可能需要手动请求上下文切换。
               使用此函数前需确保配置中INCLUDE_xTaskResumeFromISR和INCLUDE_vTaskSuspend均为1。

 修改日期      版本号          修改人            修改内容
 -----------------------------------------------------------------------------
 2024/06/05     V1.00          AI Assistant       创建并添加详细注释
 *******************************************************************************/

	BaseType_t xTaskResumeFromISR( TaskHandle_t xTaskToResume )
	{
	BaseType_t xYieldRequired = pdFALSE; /* 初始化返回值，默认不需要上下文切换 */
	TCB_t * const pxTCB = ( TCB_t * ) xTaskToResume; /* 将任务句柄转换为任务控制块(TCB)指针 */
	UBaseType_t uxSavedInterruptStatus; /* 用于保存中断状态，以便恢复 */

		/* 断言检查：传入的任务句柄不能为NULL */
		configASSERT( xTaskToResume );

		/* 检查中断优先级有效性：
		   FreeRTOS支持中断嵌套的端口有最大系统调用中断优先级的概念。
		   高于此优先级的中断即使在内核临界区也保持启用，但不能调用任何FreeRTOS API函数。
		   只有以FromISR结尾的FreeRTOS函数可以从优先级等于或低于最大系统调用中断优先级的中断中调用。
		   此宏会验证当前中断优先级是否有效，如果无效则会触发断言失败。 */
		portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

		/* 保存当前中断状态并禁用中断（中断安全版本），保护后续操作 */
		uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
		{
			/* 使用内部函数检查任务是否真正处于挂起状态 */
			if( prvTaskIsTaskSuspended( pxTCB ) != pdFALSE )
			{
				/* 调用 trace 宏，用于输出从中断恢复任务的调试信息 */
				traceTASK_RESUME_FROM_ISR( pxTCB );

				/* 检查就绪列表是否可以被访问（调度器是否未被挂起） */
				if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
				{
					/* 就绪列表可以访问，直接将任务从挂起列表移动到就绪列表 */
					
					/* 检查恢复的任务优先级是否高于或等于当前运行任务的优先级 */
					if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
					{
						/* 恢复的任务优先级不低于当前任务，需要在中断退出前请求上下文切换 */
						xYieldRequired = pdTRUE;
					}
					else
					{
						/* 代码覆盖测试标记：恢复的任务优先级低于当前任务，不需要上下文切换 */
						mtCOVERAGE_TEST_MARKER();
					}

					/* 将任务从挂起列表中移除 */
					( void ) uxListRemove( &( pxTCB->xStateListItem ) );
					/* 将任务添加到就绪列表中，使其能够被调度器选择执行 */
					prvAddTaskToReadyList( pxTCB );
				}
				else
				{
					/* 延迟列表或就绪列表当前无法访问（调度器被挂起），
					   因此将任务放在待处理就绪列表中，直到调度器恢复 */
					vListInsertEnd( &( xPendingReadyList ), &( pxTCB->xEventListItem ) );
				}
			}
			else
			{
				/* 代码覆盖测试标记：任务不处于挂起状态（可能是已经恢复或从未挂起） */
				mtCOVERAGE_TEST_MARKER();
			}
		}
		/* 恢复之前保存的中断状态 */
		portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

		/* 返回是否需要上下文切换的标志 */
		return xYieldRequired;
	}

#endif /* ( ( INCLUDE_xTaskResumeFromISR == 1 ) && ( INCLUDE_vTaskSuspend == 1 ) ) */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称: vTaskStartScheduler
 * 功能描述: 启动FreeRTOS任务调度器，初始化系统内核组件，创建空闲任务和定时器任务（可选），
 *           启动硬件定时器并开始任务调度。该函数不会返回，除非调用xTaskEndScheduler()。
 * 输入参数: 无
 * 输出参数: 无
 * 返 回 值: 无
 * 其它说明: 1.根据configSUPPORT_STATIC_ALLOCATION配置选择静态或动态创建空闲任务
 *          2.根据configUSE_TIMERS配置决定是否创建定时器任务
 *          3.调用端口特定的xPortStartScheduler()启动硬件调度器
 * 修改日期      版本号          修改人            修改内容
 * ----------------------------------------------------------------------------
 * 2025/09/02     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
void vTaskStartScheduler(void)
{
    BaseType_t xReturn; /* 操作结果状态码 */

    /* 在最低优先级创建空闲任务 */
    #if (configSUPPORT_STATIC_ALLOCATION == 1) /* 静态内存分配配置 */
    {
        StaticTask_t *pxIdleTaskTCBBuffer = NULL;   /* 空闲任务控制块内存指针 */
        StackType_t *pxIdleTaskStackBuffer = NULL;  /* 空闲任务堆栈内存指针 */
        uint32_t ulIdleTaskStackSize;               /* 空闲任务堆栈大小 */

        /* 通过应用回调函数获取用户提供的空闲任务内存地址 */
        vApplicationGetIdleTaskMemory(&pxIdleTaskTCBBuffer, 
                                     &pxIdleTaskStackBuffer, 
                                     &ulIdleTaskStackSize);
        
        /* 使用静态方法创建空闲任务 */
        xIdleTaskHandle = xTaskCreateStatic(
            prvIdleTask,                       /* 任务函数入口 */
            "IDLE",                            /* 任务名称 */
            ulIdleTaskStackSize,               /* 堆栈深度 */
            (void *)NULL,                      /* 任务参数 */
            (tskIDLE_PRIORITY | portPRIVILEGE_BIT), /* 任务优先级（含特权位） */
            pxIdleTaskStackBuffer,             /* 堆栈缓冲区指针 */
            pxIdleTaskTCBBuffer                /* 任务控制块缓冲区指针 */
        );

        if (xIdleTaskHandle != NULL) /* 检查任务创建结果 */
        {
            xReturn = pdPASS; /* 创建成功 */
        }
        else
        {
            xReturn = pdFAIL; /* 创建失败 */
        }
    }
    #else /* 动态内存分配配置 */
    {
        /* 使用动态内存分配方法创建空闲任务 */
        xReturn = xTaskCreate(
            prvIdleTask,                       /* 任务函数入口 */
            "IDLE",                            /* 任务名称 */
            configMINIMAL_STACK_SIZE,          /* 使用默认最小堆栈深度 */
            (void *)NULL,                      /* 任务参数 */
            (tskIDLE_PRIORITY | portPRIVILEGE_BIT), /* 任务优先级 */
            &xIdleTaskHandle                   /* 任务句柄指针 */
        );
    }
    #endif /* configSUPPORT_STATIC_ALLOCATION */

    #if (configUSE_TIMERS == 1) /* 定时器功能配置 */
    {
        if (xReturn == pdPASS) /* 若空闲任务创建成功 */
        {
            xReturn = xTimerCreateTimerTask(); /* 创建定时器服务任务 */
        }
        else
        {
            mtCOVERAGE_TEST_MARKER(); /* 代码覆盖测试标记 */
        }
    }
    #endif /* configUSE_TIMERS */

    if (xReturn == pdPASS) /* 检查前期任务创建是否全部成功 */
    {
        /* 关闭中断确保在调用xPortStartScheduler()前不会发生时钟滴答中断。
           已创建任务的堆栈中包含中断启用状态字，因此当第一个任务开始运行时中断会自动重新启用 */
        portDISABLE_INTERRUPTS();

        #if (configUSE_NEWLIB_REENTRANT == 1) /* Newlib可重入配置 */
        {
            /* 将Newlib的_impure_ptr指向即将运行的首个任务的_reent结构 */
            _impure_ptr = &(pxCurrentTCB->xNewLib_reent);
        }
        #endif /* configUSE_NEWLIB_REENTRANT */

        xNextTaskUnblockTime = portMAX_DELAY; /* 初始化下一个任务解除阻塞时间 */
        xSchedulerRunning = pdTRUE;           /* 设置调度器运行标志 */
        xTickCount = (TickType_t)0U;          /* 初始化系统时钟计数器 */

        /* 若定义了configGENERATE_RUN_TIME_STATS，需配置运行时统计计时器 */
        portCONFIGURE_TIMER_FOR_RUN_TIME_STATS();

        /* 调用硬件相关的定时器初始化函数（此函数不会返回） */
        if (xPortStartScheduler() != pdFALSE)
        {
            /* 正常情况下不应执行至此，因为调度器运行后函数不会返回 */
        }
        else
        {
            /* 仅当调用xTaskEndScheduler()时才会执行至此 */
        }
    }
    else
    {
        /* 内核启动失败：无法分配足够内存创建空闲任务或定时器任务 */
        configASSERT(xReturn != errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY);
    }

    /* 防止编译器警告（当INCLUDE_xTaskGetIdleTaskHandle为0时xIdleTaskHandle未被使用） */
    (void)xIdleTaskHandle;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vTaskEndScheduler
 * 功能描述：停止FreeRTOS调度器运行，恢复原始的中断服务程序
 *           此函数用于完全停止RTOS调度器，通常用于从RTOS模式切换回裸机模式
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 停止调度器中断并调用可移植的调度器结束例程
 *   - 端口层必须确保中断使能位处于正确状态
 *   - 此操作不可逆，一旦停止调度器，RTOS功能将不再可用
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
void vTaskEndScheduler( void )
{
    /* 停止调度器中断并调用可移植的调度器结束例程，
       以便必要时可以恢复原始的中断服务程序。端口层必须确保中断使能位处于正确状态 */
    portDISABLE_INTERRUPTS();
    xSchedulerRunning = pdFALSE;
    vPortEndScheduler();
}
/*----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vTaskSuspendAll
 * 功能描述：挂起所有任务调度，增加调度器挂起计数
 *           此函数用于临时挂起任务调度，确保临界操作的原子性
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 不需要临界区，因为变量类型是BaseType_t
 *   - 调度器挂起可以嵌套，需要相同次数的vTaskResumeAll调用来恢复
 *   - 在挂起期间，任务不会切换，但中断仍然可以执行
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
void vTaskSuspendAll( void )
{
    /* 不需要临界区，因为变量类型是BaseType_t。在将此报告为错误之前，
       请阅读Richard Barry在FreeRTOS支持论坛中的回复 -
       http://goo.gl/wu4acr */
    ++uxSchedulerSuspended;
}
/*----------------------------------------------------------*/

#if ( configUSE_TICKLESS_IDLE != 0 )

/*******************************************************************************
 * 函数名称：prvGetExpectedIdleTime
 * 功能描述：获取预期的空闲时间，用于决定系统可以进入低功耗状态的时间长度
 *           此函数计算下一个任务解除阻塞之前的时间，用于无空闲滴答模式
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：
 *   - TickType_t: 预期的空闲时间（滴答数），如果不应进入低功耗状态则返回0
 * 其它说明：
 *   - 此函数仅在启用无空闲滴答模式(configUSE_TICKLESS_IDLE != 0)时编译
 *   - 考虑多种情况决定是否应该进入低功耗状态
 *   - 处理不同配置（抢占式/协作式调度，端口优化任务选择）的情况
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
static TickType_t prvGetExpectedIdleTime( void )
{
    TickType_t xReturn;  /* 返回值：预期的空闲时间 */
    UBaseType_t uxHigherPriorityReadyTasks = pdFALSE;  /* 标志：是否有更高优先级的就绪任务 */

    /* uxHigherPriorityReadyTasks处理configUSE_PREEMPTION为0的情况，
       因此即使空闲任务正在运行，也可能有高于空闲优先级的任务处于就绪状态 */
    
    /* 如果没有使用端口优化的任务选择 */
    #if( configUSE_PORT_OPTIMISED_TASK_SELECTION == 0 )
    {
        /* 检查是否有优先级高于空闲优先级的任务 */
        if( uxTopReadyPriority > tskIDLE_PRIORITY )
        {
            uxHigherPriorityReadyTasks = pdTRUE;
        }
    }
    #else
    {
        const UBaseType_t uxLeastSignificantBit = ( UBaseType_t ) 0x01;

        /* 当使用端口优化任务选择时，uxTopReadyPriority变量用作位图。
           如果设置了除最低有效位以外的位，则表示有优先级高于空闲优先级的任务处于就绪状态。
           这处理了使用协作式调度器的情况 */
        if( uxTopReadyPriority > uxLeastSignificantBit )
        {
            uxHigherPriorityReadyTasks = pdTRUE;
        }
    }
    #endif

    /* 检查当前任务优先级是否高于空闲优先级 */
    if( pxCurrentTCB->uxPriority > tskIDLE_PRIORITY )
    {
        /* 当前任务优先级高于空闲优先级，不应进入低功耗状态 */
        xReturn = 0;
    }
    /* 检查就绪列表中空闲优先级任务的数量是否大于1 */
    else if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ tskIDLE_PRIORITY ] ) ) > 1 )
    {
        /* 就绪状态中有其他空闲优先级任务。如果使用时间切片，
           则必须处理下一个滴答中断 */
        xReturn = 0;
    }
    /* 检查是否有优先级高于空闲优先级的就绪任务 */
    else if( uxHigherPriorityReadyTasks != pdFALSE )
    {
        /* 有优先级高于空闲优先级的任务处于就绪状态。
           只有在configUSE_PREEMPTION为0时才能达到此路径 */
        xReturn = 0;
    }
    else
    {
        /* 计算下一个任务解除阻塞时间与当前滴答计数的差值 */
        xReturn = xNextTaskUnblockTime - xTickCount;
    }

    /* 返回预期的空闲时间 */
    return xReturn;
}

#endif /* configUSE_TICKLESS_IDLE */
/*----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xTaskResumeAll
 * 功能描述：恢复调度器运行，处理在调度器挂起期间积累的待处理任务和滴答事件
 *           此函数是vTaskSuspendAll的配对函数，用于恢复被挂起的调度器
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：
 *   - BaseType_t: 指示是否已经执行了任务切换
 *     pdTRUE: 已经执行了任务切换
 *     pdFALSE: 没有执行任务切换
 * 其它说明：
 *   - 此函数处理在调度器挂起期间积累的待处理任务和滴答事件
 *   - 将待就绪任务移动到适当的就绪列表，并处理挂起的滴答事件
 *   - 可能需要重新计算下一个任务解除阻塞时间，特别是对于低功耗实现
 *   - 如果需要，会执行任务切换
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
BaseType_t xTaskResumeAll( void )
{
    TCB_t *pxTCB = NULL;              /* 指向任务控制块的指针 */
    BaseType_t xAlreadyYielded = pdFALSE;  /* 返回值：是否已经执行了任务切换 */

    /* 如果uxSchedulerSuspended为零，则此函数与之前对vTaskSuspendAll()的调用不匹配 */
    configASSERT( uxSchedulerSuspended );

    /* 可能在调度器挂起期间，ISR导致任务从事件列表中移除。
       如果是这种情况，被移除的任务将被添加到xPendingReadyList。
       一旦调度器恢复，就可以安全地将所有待就绪任务从此列表移动到适当的就绪列表 */
    taskENTER_CRITICAL();
    {
        /* 减少调度器挂起计数 */
        --uxSchedulerSuspended;

        /* 检查调度器挂起计数是否减到零（完全恢复） */
        if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
        {
            /* 检查系统中是否存在任务 */
            if( uxCurrentNumberOfTasks > ( UBaseType_t ) 0U )
            {
                /* 将待就绪列表中的任何就绪任务移动到适当的就绪列表 */
                while( listLIST_IS_EMPTY( &xPendingReadyList ) == pdFALSE )
                {
                    /* 获取待就绪列表中的第一个任务 */
                    pxTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( ( &xPendingReadyList ) );
                    
                    /* 从事件列表和状态列表中移除任务 */
                    ( void ) uxListRemove( &( pxTCB->xEventListItem ) );
                    ( void ) uxListRemove( &( pxTCB->xStateListItem ) );
                    
                    /* 将任务添加到就绪列表 */
                    prvAddTaskToReadyList( pxTCB );

                    /* 如果移动的任务的优先级高于或等于当前任务，则必须执行yield */
                    if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
                    {
                        xYieldPending = pdTRUE;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }

                /* 如果有任务被移动（即pxTCB不为NULL） */
                if( pxTCB != NULL )
                {
                    /* 在调度器挂起期间有任务被解除阻塞，这可能阻止了下一个解除阻塞时间的重新计算，
                       在这种情况下现在重新计算它。主要对低功耗无空闲滴答实现很重要，
                       这可以避免不必要的低功耗状态退出 */
                    prvResetNextTaskUnblockTime();
                }

                /* 如果在调度器挂起期间发生了任何滴答，现在应该处理它们。
                   这确保滴答计数不会滑动，并且任何延迟的任务在正确的时间恢复 */
                {
                    UBaseType_t uxPendedCounts = uxPendedTicks; /* 非易失性副本 */

                    if( uxPendedCounts > ( UBaseType_t ) 0U )
                    {
                        /* 处理所有挂起的滴答 */
                        do
                        {
                            /* 增加滴答计数并检查是否需要任务切换 */
                            if( xTaskIncrementTick() != pdFALSE )
                            {
                                xYieldPending = pdTRUE;
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();
                            }
                            --uxPendedCounts;
                        } while( uxPendedCounts > ( UBaseType_t ) 0U );

                        /* 重置挂起的滴答计数 */
                        uxPendedTicks = 0;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }

                /* 检查是否有yield挂起 */
                if( xYieldPending != pdFALSE )
                {
                    #if( configUSE_PREEMPTION != 0 )
                    {
                        /* 如果使用抢占式调度，标记已经执行了yield */
                        xAlreadyYielded = pdTRUE;
                    }
                    #endif
                    
                    /* 如果使用抢占式调度，执行任务切换 */
                    taskYIELD_IF_USING_PREEMPTION();
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }
    taskEXIT_CRITICAL();

    /* 返回是否已经执行了任务切换 */
    return xAlreadyYielded;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xTaskGetTickCount
 * 功能描述：获取当前系统滴答计数器的值，表示系统启动后经过的时钟节拍数
 *           此函数用于获取系统的当前时间，用于时间测量和超时计算
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：
 *   - TickType_t: 当前的系统滴答计数值
 * 其它说明：
 *   - 使用临界区保护滴答计数器的读取，确保在16位处理器上的原子性
 *   - 滴答计数器从系统启动开始计数，随时间递增
 *   - 可用于计算时间间隔和实现超时机制
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
TickType_t xTaskGetTickCount( void )
{
    TickType_t xTicks;  /* 存储滴答计数值的变量 */

    /* 如果在16位处理器上运行，需要临界区保护 */
    portTICK_TYPE_ENTER_CRITICAL();
    {
        /* 读取当前的滴答计数值 */
        xTicks = xTickCount;
    }
    portTICK_TYPE_EXIT_CRITICAL();

    /* 返回滴答计数值 */
    return xTicks;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xTaskGetTickCountFromISR
 * 功能描述：从中断服务程序中获取当前系统滴答计数器的值
 *           此函数是xTaskGetTickCount的中断安全版本，可在ISR中调用
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：
 *   - TickType_t: 当前的系统滴答计数值
 * 其它说明：
 *   - 此函数是中断安全版本，可在中断服务程序中调用
 *   - 验证中断优先级，确保不会从过高优先级的中断调用
 *   - 使用中断安全的临界区保护滴答计数器的读取
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
TickType_t xTaskGetTickCountFromISR( void )
{
    TickType_t xReturn;               /* 返回值：滴答计数值 */
    UBaseType_t uxSavedInterruptStatus;  /* 保存的中断状态 */

    /* 支持中断嵌套的RTOS端口具有最大系统调用（或最大API调用）中断优先级的概念。
       高于最大系统调用优先级的中断保持永久启用，即使RTOS内核处于临界区，
       但不能调用任何FreeRTOS API函数。如果在FreeRTOSConfig.h中定义了configASSERT()，
       则portASSERT_IF_INTERRUPT_PRIORITY_INVALID()将导致断言失败，
       如果从已被分配高于配置的最大系统调用优先级的中断调用FreeRTOS API函数。
       只有以FromISR结尾的FreeRTOS函数可以从已被分配优先级等于或（逻辑上）
       低于最大系统调用中断优先级的中断调用。FreeRTOS维护一个单独的中断安全API，
       以确保中断入口尽可能快速和简单。更多信息（尽管是Cortex-M特定的）
       在以下链接提供：http://www.freertos.org/RTOS-Cortex-M3-M4.html */
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    /* 保存中断状态并屏蔽中断 */
    uxSavedInterruptStatus = portTICK_TYPE_SET_INTERRUPT_MASK_FROM_ISR();
    {
        /* 读取当前的滴答计数值 */
        xReturn = xTickCount;
    }
    /* 恢复中断状态 */
    portTICK_TYPE_CLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

    /* 返回滴答计数值 */
    return xReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：uxTaskGetNumberOfTasks
 * 功能描述：获取系统中当前存在的任务数量
 *           此函数返回系统中所有任务的总数，包括就绪、阻塞、挂起和已删除但未清理的任务
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：
 *   - UBaseType_t: 系统中当前存在的任务数量
 * 其它说明：
 *   - 不需要临界区保护，因为变量类型是BaseType_t
 *   - 返回的值包括所有状态的任务（就绪、阻塞、挂起、已删除但未清理）
 *   - 可用于系统监控和资源管理
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
UBaseType_t uxTaskGetNumberOfTasks( void )
{
    /* 不需要临界区，因为变量类型是BaseType_t */
    return uxCurrentNumberOfTasks;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：pcTaskGetName
 * 功能描述：获取指定任务的名称字符串
 *           此函数返回指向任务名称字符串的指针，可用于标识和显示任务信息
 * 输入参数：
 *   - xTaskToQuery: 要查询的任务句柄
 *     特殊值：传入NULL表示查询当前任务的名称
 * 输出参数：无
 * 返 回 值：
 *   - char*: 指向任务名称字符串的指针
 * 其它说明：
 *   - 任务名称在创建任务时设置，最大长度为configMAX_TASK_NAME_LEN
 *   - 返回的指针指向任务控制块中的名称字符串，不应被修改
 *   - 可用于调试、日志记录和任务标识
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
char *pcTaskGetName( TaskHandle_t xTaskToQuery ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
    TCB_t *pxTCB;  /* 指向任务控制块的指针 */

    /* 如果传入null，则查询调用任务的名称 */
    pxTCB = prvGetTCBFromHandle( xTaskToQuery );
    configASSERT( pxTCB );
    
    /* 返回任务名称字符串的指针 */
    return &( pxTCB->pcTaskName[ 0 ] );
}
/*-----------------------------------------------------------*/

#if ( INCLUDE_xTaskGetHandle == 1 )

/*******************************************************************************
 * 函数名称：prvSearchForNameWithinSingleList
 * 功能描述：在单个任务列表中搜索指定名称的任务控制块
 *           此函数是xTaskGetHandle的内部实现，用于在特定列表中按名称搜索任务
 * 输入参数：
 *   - pxList: 要搜索的任务列表指针
 *   - pcNameToQuery: 要查询的任务名称字符串
 * 输出参数：无
 * 返 回 值：
 *   - TCB_t*: 找到的任务控制块指针，如果未找到则返回NULL
 * 其它说明：
 *   - 此函数在调度器挂起的情况下调用，确保搜索过程的原子性
 *   - 使用字符比较逐字符匹配任务名称
 *   - 支持任务名称的部分匹配和完整匹配
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
static TCB_t *prvSearchForNameWithinSingleList( List_t *pxList, const char pcNameToQuery[] )
{
    TCB_t *pxNextTCB;        /* 指向下一个任务控制块的指针 */
    TCB_t *pxFirstTCB;       /* 指向列表第一个任务控制块的指针（用于检测循环结束） */
    TCB_t *pxReturn = NULL;  /* 返回值：找到的任务控制块指针 */
    UBaseType_t x;           /* 循环计数器：用于字符比较 */
    char cNextChar;          /* 当前比较的字符 */

    /* 此函数在调度器挂起的情况下调用 */

    /* 检查列表是否包含任务（列表长度大于0） */
    if( listCURRENT_LIST_LENGTH( pxList ) > ( UBaseType_t ) 0 )
    {
        /* 获取列表中的第一个任务控制块，作为循环结束的标记 */
        listGET_OWNER_OF_NEXT_ENTRY( pxFirstTCB, pxList );

        /* 遍历列表中的每个任务，搜索指定名称的任务 */
        do
        {
            /* 获取列表中的下一个任务控制块 */
            listGET_OWNER_OF_NEXT_ENTRY( pxNextTCB, pxList );

            /* 检查名称中的每个字符，寻找匹配或不匹配 */
            for( x = ( UBaseType_t ) 0; x < ( UBaseType_t ) configMAX_TASK_NAME_LEN; x++ )
            {
                /* 获取任务名称中的下一个字符 */
                cNextChar = pxNextTCB->pcTaskName[ x ];

                /* 检查字符是否不匹配 */
                if( cNextChar != pcNameToQuery[ x ] )
                {
                    /* 字符不匹配，跳出字符比较循环 */
                    break;
                }
                /* 检查是否到达字符串结尾 */
                else if( cNextChar == 0x00 )
                {
                    /* 两个字符串都终止，一定找到了匹配 */
                    pxReturn = pxNextTCB;
                    break;
                }
                else
                {
                    /* 字符匹配但未到字符串结尾：添加测试覆盖率标记 */
                    mtCOVERAGE_TEST_MARKER();
                }
            }

            /* 如果找到了匹配的任务，跳出循环 */
            if( pxReturn != NULL )
            {
                /* 已找到任务句柄 */
                break;
            }

        } while( pxNextTCB != pxFirstTCB );  /* 循环直到回到第一个任务 */
    }
    else
    {
        /* 列表为空：添加测试覆盖率标记 */
        mtCOVERAGE_TEST_MARKER();
    }

    /* 返回找到的任务控制块指针（如果未找到则返回NULL） */
    return pxReturn;
}

#endif /* INCLUDE_xTaskGetHandle */
/*-----------------------------------------------------------*/

#if ( INCLUDE_xTaskGetHandle == 1 )

/*******************************************************************************
 * 函数名称：xTaskGetHandle
 * 功能描述：根据任务名称查询任务句柄，用于通过任务名称获取任务的控制句柄
 *           此函数在系统中搜索指定名称的任务，并返回其任务句柄
 * 输入参数：
 *   - pcNameToQuery: 要查询的任务名称字符串
 * 输出参数：无
 * 返 回 值：
 *   - TaskHandle_t: 找到的任务句柄，如果未找到则返回NULL
 * 其它说明：
 *   - 此函数仅在启用获取任务句柄功能(INCLUDE_xTaskGetHandle == 1)时编译
 *   - 任务名称将被截断为configMAX_TASK_NAME_LEN - 1字节
 *   - 需要在挂起所有任务的情况下执行，以确保搜索过程中的数据一致性
 *   - 在所有可能的状态列表中搜索任务（就绪、延迟、挂起、已删除）
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
TaskHandle_t xTaskGetHandle( const char *pcNameToQuery ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
    UBaseType_t uxQueue = configMAX_PRIORITIES;  /* 循环计数器：用于遍历优先级队列 */
    TCB_t* pxTCB = NULL;                         /* 指向找到的任务控制块的指针 */

    /* 任务名称将被截断为configMAX_TASK_NAME_LEN - 1字节 */
    configASSERT( strlen( pcNameToQuery ) < configMAX_TASK_NAME_LEN );

    /* 挂起所有任务，确保在搜索过程中任务状态不会改变 */
    vTaskSuspendAll();
    {
        /* 搜索就绪列表 */
        do
        {
            uxQueue--;  /* 递减优先级计数器 */
            
            /* 在当前优先级队列中搜索指定名称的任务 */
            pxTCB = prvSearchForNameWithinSingleList( ( List_t * ) &( pxReadyTasksLists[ uxQueue ] ), pcNameToQuery );

            if( pxTCB != NULL )
            {
                /* 找到了任务句柄，跳出循环 */
                break;
            }

        } while( uxQueue > ( UBaseType_t ) tskIDLE_PRIORITY ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */

        /* 搜索延迟列表 */
        if( pxTCB == NULL )
        {
            pxTCB = prvSearchForNameWithinSingleList( ( List_t * ) pxDelayedTaskList, pcNameToQuery );
        }

        if( pxTCB == NULL )
        {
            pxTCB = prvSearchForNameWithinSingleList( ( List_t * ) pxOverflowDelayedTaskList, pcNameToQuery );
        }

        /* 如果启用了任务挂起功能 */
        #if ( INCLUDE_vTaskSuspend == 1 )
        {
            if( pxTCB == NULL )
            {
                /* 搜索挂起列表 */
                pxTCB = prvSearchForNameWithinSingleList( &xSuspendedTaskList, pcNameToQuery );
            }
        }
        #endif

        /* 如果启用了任务删除功能 */
        #if( INCLUDE_vTaskDelete == 1 )
        {
            if( pxTCB == NULL )
            {
                /* 搜索已删除任务列表 */
                pxTCB = prvSearchForNameWithinSingleList( &xTasksWaitingTermination, pcNameToQuery );
            }
        }
        #endif
    }
    /* 恢复所有任务 */
    ( void ) xTaskResumeAll();

    /* 返回找到的任务句柄（如果未找到则返回NULL） */
    return ( TaskHandle_t ) pxTCB;
}

#endif /* INCLUDE_xTaskGetHandle */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

/*******************************************************************************
 * 函数名称：uxTaskGetSystemState
 * 功能描述：获取系统中所有任务的状态信息，包括就绪、阻塞、挂起和已删除的任务
 *           此函数提供系统的完整快照，用于监控、调试和性能分析
 * 输入参数：
 *   - pxTaskStatusArray: 指向TaskStatus_t数组的指针，用于存储任务状态信息
 *   - uxArraySize: 数组大小，必须至少等于当前任务数量
 *   - pulTotalRunTime: 指向总运行时间的指针，用于存储系统总运行时间（如果启用运行时统计）
 * 输出参数：
 *   - pxTaskStatusArray: 被填充的任务状态数组
 *   - pulTotalRunTime: 被设置的总运行时间值
 * 返 回 值：
 *   - UBaseType_t: 成功填充的任务状态结构数量
 * 其它说明：
 *   - 此函数仅在启用跟踪功能(configUSE_TRACE_FACILITY == 1)时编译
 *   - 需要在挂起所有任务的情况下执行，以确保数据一致性
 *   - 提供系统的完整快照，包括所有状态的任务信息
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
UBaseType_t uxTaskGetSystemState( TaskStatus_t * const pxTaskStatusArray, const UBaseType_t uxArraySize, uint32_t * const pulTotalRunTime )
{
    UBaseType_t uxTask = 0;      /* 计数器：已处理的任务数量 */
    UBaseType_t uxQueue = configMAX_PRIORITIES;  /* 循环计数器：用于遍历优先级队列 */

    /* 挂起所有任务，确保在获取系统状态时任务状态不会改变 */
    vTaskSuspendAll();
    {
        /* 检查数组大小是否足够容纳系统中的所有任务 */
        if( uxArraySize >= uxCurrentNumberOfTasks )
        {
            /* 填充TaskStatus_t结构，包含就绪状态下每个任务的信息 */
            do
            {
                uxQueue--;  /* 递减优先级计数器 */
                
                /* 处理当前优先级队列中的任务，添加到任务状态数组 */
                uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), 
                                                       &( pxReadyTasksLists[ uxQueue ] ), 
                                                       eReady );

            } while( uxQueue > ( UBaseType_t ) tskIDLE_PRIORITY ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */

            /* 填充TaskStatus_t结构，包含阻塞状态下每个任务的信息 */
            uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), 
                                                   ( List_t * ) pxDelayedTaskList, 
                                                   eBlocked );
            
            /* 处理溢出延迟任务列表中的阻塞任务 */
            uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), 
                                                   ( List_t * ) pxOverflowDelayedTaskList, 
                                                   eBlocked );

            /* 如果启用了任务删除功能 */
            #if( INCLUDE_vTaskDelete == 1 )
            {
                /* 填充TaskStatus_t结构，包含已删除但尚未清理的每个任务的信息 */
                uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), 
                                                       &xTasksWaitingTermination, 
                                                       eDeleted );
            }
            #endif

            /* 如果启用了任务挂起功能 */
            #if ( INCLUDE_vTaskSuspend == 1 )
            {
                /* 填充TaskStatus_t结构，包含挂起状态下每个任务的信息 */
                uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), 
                                                       &xSuspendedTaskList, 
                                                       eSuspended );
            }
            #endif

            /* 如果启用了运行时统计功能 */
            #if ( configGENERATE_RUN_TIME_STATS == 1)
            {
                if( pulTotalRunTime != NULL )
                {
                    /* 获取总运行时间计数器值 */
                    #ifdef portALT_GET_RUN_TIME_COUNTER_VALUE
                        portALT_GET_RUN_TIME_COUNTER_VALUE( ( *pulTotalRunTime ) );
                    #else
                        *pulTotalRunTime = portGET_RUN_TIME_COUNTER_VALUE();
                    #endif
                }
            }
            #else
            {
                if( pulTotalRunTime != NULL )
                {
                    /* 未启用运行时统计，将总运行时间设为0 */
                    *pulTotalRunTime = 0;
                }
            }
            #endif
        }
        else
        {
            /* 数组大小不足：添加测试覆盖率标记 */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    /* 恢复所有任务 */
    ( void ) xTaskResumeAll();

    /* 返回成功填充的任务状态结构数量 */
    return uxTask;
}

#endif /* configUSE_TRACE_FACILITY */
/*----------------------------------------------------------*/

#if ( INCLUDE_xTaskGetIdleTaskHandle == 1 )

/*******************************************************************************
 * 函数名称：xTaskGetIdleTaskHandle
 * 功能描述：获取空闲任务的句柄，用于直接访问FreeRTOS的空闲任务
 *           此函数提供对系统自动创建的空闲任务的访问能力
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：
 *   - TaskHandle_t: 空闲任务的句柄，如果调度器未启动则可能为NULL
 * 其它说明：
 *   - 此函数仅在启用获取空闲任务句柄功能(INCLUDE_xTaskGetIdleTaskHandle == 1)时编译
 *   - 使用断言确保在调用时空闲任务句柄不为NULL（调度器已启动）
 *   - 主要用于高级调试和监控场景，普通应用程序通常不需要直接访问空闲任务
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
TaskHandle_t xTaskGetIdleTaskHandle( void )
{
    /* 如果在调度器启动之前调用xTaskGetIdleTaskHandle()，则xIdleTaskHandle将为NULL */
    configASSERT( ( xIdleTaskHandle != NULL ) );
    
    /* 返回空闲任务的句柄 */
    return xIdleTaskHandle;
}

#endif /* INCLUDE_xTaskGetIdleTaskHandle */
/*----------------------------------------------------------*/

#if ( configUSE_TICKLESS_IDLE != 0 )

/*******************************************************************************
 * 函数名称：vTaskStepTick
 * 功能描述：在 tick 被抑制一段时间后，校正 tick 计数值
 *           此函数用于无空闲滴答模式，在系统从低功耗睡眠唤醒后调整滴答计数器
 * 输入参数：
 *   - xTicksToJump: 要跳过的 tick 数量，即系统处于低功耗状态的 tick 周期数
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数仅在启用无空闲滴答模式(configUSE_TICKLESS_IDLE != 0)时编译
 *   - 不会为每个跳过的 tick 调用 tick 钩子函数
 *   - 使用断言确保跳过的 tick 数量不会导致 tick 计数超过下一个任务解除阻塞时间
 *   - 更新 tick 计数并增加跟踪计数
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
void vTaskStepTick( const TickType_t xTicksToJump )
{
    /* 在 tick 被抑制一段时间后，校正 tick 计数值。
       注意：这不会为每个跳过的 tick 调用 tick 钩子函数 */
    configASSERT( ( xTickCount + xTicksToJump ) <= xNextTaskUnblockTime );
    
    /* 增加 tick 计数值，跳过指定的 tick 数量 */
    xTickCount += xTicksToJump;
    
    /* 跟踪 tick 计数的增加，用于调试和性能分析 */
    traceINCREASE_TICK_COUNT( xTicksToJump );
}

#endif /* configUSE_TICKLESS_IDLE */
/*----------------------------------------------------------*/

#if ( INCLUDE_xTaskAbortDelay == 1 )

/*******************************************************************************
 * 函数名称：xTaskAbortDelay
 * 功能描述：中止指定任务的延迟状态，提前将其从阻塞状态解除
 *           此函数允许强制将任务从阻塞状态唤醒，而不需要等待超时或事件发生
 * 输入参数：
 *   - xTask: 任务句柄，指定要中止延迟的任务
 * 输出参数：无
 * 返 回 值：
 *   - BaseType_t: 操作结果
 *     pdTRUE: 成功中止任务的延迟状态
 *     pdFALSE: 任务不在阻塞状态，无法中止延迟
 * 其它说明：
 *   - 此函数仅在启用任务延迟中止功能(INCLUDE_xTaskAbortDelay == 1)时编译
 *   - 使用调度器挂起和临界区保护确保操作的原子性
 *   - 处理任务从阻塞列表和事件列表中的移除
 *   - 考虑抢占式调度下的上下文切换需求
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
BaseType_t xTaskAbortDelay( TaskHandle_t xTask )
{
    TCB_t *pxTCB = ( TCB_t * ) xTask;  /* 指向任务控制块的指针 */
    BaseType_t xReturn = pdFALSE;      /* 返回值：操作结果 */

    /* 断言检查任务控制块指针有效性 */
    configASSERT( pxTCB );

    /* 挂起所有任务，确保操作的原子性 */
    vTaskSuspendAll();
    {
        /* 任务只有真正处于阻塞状态时才能被提前从阻塞状态移除 */
        if( eTaskGetState( xTask ) == eBlocked )
        {
            /* 从阻塞列表中移除对任务的引用。中断不会触碰xStateListItem，
               因为调度器已挂起 */
            ( void ) uxListRemove( &( pxTCB->xStateListItem ) );

            /* 任务是否也在等待事件？如果是，也从事件列表中移除。
               即使调度器已挂起，中断也可能触碰事件列表项，
               因此使用临界区 */
            taskENTER_CRITICAL();
            {
                if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
                {
                    /* 从事件列表中移除任务的事件列表项 */
                    ( void ) uxListRemove( &( pxTCB->xEventListItem ) );
                    /* 设置延迟中止标志 */
                    pxTCB->ucDelayAborted = pdTRUE;
                }
                else
                {
                    /* 任务不在事件列表中：添加测试覆盖率标记 */
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            taskEXIT_CRITICAL();

            /* 将已解除阻塞的任务放入适当的就绪列表 */
            prvAddTaskToReadyList( pxTCB );

            /* 如果抢占关闭，被解除阻塞的任务不会导致立即上下文切换 */
            #if (  configUSE_PREEMPTION == 1 )
            {
                /* 抢占开启，但只有在被解除阻塞的任务的优先级
                   等于或高于当前正在执行的任务时才应执行上下文切换 */
                if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
                {
                    /* 挂起yield操作，在调度器恢复时执行 */
                    xYieldPending = pdTRUE;
                }
                else
                {
                    /* 优先级不高于当前任务：添加测试覆盖率标记 */
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            #endif /* configUSE_PREEMPTION */

            /* 操作成功，设置返回值为pdTRUE */
            xReturn = pdTRUE;
        }
        else
        {
            /* 任务不在阻塞状态：添加测试覆盖率标记 */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    /* 恢复所有任务 */
    xTaskResumeAll();

    /* 返回操作结果 */
    return xReturn;
}

#endif /* INCLUDE_xTaskAbortDelay */
/*----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xTaskIncrementTick
 * 功能描述：递增系统时钟节拍计数器，检查是否有任务需要解除阻塞，并处理时间片调度。
 *           该函数由移植层在每次时钟节拍中断时调用，是FreeRTOS调度器的核心函数之一。
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：BaseType_t 
 *           - pdTRUE: 需要进行上下文切换
 *           - pdFALSE: 不需要进行上下文切换
 * 其它说明：
 *   - 当调度器挂起时(uxSchedulerSuspended != pdFALSE)，节拍计数会被暂存到uxPendedTicks
 *   - 会处理延迟任务列表和溢出延迟列表的切换
 *   - 会检查并解除阻塞到期的任务
 *   - 会触发Tick钩子函数
 *   - 支持时间片轮转调度和抢占式调度
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
BaseType_t xTaskIncrementTick( void )
{
    TCB_t * pxTCB;                          /* 指向任务控制块的指针 */
    TickType_t xItemValue;                  /* 存储列表项值（任务解除阻塞时间） */
    BaseType_t xSwitchRequired = pdFALSE;   /* 是否需要上下文切换的标志，初始化为pdFALSE */

    /* 每次发生节拍中断时由移植层调用。
       递增节拍计数器，然后检查新的节拍值是否会导致任何任务解除阻塞。 */
    traceTASK_INCREMENT_TICK( xTickCount ); /* 跟踪调试：记录节拍递增事件 */

    /* 检查调度器是否未被挂起 */
    if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
    {
        /* 小优化：在此代码块内节拍计数不会改变，使用常量提高代码效率 */
        const TickType_t xConstTickCount = xTickCount + 1;

        /* 递增RTOS节拍计数器，如果计数器溢出归零，则切换延迟和溢出延迟列表 */
        xTickCount = xConstTickCount;       /* 更新全局节拍计数器 */

        /* 检查节拍计数器是否溢出（归零） */
        if( xConstTickCount == ( TickType_t ) 0U )
        {
            taskSWITCH_DELAYED_LISTS();     /* 切换延迟列表和溢出延迟列表 */
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();       /* 用于测试覆盖率，无实际功能 */
        }

        /* 检查当前节拍数是否达到下一个任务解除阻塞时间 */
        if( xConstTickCount >= xNextTaskUnblockTime )
        {
            /* 循环处理所有需要解除阻塞的任务 */
            for( ;; )
            {
                /* 检查延迟任务列表是否为空 */
                if( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )
                {
                    /* 延迟列表为空，将xNextTaskUnblockTime设置为最大可能值，
                       使得下次很难通过xTickCount >= xNextTaskUnblockTime检查 */
                    xNextTaskUnblockTime = portMAX_DELAY; /*lint !e961 屏蔽MISRA警告 */
                    break;                                /* 退出循环 */
                }
                else
                {
                    /* 延迟列表不为空，获取列表首项的任务控制块和列表项值 */
                    pxTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList );
                    xItemValue = listGET_LIST_ITEM_VALUE( &( pxTCB->xStateListItem ) );

                    /* 检查当前节拍数是否小于任务的解除阻塞时间 */
                    if( xConstTickCount < xItemValue )
                    {
                        /* 尚未到达任务解除阻塞时间，但需要更新下一个解除阻塞时间为该任务的阻塞时间 */
                        xNextTaskUnblockTime = xItemValue;
                        break;                            /* 退出循环 */
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();         /* 测试覆盖率标记 */
                    }

                    /* 从阻塞状态移除任务：将任务从阻塞列表移除 */
                    ( void ) uxListRemove( &( pxTCB->xStateListItem ) );

                    /* 检查任务是否同时在事件列表中等待（如信号量、队列等） */
                    if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
                    {
                        /* 从事件列表中移除该任务 */
                        ( void ) uxListRemove( &( pxTCB->xEventListItem ) );
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();         /* 测试覆盖率标记 */
                    }

                    /* 将已解除阻塞的任务添加到就绪列表 */
                    prvAddTaskToReadyList( pxTCB );

                    /* 如果启用抢占式调度，检查是否需要上下文切换 */
                    #if (  configUSE_PREEMPTION == 1 )
                    {
                        /* 只有当解除阻塞的任务优先级大于等于当前任务时才需要切换 */
                        if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
                        {
                            xSwitchRequired = pdTRUE;     /* 设置切换标志 */
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();     /* 测试覆盖率标记 */
                        }
                    }
                    #endif /* configUSE_PREEMPTION */
                }
            }
        }

        /* 如果启用抢占式调度和时间片轮转，检查同优先级任务是否需要时间片切换 */
        #if ( ( configUSE_PREEMPTION == 1 ) && ( configUSE_TIME_SLICING == 1 ) )
        {
            /* 检查当前优先级就绪列表中的任务数量是否大于1 */
            if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ pxCurrentTCB->uxPriority ] ) ) > ( UBaseType_t ) 1 )
            {
                xSwitchRequired = pdTRUE;                 /* 设置切换标志 */
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();                 /* 测试覆盖率标记 */
            }
        }
        #endif /* ( ( configUSE_PREEMPTION == 1 ) && ( configUSE_TIME_SLICING == 1 ) ) */

        /* 如果启用Tick钩子函数，执行应用程序定义的钩子函数 */
        #if ( configUSE_TICK_HOOK == 1 )
        {
            /* 在调度器解锁（处理挂起的节拍）时不调用钩子函数 */
            if( uxPendedTicks == ( UBaseType_t ) 0U )
            {
                vApplicationTickHook();                   /* 调用应用钩子函数 */
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();                 /* 测试覆盖率标记 */
            }
        }
        #endif /* configUSE_TICK_HOOK */
    }
    else
    {
        /* 调度器挂起时，增加挂起的节拍计数 */
        ++uxPendedTicks;

        /* 即使调度器锁定，Tick钩子函数也会定期调用 */
        #if ( configUSE_TICK_HOOK == 1 )
        {
            vApplicationTickHook();                       /* 调用应用钩子函数 */
        }
        #endif
    }

    /* 如果启用抢占式调度，检查是否有延迟的切换请求 */
    #if ( configUSE_PREEMPTION == 1 )
    {
        if( xYieldPending != pdFALSE )
        {
            xSwitchRequired = pdTRUE;                     /* 设置切换标志 */
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();                     /* 测试覆盖率标记 */
        }
    }
    #endif /* configUSE_PREEMPTION */

    return xSwitchRequired;                               /* 返回是否需要上下文切换 */
}
/*-----------------------------------------------------------*/

#if ( configUSE_APPLICATION_TASK_TAG == 1 )

/*******************************************************************************
 * 函数名称：vTaskSetApplicationTaskTag
 * 功能描述：设置指定任务的应用程序任务标签（钩子函数）
 *           此函数用于为任务设置应用程序定义的钩子函数
 * 输入参数：
 *   - xTask: 任务句柄，指定要设置钩子函数的任务
 *     特殊值：传入NULL表示设置当前任务的钩子函数
 *   - pxHookFunction: 要设置的钩子函数指针
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数仅在启用应用程序任务标签功能(configUSE_APPLICATION_TASK_TAG == 1)时编译
 *   - 使用临界区保护钩子函数的设置，因为该值可能被中断访问
 *   - 提供了一种为任务设置自定义钩子函数的机制
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
void vTaskSetApplicationTaskTag( TaskHandle_t xTask, TaskHookFunction_t pxHookFunction )
{
    TCB_t *xTCB;  /* 指向任务控制块的指针 */

    /* 如果xTask为NULL，则设置的是调用任务的钩子函数 */
    if( xTask == NULL )
    {
        xTCB = ( TCB_t * ) pxCurrentTCB;
    }
    else
    {
        xTCB = ( TCB_t * ) xTask;
    }

    /* 保存TCB中的钩子函数。需要临界区，因为该值可能被中断访问 */
    taskENTER_CRITICAL();
    {
        /* 将钩子函数指针保存到任务控制块中 */
        xTCB->pxTaskTag = pxHookFunction;
    }
    taskEXIT_CRITICAL();
}
#endif
/*-----------------------------------------------------------*/

#if ( configUSE_APPLICATION_TASK_TAG == 1 )

/*******************************************************************************
 * 函数名称：xTaskGetApplicationTaskTag
 * 功能描述：获取指定任务的应用程序任务标签（钩子函数）
 *           此函数用于检索与任务关联的应用程序定义的钩子函数
 * 输入参数：
 *   - xTask: 任务句柄，指定要获取钩子函数的任务
 *     特殊值：传入NULL表示获取当前任务的钩子函数
 * 输出参数：无
 * 返 回 值：
 *   - TaskHookFunction_t: 任务的应用程序钩子函数指针，如果未设置则返回NULL
 * 其它说明：
 *   - 此函数仅在启用应用程序任务标签功能(configUSE_APPLICATION_TASK_TAG == 1)时编译
 *   - 使用临界区保护钩子函数的访问，因为该值可能被中断访问
 *   - 提供了一种检索任务特定钩子函数的机制
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
TaskHookFunction_t xTaskGetApplicationTaskTag( TaskHandle_t xTask )
{
    TCB_t *xTCB;                /* 指向任务控制块的指针 */
    TaskHookFunction_t xReturn;  /* 返回值：钩子函数指针 */

    /* 如果xTask为NULL，则获取当前任务的钩子函数 */
    if( xTask == NULL )
    {
        xTCB = ( TCB_t * ) pxCurrentTCB;
    }
    else
    {
        xTCB = ( TCB_t * ) xTask;
    }

    /* 保存TCB中的钩子函数。需要临界区，因为该值可能被中断访问 */
    taskENTER_CRITICAL();
    {
        /* 从任务控制块中获取钩子函数指针 */
        xReturn = xTCB->pxTaskTag;
    }
    taskEXIT_CRITICAL();

    /* 返回钩子函数指针 */
    return xReturn;
}

#endif /* configUSE_APPLICATION_TASK_TAG */
/*-----------------------------------------------------------*/

#if ( configUSE_APPLICATION_TASK_TAG == 1 )

/*******************************************************************************
 * 函数名称：xTaskCallApplicationTaskHook
 * 功能描述：调用应用程序任务钩子函数，允许应用程序为特定任务注册自定义回调函数
 *           此函数提供了一种机制，允许应用程序为任务注册自定义的回调函数
 * 输入参数：
 *   - xTask: 任务句柄，指定要调用钩子函数的任务
 *     特殊值：传入NULL表示调用当前任务的钩子函数
 *   - pvParameter: 传递给钩子函数的参数
 * 输出参数：无
 * 返 回 值：
 *   - BaseType_t: 钩子函数的返回值，如果没有钩子函数则返回pdFAIL
 * 其它说明：
 *   - 此函数仅在启用应用程序任务标签功能(configUSE_APPLICATION_TASK_TAG == 1)时编译
 *   - 提供了一种扩展机制，允许应用程序为任务注册自定义的回调函数
 *   - 可以用于实现任务特定的功能扩展或监控
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
BaseType_t xTaskCallApplicationTaskHook( TaskHandle_t xTask, void *pvParameter )
{
    TCB_t *xTCB;        /* 指向任务控制块的指针 */
    BaseType_t xReturn; /* 返回值：钩子函数的返回值 */

    /* 如果xTask为NULL，则调用当前任务的钩子函数 */
    if( xTask == NULL )
    {
        xTCB = ( TCB_t * ) pxCurrentTCB;
    }
    else
    {
        xTCB = ( TCB_t * ) xTask;
    }

    /* 检查任务是否注册了钩子函数 */
    if( xTCB->pxTaskTag != NULL )
    {
        /* 调用任务的钩子函数并传递参数 */
        xReturn = xTCB->pxTaskTag( pvParameter );
    }
    else
    {
        /* 任务没有注册钩子函数，返回失败 */
        xReturn = pdFAIL;
    }

    /* 返回钩子函数的返回值或失败状态 */
    return xReturn;
}

#endif /* configUSE_APPLICATION_TASK_TAG */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vTaskSwitchContext
 * 功能描述：执行任务上下文切换，选择并切换到最高优先级的就绪任务
 *           此函数是FreeRTOS调度器的核心，负责任务的切换和管理
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数是FreeRTOS调度器的核心实现，负责任务的上下文切换
 *   - 处理调度器挂起状态，防止在挂起时进行上下文切换
 *   - 支持运行时统计、栈溢出检查和新库重入等功能
 *   - 根据配置选择通用C代码或端口优化的汇编代码进行任务选择
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
void vTaskSwitchContext( void )
{
    /* 检查调度器是否被挂起 */
    if( uxSchedulerSuspended != ( UBaseType_t ) pdFALSE )
    {
        /* 调度器当前被挂起 - 不允许上下文切换 */
        xYieldPending = pdTRUE;
    }
    else
    {
        /* 调度器未挂起，可以执行上下文切换 */
        xYieldPending = pdFALSE;
        
        /* 跟踪任务切换出事件 */
        traceTASK_SWITCHED_OUT();

        /* 如果启用了运行时统计功能 */
        #if ( configGENERATE_RUN_TIME_STATS == 1 )
        {
            /* 获取当前运行时间计数器值 */
            #ifdef portALT_GET_RUN_TIME_COUNTER_VALUE
                portALT_GET_RUN_TIME_COUNTER_VALUE( ulTotalRunTime );
            #else
                ulTotalRunTime = portGET_RUN_TIME_COUNTER_VALUE();
            #endif

            /* 将任务运行的时间量添加到累计时间中。
               任务开始运行的时间存储在ulTaskSwitchedInTime中。
               注意：这里没有溢出保护，因此计数值仅在定时器溢出之前有效。
               对负值的防护是为了防止可疑的运行时统计计数器实现 -
               这些实现由应用程序提供，而不是内核 */
            if( ulTotalRunTime > ulTaskSwitchedInTime )
            {
                /* 计算任务运行时间并添加到运行时间计数器 */
                pxCurrentTCB->ulRunTimeCounter += ( ulTotalRunTime - ulTaskSwitchedInTime );
            }
            else
            {
                /* 时间值异常：添加测试覆盖率标记 */
                mtCOVERAGE_TEST_MARKER();
            }
            
            /* 更新任务切换时间点为当前时间 */
            ulTaskSwitchedInTime = ulTotalRunTime;
        }
        #endif /* configGENERATE_RUN_TIME_STATS */

        /* 检查栈溢出（如果配置了） */
        taskCHECK_FOR_STACK_OVERFLOW();

        /* 使用通用C代码或端口优化的汇编代码选择要运行的新任务 */
        taskSELECT_HIGHEST_PRIORITY_TASK();
        
        /* 跟踪任务切换入事件 */
        traceTASK_SWITCHED_IN();

        /* 如果使用NewLib且配置为可重入 */
        #if ( configUSE_NEWLIB_REENTRANT == 1 )
        {
            /* 切换Newlib的_impure_ptr变量，指向此任务特定的_reent结构 */
            _impure_ptr = &( pxCurrentTCB->xNewLib_reent );
        }
        #endif /* configUSE_NEWLIB_REENTRANT */
    }
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vTaskPlaceOnEventList
 * 功能描述：将当前任务放置到事件列表中，并设置等待时间
 *           此函数用于将任务按优先级顺序插入事件列表，并添加到延迟列表实现超时
 * 输入参数：
 *   - pxEventList: 指向事件列表的指针，任务将被按优先级顺序插入此列表
 *   - xTicksToWait: 等待的超时时间（以时钟节拍为单位）
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数必须在中断禁用或调度器挂起且队列被锁定的情况下调用
 *   - 任务按优先级顺序插入事件列表，确保最高优先级任务最先被事件唤醒
 *   - 包含事件列表的队列被锁定，防止中断同时访问
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
void vTaskPlaceOnEventList( List_t * const pxEventList, const TickType_t xTicksToWait )
{
    /* 断言检查事件列表指针有效性 */
    configASSERT( pxEventList );

    /* 此函数必须在中断禁用或调度器挂起且队列被锁定的情况下调用 */

    /* 将TCB的事件列表项放置在适当的事件列表中。
       按优先级顺序放置在列表中，因此最高优先级的任务将首先被事件唤醒。
       包含事件列表的队列被锁定，防止中断同时访问 */
    vListInsert( pxEventList, &( pxCurrentTCB->xEventListItem ) );

    /* 将当前任务添加到延迟列表，实现超时机制 */
    prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vTaskPlaceOnUnorderedEventList
 * 功能描述：将当前任务放置到无序事件列表中，并设置事件项值和等待时间
 *           此函数在调度器挂起时调用，用于事件组实现中的任务阻塞操作
 * 输入参数：
 *   - pxEventList: 指向事件列表的指针，任务将被放置到此列表中
 *   - xItemValue: 要设置的事件列表项值，通常包含事件标志信息
 *   - xTicksToWait: 等待的超时时间（以时钟节拍为单位）
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数必须在调度器挂起的情况下调用，用于事件组实现
 *   - 事件列表是无序的，不按优先级排序
 *   - 设置事件列表项值并标记为正在使用
 *   - 将任务添加到延迟列表，实现超时机制
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
void vTaskPlaceOnUnorderedEventList( List_t * pxEventList, const TickType_t xItemValue, const TickType_t xTicksToWait )
{
    /* 断言检查事件列表指针有效性 */
    configASSERT( pxEventList );

    /* 此函数必须在调度器挂起的情况下调用。用于事件组实现 */
    configASSERT( uxSchedulerSuspended != 0 );

    /* 在事件列表项中存储项值。这里安全地访问事件列表项，
       因为中断不会访问不在阻塞状态的任务的事件列表项 */
    listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xEventListItem ), xItemValue | taskEVENT_LIST_ITEM_VALUE_IN_USE );

    /* 将TCB的事件列表项放置在适当的事件列表的末尾。
       这里安全地访问事件列表，因为它是事件组实现的一部分 -
       中断不会直接访问事件组（而是通过将函数调用挂起到任务级别来间接访问） */
    vListInsertEnd( pxEventList, &( pxCurrentTCB->xEventListItem ) );

    /* 将当前任务添加到延迟列表，实现超时机制 */
    prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
}
/*-----------------------------------------------------------*/

#if( configUSE_TIMERS == 1 )

/*******************************************************************************
 * 函数名称：vTaskPlaceOnEventListRestricted
 * 功能描述：将当前任务放置到受限事件列表中，并设置等待时间或无限期等待
 *           此函数是内核代码专用的受限函数，不应由应用程序代码调用
 * 输入参数：
 *   - pxEventList: 指向事件列表的指针，任务将被放置到此列表中
 *   - xTicksToWait: 等待的超时时间（以时钟节拍为单位）
 *   - xWaitIndefinitely: 是否无限期等待的标志
 *     pdTRUE: 无限期等待，忽略xTicksToWait参数
 *     pdFALSE: 使用xTicksToWait参数指定的超时时间
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数是内核代码专用的受限函数，不应由应用程序代码调用
 *   - 需要在调度器挂起的情况下调用
 *   - 假设只有一个任务等待此事件列表，因此使用vListInsertEnd代替vListInsert
 *   - 主要用于定时器实现
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
void vTaskPlaceOnEventListRestricted( List_t * const pxEventList, TickType_t xTicksToWait, const BaseType_t xWaitIndefinitely )
{
    /* 断言检查事件列表指针有效性 */
    configASSERT( pxEventList );

    /* 此函数不应由应用程序代码调用，因此名称中包含"Restricted"。
       它不是公共API的一部分。它专为内核代码使用而设计，
       并且有特殊的调用要求 - 应在调度器挂起的情况下调用 */

    /* 将TCB的事件列表项放置在适当的事件列表中。
       在这种情况下，假设这是唯一等待此事件列表的任务，
       因此可以使用更快的vListInsertEnd()函数代替vListInsert */
    vListInsertEnd( pxEventList, &( pxCurrentTCB->xEventListItem ) );

    /* 如果任务应该无限期阻塞，则将阻塞时间设置为一个值，
       该值将在prvAddCurrentTaskToDelayedList()函数中被识别为无限延迟 */
    if( xWaitIndefinitely != pdFALSE )
    {
        xTicksToWait = portMAX_DELAY;
    }

    /* 跟踪任务延迟直到指定时间点 */
    traceTASK_DELAY_UNTIL( ( xTickCount + xTicksToWait ) );
    
    /* 将当前任务添加到延迟列表 */
    prvAddCurrentTaskToDelayedList( xTicksToWait, xWaitIndefinitely );
}

#endif /* configUSE_TIMERS */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xTaskRemoveFromEventList
 * 功能描述：从事件列表中移除优先级最高的任务并将其添加到就绪列表
 *           此函数在临界区内调用，用于处理事件相关的任务解除阻塞操作
 * 输入参数：
 *   - pxEventList: 指向事件列表的指针，该列表按优先级排序
 * 输出参数：无
 * 返 回 值：
 *   - BaseType_t: 指示是否需要立即进行上下文切换
 *     pdTRUE: 被移除的任务优先级高于当前任务，需要立即上下文切换
 *     pdFALSE: 被移除的任务优先级不高于当前任务，不需要立即上下文切换
 * 其它说明：
 *   - 此函数必须在临界区内调用，也可以从中断服务程序的临界区内调用
 *   - 事件列表按优先级排序，因此可以安全地移除第一个任务（优先级最高）
 *   - 根据调度器状态决定是立即将任务添加到就绪列表还是暂挂到待就绪列表
 *   - 如果启用了无空闲滴答模式，会重置下一个任务解除阻塞时间
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
BaseType_t xTaskRemoveFromEventList( const List_t * const pxEventList )
{
    TCB_t *pxUnblockedTCB;  /* 指向被解除阻塞的任务控制块 */
    BaseType_t xReturn;     /* 返回值：是否需要立即上下文切换 */

    /* 此函数必须在临界区内调用。也可以从中断服务程序的临界区内调用 */

    /* 事件列表按优先级排序，因此可以移除列表中的第一个任务，
       因为它已知是最高优先级的。将TCB从延迟列表中移除，
       并将其添加到就绪列表。

       如果事件是针对被锁定的队列，则永远不会调用此函数 -
       队列上的锁定计数将被修改。这意味着这里保证对事件列表的独占访问。

       此函数假定已经进行了检查以确保pxEventList不为空 */
    pxUnblockedTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxEventList );
    configASSERT( pxUnblockedTCB );
    ( void ) uxListRemove( &( pxUnblockedTCB->xEventListItem ) );

    /* 检查调度器是否未挂起 */
    if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
    {
        /* 调度器未挂起：将任务从状态列表中移除并添加到就绪列表 */
        ( void ) uxListRemove( &( pxUnblockedTCB->xStateListItem ) );
        prvAddTaskToReadyList( pxUnblockedTCB );
    }
    else
    {
        /* 调度器已挂起：无法访问延迟和就绪列表，
           因此将此任务暂挂直到调度器恢复 */
        vListInsertEnd( &( xPendingReadyList ), &( pxUnblockedTCB->xEventListItem ) );
    }

    /* 检查被解除阻塞的任务优先级是否高于当前任务 */
    if( pxUnblockedTCB->uxPriority > pxCurrentTCB->uxPriority )
    {
        /* 如果从事件列表中移除的任务优先级高于调用任务，则返回true。
           这允许调用任务知道是否应该立即强制上下文切换 */
        xReturn = pdTRUE;

        /* 标记有yield挂起，以防用户没有使用ISR安全的FreeRTOS函数中的
           "xHigherPriorityTaskWoken"参数 */
        xYieldPending = pdTRUE;
    }
    else
    {
        xReturn = pdFALSE;
    }

    /* 如果启用了无空闲滴答模式 */
    #if( configUSE_TICKLESS_IDLE != 0 )
    {
        /* 如果任务在内核对象上阻塞，则xNextTaskUnblockTime可能设置为
           被阻塞任务的超时时间。如果任务因超时以外的原因解除阻塞，
           xNextTaskUnblockTime通常保持不变，因为当滴答计数等于
           xNextTaskUnblockTime时会自动重置为新值。但是，如果使用
           无空闲滴答模式，可能更重要的是在尽可能早的时间进入睡眠模式 -
           因此在这里重置xNextTaskUnblockTime以确保在尽可能早的时间更新 */
        prvResetNextTaskUnblockTime();
    }
    #endif

    return xReturn;
}
/*-----------------------------------------------------------*/
/*******************************************************************************
 * 函数名称：xTaskRemoveFromUnorderedEventList
 * 功能描述：从无序事件列表中移除任务并将其添加到就绪列表，用于事件标志实现
 *           此函数在调度器挂起时调用，用于处理事件标志相关的任务解除阻塞操作
 * 输入参数：
 *   - pxEventListItem: 指向事件列表项的指针，表示要移除的事件列表项
 *   - xItemValue: 要设置的事件列表项值，通常包含事件标志信息
 * 输出参数：无
 * 返 回 值：
 *   - BaseType_t: 指示是否需要立即进行上下文切换
 *     pdTRUE: 被移除的任务优先级高于当前任务，需要立即上下文切换
 *     pdFALSE: 被移除的任务优先级不高于当前任务，不需要立即上下文切换
 * 其它说明：
 *   - 此函数必须在调度器挂起的情况下调用，用于事件标志实现
 *   - 操作涉及多个列表的修改，需要确保原子性
 *   - 如果被解除阻塞的任务优先级更高，会设置yield挂起标志
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
BaseType_t xTaskRemoveFromUnorderedEventList( ListItem_t * pxEventListItem, const TickType_t xItemValue )
{
    TCB_t *pxUnblockedTCB;  /* 指向被解除阻塞的任务控制块 */
    BaseType_t xReturn;     /* 返回值：是否需要立即上下文切换 */

    /* 此函数必须在调度器挂起的情况下调用。用于事件标志实现 */
    configASSERT( uxSchedulerSuspended != pdFALSE );

    /* 在事件列表中存储新的项值，并标记为正在使用 */
    listSET_LIST_ITEM_VALUE( pxEventListItem, xItemValue | taskEVENT_LIST_ITEM_VALUE_IN_USE );

    /* 从事件标志中移除事件列表。中断不会访问事件标志 */
    pxUnblockedTCB = ( TCB_t * ) listGET_LIST_ITEM_OWNER( pxEventListItem );
    configASSERT( pxUnblockedTCB );
    ( void ) uxListRemove( pxEventListItem );

    /* 将任务从延迟列表中移除并添加到就绪列表。调度器已挂起，
       因此中断不会访问就绪列表 */
    ( void ) uxListRemove( &( pxUnblockedTCB->xStateListItem ) );
    prvAddTaskToReadyList( pxUnblockedTCB );

    /* 检查被解除阻塞的任务优先级是否高于当前任务 */
    if( pxUnblockedTCB->uxPriority > pxCurrentTCB->uxPriority )
    {
        /* 如果从事件列表中移除的任务优先级高于调用任务，则返回true。
           这允许调用任务知道是否应该立即强制上下文切换 */
        xReturn = pdTRUE;

        /* 标记有yield挂起，以防用户没有使用ISR安全的FreeRTOS函数中的
           "xHigherPriorityTaskWoken"参数 */
        xYieldPending = pdTRUE;
    }
    else
    {
        xReturn = pdFALSE;
    }

    return xReturn;
	}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vTaskSetTimeOutState
 * 功能描述：设置超时状态结构，记录当前时间点和滴答计数器溢出次数
 *           此函数用于初始化或重置超时状态，为后续的超时检查提供基准时间
 * 输入参数：
 *   - pxTimeOut: 指向TimeOut_t结构的指针，用于存储超时状态信息
 * 输出参数：
 *   - pxTimeOut: 被初始化的超时状态结构，包含当前时间点和溢出计数
 * 返 回 值：无
 * 其它说明：
 *   - 此函数记录当前滴答计数和溢出次数，作为超时计算的起始点
 *   - 通常与xTaskCheckForTimeOut配合使用，实现可中断的阻塞操作
 *   - 需要在临界区外调用，因为它只是简单记录当前状态
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
void vTaskSetTimeOutState( TimeOut_t * const pxTimeOut )
{
    /* 参数断言检查，确保指针有效 */
    configASSERT( pxTimeOut );
    
    /* 记录当前的滴答计数器溢出次数 */
    pxTimeOut->xOverflowCount = xNumOfOverflows;
    
    /* 记录当前的滴答计数值 */
    pxTimeOut->xTimeOnEntering = xTickCount;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xTaskCheckForTimeOut
 * 功能描述：检查超时状态并更新剩余等待时间，用于实现可中断的阻塞操作
 *           此函数处理任务阻塞时的超时检查，支持延迟中止和无限等待特殊情况
 * 输入参数：
 *   - pxTimeOut: 指向TimeOut_t结构的指针，包含超时状态信息
 *   - pxTicksToWait: 指向剩余等待时间的指针，函数会更新此值
 * 输出参数：
 *   - pxTicksToWait: 更新的剩余等待时间（如果没有超时）
 * 返 回 值：
 *   - BaseType_t: 超时状态
 *     pdTRUE: 已超时或延迟被中止
 *     pdFALSE: 未超时，剩余等待时间已更新
 * 其它说明：
 *   - 此函数在临界区内执行，确保超时检查的原子性
 *   - 处理滴答计数器溢出的特殊情况
 *   - 支持延迟中止和无限等待功能（如果启用）
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
BaseType_t xTaskCheckForTimeOut( TimeOut_t * const pxTimeOut, TickType_t * const pxTicksToWait )
{
    BaseType_t xReturn;  /* 返回值：超时状态 */

    /* 参数断言检查，确保指针有效 */
    configASSERT( pxTimeOut );
    configASSERT( pxTicksToWait );

    /* 进入临界区，确保超时检查的原子性 */
    taskENTER_CRITICAL();
    {
        /* 小优化：在此块内滴答计数不会改变，使用常量保存当前滴答计数 */
        const TickType_t xConstTickCount = xTickCount;

        /* 如果启用了延迟中止功能 */
        #if( INCLUDE_xTaskAbortDelay == 1 )
            if( pxCurrentTCB->ucDelayAborted != pdFALSE )
            {
                /* 延迟被中止，这与超时不同，但结果相同 */
                pxCurrentTCB->ucDelayAborted = pdFALSE;  /* 清除中止标志 */
                xReturn = pdTRUE;  /* 返回超时状态 */
            }
            else
        #endif

        /* 如果启用了任务挂起功能 */
        #if ( INCLUDE_vTaskSuspend == 1 )
            if( *pxTicksToWait == portMAX_DELAY )
            {
                /* 如果启用了vTaskSuspend且指定的阻塞时间是最大阻塞时间，
                   则任务应无限期阻塞，因此永远不会超时 */
                xReturn = pdFALSE;  /* 返回未超时状态 */
            }
            else
        #endif

        /* 检查滴答计数器是否溢出且当前时间大于等于进入时间 */
        if( ( xNumOfOverflows != pxTimeOut->xOverflowCount ) && ( xConstTickCount >= pxTimeOut->xTimeOnEntering ) ) /*lint !e525 Indentation preferred as is to make code within pre-processor directives clearer. */
        {
            /* 滴答计数大于调用vTaskSetTimeout()时的时间，并且自调用vTaskSetTimeOut()以来
               已经溢出。它必须已经环绕了一圈并再次经过。这表明自调用vTaskSetTimeout()以来
               已经超时 */
            xReturn = pdTRUE;  /* 返回超时状态 */
        }
        /* 检查是否尚未超时 */
        else if( ( ( TickType_t ) ( xConstTickCount - pxTimeOut->xTimeOnEntering ) ) < *pxTicksToWait ) /*lint !e961 Explicit casting is only redundant with some compilers, whereas others require it to prevent integer conversion errors. */
        {
            /* 不是真正的超时。调整剩余时间参数 */
            *pxTicksToWait -= ( xConstTickCount - pxTimeOut->xTimeOnEntering );  /* 更新剩余等待时间 */
            vTaskSetTimeOutState( pxTimeOut );  /* 重置超时状态 */
            xReturn = pdFALSE;  /* 返回未超时状态 */
        }
        else
        {
            /* 其他情况（已超时） */
            xReturn = pdTRUE;  /* 返回超时状态 */
        }
    }
    /* 退出临界区 */
    taskEXIT_CRITICAL();

    /* 返回超时状态 */
    return xReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vTaskMissedYield
 * 功能描述：设置Yield挂起标志，指示在中断服务程序中发生了需要任务切换的情况
 *           此函数通常在中断服务程序(ISR)中调用，用于延迟任务切换操作
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数设置全局标志xYieldPending，指示需要延迟执行任务切换
 *   - 实际的任务切换将在适当的时机（如退出临界区）由调度器执行
 *   - 用于在ISR中安全地请求任务切换，避免在ISR中直接进行上下文切换
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
void vTaskMissedYield( void )
{
    /* 设置Yield挂起标志，指示需要延迟执行任务切换 */
    xYieldPending = pdTRUE;
}
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

/*******************************************************************************
 * 函数名称：uxTaskGetTaskNumber
 * 功能描述：获取指定任务的任务编号，用于任务标识和跟踪
 *           任务编号是任务的唯一标识符，可用于调试和跟踪目的
 * 输入参数：
 *   - xTask: 要获取任务编号的任务句柄
 *     特殊值：传入NULL将返回0
 * 输出参数：无
 * 返 回 值：
 *   - UBaseType_t: 任务的编号，如果任务句柄无效则返回0
 * 其它说明：
 *   - 此函数仅在启用跟踪功能(configUSE_TRACE_FACILITY == 1)时编译
 *   - 任务编号是任务的唯一标识符，不同于任务优先级
 *   - 可用于调试、跟踪和性能分析等场景
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
UBaseType_t uxTaskGetTaskNumber( TaskHandle_t xTask )
{
    UBaseType_t uxReturn;  /* 返回值：任务编号 */
    TCB_t *pxTCB;          /* 指向任务控制块的指针 */

    /* 检查任务句柄是否有效 */
    if( xTask != NULL )
    {
        /* 将任务句柄转换为任务控制块指针 */
        pxTCB = ( TCB_t * ) xTask;
        /* 获取任务编号 */
        uxReturn = pxTCB->uxTaskNumber;
    }
    else
    {
        /* 任务句柄无效，返回0 */
        uxReturn = 0U;
    }

    /* 返回任务编号 */
    return uxReturn;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

/*******************************************************************************
 * 函数名称：vTaskSetTaskNumber
 * 功能描述：设置指定任务的任务编号，用于任务标识和跟踪
 *           任务编号是任务的唯一标识符，可用于调试和跟踪目的
 * 输入参数：
 *   - xTask: 要设置任务编号的任务句柄
 *     特殊值：传入NULL将不执行任何操作
 *   - uxHandle: 要设置的任务编号值
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数仅在启用跟踪功能(configUSE_TRACE_FACILITY == 1)时编译
 *   - 任务编号是任务的唯一标识符，不同于任务优先级
 *   - 可用于调试、跟踪和性能分析等场景
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
void vTaskSetTaskNumber( TaskHandle_t xTask, const UBaseType_t uxHandle )
{
    TCB_t *pxTCB;  /* 指向任务控制块的指针 */

    /* 检查任务句柄是否有效 */
    if( xTask != NULL )
    {
        /* 将任务句柄转换为任务控制块指针 */
        pxTCB = ( TCB_t * ) xTask;
        /* 设置任务编号 */
        pxTCB->uxTaskNumber = uxHandle;
    }
    /* 如果任务句柄无效，静默失败（不执行任何操作） */
}

#endif /* configUSE_TRACE_FACILITY */

/*******************************************************************************
 * 函数名称：prvIdleTask
 * 功能描述：FreeRTOS空闲任务，是系统自动创建的低优先级任务，在系统没有其他任务运行时执行
 *           负责清理已终止的任务、执行低功耗处理、调用用户空闲钩子函数等系统维护工作
 * 输入参数：
 *   - pvParameters: 任务参数（未使用，仅为消除编译器警告）
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此任务是RTOS自动创建的空闲任务，在调度器启动时创建
 *   - 优先级为最低优先级（tskIDLE_PRIORITY）
 *   - 在系统没有其他就绪任务时运行
 *   - 包含任务清理、低功耗处理和用户钩子函数调用等功能
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
static portTASK_FUNCTION( prvIdleTask, pvParameters )
{
    /* 消除未使用参数的编译器警告 */
    ( void ) pvParameters;

    /** 这是RTOS空闲任务 - 在调度器启动时自动创建 **/

    /* 无限循环，持续执行空闲任务的工作 */
    for( ;; )
    {
        /* 检查是否有任务已删除自身 - 如果有，空闲任务负责释放已删除任务的TCB和栈 */
        prvCheckTasksWaitingTermination();

        /* 如果未使用抢占式调度，需要主动让出CPU */
        #if ( configUSE_PREEMPTION == 0 )
        {
            /* 如果不使用抢占式调度，我们需要强制任务切换，
               检查是否有其他任务变为可用。如果使用抢占式调度，
               则不需要这样做，因为任何可用的任务都会自动获得处理器 */
            taskYIELD();
        }
        #endif /* configUSE_PREEMPTION */

        /* 如果使用抢占式调度且配置了空闲任务让出 */
        #if ( ( configUSE_PREEMPTION == 1 ) && ( configIDLE_SHOULD_YIELD == 1 ) )
        {
            /* 使用抢占式调度时，相同优先级的任务会进行时间片轮转。
               如果共享空闲优先级的任务准备运行，空闲任务应在时间片结束前让出CPU。

               这里不需要临界区，因为我们只是从列表中读取数据，
               偶尔的错误值不会产生影响。如果就绪列表中空闲优先级
               的任务数量大于1，则表示有空闲任务以外的任务准备执行 */
            if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ tskIDLE_PRIORITY ] ) ) > ( UBaseType_t ) 1 )
            {
                taskYIELD();  /* 让出CPU */
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
            }
        }
        #endif /* ( ( configUSE_PREEMPTION == 1 ) && ( configIDLE_SHOULD_YIELD == 1 ) ) */

        /* 如果启用了空闲钩子函数 */
        #if ( configUSE_IDLE_HOOK == 1 )
        {
            /* 声明外部定义的空闲钩子函数 */
            extern void vApplicationIdleHook( void );

            /* 在空闲任务中调用用户定义的函数。这允许应用程序设计者添加后台功能，
               而无需额外任务的开销。
               注意：vApplicationIdleHook()在任何情况下都不得调用可能阻塞的函数 */
            vApplicationIdleHook();
        }
        #endif /* configUSE_IDLE_HOOK */

        /* 此条件编译应使用不等于0，而不是等于1。这是为了确保当用户定义的低功耗模式
           实现需要将configUSE_TICKLESS_IDLE设置为1以外的值时，也能调用
           portSUPPRESS_TICKS_AND_SLEEP() */
        #if ( configUSE_TICKLESS_IDLE != 0 )
        {
        TickType_t xExpectedIdleTime;  /* 预期空闲时间 */

            /* 不希望每次空闲任务迭代都挂起然后恢复调度器。
               因此，先在不挂起调度器的情况下进行预期空闲时间的初步测试。
               这里的结果不一定有效 */
            xExpectedIdleTime = prvGetExpectedIdleTime();

            if( xExpectedIdleTime >= configEXPECTED_IDLE_TIME_BEFORE_SLEEP )
            {
                /* 挂起所有任务 */
                vTaskSuspendAll();
                {
                    /* 现在调度器已挂起，可以再次采样预期空闲时间，
                       这次可以使用其值 */
                    configASSERT( xNextTaskUnblockTime >= xTickCount );
                    xExpectedIdleTime = prvGetExpectedIdleTime();

                    if( xExpectedIdleTime >= configEXPECTED_IDLE_TIME_BEFORE_SLEEP )
                    {
                        /* 跟踪低功耗空闲开始 */
                        traceLOW_POWER_IDLE_BEGIN();
                        /* 抑制时钟滴答并进入睡眠 */
                        portSUPPRESS_TICKS_AND_SLEEP( xExpectedIdleTime );
                        /* 跟踪低功耗空闲结束 */
                        traceLOW_POWER_IDLE_END();
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
                    }
                }
                /* 恢复所有任务 */
                ( void ) xTaskResumeAll();
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
            }
        }
        #endif /* configUSE_TICKLESS_IDLE */
    }
}
/*-----------------------------------------------------------*/

#if( configUSE_TICKLESS_IDLE != 0 )

	eSleepModeStatus eTaskConfirmSleepModeStatus( void )
	{
	/* The idle task exists in addition to the application tasks. */
	const UBaseType_t uxNonApplicationTasks = 1;
	eSleepModeStatus eReturn = eStandardSleep;

		if( listCURRENT_LIST_LENGTH( &xPendingReadyList ) != 0 )
		{
			/* A task was made ready while the scheduler was suspended. */
			eReturn = eAbortSleep;
		}
		else if( xYieldPending != pdFALSE )
		{
			/* A yield was pended while the scheduler was suspended. */
			eReturn = eAbortSleep;
		}
		else
		{
			/* If all the tasks are in the suspended list (which might mean they
			have an infinite block time rather than actually being suspended)
			then it is safe to turn all clocks off and just wait for external
			interrupts. */
			if( listCURRENT_LIST_LENGTH( &xSuspendedTaskList ) == ( uxCurrentNumberOfTasks - uxNonApplicationTasks ) )
			{
				eReturn = eNoTasksWaitingTimeout;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}

		return eReturn;
	}

#endif /* configUSE_TICKLESS_IDLE */
/*-----------------------------------------------------------*/

#if ( configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0 )

/*******************************************************************************
 * 函数名称：vTaskSetThreadLocalStoragePointer
 * 功能描述：设置指定任务的线程本地存储(TLS)指针，用于存储任务特定的数据
 *           此函数提供了一种机制，允许每个任务拥有自己的数据存储空间
 * 输入参数：
 *   - xTaskToSet: 要设置的任务句柄
 *     特殊值：传入NULL表示设置当前任务的TLS指针
 *   - xIndex: TLS指针的索引，范围从0到configNUM_THREAD_LOCAL_STORAGE_POINTERS-1
 *   - pvValue: 要设置的指针值，可以是任意类型的数据指针
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数仅在配置了线程本地存储指针(configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0)时编译
 *   - 提供了一种任务特定的数据存储机制，类似于线程本地存储(TLS)的概念
 *   - 每个任务可以有多个TLS指针，通过索引区分
 *   - 如果索引无效，函数不会执行任何操作（静默失败）
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
void vTaskSetThreadLocalStoragePointer( TaskHandle_t xTaskToSet, BaseType_t xIndex, void *pvValue )
{
    TCB_t *pxTCB;  /* 指向任务控制块的指针 */

    /* 检查索引是否在有效范围内 */
    if( xIndex < configNUM_THREAD_LOCAL_STORAGE_POINTERS )
    {
        /* 根据任务句柄获取任务控制块 */
        pxTCB = prvGetTCBFromHandle( xTaskToSet );
        /* 设置指定索引的TLS指针值 */
        pxTCB->pvThreadLocalStoragePointers[ xIndex ] = pvValue;
    }
    /* 如果索引超出范围，静默失败（不执行任何操作） */
}

#endif /* configNUM_THREAD_LOCAL_STORAGE_POINTERS */
/*-----------------------------------------------------------*/

#if ( configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0 )

/*******************************************************************************
 * 函数名称：pvTaskGetThreadLocalStoragePointer
 * 功能描述：获取指定任务的线程本地存储(TLS)指针，用于访问任务特定的数据存储
 *           此函数提供了一种机制，允许每个任务拥有自己的数据存储空间
 * 输入参数：
 *   - xTaskToQuery: 要查询的任务句柄
 *     特殊值：传入NULL表示查询当前任务的TLS指针
 *   - xIndex: TLS指针的索引，范围从0到configNUM_THREAD_LOCAL_STORAGE_POINTERS-1
 * 输出参数：无
 * 返 回 值：
 *   - void*: 指向线程本地存储数据的指针，如果索引无效则返回NULL
 * 其它说明：
 *   - 此函数仅在配置了线程本地存储指针(configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0)时编译
 *   - 提供了一种任务特定的数据存储机制，类似于线程本地存储(TLS)的概念
 *   - 每个任务可以有多个TLS指针，通过索引区分
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
void *pvTaskGetThreadLocalStoragePointer( TaskHandle_t xTaskToQuery, BaseType_t xIndex )
{
    void *pvReturn = NULL;  /* 返回值：TLS指针，初始化为NULL */
    TCB_t *pxTCB;           /* 指向任务控制块的指针 */

    /* 检查索引是否在有效范围内 */
    if( xIndex < configNUM_THREAD_LOCAL_STORAGE_POINTERS )
    {
        /* 根据任务句柄获取任务控制块 */
        pxTCB = prvGetTCBFromHandle( xTaskToQuery );
        /* 获取指定索引的TLS指针 */
        pvReturn = pxTCB->pvThreadLocalStoragePointers[ xIndex ];
    }
    else
    {
        /* 索引超出范围，返回NULL */
        pvReturn = NULL;
    }

    /* 返回TLS指针 */
    return pvReturn;
}

#endif /* configNUM_THREAD_LOCAL_STORAGE_POINTERS */
/*-----------------------------------------------------------*/

#if ( portUSING_MPU_WRAPPERS == 1 )

/*******************************************************************************
 * 函数名称：vTaskAllocateMPURegions
 * 功能描述：为指定任务分配MPU（内存保护单元）区域，配置任务的内存访问权限
 *           此函数允许动态修改任务的MPU设置，提供内存保护功能
 * 输入参数：
 *   - xTaskToModify: 要修改MPU设置的任务句柄
 *     特殊值：传入NULL表示修改调用任务的MPU设置
 *   - xRegions: 指向MPU区域配置数组的指针，包含内存区域的地址、大小和权限设置
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数仅在启用MPU包装器(portUSING_MPU_WRAPPERS == 1)时编译
 *   - 用于动态配置任务的内存保护区域，增强系统的安全性和稳定性
 *   - 必须在MPU支持的硬件平台上使用
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
void vTaskAllocateMPURegions( TaskHandle_t xTaskToModify, const MemoryRegion_t * const xRegions )
{
    TCB_t *pxTCB;  /* 指向任务控制块的指针 */

    /* 如果传入NULL，则修改调用任务的MPU设置 */
    pxTCB = prvGetTCBFromHandle( xTaskToModify );

    /* 存储任务的MPU设置到任务控制块中 */
    vPortStoreTaskMPUSettings( &( pxTCB->xMPUSettings ),  /* 目标MPU设置结构 */
                              xRegions,                   /* MPU区域配置数组 */
                              NULL,                       /* 可选的额外参数（未使用） */
                              0 );                        /* 额外参数的大小（未使用） */
}

#endif /* portUSING_MPU_WRAPPERS */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvInitialiseTaskLists
 * 功能描述：初始化FreeRTOS中的所有任务列表，包括就绪列表、延迟列表、挂起列表等
 *           此函数在调度器启动前调用，为任务管理准备所需的数据结构
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数在FreeRTOS内核初始化过程中调用，为任务调度准备必要的列表结构
 *   - 根据配置选项初始化不同的任务列表（如删除、挂起等功能相关的列表）
 *   - 设置延迟任务列表和溢出延迟任务列表的初始指向
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
static void prvInitialiseTaskLists( void )
{
    UBaseType_t uxPriority;  /* 循环计数器：用于遍历所有优先级 */

    /* 初始化所有优先级的就绪任务列表 */
    for( uxPriority = ( UBaseType_t ) 0U; uxPriority < ( UBaseType_t ) configMAX_PRIORITIES; uxPriority++ )
    {
        /* 初始化当前优先级的就绪任务列表 */
        vListInitialise( &( pxReadyTasksLists[ uxPriority ] ) );
    }

    /* 初始化两个延迟任务列表（用于实现时间片轮转） */
    vListInitialise( &xDelayedTaskList1 );  /* 初始化第一个延迟任务列表 */
    vListInitialise( &xDelayedTaskList2 );  /* 初始化第二个延迟任务列表 */
    
    /* 初始化待就绪任务列表（用于存放等待被设置为就绪状态的任务） */
    vListInitialise( &xPendingReadyList );  /* 初始化待就绪任务列表 */

    /* 如果启用了任务删除功能，初始化等待终止的任务列表 */
    #if ( INCLUDE_vTaskDelete == 1 )
    {
        /* 初始化等待终止的任务列表（存放已删除但尚未清理的任务） */
        vListInitialise( &xTasksWaitingTermination );
    }
    #endif /* INCLUDE_vTaskDelete */

    /* 如果启用了任务挂起功能，初始化挂起的任务列表 */
    #if ( INCLUDE_vTaskSuspend == 1 )
    {
        /* 初始化挂起的任务列表（存放被挂起的任务） */
        vListInitialise( &xSuspendedTaskList );
    }
    #endif /* INCLUDE_vTaskSuspend */

    /* 初始设置：pxDelayedTaskList使用list1，pxOverflowDelayedTaskList使用list2 */
    pxDelayedTaskList = &xDelayedTaskList1;          /* 设置当前延迟任务列表指向list1 */
    pxOverflowDelayedTaskList = &xDelayedTaskList2;  /* 设置溢出延迟任务列表指向list2 */
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvCheckTasksWaitingTermination
 * 功能描述：检查并清理等待终止的任务，从空闲任务中调用以安全删除已终止的任务
 *           此函数负责清理已删除但尚未释放资源的任务，防止资源泄漏
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数从RTOS空闲任务中调用，确保在系统空闲时执行清理操作
 *   - 仅在启用任务删除功能(INCLUDE_vTaskDelete == 1)时编译
 *   - 使用任务挂起机制确保列表访问的原子性
 *   - 通过计数器优化防止频繁挂起调度器
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
static void prvCheckTasksWaitingTermination( void )
{
    /** 此函数从RTOS空闲任务中调用 **/

    #if ( INCLUDE_vTaskDelete == 1 )
    {
        BaseType_t xListIsEmpty;  /* 标志位：指示终止任务列表是否为空 */

        /* uxDeletedTasksWaitingCleanUp用于防止在空闲任务中过于频繁地调用vTaskSuspendAll() */
        while( uxDeletedTasksWaitingCleanUp > ( UBaseType_t ) 0U )
        {
            /* 挂起所有任务以确保对终止任务列表的原子访问 */
            vTaskSuspendAll();
            {
                /* 检查终止任务列表是否为空 */
                xListIsEmpty = listLIST_IS_EMPTY( &xTasksWaitingTermination );
            }
            /* 恢复任务调度 */
            ( void ) xTaskResumeAll();

            /* 如果终止任务列表不为空，处理列表中的第一个任务 */
            if( xListIsEmpty == pdFALSE )
            {
                TCB_t *pxTCB;  /* 指向要清理的任务控制块 */

                /* 进入临界区以保护对任务列表的访问 */
                taskENTER_CRITICAL();
                {
                    /* 获取终止任务列表中第一个任务的TCB */
                    pxTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( ( &xTasksWaitingTermination ) );
                    /* 将任务从状态列表中移除 */
                    ( void ) uxListRemove( &( pxTCB->xStateListItem ) );
                    /* 减少当前任务计数 */
                    --uxCurrentNumberOfTasks;
                    /* 减少等待清理的任务计数 */
                    --uxDeletedTasksWaitingCleanUp;
                }
                /* 退出临界区 */
                taskEXIT_CRITICAL();

                /* 删除任务控制块并释放相关资源 */
                prvDeleteTCB( pxTCB );
            }
            else
            {
                /* 列表为空：添加测试覆盖率标记 */
                mtCOVERAGE_TEST_MARKER();
            }
        }
    }
    #endif /* INCLUDE_vTaskDelete */
}
/*-----------------------------------------------------------*/
#if( configUSE_TRACE_FACILITY == 1 )

/*******************************************************************************
 * 函数名称：vTaskGetInfo
 * 功能描述：获取指定任务的详细信息并填充到任务状态结构中
 *           此函数提供任务的完整状态信息，包括基本属性、运行时统计和栈使用情况
 * 输入参数：
 *   - xTask: 任务句柄，指定要获取信息的任务
 *     特殊值：传入NULL表示获取当前任务的信息
 *   - pxTaskStatus: 指向任务状态结构的指针，用于存储获取的任务信息
 *   - xGetFreeStackSpace: 标志位，指示是否获取栈高水位标记信息
 *     pdTRUE: 获取栈高水位标记，pdFALSE: 不获取（提高性能）
 *   - eState: 任务状态，如果传入eInvalid则自动获取任务状态
 *     否则使用传入的状态值（用于优化性能）
 * 输出参数：
 *   - pxTaskStatus: 被填充的任务状态结构，包含任务的详细信息
 * 返 回 值：无
 * 其它说明：
 *   - 此函数仅在启用跟踪功能(configUSE_TRACE_FACILITY == 1)时编译
 *   - 提供完整的任务信息，包括基本属性、运行时统计和栈使用情况
 *   - 支持选择性获取信息以优化性能（如跳过栈空间计算）
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
void vTaskGetInfo( TaskHandle_t xTask, TaskStatus_t *pxTaskStatus, BaseType_t xGetFreeStackSpace, eTaskState eState )
{
    TCB_t *pxTCB;  /* 指向任务控制块的指针 */

    /* 如果xTask为NULL，则获取调用任务的状态信息 */
    pxTCB = prvGetTCBFromHandle( xTask );

    /* 填充任务状态结构的基本信息 */
    pxTaskStatus->xHandle = ( TaskHandle_t ) pxTCB;                  /* 任务句柄 */
    pxTaskStatus->pcTaskName = ( const char * ) &( pxTCB->pcTaskName [ 0 ] ); /* 任务名称 */
    pxTaskStatus->uxCurrentPriority = pxTCB->uxPriority;             /* 当前优先级 */
    pxTaskStatus->pxStackBase = pxTCB->pxStack;                      /* 栈基地址 */
    pxTaskStatus->xTaskNumber = pxTCB->uxTCBNumber;                  /* 任务编号 */

    /* 如果启用了任务挂起功能，处理挂起状态的特殊情况 */
    #if ( INCLUDE_vTaskSuspend == 1 )
    {
        /* 如果任务处于挂起状态，检查是否实际上是无限期阻塞（应报告为阻塞状态） */
        if( pxTaskStatus->eCurrentState == eSuspended )
        {
            /* 挂起所有任务以确保列表访问的原子性 */
            vTaskSuspendAll();
            {
                /* 检查任务的事件列表项是否在某个列表中（表示任务实际上是在等待事件） */
                if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
                {
                    pxTaskStatus->eCurrentState = eBlocked;  /* 将状态改为阻塞 */
                }
            }
            /* 恢复任务调度 */
            xTaskResumeAll();
        }
    }
    #endif /* INCLUDE_vTaskSuspend */

    /* 处理基础优先级信息（如果使用互斥量） */
    #if ( configUSE_MUTEXES == 1 )
    {
        /* 使用互斥量时，任务可能有优先级继承，记录基础优先级 */
        pxTaskStatus->uxBasePriority = pxTCB->uxBasePriority;
    }
    #else
    {
        /* 未使用互斥量时，基础优先级设为0 */
        pxTaskStatus->uxBasePriority = 0;
    }
    #endif

    /* 处理运行时统计信息（如果启用运行时统计） */
    #if ( configGENERATE_RUN_TIME_STATS == 1 )
    {
        /* 记录任务的运行时间计数器 */
        pxTaskStatus->ulRunTimeCounter = pxTCB->ulRunTimeCounter;
    }
    #else
    {
        /* 未启用运行时统计时，运行时间计数器设为0 */
        pxTaskStatus->ulRunTimeCounter = 0;
    }
    #endif

    /* 获取任务状态：如果传入的eState不是eInvalid，则使用传入的状态 */
    /* 否则调用eTaskGetState获取实际状态（这需要更多时间） */
    if( eState != eInvalid )
    {
        /* 使用传入的任务状态（性能优化） */
        pxTaskStatus->eCurrentState = eState;
    }
    else
    {
        /* 调用eTaskGetState获取任务的实际状态 */
        pxTaskStatus->eCurrentState = eTaskGetState( xTask );
    }

    /* 获取栈高水位标记：根据xGetFreeStackSpace参数决定是否计算 */
    /* 计算栈空间需要时间，因此提供参数允许跳过此操作 */
    if( xGetFreeStackSpace != pdFALSE )
    {
        /* 根据栈增长方向调用适当的函数计算栈高水位标记 */
        #if ( portSTACK_GROWTH > 0 )
        {
            /* 向上增长的栈：使用栈结束地址作为参数 */
            pxTaskStatus->usStackHighWaterMark = prvTaskCheckFreeStackSpace( ( uint8_t * ) pxTCB->pxEndOfStack );
        }
        #else
        {
            /* 向下增长的栈：使用栈起始地址作为参数 */
            pxTaskStatus->usStackHighWaterMark = prvTaskCheckFreeStackSpace( ( uint8_t * ) pxTCB->pxStack );
        }
        #endif
    }
    else
    {
        /* 不计算栈高水位标记，设为0 */
        pxTaskStatus->usStackHighWaterMark = 0;
    }
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/
#if ( configUSE_TRACE_FACILITY == 1 )

/*******************************************************************************
 * 函数名称：prvListTasksWithinSingleList
 * 功能描述：遍历单个任务列表并填充任务状态数组，收集列表中所有任务的状态信息
 *           此函数是vTaskList和uxTaskGetSystemState的内部实现，用于收集任务信息
 * 输入参数：
 *   - pxTaskStatusArray: 指向任务状态数组的指针，用于存储收集到的任务信息
 *   - pxList: 指向要遍历的任务列表的指针（如就绪列表、挂起列表等）
 *   - eState: 列表中所有任务的状态（因为同一列表中的任务状态相同）
 * 输出参数：
 *   - pxTaskStatusArray: 被填充的任务状态数组，包含列表中所有任务的信息
 * 返 回 值：
 *   - UBaseType_t: 成功填充的任务状态结构数量
 * 其它说明：
 *   - 此函数仅在启用跟踪功能(configUSE_TRACE_FACILITY == 1)时编译
 *   - 通过循环遍历列表中的每个任务，收集其状态信息
 *   - 使用vTaskGetInfo函数获取每个任务的详细信息
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
static UBaseType_t prvListTasksWithinSingleList( TaskStatus_t *pxTaskStatusArray, List_t *pxList, eTaskState eState )
{
    volatile TCB_t *pxNextTCB;      /* 指向下一个任务控制块的指针（volatile防止优化） */
    volatile TCB_t *pxFirstTCB;     /* 指向列表第一个任务控制块的指针（用于检测循环结束） */
    UBaseType_t uxTask = 0;         /* 计数器：已处理的任务数量 */

    /* 检查列表是否包含任务（列表长度大于0） */
    if( listCURRENT_LIST_LENGTH( pxList ) > ( UBaseType_t ) 0 )
    {
        /* 获取列表中的第一个任务控制块，作为循环结束的标记 */
        listGET_OWNER_OF_NEXT_ENTRY( pxFirstTCB, pxList );

        /* 遍历列表中的每个任务，填充任务状态数组 */
        do
        {
            /* 获取列表中的下一个任务控制块 */
            listGET_OWNER_OF_NEXT_ENTRY( pxNextTCB, pxList );
            
            /* 获取任务的详细信息并填充到任务状态数组中 */
            vTaskGetInfo( ( TaskHandle_t ) pxNextTCB,            /* 任务句柄 */
                         &( pxTaskStatusArray[ uxTask ] ),       /* 任务状态结构指针 */
                         pdTRUE,                                 /* 包含运行时统计信息 */
                         eState );                               /* 任务状态 */
            
            /* 增加已处理任务计数器 */
            uxTask++;
            
        } while( pxNextTCB != pxFirstTCB );  /* 循环直到回到第一个任务 */
    }
    else
    {
        /* 列表为空：添加测试覆盖率标记 */
        mtCOVERAGE_TEST_MARKER();
    }

    /* 返回成功填充的任务状态结构数量 */
    return uxTask;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) )

  /*******************************************************************************
  * 函数名称：prvTaskCheckFreeStackSpace
  * 功能描述：检查任务栈的空闲空间大小，通过统计连续填充字节的数量计算剩余栈空间
  *           此函数是uxTaskGetStackHighWaterMark的内部实现，用于计算栈高水位标记
  * 输入参数：
  *   - pucStackByte: 指向栈检查起始位置的指针，通常是栈的结束位置
  *     注意事项：必须指向有效的栈内存区域
  * 输出参数：无
  * 返 回 值：
  *   - uint16_t: 任务栈的剩余空间大小（以StackType_t为单位）
  *     表示从检查起始位置到第一个非填充字节之间的空闲栈空间
  * 其它说明：
  *   - 此函数仅在启用跟踪功能或栈高水位标记功能时编译
  *   - 通过检测连续的tskSTACK_FILL_BYTE填充字节来计算空闲栈空间
  *   - 结果以StackType_t为单位，便于直接用于栈空间计算
  * 
  * 修改日期      版本号          修改人            修改内容
  * -----------------------------------------------------------------------------
  * 2025/09/03     V1.00          DeepSeek          创建及注释
  *******************************************************************************/
  static uint16_t prvTaskCheckFreeStackSpace( const uint8_t * pucStackByte )
  {
      uint32_t ulCount = 0U;  /* 计数器：统计连续的填充字节数量 */

      /* 遍历栈内存，统计从起始位置开始的连续填充字节数量 */
      while( *pucStackByte == ( uint8_t ) tskSTACK_FILL_BYTE )
      {
          /* 根据栈增长方向移动指针：向下增长则减，向上增长则加 */
          pucStackByte -= portSTACK_GROWTH;
          /* 增加计数器：每找到一个填充字节计数加1 */
          ulCount++;
      }

      /* 将字节计数转换为StackType_t单位计数：除以StackType_t的大小 */
      ulCount /= ( uint32_t ) sizeof( StackType_t ); /*lint !e961 Casting is not redundant on smaller architectures. */

      /* 返回计算得到的空闲栈空间大小（以StackType_t为单位） */
      return ( uint16_t ) ulCount;
  }

#endif /* ( ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) ) */
/*-----------------------------------------------------------*/

#if ( INCLUDE_uxTaskGetStackHighWaterMark == 1 )

/*******************************************************************************
 * 函数名称：uxTaskGetStackHighWaterMark
 * 功能描述：获取指定任务的栈高水位标记，即任务运行过程中栈空间的最小剩余量
 *           此函数用于检测任务栈使用情况，预防栈溢出问题
 * 输入参数：
 *   - xTask: 任务句柄，指定要检查的任务
 *     特殊值：传入NULL表示检查当前任务
 * 输出参数：无
 * 返 回 值：
 *   - UBaseType_t: 任务栈的高水位标记值（单位取决于具体实现，通常是字节数）
 *     返回值越小表示栈使用率越高，0表示栈已满或接近满
 * 其它说明：
 *   - 此函数仅在INCLUDE_uxTaskGetStackHighWaterMark为1时编译
 *   - 高水位标记表示任务运行过程中栈空间的最小剩余量
 *   - 可用于监控任务栈使用情况和预防栈溢出
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
UBaseType_t uxTaskGetStackHighWaterMark( TaskHandle_t xTask )
{
    TCB_t *pxTCB;                   /* 指向任务控制块的指针 */
    uint8_t *pucEndOfStack;         /* 指向栈结束位置的指针 */
    UBaseType_t uxReturn;           /* 返回值：高水位标记 */

    /* 根据任务句柄获取任务控制块(TCB) */
    pxTCB = prvGetTCBFromHandle( xTask );

    /* 根据栈增长方向确定栈结束位置 */
    #if portSTACK_GROWTH < 0
    {
        /* 向下增长的栈：栈结束位置是栈数组的起始地址 */
        pucEndOfStack = ( uint8_t * ) pxTCB->pxStack;
    }
    #else
    {
        /* 向上增长的栈：栈结束位置是栈数组的结束地址 */
        pucEndOfStack = ( uint8_t * ) pxTCB->pxEndOfStack;
    }
    #endif

    /* 计算并返回栈空间的高水位标记 */
    uxReturn = ( UBaseType_t ) prvTaskCheckFreeStackSpace( pucEndOfStack );

    /* 返回高水位标记值 */
    return uxReturn;
}

#endif /* INCLUDE_uxTaskGetStackHighWaterMark */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskDelete == 1 )

/*******************************************************************************
 * 函数名称：prvDeleteTCB
 * 功能描述：删除并清理任务控制块(TCB)，根据配置释放动态分配的内存或处理静态分配资源
 *           1. 执行端口特定的TCB清理操作
 *           2. 处理NewLib可重入结构的回收（如果启用）
 *           3. 根据任务的分配方式（静态/动态）安全释放内存
 * 输入参数：
 *   - pxTCB: 指向要删除的任务控制块(TCB)的指针
 *     注意事项：必须指向有效的TCB结构，调用后指针将不可用
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数仅在INCLUDE_vTaskDelete为1时编译，是vTaskDelete的内部实现
 *   - 根据FreeRTOS配置选项处理不同的内存分配方案
 *   - 静态分配的内存不会在此函数中释放，需要由分配者管理
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Qiguo_Cui          创建
 *******************************************************************************/  
static void prvDeleteTCB( TCB_t *pxTCB )
{
    /* 端口特定的清理操作：针对TriCore等端口需要特殊的TCB清理处理，
       此宏必须在释放内存前调用，也可能被其他端口或演示代码用于静态内存清理 */
    portCLEAN_UP_TCB( pxTCB );

    /* 如果使用NewLib且配置为可重入，需要回收任务关联的_reent结构 */
    #if ( configUSE_NEWLIB_REENTRANT == 1 )
    {
        /* 回收NewLib的可重入结构体资源，防止内存泄漏 */
        _reclaim_reent( &( pxTCB->xNewLib_reent ) );
    }
    #endif /* configUSE_NEWLIB_REENTRANT */

    /* 处理动态分配且无双重分配可能性的情况（无静态分配、无MPU包装） */
    #if( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 0 ) && ( portUSING_MPU_WRAPPERS == 0 ) )
    {
        /* 任务完全动态分配：同时释放任务栈和TCB内存 */
        vPortFree( pxTCB->pxStack );  /* 释放任务栈空间 */
        vPortFree( pxTCB );           /* 释放任务控制块内存 */
    }
    /* 处理静态和动态分配共存的情况 */
    #elif( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE == 1 )
    {
        /* 根据TCB中的分配标志ucStaticallyAllocated判断内存分配方式 */
        if( pxTCB->ucStaticallyAllocated == tskDYNAMICALLY_ALLOCATED_STACK_AND_TCB )
        {
            /* 栈和TCB都是动态分配：需要释放两者 */
            vPortFree( pxTCB->pxStack );  /* 释放动态分配的任务栈 */
            vPortFree( pxTCB );           /* 释放动态分配的TCB */
        }
        else if( pxTCB->ucStaticallyAllocated == tskSTATICALLY_ALLOCATED_STACK_ONLY )
        {
            /* 仅栈是静态分配：只释放动态分配的TCB，栈内存由系统管理 */
            vPortFree( pxTCB );           /* 仅释放TCB内存 */
        }
        else
        {
            /* 栈和TCB都是静态分配：不需要释放任何内存，
               使用configASSERT验证分配标志的正确性 */
            configASSERT( pxTCB->ucStaticallyAllocated == tskSTATICALLY_ALLOCATED_STACK_AND_TCB );
            /* 覆盖率标记：用于测试覆盖分析 */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
}

#endif /* INCLUDE_vTaskDelete */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvResetNextTaskUnblockTime
 * 功能描述：重置下一个任务解除阻塞时间的内部函数
 *           根据延迟任务列表的状态更新下一个任务解除阻塞时间，用于优化调度器性能
 * 输入参数：void - 无输入参数
 * 输出参数：无
 * 返回值：无
 * 其它说明：
 *   - 此函数是FreeRTOS调度器的内部函数，不对外公开
 *   - 用于维护xNextTaskUnblockTime变量，该变量记录最早需要解除阻塞的任务时间
 *   - 当延迟任务列表发生变化时调用此函数，确保xNextTaskUnblockTime保持最新
 *   - 优化调度器性能，减少不必要的列表遍历和检查
 *   - 处理延迟任务列表为空和非空两种情况
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
static void prvResetNextTaskUnblockTime( void )
{
    /* 指向任务控制块的指针 */
    TCB_t *pxTCB;

    /* 检查延迟任务列表是否为空 */
    if( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )
    {
        /* 新的当前延迟列表为空。将xNextTaskUnblockTime设置为最大可能值，
           这样在延迟列表中有项目之前，if(xTickCount >= xNextTaskUnblockTime)测试
           极不可能通过 */
        xNextTaskUnblockTime = portMAX_DELAY;
    }
    else
    {
        /* 新的当前延迟列表不为空，获取延迟列表头部项目的值
           这是延迟列表头部任务应该从阻塞状态移除的时间 */
        ( pxTCB ) = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList );
        xNextTaskUnblockTime = listGET_LIST_ITEM_VALUE( &( ( pxTCB )->xStateListItem ) );
    }
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xTaskGetCurrentTaskHandle
 * 功能描述：获取当前正在运行的任务的任务句柄
 *           此函数用于获取当前执行任务的任务控制块指针，便于任务识别和自我引用
 * 输入参数：void - 无输入参数
 * 输出参数：无
 * 返回值：
 *   - TaskHandle_t: 返回当前正在运行的任务的任务句柄
 *                   指向当前任务的任务控制块（TCB）的指针
 * 其它说明：
 *   - 此函数在INCLUDE_xTaskGetCurrentTaskHandle为1或configUSE_MUTEXES为1时编译
 *   - 不需要临界区保护，因为不是在中断中调用，且当前TCB对任何单独执行线程都是相同的
 *   - 提供了一种简单的方式让任务获取自身的句柄或当前运行任务的句柄
 *   - 对于任务自我识别、调试和任务间通信非常有用
 *   - 函数执行简单快速，不会阻塞或影响系统性能
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
#if ( ( INCLUDE_xTaskGetCurrentTaskHandle == 1 ) || ( configUSE_MUTEXES == 1 ) )

TaskHandle_t xTaskGetCurrentTaskHandle( void )
{
    /* 存储函数返回结果 */
    TaskHandle_t xReturn;

    /* 不需要临界区，因为这不是从中断调用的，
       并且当前TCB对于任何单独的执行线程都是相同的 */
    xReturn = pxCurrentTCB;

    /* 返回当前任务句柄 */
    return xReturn;
}

#endif /* ( ( INCLUDE_xTaskGetCurrentTaskHandle == 1 ) || ( configUSE_MUTEXES == 1 ) ) */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xTaskGetSchedulerState
 * 功能描述：获取当前调度器状态的函数
 *           此函数用于查询FreeRTOS调度器的当前运行状态，包括未启动、运行中和挂起三种状态
 * 输入参数：void - 无输入参数
 * 输出参数：无
 * 返回值：
 *   - BaseType_t: 返回当前调度器的状态
 *                 taskSCHEDULER_NOT_STARTED 表示调度器未启动
 *                 taskSCHEDULER_RUNNING 表示调度器正在运行
 *                 taskSCHEDULER_SUSPENDED 表示调度器已挂起
 * 其它说明：
 *   - 此函数在INCLUDE_xTaskGetSchedulerState为1或configUSE_TIMERS为1时编译
 *   - 提供了一种安全的方式查询调度器状态，便于应用程序根据状态采取相应操作
 *   - 调度器状态对于定时器操作和任务同步非常重要
 *   - 函数执行简单快速，不会阻塞或影响系统性能
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
#if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )

BaseType_t xTaskGetSchedulerState( void )
{
    /* 存储函数返回结果 */
    BaseType_t xReturn;

    /* 检查调度器是否未启动 */
    if( xSchedulerRunning == pdFALSE )
    {
        /* 返回调度器未启动状态 */
        xReturn = taskSCHEDULER_NOT_STARTED;
    }
    else
    {
        /* 检查调度器是否未挂起 */
        if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
        {
            /* 返回调度器运行中状态 */
            xReturn = taskSCHEDULER_RUNNING;
        }
        else
        {
            /* 返回调度器挂起状态 */
            xReturn = taskSCHEDULER_SUSPENDED;
        }
    }

    /* 返回调度器状态 */
    return xReturn;
}

#endif /* ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) ) */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vTaskPriorityInherit
 * 功能描述：任务优先级继承的内部函数，用于处理互斥信号量获取时的优先级提升
 *           当高优先级任务尝试获取已被低优先级任务持有的互斥信号量时，
 *           此函数负责将低优先级任务的优先级提升到与高优先级任务相同，
 *           以防止优先级反转问题
 * 输入参数：
 *   - pxMutexHolder: 互斥信号量持有者的任务句柄，指向需要提升优先级的任务对象
 * 输出参数：无
 * 返回值：无
 * 其它说明：
 *   - 此函数仅在configUSE_MUTEXES为1时编译，是互斥信号量功能的一部分
 *   - 处理优先级继承机制，防止优先级反转问题
 *   - 当互斥信号量持有者的优先级低于尝试获取互斥信号量的任务时，临时提升持有者优先级
 *   - 更新任务的就绪列表状态，确保调度器能够正确调度
 *   - 提供跟踪功能，便于调试和性能分析
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
#if ( configUSE_MUTEXES == 1 )

void vTaskPriorityInherit( TaskHandle_t const pxMutexHolder )
{
    /* 将互斥信号量持有者句柄转换为任务控制块指针 */
    TCB_t * const pxTCB = ( TCB_t * ) pxMutexHolder;

    /* 如果互斥信号量在队列锁定时由中断返回，则互斥信号量持有者现在可能为NULL */
    if( pxMutexHolder != NULL )
    {
        /* 如果互斥信号量持有者的优先级低于尝试获取互斥信号量的任务的优先级，
           则它将临时继承尝试获取互斥信号量的任务的优先级 */
        if( pxTCB->uxPriority < pxCurrentTCB->uxPriority )
        {
            /* 调整互斥信号量持有者状态以适应其新优先级
               仅当事件列表项值未用于其他任何用途时才重置它 */
            if( ( listGET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ) ) & taskEVENT_LIST_ITEM_VALUE_IN_USE ) == 0UL )
            {
                /* 设置事件列表项值为基于新优先级的计算值 */
                listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) pxCurrentTCB->uxPriority ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
            }
            else
            {
                /* 代码覆盖测试标记（事件列表项值正在使用的情况） */
                mtCOVERAGE_TEST_MARKER();
            }

            /* 如果被修改的任务处于就绪状态，则需要将其移动到新列表中 */
            if( listIS_CONTAINED_WITHIN( &( pxReadyTasksLists[ pxTCB->uxPriority ] ), &( pxTCB->xStateListItem ) ) != pdFALSE )
            {
                /* 从当前就绪列表中移除任务 */
                if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
                {
                    /* 如果该优先级就绪列表为空，重置就绪优先级位图 */
                    taskRESET_READY_PRIORITY( pxTCB->uxPriority );
                }
                else
                {
                    /* 代码覆盖测试标记（任务不在就绪列表中的情况） */
                    mtCOVERAGE_TEST_MARKER();
                }

                /* 在移动到新列表之前继承优先级 */
                pxTCB->uxPriority = pxCurrentTCB->uxPriority;
                /* 将任务添加到新优先级的就绪列表 */
                prvAddTaskToReadyList( pxTCB );
            }
            else
            {
                /* 仅继承优先级，不操作就绪列表 */
                pxTCB->uxPriority = pxCurrentTCB->uxPriority;
            }

            /* 记录优先级继承的跟踪信息 */
            traceTASK_PRIORITY_INHERIT( pxTCB, pxCurrentTCB->uxPriority );
        }
        else
        {
            /* 代码覆盖测试标记（持有者优先级不低于当前任务的情况） */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    else
    {
        /* 代码覆盖测试标记（互斥信号量持有者为空的情况） */
        mtCOVERAGE_TEST_MARKER();
    }
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xTaskPriorityDisinherit
 * 功能描述：任务优先级取消继承的内部函数，用于处理互斥信号量释放时的优先级恢复
 *           当任务释放互斥信号量时，如果该任务因优先级继承机制被提升了优先级，
 *           此函数负责将其优先级恢复为基础优先级，并处理相关的就绪列表更新
 * 输入参数：
 *   - pxMutexHolder: 互斥信号量持有者的任务句柄，指向需要取消优先级继承的任务对象
 * 输出参数：无
 * 返回值：
 *   - BaseType_t: 返回是否需要上下文切换的标志
 *                 pdTRUE表示需要上下文切换，pdFALSE表示不需要上下文切换
 * 其它说明：
 *   - 此函数仅在configUSE_MUTEXES为1时编译，是互斥信号量功能的一部分
 *   - 处理优先级继承机制的取消，防止优先级反转问题
 *   - 确保任务在释放互斥信号量后恢复其原始优先级
 *   - 更新任务的就绪列表状态，确保调度器能够正确调度
 *   - 只在所有互斥信号量都释放后才取消优先级继承
 *   - 提供跟踪功能，便于调试和性能分析
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
#if ( configUSE_MUTEXES == 1 )

BaseType_t xTaskPriorityDisinherit( TaskHandle_t const pxMutexHolder )
{
    /* 将互斥信号量持有者句柄转换为任务控制块指针 */
    TCB_t * const pxTCB = ( TCB_t * ) pxMutexHolder;
    /* 存储函数返回结果，默认为不需要上下文切换 */
    BaseType_t xReturn = pdFALSE;

    /* 检查互斥信号量持有者是否有效 */
    if( pxMutexHolder != NULL )
    {
        /* 任务只有在持有互斥信号量时才能有继承的优先级
           如果互斥信号量由任务持有，则不能从中断中给出，
           如果互斥信号量由持有任务给出，则它必须是运行状态任务 */
        configASSERT( pxTCB == pxCurrentTCB );

        /* 断言确保任务确实持有互斥信号量 */
        configASSERT( pxTCB->uxMutexesHeld );
        /* 减少任务持有的互斥信号量计数 */
        ( pxTCB->uxMutexesHeld )--;

        /* 互斥信号量持有者是否继承了另一个任务的优先级？ */
        if( pxTCB->uxPriority != pxTCB->uxBasePriority )
        {
            /* 只有在没有其他互斥信号量持有时才取消继承 */
            if( pxTCB->uxMutexesHeld == ( UBaseType_t ) 0 )
            {
                /* 任务只有在持有互斥信号量时才能有继承的优先级
                   如果互斥信号量由任务持有，则不能从中断中给出，
                   如果互斥信号量由持有任务给出，则它必须是运行状态任务
                   从就绪列表中移除持有任务 */
                if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
                {
                    /* 重置就绪优先级位图中的对应位 */
                    taskRESET_READY_PRIORITY( pxTCB->uxPriority );
                }
                else
                {
                    /* 代码覆盖测试标记（任务不在就绪列表中的情况） */
                    mtCOVERAGE_TEST_MARKER();
                }

                /* 在将任务添加到新就绪列表之前取消优先级继承 */
                traceTASK_PRIORITY_DISINHERIT( pxTCB, pxTCB->uxBasePriority );
                /* 将任务优先级恢复为基础优先级 */
                pxTCB->uxPriority = pxTCB->uxBasePriority;

                /* 重置事件列表项值。如果此任务正在运行，则它不能用于任何其他目的，
                   并且它必须正在运行才能返回互斥信号量 */
                listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) pxTCB->uxPriority ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
                /* 将任务添加到就绪列表 */
                prvAddTaskToReadyList( pxTCB );

                /* 返回true表示需要上下文切换
                   这实际上只在极端情况下需要，即持有多个互斥信号量并且
                   互斥信号量以不同于获取顺序的顺序返回
                   如果在返回第一个互斥信号量时没有发生上下文切换，即使有任务在等待它，
                   那么当最后一个互斥信号量返回时，无论是否有任务等待，都应发生上下文切换 */
                xReturn = pdTRUE;
            }
            else
            {
                /* 代码覆盖测试标记（仍有其他互斥信号量持有的情况） */
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            /* 代码覆盖测试标记（优先级已等于基础优先级的情况） */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    else
    {
        /* 代码覆盖测试标记（互斥信号量持有者为空的情况） */
        mtCOVERAGE_TEST_MARKER();
    }

    /* 返回是否需要上下文切换的标志 */
    return xReturn;
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vTaskEnterCritical
 * 功能描述：进入临界区的任务版本函数，用于保护关键代码段免受中断和其他任务干扰
 *           此函数禁用中断并增加当前任务的临界区嵌套计数，确保关键代码段的原子性执行
 * 输入参数：void - 无输入参数
 * 输出参数：无
 * 返回值：无
 * 其它说明：
 *   - 此函数仅在portCRITICAL_NESTING_IN_TCB为1时编译，表示临界区嵌套计数存储在TCB中
 *   - 非中断安全版本，不能在中断服务程序中调用
 *   - 使用portDISABLE_INTERRUPTS()禁用中断
 *   - 增加当前任务的临界区嵌套计数，支持嵌套临界区
 *   - 检查是否在中断上下文中调用，如果是从中断调用会触发断言
 *   - 只有在调度器运行后才进行嵌套计数操作
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
#if ( portCRITICAL_NESTING_IN_TCB == 1 )

void vTaskEnterCritical( void )
{
    /* 禁用中断，开始临界区 */
    portDISABLE_INTERRUPTS();

    /* 检查调度器是否正在运行 */
    if( xSchedulerRunning != pdFALSE )
    {
        /* 增加当前任务的临界区嵌套计数 */
        ( pxCurrentTCB->uxCriticalNesting )++;

        /* 这不是中断安全版本的进入临界区函数，因此如果从中断上下文调用它会触发断言
           只有以"FromISR"结尾的API函数才能在中断中使用
           只有当临界嵌套计数为1时才断言，以防止如果断言函数也使用临界区时的递归调用 */
        if( pxCurrentTCB->uxCriticalNesting == 1 )
        {
            /* 检查是否在中断中调用，如果是则触发断言 */
            portASSERT_IF_IN_ISR();
        }
    }
    else
    {
        /* 代码覆盖测试标记（调度器未运行的情况） */
        mtCOVERAGE_TEST_MARKER();
    }
}

#endif /* portCRITICAL_NESTING_IN_TCB */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vTaskExitCritical
 * 功能描述：退出临界区的任务版本函数，用于结束关键代码段的保护
 *           此函数减少当前任务的临界区嵌套计数，当嵌套计数为零时重新使能中断
 * 输入参数：void - 无输入参数
 * 输出参数：无
 * 返回值：无
 * 其它说明：
 *   - 此函数仅在portCRITICAL_NESTING_IN_TCB为1时编译，表示临界区嵌套计数存储在TCB中
 *   - 非中断安全版本，不能在中断服务程序中调用
 *   - 减少当前任务的临界区嵌套计数，支持嵌套临界区
 *   - 当嵌套计数为零时使用portENABLE_INTERRUPTS()重新使能中断
 *   - 只有在调度器运行后才进行嵌套计数操作
 *   - 提供安全的嵌套临界区退出机制，确保中断只在所有嵌套临界区都退出后重新使能
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
#if ( portCRITICAL_NESTING_IN_TCB == 1 )

void vTaskExitCritical( void )
{
    /* 检查调度器是否正在运行 */
    if( xSchedulerRunning != pdFALSE )
    {
        /* 检查临界区嵌套计数是否大于0 */
        if( pxCurrentTCB->uxCriticalNesting > 0U )
        {
            /* 减少当前任务的临界区嵌套计数 */
            ( pxCurrentTCB->uxCriticalNesting )--;

            /* 如果嵌套计数减至0，重新使能中断 */
            if( pxCurrentTCB->uxCriticalNesting == 0U )
            {
                portENABLE_INTERRUPTS();
            }
            else
            {
                /* 代码覆盖测试标记（嵌套计数仍大于0的情况） */
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            /* 代码覆盖测试标记（嵌套计数已经为0的情况） */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    else
    {
        /* 代码覆盖测试标记（调度器未运行的情况） */
        mtCOVERAGE_TEST_MARKER();
    }
}

#endif /* portCRITICAL_NESTING_IN_TCB */
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )

	static char *prvWriteNameToBuffer( char *pcBuffer, const char *pcTaskName )
	{
	size_t x;

		/* Start by copying the entire string. */
		strcpy( pcBuffer, pcTaskName );

		/* Pad the end of the string with spaces to ensure columns line up when
		printed out. */
		for( x = strlen( pcBuffer ); x < ( size_t ) ( configMAX_TASK_NAME_LEN - 1 ); x++ )
		{
			pcBuffer[ x ] = ' ';
		}

		/* Terminate. */
		pcBuffer[ x ] = 0x00;

		/* Return the new end of string. */
		return &( pcBuffer[ x ] );
	}

#endif /* ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vTaskList
 * 功能描述：获取系统中所有任务的状态信息并格式化为可读字符串的内部函数
 *           此函数生成系统中所有任务的详细信息列表，包括任务名称、状态、优先级、
 *           栈高水位线和任务编号，并将结果格式化为表格形式写入提供的缓冲区
 * 输入参数：
 *   - pcWriteBuffer: 指向字符缓冲区的指针，用于存储格式化后的任务列表信息字符串
 * 输出参数：
 *   - pcWriteBuffer: 写入格式化后的任务列表信息字符串
 * 返回值：无
 * 其它说明：
 *   - 此函数仅在configUSE_TRACE_FACILITY和configUSE_STATS_FORMATTING_FUNCTIONS启用时编译
 *   - 使用动态内存分配获取任务状态数组内存，需要确保系统支持动态分配
 *   - 生成人类可读的表格，显示每个任务的状态、优先级、栈使用情况和任务编号
 *   - 使用sprintf函数格式化输出，可能会增加代码大小和栈使用量
 *   - 建议生产系统直接调用uxTaskGetSystemState()获取原始统计数据
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )

void vTaskList( char * pcWriteBuffer )
{
    /* 指向任务状态数组的指针 */
    TaskStatus_t *pxTaskStatusArray;
    /* 数组大小和循环变量 */
    volatile UBaseType_t uxArraySize, x;
    /* 任务状态字符表示 */
    char cStatus;

    /*
     * 请注意：
     *
     * 此函数仅为方便而提供，许多演示应用程序使用它。不要将其视为调度程序的一部分。
     *
     * vTaskList()调用uxTaskGetSystemState()，然后将uxTaskGetSystemState()输出的一部分
     * 格式化为人类可读的表格，显示任务名称、状态和栈使用情况。
     *
     * vTaskList()依赖于sprintf() C库函数，可能会增加代码大小，使用大量堆栈，
     * 并在不同平台上提供不同的结果。在许多FreeRTOS/Demo子目录中，提供了一个替代的、小型的、
     * 第三方的、功能有限的sprintf()实现，位于名为printf-stdarg.c的文件中
     * （注意printf-stdarg.c不提供完整的snprintf()实现！）。
     *
     * 建议生产系统直接调用uxTaskGetSystemState()来获取原始统计数据，而不是间接通过调用vTaskList()。
     */

    /* 确保写入缓冲区不包含字符串（以空字符开头） */
    *pcWriteBuffer = 0x00;

    /* 获取任务数量的快照，以防此函数执行期间任务数量发生变化 */
    uxArraySize = uxCurrentNumberOfTasks;

    /* 为每个任务分配数组索引。注意！如果configSUPPORT_DYNAMIC_ALLOCATION设置为0，则pvPortMalloc()将等于NULL */
    pxTaskStatusArray = pvPortMalloc( uxCurrentNumberOfTasks * sizeof( TaskStatus_t ) );

    /* 检查内存分配是否成功 */
    if( pxTaskStatusArray != NULL )
    {
        /* 生成（二进制）数据，不需要总运行时间 */
        uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, NULL );

        /* 从二进制数据创建人类可读的表格 */
        for( x = 0; x < uxArraySize; x++ )
        {
            /* 根据任务当前状态设置状态字符 */
            switch( pxTaskStatusArray[ x ].eCurrentState )
            {
                case eReady:        /* 就绪状态 */
                    cStatus = tskREADY_CHAR;
                    break;

                case eBlocked:        /* 阻塞状态 */
                    cStatus = tskBLOCKED_CHAR;
                    break;

                case eSuspended:    /* 挂起状态 */
                    cStatus = tskSUSPENDED_CHAR;
                    break;

                case eDeleted:        /* 删除状态 */
                    cStatus = tskDELETED_CHAR;
                    break;

                default:            /* 不应该到达这里，但包含它以防止静态检查错误 */
                    cStatus = 0x00;
                    break;
            }

            /* 将任务名称写入字符串，用空格填充以便更容易以表格形式打印 */
            pcWriteBuffer = prvWriteNameToBuffer( pcWriteBuffer, pxTaskStatusArray[ x ].pcTaskName );

            /* 写入字符串的其余部分（状态、优先级、栈高水位线、任务编号） */
            sprintf( pcWriteBuffer, "\t%c\t%u\t%u\t%u\r\n", cStatus, ( unsigned int ) pxTaskStatusArray[ x ].uxCurrentPriority, ( unsigned int ) pxTaskStatusArray[ x ].usStackHighWaterMark, ( unsigned int ) pxTaskStatusArray[ x ].xTaskNumber );
            /* 移动缓冲区指针到字符串末尾 */
            pcWriteBuffer += strlen( pcWriteBuffer );
        }

        /* 再次释放数组。注意！如果configSUPPORT_DYNAMIC_ALLOCATION为0，则vPortFree()将被定义为空 */
        vPortFree( pxTaskStatusArray );
    }
    else
    {
        /* 代码覆盖测试标记（内存分配失败的情况） */
        mtCOVERAGE_TEST_MARKER();
    }
}

#endif /* ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) ) */
/*----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vTaskGetRunTimeStats
 * 功能描述：获取任务运行时统计信息并格式化为可读字符串的内部函数
 *           此函数生成系统中所有任务的运行时统计信息，包括运行时间绝对值和百分比
 *           并将结果格式化为表格形式写入提供的缓冲区
 * 输入参数：
 *   - pcWriteBuffer: 指向字符缓冲区的指针，用于存储格式化后的统计信息字符串
 * 输出参数：
 *   - pcWriteBuffer: 写入格式化后的统计信息字符串
 * 返回值：无
 * 其它说明：
 *   - 此函数仅在configGENERATE_RUN_TIME_STATS和configUSE_STATS_FORMATTING_FUNCTIONS启用时编译
 *   - 依赖于configUSE_TRACE_FACILITY功能，必须设置为1
 *   - 使用动态内存分配获取任务状态数组，需要确保系统支持动态分配
 *   - 生成人类可读的表格，显示每个任务在运行状态下花费的时间
 *   - 使用sprintf函数格式化输出，可能会增加代码大小和栈使用量
 *   - 建议生产系统直接调用uxTaskGetSystemState()获取原始统计数据
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
#if ( ( configGENERATE_RUN_TIME_STATS == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )

void vTaskGetRunTimeStats( char *pcWriteBuffer )
{
    /* 指向任务状态数组的指针 */
    TaskStatus_t *pxTaskStatusArray;
    /* 数组大小和循环变量 */
    volatile UBaseType_t uxArraySize, x;
    /* 总运行时间和百分比统计 */
    uint32_t ulTotalTime, ulStatsAsPercentage;

    /* 检查是否启用了跟踪功能 */
    #if( configUSE_TRACE_FACILITY != 1 )
    {
        /* 必须设置configUSE_TRACE_FACILITY为1才能使用vTaskGetRunTimeStats() */
        #error configUSE_TRACE_FACILITY must also be set to 1 in FreeRTOSConfig.h to use vTaskGetRunTimeStats().
    }
    #endif

    /*
     * 请注意：
     *
     * 此函数仅为方便而提供，许多演示应用程序使用它。不要将其视为调度程序的一部分。
     *
     * vTaskGetRunTimeStats()调用uxTaskGetSystemState()，然后将uxTaskGetSystemState()输出的一部分
     * 格式化为人类可读的表格，显示每个任务在运行状态下花费的时间（绝对值和百分比）。
     *
     * vTaskGetRunTimeStats()依赖于sprintf() C库函数，可能会增加代码大小，使用大量堆栈，
     * 并在不同平台上提供不同的结果。在许多FreeRTOS/Demo子目录中，提供了一个替代的、小型的、
     * 第三方的、功能有限的sprintf()实现，位于名为printf-stdarg.c的文件中
     * （注意printf-stdarg.c不提供完整的snprintf()实现！）。
     *
     * 建议生产系统直接调用uxTaskGetSystemState()来获取原始统计数据，而不是间接通过调用vTaskGetRunTimeStats()。
     */

    /* 确保写入缓冲区不包含字符串（以空字符开头） */
    *pcWriteBuffer = 0x00;

    /* 获取任务数量的快照，以防此函数执行期间任务数量发生变化 */
    uxArraySize = uxCurrentNumberOfTasks;

    /* 为每个任务分配数组索引。注意！如果configSUPPORT_DYNAMIC_ALLOCATION设置为0，则pvPortMalloc()将等于NULL */
    pxTaskStatusArray = pvPortMalloc( uxCurrentNumberOfTasks * sizeof( TaskStatus_t ) );

    /* 检查内存分配是否成功 */
    if( pxTaskStatusArray != NULL )
    {
        /* 生成（二进制）数据 */
        uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, &ulTotalTime );

        /* 用于百分比计算 */
        ulTotalTime /= 100UL;

        /* 避免除零错误 */
        if( ulTotalTime > 0 )
        {
            /* 从二进制数据创建人类可读的表格 */
            for( x = 0; x < uxArraySize; x++ )
            {
                /* 任务使用了总运行时间的百分比是多少？
                   这将始终向下取整到最接近的整数。
                   ulTotalTime已经被除以100 */
                ulStatsAsPercentage = pxTaskStatusArray[ x ].ulRunTimeCounter / ulTotalTime;

                /* 将任务名称写入字符串，用空格填充以便更容易以表格形式打印 */
                pcWriteBuffer = prvWriteNameToBuffer( pcWriteBuffer, pxTaskStatusArray[ x ].pcTaskName );

                /* 检查百分比是否大于0 */
                if( ulStatsAsPercentage > 0UL )
                {
                    /* 检查是否需要长无符号整型打印说明符 */
                    #ifdef portLU_PRINTF_SPECIFIER_REQUIRED
                    {
                        /* 使用%lu格式说明符写入运行时间和百分比 */
                        sprintf( pcWriteBuffer, "\t%lu\t\t%lu%%\r\n", pxTaskStatusArray[ x ].ulRunTimeCounter, ulStatsAsPercentage );
                    }
                    #else
                    {
                        /* sizeof(int) == sizeof(long) 所以可以使用较小的printf()库 */
                        sprintf( pcWriteBuffer, "\t%u\t\t%u%%\r\n", ( unsigned int ) pxTaskStatusArray[ x ].ulRunTimeCounter, ( unsigned int ) ulStatsAsPercentage );
                    }
                    #endif
                }
                else
                {
                    /* 如果这里的百分比为零，则任务消耗的总运行时间少于1% */
                    #ifdef portLU_PRINTF_SPECIFIER_REQUIRED
                    {
                        sprintf( pcWriteBuffer, "\t%lu\t\t<1%%\r\n", pxTaskStatusArray[ x ].ulRunTimeCounter );
                    }
                    #else
                    {
                        /* sizeof(int) == sizeof(long) 所以可以使用较小的printf()库 */
                        sprintf( pcWriteBuffer, "\t%u\t\t<1%%\r\n", ( unsigned int ) pxTaskStatusArray[ x ].ulRunTimeCounter );
                    }
                    #endif
                }

                /* 移动缓冲区指针到字符串末尾 */
                pcWriteBuffer += strlen( pcWriteBuffer );
            }
        }
        else
        {
            /* 代码覆盖测试标记（总时间为0的情况） */
            mtCOVERAGE_TEST_MARKER();
        }

        /* 再次释放数组。注意！如果configSUPPORT_DYNAMIC_ALLOCATION为0，则vPortFree()将被定义为空 */
        vPortFree( pxTaskStatusArray );
    }
    else
    {
        /* 代码覆盖测试标记（内存分配失败的情况） */
        mtCOVERAGE_TEST_MARKER();
    }
}

#endif /* ( ( configGENERATE_RUN_TIME_STATS == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) ) */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：uxTaskResetEventItemValue
 * 功能描述：重置任务事件列表项值并返回原值的内部函数
 *           此函数用于获取并重置当前任务事件列表项的值，使其恢复为默认优先级相关值
 * 输入参数：void - 无输入参数
 * 输出参数：无
 * 返回值：
 *   - TickType_t: 返回重置前任务事件列表项的值
 * 其它说明：
 *   - 此函数主要用于事件管理和任务同步机制中
 *   - 将任务事件列表项的值重置为基于任务优先级的默认值
 *   - 确保事件列表项可以正常用于队列和信号量操作
 *   - 使用列表项操作宏获取和设置值，确保操作的高效性
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
TickType_t uxTaskResetEventItemValue( void )
{
    /* 存储返回的事件列表项原值 */
    TickType_t uxReturn;

    /* 获取当前任务事件列表项的当前值 */
    uxReturn = listGET_LIST_ITEM_VALUE( &( pxCurrentTCB->xEventListItem ) );

    /* 将事件列表项重置为其正常值 - 以便它可以与队列和信号量一起使用
       正常值计算公式：configMAX_PRIORITIES - 当前任务优先级
       这样确保高优先级任务的事件列表项值较小，在列表中排在前面 */
    listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xEventListItem ), ( ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) pxCurrentTCB->uxPriority ) ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */

    /* 返回重置前的事件列表项值 */
    return uxReturn;
}
/*-----------------------------------------------------------*/

#if ( configUSE_MUTEXES == 1 )
/*******************************************************************************
 * 函数名称：pvTaskIncrementMutexHeldCount
 * 功能描述：增加任务持有的互斥信号量计数的内部函数
 *           此函数用于增加当前任务持有的互斥信号量计数，并返回任务控制块指针
 * 输入参数：void - 无输入参数
 * 输出参数：无
 * 返回值：
 *   - void*: 返回当前任务控制块指针（TCB指针）
 * 其它说明：
 *   - 此函数仅在configUSE_MUTEXES为1时编译，是互斥信号量功能的一部分
 *   - 用于跟踪任务持有的互斥信号量数量，支持优先级继承机制
 *   - 如果任务创建前调用，pxCurrentTCB可能为NULL，需要进行空指针检查
 *   - 返回任务控制块指针，便于调用者进行后续操作
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
void *pvTaskIncrementMutexHeldCount( void )
{
    /* 如果xSemaphoreCreateMutex()在任何任务创建之前调用，则pxCurrentTCB将为NULL */
    if( pxCurrentTCB != NULL )
    {
        /* 增加当前任务持有的互斥信号量计数 */
        ( pxCurrentTCB->uxMutexesHeld )++;
    }

    /* 返回当前任务控制块指针 */
    return pxCurrentTCB;
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：ulTaskNotifyTake
 * 功能描述：获取任务通知值的核心函数，实现类似计数信号量的"take"操作
 *           此函数允许任务等待通知值变为非零，然后获取并可选地清除通知值
 * 输入参数：
 *   - xClearCountOnExit: 退出时清除计数的方式
 *                        pdTRUE表示获取后清零通知值（类似二进制信号量）
 *                        pdFALSE表示获取后减1（类似计数信号量）
 *   - xTicksToWait: 最大等待时间（以系统节拍为单位）
 *                   可以是portMAX_DELAY表示无限等待，0表示不等待立即返回
 * 输出参数：无
 * 返回值：
 *   - uint32_t: 返回获取到的通知值
 *               如果成功获取通知，返回非零值
 *               如果等待超时或未获取到通知，返回0
 * 其它说明：
 *   - 此函数仅在configUSE_TASK_NOTIFICATIONS为1时编译，是任务通知功能的一部分
 *   - 提供类似计数信号量的功能，但比传统信号量更高效，占用资源更少
 *   - 支持两种清除模式：完全清零（二进制信号量模式）或减1（计数信号量模式）
 *   - 使用临界区保护状态操作，确保在多任务环境下的线程安全性
 *   - 提供跟踪功能，便于调试和性能分析
 *   - 只有在通知值为0时才阻塞任务，提高执行效率
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
#if( configUSE_TASK_NOTIFICATIONS == 1 )

uint32_t ulTaskNotifyTake( BaseType_t xClearCountOnExit, TickType_t xTicksToWait )
{
    /* 存储函数返回的通知值 */
    uint32_t ulReturn;

    /* 进入临界区，保护状态操作的原子性 */
    taskENTER_CRITICAL();
    {
        /* 只有在通知计数尚未非零时才阻塞 */
        if( pxCurrentTCB->ulNotifiedValue == 0UL )
        {
            /* 将此任务标记为等待通知 */
            pxCurrentTCB->ucNotifyState = taskWAITING_NOTIFICATION;

            /* 检查等待时间是否大于0 */
            if( xTicksToWait > ( TickType_t ) 0 )
            {
                /* 将当前任务添加到延迟列表，允许无限期阻塞 */
                prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
                /* 记录任务通知获取阻塞的跟踪信息 */
                traceTASK_NOTIFY_TAKE_BLOCK();

                /* 所有端口都编写为允许在临界section中进行yield
                   （有些会立即yield，其他则等待临界section退出）
                   但这不是应用程序代码应该做的事情 */
                portYIELD_WITHIN_API();
            }
            else
            {
                /* 代码覆盖测试标记（等待时间为0的情况） */
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            /* 代码覆盖测试标记（通知值已非零的情况） */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    /* 退出临界区 */
    taskEXIT_CRITICAL();

    /* 再次进入临界区，处理获取结果 */
    taskENTER_CRITICAL();
    {
        /* 记录任务通知获取的跟踪信息 */
        traceTASK_NOTIFY_TAKE();
        /* 获取当前任务的通知值 */
        ulReturn = pxCurrentTCB->ulNotifiedValue;

        /* 如果通知值非零，根据清除模式处理通知值 */
        if( ulReturn != 0UL )
        {
            if( xClearCountOnExit != pdFALSE )
            {
                /* 二进制信号量模式：获取后清零通知值 */
                pxCurrentTCB->ulNotifiedValue = 0UL;
            }
            else
            {
                /* 计数信号量模式：获取后通知值减1 */
                pxCurrentTCB->ulNotifiedValue = ulReturn - 1;
            }
        }
        else
        {
            /* 代码覆盖测试标记（通知值为0的情况） */
            mtCOVERAGE_TEST_MARKER();
        }

        /* 将任务通知状态重置为"未等待通知" */
        pxCurrentTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION;
    }
    /* 退出临界区 */
    taskEXIT_CRITICAL();

    /* 返回获取到的通知值 */
    return ulReturn;
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xTaskNotifyWait
 * 功能描述：等待任务通知的核心函数，允许任务阻塞等待通知到达或超时
 *           此函数提供了一种轻量级的任务同步机制，比传统的信号量或事件组更高效
 * 输入参数：
 *   - ulBitsToClearOnEntry: 进入等待时要从通知值中清除的位掩码
 *   - ulBitsToClearOnExit: 退出等待时要从通知值中清除的位掩码
 *   - pulNotificationValue: 指向通知值的指针，用于返回接收到的通知值
 *   - xTicksToWait: 最大等待时间（以系统节拍为单位），可以是portMAX_DELAY表示无限等待
 * 输出参数：
 *   - pulNotificationValue: 如果非NULL，返回接收到的通知值
 * 返回值：
 *   - BaseType_t: 返回等待操作的结果
 *                 pdTRUE表示成功接收到通知
 *                 pdFALSE表示等待超时或未接收到通知
 * 其它说明：
 *   - 此函数仅在configUSE_TASK_NOTIFICATIONS为1时编译，是任务通知功能的一部分
 *   - 提供灵活的位清除机制，可以在进入和退出等待时清除通知值的特定位
 *   - 支持超时机制，可以指定最大等待时间或无限等待
 *   - 使用临界区保护状态操作，确保在多任务环境下的线程安全性
 *   - 提供跟踪功能，便于调试和性能分析
 *   - 比传统的信号量或事件组更高效，占用资源更少
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
#if( configUSE_TASK_NOTIFICATIONS == 1 )

BaseType_t xTaskNotifyWait( uint32_t ulBitsToClearOnEntry, uint32_t ulBitsToClearOnExit, uint32_t *pulNotificationValue, TickType_t xTicksToWait )
{
    /* 存储函数返回结果 */
    BaseType_t xReturn;

    /* 进入临界区，保护状态操作的原子性 */
    taskENTER_CRITICAL();
    {
        /* 只有在没有通知挂起时才阻塞 */
        if( pxCurrentTCB->ucNotifyState != taskNOTIFICATION_RECEIVED )
        {
            /* 清除任务通知值中的位，因为这些位可能会被通知任务或中断设置
               这可以用于将值清零或清除特定位 */
            pxCurrentTCB->ulNotifiedValue &= ~ulBitsToClearOnEntry;

            /* 将此任务标记为等待通知 */
            pxCurrentTCB->ucNotifyState = taskWAITING_NOTIFICATION;

            /* 检查等待时间是否大于0 */
            if( xTicksToWait > ( TickType_t ) 0 )
            {
                /* 将当前任务添加到延迟列表，允许无限期阻塞 */
                prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
                /* 记录任务通知等待阻塞的跟踪信息 */
                traceTASK_NOTIFY_WAIT_BLOCK();

                /* 所有端口都编写为允许在临界section中进行yield
                   （有些会立即yield，其他则等待临界section退出）
                   但这不是应用程序代码应该做的事情 */
                portYIELD_WITHIN_API();
            }
            else
            {
                /* 代码覆盖测试标记（等待时间为0的情况） */
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            /* 代码覆盖测试标记（已有通知挂起的情况） */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    /* 退出临界区 */
    taskEXIT_CRITICAL();

    /* 再次进入临界区，处理等待结果 */
    taskENTER_CRITICAL();
    {
        /* 记录任务通知等待的跟踪信息 */
        traceTASK_NOTIFY_WAIT();

        /* 如果提供了通知值指针，输出当前通知值（可能已更改或未更改） */
        if( pulNotificationValue != NULL )
        {
            *pulNotificationValue = pxCurrentTCB->ulNotifiedValue;
        }

        /* 如果ucNotifyValue设置为taskWAITING_NOTIFICATION，则任务要么从未进入阻塞状态
           （因为已经有通知挂起），要么任务因通知而解除阻塞。否则任务因超时而解除阻塞 */
        if( pxCurrentTCB->ucNotifyState == taskWAITING_NOTIFICATION )
        {
            /* 未收到通知（超时） */
            xReturn = pdFALSE;
        }
        else
        {
            /* 已经有通知挂起或任务在等待时收到了通知 */
            pxCurrentTCB->ulNotifiedValue &= ~ulBitsToClearOnExit;
            xReturn = pdTRUE;
        }

        /* 将任务通知状态重置为"未等待通知" */
        pxCurrentTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION;
    }
    /* 退出临界区 */
    taskEXIT_CRITICAL();

    /* 返回等待操作的结果 */
    return xReturn;
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xTaskGenericNotify
 * 功能描述：从任务上下文中向指定任务发送通用通知的核心函数
 *           此函数是任务通知机制的通用任务版本，支持多种通知动作和灵活的参数配置
 *           用于在任务上下文中更新任务的通知值、状态，并可能解除任务的阻塞状态
 * 输入参数：
 *   - xTaskToNotify: 要通知的任务句柄，指向需要接收通知的任务对象
 *   - ulValue: 通知值，根据不同的通知动作具有不同的含义
 *   - eAction: 通知动作枚举，指定如何更新任务的通知值
 *   - pulPreviousNotificationValue: 指向先前通知值的指针，用于返回更新前的通知值
 * 输出参数：
 *   - pulPreviousNotificationValue: 如果非NULL，返回更新前的任务通知值
 * 返回值：
 *   - BaseType_t: 返回通知操作的结果
 *                 pdPASS表示通知操作成功完成
 *                 pdFAIL表示某些操作失败（如eSetValueWithoutOverwrite时已有通知）
 * 其它说明：
 *   - 此函数仅在configUSE_TASK_NOTIFICATIONS为1时编译，是任务通知功能的一部分
 *   - 专为任务上下文设计，使用临界区保护确保线程安全
 *   - 支持多种通知动作：设置位、递增、带覆盖设置值、不带覆盖设置值、无动作
 *   - 可选择性返回先前的通知值，便于应用程序处理
 *   - 如果任务正在等待通知，则解除其阻塞状态并将其添加到就绪列表
 *   - 检查是否需要任务切换，并在必要时触发任务切换
 *   - 支持无滴答空闲模式，确保在任务解除阻塞时正确更新下一个任务解除阻塞时间
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
#if( configUSE_TASK_NOTIFICATIONS == 1 )

BaseType_t xTaskGenericNotify( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, uint32_t *pulPreviousNotificationValue )
{
    /* 指向任务控制块的指针 */
    TCB_t * pxTCB;
    /* 存储函数返回结果，默认为成功 */
    BaseType_t xReturn = pdPASS;
    /* 存储原始的通知状态 */
    uint8_t ucOriginalNotifyState;

    /* 断言检查确保任务句柄有效 */
    configASSERT( xTaskToNotify );
    /* 将任务句柄转换为任务控制块指针 */
    pxTCB = ( TCB_t * ) xTaskToNotify;

    /* 进入临界区，保护通知操作的原子性 */
    taskENTER_CRITICAL();
    {
        /* 如果提供了先前通知值指针，则保存当前通知值 */
        if( pulPreviousNotificationValue != NULL )
        {
            *pulPreviousNotificationValue = pxTCB->ulNotifiedValue;
        }

        /* 保存原始的通知状态 */
        ucOriginalNotifyState = pxTCB->ucNotifyState;
        /* 将任务通知状态设置为"已接收通知" */
        pxTCB->ucNotifyState = taskNOTIFICATION_RECEIVED;

        /* 根据通知动作类型更新任务的通知值 */
        switch( eAction )
        {
            case eSetBits : /* 设置位：使用按位或操作更新通知值 */
                pxTCB->ulNotifiedValue |= ulValue;
                break;

            case eIncrement : /* 递增：增加通知值（类似计数信号量） */
                ( pxTCB->ulNotifiedValue )++;
                break;

            case eSetValueWithOverwrite : /* 带覆盖设置值：直接覆盖通知值 */
                pxTCB->ulNotifiedValue = ulValue;
                break;

            case eSetValueWithoutOverwrite : /* 不带覆盖设置值：仅在任务未处于"已接收通知"状态时设置值 */
                if( ucOriginalNotifyState != taskNOTIFICATION_RECEIVED )
                {
                    pxTCB->ulNotifiedValue = ulValue;
                }
                else
                {
                    /* 无法将值写入任务（已有通知未处理） */
                    xReturn = pdFAIL;
                }
                break;

            case eNoAction: /* 无动作：只更新通知状态，不修改通知值 */
                /* 任务被通知但不更新其通知值 */
                break;
        }

        /* 记录任务通知的跟踪信息 */
        traceTASK_NOTIFY();

        /* 如果任务处于阻塞状态专门等待通知，则立即解除其阻塞 */
        if( ucOriginalNotifyState == taskWAITING_NOTIFICATION )
        {
            /* 从当前状态列表（阻塞列表）中移除任务 */
            ( void ) uxListRemove( &( pxTCB->xStateListItem ) );
            /* 将任务添加到就绪列表 */
            prvAddTaskToReadyList( pxTCB );

            /* 任务不应该在事件列表上 */
            configASSERT( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) == NULL );

            /* 检查是否启用了无滴答空闲模式 */
            #if( configUSE_TICKLESS_IDLE != 0 )
            {
                /* 如果任务因等待通知而被阻塞，那么xNextTaskUnblockTime可能设置为阻塞任务的超时时间
                   如果任务因超时以外的原因解除阻塞，xNextTaskUnblockTime通常保持不变，
                   因为当滴答计数等于xNextTaskUnblockTime时，它会自动重置为新值
                   但是，如果使用无滴答空闲模式，可能更重要的是在尽可能早的时间进入睡眠模式
                   因此在这里重置xNextTaskUnblockTime以确保它在尽可能早的时间更新 */
                prvResetNextTaskUnblockTime();
            }
            #endif

            /* 检查被通知任务的优先级是否高于当前执行任务 */
            if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
            {
                /* 被通知的任务的优先级高于当前执行的任务，因此需要 yield */
                taskYIELD_IF_USING_PREEMPTION();
            }
            else
            {
                /* 代码覆盖测试标记（优先级不高于当前任务的情况） */
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            /* 代码覆盖测试标记（任务不在等待通知状态的情况） */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    /* 退出临界区 */
    taskEXIT_CRITICAL();

    /* 返回通知操作的结果 */
    return xReturn;
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xTaskGenericNotifyFromISR
 * 功能描述：从中断服务程序(ISR)中向任务发送通用通知的内部函数
 *           此函数是任务通知机制的通用ISR版本，支持多种通知动作和灵活的参数配置
 *           用于在中断上下文中更新任务的通知值、状态，并可能解除任务的阻塞状态
 * 输入参数：
 *   - xTaskToNotify: 要通知的任务句柄，指向需要接收通知的任务对象
 *   - ulValue: 通知值，根据不同的通知动作具有不同的含义
 *   - eAction: 通知动作枚举，指定如何更新任务的通知值
 *   - pulPreviousNotificationValue: 指向先前通知值的指针，用于返回更新前的通知值
 *   - pxHigherPriorityTaskWoken: 指向更高优先级任务唤醒标志的指针
 * 输出参数：
 *   - pulPreviousNotificationValue: 如果非NULL，返回更新前的任务通知值
 *   - pxHigherPriorityTaskWoken: 如果通知操作导致更高优先级任务就绪，则设置为pdTRUE
 * 返回值：
 *   - BaseType_t: 返回通知操作的结果
 *                 pdPASS表示通知操作成功完成
 *                 pdFAIL表示某些操作失败（如eSetValueWithoutOverwrite时已有通知）
 * 其它说明：
 *   - 此函数仅在configUSE_TASK_NOTIFICATIONS为1时编译，是任务通知功能的一部分
 *   - 专为中断服务程序设计，使用中断安全的API和操作
 *   - 支持多种通知动作：设置位、递增、带覆盖设置值、不带覆盖设置值、无动作
 *   - 可选择性返回先前的通知值，便于应用程序处理
 *   - 如果任务正在等待通知，则解除其阻塞状态
 *   - 检查是否需要任务切换，并通过参数通知调用者
 *   - 使用中断掩码保护关键操作，确保在中断上下文中的线程安全
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
#if( configUSE_TASK_NOTIFICATIONS == 1 )

BaseType_t xTaskGenericNotifyFromISR( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, uint32_t *pulPreviousNotificationValue, BaseType_t *pxHigherPriorityTaskWoken )
{
    /* 指向任务控制块的指针 */
    TCB_t * pxTCB;
    /* 存储原始的通知状态 */
    uint8_t ucOriginalNotifyState;
    /* 存储函数返回结果，默认为成功 */
    BaseType_t xReturn = pdPASS;
    /* 存储中断状态，用于恢复中断掩码 */
    UBaseType_t uxSavedInterruptStatus;

    /* 断言检查确保任务句柄有效 */
    configASSERT( xTaskToNotify );

    /* 支持中断嵌套的RTOS端口有最大系统调用（或最大API调用）中断优先级的概念
       高于最大系统调用优先级的中断保持永久启用，即使RTOS内核处于临界section
       但不能调用任何FreeRTOS API函数。如果在FreeRTOSConfig.h中定义了configASSERT()
       那么如果从中断调用FreeRTOS API函数，portASSERT_IF_INTERRUPT_PRIORITY_INVALID()
       将导致断言失败，该中断已被分配高于配置的最大系统调用优先级。
       只有以FromISR结尾的FreeRTOS函数可以从已被分配优先级等于或（逻辑上）
       低于最大系统调用中断优先级的中断调用。FreeRTOS维护一个单独的中断安全API
       以确保中断入口尽可能快速和简单。更多信息（尽管是Cortex-M特定的）
       在以下链接提供：http://www.freertos.org/RTOS-Cortex-M3-M4.html */
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    /* 将任务句柄转换为任务控制块指针 */
    pxTCB = ( TCB_t * ) xTaskToNotify;

    /* 保存当前中断状态并设置中断掩码 */
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        /* 如果提供了先前通知值指针，则保存当前通知值 */
        if( pulPreviousNotificationValue != NULL )
        {
            *pulPreviousNotificationValue = pxTCB->ulNotifiedValue;
        }

        /* 保存原始的通知状态 */
        ucOriginalNotifyState = pxTCB->ucNotifyState;
        /* 将任务通知状态设置为"已接收通知" */
        pxTCB->ucNotifyState = taskNOTIFICATION_RECEIVED;

        /* 根据通知动作类型更新任务的通知值 */
        switch( eAction )
        {
            case eSetBits : /* 设置位：使用按位或操作更新通知值 */
                pxTCB->ulNotifiedValue |= ulValue;
                break;

            case eIncrement : /* 递增：增加通知值（类似计数信号量） */
                ( pxTCB->ulNotifiedValue )++;
                break;

            case eSetValueWithOverwrite : /* 带覆盖设置值：直接覆盖通知值 */
                pxTCB->ulNotifiedValue = ulValue;
                break;

            case eSetValueWithoutOverwrite : /* 不带覆盖设置值：仅在任务未处于"已接收通知"状态时设置值 */
                if( ucOriginalNotifyState != taskNOTIFICATION_RECEIVED )
                {
                    pxTCB->ulNotifiedValue = ulValue;
                }
                else
                {
                    /* 无法将值写入任务（已有通知未处理） */
                    xReturn = pdFAIL;
                }
                break;

            case eNoAction : /* 无动作：只更新通知状态，不修改通知值 */
                /* 任务被通知但不更新其通知值 */
                break;
        }

        /* 记录任务通知的跟踪信息 */
        traceTASK_NOTIFY_FROM_ISR();

        /* 如果任务处于阻塞状态专门等待通知，则立即解除其阻塞 */
        if( ucOriginalNotifyState == taskWAITING_NOTIFICATION )
        {
            /* 任务不应该在事件列表上 */
            configASSERT( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) == NULL );

            /* 检查调度器是否被挂起 */
            if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
            {
                /* 从当前状态列表（阻塞列表）中移除任务 */
                ( void ) uxListRemove( &( pxTCB->xStateListItem ) );
                /* 将任务添加到就绪列表 */
                prvAddTaskToReadyList( pxTCB );
            }
            else
            {
                /* 无法访问延迟和就绪列表，因此将此任务挂起直到调度器恢复 */
                vListInsertEnd( &( xPendingReadyList ), &( pxTCB->xEventListItem ) );
            }

            /* 检查被通知任务的优先级是否高于当前执行任务 */
            if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
            {
                /* 被通知的任务的优先级高于当前执行的任务，因此需要 yield */
                if( pxHigherPriorityTaskWoken != NULL )
                {
                    /* 设置更高优先级任务唤醒标志 */
                    *pxHigherPriorityTaskWoken = pdTRUE;
                }
                else
                {
                    /* 标记有 yield 待处理，以防用户未在ISR安全的FreeRTOS函数中使用
                       "xHigherPriorityTaskWoken"参数 */
                    xYieldPending = pdTRUE;
                }
            }
            else
            {
                /* 代码覆盖测试标记（优先级不高于当前任务的情况） */
                mtCOVERAGE_TEST_MARKER();
            }
        }
    }
    /* 清除中断掩码并恢复之前的中断状态 */
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

    /* 返回通知操作的结果 */
    return xReturn;
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vTaskNotifyGiveFromISR
 * 功能描述：从中断服务程序(ISR)中向任务发送通知的内部函数
 *           此函数是任务通知机制的ISR版本，用于在中断上下文中增加任务的通知值
 *           并可能解除任务的阻塞状态，类似于计数信号量的"give"操作
 * 输入参数：
 *   - xTaskToNotify: 要通知的任务句柄，指向需要接收通知的任务对象
 *   - pxHigherPriorityTaskWoken: 指向更高优先级任务唤醒标志的指针
 * 输出参数：
 *   - pxHigherPriorityTaskWoken: 如果通知操作导致更高优先级任务就绪，则设置为pdTRUE
 * 返回值：无
 * 其它说明：
 *   - 此函数仅在configUSE_TASK_NOTIFICATIONS为1时编译，是任务通知功能的一部分
 *   - 专为中断服务程序设计，使用中断安全的API和操作
 *   - 增加任务的通知值，类似于计数信号量的递增操作
 *   - 如果任务正在等待通知，则解除其阻塞状态
 *   - 检查是否需要任务切换，并通过参数通知调用者
 *   - 使用中断掩码保护关键操作，确保在中断上下文中的线程安全
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
#if( configUSE_TASK_NOTIFICATIONS == 1 )

void vTaskNotifyGiveFromISR( TaskHandle_t xTaskToNotify, BaseType_t *pxHigherPriorityTaskWoken )
{
    /* 指向任务控制块的指针 */
    TCB_t * pxTCB;
    /* 存储原始的通知状态 */
    uint8_t ucOriginalNotifyState;
    /* 存储中断状态，用于恢复中断掩码 */
    UBaseType_t uxSavedInterruptStatus;

    /* 断言检查确保任务句柄有效 */
    configASSERT( xTaskToNotify );

    /* 支持中断嵌套的RTOS端口有最大系统调用（或最大API调用）中断优先级的概念
       高于最大系统调用优先级的中断保持永久启用，即使RTOS内核处于临界section
       但不能调用任何FreeRTOS API函数。如果在FreeRTOSConfig.h中定义了configASSERT()
       那么如果从中断调用FreeRTOS API函数，portASSERT_IF_INTERRUPT_PRIORITY_INVALID()
       将导致断言失败，该中断已被分配高于配置的最大系统调用优先级。
       只有以FromISR结尾的FreeRTOS函数可以从已被分配优先级等于或（逻辑上）
       低于最大系统调用中断优先级的中断调用。FreeRTOS维护一个单独的中断安全API
       以确保中断入口尽可能快速和简单。更多信息（尽管是Cortex-M特定的）
       在以下链接提供：http://www.freertos.org/RTOS-Cortex-M3-M4.html */
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    /* 将任务句柄转换为任务控制块指针 */
    pxTCB = ( TCB_t * ) xTaskToNotify;

    /* 保存当前中断状态并设置中断掩码 */
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        /* 保存原始的通知状态 */
        ucOriginalNotifyState = pxTCB->ucNotifyState;
        /* 将任务通知状态设置为"已接收通知" */
        pxTCB->ucNotifyState = taskNOTIFICATION_RECEIVED;

        /* '给予'通知相当于增加计数信号量中的计数 */
        ( pxTCB->ulNotifiedValue )++;

        /* 记录任务通知给予的跟踪信息 */
        traceTASK_NOTIFY_GIVE_FROM_ISR();

        /* 如果任务处于阻塞状态专门等待通知，则立即解除其阻塞 */
        if( ucOriginalNotifyState == taskWAITING_NOTIFICATION )
        {
            /* 任务不应该在事件列表上 */
            configASSERT( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) == NULL );

            /* 检查调度器是否被挂起 */
            if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
            {
                /* 从当前状态列表（阻塞列表）中移除任务 */
                ( void ) uxListRemove( &( pxTCB->xStateListItem ) );
                /* 将任务添加到就绪列表 */
                prvAddTaskToReadyList( pxTCB );
            }
            else
            {
                /* 无法访问延迟和就绪列表，因此将此任务挂起直到调度器恢复 */
                vListInsertEnd( &( xPendingReadyList ), &( pxTCB->xEventListItem ) );
            }

            /* 检查被通知任务的优先级是否高于当前执行任务 */
            if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
            {
                /* 被通知的任务的优先级高于当前执行的任务，因此需要 yield */
                if( pxHigherPriorityTaskWoken != NULL )
                {
                    /* 设置更高优先级任务唤醒标志 */
                    *pxHigherPriorityTaskWoken = pdTRUE;
                }
                else
                {
                    /* 标记有 yield 待处理，以防用户未在ISR安全的FreeRTOS函数中使用
                       "xHigherPriorityTaskWoken"参数 */
                    xYieldPending = pdTRUE;
                }
            }
            else
            {
                /* 代码覆盖测试标记（优先级不高于当前任务的情况） */
                mtCOVERAGE_TEST_MARKER();
            }
        }
    }
    /* 清除中断掩码并恢复之前的中断状态 */
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );
}

#endif /* configUSE_TASK_NOTIFICATIONS */

/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xTaskNotifyStateClear
 * 功能描述：清除任务通知状态的核心函数
 *           此函数用于将任务的通知状态从"已接收通知"重置为"未等待通知"
 *           主要用于任务通知机制中，当需要重置任务的通知状态时调用
 * 输入参数：
 *   - xTask: 要清除通知状态的任务句柄
 *            如果传入NULL，则表示清除当前调用任务的通知状态
 * 输出参数：无
 * 返回值：
 *   - BaseType_t: 返回状态清除操作的结果
 *                 pdPASS表示成功清除了任务的通知状态
 *                 pdFAIL表示清除失败（任务当前不处于"已接收通知"状态）
 * 其它说明：
 *   - 此函数仅在configUSE_TASK_NOTIFICATIONS为1时编译，是任务通知功能的一部分
 *   - 使用临界区保护状态清除操作，确保在多任务环境下的线程安全性
 *   - 通过任务句柄获取任务控制块，支持清除任意任务或当前任务的通知状态
 *   - 主要用于任务通知机制中，当处理完通知后需要重置任务状态时调用
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
#if( configUSE_TASK_NOTIFICATIONS == 1 )

BaseType_t xTaskNotifyStateClear( TaskHandle_t xTask )
{
    /* 指向任务控制块的指针 */
    TCB_t *pxTCB;
    /* 存储函数返回结果 */
    BaseType_t xReturn;

    /* 如果此处传入null，则表示要清除通知状态的是调用任务本身 */
    pxTCB = prvGetTCBFromHandle( xTask );

    /* 进入临界区，保护状态清除操作的原子性 */
    taskENTER_CRITICAL();
    {
        /* 检查任务当前是否处于"已接收通知"状态 */
        if( pxTCB->ucNotifyState == taskNOTIFICATION_RECEIVED )
        {
            /* 将任务通知状态重置为"未等待通知" */
            pxTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION;
            /* 设置返回值为成功 */
            xReturn = pdPASS;
        }
        else
        {
            /* 设置返回值为失败 */
            xReturn = pdFAIL;
        }
    }
    /* 退出临界区 */
    taskEXIT_CRITICAL();

    /* 返回状态清除操作的结果 */
    return xReturn;
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/


/*******************************************************************************
 * 函数名称：prvAddCurrentTaskToDelayedList
 * 功能描述：将当前任务添加到延迟列表（阻塞列表）的内部函数
 *           根据等待时间计算任务的唤醒时间，并将任务添加到相应的延迟列表（正常列表或溢出列表）
 *           同时处理无限期阻塞的情况，将任务挂起而不是添加到延迟列表
 * 输入参数：
 *   - xTicksToWait: 任务需要等待的时钟节拍数
 *   - xCanBlockIndefinitely: 指示任务是否可以无限期阻塞的标志
 *                            pdTRUE表示可以无限期阻塞，pdFALSE表示不能
 * 输出参数：无
 * 返回值：无
 * 其它说明：
 *   - 此函数是任务调度器的核心部分，负责管理任务的阻塞状态
 *   - 处理系统节拍计数溢出情况，正确计算任务的唤醒时间
 *   - 支持任务延迟中止功能（如果启用INCLUDE_xTaskAbortDelay）
 *   - 根据等待时间是否溢出，将任务添加到不同的延迟列表
 *   - 更新下一个任务解除阻塞时间，优化调度器性能
 *   - 使用条件编译适配不同的功能配置（如任务挂起、延迟中止等）
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
static void prvAddCurrentTaskToDelayedList( TickType_t xTicksToWait, const BaseType_t xCanBlockIndefinitely )
{
    /* 存储任务应该被唤醒的时间 */
    TickType_t xTimeToWake;
    /* 存储当前的节拍计数值，避免在计算过程中值发生变化 */
    const TickType_t xConstTickCount = xTickCount;

    /* 检查是否启用了任务延迟中止功能 */
    #if( INCLUDE_xTaskAbortDelay == 1 )
    {
        /* 即将进入延迟列表，因此确保ucDelayAborted标志重置为pdFALSE
           这样可以在任务离开阻塞状态时检测到它是否被设置为pdTRUE */
        pxCurrentTCB->ucDelayAborted = pdFALSE;
    }
    #endif

    /* 在将任务添加到阻塞列表之前，先将其从就绪列表中移除
       因为同一个列表项用于这两个列表 */
    if( uxListRemove( &( pxCurrentTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
    {
        /* 当前任务必须在就绪列表中，因此不需要检查，可以直接调用端口重置宏 */
        portRESET_READY_PRIORITY( pxCurrentTCB->uxPriority, uxTopReadyPriority );
    }
    else
    {
        /* 代码覆盖测试标记（任务不在就绪列表中的情况） */
        mtCOVERAGE_TEST_MARKER();
    }

    /* 检查是否启用了任务挂起功能 */
    #if ( INCLUDE_vTaskSuspend == 1 )
    {
        /* 检查是否是无限期阻塞且允许无限期阻塞 */
        if( ( xTicksToWait == portMAX_DELAY ) && ( xCanBlockIndefinitely != pdFALSE ) )
        {
            /* 将任务添加到挂起任务列表而不是延迟任务列表
               确保它不会被定时事件唤醒，它将无限期阻塞 */
            vListInsertEnd( &xSuspendedTaskList, &( pxCurrentTCB->xStateListItem ) );
        }
        else
        {
            /* 计算如果事件未发生，任务应该被唤醒的时间
               这可能会溢出，但这没关系，内核会正确管理它 */
            xTimeToWake = xConstTickCount + xTicksToWait;

            /* 列表项将按唤醒时间顺序插入 */
            listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xStateListItem ), xTimeToWake );

            /* 检查唤醒时间是否小于当前时间（溢出情况） */
            if( xTimeToWake < xConstTickCount )
            {
                /* 唤醒时间已溢出，将此项目放在溢出列表中 */
                vListInsert( pxOverflowDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );
            }
            else
            {
                /* 唤醒时间未溢出，因此使用当前阻塞列表 */
                vListInsert( pxDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );

                /* 如果进入阻塞状态的任务被放置在阻塞任务列表的头部
                   那么xNextTaskUnblockTime也需要更新 */
                if( xTimeToWake < xNextTaskUnblockTime )
                {
                    xNextTaskUnblockTime = xTimeToWake;
                }
                else
                {
                    /* 代码覆盖测试标记（不需要更新xNextTaskUnblockTime的情况） */
                    mtCOVERAGE_TEST_MARKER();
                }
            }
        }
    }
    #else /* INCLUDE_vTaskSuspend */
    {
        /* 计算如果事件未发生，任务应该被唤醒的时间
           这可能会溢出，但这没关系，内核会正确管理它 */
        xTimeToWake = xConstTickCount + xTicksToWait;

        /* 列表项将按唤醒时间顺序插入 */
        listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xStateListItem ), xTimeToWake );

        /* 检查唤醒时间是否小于当前时间（溢出情况） */
        if( xTimeToWake < xConstTickCount )
        {
            /* 唤醒时间已溢出，将此项目放在溢出列表中 */
            vListInsert( pxOverflowDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );
        }
        else
        {
            /* 唤醒时间未溢出，因此使用当前阻塞列表 */
            vListInsert( pxDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );

            /* 如果进入阻塞状态的任务被放置在阻塞任务列表的头部
               那么xNextTaskUnblockTime也需要更新 */
            if( xTimeToWake < xNextTaskUnblockTime )
            {
                xNextTaskUnblockTime = xTimeToWake;
            }
            else
            {
                /* 代码覆盖测试标记（不需要更新xNextTaskUnblockTime的情况） */
                mtCOVERAGE_TEST_MARKER();
            }
        }

        /* 当INCLUDE_vTaskSuspend不为1时避免编译器警告 */
        ( void ) xCanBlockIndefinitely;
    }
    #endif /* INCLUDE_vTaskSuspend */
}


#ifdef FREERTOS_MODULE_TEST
	#include "tasks_test_access_functions.h"
#endif


TCB_t *test_tskTCB1 = NULL;
TCB_t *test_tsKTCB2 = NULL; 
TCB_t *test_tsKTCB3 = NULL;


void debugs_test( TaskHandle_t * pxCreatedTask_Debug1 ,TaskHandle_t * pxCreatedTask_Debug2 ,TaskHandle_t * pxCreatedTask_Debug3)
{
	test_tskTCB1 = (tskTCB*)pxCreatedTask_Debug1;
	test_tsKTCB2 = (tskTCB*)pxCreatedTask_Debug2;
  test_tsKTCB3 = (tskTCB*)pxCreatedTask_Debug3;
}



/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： LM_timers.c
 * 文件标识： 
 * 内容摘要： 定时器模块定义
 * 其它说明： 无
 * 当前版本： FreeRTOS V9.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年09月01日
 *
 *******************************************************************************/

/* Includes ------------------------------------------------------------------*/
/* Standard includes. */
#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

/* Exported types ------------------------------------------------------------*/
/**
 * 定时器控制结构体定义
 * 包含定时器的所有属性和配置信息
 */
 #if ( configUSE_TIMERS == 1 )
 
typedef struct tmrTimerControl
{
	const char				*pcTimerName;		/*<< 文本名称，用于调试目的 */
	ListItem_t				xTimerListItem;		/*<< 标准链表项，用于事件管理 */
	TickType_t				xTimerPeriodInTicks;/*<< 定时器到期的时间间隔（滴答数） */
	UBaseType_t				uxAutoReload;		/*<< 设置为pdTRUE表示定时器自动重载，pdFALSE表示单次定时器 */
	void 					*pvTimerID;			/*<< 定时器标识符，允许多个定时器使用相同回调函数时进行区分 */
	TimerCallbackFunction_t	pxCallbackFunction;	/*<< 定时器到期时调用的函数 */
	#if( configUSE_TRACE_FACILITY == 1 )
		UBaseType_t			uxTimerNumber;		/*<< 跟踪工具分配的ID */
	#endif

	#if( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
		uint8_t 			ucStaticallyAllocated; /*<< 如果定时器是静态创建的，设置为pdTRUE，避免删除时尝试释放内存 */
	#endif
} xTIMER;

/**
 * 定时器类型定义
 * 保持与旧版本的兼容性
 */
typedef xTIMER Timer_t;

/**
 * 定时器参数结构体定义
 * 用于定时器队列消息中操作定时器的参数
 */
typedef struct tmrTimerParameters
{
	TickType_t			xMessageValue;		/*<< 可选值，用于某些命令，如更改定时器周期 */
	Timer_t *			pxTimer;			/*<< 要应用命令的定时器 */
} TimerParameter_t;

/**
 * 回调参数结构体定义
 * 用于定时器队列消息中执行非定时器相关回调的参数
 */
typedef struct tmrCallbackParameters
{
	PendedFunction_t	pxCallbackFunction;	/*<< 要执行的回调函数 */
	void *pvParameter1;						/*<< 回调函数的第一个参数值 */
	uint32_t ulParameter2;					/*<< 回调函数的第二个参数值 */
} CallbackParameters_t;

/**
 * 定时器队列消息结构体定义
 * 包含两种消息类型以及用于确定哪种消息类型有效的标识符
 */
typedef struct tmrTimerQueueMessage
{
	BaseType_t			xMessageID;			/*<< 发送给定时器服务任务的命令 */
	union
	{
		TimerParameter_t xTimerParameters;

		#if ( INCLUDE_xTimerPendFunctionCall == 1 )
			CallbackParameters_t xCallbackParameters;
		#endif /* INCLUDE_xTimerPendFunctionCall */
	} u;
} DaemonTaskMessage_t;

/* Exported constants --------------------------------------------------------*/
/**
 * 无延迟常量定义
 * 用于表示不延迟的定时器操作
 */
#define tmrNO_DELAY		( TickType_t ) 0U

/* Exported macro ------------------------------------------------------------*/
/* 注：定时器模块没有导出的宏定义 */

/* Exported functions --------------------------------------------------------*/
/* 注：定时器模块的API函数在timers.h中声明，此处不重复 */

/* Private types -------------------------------------------------------------*/
/* 注：私有类型定义已包含在导出类型中 */

/* Private variables ---------------------------------------------------------*/
/*lint -e956 手动分析确定哪些静态变量必须声明为volatile */

/**
 * 活动定时器列表
 * 存储活动定时器的列表，按到期时间排序，最近到期时间在前
 * 只有定时器服务任务可以访问这些列表
 */
PRIVILEGED_DATA static List_t xActiveTimerList1;
PRIVILEGED_DATA static List_t xActiveTimerList2;
PRIVILEGED_DATA static List_t *pxCurrentTimerList;
PRIVILEGED_DATA static List_t *pxOverflowTimerList;

/**
 * 定时器队列
 * 用于向定时器服务任务发送命令的队列
 */
PRIVILEGED_DATA static QueueHandle_t xTimerQueue = NULL;

/**
 * 定时器任务句柄
 * 定时器服务任务的句柄
 */
PRIVILEGED_DATA static TaskHandle_t xTimerTaskHandle = NULL;

/*lint +e956 */

/* Private constants ---------------------------------------------------------*/
/* 注：定时器模块没有私有常量定义 */

/* Private macros ------------------------------------------------------------*/
/* 注：定时器模块没有私有宏定义 */

/* Private functions ---------------------------------------------------------*/
/**
 * 检查定时器列表和队列的有效性
 * 如果尚未初始化，则初始化定时器服务任务使用的基础设施
 */
static void prvCheckForValidListAndQueue( void ) PRIVILEGED_FUNCTION;

/**
 * 定时器服务任务（守护进程）
 * 定时器功能由此任务控制，其他任务使用xTimerQueue队列与定时器服务任务通信
 * @param pvParameters 任务参数
 */
static void prvTimerTask( void *pvParameters ) PRIVILEGED_FUNCTION;

/**
 * 处理接收到的命令
 * 由定时器服务任务调用，解释和处理在定时器队列上接收到的命令
 */
static void prvProcessReceivedCommands( void ) PRIVILEGED_FUNCTION;

/**
 * 将定时器插入活动列表
 * 根据到期时间是否导致定时器计数器溢出，将定时器插入xActiveTimerList1或xActiveTimerList2
 * @param pxTimer 要插入的定时器
 * @param xNextExpiryTime 下一个到期时间
 * @param xTimeNow 当前时间
 * @param xCommandTime 命令时间
 * @return 插入结果
 */
static BaseType_t prvInsertTimerInActiveList( Timer_t * const pxTimer, const TickType_t xNextExpiryTime, const TickType_t xTimeNow, const TickType_t xCommandTime ) PRIVILEGED_FUNCTION;

/**
 * 处理到期定时器
 * 活动定时器已达到其到期时间，如果是自动重载定时器则重新加载，然后调用其回调函数
 * @param xNextExpireTime 下一个到期时间
 * @param xTimeNow 当前时间
 */
static void prvProcessExpiredTimer( const TickType_t xNextExpireTime, const TickType_t xTimeNow ) PRIVILEGED_FUNCTION;

/**
 * 切换定时器列表
 * 滴答计数已溢出，在确保当前定时器列表不再引用某些定时器后切换定时器列表
 */
static void prvSwitchTimerLists( void ) PRIVILEGED_FUNCTION;

/**
 * 获取当前滴答计数
 * 如果自上次调用prvSampleTimeNow()以来发生了滴答计数溢出，则将pxTimerListsWereSwitched设置为pdTRUE
 * @param pxTimerListsWereSwitched 指示是否发生列表切换的指针
 * @return 当前滴答计数
 */
static TickType_t prvSampleTimeNow( BaseType_t * const pxTimerListsWereSwitched ) PRIVILEGED_FUNCTION;

/**
 * 获取下一个到期时间
 * 如果定时器列表包含任何活动定时器，则返回最先到期的定时器的到期时间并将pxListWasEmpty设置为false
 * 如果定时器列表不包含任何定时器，则返回0并将pxListWasEmpty设置为pdTRUE
 * @param pxListWasEmpty 指示列表是否为空的指针
 * @return 下一个到期时间
 */
static TickType_t prvGetNextExpireTime( BaseType_t * const pxListWasEmpty ) PRIVILEGED_FUNCTION;

/**
 * 处理定时器或阻塞任务
 * 如果定时器已到期，则处理它；否则，阻塞定时器服务任务，直到定时器到期或收到命令
 * @param xNextExpireTime 下一个到期时间
 * @param xListWasEmpty 列表是否为空
 */
static void prvProcessTimerOrBlockTask( const TickType_t xNextExpireTime, BaseType_t xListWasEmpty ) PRIVILEGED_FUNCTION;

static void prvInitialiseNewTimer(	const char * const pcTimerName,
									const TickType_t xTimerPeriodInTicks,
									const UBaseType_t uxAutoReload,
									void * const pvTimerID,
									TimerCallbackFunction_t pxCallbackFunction,
									Timer_t *pxNewTimer ) PRIVILEGED_FUNCTION; 
/*******************************************************************************
 * 函数名称：xTimerCreateTimerTask
 * 功能描述：创建FreeRTOS定时器服务任务，用于处理软件定时器的到期和回调函数执行
 *           此函数在调度器启动时自动调用，负责初始化定时器服务任务所需的基础设施
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：
 *   - BaseType_t: 任务创建结果，pdPASS表示成功，pdFAIL表示失败
 * 其它说明：
 *   - 此函数在调度器启动时自动调用，当configUSE_TIMERS设置为1时启用
 *   - 首先检查定时器服务任务所需的基础设施是否已创建/初始化
 *   - 支持静态和动态两种内存分配方式创建定时器任务
 *   - 定时器任务以特权模式运行，具有较高的优先级
 *   - 如果定时器队列创建失败，则无法创建定时器任务
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
BaseType_t xTimerCreateTimerTask( void )
{
    BaseType_t xReturn = pdFAIL;  /* 返回值，初始化为失败状态 */

    /* 当调度器启动且configUSE_TIMERS设置为1时调用此函数。
       检查定时器服务任务使用的基础设施是否已创建/初始化。
       如果定时器已经创建，则初始化已经完成 */
    prvCheckForValidListAndQueue();  /* 检查有效的列表和队列 */

    /* 检查定时器队列是否成功创建 */
    if( xTimerQueue != NULL )
    {
        /* 根据配置选择静态或动态内存分配方式 */
        #if( configSUPPORT_STATIC_ALLOCATION == 1 )
        {
            StaticTask_t *pxTimerTaskTCBBuffer = NULL;  /* 静态TCB缓冲区指针 */
            StackType_t *pxTimerTaskStackBuffer = NULL; /* 静态栈缓冲区指针 */
            uint32_t ulTimerTaskStackSize;              /* 定时器任务栈大小 */

            /* 获取定时器任务的内存配置（TCB和栈缓冲区） */
            vApplicationGetTimerTaskMemory( &pxTimerTaskTCBBuffer, &pxTimerTaskStackBuffer, &ulTimerTaskStackSize );
            
            /* 使用静态分配方式创建定时器任务 */
            xTimerTaskHandle = xTaskCreateStatic( prvTimerTask,              /* 定时器任务函数 */
                                                  "Tmr Svc",                 /* 任务名称 */
                                                  ulTimerTaskStackSize,      /* 任务栈大小 */
                                                  NULL,                      /* 任务参数 */
                                                  ( ( UBaseType_t ) configTIMER_TASK_PRIORITY ) | portPRIVILEGE_BIT,  /* 任务优先级（带特权位） */
                                                  pxTimerTaskStackBuffer,    /* 栈缓冲区 */
                                                  pxTimerTaskTCBBuffer );    /* TCB缓冲区 */

            /* 检查任务是否创建成功 */
            if( xTimerTaskHandle != NULL )
            {
                xReturn = pdPASS;  /* 设置返回值为成功 */
            }
        }
        #else
        {
            /* 使用动态分配方式创建定时器任务 */
            xReturn = xTaskCreate( prvTimerTask,              /* 定时器任务函数 */
                                   "Tmr Svc",                 /* 任务名称 */
                                   configTIMER_TASK_STACK_DEPTH,  /* 任务栈深度 */
                                   NULL,                      /* 任务参数 */
                                   ( ( UBaseType_t ) configTIMER_TASK_PRIORITY ) | portPRIVILEGE_BIT,  /* 任务优先级（带特权位） */
                                   &xTimerTaskHandle );       /* 返回任务句柄 */
        }
        #endif /* configSUPPORT_STATIC_ALLOCATION */
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
    }

    /* 断言检查确保返回值有效 */
    configASSERT( xReturn );
    
    /* 返回任务创建结果 */
    return xReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xTimerCreate
 * 功能描述：动态创建软件定时器，分配内存并初始化定时器属性
 *           此函数创建并初始化一个软件定时器，支持单次和周期性定时模式
 * 输入参数：
 *   - pcTimerName: 定时器名称字符串，用于调试和识别定时器
 *   - xTimerPeriodInTicks: 定时器周期，以时钟节拍为单位
 *   - uxAutoReload: 定时器重载模式，pdTRUE为周期性定时器，pdFALSE为单次定时器
 *   - pvTimerID: 定时器标识符，可用于在回调函数中识别定时器
 *   - pxCallbackFunction: 定时器回调函数指针，定时器到期时调用
 * 输出参数：无
 * 返 回 值：
 *   - TimerHandle_t: 成功创建时返回定时器句柄，失败时返回NULL
 * 其它说明：
 *   - 此函数仅在启用动态分配时编译（configSUPPORT_DYNAMIC_ALLOCATION == 1）
 *   - 定时器内存使用pvPortMalloc动态分配
 *   - 定时器创建后处于休眠状态，需要调用xTimerStart等函数启动
 *   - 支持静态和动态分配标识，便于后续删除时正确处理内存
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

TimerHandle_t xTimerCreate( const char * const pcTimerName,
                            const TickType_t xTimerPeriodInTicks,
                            const UBaseType_t uxAutoReload,
                            void * const pvTimerID,
                            TimerCallbackFunction_t pxCallbackFunction ) /*lint !e971 允许未限定的char类型用于字符串和单个字符 */
{
    Timer_t *pxNewTimer;  /* 指向新定时器结构的指针 */

    /* 为定时器结构分配内存 */
    pxNewTimer = ( Timer_t * ) pvPortMalloc( sizeof( Timer_t ) );

    /* 检查内存是否分配成功 */
    if( pxNewTimer != NULL )
    {
        /* 初始化新定时器的属性 */
        prvInitialiseNewTimer( pcTimerName,          /* 定时器名称 */
                               xTimerPeriodInTicks,  /* 定时器周期 */
                               uxAutoReload,         /* 重载模式 */
                               pvTimerID,            /* 定时器ID */
                               pxCallbackFunction,   /* 回调函数 */
                               pxNewTimer );         /* 定时器结构指针 */

        /* 如果同时支持静态分配，标记定时器的分配方式 */
        #if( configSUPPORT_STATIC_ALLOCATION == 1 )
        {
            /* 定时器可以静态或动态创建，因此标记此定时器是动态创建的，
               以便在后续删除定时器时正确处理内存 */
            pxNewTimer->ucStaticallyAllocated = pdFALSE;
        }
        #endif /* configSUPPORT_STATIC_ALLOCATION */
    }

    /* 返回定时器句柄（可能为NULL如果分配失败） */
    return pxNewTimer;
}

#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xTimerCreateStatic
 * 功能描述：静态创建软件定时器，使用预分配的内存缓冲区初始化定时器属性
 *           此函数创建并初始化一个软件定时器，使用静态内存分配，避免动态内存分配
 * 输入参数：
 *   - pcTimerName: 定时器名称字符串，用于调试和识别定时器
 *   - xTimerPeriodInTicks: 定时器周期，以时钟节拍为单位
 *   - uxAutoReload: 定时器重载模式，pdTRUE为周期性定时器，pdFALSE为单次定时器
 *   - pvTimerID: 定时器标识符，可用于在回调函数中识别定时器
 *   - pxCallbackFunction: 定时器回调函数指针，定时器到期时调用
 *   - pxTimerBuffer: 指向静态定时器内存缓冲区的指针
 * 输出参数：无
 * 返 回 值：
 *   - TimerHandle_t: 成功创建时返回定时器句柄，失败时返回NULL
 * 其它说明：
 *   - 此函数仅在启用静态分配时编译（configSUPPORT_STATIC_ALLOCATION == 1）
 *   - 定时器内存由用户预先分配，不涉及动态内存分配
 *   - 包含断言检查，确保StaticTimer_t和Timer_t结构大小一致
 *   - 支持静态和动态分配标识，便于后续删除时正确处理内存
 *   - 适用于内存受限或需要确定性的嵌入式系统
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
#if( configSUPPORT_STATIC_ALLOCATION == 1 )

TimerHandle_t xTimerCreateStatic( const char * const pcTimerName,
                                  const TickType_t xTimerPeriodInTicks,
                                  const UBaseType_t uxAutoReload,
                                  void * const pvTimerID,
                                  TimerCallbackFunction_t pxCallbackFunction,
                                  StaticTimer_t *pxTimerBuffer ) /*lint !e971 允许未限定的char类型用于字符串和单个字符 */
{
    Timer_t *pxNewTimer;  /* 指向新定时器结构的指针 */

    /* 如果启用了断言检查，验证StaticTimer_t结构的大小是否与Timer_t结构相同 */
    #if( configASSERT_DEFINED == 1 )
    {
        /* 健全性检查，用于声明StaticTimer_t类型变量的结构体大小与实际定时器结构的大小相等 */
        volatile size_t xSize = sizeof( StaticTimer_t );
        configASSERT( xSize == sizeof( Timer_t ) );  /* 断言检查结构大小一致 */
    }
    #endif /* configASSERT_DEFINED */

    /* 必须提供指向StaticTimer_t结构的指针，使用它 */
    configASSERT( pxTimerBuffer );  /* 断言检查缓冲区指针不为NULL */
    pxNewTimer = ( Timer_t * ) pxTimerBuffer; /*lint !e740 不寻常的转换是可以的，因为结构设计为具有相同的对齐方式，并且大小由断言检查 */

    /* 检查指针是否有效 */
    if( pxNewTimer != NULL )
    {
        /* 初始化新定时器的属性 */
        prvInitialiseNewTimer( pcTimerName,          /* 定时器名称 */
                               xTimerPeriodInTicks,  /* 定时器周期 */
                               uxAutoReload,         /* 重载模式 */
                               pvTimerID,            /* 定时器ID */
                               pxCallbackFunction,   /* 回调函数 */
                               pxNewTimer );         /* 定时器结构指针 */

        /* 如果同时支持动态分配，标记定时器的分配方式 */
        #if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
        {
            /* 定时器可以静态或动态创建，因此标记此定时器是静态创建的，
               以便在后续删除定时器时正确处理内存 */
            pxNewTimer->ucStaticallyAllocated = pdTRUE;
        }
        #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
    }

    /* 返回定时器句柄 */
    return pxNewTimer;
}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvInitialiseNewTimer
 * 功能描述：初始化新定时器结构体的各个成员，设置定时器属性和回调函数
 *           此函数是定时器创建的核心初始化函数，负责设置定时器的基本属性和状态
 * 输入参数：
 *   - pcTimerName: 定时器名称字符串，用于调试和识别定时器
 *   - xTimerPeriodInTicks: 定时器周期，以时钟节拍为单位
 *   - uxAutoReload: 定时器重载模式，pdTRUE为周期性定时器，pdFALSE为单次定时器
 *   - pvTimerID: 定时器标识符，可用于在回调函数中识别定时器
 *   - pxCallbackFunction: 定时器回调函数指针，定时器到期时调用
 *   - pxNewTimer: 指向要初始化的定时器结构体的指针
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数是静态函数，仅在FreeRTOS定时器模块内部使用
 *   - 包含参数验证，确保定时器周期大于0
 *   - 初始化定时器列表项，为将定时器插入定时器列表做准备
 *   - 检查定时器服务任务所需的基础设施是否已创建/初始化
 *   - 提供定时器创建跟踪，便于调试和性能分析
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
static void prvInitialiseNewTimer( const char * const pcTimerName,
                                   const TickType_t xTimerPeriodInTicks,
                                   const UBaseType_t uxAutoReload,
                                   void * const pvTimerID,
                                   TimerCallbackFunction_t pxCallbackFunction,
                                   Timer_t *pxNewTimer ) /*lint !e971 允许未限定的char类型用于字符串和单个字符 */
{
    /* 0不是xTimerPeriodInTicks的有效值，使用断言检查周期是否大于0 */
    configASSERT( ( xTimerPeriodInTicks > 0 ) );

    /* 检查定时器结构指针是否有效 */
    if( pxNewTimer != NULL )
    {
        /* 确保定时器服务任务使用的基础设施已创建/初始化 */
        prvCheckForValidListAndQueue();

        /* 使用函数参数初始化定时器结构成员 */
        pxNewTimer->pcTimerName = pcTimerName;                /* 设置定时器名称 */
        pxNewTimer->xTimerPeriodInTicks = xTimerPeriodInTicks; /* 设置定时器周期 */
        pxNewTimer->uxAutoReload = uxAutoReload;              /* 设置重载模式 */
        pxNewTimer->pvTimerID = pvTimerID;                    /* 设置定时器ID */
        pxNewTimer->pxCallbackFunction = pxCallbackFunction;  /* 设置回调函数 */
        
        /* 初始化定时器列表项，为将定时器插入定时器列表做准备 */
        vListInitialiseItem( &( pxNewTimer->xTimerListItem ) );
        
        /* 跟踪定时器创建事件 */
        traceTIMER_CREATE( pxNewTimer );
    }
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xTimerGenericCommand
 * 功能描述：向定时器服务任务发送通用命令，执行特定的定时器操作
 *           此函数是定时器操作的统一接口，支持多种命令类型和调用上下文
 * 输入参数：
 *   - xTimer: 定时器句柄，指定要操作的定时器
 *   - xCommandID: 命令ID，指定要执行的定时器操作类型
 *   - xOptionalValue: 可选值，根据命令不同可能有不同的含义
 *   - pxHigherPriorityTaskWoken: 指向更高优先级任务唤醒标志的指针（用于ISR）
 *   - xTicksToWait: 发送命令到定时器队列的最大等待时间（以时钟节拍为单位）
 * 输出参数：
 *   - pxHigherPriorityTaskWoken: 在ISR调用时，指示是否有更高优先级任务被唤醒
 * 返 回 值：
 *   - BaseType_t: 如果命令成功发送到定时器队列则返回pdPASS，否则返回pdFAIL
 * 其它说明：
 *   - 此函数是定时器操作的统一入口点，支持任务和中断两种调用上下文
 *   - 根据命令ID区分不同的定时器操作（启动、停止、重置、删除、改变周期等）
 *   - 根据调用上下文选择适当的队列发送函数（任务或ISR版本）
 *   - 提供命令发送跟踪，便于调试和性能分析
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
BaseType_t xTimerGenericCommand( TimerHandle_t xTimer, const BaseType_t xCommandID, const TickType_t xOptionalValue, BaseType_t * const pxHigherPriorityTaskWoken, const TickType_t xTicksToWait )
{
    BaseType_t xReturn = pdFAIL;            /* 返回值，初始化为失败状态 */
    DaemonTaskMessage_t xMessage;           /* 定时器服务任务消息结构 */

    /* 断言检查定时器句柄有效性 */
    configASSERT( xTimer );

    /* 向定时器服务任务发送消息，以对特定定时器定义执行特定操作 */
    if( xTimerQueue != NULL )
    {
        /* 向定时器服务任务发送命令以操作xTimer定时器 */
        xMessage.xMessageID = xCommandID;                                   /* 设置消息ID（命令类型） */
        xMessage.u.xTimerParameters.xMessageValue = xOptionalValue;         /* 设置消息值（可选参数） */
        xMessage.u.xTimerParameters.pxTimer = ( Timer_t * ) xTimer;         /* 设置定时器指针 */

        /* 根据命令ID判断调用上下文（任务或中断） */
        if( xCommandID < tmrFIRST_FROM_ISR_COMMAND )
        {
            /* 任务上下文：使用普通队列发送函数 */
            if( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING )
            {
                /* 调度器正在运行，使用指定的等待时间 */
                xReturn = xQueueSendToBack( xTimerQueue, &xMessage, xTicksToWait );
            }
            else
            {
                /* 调度器未运行，不等待立即发送 */
                xReturn = xQueueSendToBack( xTimerQueue, &xMessage, tmrNO_DELAY );
            }
        }
        else
        {
            /* 中断上下文：使用ISR队列发送函数 */
            xReturn = xQueueSendToBackFromISR( xTimerQueue, &xMessage, pxHigherPriorityTaskWoken );
        }

        /* 跟踪定时器命令发送事件 */
        traceTIMER_COMMAND_SEND( xTimer, xCommandID, xOptionalValue, xReturn );
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
    }

    /* 返回命令发送结果 */
    return xReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
函数名称：xTimerGetTimerDaemonTaskHandle
功能描述：获取FreeRTOS定时器守护进程任务的句柄    
输入参数：void - 无输入参数   
输出参数：无    
返回值：TimerHandle_t - 成功返回定时器守护进程任务句柄，失败触发断言    
其它说明：此函数应在调度器启动后调用，否则xTimerTaskHandle为NULL将触发断言
修改日期      版本号          修改人            修改内容
------------------------------------------------------------------------------
2025/09/03     V1.00          Your Name          创建
*******************************************************************************/
TaskHandle_t xTimerGetTimerDaemonTaskHandle( void )
{
	/* 检查调度器是否已启动，未启动时xTimerTaskHandle为NULL会触发断言 */
	configASSERT( ( xTimerTaskHandle != NULL ) );
	/* 返回定时器守护进程任务句柄 */
	return xTimerTaskHandle;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
函数名称：xTimerGetPeriod
功能描述：获取指定软件定时器的定时周期值    
输入参数：xTimer - 要查询的定时器句柄   
输出参数：无    
返回值：TickType_t - 返回定时器的周期值（以系统节拍数表示）    
其它说明：输入参数必须为有效的定时器句柄，否则触发断言
修改日期      版本号          修改人            修改内容
------------------------------------------------------------------------------
2025/09/03     V1.00          Your Name          创建
*******************************************************************************/
TickType_t xTimerGetPeriod( TimerHandle_t xTimer )
{
	/* 将定时器句柄转换为定时器结构体指针 */
	Timer_t *pxTimer = ( Timer_t * ) xTimer;

	/* 断言检查确保定时器句柄有效 */
	configASSERT( xTimer );
	/* 返回定时器结构体中存储的周期值 */
	return pxTimer->xTimerPeriodInTicks;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
函数名称：xTimerGetExpiryTime
功能描述：获取指定定时器的下一次到期时间点    
输入参数：xTimer - 要查询的定时器句柄   
输出参数：无    
返回值：TickType_t - 返回定时器下一次到期时的系统节拍计数值    
其它说明：输入参数必须为有效的定时器句柄，否则触发断言
修改日期      版本号          修改人            修改内容
------------------------------------------------------------------------------
2025/09/03     V1.00          Your Name          创建
*******************************************************************************/
TickType_t xTimerGetExpiryTime( TimerHandle_t xTimer )
{
	/* 将定时器句柄转换为定时器结构体指针 */
	Timer_t * pxTimer = ( Timer_t * ) xTimer;
	TickType_t xReturn;

	/* 断言检查确保定时器句柄有效 */
	configASSERT( xTimer );
	/* 从定时器链表项中获取到期时间值 */
	xReturn = listGET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ) );
	/* 返回获取到的到期时间值 */
	return xReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
函数名称：pcTimerGetName
功能描述：获取指定定时器的名称字符串    
输入参数：xTimer - 要查询的定时器句柄   
输出参数：无    
返回值：const char* - 返回指向定时器名称字符串的指针    
其它说明：输入参数必须为有效的定时器句柄，否则触发断言
修改日期      版本号          修改人            修改内容
------------------------------------------------------------------------------
2025/09/03     V1.00          Your Name          创建
*******************************************************************************/
const char * pcTimerGetName( TimerHandle_t xTimer )
{
	/* 将定时器句柄转换为定时器结构体指针 */
	Timer_t *pxTimer = ( Timer_t * ) xTimer;

	/* 断言检查确保定时器句柄有效 */
	configASSERT( xTimer );
	/* 返回定时器结构体中存储的名称字符串 */
	return pxTimer->pcTimerName;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvProcessExpiredTimer
 * 功能描述：处理已到期定时器的内部核心函数
 *           负责从活动定时器列表中移除到期定时器，处理自动重载定时器的重新插入，
 *           并执行定时器关联的回调函数
 * 输入参数：
 *   - xNextExpireTime: 下一个到期时间点（系统节拍数），表示当前处理的时间基准
 *   - xTimeNow: 当前系统时间（系统节拍数），用于计算定时器重新插入的时间
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数为静态函数，仅在定时器守护任务内部使用，处理定时器到期后的相关操作
 *   - 函数假设调用时当前活动定时器列表不为空，且第一个定时器已到期
 *   - 对于自动重载定时器，会计算下一次到期时间并重新插入到活动定时器列表
 *   - 无论定时器类型如何，最终都会执行其关联的回调函数
 *   - 使用代码覆盖标记标识测试覆盖情况，提高代码可测试性
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
static void prvProcessExpiredTimer( const TickType_t xNextExpireTime, const TickType_t xTimeNow )
{
    /* 存储函数调用结果 */
    BaseType_t xResult;
    /* 从当前活动定时器列表中获取第一个定时器（即最早到期的定时器）并转换为定时器结构体指针 */
    Timer_t * const pxTimer = ( Timer_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxCurrentTimerList );

    /* 从活动定时器列表中移除该定时器。调用前已确保列表不为空 */
    ( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
    /* 记录定时器到期跟踪信息（如果启用了跟踪功能） */
    traceTIMER_EXPIRED( pxTimer );

    /* 检查定时器是否为自动重载类型，如果是则计算下一次到期时间并重新插入活动定时器列表 */
    if( pxTimer->uxAutoReload == ( UBaseType_t ) pdTRUE )
    {
        /* 定时器使用相对于当前时间之外的时间插入列表，因此会根据任务认为的当前时间正确插入到相应列表 */
        if( prvInsertTimerInActiveList( pxTimer, ( xNextExpireTime + pxTimer->xTimerPeriodInTicks ), xTimeNow, xNextExpireTime ) != pdFALSE )
        {
            /* 定时器在添加到活动定时器列表之前就已到期，立即重新加载它 */
            xResult = xTimerGenericCommand( pxTimer, tmrCOMMAND_START_DONT_TRACE, xNextExpireTime, NULL, tmrNO_DELAY );
            /* 断言确保命令执行成功 */
            configASSERT( xResult );
            /* 忽略返回值（避免编译器警告） */
            ( void ) xResult;
        }
        else
        {
            /* 代码覆盖测试标记（正常执行路径） */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    else
    {
        /* 代码覆盖测试标记（单次定时器情况） */
        mtCOVERAGE_TEST_MARKER();
    }

    /* 调用定时器的回调函数，传递定时器句柄作为参数 */
    pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvTimerTask
 * 功能描述：FreeRTOS定时器守护任务的主函数，负责管理所有软件定时器的生命周期
 *           包括定时器到期处理、命令处理和任务阻塞/唤醒等核心功能
 * 输入参数：
 *   - pvParameters: 任务参数指针（当前版本未使用，仅为避免编译器警告）
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数为FreeRTOS定时器系统的核心任务函数，以无限循环方式运行
 *   - 负责处理定时器到期事件和执行定时器相关的各种命令
 *   - 支持应用层启动钩子函数，允许应用在定时器任务启动时执行特定初始化
 *   - 采用高效的任务阻塞机制，在没有定时器到期时自动阻塞以节省CPU资源
 *   - 通过三个核心子函数实现完整的定时器管理功能
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
static void prvTimerTask( void *pvParameters )
{
    /* 存储下一个定时器的到期时间 */
    TickType_t xNextExpireTime;
    /* 存储定时器列表是否为空的标志 */
    BaseType_t xListWasEmpty;

    /* 仅为避免编译器警告，参数在当前实现中未使用 */
    ( void ) pvParameters;

    /* 如果配置了守护任务启动钩子，则执行应用层定义的启动初始化代码 */
    #if( configUSE_DAEMON_TASK_STARTUP_HOOK == 1 )
    {
        /* 声明外部启动钩子函数 */
        extern void vApplicationDaemonTaskStartupHook( void );

        /* 允许应用程序编写者在定时器任务开始执行时执行一些代码
           这对于需要在调度器启动后执行的初始化代码非常有用 */
        vApplicationDaemonTaskStartupHook();
    }
    #endif /* configUSE_DAEMON_TASK_STARTUP_HOOK */

    /* 定时器守护任务的主循环，无限循环处理定时器事件和命令 */
    for( ;; )
    {
        /* 查询定时器列表，检查是否包含任何定时器
           如果存在定时器，则获取下一个定时器的到期时间 */
        xNextExpireTime = prvGetNextExpireTime( &xListWasEmpty );

        /* 如果有定时器已到期，则处理它
           否则，阻塞此任务直到有定时器到期或接收到命令 */
        prvProcessTimerOrBlockTask( xNextExpireTime, xListWasEmpty );

        /* 处理接收到的所有命令，清空命令队列 */
        prvProcessReceivedCommands();
    }
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvProcessTimerOrBlockTask
 * 功能描述：处理定时器到期或阻塞定时器任务的核心函数
 *           负责判断定时器是否到期并处理，或根据需要阻塞任务等待下一个事件
 * 输入参数：
 *   - xNextExpireTime: 下一个定时器的到期时间（系统节拍数）
 *   - xListWasEmpty: 指示定时器列表是否为空的状态标志
 *                      pdFALSE表示列表非空，pdTRUE表示列表为空
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数是定时器守护任务的核心处理逻辑，负责决定是处理到期定时器还是阻塞任务
 *   - 使用临界区保护时间采样和列表切换判断，确保操作的原子性
 *   - 处理系统节拍计数溢出导致的定时器列表切换情况
 *   - 采用高效的任务阻塞机制，精确等待下一个定时器到期或命令到达
 *   - 支持任务恢复后的智能调度决策，优化系统响应性能
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
static void prvProcessTimerOrBlockTask( const TickType_t xNextExpireTime, BaseType_t xListWasEmpty )
{
    /* 存储当前系统时间 */
    TickType_t xTimeNow;
    /* 存储定时器列表是否发生切换的标志 */
    BaseType_t xTimerListsWereSwitched;

    /* 挂起所有任务，进入临界区，确保时间采样和状态判断的原子性 */
    vTaskSuspendAll();
    {
        /* 获取当前时间以评估定时器是否已到期
           如果获取时间导致列表切换，则不处理此定时器，因为在列表切换时仍留在列表中的
           任何定时器都已在prvSampleTimeNow()函数中处理过了 */
        xTimeNow = prvSampleTimeNow( &xTimerListsWereSwitched );
        /* 检查定时器列表是否发生了切换（通常由于节拍计数溢出引起） */
        if( xTimerListsWereSwitched == pdFALSE )
        {
            /* 节拍计数未溢出，检查定时器是否已到期 */
            if( ( xListWasEmpty == pdFALSE ) && ( xNextExpireTime <= xTimeNow ) )
            {
                /* 恢复任务调度 */
                ( void ) xTaskResumeAll();
                /* 处理已到期的定时器 */
                prvProcessExpiredTimer( xNextExpireTime, xTimeNow );
            }
            else
            {
                /* 节拍计数未溢出，且下一个到期时间尚未到达
                   此任务应阻塞以等待下一个到期时间或接收到命令 - 以先到者为准
                   除非xNextExpireTime > xTimeNow，否则以下行无法到达，除非当前定时器列表为空 */
                if( xListWasEmpty != pdFALSE )
                {
                    /* 当前定时器列表为空 - 检查溢出列表是否也为空？ */
                    xListWasEmpty = listLIST_IS_EMPTY( pxOverflowTimerList );
                }

                /* 限制性地等待队列消息，最多等待到下一个到期时间 */
                vQueueWaitForMessageRestricted( xTimerQueue, ( xNextExpireTime - xTimeNow ), xListWasEmpty );

                /* 恢复任务调度，并检查是否需要主动让出CPU */
                if( xTaskResumeAll() == pdFALSE )
                {
                    /* 让出CPU以等待命令到达或阻塞时间到期
                       如果在临界区退出和此让出之间已有命令到达，则让出不会导致任务阻塞 */
                    portYIELD_WITHIN_API();
                }
                else
                {
                    /* 代码覆盖测试标记（正常执行路径） */
                    mtCOVERAGE_TEST_MARKER();
                }
            }
        }
        else
        {
            /* 定时器列表已切换，只需恢复任务调度 */
            ( void ) xTaskResumeAll();
        }
    }
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvGetNextExpireTime
 * 功能描述：获取下一个定时器的到期时间并检查定时器列表是否为空
 *           此函数是定时器守护任务的核心辅助函数，用于确定下一个需要处理的定时器事件
 * 输入参数：
 *   - pxListWasEmpty: 指向列表空状态标志的指针，用于输出当前定时器列表是否为空的状态
 * 输出参数：
 *   - pxListWasEmpty: 返回当前定时器列表是否为空的状态
 *                     pdFALSE表示列表非空，pdTRUE表示列表为空
 * 返回值：
 *   - TickType_t: 返回下一个定时器的到期时间（系统节拍数）
 *                 如果列表为空，返回0表示在节拍计数溢出时解除任务阻塞
 * 其它说明：
 *   - 此函数是定时器管理的核心查询函数，为定时器守护任务提供决策依据
 *   - 定时器按到期时间顺序排列，列表头部引用最先到期的定时器
 *   - 当没有活动定时器时，设置下一个到期时间为0，使任务在节拍计数溢出时解除阻塞
 *   - 通过指针参数返回列表状态，避免多次查询列表状态造成的性能开销
 *   - 确保在节拍计数翻转时任务能够正确解除阻塞并重新评估下一个到期时间
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
static TickType_t prvGetNextExpireTime( BaseType_t * const pxListWasEmpty )
{
    /* 存储下一个定时器的到期时间 */
    TickType_t xNextExpireTime;

    /* 定时器按到期时间顺序排列，列表头部引用最先到期的定时器
       获取具有最近到期时间的定时器的到期时间
       如果没有活动定时器，则将下一个到期时间设置为0，这将导致
       此任务在节拍计数溢出时解除阻塞，此时定时器列表将切换
       并且可以重新评估下一个到期时间 */
    /* 检查当前定时器列表是否为空，结果通过指针参数返回 */
    *pxListWasEmpty = listLIST_IS_EMPTY( pxCurrentTimerList );
    /* 如果定时器列表不为空 */
    if( *pxListWasEmpty == pdFALSE )
    {
        /* 获取列表头部条目的值（即下一个定时器的到期时间） */
        xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxCurrentTimerList );
    }
    else
    {
        /* 确保任务在节拍计数翻转时解除阻塞 */
        xNextExpireTime = ( TickType_t ) 0U;
    }

    /* 返回下一个定时器的到期时间 */
    return xNextExpireTime;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvSampleTimeNow
 * 功能描述：采样当前系统时间并检测是否发生定时器列表切换
 *           此函数是定时器系统的核心时间管理函数，负责处理系统节拍计数溢出的特殊情况
 * 输入参数：
 *   - pxTimerListsWereSwitched: 指向定时器列表切换标志的指针，用于输出列表是否发生切换
 * 输出参数：
 *   - pxTimerListsWereSwitched: 返回定时器列表是否发生切换的状态
 *                               pdTRUE表示列表已切换，pdFALSE表示列表未切换
 * 返回值：
 *   - TickType_t: 返回当前系统时间（系统节拍数）
 * 其它说明：
 *   - 此函数是定时器时间管理的核心，处理系统节拍计数溢出的关键逻辑
 *   - 使用静态变量保存上一次采样时间，用于检测节拍计数溢出
 *   - 当检测到节拍计数溢出时，自动切换定时器列表
 *   - 确保定时器系统能够正确处理32位节拍计数器的溢出情况
 *   - 使用特权数据修饰符，确保变量仅由一个任务访问（尽管是静态变量）
 *   - 提供准确的时间采样和列表切换检测，为定时器处理提供可靠的时间基准
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
static TickType_t prvSampleTimeNow( BaseType_t * const pxTimerListsWereSwitched )
{
    /* 存储当前系统时间 */
    TickType_t xTimeNow;
    /* 静态变量存储上一次采样时间，初始化为0，仅由一个任务访问 */
    PRIVILEGED_DATA static TickType_t xLastTime = ( TickType_t ) 0U; /*lint !e956 Variable is only accessible to one task. */

    /* 获取当前系统节拍计数 */
    xTimeNow = xTaskGetTickCount();

    /* 检查当前时间是否小于上一次时间（检测节拍计数溢出） */
    if( xTimeNow < xLastTime )
    {
        /* 节拍计数已溢出，切换定时器列表 */
        prvSwitchTimerLists();
        /* 设置列表切换标志为真 */
        *pxTimerListsWereSwitched = pdTRUE;
    }
    else
    {
        /* 节拍计数未溢出，设置列表切换标志为假 */
        *pxTimerListsWereSwitched = pdFALSE;
    }

    /* 更新上一次采样时间为当前时间 */
    xLastTime = xTimeNow;

    /* 返回当前系统时间 */
    return xTimeNow;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvInsertTimerInActiveList
 * 功能描述：将定时器插入活动列表的核心函数，负责根据定时器的到期时间将其插入到合适的列表中
 *           并判断定时器是否需要立即处理，处理节拍计数溢出等特殊情况
 * 输入参数：
 *   - pxTimer: 指向要插入的定时器结构的指针
 *   - xNextExpiryTime: 定时器的下一次到期时间（系统节拍数）
 *   - xTimeNow: 当前系统时间（系统节拍数）
 *   - xCommandTime: 命令发出时的时间（系统节拍数）
 * 输出参数：无
 * 返回值：
 *   - BaseType_t: 返回是否需要立即处理定时器的标志
 *                 pdTRUE表示需要立即处理定时器，pdFALSE表示定时器已插入列表
 * 其它说明：
 *   - 此函数是定时器列表管理的核心，负责根据时间条件将定时器插入正确的列表
 *   - 处理命令发出到处理之间可能发生的时间流逝和节拍计数溢出情况
 *   - 根据定时器到期时间与当前时间的比较，决定插入当前列表还是溢出列表
 *   - 在特定条件下，即使定时器尚未到期也可能需要立即处理
 *   - 使用链表操作维护定时器的有序性，确保最早到期的定时器在列表头部
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
static BaseType_t prvInsertTimerInActiveList( Timer_t * const pxTimer, const TickType_t xNextExpiryTime, const TickType_t xTimeNow, const TickType_t xCommandTime )
{
    /* 初始化是否需要立即处理定时器的标志，默认为不需要 */
    BaseType_t xProcessTimerNow = pdFALSE;

    /* 设置定时器链表项的值为下一次到期时间 */
    listSET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ), xNextExpiryTime );
    /* 设置定时器链表项的所有者为当前定时器 */
    listSET_LIST_ITEM_OWNER( &( pxTimer->xTimerListItem ), pxTimer );

    /* 检查定时器的下一次到期时间是否已经小于或等于当前时间 */
    if( xNextExpiryTime <= xTimeNow )
    {
        /* 检查从命令发出到命令处理之间的时间是否已经超过定时器的周期
           这种情况可能发生在命令处理延迟较大的情况下 */
        if( ( ( TickType_t ) ( xTimeNow - xCommandTime ) ) >= pxTimer->xTimerPeriodInTicks ) /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
        {
            /* 命令发出到命令处理之间的时间实际上已经超过了定时器的周期
               需要立即处理定时器 */
            xProcessTimerNow = pdTRUE;
        }
        else
        {
            /* 将定时器插入溢出定时器列表 */
            vListInsert( pxOverflowTimerList, &( pxTimer->xTimerListItem ) );
        }
    }
    else
    {
        /* 检查是否发生了节拍计数溢出，且定时器的到期时间尚未到达 */
        if( ( xTimeNow < xCommandTime ) && ( xNextExpiryTime >= xCommandTime ) )
        {
            /* 如果自命令发出以来，节拍计数已经溢出但到期时间尚未到达
               那么定时器实际上已经超过了它的到期时间，应该立即处理 */
            xProcessTimerNow = pdTRUE;
        }
        else
        {
            /* 将定时器插入当前定时器列表 */
            vListInsert( pxCurrentTimerList, &( pxTimer->xTimerListItem ) );
        }
    }

    /* 返回是否需要立即处理定时器的标志 */
    return xProcessTimerNow;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvProcessReceivedCommands
 * 功能描述：处理从定时器队列接收到的所有命令的核心函数
 *           负责解析和执行定时器命令，包括启动、停止、重置、删除定时器以及改变周期等操作
 *           同时支持 pend 函数调用功能，提供灵活的命令处理机制
 * 输入参数：void - 无直接输入参数，通过全局队列 xTimerQueue 接收命令消息
 * 输出参数：无
 * 返回值：无
 * 其它说明：
 *   - 此函数是定时器守护任务的核心命令处理循环，负责处理所有异步定时器命令
 *   - 支持多种定时器操作命令，包括从任务和中断上下文发出的命令
 *   - 处理 pend 函数调用功能，允许在定时器任务上下文中执行用户自定义函数
 *   - 使用队列机制确保命令处理的线程安全性，避免竞态条件
 *   - 包含详细的错误检查和断言，确保命令处理的可靠性
 *   - 处理定时器列表管理，确保定时器在正确的时间被激活或移除
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
static void prvProcessReceivedCommands( void )
{
    /* 定义消息结构用于接收队列中的命令 */
    DaemonTaskMessage_t xMessage;
    /* 指向定时器结构的指针 */
    Timer_t *pxTimer;
    /* 存储定时器列表是否切换的标志 */
    BaseType_t xTimerListsWereSwitched, xResult;
    /* 存储当前时间 */
    TickType_t xTimeNow;

    /* 循环从定时器队列接收消息，不阻塞立即返回 */
    while( xQueueReceive( xTimerQueue, &xMessage, tmrNO_DELAY ) != pdFAIL ) /*lint !e603 xMessage does not have to be initialised as it is passed out, not in, and it is not used unless xQueueReceive() returns pdTRUE. */
    {
        /* 检查是否启用了 pend 函数调用功能 */
        #if ( INCLUDE_xTimerPendFunctionCall == 1 )
        {
            /* 负数的命令ID表示是 pend 函数调用而不是定时器命令 */
            if( xMessage.xMessageID < ( BaseType_t ) 0 )
            {
                /* 获取回调参数结构指针 */
                const CallbackParameters_t * const pxCallback = &( xMessage.u.xCallbackParameters );

                /* 断言检查回调参数有效性 */
                configASSERT( pxCallback );

                /* 调用 pend 函数 */
                pxCallback->pxCallbackFunction( pxCallback->pvParameter1, pxCallback->ulParameter2 );
            }
            else
            {
                /* 代码覆盖测试标记（非 pend 函数调用情况） */
                mtCOVERAGE_TEST_MARKER();
            }
        }
        #endif /* INCLUDE_xTimerPendFunctionCall */

        /* 正数的命令ID表示是定时器命令而不是 pend 函数调用 */
        if( xMessage.xMessageID >= ( BaseType_t ) 0 )
        {
            /* 从消息中获取定时器指针 */
            pxTimer = xMessage.u.xTimerParameters.pxTimer;

            /* 检查定时器是否在某个列表中 */
            if( listIS_CONTAINED_WITHIN( NULL, &( pxTimer->xTimerListItem ) ) == pdFALSE )
            {
                /* 定时器在列表中，先将其移除 */
                ( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
            }
            else
            {
                /* 代码覆盖测试标记（定时器不在列表中的情况） */
                mtCOVERAGE_TEST_MARKER();
            }

            /* 记录定时器命令接收跟踪信息 */
            traceTIMER_COMMAND_RECEIVED( pxTimer, xMessage.xMessageID, xMessage.u.xTimerParameters.xMessageValue );

            /* 获取当前时间，必须在从队列接收消息后调用，以确保时间准确性
               避免高优先级任务在设置 xTimeNow 值后抢占定时器守护任务
               并向消息队列添加具有更早时间的消息 */
            xTimeNow = prvSampleTimeNow( &xTimerListsWereSwitched );

            /* 根据命令ID执行相应的操作 */
            switch( xMessage.xMessageID )
            {
                case tmrCOMMAND_START :
                case tmrCOMMAND_START_FROM_ISR :
                case tmrCOMMAND_RESET :
                case tmrCOMMAND_RESET_FROM_ISR :
                case tmrCOMMAND_START_DONT_TRACE :
                    /* 启动或重启定时器 */
                    if( prvInsertTimerInActiveList( pxTimer,  xMessage.u.xTimerParameters.xMessageValue + pxTimer->xTimerPeriodInTicks, xTimeNow, xMessage.u.xTimerParameters.xMessageValue ) != pdFALSE )
                    {
                        /* 定时器在添加到活动定时器列表之前就已到期，立即处理它 */
                        pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );
                        /* 记录定时器到期跟踪信息 */
                        traceTIMER_EXPIRED( pxTimer );

                        /* 如果是自动重载定时器，重新启动它 */
                        if( pxTimer->uxAutoReload == ( UBaseType_t ) pdTRUE )
                        {
                            xResult = xTimerGenericCommand( pxTimer, tmrCOMMAND_START_DONT_TRACE, xMessage.u.xTimerParameters.xMessageValue + pxTimer->xTimerPeriodInTicks, NULL, tmrNO_DELAY );
                            /* 断言确保命令执行成功 */
                            configASSERT( xResult );
                            ( void ) xResult;
                        }
                        else
                        {
                            /* 代码覆盖测试标记（单次定时器情况） */
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    else
                    {
                        /* 代码覆盖测试标记（定时器成功插入列表的情况） */
                        mtCOVERAGE_TEST_MARKER();
                    }
                    break;

                case tmrCOMMAND_STOP :
                case tmrCOMMAND_STOP_FROM_ISR :
                    /* 定时器已经从活动列表中移除，这里不需要做任何操作 */
                    break;

                case tmrCOMMAND_CHANGE_PERIOD :
                case tmrCOMMAND_CHANGE_PERIOD_FROM_ISR :
                    /* 改变定时器的周期 */
                    pxTimer->xTimerPeriodInTicks = xMessage.u.xTimerParameters.xMessageValue;
                    /* 断言确保新的周期值大于0 */
                    configASSERT( ( pxTimer->xTimerPeriodInTicks > 0 ) );

                    /* 新周期没有真正的参考点，可以比旧周期长或短
                       因此将命令时间设置为当前时间，并且由于周期不能为零
                       下一次到期时间只能在未来，这意味着（与上面的 xTimerStart() 情况不同）
                       这里不需要处理失败情况 */
                    ( void ) prvInsertTimerInActiveList( pxTimer, ( xTimeNow + pxTimer->xTimerPeriodInTicks ), xTimeNow, xTimeNow );
                    break;

                case tmrCOMMAND_DELETE :
                    /* 定时器已经从活动列表中移除，只需释放内存（如果是动态分配的） */
                    #if( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 0 ) )
                    {
                        /* 定时器只能是动态分配的 - 释放它 */
                        vPortFree( pxTimer );
                    }
                    #elif( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
                    {
                        /* 定时器可能是静态或动态分配的，因此在尝试释放内存前检查 */
                        if( pxTimer->ucStaticallyAllocated == ( uint8_t ) pdFALSE )
                        {
                            vPortFree( pxTimer );
                        }
                        else
                        {
                            /* 代码覆盖测试标记（静态分配定时器情况） */
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
                    break;

                default :
                    /* 不应该到达这里，处理未知命令 */
                    break;
            }
        }
    }
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvSwitchTimerLists
 * 功能描述：切换定时器列表的内部函数，处理系统节拍计数溢出时的定时器管理
 *           当系统节拍计数溢出时，必须切换定时器列表，并处理所有已到期的定时器
 * 输入参数：void - 无输入参数
 * 输出参数：无
 * 返回值：无
 * 其它说明：
 *   - 此函数是处理系统节拍计数溢出的核心函数，确保定时器系统在节拍溢出后仍能正确工作
 *   - 在节拍计数溢出时，所有当前定时器列表中的定时器都必须被处理，因为它们都已到期
 *   - 处理自动重载定时器的特殊逻辑，确保它们在节拍溢出后能正确重新启动
 *   - 最后交换当前定时器列表和溢出定时器列表的指针，完成列表切换
 *   - 使用跟踪功能记录定时器到期事件，便于调试和性能分析
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
static void prvSwitchTimerLists( void )
{
    /* 存储下一个到期时间和重载时间 */
    TickType_t xNextExpireTime, xReloadTime;
    /* 临时指针用于列表交换 */
    List_t *pxTemp;
    /* 指向定时器的指针 */
    Timer_t *pxTimer;
    /* 存储函数调用结果 */
    BaseType_t xResult;

    /* 节拍计数已溢出，必须切换定时器列表
       如果当前定时器列表中仍有任何定时器被引用，则它们必须已到期
       应该在列表切换之前处理它们 */
    /* 循环处理当前定时器列表中的所有定时器，直到列表为空 */
    while( listLIST_IS_EMPTY( pxCurrentTimerList ) == pdFALSE )
    {
        /* 获取列表头部条目的值（即下一个定时器的到期时间） */
        xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxCurrentTimerList );

        /* 从列表中移除定时器 */
        pxTimer = ( Timer_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxCurrentTimerList );
        ( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
        /* 记录定时器到期跟踪信息 */
        traceTIMER_EXPIRED( pxTimer );

        /* 执行它的回调函数，然后如果是自动重载定时器则发送命令重新启动它
           不能在这里重新启动，因为列表尚未切换 */
        pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );

        /* 检查定时器是否为自动重载类型 */
        if( pxTimer->uxAutoReload == ( UBaseType_t ) pdTRUE )
        {
            /* 计算重载值，如果重载值导致定时器进入相同的定时器列表
               那么它已经到期，定时器应该重新插入当前列表，以便在此循环中再次处理
               否则应该发送命令重新启动定时器，确保它只在列表交换后插入到列表中 */
            xReloadTime = ( xNextExpireTime + pxTimer->xTimerPeriodInTicks );
            /* 检查重载时间是否大于下一个到期时间（避免溢出情况） */
            if( xReloadTime > xNextExpireTime )
            {
                /* 设置定时器链表项的值和所有者 */
                listSET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ), xReloadTime );
                listSET_LIST_ITEM_OWNER( &( pxTimer->xTimerListItem ), pxTimer );
                /* 将定时器重新插入当前列表 */
                vListInsert( pxCurrentTimerList, &( pxTimer->xTimerListItem ) );
            }
            else
            {
                /* 发送命令重新启动定时器 */
                xResult = xTimerGenericCommand( pxTimer, tmrCOMMAND_START_DONT_TRACE, xNextExpireTime, NULL, tmrNO_DELAY );
                /* 断言确保命令执行成功 */
                configASSERT( xResult );
                ( void ) xResult;
            }
        }
        else
        {
            /* 代码覆盖测试标记（单次定时器情况） */
            mtCOVERAGE_TEST_MARKER();
        }
    }

    /* 交换当前定时器列表和溢出定时器列表的指针 */
    pxTemp = pxCurrentTimerList;
    pxCurrentTimerList = pxOverflowTimerList;
    pxOverflowTimerList = pxTemp;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvCheckForValidListAndQueue
 * 功能描述：检查并确保定时器列表和队列已正确初始化的内部函数
 *           如果定时器队列未初始化，则初始化定时器列表和队列，确保定时器系统可用
 * 输入参数：void - 无输入参数
 * 输出参数：无
 * 返回值：无
 * 其它说明：
 *   - 此函数是定时器系统的初始化保障函数，确保定时器列表和队列在使用前已正确初始化
 *   - 使用临界区保护初始化过程，确保多任务环境下的线程安全
 *   - 支持静态和动态两种内存分配方式，根据配置选择适当的初始化方式
 *   - 初始化两个定时器列表（当前列表和溢出列表）并设置初始指针
 *   - 创建定时器命令队列，用于与定时器服务任务通信
 *   - 可选地将队列添加到注册表，便于调试和监控
 *   - 使用代码覆盖标记标识测试覆盖情况，提高代码可测试性
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
static void prvCheckForValidListAndQueue( void )
{
    /* 检查活动定时器引用的列表和用于与定时器服务通信的队列是否已初始化 */
    /* 进入临界区，确保初始化过程的原子性 */
    taskENTER_CRITICAL();
    {
        /* 检查定时器队列是否为空（未初始化） */
        if( xTimerQueue == NULL )
        {
            /* 初始化两个定时器列表 */
            vListInitialise( &xActiveTimerList1 );
            vListInitialise( &xActiveTimerList2 );
            /* 设置当前定时器列表和溢出定时器列表的初始指针 */
            pxCurrentTimerList = &xActiveTimerList1;
            pxOverflowTimerList = &xActiveTimerList2;

            /* 检查是否支持静态内存分配 */
            #if( configSUPPORT_STATIC_ALLOCATION == 1 )
            {
                /* 如果configSUPPORT_DYNAMIC_ALLOCATION为0，则静态分配定时器队列 */
                /* 定义静态队列结构 */
                static StaticQueue_t xStaticTimerQueue;
                /* 定义静态队列存储空间 */
                static uint8_t ucStaticTimerQueueStorage[ configTIMER_QUEUE_LENGTH * sizeof( DaemonTaskMessage_t ) ];

                /* 创建静态队列 */
                xTimerQueue = xQueueCreateStatic( ( UBaseType_t ) configTIMER_QUEUE_LENGTH, sizeof( DaemonTaskMessage_t ), &( ucStaticTimerQueueStorage[ 0 ] ), &xStaticTimerQueue );
            }
            #else
            {
                /* 动态创建队列 */
                xTimerQueue = xQueueCreate( ( UBaseType_t ) configTIMER_QUEUE_LENGTH, sizeof( DaemonTaskMessage_t ) );
            }
            #endif

            /* 检查是否启用了队列注册表功能 */
            #if ( configQUEUE_REGISTRY_SIZE > 0 )
            {
                /* 如果队列创建成功，将其添加到队列注册表 */
                if( xTimerQueue != NULL )
                {
                    vQueueAddToRegistry( xTimerQueue, "TmrQ" );
                }
                else
                {
                    /* 代码覆盖测试标记（队列创建失败情况） */
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            #endif /* configQUEUE_REGISTRY_SIZE */
        }
        else
        {
            /* 代码覆盖测试标记（队列已初始化情况） */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    /* 退出临界区 */
    taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xTimerIsTimerActive
 * 功能描述：检查指定定时器是否处于活动状态（是否在活动定时器列表中）
 *           此函数用于查询定时器的激活状态，判断定时器是否正在运行或等待到期
 * 输入参数：
 *   - xTimer: 要检查的定时器句柄，指向需要查询状态的定时器对象
 * 输出参数：无
 * 返回值：
 *   - BaseType_t: 返回定时器的活动状态
 *                 pdTRUE表示定时器处于活动状态（在活动列表中）
 *                 pdFALSE表示定时器处于非活动状态（不在活动列表中）
 * 其它说明：
 *   - 此函数是定时器状态查询接口，提供了一种安全的方式检查定时器是否处于活动状态
 *   - 使用临界区保护状态检查过程，确保在多任务环境下的线程安全性
 *   - 通过检查定时器的链表项是否包含在某个列表中来判断定时器状态
 *   - 函数内部包含参数有效性断言，确保传入的定时器句柄有效
 *   - 返回值的逻辑需要反转，因为listIS_CONTAINED_WITHIN在不在列表中时返回pdTRUE
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
BaseType_t xTimerIsTimerActive( TimerHandle_t xTimer )
{
    /* 存储定时器是否在活动列表中的状态 */
    BaseType_t xTimerIsInActiveList;
    /* 将定时器句柄转换为定时器结构体指针 */
    Timer_t *pxTimer = ( Timer_t * ) xTimer;

    /* 断言检查确保定时器句柄有效 */
    configASSERT( xTimer );

    /* 检查定时器是否在活动定时器列表中？ */
    /* 进入临界区，确保状态检查的原子性 */
    taskENTER_CRITICAL();
    {
        /* 检查定时器链表项是否不在NULL列表中，这实际上是一次性检查它是否
           被当前或溢出定时器列表引用，但逻辑必须反转，因此有'!'符号 */
        xTimerIsInActiveList = ( BaseType_t ) !( listIS_CONTAINED_WITHIN( NULL, &( pxTimer->xTimerListItem ) ) );
    }
    /* 退出临界区 */
    taskEXIT_CRITICAL();

    /* 返回定时器是否在活动列表中的状态 */
    return xTimerIsInActiveList;
} /*lint !e818 Can't be pointer to const due to the typedef. */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：pvTimerGetTimerID
 * 功能描述：获取定时器的ID值，该ID是用户与定时器关联的自定义标识符
 *           此函数用于检索与定时器关联的用户定义ID，可用于存储上下文信息
 * 输入参数：
 *   - xTimer: 定时器句柄，指向需要获取ID的定时器对象
 * 输出参数：无
 * 返回值：
 *   - void*: 返回与定时器关联的用户定义ID值
 *            如果未设置ID，则返回NULL或用户设置的特定值
 * 其它说明：
 *   - 此函数提供了获取定时器用户ID的安全方式，使用临界区保护确保线程安全
 *   - ID值是用户自定义的任意指针类型数据，可以用于存储上下文信息或标识定时器
 *   - 函数内部包含参数有效性断言，确保传入的定时器句柄有效
 *   - 使用临界区保护ID读取操作，防止在多任务环境下出现数据竞争
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
void *pvTimerGetTimerID( const TimerHandle_t xTimer )
{
    /* 将定时器句柄转换为定时器结构体指针 */
    Timer_t * const pxTimer = ( Timer_t * ) xTimer;
    /* 存储返回的ID值 */
    void *pvReturn;

    /* 断言检查确保定时器句柄有效 */
    configASSERT( xTimer );

    /* 进入临界区，保护ID读取操作的原子性 */
    taskENTER_CRITICAL();
    {
        /* 从定时器结构中获取用户定义的ID值 */
        pvReturn = pxTimer->pvTimerID;
    }
    /* 退出临界区 */
    taskEXIT_CRITICAL();

    /* 返回获取到的ID值 */
    return pvReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vTimerSetTimerID
 * 功能描述：设置定时器的ID值，该ID是用户与定时器关联的自定义标识符
 *           此函数用于设置或更新与定时器关联的用户定义ID，可用于存储上下文信息
 * 输入参数：
 *   - xTimer: 定时器句柄，指向需要设置ID的定时器对象
 *   - pvNewID: 要设置的新ID值，可以是任意指针类型的数据
 * 输出参数：无
 * 返回值：无
 * 其它说明：
 *   - 此函数提供了设置定时器用户ID的安全方式，使用临界区保护确保线程安全
 *   - ID值是用户自定义的任意指针类型数据，可以用于存储上下文信息或标识定时器
 *   - 函数内部包含参数有效性断言，确保传入的定时器句柄有效
 *   - 使用临界区保护ID设置操作，防止在多任务环境下出现数据竞争
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
void vTimerSetTimerID( TimerHandle_t xTimer, void *pvNewID )
{
    /* 将定时器句柄转换为定时器结构体指针 */
    Timer_t * const pxTimer = ( Timer_t * ) xTimer;

    /* 断言检查确保定时器句柄有效 */
    configASSERT( xTimer );

    /* 进入临界区，保护ID设置操作的原子性 */
    taskENTER_CRITICAL();
    {
        /* 将新ID值设置到定时器结构中 */
        pxTimer->pvTimerID = pvNewID;
    }
    /* 退出临界区 */
    taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

#if( INCLUDE_xTimerPendFunctionCall == 1 )

/*******************************************************************************
 * 函数名称：xTimerPendFunctionCallFromISR
 * 功能描述：从中断服务程序(ISR)中挂起一个函数调用请求到定时器守护任务
 *           此函数允许在ISR上下文请求一个函数调用，该函数将在定时器任务上下文中执行
 * 输入参数：
 *   - xFunctionToPend: 要挂起的函数指针，指向需要在定时器任务中执行的函数
 *   - pvParameter1: 传递给挂起函数的第一个参数，可以是任意类型的指针
 *   - ulParameter2: 传递给挂起函数的第二个参数，32位无符号整数值
 *   - pxHigherPriorityTaskWoken: 指向更高优先级任务唤醒标志的指针
 * 输出参数：
 *   - pxHigherPriorityTaskWoken: 如果挂起操作导致更高优先级任务就绪，则设置为pdTRUE
 * 返回值：
 *   - BaseType_t: 返回函数挂起操作的结果
 *                 pdPASS表示成功将函数调用请求发送到定时器队列
 *                 pdFAIL表示发送失败（通常因为队列已满）
 * 其它说明：
 *   - 此函数专为中断上下文设计，使用FromISR版本的消息发送函数
 *   - 挂起的函数将在定时器守护任务上下文中执行，而不是在ISR上下文中
 *   - 提供跟踪功能，便于调试和性能分析
 *   - 函数执行是异步的，请求发送后立即返回，实际函数执行时间取决于定时器任务调度
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
BaseType_t xTimerPendFunctionCallFromISR( PendedFunction_t xFunctionToPend, void *pvParameter1, uint32_t ulParameter2, BaseType_t *pxHigherPriorityTaskWoken )
{
    /* 定义消息结构用于存储函数调用参数 */
    DaemonTaskMessage_t xMessage;
    /* 存储函数返回结果 */
    BaseType_t xReturn;

    /* 使用函数参数完成消息结构并将其发送到守护任务 */
    xMessage.xMessageID = tmrCOMMAND_EXECUTE_CALLBACK_FROM_ISR;
    xMessage.u.xCallbackParameters.pxCallbackFunction = xFunctionToPend;
    xMessage.u.xCallbackParameters.pvParameter1 = pvParameter1;
    xMessage.u.xCallbackParameters.ulParameter2 = ulParameter2;

    /* 从中断服务程序发送消息到定时器队列 */
    xReturn = xQueueSendFromISR( xTimerQueue, &xMessage, pxHigherPriorityTaskWoken );

    /* 记录挂起函数调用的跟踪信息 */
    tracePEND_FUNC_CALL_FROM_ISR( xFunctionToPend, pvParameter1, ulParameter2, xReturn );

    /* 返回发送操作的结果 */
    return xReturn;
}

#endif /* INCLUDE_xTimerPendFunctionCall */
/*-----------------------------------------------------------*/

#if( INCLUDE_xTimerPendFunctionCall == 1 )

/*******************************************************************************
 * 函数名称：xTimerPendFunctionCall
 * 功能描述：从任务上下文中挂起一个函数调用请求到定时器守护任务
 *           此函数允许在任务上下文请求一个函数调用，该函数将在定时器任务上下文中执行
 * 输入参数：
 *   - xFunctionToPend: 要挂起的函数指针，指向需要在定时器任务中执行的函数
 *   - pvParameter1: 传递给挂起函数的第一个参数，可以是任意类型的指针
 *   - ulParameter2: 传递给挂起函数的第二个参数，32位无符号整数值
 *   - xTicksToWait: 发送请求到定时器队列的最大等待时间（以系统节拍为单位）
 * 输出参数：无
 * 返回值：
 *   - BaseType_t: 返回函数挂起操作的结果
 *                 pdPASS表示成功将函数调用请求发送到定时器队列
 *                 pdFAIL表示发送失败（队列已满且在等待时间内没有空间可用）
 * 其它说明：
 *   - 此函数只能在定时器创建后或调度器启动后调用，因为在此之前定时器队列不存在
 *   - 挂起的函数将在定时器守护任务上下文中执行，而不是在调用任务上下文中
 *   - 提供跟踪功能，便于调试和性能分析
 *   - 函数执行是异步的，请求发送后立即返回，实际函数执行时间取决于定时器任务调度
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          创建
 *******************************************************************************/
BaseType_t xTimerPendFunctionCall( PendedFunction_t xFunctionToPend, void *pvParameter1, uint32_t ulParameter2, TickType_t xTicksToWait )
{
    /* 定义消息结构用于存储函数调用参数 */
    DaemonTaskMessage_t xMessage;
    /* 存储函数返回结果 */
    BaseType_t xReturn;

    /* 此函数只能在定时器创建后或调度器启动后调用，因为在此之前定时器队列不存在 */
    configASSERT( xTimerQueue );

    /* 使用函数参数完成消息结构并将其发送到守护任务 */
    xMessage.xMessageID = tmrCOMMAND_EXECUTE_CALLBACK;
    xMessage.u.xCallbackParameters.pxCallbackFunction = xFunctionToPend;
    xMessage.u.xCallbackParameters.pvParameter1 = pvParameter1;
    xMessage.u.xCallbackParameters.ulParameter2 = ulParameter2;

    /* 发送消息到定时器队列尾部 */
    xReturn = xQueueSendToBack( xTimerQueue, &xMessage, xTicksToWait );

    /* 记录挂起函数调用的跟踪信息 */
    tracePEND_FUNC_CALL( xFunctionToPend, pvParameter1, ulParameter2, xReturn );

    /* 返回发送操作的结果 */
    return xReturn;
}

#endif /* INCLUDE_xTimerPendFunctionCall */
/*-----------------------------------------------------------*/

/* This entire source file will be skipped if the application is not configured
to include software timer functionality.  If you want to include software timer
functionality then ensure configUSE_TIMERS is set to 1 in FreeRTOSConfig.h. */
#endif /* configUSE_TIMERS == 1 */




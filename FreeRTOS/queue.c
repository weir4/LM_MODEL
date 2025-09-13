/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： LM_queue.c
 * 文件标识： 
 * 内容摘要： 队列模块定义
 * 其它说明： 无
 * 当前版本： FreeRTOS V9.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年09月01日
 *
 *******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include <stdlib.h>
#include <string.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#if ( configUSE_CO_ROUTINES == 1 )
	#include "croutine.h"
#endif



#define pxMutexHolder					pcTail
#define uxQueueType						pcHead
#define queueQUEUE_IS_MUTEX				NULL

/* Exported types ------------------------------------------------------------*/
/**
 * 队列结构体定义
 * 用于表示FreeRTOS中的队列、信号量和互斥量
 */
typedef struct QueueDefinition
{
	int8_t *pcHead;					/*< 指向队列存储区域的开头 */
	int8_t *pcTail;					/*< 指向队列存储区域末尾的字节 */
	int8_t *pcWriteTo;				/*< 指向存储区域中的下一个空闲位置 */

	union							/* 使用联合体确保两个互斥的结构成员不会同时出现（节省RAM） */
	{
		int8_t *pcReadFrom;			/*< 当结构用作队列时，指向最后一个读取队列项的位置 */
		UBaseType_t uxRecursiveCallCount;/*< 当结构用作互斥量时，维护递归互斥量被递归"获取"的次数 */
	} u;

	List_t xTasksWaitingToSend;		/*< 阻塞等待发送到此队列的任务列表（按优先级排序） */
	List_t xTasksWaitingToReceive;	/*< 阻塞等待从此队列读取的任务列表（按优先级排序） */

	volatile UBaseType_t uxMessagesWaiting;/*< 当前队列中的项目数量 */
	UBaseType_t uxLength;			/*< 队列的长度（定义为它将容纳的项目数量，而不是字节数） */
	UBaseType_t uxItemSize;			/*< 队列将容纳的每个项目的大小 */

	volatile int8_t cRxLock;		/*< 存储队列锁定时从队列接收的项目数量（从队列中移除） */
	volatile int8_t cTxLock;		/*< 存储队列锁定时传输到队列的项目数量（添加到队列） */

	#if( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
		uint8_t ucStaticallyAllocated;	/*< 如果队列使用的内存是静态分配的，则设置为pdTRUE，确保不会尝试释放内存 */
	#endif

	#if ( configUSE_QUEUE_SETS == 1 )
		struct QueueDefinition *pxQueueSetContainer; /*< 队列集容器指针 */
	#endif

	#if ( configUSE_TRACE_FACILITY == 1 )
		UBaseType_t uxQueueNumber;	/*< 队列编号 */
		uint8_t ucQueueType;		/*< 队列类型 */
	#endif

} xQUEUE;

/**
 * 队列类型定义
 * 保持与旧版本的兼容性
 */
typedef xQUEUE Queue_t;

#if ( configQUEUE_REGISTRY_SIZE > 0 )
	/**
	 * 队列注册表项结构体定义
	 * 允许为每个队列分配名称，使内核感知调试更加用户友好
	 */
	typedef struct QUEUE_REGISTRY_ITEM
	{
		const char *pcQueueName; /*< 队列名称 */
		QueueHandle_t xHandle;   /*< 队列句柄 */
	} xQueueRegistryItem;

	/**
	 * 队列注册表项类型定义
	 * 保持与旧版本的兼容性
	 */
	typedef xQueueRegistryItem QueueRegistryItem_t;
#endif /* configQUEUE_REGISTRY_SIZE */

/* Exported constants --------------------------------------------------------*/
/**
 * 队列锁定状态常量定义
 */
#define queueUNLOCKED					( ( int8_t ) -1 )  /*< 队列未锁定状态 */
#define queueLOCKED_UNMODIFIED			( ( int8_t ) 0 )   /*< 队列锁定但未修改状态 */

/**
 * 互斥量相关常量定义
 */
#define queueQUEUE_IS_MUTEX				NULL              /*< 表示队列实际上是互斥量的标记 */
#define queueMUTEX_GIVE_BLOCK_TIME		 ( ( TickType_t ) 0U ) /*< 互斥量给予阻塞时间 */

/**
 * 信号量相关常量定义
 */
#define queueSEMAPHORE_QUEUE_ITEM_LENGTH ( ( UBaseType_t ) 0 ) /*< 信号量队列项长度（信号量不存储数据） */

/* Exported macro ------------------------------------------------------------*/
#if( configUSE_PREEMPTION == 0 )
	/* 如果使用协作调度器，则不应因为唤醒了更高优先级的任务而执行 yield */
	#define queueYIELD_IF_USING_PREEMPTION()  /*< 协作调度器下不进行任务切换 */
#else
	#define queueYIELD_IF_USING_PREEMPTION() portYIELD_WITHIN_API()  /*< 抢占式调度器下进行任务切换 */
#endif

/**
 * 队列锁定宏
 * 锁定队列可防止ISR访问队列事件列表
 * @param pxQueue 要锁定的队列指针
 */
#define prvLockQueue( pxQueue )								\
	taskENTER_CRITICAL();									\
	{														\
		if( ( pxQueue )->cRxLock == queueUNLOCKED )			\
		{													\
			( pxQueue )->cRxLock = queueLOCKED_UNMODIFIED;	\
		}													\
		if( ( pxQueue )->cTxLock == queueUNLOCKED )			\
		{													\
			( pxQueue )->cTxLock = queueLOCKED_UNMODIFIED;	\
		}													\
	}														\
	taskEXIT_CRITICAL()

/* Exported functions --------------------------------------------------------*/
/* 注：队列模块的API函数在queue.h中声明，此处不重复 */

/* Private types -------------------------------------------------------------*/
/* 注：私有类型定义已包含在导出类型中 */

/* Private variables ---------------------------------------------------------*/
#if ( configQUEUE_REGISTRY_SIZE > 0 )
	/**
	 * 队列注册表数组
	 * 队列注册表只是一个便于内核感知调试器定位队列结构的机制
	 */
	PRIVILEGED_DATA QueueRegistryItem_t xQueueRegistry[ configQUEUE_REGISTRY_SIZE ];
#endif /* configQUEUE_REGISTRY_SIZE */

/* Private constants ---------------------------------------------------------*/
/* 注：私有常量定义已包含在导出常量中 */

/* Private macros ------------------------------------------------------------*/
/* 注：私有宏定义已包含在导出宏中 */

/* Private functions ---------------------------------------------------------*/
/**
 * 解锁队列
 * 解锁由prvLockQueue调用锁定的队列
 * 锁定队列不会阻止ISR向队列添加或删除项目，但会阻止ISR从队列事件列表中删除任务
 * 如果ISR发现队列被锁定，它将增加相应的队列锁计数，表示可能需要取消阻塞任务
 * 当队列解锁时，会检查这些锁计数并采取适当的操作
 * @param pxQueue 要解锁的队列指针
 */
static void prvUnlockQueue( Queue_t * const pxQueue ) PRIVILEGED_FUNCTION;

/**
 * 检查队列是否为空
 * 使用临界区确定队列中是否有任何数据
 * @param pxQueue 要检查的队列指针
 * @return 如果队列为空返回pdTRUE，否则返回pdFALSE
 */
static BaseType_t prvIsQueueEmpty( const Queue_t *pxQueue ) PRIVILEGED_FUNCTION;

/**
 * 检查队列是否已满
 * 使用临界区确定队列中是否有空间
 * @param pxQueue 要检查的队列指针
 * @return 如果没有空间返回pdTRUE，否则返回pdFALSE
 */
static BaseType_t prvIsQueueFull( const Queue_t *pxQueue ) PRIVILEGED_FUNCTION;

/**
 * 将数据复制到队列
 * 将项目复制到队列的前端或后端
 * @param pxQueue 目标队列指针
 * @param pvItemToQueue 要队列化的项目指针
 * @param xPosition 复制位置（前端或后端）
 * @return 复制结果
 */
static BaseType_t prvCopyDataToQueue( Queue_t * const pxQueue, const void *pvItemToQueue, const BaseType_t xPosition ) PRIVILEGED_FUNCTION;

/**
 * 从队列复制数据
 * 从队列中复制一个项目
 * @param pxQueue 源队列指针
 * @param pvBuffer 目标缓冲区指针
 */
static void prvCopyDataFromQueue( Queue_t * const pxQueue, void * const pvBuffer ) PRIVILEGED_FUNCTION;

#if ( configUSE_QUEUE_SETS == 1 )
	/**
	 * 通知队列集容器
	 * 检查队列是否是队列集的成员，如果是，则通知队列集队列包含数据
	 * @param pxQueue 队列指针
	 * @param xCopyPosition 复制位置
	 * @return 通知结果
	 */
	static BaseType_t prvNotifyQueueSetContainer( const Queue_t * const pxQueue, const BaseType_t xCopyPosition ) PRIVILEGED_FUNCTION;
#endif

/**
 * 初始化新队列
 * 在静态或动态分配Queue_t结构后调用，以填充结构的成员
 * @param uxQueueLength 队列长度
 * @param uxItemSize 项目大小
 * @param pucQueueStorage 队列存储区域指针
 * @param ucQueueType 队列类型
 * @param pxNewQueue 新队列指针
 */
static void prvInitialiseNewQueue( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, uint8_t *pucQueueStorage, const uint8_t ucQueueType, Queue_t *pxNewQueue ) PRIVILEGED_FUNCTION;

#if( configUSE_MUTEXES == 1 )
	/**
	 * 初始化互斥量
	 * 互斥量是一种特殊类型的队列。创建互斥量时，首先创建队列，然后调用prvInitialiseMutex()将队列配置为互斥量
	 * @param pxNewQueue 新队列指针
	 */
	static void prvInitialiseMutex( Queue_t *pxNewQueue ) PRIVILEGED_FUNCTION;
#endif

/*-----------------------------------------------------------*/

/*
 * Macro to mark a queue as locked.  Locking a queue prevents an ISR from
 * accessing the queue event lists.
 */
#define prvLockQueue( pxQueue )								\
	taskENTER_CRITICAL();									\
	{														\
		if( ( pxQueue )->cRxLock == queueUNLOCKED )			\
		{													\
			( pxQueue )->cRxLock = queueLOCKED_UNMODIFIED;	\
		}													\
		if( ( pxQueue )->cTxLock == queueUNLOCKED )			\
		{													\
			( pxQueue )->cTxLock = queueLOCKED_UNMODIFIED;	\
		}													\
	}														\
	taskEXIT_CRITICAL()
/*-----------------------------------------------------------*/

/*******************************************************************************
函数名称：xQueueGenericReset
功能描述：    
    重置消息队列至初始状态，可选择是否初始化等待列表。若队列非新建，则会尝试唤醒等待写入的任务。
输入参数：   
    xQueue: 要重置的消息队列句柄，实际为指向队列结构体的指针
    xNewQueue: 标识是否为新创建队列，pdTRUE表示新队列，pdFALSE表示已存在队列
输出参数：    
    无
返 回 值：    
    固定返回pdPASS，表示操作成功（为向前兼容而保留）
其它说明：    
    - 重置操作包括重置读写指针、队列计数和锁状态
    - 对于非新队列，会检查是否有任务阻塞在写入等待列表，并尝试唤醒一个任务
    - 使用临界区保护操作过程
修改日期      版本号          修改人            修改内容
------------------------------------------------------------------------------
2025/09/02     V1.00          Qiguo_Cui          创建
*******************************************************************************/
BaseType_t xQueueGenericReset( QueueHandle_t xQueue, BaseType_t xNewQueue )
{
    /* 将队列句柄转换为队列结构体指针 */
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;

    /* 断言确保队列指针有效 */
    configASSERT( pxQueue );

    /* 进入临界区保护队列操作 */
    taskENTER_CRITICAL();
    {
        /* 重置尾指针：指向存储区域末尾（头指针 + 总长度*项目大小） */
        pxQueue->pcTail = pxQueue->pcHead + ( pxQueue->uxLength * pxQueue->uxItemSize );
        
        /* 清空队列中等待的消息数量 */
        pxQueue->uxMessagesWaiting = ( UBaseType_t ) 0U;
        
        /* 重置写入位置指针：指向队列头部 */
        pxQueue->pcWriteTo = pxQueue->pcHead;
        
        /* 重置读取位置指针：指向队列倒数第一个元素位置（实现循环队列） */
        pxQueue->u.pcReadFrom = pxQueue->pcHead + ( ( pxQueue->uxLength - ( UBaseType_t ) 1U ) * pxQueue->uxItemSize );
        
        /* 解锁接收锁（允许读取操作） */
        pxQueue->cRxLock = queueUNLOCKED;
        
        /* 解锁发送锁（允许写入操作） */
        pxQueue->cTxLock = queueUNLOCKED;

        /* 根据队列类型进行差异化处理 */
        if( xNewQueue == pdFALSE )
        {
            /* 非新建队列：处理等待中的任务 */
            
            /* 检查发送等待列表（等待写入的任务）是否非空 */
            if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
            {
                /* 从发送等待列表中移除一个任务并就绪 */
                if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                {
                    /* 如果解除阻塞的任务优先级更高，则触发任务调度 */
                    queueYIELD_IF_USING_PREEMPTION();
                }
                else
                {
                    /* 空分支（用于覆盖率测试） */
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                /* 空分支（用于覆盖率测试） */
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            /* 新建队列：初始化任务等待列表 */
            vListInitialise( &( pxQueue->xTasksWaitingToSend ) );
            vListInitialise( &( pxQueue->xTasksWaitingToReceive ) );
        }
    }
    /* 退出临界区 */
    taskEXIT_CRITICAL();

    /* 返回固定成功值（向前兼容） */
    return pdPASS;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
函数名称：xQueueGenericCreateStatic
功能描述：    
    使用静态内存分配方式创建消息队列，需要提供队列存储区和队列控制块内存
输入参数：   
    uxQueueLength: 队列长度（最多可容纳的消息数量）
    uxItemSize: 每个队列项目的大小（以字节为单位）
    pucQueueStorage: 指向队列存储区的指针（用户提供的静态内存块）
    pxStaticQueue: 指向静态队列控制块的指针（用户提供的静态内存）
    ucQueueType: 队列类型标识（普通队列、信号量等）
输出参数：    
    无
返 回 值：    
    成功创建返回队列句柄(QueueHandle_t)，失败返回NULL
其它说明：    
    - 此函数仅在configSUPPORT_STATIC_ALLOCATION为1时可用
    - 需要用户预先分配队列存储区和队列控制块内存
    - 队列存储区大小至少为uxQueueLength * uxItemSize字节
    - 会进行多项参数有效性断言检查
修改日期      版本号          修改人            修改内容
------------------------------------------------------------------------------
2025/09/02     V1.00          Qiguo_Cui          创建
*******************************************************************************/
#if( configSUPPORT_STATIC_ALLOCATION == 1 )

QueueHandle_t xQueueGenericCreateStatic( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, uint8_t *pucQueueStorage, StaticQueue_t *pxStaticQueue, const uint8_t ucQueueType )
{
    /* 声明队列指针 */
    Queue_t *pxNewQueue;

    /* 断言检查：队列长度必须大于0 */
    configASSERT( uxQueueLength > ( UBaseType_t ) 0 );

    /* 断言检查：静态队列控制块指针不能为空 */
    configASSERT( pxStaticQueue != NULL );

    /* 断言检查：如果项目大小不为0，必须提供存储区；如果项目大小为0，不能提供存储区 */
    configASSERT( !( ( pucQueueStorage != NULL ) && ( uxItemSize == 0 ) ) );
    configASSERT( !( ( pucQueueStorage == NULL ) && ( uxItemSize != 0 ) ) );

    /* 调试断言：检查StaticQueue_t结构体大小是否与Queue_t一致 */
    #if( configASSERT_DEFINED == 1 )
    {
        /* 获取StaticQueue_t结构体大小并进行比较 */
        volatile size_t xSize = sizeof( StaticQueue_t );
        configASSERT( xSize == sizeof( Queue_t ) );
    }
    #endif /* configASSERT_DEFINED */

    /* 将静态队列控制块转换为Queue_t类型指针 */
    /* lint注释：不寻常的类型转换是允许的，因为结构体设计有相同的对齐方式，且大小已通过断言检查 */
    pxNewQueue = ( Queue_t * ) pxStaticQueue;

    /* 检查队列指针是否有效（非空） */
    if( pxNewQueue != NULL )
    {
        /* 如果支持动态分配，标记此队列为静态分配 */
        #if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
        {
            /* 设置静态分配标志，以便后续删除队列时知道不需要释放内存 */
            pxNewQueue->ucStaticallyAllocated = pdTRUE;
        }
        #endif /* configSUPPORT_DYNAMIC_ALLOCATION */

        /* 初始化新队列：设置队列长度、项目大小、存储区指针、类型等参数 */
        prvInitialiseNewQueue( uxQueueLength, uxItemSize, pucQueueStorage, ucQueueType, pxNewQueue );
    }

    /* 返回创建的队列句柄（可能为NULL如果创建失败） */
    return pxNewQueue;
}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

/*******************************************************************************
函数名称：xQueueGenericCreate
功能描述：    
    使用动态内存分配方式创建消息队列，自动分配队列存储区和控制块所需内存
输入参数：   
    uxQueueLength: 队列长度（最多可容纳的消息数量）
    uxItemSize: 每个队列项目的大小（以字节为单位）
    ucQueueType: 队列类型标识（普通队列、信号量、互斥量等）
输出参数：    
    无
返 回 值：    
    成功创建返回队列句柄(QueueHandle_t)，失败返回NULL
其它说明：    
    - 此函数仅在configSUPPORT_DYNAMIC_ALLOCATION为1时可用
    - 函数内部会自动计算所需内存并分配
    - 如果内存分配失败，则返回NULL
    - 对于项目大小为0的队列（如信号量），不会分配存储区
修改日期      版本号          修改人            修改内容
------------------------------------------------------------------------------
2025/09/02     V1.00          Qiguo_Cui          创建
*******************************************************************************/
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

QueueHandle_t xQueueGenericCreate( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, const uint8_t ucQueueType )
{
    /* 声明队列指针 */
    Queue_t *pxNewQueue;
    
    /* 声明队列存储区所需字节数 */
    size_t xQueueSizeInBytes;
    
    /* 声明指向队列存储区的指针 */
    uint8_t *pucQueueStorage;

    /* 断言检查：队列长度必须大于0 */
    configASSERT( uxQueueLength > ( UBaseType_t ) 0 );

    /* 计算队列存储区所需字节数 */
    if( uxItemSize == ( UBaseType_t ) 0 )
    {
        /* 项目大小为0（如信号量）：不需要分配存储区 */
        xQueueSizeInBytes = ( size_t ) 0;
    }
    else
    {
        /* 计算存储区总大小：队列长度 × 每个项目的大小 */
        xQueueSizeInBytes = ( size_t ) ( uxQueueLength * uxItemSize ); /*lint !e961 MISRA异常，某些端口上类型转换是冗余的 */
    }

    /* 分配内存：队列控制结构 + 队列存储区（一次性分配） */
    pxNewQueue = ( Queue_t * ) pvPortMalloc( sizeof( Queue_t ) + xQueueSizeInBytes );

    /* 检查内存是否分配成功 */
    if( pxNewQueue != NULL )
    {
        /* 计算存储区位置：跳过队列控制结构，指向存储区起始位置 */
        pucQueueStorage = ( ( uint8_t * ) pxNewQueue ) + sizeof( Queue_t );

        /* 如果支持静态分配，标记此队列为动态分配 */
        #if( configSUPPORT_STATIC_ALLOCATION == 1 )
        {
            /* 设置动态分配标志，以便后续删除队列时知道需要释放内存 */
            pxNewQueue->ucStaticallyAllocated = pdFALSE;
        }
        #endif /* configSUPPORT_STATIC_ALLOCATION */

        /* 初始化新队列：设置队列长度、项目大小、存储区指针、类型等参数 */
        prvInitialiseNewQueue( uxQueueLength, uxItemSize, pucQueueStorage, ucQueueType, pxNewQueue );
    }

    /* 返回创建的队列句柄（可能为NULL如果内存分配失败） */
    return pxNewQueue;
}

#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
/*-----------------------------------------------------------*/

/*******************************************************************************
函数名称：prvInitialiseNewQueue
功能描述：    
    初始化新队列的各个成员变量，设置队列的基本属性和存储区域，并调用重置函数完成初始化
输入参数：   
    uxQueueLength: 队列长度（最多可容纳的消息数量）
    uxItemSize: 每个队列项目的大小（以字节为单位）
    pucQueueStorage: 指向队列存储区的指针
    ucQueueType: 队列类型标识（普通队列、信号量、互斥量等）
    pxNewQueue: 指向要初始化的队列控制结构的指针
输出参数：    
    无（直接修改pxNewQueue指向的队列结构）
返 回 值：    
    无
其它说明：    
    - 此函数为静态函数，仅在当前文件内可见
    - 会根据项目大小是否为0来设置不同的存储区指针
    - 调用xQueueGenericReset完成队列的最终初始化
    - 根据配置选项设置队列类型和队列集容器
修改日期      版本号          修改人            修改内容
------------------------------------------------------------------------------
2025/09/02     V1.00          Qiguo_Cui          创建
*******************************************************************************/
static void prvInitialiseNewQueue( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, uint8_t *pucQueueStorage, const uint8_t ucQueueType, Queue_t *pxNewQueue )
{
    /* 防止编译器警告：当configUSE_TRACE_FACILITY未设置为1时，ucQueueType可能未使用 */
    ( void ) ucQueueType;

    /* 根据项目大小设置队列头指针 */
    if( uxItemSize == ( UBaseType_t ) 0 )
    {
        /* 项目大小为0（如信号量）：没有为队列存储区分配RAM
           但不能将pcHead设置为NULL，因为NULL用作表示队列被用作互斥量的键
           因此，将pcHead设置为指向队列本身的良性值，该值已知在内存映射中 */
        pxNewQueue->pcHead = ( int8_t * ) pxNewQueue;
    }
    else
    {
        /* 项目大小不为0：将头指针设置为队列存储区的起始位置 */
        pxNewQueue->pcHead = ( int8_t * ) pucQueueStorage;
    }

    /* 初始化队列成员，如队列类型定义中所述 */
    pxNewQueue->uxLength = uxQueueLength;    /* 设置队列长度 */
    pxNewQueue->uxItemSize = uxItemSize;     /* 设置每个项目的大小 */
    
    /* 调用通用重置函数初始化队列状态（pdTRUE表示是新队列） */
    ( void ) xQueueGenericReset( pxNewQueue, pdTRUE );

    /* 如果启用了跟踪功能，设置队列类型 */
    #if ( configUSE_TRACE_FACILITY == 1 )
    {
        pxNewQueue->ucQueueType = ucQueueType;
    }
    #endif /* configUSE_TRACE_FACILITY */

    /* 如果启用了队列集功能，初始化队列集容器指针为NULL */
    #if( configUSE_QUEUE_SETS == 1 )
    {
        pxNewQueue->pxQueueSetContainer = NULL;
    }
    #endif /* configUSE_QUEUE_SETS */

    /* 队列创建跟踪宏（用于调试和性能分析） */
    traceQUEUE_CREATE( pxNewQueue );
}
/*-----------------------------------------------------------*/

/*******************************************************************************
函数名称：prvInitialiseMutex
功能描述：    
    初始化互斥量队列的特殊属性，设置互斥量持有者、类型和递归计数，并将其置于可用状态
输入参数：   
    pxNewQueue: 指向要初始化的互斥量队列的指针
输出参数：    
    无（直接修改pxNewQueue指向的互斥量队列结构）
返 回 值：    
    无
其它说明：    
    - 此函数为静态函数，仅在当前文件内可见
    - 仅在configUSE_MUTEXES为1时编译
    - 会覆盖通用队列初始化中的某些成员，以适应互斥量的特殊需求
    - 将互斥量初始化为可用状态（通过发送一个空消息）
修改日期      版本号          修改人            修改内容
------------------------------------------------------------------------------
2025/09/02     V1.00          Qiguo_Cui          创建
*******************************************************************************/
#if( configUSE_MUTEXES == 1 )

static void prvInitialiseMutex( Queue_t *pxNewQueue )
{
    /* 检查队列指针是否有效 */
    if( pxNewQueue != NULL )
    {
        /* 队列创建函数已经为通用队列正确设置了所有队列结构成员，
           但此函数正在创建互斥量。需要覆盖那些需要不同设置的成员 -
           特别是优先级继承所需的信息。 */
        
        /* 初始化互斥量持有者为空（当前无任务持有） */
        pxNewQueue->pxMutexHolder = NULL;
        
        /* 设置队列类型为互斥量 */
        pxNewQueue->uxQueueType = queueQUEUE_IS_MUTEX;

        /* 如果是递归互斥量，初始化递归调用计数为0 */
        pxNewQueue->u.uxRecursiveCallCount = 0;

        /* 跟踪互斥量创建事件 */
        traceCREATE_MUTEX( pxNewQueue );

        /* 通过向队列发送空消息，将互斥量初始化为可用状态
           使用queueSEND_TO_BACK确保按照预期方式发送
           超时设置为0，表示不等待 */
        ( void ) xQueueGenericSend( pxNewQueue, NULL, ( TickType_t ) 0U, queueSEND_TO_BACK );
    }
    else
    {
        /* 互斥量创建失败跟踪 */
        traceCREATE_MUTEX_FAILED();
    }
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

/*******************************************************************************
函数名称：xQueueCreateMutex
功能描述：    
    创建动态互斥量，使用动态内存分配方式，初始化互斥量的特殊属性并返回互斥量句柄
输入参数：   
    ucQueueType: 队列类型标识（用于区分不同类型的互斥量）
输出参数：    
    无
返 回 值：    
    成功创建返回互斥量句柄(QueueHandle_t)，失败返回NULL
其它说明：    
    - 此函数仅在configUSE_MUTEXES和configSUPPORT_DYNAMIC_ALLOCATION都为1时可用
    - 使用动态内存分配创建互斥量，无需预先分配内存
    - 互斥量是一种特殊的队列，长度为1，项目大小为0
    - 创建成功后，互斥量处于可用状态
修改日期      版本号          修改人            修改内容
------------------------------------------------------------------------------
2025/09/02     V1.00          Qiguo_Cui          创建
*******************************************************************************/
#if( ( configUSE_MUTEXES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

QueueHandle_t xQueueCreateMutex( const uint8_t ucQueueType )
{
    /* 声明队列指针 */
    Queue_t *pxNewQueue;
    
    /* 定义互斥量的固定参数：长度为1，项目大小为0 */
    const UBaseType_t uxMutexLength = ( UBaseType_t ) 1, uxMutexSize = ( UBaseType_t ) 0;

    /* 使用通用队列创建函数创建基本队列结构
       互斥量是长度为1、项目大小为0的特殊队列 */
    pxNewQueue = ( Queue_t * ) xQueueGenericCreate( uxMutexLength, uxMutexSize, ucQueueType );
    
    /* 初始化互斥量的特殊属性（持有者、递归计数等） */
    prvInitialiseMutex( pxNewQueue );

    /* 返回创建的互斥量句柄（可能为NULL如果创建失败） */
    return pxNewQueue;
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称: xQueueCreateMutexStatic
 * 功能描述: 创建静态分配的互斥量队列。该函数用于创建一个静态分配的互斥量，使用预先分配的内存空间，
 *           避免了动态内存分配，适用于对内存分配有严格要求的嵌入式系统。
 * 输入参数: 
 *   - ucQueueType: 队列类型标识符，用于区分不同类型的队列（如互斥量、信号量等）
 *   - pxStaticQueue: 指向静态队列结构的指针，提供队列所需的内存空间
 * 输出参数: 无
 * 返 回 值: 
 *   - 成功: 返回创建的互斥量队列句柄
 *   - 失败: 返回NULL（当参数无效或初始化失败时）
 * 其它说明: 
 *   1. 此函数仅在 configUSE_MUTEXES 和 configSUPPORT_STATIC_ALLOCATION 都为 1 时可用
 *   2. 互斥量队列的长度固定为1，大小为0，因为互斥量不需要存储实际数据
 *   3. 创建后会调用 prvInitialiseMutex 函数对互斥量进行初始化
 * 修改日期      版本号          修改人            修改内容
 * ----------------------------------------------------------------------------
 * 2024/06/02     V1.00          ChatGPT           创建并添加详细注释
 *******************************************************************************/
#if( ( configUSE_MUTEXES == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )

QueueHandle_t xQueueCreateMutexStatic( const uint8_t ucQueueType, StaticQueue_t *pxStaticQueue )
{
Queue_t *pxNewQueue;                                       /* 定义新队列指针 */
const UBaseType_t uxMutexLength = ( UBaseType_t ) 1;       /* 互斥量队列长度固定为1 */
const UBaseType_t uxMutexSize = ( UBaseType_t ) 0;         /* 互斥量队列项大小为0（不存储数据） */

    /* 防止编译器警告：当 configUSE_TRACE_FACILITY 不等于 1 时，避免未使用参数的警告 */
    ( void ) ucQueueType;

    /* 调用通用队列创建函数创建静态队列
       参数说明：
       - uxMutexLength: 队列长度（互斥量固定为1）
       - uxMutexSize: 队列项大小（互斥量不需要存储数据，故为0）
       - NULL: 队列存储区域指针（互斥量不需要数据存储，故为NULL）
       - pxStaticQueue: 静态队列结构指针
       - ucQueueType: 队列类型标识符 */
    pxNewQueue = ( Queue_t * ) xQueueGenericCreateStatic( uxMutexLength, uxMutexSize, NULL, pxStaticQueue, ucQueueType );

    /* 初始化互斥量特定属性 */
    prvInitialiseMutex( pxNewQueue );

    /* 返回创建的互斥量队列句柄 */
    return pxNewQueue;
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xQueueGetMutexHolder
 * 功能描述：获取互斥锁的当前持有者任务句柄
 *           此函数用于查询指定互斥锁的当前持有者，是xSemaphoreGetMutexHolder的内部实现
 * 输入参数：
 *   - xSemaphore: 信号量句柄，实际上应该是互斥锁的句柄
 * 输出参数：无
 * 返 回 值：
 *   - void*: 互斥锁持有者的任务句柄，如果不是互斥锁或没有持有者则返回NULL
 * 其它说明：
 *   - 此函数由xSemaphoreGetMutexHolder()调用，不应直接调用
 *   - 在临界区内执行，确保查询操作的原子性
 *   - 只能用于确定调用任务是否是互斥锁持有者，不适合用于确定持有者身份
 *   - 因为持有者可能在临界区退出和函数返回之间发生变化
 *   - 仅在启用互斥量和xSemaphoreGetMutexHolder功能时编译
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
#if ( ( configUSE_MUTEXES == 1 ) && ( INCLUDE_xSemaphoreGetMutexHolder == 1 ) )

void* xQueueGetMutexHolder( QueueHandle_t xSemaphore )
{
    void *pxReturn;  /* 返回值，存储互斥锁持有者的任务句柄 */

    /* 此函数由xSemaphoreGetMutexHolder()调用，不应直接调用。
       注意：这是确定调用任务是否是互斥锁持有者的好方法，
       但不是确定互斥锁持有者身份的好方法，因为持有者可能在
       以下临界区退出和函数返回之间发生变化 */
    
    /* 进入临界区，保护互斥锁持有者查询操作 */
    taskENTER_CRITICAL();
    {
        /* 检查信号量类型是否为互斥锁 */
        if( ( ( Queue_t * ) xSemaphore )->uxQueueType == queueQUEUE_IS_MUTEX )
        {
            /* 获取互斥锁的当前持有者任务句柄 */
            pxReturn = ( void * ) ( ( Queue_t * ) xSemaphore )->pxMutexHolder;
        }
        else
        {
            /* 如果不是互斥锁，返回NULL */
            pxReturn = NULL;
        }
    }
    /* 退出临界区 */
    taskEXIT_CRITICAL();

    /* 返回互斥锁持有者的任务句柄或NULL */
    return pxReturn;
} /*lint !e818 xSemaphore不能是指向const的指针，因为它是一个typedef */

#endif
/*-----------------------------------------------------------*/

#if ( configUSE_RECURSIVE_MUTEXES == 1 )

	BaseType_t xQueueGiveMutexRecursive( QueueHandle_t xMutex )
	{
	BaseType_t xReturn;
	Queue_t * const pxMutex = ( Queue_t * ) xMutex;

		configASSERT( pxMutex );

		/* If this is the task that holds the mutex then pxMutexHolder will not
		change outside of this task.  If this task does not hold the mutex then
		pxMutexHolder can never coincidentally equal the tasks handle, and as
		this is the only condition we are interested in it does not matter if
		pxMutexHolder is accessed simultaneously by another task.  Therefore no
		mutual exclusion is required to test the pxMutexHolder variable. */
		if( pxMutex->pxMutexHolder == ( void * ) xTaskGetCurrentTaskHandle() ) /*lint !e961 Not a redundant cast as TaskHandle_t is a typedef. */
		{
			traceGIVE_MUTEX_RECURSIVE( pxMutex );

			/* uxRecursiveCallCount cannot be zero if pxMutexHolder is equal to
			the task handle, therefore no underflow check is required.  Also,
			uxRecursiveCallCount is only modified by the mutex holder, and as
			there can only be one, no mutual exclusion is required to modify the
			uxRecursiveCallCount member. */
			( pxMutex->u.uxRecursiveCallCount )--;

			/* Has the recursive call count unwound to 0? */
			if( pxMutex->u.uxRecursiveCallCount == ( UBaseType_t ) 0 )
			{
				/* Return the mutex.  This will automatically unblock any other
				task that might be waiting to access the mutex. */
				( void ) xQueueGenericSend( pxMutex, NULL, queueMUTEX_GIVE_BLOCK_TIME, queueSEND_TO_BACK );
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			xReturn = pdPASS;
		}
		else
		{
			/* The mutex cannot be given because the calling task is not the
			holder. */
			xReturn = pdFAIL;

			traceGIVE_MUTEX_RECURSIVE_FAILED( pxMutex );
		}

		return xReturn;
	}

#endif /* configUSE_RECURSIVE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( configUSE_RECURSIVE_MUTEXES == 1 )

/*******************************************************************************
 * 函数名称：xQueueTakeMutexRecursive
 * 功能描述：递归互斥量的获取操作，允许同一个任务多次获取递归互斥量
 *           此函数用于获取递归互斥量，如果任务已持有互斥量则增加递归计数
 * 输入参数：
 *   - xMutex: 递归互斥量的句柄
 *   - xTicksToWait: 等待获取互斥量的最大时间（以时钟节拍为单位）
 *     特殊值：portMAX_DELAY表示无限期等待
 * 输出参数：无
 * 返 回 值：
 *   - BaseType_t: 操作结果
 *     pdPASS: 成功获取递归互斥量
 *     pdFAIL: 获取失败，超时或发生错误
 * 其它说明：
 *   - 此函数仅在启用递归互斥量功能(configUSE_RECURSIVE_MUTEXES == 1)时编译
 *   - 递归互斥量允许同一个任务多次获取，必须释放相同次数才会真正释放
 *   - 如果任务已持有互斥量，则直接增加递归计数而不需要等待
 *   - 如果任务未持有互斥量，则尝试获取互斥量，可能需要等待
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
BaseType_t xQueueTakeMutexRecursive( QueueHandle_t xMutex, TickType_t xTicksToWait )
{
    BaseType_t xReturn;               /* 返回值：操作结果 */
    Queue_t * const pxMutex = ( Queue_t * ) xMutex;  /* 将互斥量句柄转换为队列结构指针 */

    /* 断言检查互斥量指针有效性 */
    configASSERT( pxMutex );

    /* 关于互斥 exclusion 的注释与xQueueGiveMutexRecursive()中的相同 */

    /* 跟踪递归互斥量获取事件 */
    traceTAKE_MUTEX_RECURSIVE( pxMutex );

    /* 检查当前任务是否已经是互斥量的持有者 */
    if( pxMutex->pxMutexHolder == ( void * ) xTaskGetCurrentTaskHandle() ) /*lint !e961 Cast is not redundant as TaskHandle_t is a typedef. */
    {
        /* 当前任务已持有互斥量，增加递归计数 */
        ( pxMutex->u.uxRecursiveCallCount )++;
        xReturn = pdPASS;
    }
    else
    {
        /* 当前任务未持有互斥量，尝试获取互斥量 */
        xReturn = xQueueGenericReceive( pxMutex, NULL, xTicksToWait, pdFALSE );

        /* 只有成功获取互斥量时才会返回pdPASS。
           调用任务在到达此处之前可能已进入阻塞状态 */
        if( xReturn != pdFAIL )
        {
            /* 成功获取互斥量，增加递归计数 */
            ( pxMutex->u.uxRecursiveCallCount )++;
        }
        else
        {
            /* 获取互斥量失败：跟踪失败事件 */
            traceTAKE_MUTEX_RECURSIVE_FAILED( pxMutex );
        }
    }

    return xReturn;
}

#endif /* configUSE_RECURSIVE_MUTEXES */
/*-----------------------------------------------------------*/

#if( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )

/*******************************************************************************
 * 函数名称：xQueueCreateCountingSemaphoreStatic
 * 功能描述：创建计数信号量的静态版本，使用静态分配的内存
 *           此函数创建并初始化一个计数信号量，使用预先分配的内存空间
 * 输入参数：
 *   - uxMaxCount: 信号量的最大计数值，也是信号量的最大资源数量
 *   - uxInitialCount: 信号量的初始计数值，表示初始可用的资源数量
 *   - pxStaticQueue: 指向StaticQueue_t结构的指针，用于存储队列状态和数据
 * 输出参数：无
 * 返 回 值：
 *   - QueueHandle_t: 创建的计数信号量的句柄，如果创建失败则返回NULL
 * 其它说明：
 *   - 此函数仅在启用计数信号量和静态分配功能时编译
 *   - 使用静态分配的内存，避免动态内存分配
 *   - 计数信号量用于管理有限数量的资源，跟踪可用资源的数量
 *   - 初始计数值不能超过最大计数值
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
QueueHandle_t xQueueCreateCountingSemaphoreStatic( const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount, StaticQueue_t *pxStaticQueue )
{
    QueueHandle_t xHandle;  /* 返回值：创建的计数信号量句柄 */

    /* 断言检查最大计数值不为0 */
    configASSERT( uxMaxCount != 0 );
    /* 断言检查初始计数值不超过最大计数值 */
    configASSERT( uxInitialCount <= uxMaxCount );

    /* 使用通用队列创建函数创建计数信号量 */
    xHandle = xQueueGenericCreateStatic( uxMaxCount,                     /* 队列长度（最大计数值） */
                                        queueSEMAPHORE_QUEUE_ITEM_LENGTH, /* 队列项长度（信号量使用固定长度） */
                                        NULL,                            /* 队列项存储区（信号量不需要） */
                                        pxStaticQueue,                   /* 静态队列结构 */
                                        queueQUEUE_TYPE_COUNTING_SEMAPHORE ); /* 队列类型：计数信号量 */

    /* 检查信号量是否创建成功 */
    if( xHandle != NULL )
    {
        /* 设置信号量的初始计数值 */
        ( ( Queue_t * ) xHandle )->uxMessagesWaiting = uxInitialCount;

        /* 跟踪计数信号量创建事件 */
        traceCREATE_COUNTING_SEMAPHORE();
    }
    else
    {
        /* 跟踪计数信号量创建失败事件 */
        traceCREATE_COUNTING_SEMAPHORE_FAILED();
    }

    /* 返回创建的计数信号量句柄 */
    return xHandle;
}

#endif /* ( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) ) */
/*-----------------------------------------------------------*/

#if( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

/*******************************************************************************
 * 函数名称：xQueueCreateCountingSemaphore
 * 功能描述：创建计数信号量的动态版本，使用动态分配的内存
 *           此函数创建并初始化一个计数信号量，使用动态分配的内存空间
 * 输入参数：
 *   - uxMaxCount: 信号量的最大计数值，也是信号量的最大资源数量
 *   - uxInitialCount: 信号量的初始计数值，表示初始可用的资源数量
 * 输出参数：无
 * 返 回 值：
 *   - QueueHandle_t: 创建的计数信号量的句柄，如果创建失败则返回NULL
 * 其它说明：
 *   - 此函数仅在启用计数信号量和动态分配功能时编译
 *   - 使用动态分配的内存，自动管理内存分配和释放
 *   - 计数信号量用于管理有限数量的资源，跟踪可用资源的数量
 *   - 初始计数值不能超过最大计数值
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
QueueHandle_t xQueueCreateCountingSemaphore( const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount )
{
    QueueHandle_t xHandle;  /* 返回值：创建的计数信号量句柄 */

    /* 断言检查最大计数值不为0 */
    configASSERT( uxMaxCount != 0 );
    /* 断言检查初始计数值不超过最大计数值 */
    configASSERT( uxInitialCount <= uxMaxCount );

    /* 使用通用队列创建函数创建计数信号量 */
    xHandle = xQueueGenericCreate( uxMaxCount,                     /* 队列长度（最大计数值） */
                                  queueSEMAPHORE_QUEUE_ITEM_LENGTH, /* 队列项长度（信号量使用固定长度） */
                                  queueQUEUE_TYPE_COUNTING_SEMAPHORE ); /* 队列类型：计数信号量 */

    /* 检查信号量是否创建成功 */
    if( xHandle != NULL )
    {
        /* 设置信号量的初始计数值 */
        ( ( Queue_t * ) xHandle )->uxMessagesWaiting = uxInitialCount;

        /* 跟踪计数信号量创建事件 */
        traceCREATE_COUNTING_SEMAPHORE();
    }
    else
    {
        /* 跟踪计数信号量创建失败事件 */
        traceCREATE_COUNTING_SEMAPHORE_FAILED();
    }

    /* 返回创建的计数信号量句柄 */
    return xHandle;
}

#endif /* ( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) ) */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xQueueGenericSend
 * 功能描述：通用队列发送函数，支持向队列发送数据的多种方式（后向、前向、覆盖）
 *           此函数是FreeRTOS队列操作的核心函数，处理数据发送、任务阻塞和唤醒等复杂逻辑
 * 输入参数：
 *   - xQueue: 队列句柄，指定要操作的队列
 *   - pvItemToQueue: 指向要发送数据的指针，如果队列项大小为0则可以为NULL
 *   - xTicksToWait: 等待队列有空间的最大时间（以时钟节拍为单位）
 *     特殊值：portMAX_DELAY表示无限期等待，0表示不等待立即返回
 *   - xCopyPosition: 数据复制位置，指定数据发送到队列的位置
 *     queueSEND_TO_BACK: 发送到队列尾部（先进先出）
 *     queueSEND_TO_FRONT: 发送到队列头部（后进先出）
 *     queueOVERWRITE: 覆盖队列头部的数据（仅用于长度为1的队列）
 * 输出参数：无
 * 返 回 值：
 *   - BaseType_t: 操作结果
 *     pdPASS: 成功发送数据到队列
 *     errQUEUE_FULL: 队列已满且等待超时，发送失败
 * 其它说明：
 *   - 此函数处理队列发送的所有情况，包括普通队列、信号量和互斥量
 *   - 支持多种等待策略和数据处理方式
 *   - 处理任务阻塞、超时和优先级唤醒等复杂逻辑
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
BaseType_t xQueueGenericSend( QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait, const BaseType_t xCopyPosition )
{
    BaseType_t xEntryTimeSet = pdFALSE;  /* 标志：是否已设置超时状态 */
    BaseType_t xYieldRequired;           /* 标志：是否需要任务切换 */
    TimeOut_t xTimeOut;                  /* 超时状态结构 */
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* 将队列句柄转换为队列结构指针 */

    /* 断言检查队列指针有效性 */
    configASSERT( pxQueue );
    /* 断言检查：如果队列项大小不为0，则数据指针不能为NULL */
    configASSERT( !( ( pvItemToQueue == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );
    /* 断言检查：覆盖模式只能用于长度为1的队列 */
    configASSERT( !( ( xCopyPosition == queueOVERWRITE ) && ( pxQueue->uxLength != 1 ) ) );
    /* 如果启用了调度器状态获取或定时器功能，检查调度器状态 */
    #if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
    {
        /* 断言检查：如果调度器已挂起，则不能等待 */
        configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
    }
    #endif

    /* 此函数稍微放宽了编码标准，允许在函数内部使用return语句。
       这样做是为了提高执行时间效率 */
    for( ;; )
    {
        /* 进入临界区 */
        taskENTER_CRITICAL();
        {
            /* 队列现在有空间吗？运行任务必须是想要访问队列的最高优先级任务。
               如果要覆盖队列头部的项目，则队列是否已满无关紧要 */
            if( ( pxQueue->uxMessagesWaiting < pxQueue->uxLength ) || ( xCopyPosition == queueOVERWRITE ) )
            {
                /* 跟踪队列发送事件 */
                traceQUEUE_SEND( pxQueue );
                /* 复制数据到队列并检查是否需要任务切换 */
                xYieldRequired = prvCopyDataToQueue( pxQueue, pvItemToQueue, xCopyPosition );

                /* 如果启用了队列集功能 */
                #if ( configUSE_QUEUE_SETS == 1 )
                {
                    if( pxQueue->pxQueueSetContainer != NULL )
                    {
                        /* 通知队列集容器 */
                        if( prvNotifyQueueSetContainer( pxQueue, xCopyPosition ) != pdFALSE )
                        {
                            /* 队列是队列集的成员，并且发布到队列集导致更高优先级的任务解除阻塞。
                               需要上下文切换 */
                            queueYIELD_IF_USING_PREEMPTION();
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    else
                    {
                        /* 如果有任务正在等待队列中的数据到达，则立即解除其阻塞 */
                        if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                        {
                            if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                            {
                                /* 被解除阻塞的任务的优先级高于我们自己的优先级，因此立即让出。
                                   从临界区内这样做是可以的 - 内核会处理这个问题 */
                                queueYIELD_IF_USING_PREEMPTION();
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();
                            }
                        }
                        else if( xYieldRequired != pdFALSE )
                        {
                            /* 此路径是一个特殊情况，只有在任务持有多个互斥量并且互斥量以与获取顺序不同的顺序返回时才会执行 */
                            queueYIELD_IF_USING_PREEMPTION();
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                }
                #else /* configUSE_QUEUE_SETS */
                {
                    /* 如果有任务正在等待队列中的数据到达，则立即解除其阻塞 */
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                    {
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                        {
                            /* 被解除阻塞的任务的优先级高于我们自己的优先级，因此立即让出。
                               从临界区内这样做是可以的 - 内核会处理这个问题 */
                            queueYIELD_IF_USING_PREEMPTION();
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    else if( xYieldRequired != pdFALSE )
                    {
                        /* 此路径是一个特殊情况，只有在任务持有多个互斥量并且互斥量以与获取顺序不同的顺序返回时才会执行 */
                        queueYIELD_IF_USING_PREEMPTION();
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                #endif /* configUSE_QUEUE_SETS */

                /* 退出临界区并返回成功 */
                taskEXIT_CRITICAL();
                return pdPASS;
            }
            else
            {
                /* 队列已满，检查是否需要等待 */
                if( xTicksToWait == ( TickType_t ) 0 )
                {
                    /* 队列已满且未指定阻塞时间（或阻塞时间已过期），立即离开 */
                    taskEXIT_CRITICAL();

                    /* 在退出函数之前返回到原始特权级别 */
                    traceQUEUE_SEND_FAILED( pxQueue );
                    return errQUEUE_FULL;
                }
                else if( xEntryTimeSet == pdFALSE )
                {
                    /* 队列已满且指定了阻塞时间，配置超时结构 */
                    vTaskSetTimeOutState( &xTimeOut );
                    xEntryTimeSet = pdTRUE;
                }
                else
                {
                    /* 进入时间已设置 */
                    mtCOVERAGE_TEST_MARKER();
                }
            }
        }
        /* 退出临界区 */
        taskEXIT_CRITICAL();

        /* 现在临界区已退出，中断和其他任务可以向队列发送和从队列接收 */

        /* 挂起所有任务并锁定队列 */
        vTaskSuspendAll();
        prvLockQueue( pxQueue );

        /* 更新超时状态以查看是否已过期 */
        if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
        {
            /* 检查队列是否仍然满 */
            if( prvIsQueueFull( pxQueue ) != pdFALSE )
            {
                /* 跟踪队列发送阻塞事件 */
                traceBLOCKING_ON_QUEUE_SEND( pxQueue );
                /* 将当前任务放置到发送等待事件列表 */
                vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToSend ), xTicksToWait );

                /* 解锁队列意味着队列事件可以影响事件列表。
                   现在发生的中断可能会再次从此事件列表中移除此任务 -
                   但由于调度器已挂起，任务将进入待就绪列表而不是实际的就绪列表 */
                prvUnlockQueue( pxQueue );

                /* 恢复调度器将把任务从待就绪列表移动到就绪列表 -
                   因此可能此任务在让出之前已经在就绪列表中 -
                   在这种情况下，除非待就绪列表中还有更高优先级的任务，否则让出不会导致上下文切换 */
                if( xTaskResumeAll() == pdFALSE )
                {
                    portYIELD_WITHIN_API();
                }
            }
            else
            {
                /* 队列不再满，重试 */
                prvUnlockQueue( pxQueue );
                ( void ) xTaskResumeAll();
            }
        }
        else
        {
            /* 超时已过期 */
            prvUnlockQueue( pxQueue );
            ( void ) xTaskResumeAll();

            /* 跟踪队列发送失败事件 */
            traceQUEUE_SEND_FAILED( pxQueue );
            return errQUEUE_FULL;
        }
    }
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xQueueGenericSendFromISR
 * 功能描述：从中断服务程序(ISR)向队列发送数据的通用函数，支持多种发送方式
 *           此函数是xQueueGenericSend的中断安全版本，专为ISR设计
 * 输入参数：
 *   - xQueue: 队列句柄，指定要操作的队列
 *   - pvItemToQueue: 指向要发送数据的指针，如果队列项大小为0则可以为NULL
 *   - pxHigherPriorityTaskWoken: 指向更高优先级任务唤醒标志的指针
 *     用于指示是否有更高优先级任务被唤醒，需要上下文切换
 *   - xCopyPosition: 数据复制位置，指定数据发送到队列的位置
 *     queueSEND_TO_BACK: 发送到队列尾部（先进先出）
 *     queueSEND_TO_FRONT: 发送到队列头部（后进先出）
 *     queueOVERWRITE: 覆盖队列头部的数据（仅用于长度为1的队列）
 * 输出参数：
 *   - pxHigherPriorityTaskWoken: 被设置的更高优先级任务唤醒标志
 * 返 回 值：
 *   - BaseType_t: 操作结果
 *     pdPASS: 成功发送数据到队列
 *     errQUEUE_FULL: 队列已满，发送失败
 * 其它说明：
 *   - 此函数是中断安全版本，只能在ISR中调用
 *   - 验证中断优先级，确保不会从过高优先级的中断调用
 *   - 使用中断安全的临界区保护队列操作
 *   - 处理队列锁定状态，支持队列集功能
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          创建及注释
 *******************************************************************************/
BaseType_t xQueueGenericSendFromISR( QueueHandle_t xQueue, const void * const pvItemToQueue, BaseType_t * const pxHigherPriorityTaskWoken, const BaseType_t xCopyPosition )
{
    BaseType_t xReturn;  /* 返回值：操作结果 */
    UBaseType_t uxSavedInterruptStatus;  /* 保存的中断状态 */
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* 将队列句柄转换为队列结构指针 */

    /* 断言检查队列指针有效性 */
    configASSERT( pxQueue );
    /* 断言检查：如果队列项大小不为0，则数据指针不能为NULL */
    configASSERT( !( ( pvItemToQueue == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );
    /* 断言检查：覆盖模式只能用于长度为1的队列 */
    configASSERT( !( ( xCopyPosition == queueOVERWRITE ) && ( pxQueue->uxLength != 1 ) ) );

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

    /* 类似于xQueueGenericSend，但在队列没有空间时不会阻塞。
       也不会直接唤醒被阻塞在队列读取上的任务，而是返回一个标志来指示是否需要上下文切换
       （即是否有比我们优先级更高的任务被此发送操作唤醒） */
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        /* 检查队列是否有空间或是否使用覆盖模式 */
        if( ( pxQueue->uxMessagesWaiting < pxQueue->uxLength ) || ( xCopyPosition == queueOVERWRITE ) )
        {
            const int8_t cTxLock = pxQueue->cTxLock;  /* 保存当前的发送锁定状态 */

            /* 跟踪从ISR发送队列事件 */
            traceQUEUE_SEND_FROM_ISR( pxQueue );

            /* 信号量使用xQueueGiveFromISR()，因此pxQueue不会是信号量或互斥量。
               这意味着prvCopyDataToQueue()不会导致任务失去优先级继承，
               并且即使失去优先级继承函数在访问就绪列表之前不检查调度器是否挂起，
               也可以在这里调用prvCopyDataToQueue() */
            ( void ) prvCopyDataToQueue( pxQueue, pvItemToQueue, xCopyPosition );

            /* 如果队列被锁定，则不更改事件列表。这将在稍后队列解锁时完成 */
            if( cTxLock == queueUNLOCKED )
            {
                /* 如果启用了队列集功能 */
                #if ( configUSE_QUEUE_SETS == 1 )
                {
                    if( pxQueue->pxQueueSetContainer != NULL )
                    {
                        /* 通知队列集容器 */
                        if( prvNotifyQueueSetContainer( pxQueue, xCopyPosition ) != pdFALSE )
                        {
                            /* 队列是队列集的成员，并且发布到队列集导致更高优先级的任务解除阻塞。
                               需要上下文切换 */
                            if( pxHigherPriorityTaskWoken != NULL )
                            {
                                *pxHigherPriorityTaskWoken = pdTRUE;
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    else
                    {
                        /* 检查是否有任务等待接收数据 */
                        if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                        {
                            /* 从事件列表中移除等待接收的任务 */
                            if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                            {
                                /* 等待的任务具有更高的优先级，因此记录需要上下文切换 */
                                if( pxHigherPriorityTaskWoken != NULL )
                                {
                                    *pxHigherPriorityTaskWoken = pdTRUE;
                                }
                                else
                                {
                                    mtCOVERAGE_TEST_MARKER();
                                }
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                }
                #else /* configUSE_QUEUE_SETS */
                {
                    /* 检查是否有任务等待接收数据 */
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                    {
                        /* 从事件列表中移除等待接收的任务 */
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                        {
                            /* 等待的任务具有更高的优先级，因此记录需要上下文切换 */
                            if( pxHigherPriorityTaskWoken != NULL )
                            {
                                *pxHigherPriorityTaskWoken = pdTRUE;
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                #endif /* configUSE_QUEUE_SETS */
            }
            else
            {
                /* 增加锁定计数，以便解锁队列的任务知道在锁定时有数据发布 */
                pxQueue->cTxLock = ( int8_t ) ( cTxLock + 1 );
            }

            xReturn = pdPASS;
        }
        else
        {
            /* 队列已满，跟踪发送失败事件 */
            traceQUEUE_SEND_FROM_ISR_FAILED( pxQueue );
            xReturn = errQUEUE_FULL;
        }
    }
    /* 恢复中断状态 */
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

    return xReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xQueueGiveFromISR
 * 功能描述：从中断服务程序（ISR）中释放信号量（适用于项大小为0的队列）
 *           此函数专门用于信号量操作，通过增加队列中的消息计数实现信号量释放
 *           会检查是否有任务因等待此信号量而被阻塞，并在必要时唤醒更高优先级任务
 * 输入参数：
 *   - xQueue: 队列句柄，实际应为信号量句柄（uxItemSize必须为0）
 *   - pxHigherPriorityTaskWoken: 指向更高优先级任务唤醒标志的指针
 * 输出参数：
 *   - pxHigherPriorityTaskWoken: 指示是否有更高优先级任务被唤醒，需要上下文切换
 * 返 回 值：
 *   - BaseType_t: 成功释放信号量返回pdPASS，队列已满返回errQUEUE_FULL
 * 其它说明：
 *   - 此函数专用于信号量操作，队列项大小必须为0
 *   - 不应从中断中释放互斥信号量（因为优先级继承对中断无意义）
 *   - 必须在中断优先级合法的中断上下文中调用（满足最大系统调用优先级要求）
 *   - 支持队列集（Queue Sets）功能，可唤醒队列集关联的等待任务
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
BaseType_t xQueueGiveFromISR( QueueHandle_t xQueue, BaseType_t * const pxHigherPriorityTaskWoken )
{
    BaseType_t xReturn;                              /* 函数返回值 */
    UBaseType_t uxSavedInterruptStatus;              /* 保存的中断状态，用于恢复中断掩码 */
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* 将队列句柄转换为队列结构体指针 */

    /* 断言检查：确保队列指针有效 */
    configASSERT( pxQueue );

    /* 断言检查：确保队列项大小为0（此函数专用于信号量） */
    configASSERT( pxQueue->uxItemSize == 0 );

    /* 断言检查：互斥信号量不应从中断释放（特别是存在互斥持有者时） */
    configASSERT( !( ( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX ) && ( pxQueue->pxMutexHolder != NULL ) ) );

    /* 中断优先级验证：确保当前中断优先级允许调用FreeRTOS API函数 */
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    /* 保存当前中断状态并设置中断掩码，进入临界区 */
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        /* 获取当前队列中等待的消息数量 */
        const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting;

        /* 检查队列是否有空间（对于信号量，即检查是否未达到最大计数值） */
        if( uxMessagesWaiting < pxQueue->uxLength )
        {
            /* 获取队列的发送锁定状态 */
            const int8_t cTxLock = pxQueue->cTxLock;

            /* 跟踪队列发送事件（用于调试和分析） */
            traceQUEUE_SEND_FROM_ISR( pxQueue );

            /* 增加队列中的消息计数（即释放信号量） */
            pxQueue->uxMessagesWaiting = uxMessagesWaiting + 1;

            /* 检查队列是否未锁定 */
            if( cTxLock == queueUNLOCKED )
            {
                #if ( configUSE_QUEUE_SETS == 1 )  /* 如果启用了队列集功能 */
                {
                    /* 检查此队列是否属于某个队列集 */
                    if( pxQueue->pxQueueSetContainer != NULL )
                    {
                        /* 通知队列集容器有消息可用 */
                        if( prvNotifyQueueSetContainer( pxQueue, queueSEND_TO_BACK ) != pdFALSE )
                        {
                            /* 如果通知导致更高优先级任务解除阻塞，设置上下文切换标志 */
                            if( pxHigherPriorityTaskWoken != NULL )
                            {
                                *pxHigherPriorityTaskWoken = pdTRUE;
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                        }
                    }
                    else  /* 队列不属于任何队列集 */
                    {
                        /* 检查是否有任务正在等待接收此队列的消息 */
                        if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                        {
                            /* 从等待接收列表中移除任务并解除阻塞 */
                            if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                            {
                                /* 如果解除阻塞的任务优先级更高，设置上下文切换标志 */
                                if( pxHigherPriorityTaskWoken != NULL )
                                {
                                    *pxHigherPriorityTaskWoken = pdTRUE;
                                }
                                else
                                {
                                    mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                                }
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                        }
                    }
                }
                #else  /* 未启用队列集功能 */
                {
                    /* 检查是否有任务正在等待接收此队列的消息 */
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                    {
                        /* 从等待接收列表中移除任务并解除阻塞 */
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                        {
                            /* 如果解除阻塞的任务优先级更高，设置上下文切换标志 */
                            if( pxHigherPriorityTaskWoken != NULL )
                            {
                                *pxHigherPriorityTaskWoken = pdTRUE;
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                        }
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                    }
                }
                #endif  /* configUSE_QUEUE_SETS */
            }
            else  /* 队列处于锁定状态 */
            {
                /* 增加发送锁定计数，解锁时会处理挂起的操作 */
                pxQueue->cTxLock = ( int8_t ) ( cTxLock + 1 );
            }

            /* 设置返回值为成功 */
            xReturn = pdPASS;
        }
        else  /* 队列已满（信号量计数已达最大值） */
        {
            /* 跟踪队列发送失败事件 */
            traceQUEUE_SEND_FROM_ISR_FAILED( pxQueue );
            /* 设置返回值为队列已满错误 */
            xReturn = errQUEUE_FULL;
        }
    }
    /* 恢复中断状态，退出临界区 */
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

    /* 返回操作结果 */
    return xReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xQueueGenericReceive
 * 功能描述：从队列中接收数据项（支持普通接收和查看模式）
 *           此函数是队列接收操作的通用实现，支持阻塞和非阻塞模式
 *           可以配置为移除数据项（普通接收）或仅查看数据项而不移除（peek模式）
 * 输入参数：
 *   - xQueue: 队列句柄，指定要操作的队列
 *   - pvBuffer: 数据缓冲区指针，用于存储接收到的数据
 *   - xTicksToWait: 最大阻塞时间（以时钟节拍为单位）
 *   - xJustPeeking: 模式选择标志，pdTRUE表示仅查看不移除，pdFALSE表示普通接收
 * 输出参数：
 *   - pvBuffer: 接收到的数据将存储到此缓冲区
 * 返 回 值：
 *   - BaseType_t: 成功接收到数据返回pdPASS，队列为空且超时返回errQUEUE_EMPTY
 * 其它说明：
 *   - 此函数是队列接收操作的核心实现，支持任务阻塞和超时机制
 *   - 对于互斥信号量（mutex）有特殊处理，支持优先级继承机制
 *   - 支持两种模式：普通接收（移除数据）和查看模式（不移除数据）
 *   - 在调度器挂起时不能使用非零阻塞时间
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
BaseType_t xQueueGenericReceive( QueueHandle_t xQueue, void * const pvBuffer, TickType_t xTicksToWait, const BaseType_t xJustPeeking )
{
    BaseType_t xEntryTimeSet = pdFALSE;          /* 超时结构初始化标志 */
    TimeOut_t xTimeOut;                          /* 超时状态结构 */
    int8_t *pcOriginalReadPosition;              /* 原始读取位置（用于peek模式） */
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* 将队列句柄转换为队列结构体指针 */

    /* 断言检查：确保队列指针有效 */
    configASSERT( pxQueue );
    /* 断言检查：对于有数据项的队列，缓冲区指针不能为NULL */
    configASSERT( !( ( pvBuffer == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );
    #if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
    {
        /* 断言检查：调度器挂起时不能使用非零阻塞时间 */
        configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
    }
    #endif

    /* 此函数放宽了编码标准，允许在函数内部使用return语句，这是为了执行时间效率 */

    /* 无限循环，直到成功接收数据或超时 */
    for( ;; )
    {
        /* 进入临界区保护队列操作 */
        taskENTER_CRITICAL();
        {
            /* 获取当前队列中等待的消息数量 */
            const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting;

            /* 检查队列中是否有数据可用（调用任务必须是想要访问队列的最高优先级任务） */
            if( uxMessagesWaiting > ( UBaseType_t ) 0 )
            {
                /* 保存原始读取位置，以便在peek模式下恢复 */
                pcOriginalReadPosition = pxQueue->u.pcReadFrom;

                /* 从队列中复制数据到提供的缓冲区 */
                prvCopyDataFromQueue( pxQueue, pvBuffer );

                /* 检查是否为普通接收模式（非peek模式） */
                if( xJustPeeking == pdFALSE )
                {
                    /* 跟踪队列接收事件 */
                    traceQUEUE_RECEIVE( pxQueue );

                    /* 实际移除数据：减少队列中的消息计数 */
                    pxQueue->uxMessagesWaiting = uxMessagesWaiting - 1;

                    #if ( configUSE_MUTEXES == 1 )  /* 如果启用了互斥信号量功能 */
                    {
                        /* 检查队列类型是否为互斥信号量 */
                        if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
                        {
                            /* 记录实现优先级继承所需的信息 */
                            pxQueue->pxMutexHolder = ( int8_t * ) pvTaskIncrementMutexHeldCount(); /*lint !e961 强制转换不是冗余的，因为TaskHandle_t是一个typedef */
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                        }
                    }
                    #endif /* configUSE_MUTEXES */

                    /* 检查是否有任务正在等待发送数据到此队列 */
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
                    {
                        /* 从等待发送列表中移除任务并解除阻塞 */
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                        {
                            /* 如果解除阻塞的任务优先级更高，触发任务切换 */
                            queueYIELD_IF_USING_PREEMPTION();
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                        }
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                    }
                }
                else  /* peek模式：仅查看数据而不移除 */
                {
                    /* 跟踪队列查看事件 */
                    traceQUEUE_PEEK( pxQueue );

                    /* 数据不被移除，因此重置读取指针到原始位置 */
                    pxQueue->u.pcReadFrom = pcOriginalReadPosition;

                    /* 数据保留在队列中，检查是否有其他任务在等待此数据 */
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                    {
                        /* 从等待接收列表中移除任务并解除阻塞 */
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                        {
                            /* 等待的任务优先级高于当前任务，触发任务切换 */
                            queueYIELD_IF_USING_PREEMPTION();
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                        }
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                    }
                }

                /* 退出临界区并返回成功 */
                taskEXIT_CRITICAL();
                return pdPASS;
            }
            else  /* 队列为空 */
            {
                /* 检查是否指定了阻塞时间 */
                if( xTicksToWait == ( TickType_t ) 0 )
                {
                    /* 队列为空且未指定阻塞时间（或阻塞时间已到期），立即返回 */
                    taskEXIT_CRITICAL();
                    traceQUEUE_RECEIVE_FAILED( pxQueue );
                    return errQUEUE_EMPTY;
                }
                else if( xEntryTimeSet == pdFALSE )
                {
                    /* 队列为空但指定了阻塞时间，配置超时结构 */
                    vTaskSetTimeOutState( &xTimeOut );
                    xEntryTimeSet = pdTRUE;
                }
                else
                {
                    /* 超时结构已设置，继续等待 */
                    mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                }
            }
        }
        /* 退出临界区，允许中断和其他任务访问队列 */
        taskEXIT_CRITICAL();

        /* 挂起所有任务，准备进入阻塞状态 */
        vTaskSuspendAll();
        /* 锁定队列以防止并发访问 */
        prvLockQueue( pxQueue );

        /* 检查超时状态，看是否已超时 */
        if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
        {
            /* 再次检查队列是否为空 */
            if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
            {
                /* 跟踪队列接收阻塞事件 */
                traceBLOCKING_ON_QUEUE_RECEIVE( pxQueue );

                #if ( configUSE_MUTEXES == 1 )  /* 如果启用了互斥信号量功能 */
                {
                    /* 检查队列类型是否为互斥信号量 */
                    if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
                    {
                        /* 进入临界区处理优先级继承 */
                        taskENTER_CRITICAL();
                        {
                            /* 为互斥信号量持有者实施优先级继承 */
                            vTaskPriorityInherit( ( void * ) pxQueue->pxMutexHolder );
                        }
                        taskEXIT_CRITICAL();
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                    }
                }
                #endif

                /* 将当前任务放置到等待接收事件列表中 */
                vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait );
                /* 解锁队列 */
                prvUnlockQueue( pxQueue );
                /* 恢复所有任务，检查是否需要任务切换 */
                if( xTaskResumeAll() == pdFALSE )
                {
                    /* 需要任务切换，执行端口相关的 yield 操作 */
                    portYIELD_WITHIN_API();
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                }
            }
            else
            {
                /* 队列不再为空，重试接收操作 */
                prvUnlockQueue( pxQueue );
                ( void ) xTaskResumeAll();
            }
        }
        else  /* 已超时 */
        {
            /* 解锁队列并恢复所有任务 */
            prvUnlockQueue( pxQueue );
            ( void ) xTaskResumeAll();

            /* 最终检查队列是否仍然为空 */
            if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
            {
                /* 跟踪队列接收失败事件 */
                traceQUEUE_RECEIVE_FAILED( pxQueue );
                return errQUEUE_EMPTY;
            }
            else
            {
                /* 队列不再为空，继续循环尝试接收 */
                mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
            }
        }
    }
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xQueueReceiveFromISR
 * 功能描述：从中断服务程序（ISR）中接收队列数据项
 *           此函数是中断安全的队列接收操作，专为中断上下文设计
 *           不会阻塞，立即返回接收结果，并支持唤醒更高优先级任务
 * 输入参数：
 *   - xQueue: 队列句柄，指定要操作的队列
 *   - pvBuffer: 数据缓冲区指针，用于存储接收到的数据
 * 输出参数：
 *   - pxHigherPriorityTaskWoken: 指向更高优先级任务唤醒标志的指针
 *   - pvBuffer: 接收到的数据将存储到此缓冲区
 * 返 回 值：
 *   - BaseType_t: 成功接收到数据返回pdPASS，队列为空返回pdFAIL
 * 其它说明：
 *   - 此函数专为中断上下文设计，不能在中断中阻塞
 *   - 必须在中断优先级合法的中断上下文中调用
 *   - 对于有数据项的队列，缓冲区指针不能为NULL
 *   - 支持队列锁定状态下的数据接收操作
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
BaseType_t xQueueReceiveFromISR( QueueHandle_t xQueue, void * const pvBuffer, BaseType_t * const pxHigherPriorityTaskWoken )
{
    BaseType_t xReturn;                              /* 函数返回值 */
    UBaseType_t uxSavedInterruptStatus;              /* 保存的中断状态，用于恢复中断掩码 */
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* 将队列句柄转换为队列结构体指针 */

    /* 断言检查：确保队列指针有效 */
    configASSERT( pxQueue );
    /* 断言检查：对于有数据项的队列，缓冲区指针不能为NULL */
    configASSERT( !( ( pvBuffer == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );

    /* 中断优先级验证：确保当前中断优先级允许调用FreeRTOS API函数 */
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    /* 保存当前中断状态并设置中断掩码，进入临界区 */
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        /* 获取当前队列中等待的消息数量 */
        const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting;

        /* 检查队列中是否有数据可用（中断中不能阻塞，所以必须立即检查） */
        if( uxMessagesWaiting > ( UBaseType_t ) 0 )
        {
            /* 获取队列的接收锁定状态 */
            const int8_t cRxLock = pxQueue->cRxLock;

            /* 跟踪队列接收事件（用于调试和分析） */
            traceQUEUE_RECEIVE_FROM_ISR( pxQueue );

            /* 从队列中复制数据到提供的缓冲区 */
            prvCopyDataFromQueue( pxQueue, pvBuffer );
            /* 减少队列中的消息计数 */
            pxQueue->uxMessagesWaiting = uxMessagesWaiting - 1;

            /* 检查队列是否未锁定 */
            if( cRxLock == queueUNLOCKED )
            {
                /* 检查是否有任务正在等待发送数据到此队列 */
                if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
                {
                    /* 从等待发送列表中移除任务并解除阻塞 */
                    if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                    {
                        /* 如果解除阻塞的任务优先级更高，设置上下文切换标志 */
                        if( pxHigherPriorityTaskWoken != NULL )
                        {
                            *pxHigherPriorityTaskWoken = pdTRUE;
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                        }
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                    }
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                }
            }
            else  /* 队列处于锁定状态 */
            {
                /* 增加接收锁定计数，解锁时会处理挂起的操作 */
                pxQueue->cRxLock = ( int8_t ) ( cRxLock + 1 );
            }

            /* 设置返回值为成功 */
            xReturn = pdPASS;
        }
        else  /* 队列为空 */
        {
            /* 设置返回值为失败 */
            xReturn = pdFAIL;
            /* 跟踪队列接收失败事件 */
            traceQUEUE_RECEIVE_FROM_ISR_FAILED( pxQueue );
        }
    }
    /* 恢复中断状态，退出临界区 */
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

    /* 返回操作结果 */
    return xReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xQueuePeekFromISR
 * 功能描述：从中断服务程序（ISR）中查看队列头部的数据项而不移除
 *           此函数是中断安全的队列查看操作，专为中断上下文设计
 *           不会阻塞，立即返回查看结果，且不会修改队列状态
 * 输入参数：
 *   - xQueue: 队列句柄，指定要操作的队列
 * 输出参数：
 *   - pvBuffer: 数据缓冲区指针，用于存储查看到的数据
 * 返 回 值：
 *   - BaseType_t: 成功查看数据返回pdPASS，队列为空返回pdFAIL
 * 其它说明：
 *   - 此函数专为中断上下文设计，不能在中断中阻塞
 *   - 必须在中断优先级合法的中断上下文中调用
 *   - 不能用于查看信号量（信号量项大小为0）
 *   - 对于有数据项的队列，缓冲区指针不能为NULL
 *   - 查看操作不会改变队列状态（消息计数和读取位置保持不变）
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
BaseType_t xQueuePeekFromISR( QueueHandle_t xQueue, void * const pvBuffer )
{
    BaseType_t xReturn;                              /* 函数返回值 */
    UBaseType_t uxSavedInterruptStatus;              /* 保存的中断状态，用于恢复中断掩码 */
    int8_t *pcOriginalReadPosition;                  /* 原始读取位置，用于查看后恢复 */
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* 将队列句柄转换为队列结构体指针 */

    /* 断言检查：确保队列指针有效 */
    configASSERT( pxQueue );
    /* 断言检查：对于有数据项的队列，缓冲区指针不能为NULL */
    configASSERT( !( ( pvBuffer == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );
    /* 断言检查：不能查看信号量（信号量项大小为0） */
    configASSERT( pxQueue->uxItemSize != 0 );

    /* 中断优先级验证：确保当前中断优先级允许调用FreeRTOS API函数 */
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    /* 保存当前中断状态并设置中断掩码，进入临界区 */
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        /* 检查队列中是否有数据可用（中断中不能阻塞，所以必须立即检查） */
        if( pxQueue->uxMessagesWaiting > ( UBaseType_t ) 0 )
        {
            /* 跟踪队列查看事件（用于调试和分析） */
            traceQUEUE_PEEK_FROM_ISR( pxQueue );

            /* 保存原始读取位置，以便查看后恢复（因为实际上不移除数据） */
            pcOriginalReadPosition = pxQueue->u.pcReadFrom;
            
            /* 从队列中复制数据到提供的缓冲区 */
            prvCopyDataFromQueue( pxQueue, pvBuffer );
            
            /* 恢复读取位置（因为只是查看，不实际移除数据） */
            pxQueue->u.pcReadFrom = pcOriginalReadPosition;

            /* 设置返回值为成功 */
            xReturn = pdPASS;
        }
        else  /* 队列为空 */
        {
            /* 设置返回值为失败 */
            xReturn = pdFAIL;
            /* 跟踪队列查看失败事件 */
            traceQUEUE_PEEK_FROM_ISR_FAILED( pxQueue );
        }
    }
    /* 恢复中断状态，退出临界区 */
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

    /* 返回操作结果 */
    return xReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：uxQueueSpacesAvailable
 * 功能描述：获取队列中当前可用的空闲空间数量
 *           此函数用于查询队列中还可以容纳的消息项数量
 * 输入参数：
 *   - xQueue: 队列句柄，指定要查询的队列
 * 返 回 值：
 *   - UBaseType_t: 队列中当前可用的空闲空间数量
 * 其它说明：
 *   - 此函数在临界区内执行，确保计算的原子性
 *   - 适用于任务上下文，不能在中段服务程序中使用
 *   - 返回值 = 队列总长度 - 当前消息数量
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
UBaseType_t uxQueueSpacesAvailable( const QueueHandle_t xQueue )
{
    UBaseType_t uxReturn;  /* 返回值，存储队列中的空闲空间数量 */
    Queue_t *pxQueue;      /* 队列结构体指针 */

    /* 将队列句柄转换为队列结构体指针 */
    pxQueue = ( Queue_t * ) xQueue;
    /* 断言检查：确保队列指针有效 */
    configASSERT( pxQueue );

    /* 进入临界区保护队列操作 */
    taskENTER_CRITICAL();
    {
        /* 计算队列中的空闲空间数量：总长度减去当前消息数量 */
        uxReturn = pxQueue->uxLength - pxQueue->uxMessagesWaiting;
    }
    /* 退出临界区 */
    taskEXIT_CRITICAL();

    /* 返回队列中的空闲空间数量 */
    return uxReturn;
} /*lint !e818 指针不能声明为const，因为xQueue是typedef而不是指针 */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：uxQueueMessagesWaitingFromISR
 * 功能描述：从中断服务程序获取队列中当前等待的消息数量
 *           此函数是中断安全版本，用于在ISR中查询队列中的消息项数量
 * 输入参数：
 *   - xQueue: 队列句柄，指定要查询的队列
 * 返 回 值：
 *   - UBaseType_t: 队列中当前等待的消息数量
 * 其它说明：
 *   - 此函数专为中断上下文设计，不进入临界区
 *   - 适用于中断服务程序，不能在任务上下文中使用
 *   - 由于在ISR中调用，不需要临界区保护
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
UBaseType_t uxQueueMessagesWaitingFromISR( const QueueHandle_t xQueue )
{
    UBaseType_t uxReturn;  /* 返回值，存储队列中的消息数量 */

    /* 断言检查：确保队列句柄有效 */
    configASSERT( xQueue );

    /* 直接从队列结构体中获取当前等待的消息数量（不需要临界区保护） */
    uxReturn = ( ( Queue_t * ) xQueue )->uxMessagesWaiting;

    /* 返回队列中的消息数量 */
    return uxReturn;
} /*lint !e818 指针不能声明为const，因为xQueue是typedef而不是指针 */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vQueueDelete
 * 功能描述：删除队列并释放相关资源
 *           此函数用于完全删除队列并释放其占用的内存资源
 * 输入参数：
 *   - xQueue: 队列句柄，指定要删除的队列
 * 其它说明：
 *   - 此函数会根据队列的分配方式（动态或静态）决定如何释放资源
 *   - 对于动态分配的队列，会释放其内存
 *   - 对于静态分配的队列，不会释放内存（因为内存不是由内核分配的）
 *   - 如果启用了队列注册表功能，会先从注册表中注销队列
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void vQueueDelete( QueueHandle_t xQueue )
{
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* 将队列句柄转换为队列结构体指针 */

    /* 断言检查：确保队列指针有效 */
    configASSERT( pxQueue );
    /* 跟踪队列删除事件（用于调试和分析） */
    traceQUEUE_DELETE( pxQueue );

    /* 如果启用了队列注册表功能 */
    #if ( configQUEUE_REGISTRY_SIZE > 0 )
    {
        /* 从队列注册表中注销该队列 */
        vQueueUnregisterQueue( pxQueue );
    }
    #endif

    /* 根据内存分配配置处理队列删除 */
    #if( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 0 ) )
    {
        /* 队列只能是动态分配的 - 释放其内存 */
        vPortFree( pxQueue );
    }
    #elif( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
    {
        /* 队列可能是静态或动态分配的，需要检查后再决定是否释放内存 */
        if( pxQueue->ucStaticallyAllocated == ( uint8_t ) pdFALSE )
        {
            /* 动态分配的队列，释放其内存 */
            vPortFree( pxQueue );
        }
        else
        {
            /* 静态分配的队列，不释放内存 */
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
        }
    }
    #else
    {
        /* 队列必须是静态分配的，因此不会被删除。避免编译器关于未使用参数的警告。 */
        ( void ) pxQueue;
    }
    #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
}
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )  /* 如果启用了跟踪功能 */

/*******************************************************************************
 * 函数名称：uxQueueGetQueueNumber
 * 功能描述：获取队列的编号
 *           此函数用于获取分配给队列的唯一编号
 * 输入参数：
 *   - xQueue: 队列句柄，指定要查询的队列
 * 返 回 值：
 *   - UBaseType_t: 队列的唯一编号
 * 其它说明：
 *   - 此功能仅在启用configUSE_TRACE_FACILITY时可用
 *   - 队列编号用于调试和跟踪目的
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
UBaseType_t uxQueueGetQueueNumber( QueueHandle_t xQueue )
{
    /* 从队列结构体中获取队列编号 */
    return ( ( Queue_t * ) xQueue )->uxQueueNumber;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )  /* 如果启用了跟踪功能 */

/*******************************************************************************
 * 函数名称：vQueueSetQueueNumber
 * 功能描述：设置队列的编号
 *           此函数用于为队列分配一个唯一编号
 * 输入参数：
 *   - xQueue: 队列句柄，指定要设置的队列
 *   - uxQueueNumber: 要分配给队列的编号
 * 其它说明：
 *   - 此功能仅在启用configUSE_TRACE_FACILITY时可用
 *   - 队列编号用于调试和跟踪目的
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void vQueueSetQueueNumber( QueueHandle_t xQueue, UBaseType_t uxQueueNumber )
{
    /* 设置队列结构体中的队列编号字段 */
    ( ( Queue_t * ) xQueue )->uxQueueNumber = uxQueueNumber;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )  /* 如果启用了跟踪功能 */

/*******************************************************************************
 * 函数名称：ucQueueGetQueueType
 * 功能描述：获取队列的类型
 *           此函数用于获取队列的类型标识
 * 输入参数：
 *   - xQueue: 队列句柄，指定要查询的队列
 * 返 回 值：
 *   - uint8_t: 队列的类型标识
 * 其它说明：
 *   - 此功能仅在启用configUSE_TRACE_FACILITY时可用
 *   - 队列类型用于区分普通队列、互斥量、信号量等
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
uint8_t ucQueueGetQueueType( QueueHandle_t xQueue )
{
    /* 从队列结构体中获取队列类型 */
    return ( ( Queue_t * ) xQueue )->ucQueueType;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvCopyDataToQueue
 * 功能描述：将数据复制到队列中的内部实现函数
 *           此函数处理数据写入队列的核心逻辑，包括不同类型队列的特殊处理
 * 输入参数：
 *   - pxQueue: 队列结构体指针，指定要操作的目标队列
 *   - pvItemToQueue: 要写入队列的数据指针
 *   - xPosition: 写入位置标识，指定数据写入队列的位置
 * 返 回 值：
 *   - BaseType_t: 对于互斥量队列，返回是否需要任务调度；其他情况返回pdFALSE
 * 其它说明：
 *   - 此函数在临界区内调用，确保操作的原子性
 *   - 处理三种队列类型：普通队列、互斥量和信号量
 *   - 支持三种写入位置：队尾、队首和覆盖写入
 *   - 对于互斥量队列，处理优先级继承解除逻辑
 *   - 对于普通队列，处理环形缓冲区指针回绕
 *   - 对于覆盖写入，特殊处理消息计数
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
static BaseType_t prvCopyDataToQueue( Queue_t * const pxQueue, const void *pvItemToQueue, const BaseType_t xPosition )
{
    BaseType_t xReturn = pdFALSE;          /* 返回值，初始化为不需要调度 */
    UBaseType_t uxMessagesWaiting;         /* 当前队列中的消息数量 */

    /* 此函数在临界区内调用 */

    /* 获取当前队列中的消息数量 */
    uxMessagesWaiting = pxQueue->uxMessagesWaiting;

    /* 检查队列项大小是否为0（信号量或互斥量） */
    if( pxQueue->uxItemSize == ( UBaseType_t ) 0 )
    {
        #if ( configUSE_MUTEXES == 1 )  /* 如果启用了互斥量功能 */
        {
            /* 检查队列类型是否为互斥量 */
            if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
            {
                /* 互斥量不再被持有，解除优先级继承 */
                xReturn = xTaskPriorityDisinherit( ( void * ) pxQueue->pxMutexHolder );
                /* 清空互斥量持有者指针 */
                pxQueue->pxMutexHolder = NULL;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
            }
        }
        #endif /* configUSE_MUTEXES */
    }
    /* 检查是否为向队尾添加数据 */
    else if( xPosition == queueSEND_TO_BACK )
    {
        /* 将数据复制到队列的写入位置 */
        ( void ) memcpy( ( void * ) pxQueue->pcWriteTo, pvItemToQueue, ( size_t ) pxQueue->uxItemSize ); /*lint !e961 !e418 MISRA异常，因为转换仅对某些端口冗余，加上先前的逻辑确保只有在复制大小为0时才能将空指针传递给memcpy() */

        /* 更新写入指针到下一个位置 */
        pxQueue->pcWriteTo += pxQueue->uxItemSize;
        
        /* 检查写入指针是否超出队列尾部，需要回绕到队列头部 */
        if( pxQueue->pcWriteTo >= pxQueue->pcTail ) /*lint !e946 MISRA异常，指针比较是最清晰的解决方案 */
        {
            pxQueue->pcWriteTo = pxQueue->pcHead;
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
        }
    }
    else  /* 向队首添加数据或覆盖写入 */
    {
        /* 将数据复制到队列的读取位置（对于队首添加） */
        ( void ) memcpy( ( void * ) pxQueue->u.pcReadFrom, pvItemToQueue, ( size_t ) pxQueue->uxItemSize ); /*lint !e961 MISRA异常，因为转换仅对某些端口冗余 */

        /* 更新读取指针到前一个位置（对于队首添加） */
        pxQueue->u.pcReadFrom -= pxQueue->uxItemSize;
        
        /* 检查读取指针是否超出队列头部，需要回绕到队列尾部 */
        if( pxQueue->u.pcReadFrom < pxQueue->pcHead ) /*lint !e946 MISRA异常，指针比较是最清晰的解决方案 */
        {
            pxQueue->u.pcReadFrom = ( pxQueue->pcTail - pxQueue->uxItemSize );
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
        }

        /* 检查是否为覆盖写入模式 */
        if( xPosition == queueOVERWRITE )
        {
            /* 检查队列中是否有消息 */
            if( uxMessagesWaiting > ( UBaseType_t ) 0 )
            {
                /* 不是添加新项而是覆盖现有项，因此从记录的消息数量中减去1，
                这样在下面再次加1时，记录的消息数量保持正确 */
                --uxMessagesWaiting;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
        }
    }

    /* 更新队列中的消息数量（增加1） */
    pxQueue->uxMessagesWaiting = uxMessagesWaiting + 1;

    /* 返回是否需要任务调度的标志（仅对互斥量有意义） */
    return xReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvCopyDataFromQueue
 * 功能描述：从队列中复制数据到指定缓冲区的内部实现函数
 *           此函数处理从队列读取数据的核心逻辑，包括指针更新和数据复制
 * 输入参数：
 *   - pxQueue: 队列结构体指针，指定要操作的源队列
 *   - pvBuffer: 目标缓冲区指针，用于存储从队列读取的数据
 * 其它说明：
 *   - 此函数假设调用者已经确保队列中有数据（uxMessagesWaiting > 0）
 *   - 对于项大小为0的队列（信号量/互斥量），不执行任何操作
 *   - 会更新队列的读指针，处理环形缓冲区回绕
 *   - 必须在临界区内调用，确保操作的原子性
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
static void prvCopyDataFromQueue( Queue_t * const pxQueue, void * const pvBuffer )
{
    /* 检查队列项大小是否为0（信号量或互斥量） */
    if( pxQueue->uxItemSize != ( UBaseType_t ) 0 )
    {
        /* 更新读指针到下一个位置 */
        pxQueue->u.pcReadFrom += pxQueue->uxItemSize;
        
        /* 检查读指针是否超出队列尾部，需要回绕到队列头部 */
        if( pxQueue->u.pcReadFrom >= pxQueue->pcTail ) /*lint !e946 MISRA异常，使用关系运算符是最清晰的解决方案 */
        {
            pxQueue->u.pcReadFrom = pxQueue->pcHead;
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
        }
        
        /* 从更新后的读指针位置复制数据到目标缓冲区 */
        ( void ) memcpy( ( void * ) pvBuffer, ( void * ) pxQueue->u.pcReadFrom, ( size_t ) pxQueue->uxItemSize ); /*lint !e961 !e418 MISRA异常，因为转换仅对某些端口冗余。此外，先前的逻辑确保只有在计数为0时才能将空指针传递给memcpy() */
    }
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvUnlockQueue
 * 功能描述：解锁队列并处理在队列锁定期间累积的待处理操作
 *           此函数处理队列解锁的核心逻辑，包括处理锁定期间累积的发送和接收操作
 * 输入参数：
 *   - pxQueue: 队列结构体指针，指定要解锁的队列
 * 其它说明：
 *   - 此函数必须在调度器挂起的情况下调用
 *   - 处理锁定计数中包含的在队列锁定期间添加或移除的额外数据项
 *   - 当队列被锁定时，可以添加或移除项，但不能更新事件列表
 *   - 解锁时会处理所有挂起的任务唤醒操作
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
static void prvUnlockQueue( Queue_t * const pxQueue )
{
    /* 此函数必须在调度器挂起的情况下调用 */

    /* 锁定计数包含在队列被锁定期间添加或移除的额外数据项的数量。
       当队列被锁定时，可以添加或移除项，但事件列表不能更新。 */

    /* 处理发送锁定（TxLock） */
    taskENTER_CRITICAL();
    {
        /* 获取当前的发送锁定计数 */
        int8_t cTxLock = pxQueue->cTxLock;

        /* 检查在队列锁定期间是否有数据被添加到队列 */
        while( cTxLock > queueLOCKED_UNMODIFIED )
        {
            /* 在队列锁定期间有数据被发布。是否有任何任务阻塞等待数据可用？ */
            #if ( configUSE_QUEUE_SETS == 1 )  /* 如果启用了队列集功能 */
            {
                /* 检查队列是否属于某个队列集 */
                if( pxQueue->pxQueueSetContainer != NULL )
                {
                    /* 通知队列集容器有数据可用 */
                    if( prvNotifyQueueSetContainer( pxQueue, queueSEND_TO_BACK ) != pdFALSE )
                    {
                        /* 队列是队列集的成员，并且发布到队列集导致更高优先级的任务解除阻塞。
                           需要上下文切换。 */
                        vTaskMissedYield();
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                    }
                }
                else  /* 队列不属于任何队列集 */
                {
                    /* 从事件列表中移除的任务将被添加到挂起就绪列表，因为调度器仍然挂起。 */
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                    {
                        /* 从等待接收列表中移除任务并解除阻塞 */
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                        {
                            /* 等待的任务具有更高的优先级，因此记录需要上下文切换。 */
                            vTaskMissedYield();
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                        }
                    }
                    else
                    {
                        /* 没有任务等待接收，退出循环 */
                        break;
                    }
                }
            }
            #else /* configUSE_QUEUE_SETS */  /* 未启用队列集功能 */
            {
                /* 从事件列表中移除的任务将被添加到挂起就绪列表，因为调度器仍然挂起。 */
                if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                {
                    /* 从等待接收列表中移除任务并解除阻塞 */
                    if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                    {
                        /* 等待的任务具有更高的优先级，因此记录需要上下文切换。 */
                        vTaskMissedYield();
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                    }
                }
                else
                {
                    /* 没有任务等待接收，退出循环 */
                    break;
                }
            }
            #endif /* configUSE_QUEUE_SETS */

            /* 递减发送锁定计数 */
            --cTxLock;
        }

        /* 将发送锁定设置为未锁定状态 */
        pxQueue->cTxLock = queueUNLOCKED;
    }
    taskEXIT_CRITICAL();

    /* 对接收锁定（RxLock）执行相同的处理 */
    taskENTER_CRITICAL();
    {
        /* 获取当前的接收锁定计数 */
        int8_t cRxLock = pxQueue->cRxLock;

        /* 检查在队列锁定期间是否有数据被移除 */
        while( cRxLock > queueLOCKED_UNMODIFIED )
        {
            /* 检查是否有任务正在等待发送数据 */
            if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
            {
                /* 从等待发送列表中移除任务并解除阻塞 */
                if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                {
                    /* 记录需要上下文切换 */
                    vTaskMissedYield();
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                }

                /* 递减接收锁定计数 */
                --cRxLock;
            }
            else
            {
                /* 没有任务等待发送，退出循环 */
                break;
            }
        }

        /* 将接收锁定设置为未锁定状态 */
        pxQueue->cRxLock = queueUNLOCKED;
    }
    taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvIsQueueEmpty
 * 功能描述：检查队列是否为空的内部实现函数
 *           此函数原子性地检查队列中的消息数量是否为0
 * 输入参数：
 *   - pxQueue: 队列结构体指针，指定要检查的队列
 * 返 回 值：
 *   - BaseType_t: 如果队列为空返回pdTRUE，否则返回pdFALSE
 * 其它说明：
 *   - 此函数在临界区内执行，确保检查操作的原子性
 *   - 通过检查uxMessagesWaiting字段判断队列是否为空
 *   - 适用于需要原子性检查队列状态的场景
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
static BaseType_t prvIsQueueEmpty( const Queue_t *pxQueue )
{
    BaseType_t xReturn;  /* 返回值，表示队列是否为空 */

    /* 进入临界区保护队列操作 */
    taskENTER_CRITICAL();
    {
        /* 检查队列中的消息数量是否为0 */
        if( pxQueue->uxMessagesWaiting == ( UBaseType_t ) 0 )
        {
            /* 队列为空，设置返回值为真 */
            xReturn = pdTRUE;
        }
        else
        {
            /* 队列不为空，设置返回值为假 */
            xReturn = pdFALSE;
        }
    }
    /* 退出临界区 */
    taskEXIT_CRITICAL();

    /* 返回队列是否为空的结果 */
    return xReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xQueueIsQueueEmptyFromISR
 * 功能描述：从中断服务程序（ISR）中检查队列是否为空
 *           此函数是中断安全版本，用于在ISR中检查队列中的消息数量是否为0
 * 输入参数：
 *   - xQueue: 队列句柄，指定要检查的队列
 * 返 回 值：
 *   - BaseType_t: 如果队列为空返回pdTRUE，否则返回pdFALSE
 * 其它说明：
 *   - 此函数专为中断上下文设计，不进入临界区
 *   - 适用于中断服务程序，不能在任务上下文中使用
 *   - 由于在ISR中调用，不需要临界区保护
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
BaseType_t xQueueIsQueueEmptyFromISR( const QueueHandle_t xQueue )
{
    BaseType_t xReturn;  /* 返回值，表示队列是否为空 */

    /* 断言检查：确保队列句柄有效 */
    configASSERT( xQueue );
    
    /* 直接检查队列中的消息数量是否为0（不需要临界区保护） */
    if( ( ( Queue_t * ) xQueue )->uxMessagesWaiting == ( UBaseType_t ) 0 )
    {
        /* 队列为空，设置返回值为真 */
        xReturn = pdTRUE;
    }
    else
    {
        /* 队列不为空，设置返回值为假 */
        xReturn = pdFALSE;
    }

    /* 返回队列是否为空的结果 */
    return xReturn;
} /*lint !e818 xQueue不能声明为指向const的指针，因为它是一个typedef */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvIsQueueFull
 * 功能描述：检查队列是否已满的内部实现函数
 *           此函数原子性地检查队列中的消息数量是否等于队列长度
 * 输入参数：
 *   - pxQueue: 队列结构体指针，指定要检查的队列
 * 返 回 值：
 *   - BaseType_t: 如果队列已满返回pdTRUE，否则返回pdFALSE
 * 其它说明：
 *   - 此函数在临界区内执行，确保检查操作的原子性
 *   - 通过比较uxMessagesWaiting和uxLength字段判断队列是否已满
 *   - 适用于需要原子性检查队列状态的场景
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
static BaseType_t prvIsQueueFull( const Queue_t *pxQueue )
{
    BaseType_t xReturn;  /* 返回值，表示队列是否已满 */

    /* 进入临界区保护队列操作 */
    taskENTER_CRITICAL();
    {
        /* 检查队列中的消息数量是否等于队列长度 */
        if( pxQueue->uxMessagesWaiting == pxQueue->uxLength )
        {
            /* 队列已满，设置返回值为真 */
            xReturn = pdTRUE;
        }
        else
        {
            /* 队列未满，设置返回值为假 */
            xReturn = pdFALSE;
        }
    }
    /* 退出临界区 */
    taskEXIT_CRITICAL();

    /* 返回队列是否已满的结果 */
    return xReturn;
}
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )  /* 如果启用了协程功能 */

/*******************************************************************************
 * 函数名称：xQueueCRSend
 * 功能描述：从协程向队列发送数据的函数
 *           此函数是协程专用的队列发送操作，支持阻塞等待
 * 输入参数：
 *   - xQueue: 队列句柄，指定要发送数据的目标队列
 *   - pvItemToQueue: 要发送的数据指针
 *   - xTicksToWait: 最大阻塞时间（以时钟节拍为单位）
 * 返 回 值：
 *   - BaseType_t: 发送结果，可能的值包括pdPASS、errQUEUE_BLOCKED、errQUEUE_FULL、errQUEUE_YIELD
 * 其它说明：
 *   - 此函数专为协程设计，不能在任务中使用
 *   - 如果队列已满且指定了阻塞时间，协程会被添加到延迟列表
 *   - 支持唤醒等待接收数据的协程
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
BaseType_t xQueueCRSend( QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait )
{
    BaseType_t xReturn;                              /* 函数返回值 */
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* 将队列句柄转换为队列结构体指针 */

    /* 如果队列已满，我们可能需要阻塞。需要临界区来防止在检查队列是否已满和
       在队列上阻塞之间，中断从队列中移除某些内容。 */
    portDISABLE_INTERRUPTS();
    {
        /* 检查队列是否已满 */
        if( prvIsQueueFull( pxQueue ) != pdFALSE )
        {
            /* 队列已满 - 我们是想要阻塞还是只是离开而不发布？ */
            if( xTicksToWait > ( TickType_t ) 0 )
            {
                /* 由于这是从协程调用的，我们不能直接阻塞，而是返回指示我们需要阻塞。 */
                vCoRoutineAddToDelayedList( xTicksToWait, &( pxQueue->xTasksWaitingToSend ) );
                portENABLE_INTERRUPTS();
                return errQUEUE_BLOCKED;
            }
            else
            {
                portENABLE_INTERRUPTS();
                return errQUEUE_FULL;
            }
        }
    }
    portENABLE_INTERRUPTS();

    portDISABLE_INTERRUPTS();
    {
        /* 检查队列是否有空间 */
        if( pxQueue->uxMessagesWaiting < pxQueue->uxLength )
        {
            /* 队列中有空间，将数据复制到队列中。 */
            prvCopyDataToQueue( pxQueue, pvItemToQueue, queueSEND_TO_BACK );
            xReturn = pdPASS;

            /* 是否有任何协程在等待数据可用？ */
            if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
            {
                /* 在这种情况下，协程可以直接放入就绪列表，因为我们在临界区内。
                   相反，使用与从中断引起事件相同的挂起就绪列表机制。 */
                if( xCoRoutineRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                {
                    /* 等待的协程具有更高的优先级，因此记录可能需要让步。 */
                    xReturn = errQUEUE_YIELD;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
            }
        }
        else
        {
            xReturn = errQUEUE_FULL;
        }
    }
    portENABLE_INTERRUPTS();

    return xReturn;
}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )  /* 如果启用了协程功能 */

/*******************************************************************************
 * 函数名称：xQueueCRReceive
 * 功能描述：从协程接收队列数据的函数
 *           此函数是协程专用的队列接收操作，支持阻塞等待
 * 输入参数：
 *   - xQueue: 队列句柄，指定要接收数据的源队列
 *   - pvBuffer: 数据缓冲区指针，用于存储接收到的数据
 *   - xTicksToWait: 最大阻塞时间（以时钟节拍为单位）
 * 返 回 值：
 *   - BaseType_t: 接收结果，可能的值包括pdPASS、errQUEUE_BLOCKED、errQUEUE_FULL、errQUEUE_YIELD
 * 其它说明：
 *   - 此函数专为协程设计，不能在任务中使用
 *   - 如果队列为空且指定了阻塞时间，协程会被添加到延迟列表
 *   - 支持唤醒等待发送数据的协程
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
BaseType_t xQueueCRReceive( QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait )
{
    BaseType_t xReturn;                              /* 函数返回值 */
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* 将队列句柄转换为队列结构体指针 */

    /* 如果队列已经为空，我们可能需要阻塞。需要临界区来防止在检查队列是否为空和
       在队列上阻塞之间，中断向队列添加某些内容。 */
    portDISABLE_INTERRUPTS();
    {
        /* 检查队列是否为空 */
        if( pxQueue->uxMessagesWaiting == ( UBaseType_t ) 0 )
        {
            /* 队列中没有消息，我们是想要阻塞还是只是离开而不获取任何内容？ */
            if( xTicksToWait > ( TickType_t ) 0 )
            {
                /* 由于这是一个协程，我们不能直接阻塞，而是返回指示我们需要阻塞。 */
                vCoRoutineAddToDelayedList( xTicksToWait, &( pxQueue->xTasksWaitingToReceive ) );
                portENABLE_INTERRUPTS();
                return errQUEUE_BLOCKED;
            }
            else
            {
                portENABLE_INTERRUPTS();
                return errQUEUE_EMPTY;
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
        }
    }
    portENABLE_INTERRUPTS();

    portDISABLE_INTERRUPTS();
    {
        /* 检查队列中是否有数据 */
        if( pxQueue->uxMessagesWaiting > ( UBaseType_t ) 0 )
        {
            /* 可以从队列中获取数据。 */
            pxQueue->u.pcReadFrom += pxQueue->uxItemSize;
            if( pxQueue->u.pcReadFrom >= pxQueue->pcTail )
            {
                pxQueue->u.pcReadFrom = pxQueue->pcHead;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
            }
            --( pxQueue->uxMessagesWaiting );
            ( void ) memcpy( ( void * ) pvBuffer, ( void * ) pxQueue->u.pcReadFrom, ( unsigned ) pxQueue->uxItemSize );

            xReturn = pdPASS;

            /* 是否有任何协程在等待空间可用？ */
            if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
            {
                /* 在这种情况下，协程可以直接放入就绪列表，因为我们在临界区内。
                   相反，使用与从中断引起事件相同的挂起就绪列表机制。 */
                if( xCoRoutineRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                {
                    xReturn = errQUEUE_YIELD;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
            }
        }
        else
        {
            xReturn = pdFAIL;
        }
    }
    portENABLE_INTERRUPTS();

    return xReturn;
}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )  /* 如果启用了协程功能 */

/*******************************************************************************
 * 函数名称：xQueueCRSendFromISR
 * 功能描述：从中断服务程序向队列发送数据的协程版本
 *           此函数是中断安全的协程队列发送操作
 * 输入参数：
 *   - xQueue: 队列句柄，指定要发送数据的目标队列
 *   - pvItemToQueue: 要发送的数据指针
 *   - xCoRoutinePreviouslyWoken: 指示是否已经有协程被唤醒的标志
 * 返 回 值：
 *   - BaseType_t: 如果唤醒了协程返回pdTRUE，否则返回xCoRoutinePreviouslyWoken
 * 其它说明：
 *   - 此函数专为中断上下文设计，不能在任务中使用
 *   - 如果队列已满，不会执行任何操作
 *   - 每次ISR只唤醒一个协程
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
BaseType_t xQueueCRSendFromISR( QueueHandle_t xQueue, const void *pvItemToQueue, BaseType_t xCoRoutinePreviouslyWoken )
{
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* 将队列句柄转换为队列结构体指针 */

    /* 不能在ISR中阻塞，因此如果队列上没有空间，则退出而不执行任何操作。 */
    if( pxQueue->uxMessagesWaiting < pxQueue->uxLength )
    {
        /* 将数据复制到队列中 */
        prvCopyDataToQueue( pxQueue, pvItemToQueue, queueSEND_TO_BACK );

        /* 我们每次ISR只想唤醒一个协程，因此检查是否已经有协程被唤醒。 */
        if( xCoRoutinePreviouslyWoken == pdFALSE )
        {
            /* 检查是否有协程在等待接收数据 */
            if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
            {
                /* 从事件列表中移除协程并解除阻塞 */
                if( xCoRoutineRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                {
                    return pdTRUE;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
        }
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
    }

    return xCoRoutinePreviouslyWoken;
}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/
#if ( configUSE_CO_ROUTINES == 1 )  /* 如果启用了协程功能 */

/*******************************************************************************
 * 函数名称：xQueueCRReceiveFromISR
 * 功能描述：从中断服务程序接收队列数据的协程版本
 *           此函数是中断安全的协程队列接收操作
 * 输入参数：
 *   - xQueue: 队列句柄，指定要接收数据的源队列
 *   - pvBuffer: 数据缓冲区指针，用于存储接收到的数据
 *   - pxCoRoutineWoken: 指向协程唤醒标志的指针
 * 返 回 值：
 *   - BaseType_t: 成功接收到数据返回pdPASS，队列为空返回pdFAIL
 * 其它说明：
 *   - 此函数专为中断上下文设计，不能在任务中使用
 *   - 如果队列为空，不会执行任何操作
 *   - 支持唤醒等待发送数据的协程
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
BaseType_t xQueueCRReceiveFromISR( QueueHandle_t xQueue, void *pvBuffer, BaseType_t *pxCoRoutineWoken )
{
    BaseType_t xReturn;                              /* 函数返回值 */
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* 将队列句柄转换为队列结构体指针 */

    /* 我们不能从ISR阻塞，因此检查是否有数据可用。如果没有，则离开而不执行任何操作。 */
    if( pxQueue->uxMessagesWaiting > ( UBaseType_t ) 0 )
    {
        /* 从队列中复制数据。 */
        pxQueue->u.pcReadFrom += pxQueue->uxItemSize;
        if( pxQueue->u.pcReadFrom >= pxQueue->pcTail )
        {
            pxQueue->u.pcReadFrom = pxQueue->pcHead;
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
        }
        --( pxQueue->uxMessagesWaiting );
        ( void ) memcpy( ( void * ) pvBuffer, ( void * ) pxQueue->u.pcReadFrom, ( unsigned ) pxQueue->uxItemSize );

        /* 检查是否已经有协程被唤醒 */
        if( ( *pxCoRoutineWoken ) == pdFALSE )
        {
            /* 检查是否有协程在等待发送数据 */
            if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
            {
                /* 从事件列表中移除协程并解除阻塞 */
                if( xCoRoutineRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                {
                    *pxCoRoutineWoken = pdTRUE;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
        }

        xReturn = pdPASS;
    }
    else
    {
        xReturn = pdFAIL;
    }

    return xReturn;
}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )  /* 如果启用了队列注册表功能 */

/*******************************************************************************
 * 函数名称：vQueueAddToRegistry
 * 功能描述：将队列添加到队列注册表中
 *           此函数用于将队列与其名称关联，便于调试和识别
 * 输入参数：
 *   - xQueue: 队列句柄，指定要注册的队列
 *   - pcQueueName: 队列名称字符串
 * 其它说明：
 *   - 此功能仅在configQUEUE_REGISTRY_SIZE > 0时可用
 *   - 队列注册表用于调试目的，便于识别队列
 *   - 会遍历注册表查找空位，将队列信息存储在第一个空位中
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void vQueueAddToRegistry( QueueHandle_t xQueue, const char *pcQueueName ) /*lint !e971 允许未限定的char类型仅用于字符串和单个字符 */
{
    UBaseType_t ux;  /* 循环计数器 */

    /* 查看注册表中是否有空位。NULL名称表示空闲槽位。 */
    for( ux = ( UBaseType_t ) 0U; ux < ( UBaseType_t ) configQUEUE_REGISTRY_SIZE; ux++ )
    {
        if( xQueueRegistry[ ux ].pcQueueName == NULL )
        {
            /* 存储此队列的信息。 */
            xQueueRegistry[ ux ].pcQueueName = pcQueueName;
            xQueueRegistry[ ux ].xHandle = xQueue;

            /* 跟踪队列注册表添加事件 */
            traceQUEUE_REGISTRY_ADD( xQueue, pcQueueName );
            break;
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
        }
    }
}

#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )  /* 如果启用了队列注册表功能 */

/*******************************************************************************
 * 函数名称：pcQueueGetName
 * 功能描述：获取队列的名称
 *           此函数用于从队列注册表中查找队列的名称
 * 输入参数：
 *   - xQueue: 队列句柄，指定要查询的队列
 * 返 回 值：
 *   - const char *: 队列的名称，如果未找到则返回NULL
 * 其它说明：
 *   - 此功能仅在configQUEUE_REGISTRY_SIZE > 0时可用
 *   - 注意这里没有保护机制防止其他任务在搜索注册表时添加或删除条目
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
const char *pcQueueGetName( QueueHandle_t xQueue ) /*lint !e971 允许未限定的char类型仅用于字符串和单个字符 */
{
    UBaseType_t ux;                                  /* 循环计数器 */
    const char *pcReturn = NULL;                     /* 返回值，初始化为NULL */

    /* 注意：这里没有保护机制防止其他任务在搜索注册表时添加或删除条目。 */
    for( ux = ( UBaseType_t ) 0U; ux < ( UBaseType_t ) configQUEUE_REGISTRY_SIZE; ux++ )
    {
        if( xQueueRegistry[ ux ].xHandle == xQueue )
        {
            /* 找到匹配的队列句柄，返回对应的名称 */
            pcReturn = xQueueRegistry[ ux ].pcQueueName;
            break;
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
        }
    }

    return pcReturn;
}

#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )  /* 如果启用了队列注册表功能 */

/*******************************************************************************
 * 函数名称：vQueueUnregisterQueue
 * 功能描述：从队列注册表中注销队列
 *           此函数用于从队列注册表中移除队列的注册信息
 * 输入参数：
 *   - xQueue: 队列句柄，指定要注销的队列
 * 其它说明：
 *   - 此功能仅在configQUEUE_REGISTRY_SIZE > 0时可用
 *   - 会遍历注册表查找匹配的队列句柄，然后清空该槽位
 *   - 将名称设置为NULL表示槽位空闲，将句柄设置为NULL防止重复添加
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void vQueueUnregisterQueue( QueueHandle_t xQueue )
{
    UBaseType_t ux;  /* 循环计数器 */

    /* 查看正在注销的队列句柄是否实际在注册表中。 */
    for( ux = ( UBaseType_t ) 0U; ux < ( UBaseType_t ) configQUEUE_REGISTRY_SIZE; ux++ )
    {
        if( xQueueRegistry[ ux ].xHandle == xQueue )
        {
            /* 将名称设置为NULL以显示此槽位再次空闲。 */
            xQueueRegistry[ ux ].pcQueueName = NULL;

            /* 将句柄设置为NULL以确保相同的队列句柄不能两次出现在注册表中，
               如果它被添加、移除，然后再次添加。 */
            xQueueRegistry[ ux ].xHandle = ( QueueHandle_t ) 0;
            break;
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
        }
    }

} /*lint !e818 xQueue不能声明为指向const的指针，因为它是一个typedef */

#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configUSE_TIMERS == 1 )  /* 如果启用了定时器功能 */

/*******************************************************************************
 * 函数名称：vQueueWaitForMessageRestricted
 * 功能描述：受限地等待队列消息的内部函数
 *           此函数专为内核代码设计，有特殊的调用要求
 * 输入参数：
 *   - xQueue: 队列句柄，指定要等待的队列
 *   - xTicksToWait: 最大等待时间（以时钟节拍为单位）
 *   - xWaitIndefinitely: 是否无限期等待的标志
 * 其它说明：
 *   - 此函数不应由应用程序代码调用，因此名称中有'Restricted'
 *   - 它不是公共API的一部分，专为内核代码设计
 *   - 调用时应锁定调度器，而不是从临界区内调用
 *   - 如果队列中没有消息，会将任务放置在阻塞列表中，但不会立即阻塞
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void vQueueWaitForMessageRestricted( QueueHandle_t xQueue, TickType_t xTicksToWait, const BaseType_t xWaitIndefinitely )
{
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* 将队列句柄转换为队列结构体指针 */

    /* 此函数不应由应用程序代码调用，因此名称中有'Restricted'。
       它不是公共API的一部分。它专为内核代码设计，有特殊的调用要求。
       它可能导致vListInsert()在一个可能只有一个项目的列表上被调用，
       因此列表操作会很快，但即使如此，也应该在调度器锁定的情况下调用，
       而不是从临界区内调用。 */

    /* 只有在队列中没有消息时才执行任何操作。此函数不会实际导致任务阻塞，
       只是将其放置在阻塞列表中。它不会阻塞直到调度器解锁 - 此时将执行yield。
       如果在队列锁定时将项添加到队列，并且调用任务在队列上阻塞，
       则当队列解锁时，调用任务将立即解除阻塞。 */
    prvLockQueue( pxQueue );
    if( pxQueue->uxMessagesWaiting == ( UBaseType_t ) 0U )
    {
        /* 队列中没有任何内容，阻塞指定的时间段。 */
        vTaskPlaceOnEventListRestricted( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait, xWaitIndefinitely );
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
    }
    prvUnlockQueue( pxQueue );
}

#endif /* configUSE_TIMERS */
/*-----------------------------------------------------------*/

#if( ( configUSE_QUEUE_SETS == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )  /* 如果启用了队列集功能和动态分配 */

/*******************************************************************************
 * 函数名称：xQueueCreateSet
 * 功能描述：创建队列集
 *           此函数用于创建一个队列集，允许多个队列向同一个集合发送数据
 * 输入参数：
 *   - uxEventQueueLength: 队列集的事件队列长度
 * 返 回 值：
 *   - QueueSetHandle_t: 新创建的队列集句柄，如果创建失败则返回NULL
 * 其它说明：
 *   - 此功能仅在启用队列集功能和动态分配时可用
 *   - 队列集是一种特殊的队列，用于监听多个队列的事件
 *   - 使用xQueueGenericCreate创建队列集，指定队列类型为queueQUEUE_TYPE_SET
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
QueueSetHandle_t xQueueCreateSet( const UBaseType_t uxEventQueueLength )
{
    QueueSetHandle_t pxQueue;  /* 队列集句柄 */

    /* 创建队列集，项大小为Queue_t指针的大小，类型为队列集 */
    pxQueue = xQueueGenericCreate( uxEventQueueLength, sizeof( Queue_t * ), queueQUEUE_TYPE_SET );

    return pxQueue;
}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )  /* 如果启用了队列集功能 */

/*******************************************************************************
 * 函数名称：xQueueAddToSet
 * 功能描述：将队列或信号量添加到队列集中
 *           此函数用于将队列或信号量注册到队列集，使其成为队列集的成员
 * 输入参数：
 *   - xQueueOrSemaphore: 队列或信号量句柄，指定要添加到队列集的成员
 *   - xQueueSet: 队列集句柄，指定目标队列集
 * 返 回 值：
 *   - BaseType_t: 成功添加到队列集返回pdPASS，失败返回pdFAIL
 * 其它说明：
 *   - 此功能仅在启用队列集功能时可用
 *   - 一个队列/信号量只能属于一个队列集
 *   - 如果队列/信号量中已有数据项，则不能添加到队列集
 *   - 操作在临界区内执行，确保原子性
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
BaseType_t xQueueAddToSet( QueueSetMemberHandle_t xQueueOrSemaphore, QueueSetHandle_t xQueueSet )
{
    BaseType_t xReturn;  /* 函数返回值 */

    /* 进入临界区保护操作 */
    taskENTER_CRITICAL();
    {
        /* 检查队列/信号量是否已经属于某个队列集 */
        if( ( ( Queue_t * ) xQueueOrSemaphore )->pxQueueSetContainer != NULL )
        {
            /* 不能将队列/信号量添加到多个队列集中。 */
            xReturn = pdFAIL;
        }
        /* 检查队列/信号量中是否有数据项 */
        else if( ( ( Queue_t * ) xQueueOrSemaphore )->uxMessagesWaiting != ( UBaseType_t ) 0 )
        {
            /* 如果队列/信号量中已有数据项，则不能将其添加到队列集中。 */
            xReturn = pdFAIL;
        }
        else
        {
            /* 设置队列/信号量的队列集容器指针 */
            ( ( Queue_t * ) xQueueOrSemaphore )->pxQueueSetContainer = xQueueSet;
            xReturn = pdPASS;
        }
    }
    /* 退出临界区 */
    taskEXIT_CRITICAL();

    return xReturn;
}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )  /* 如果启用了队列集功能 */

/*******************************************************************************
 * 函数名称：xQueueRemoveFromSet
 * 功能描述：从队列集中移除队列或信号量
 *           此函数用于将队列或信号量从队列集中移除，取消其成员资格
 * 输入参数：
 *   - xQueueOrSemaphore: 队列或信号量句柄，指定要从队列集移除的成员
 *   - xQueueSet: 队列集句柄，指定源队列集
 * 返 回 值：
 *   - BaseType_t: 成功从队列集移除返回pdPASS，失败返回pdFAIL
 * 其它说明：
 *   - 此功能仅在启用队列集功能时可用
 *   - 如果队列/信号量不是指定队列集的成员，则移除失败
 *   - 如果队列/信号量中有数据项，则移除失败（避免悬垂事件）
 *   - 操作在临界区内执行，确保原子性
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
BaseType_t xQueueRemoveFromSet( QueueSetMemberHandle_t xQueueOrSemaphore, QueueSetHandle_t xQueueSet )
{
    BaseType_t xReturn;                              /* 函数返回值 */
    Queue_t * const pxQueueOrSemaphore = ( Queue_t * ) xQueueOrSemaphore;  /* 将句柄转换为队列结构体指针 */

    /* 检查队列/信号量是否属于指定的队列集 */
    if( pxQueueOrSemaphore->pxQueueSetContainer != xQueueSet )
    {
        /* 该队列不是集合的成员。 */
        xReturn = pdFAIL;
    }
    /* 检查队列/信号量中是否有数据项 */
    else if( pxQueueOrSemaphore->uxMessagesWaiting != ( UBaseType_t ) 0 )
    {
        /* 当队列不为空时从集合中移除队列是危险的，因为队列集仍将保存该队列的挂起事件。 */
        xReturn = pdFAIL;
    }
    else
    {
        /* 进入临界区保护操作 */
        taskENTER_CRITICAL();
        {
            /* 队列不再包含在集合中。 */
            pxQueueOrSemaphore->pxQueueSetContainer = NULL;
        }
        /* 退出临界区 */
        taskEXIT_CRITICAL();
        xReturn = pdPASS;
    }

    return xReturn;
} /*lint !e818 xQueueSet不能声明为指向const的指针，因为它是一个typedef */

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xQueueSelectFromSet
 * 功能描述：从队列集中选择有数据的成员
 *           此函数用于从队列集中接收有数据可用的队列或信号量句柄
 * 输入参数：
 *   - xQueueSet: 队列集句柄，指定要操作的队列集
 *   - xTicksToWait: 最大等待时间（以时钟节拍为单位）
 * 返 回 值：
 *   - QueueSetMemberHandle_t: 有数据可用的队列或信号量句柄，如果超时则返回NULL
 * 其它说明：
 *   - 此功能仅在启用队列集功能时可用
 *   - 使用队列接收操作从队列集中获取有数据的成员句柄
 *   - 支持阻塞等待，直到有成员有数据可用或超时
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
 #if ( configUSE_QUEUE_SETS == 1 )  /* 如果启用了队列集功能 */
 
QueueSetMemberHandle_t xQueueSelectFromSet( QueueSetHandle_t xQueueSet, TickType_t const xTicksToWait )
{
    QueueSetMemberHandle_t xReturn = NULL;  /* 返回值，初始化为NULL */

    /* 使用通用队列接收函数从队列集中接收数据（成员句柄） */
    ( void ) xQueueGenericReceive( ( QueueHandle_t ) xQueueSet, &xReturn, xTicksToWait, pdFALSE ); /*lint !e961 从一个typedef转换到另一个typedef不是冗余的 */
    return xReturn;
}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/
#if ( configUSE_QUEUE_SETS == 1 )  /* 如果启用了队列集功能 */

/*******************************************************************************
 * 函数名称：xQueueSelectFromSetFromISR
 * 功能描述：从中断服务程序从队列集中选择有数据的成员
 *           此函数是中断安全版本，用于在ISR中从队列集获取有数据可用的成员句柄
 * 输入参数：
 *   - xQueueSet: 队列集句柄，指定要操作的队列集
 * 返 回 值：
 *   - QueueSetMemberHandle_t: 有数据可用的队列或信号量句柄，如果没有则返回NULL
 * 其它说明：
 *   - 此功能仅在启用队列集功能时可用
 *   - 使用中断安全的队列接收操作从队列集中获取成员句柄
 *   - 不会阻塞，立即返回结果
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
QueueSetMemberHandle_t xQueueSelectFromSetFromISR( QueueSetHandle_t xQueueSet )
{
    QueueSetMemberHandle_t xReturn = NULL;  /* 返回值，初始化为NULL */

    /* 使用中断安全的队列接收函数从队列集中接收数据（成员句柄） */
    ( void ) xQueueReceiveFromISR( ( QueueHandle_t ) xQueueSet, &xReturn, NULL ); /*lint !e961 从一个typedef转换到另一个typedef不是冗余的 */
    return xReturn;
}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )  /* 如果启用了队列集功能 */

/*******************************************************************************
 * 函数名称：prvNotifyQueueSetContainer
 * 功能描述：通知队列集容器的内部函数
 *           当队列有数据时，此函数用于通知其所属的队列集
 * 输入参数：
 *   - pxQueue: 队列结构体指针，指定有数据的队列
 *   - xCopyPosition: 数据复制位置标识
 * 返 回 值：
 *   - BaseType_t: 如果唤醒了更高优先级的任务返回pdTRUE，否则返回pdFALSE
 * 其它说明：
 *   - 此功能仅在启用队列集功能时可用
 *   - 此函数必须在临界区内调用
 *   - 将队列句柄复制到队列集中，表示该队列有数据可用
 *   - 处理队列集锁定状态下的通知操作
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
static BaseType_t prvNotifyQueueSetContainer( const Queue_t * const pxQueue, const BaseType_t xCopyPosition )
{
    Queue_t *pxQueueSetContainer = pxQueue->pxQueueSetContainer;  /* 队列集容器指针 */
    BaseType_t xReturn = pdFALSE;                                 /* 返回值，初始化为pdFALSE */

    /* 此函数必须从临界区内调用 */

    /* 断言检查：确保队列集容器有效 */
    configASSERT( pxQueueSetContainer );
    /* 断言检查：确保队列集有空间 */
    configASSERT( pxQueueSetContainer->uxMessagesWaiting < pxQueueSetContainer->uxLength );

    /* 再次检查队列集是否有空间 */
    if( pxQueueSetContainer->uxMessagesWaiting < pxQueueSetContainer->uxLength )
    {
        /* 获取队列集的发送锁定状态 */
        const int8_t cTxLock = pxQueueSetContainer->cTxLock;

        /* 跟踪队列发送事件 */
        traceQUEUE_SEND( pxQueueSetContainer );

        /* 复制的数据是包含数据的队列的句柄。 */
        xReturn = prvCopyDataToQueue( pxQueueSetContainer, &pxQueue, xCopyPosition );

        /* 检查队列集是否未锁定 */
        if( cTxLock == queueUNLOCKED )
        {
            /* 检查是否有任务正在等待从队列集接收数据 */
            if( listLIST_IS_EMPTY( &( pxQueueSetContainer->xTasksWaitingToReceive ) ) == pdFALSE )
            {
                /* 从等待接收列表中移除任务并解除阻塞 */
                if( xTaskRemoveFromEventList( &( pxQueueSetContainer->xTasksWaitingToReceive ) ) != pdFALSE )
                {
                    /* 等待的任务具有更高的优先级。 */
                    xReturn = pdTRUE;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
            }
        }
        else  /* 队列集处于锁定状态 */
        {
            /* 增加发送锁定计数，解锁时会处理挂起的操作 */
            pxQueueSetContainer->cTxLock = ( int8_t ) ( cTxLock + 1 );
        }
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（空分支） */
    }

    return xReturn;
}

#endif /* configUSE_QUEUE_SETS */













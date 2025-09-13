/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： LM_queue.h
 * 文件标识： 
 * 内容摘要： 队列模块定义
 * 其它说明： 无
 * 当前版本： FreeRTOS V9.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年09月01日
 *
 *******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef QUEUE_H
#define QUEUE_H

/* Includes ------------------------------------------------------------------*/
#ifndef INC_FREERTOS_H
	#error "include FreeRTOS.h" must appear in source files before "include queue.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types ------------------------------------------------------------*/
/**
 * 队列句柄类型定义
 * 通过xQueueCreate返回的队列引用句柄
 */
typedef void * QueueHandle_t;

/**
 * 队列集句柄类型定义
 * 通过xQueueCreateSet返回的队列集引用句柄
 */
typedef void * QueueSetHandle_t;

/**
 * 队列集成员工句柄类型定义
 * 用于参数或返回值可以是QueueHandle_t或SemaphoreHandle_t的情况
 */
typedef void * QueueSetMemberHandle_t;

/* Exported constants --------------------------------------------------------*/
/* 注：队列模块没有导出的常量定义 */

/* Exported macro ------------------------------------------------------------*/
/* 内部使用的发送位置定义 */
#define	queueSEND_TO_BACK		( ( BaseType_t ) 0 )  /*< 发送到队列尾部 */
#define	queueSEND_TO_FRONT		( ( BaseType_t ) 1 )  /*< 发送到队列头部 */
#define queueOVERWRITE			( ( BaseType_t ) 2 )  /*< 覆盖队列内容 */

/* 内部使用的队列类型定义 */
#define queueQUEUE_TYPE_BASE				( ( uint8_t ) 0U )  /*< 基本队列类型 */
#define queueQUEUE_TYPE_SET					( ( uint8_t ) 0U )  /*< 队列集类型 */
#define queueQUEUE_TYPE_MUTEX 				( ( uint8_t ) 1U )  /*< 互斥量类型 */
#define queueQUEUE_TYPE_COUNTING_SEMAPHORE	( ( uint8_t ) 2U )  /*< 计数信号量类型 */
#define queueQUEUE_TYPE_BINARY_SEMAPHORE	( ( uint8_t ) 3U )  /*< 二进制信号量类型 */
#define queueQUEUE_TYPE_RECURSIVE_MUTEX		( ( uint8_t ) 4U )  /*< 递归互斥量类型 */

/**
 * 创建队列宏（动态内存分配）
 * @param uxQueueLength 队列长度
 * @param uxItemSize 队列项大小
 */
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	#define xQueueCreate( uxQueueLength, uxItemSize ) xQueueGenericCreate( ( uxQueueLength ), ( uxItemSize ), ( queueQUEUE_TYPE_BASE ) )
#endif

/**
 * 创建队列宏（静态内存分配）
 * @param uxQueueLength 队列长度
 * @param uxItemSize 队列项大小
 * @param pucQueueStorage 队列存储缓冲区
 * @param pxQueueBuffer 静态队列缓冲区
 */
#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	#define xQueueCreateStatic( uxQueueLength, uxItemSize, pucQueueStorage, pxQueueBuffer ) xQueueGenericCreateStatic( ( uxQueueLength ), ( uxItemSize ), ( pucQueueStorage ), ( pxQueueBuffer ), ( queueQUEUE_TYPE_BASE ) )
#endif /* configSUPPORT_STATIC_ALLOCATION */

/**
 * 发送到队列头部宏
 * @param xQueue 队列句柄
 * @param pvItemToQueue 要发送的数据指针
 * @param xTicksToWait 等待的最大滴答数
 */
#define xQueueSendToFront( xQueue, pvItemToQueue, xTicksToWait ) xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), ( xTicksToWait ), queueSEND_TO_FRONT )

/**
 * 发送到队列尾部宏
 * @param xQueue 队列句柄
 * @param pvItemToQueue 要发送的数据指针
 * @param xTicksToWait 等待的最大滴答数
 */
#define xQueueSendToBack( xQueue, pvItemToQueue, xTicksToWait ) xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), ( xTicksToWait ), queueSEND_TO_BACK )

/**
 * 发送到队列宏（向后兼容）
 * @param xQueue 队列句柄
 * @param pvItemToQueue 要发送的数据指针
 * @param xTicksToWait 等待的最大滴答数
 */
#define xQueueSend( xQueue, pvItemToQueue, xTicksToWait ) xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), ( xTicksToWait ), queueSEND_TO_BACK )

/**
 * 覆盖队列内容宏
 * @param xQueue 队列句柄
 * @param pvItemToQueue 要发送的数据指针
 */
#define xQueueOverwrite( xQueue, pvItemToQueue ) xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), 0, queueOVERWRITE )

/**
 * 查看队列头部数据宏
 * @param xQueue 队列句柄
 * @param pvBuffer 数据接收缓冲区
 * @param xTicksToWait 等待的最大滴答数
 */
#define xQueuePeek( xQueue, pvBuffer, xTicksToWait ) xQueueGenericReceive( ( xQueue ), ( pvBuffer ), ( xTicksToWait ), pdTRUE )

/**
 * 接收队列数据宏
 * @param xQueue 队列句柄
 * @param pvBuffer 数据接收缓冲区
 * @param xTicksToWait 等待的最大滴答数
 */
#define xQueueReceive( xQueue, pvBuffer, xTicksToWait ) xQueueGenericReceive( ( xQueue ), ( pvBuffer ), ( xTicksToWait ), pdFALSE )

/**
 * 从ISR发送到队列头部宏
 * @param xQueue 队列句柄
 * @param pvItemToQueue 要发送的数据指针
 * @param pxHigherPriorityTaskWoken 高优先级任务唤醒标志指针
 */
#define xQueueSendToFrontFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) xQueueGenericSendFromISR( ( xQueue ), ( pvItemToQueue ), ( pxHigherPriorityTaskWoken ), queueSEND_TO_FRONT )

/**
 * 从ISR发送到队列尾部宏
 * @param xQueue 队列句柄
 * @param pvItemToQueue 要发送的数据指针
 * @param pxHigherPriorityTaskWoken 高优先级任务唤醒标志指针
 */
#define xQueueSendToBackFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) xQueueGenericSendFromISR( ( xQueue ), ( pvItemToQueue ), ( pxHigherPriorityTaskWoken ), queueSEND_TO_BACK )

/**
 * 从ISR发送到队列宏
 * @param xQueue 队列句柄
 * @param pvItemToQueue 要发送的数据指针
 * @param pxHigherPriorityTaskWoken 高优先级任务唤醒标志指针
 */
#define xQueueSendFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) xQueueGenericSendFromISR( ( xQueue ), ( pvItemToQueue ), ( pxHigherPriorityTaskWoken ), queueSEND_TO_BACK )

/**
 * 从ISR覆盖队列内容宏
 * @param xQueue 队列句柄
 * @param pvItemToQueue 要发送的数据指针
 * @param pxHigherPriorityTaskWoken 高优先级任务唤醒标志指针
 */
#define xQueueOverwriteFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) xQueueGenericSendFromISR( ( xQueue ), ( pvItemToQueue ), ( pxHigherPriorityTaskWoken ), queueOVERWRITE )

/**
 * 重置队列宏
 * @param xQueue 队列句柄
 */
#define xQueueReset( xQueue ) xQueueGenericReset( xQueue, pdFALSE )

/* Exported functions --------------------------------------------------------*/
/**
 * 通用队列发送函数
 * @param xQueue 队列句柄
 * @param pvItemToQueue 要发送的数据指针
 * @param xTicksToWait 等待的最大滴答数
 * @param xCopyPosition 发送位置（头部或尾部）
 * @return 发送结果
 */
BaseType_t xQueueGenericSend( QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait, const BaseType_t xCopyPosition ) PRIVILEGED_FUNCTION;

/**
 * 从ISR查看队列头部数据函数
 * @param xQueue 队列句柄
 * @param pvBuffer 数据接收缓冲区
 * @return 查看结果
 */
BaseType_t xQueuePeekFromISR( QueueHandle_t xQueue, void * const pvBuffer ) PRIVILEGED_FUNCTION;

/**
 * 通用队列接收函数
 * @param xQueue 队列句柄
 * @param pvBuffer 数据接收缓冲区
 * @param xTicksToWait 等待的最大滴答数
 * @param xJustPeek 是否只查看不移除
 * @return 接收结果
 */
BaseType_t xQueueGenericReceive( QueueHandle_t xQueue, void * const pvBuffer, TickType_t xTicksToWait, const BaseType_t xJustPeek ) PRIVILEGED_FUNCTION;

/**
 * 获取队列中消息数量函数
 * @param xQueue 队列句柄
 * @return 消息数量
 */
UBaseType_t uxQueueMessagesWaiting( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

/**
 * 获取队列中可用空间数量函数
 * @param xQueue 队列句柄
 * @return 可用空间数量
 */
UBaseType_t uxQueueSpacesAvailable( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

/**
 * 删除队列函数
 * @param xQueue 队列句柄
 */
void vQueueDelete( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

/**
 * 从ISR通用队列发送函数
 * @param xQueue 队列句柄
 * @param pvItemToQueue 要发送的数据指针
 * @param pxHigherPriorityTaskWoken 高优先级任务唤醒标志指针
 * @param xCopyPosition 发送位置（头部或尾部）
 * @return 发送结果
 */
BaseType_t xQueueGenericSendFromISR( QueueHandle_t xQueue, const void * const pvItemToQueue, BaseType_t * const pxHigherPriorityTaskWoken, const BaseType_t xCopyPosition ) PRIVILEGED_FUNCTION;

/**
 * 从ISR给予信号量函数
 * @param xQueue 队列句柄
 * @param pxHigherPriorityTaskWoken 高优先级任务唤醒标志指针
 * @return 给予结果
 */
BaseType_t xQueueGiveFromISR( QueueHandle_t xQueue, BaseType_t * const pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;

/**
 * 从ISR接收队列数据函数
 * @param xQueue 队列句柄
 * @param pvBuffer 数据接收缓冲区
 * @param pxHigherPriorityTaskWoken 高优先级任务唤醒标志指针
 * @return 接收结果
 */
BaseType_t xQueueReceiveFromISR( QueueHandle_t xQueue, void * const pvBuffer, BaseType_t * const pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;

/**
 * 从ISR检查队列是否为空函数
 * @param xQueue 队列句柄
 * @return 是否为空
 */
BaseType_t xQueueIsQueueEmptyFromISR( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

/**
 * 从ISR检查队列是否已满函数
 * @param xQueue 队列句柄
 * @return 是否已满
 */
BaseType_t xQueueIsQueueFullFromISR( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

/**
 * 从ISR获取队列中消息数量函数
 * @param xQueue 队列句柄
 * @return 消息数量
 */
UBaseType_t uxQueueMessagesWaitingFromISR( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

/**
 * 创建互斥量函数
 * @param ucQueueType 队列类型
 * @return 互斥量句柄
 */
QueueHandle_t xQueueCreateMutex( const uint8_t ucQueueType ) PRIVILEGED_FUNCTION;

/**
 * 创建静态互斥量函数
 * @param ucQueueType 队列类型
 * @param pxStaticQueue 静态队列缓冲区
 * @return 互斥量句柄
 */
QueueHandle_t xQueueCreateMutexStatic( const uint8_t ucQueueType, StaticQueue_t *pxStaticQueue ) PRIVILEGED_FUNCTION;

/**
 * 创建计数信号量函数
 * @param uxMaxCount 最大计数值
 * @param uxInitialCount 初始计数值
 * @return 信号量句柄
 */
QueueHandle_t xQueueCreateCountingSemaphore( const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount ) PRIVILEGED_FUNCTION;

/**
 * 创建静态计数信号量函数
 * @param uxMaxCount 最大计数值
 * @param uxInitialCount 初始计数值
 * @param pxStaticQueue 静态队列缓冲区
 * @return 信号量句柄
 */
QueueHandle_t xQueueCreateCountingSemaphoreStatic( const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount, StaticQueue_t *pxStaticQueue ) PRIVILEGED_FUNCTION;

/**
 * 获取互斥量持有者函数
 * @param xSemaphore 信号量句柄
 * @return 持有者指针
 */
void* xQueueGetMutexHolder( QueueHandle_t xSemaphore ) PRIVILEGED_FUNCTION;

/**
 * 递归获取互斥量函数
 * @param xMutex 互斥量句柄
 * @param xTicksToWait 等待的最大滴答数
 * @return 获取结果
 */
BaseType_t xQueueTakeMutexRecursive( QueueHandle_t xMutex, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/**
 * 递归释放互斥量函数
 * @param pxMutex 互斥量句柄
 * @return 释放结果
 */
BaseType_t xQueueGiveMutexRecursive( QueueHandle_t pxMutex ) PRIVILEGED_FUNCTION;

#if( configQUEUE_REGISTRY_SIZE > 0 )
	/**
	 * 添加队列到注册表函数
	 * @param xQueue 队列句柄
	 * @param pcName 队列名称
	 */
	void vQueueAddToRegistry( QueueHandle_t xQueue, const char *pcName ) PRIVILEGED_FUNCTION;

	/**
	 * 从注册表移除队列函数
	 * @param xQueue 队列句柄
	 */
	void vQueueUnregisterQueue( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

	/**
	 * 获取队列名称函数
	 * @param xQueue 队列句柄
	 * @return 队列名称
	 */
	const char *pcQueueGetName( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;
#endif

#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	/**
	 * 通用队列创建函数（动态内存分配）
	 * @param uxQueueLength 队列长度
	 * @param uxItemSize 队列项大小
	 * @param ucQueueType 队列类型
	 * @return 队列句柄
	 */
	QueueHandle_t xQueueGenericCreate( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, const uint8_t ucQueueType ) PRIVILEGED_FUNCTION;
#endif

#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	/**
	 * 通用队列创建函数（静态内存分配）
	 * @param uxQueueLength 队列长度
	 * @param uxItemSize 队列项大小
	 * @param pucQueueStorage 队列存储缓冲区
	 * @param pxStaticQueue 静态队列缓冲区
	 * @param ucQueueType 队列类型
	 * @return 队列句柄
	 */
	QueueHandle_t xQueueGenericCreateStatic( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, uint8_t *pucQueueStorage, StaticQueue_t *pxStaticQueue, const uint8_t ucQueueType ) PRIVILEGED_FUNCTION;
#endif

/**
 * 创建队列集函数
 * @param uxEventQueueLength 事件队列长度
 * @return 队列集句柄
 */
QueueSetHandle_t xQueueCreateSet( const UBaseType_t uxEventQueueLength ) PRIVILEGED_FUNCTION;

/**
 * 添加到队列集函数
 * @param xQueueOrSemaphore 队列或信号量句柄
 * @param xQueueSet 队列集句柄
 * @return 添加结果
 */
BaseType_t xQueueAddToSet( QueueSetMemberHandle_t xQueueOrSemaphore, QueueSetHandle_t xQueueSet ) PRIVILEGED_FUNCTION;

/**
 * 从队列集移除函数
 * @param xQueueOrSemaphore 队列或信号量句柄
 * @param xQueueSet 队列集句柄
 * @return 移除结果
 */
BaseType_t xQueueRemoveFromSet( QueueSetMemberHandle_t xQueueOrSemaphore, QueueSetHandle_t xQueueSet ) PRIVILEGED_FUNCTION;

/**
 * 从队列集选择函数
 * @param xQueueSet 队列集句柄
 * @param xTicksToWait 等待的最大滴答数
 * @return 选择的队列或信号量句柄
 */
QueueSetMemberHandle_t xQueueSelectFromSet( QueueSetHandle_t xQueueSet, const TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/**
 * 从ISR从队列集选择函数
 * @param xQueueSet 队列集句柄
 * @return 选择的队列或信号量句柄
 */
QueueSetMemberHandle_t xQueueSelectFromSetFromISR( QueueSetHandle_t xQueueSet ) PRIVILEGED_FUNCTION;

/* Private types -------------------------------------------------------------*/
/* 注：队列模块没有私有类型定义 */

/* Private variables ---------------------------------------------------------*/
/* 注：队列模块没有私有变量定义 */

/* Private constants ---------------------------------------------------------*/
/* 注：队列模块没有私有常量定义 */

/* Private macros ------------------------------------------------------------*/
/* 注：私有宏定义已包含在导出宏中 */

/* Private functions ---------------------------------------------------------*/
/**
 * 受限等待队列消息函数
 * @param xQueue 队列句柄
 * @param xTicksToWait 等待的最大滴答数
 * @param xWaitIndefinitely 是否无限等待
 */
void vQueueWaitForMessageRestricted( QueueHandle_t xQueue, TickType_t xTicksToWait, const BaseType_t xWaitIndefinitely ) PRIVILEGED_FUNCTION;

/**
 * 通用队列重置函数
 * @param xQueue 队列句柄
 * @param xNewQueue 是否为新队列
 * @return 重置结果
 */
BaseType_t xQueueGenericReset( QueueHandle_t xQueue, BaseType_t xNewQueue ) PRIVILEGED_FUNCTION;

/**
 * 设置队列编号函数
 * @param xQueue 队列句柄
 * @param uxQueueNumber 队列编号
 */
void vQueueSetQueueNumber( QueueHandle_t xQueue, UBaseType_t uxQueueNumber ) PRIVILEGED_FUNCTION;

/**
 * 获取队列编号函数
 * @param xQueue 队列句柄
 * @return 队列编号
 */
UBaseType_t uxQueueGetQueueNumber( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

/**
 * 获取队列类型函数
 * @param xQueue 队列句柄
 * @return 队列类型
 */
uint8_t ucQueueGetQueueType( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

#ifdef __cplusplus
}
#endif

#endif /* QUEUE_H */

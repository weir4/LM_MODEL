/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： LM_timers.h
 * 文件标识： 
 * 内容摘要： 定时器模块声明
 * 其它说明： 无
 * 当前版本： FreeRTOS V9.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年09月01日
 *
 *******************************************************************************/


/* Includes ------------------------------------------------------------------*/
#ifndef INC_FREERTOS_H
	#error "include FreeRTOS.h must appear in source files before include timers.h"
#endif

#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types ------------------------------------------------------------*/
/**
 * 定时器命令ID定义
 * 用于在定时器队列上发送/接收的命令ID
 */
#define tmrCOMMAND_EXECUTE_CALLBACK_FROM_ISR 	( ( BaseType_t ) -2 )  /*< 从ISR执行回调命令 */
#define tmrCOMMAND_EXECUTE_CALLBACK				( ( BaseType_t ) -1 )  /*< 执行回调命令 */
#define tmrCOMMAND_START_DONT_TRACE				( ( BaseType_t ) 0 )   /*< 启动定时器（不跟踪） */
#define tmrCOMMAND_START					    ( ( BaseType_t ) 1 )   /*< 启动定时器 */
#define tmrCOMMAND_RESET						( ( BaseType_t ) 2 )   /*< 重置定时器 */
#define tmrCOMMAND_STOP							( ( BaseType_t ) 3 )   /*< 停止定时器 */
#define tmrCOMMAND_CHANGE_PERIOD				( ( BaseType_t ) 4 )   /*< 更改定时器周期 */
#define tmrCOMMAND_DELETE						( ( BaseType_t ) 5 )   /*< 删除定时器 */

#define tmrFIRST_FROM_ISR_COMMAND				( ( BaseType_t ) 6 )   /*< 第一个ISR命令 */
#define tmrCOMMAND_START_FROM_ISR				( ( BaseType_t ) 6 )   /*< 从ISR启动定时器 */
#define tmrCOMMAND_RESET_FROM_ISR				( ( BaseType_t ) 7 )   /*< 从ISR重置定时器 */
#define tmrCOMMAND_STOP_FROM_ISR				( ( BaseType_t ) 8 )   /*< 从ISR停止定时器 */
#define tmrCOMMAND_CHANGE_PERIOD_FROM_ISR		( ( BaseType_t ) 9 )   /*< 从ISR更改定时器周期 */

/**
 * 定时器句柄类型定义
 * 通过xTimerCreate返回的定时器引用句柄
 */
typedef void * TimerHandle_t;

/**
 * 定时器回调函数原型定义
 * 定时器回调函数必须符合此原型
 */
typedef void (*TimerCallbackFunction_t)( TimerHandle_t xTimer );

/**
 * 挂起函数原型定义
 * 与xTimerPendFunctionCallFromISR()函数一起使用的函数必须符合此原型
 */
typedef void (*PendedFunction_t)( void *, uint32_t );

/* Exported constants --------------------------------------------------------*/
/* 注：定时器模块没有导出的常量定义 */

/* Exported macro ------------------------------------------------------------*/
/**
 * 启动定时器宏
 * @param xTimer 要启动的定时器句柄
 * @param xTicksToWait 等待命令发送的最大滴答数
 */
#define xTimerStart( xTimer, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_START, ( xTaskGetTickCount() ), NULL, ( xTicksToWait ) )

/**
 * 停止定时器宏
 * @param xTimer 要停止的定时器句柄
 * @param xTicksToWait 等待命令发送的最大滴答数
 */
#define xTimerStop( xTimer, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_STOP, 0U, NULL, ( xTicksToWait ) )

/**
 * 更改定时器周期宏
 * @param xTimer 要更改周期的定时器句柄
 * @param xNewPeriod 新的定时器周期
 * @param xTicksToWait 等待命令发送的最大滴答数
 */
#define xTimerChangePeriod( xTimer, xNewPeriod, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_CHANGE_PERIOD, ( xNewPeriod ), NULL, ( xTicksToWait ) )

/**
 * 删除定时器宏
 * @param xTimer 要删除的定时器句柄
 * @param xTicksToWait 等待命令发送的最大滴答数
 */
#define xTimerDelete( xTimer, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_DELETE, 0U, NULL, ( xTicksToWait ) )

/**
 * 重置定时器宏
 * @param xTimer 要重置的定时器句柄
 * @param xTicksToWait 等待命令发送的最大滴答数
 */
#define xTimerReset( xTimer, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_RESET, ( xTaskGetTickCount() ), NULL, ( xTicksToWait ) )

/**
 * 从ISR启动定时器宏
 * @param xTimer 要启动的定时器句柄
 * @param pxHigherPriorityTaskWoken 高优先级任务唤醒标志指针
 */
#define xTimerStartFromISR( xTimer, pxHigherPriorityTaskWoken ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_START_FROM_ISR, ( xTaskGetTickCountFromISR() ), ( pxHigherPriorityTaskWoken ), 0U )

/**
 * 从ISR停止定时器宏
 * @param xTimer 要停止的定时器句柄
 * @param pxHigherPriorityTaskWoken 高优先级任务唤醒标志指针
 */
#define xTimerStopFromISR( xTimer, pxHigherPriorityTaskWoken ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_STOP_FROM_ISR, 0, ( pxHigherPriorityTaskWoken ), 0U )

/**
 * 从ISR更改定时器周期宏
 * @param xTimer 要更改周期的定时器句柄
 * @param xNewPeriod 新的定时器周期
 * @param pxHigherPriorityTaskWoken 高优先级任务唤醒标志指针
 */
#define xTimerChangePeriodFromISR( xTimer, xNewPeriod, pxHigherPriorityTaskWoken ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_CHANGE_PERIOD_FROM_ISR, ( xNewPeriod ), ( pxHigherPriorityTaskWoken ), 0U )

/**
 * 从ISR重置定时器宏
 * @param xTimer 要重置的定时器句柄
 * @param pxHigherPriorityTaskWoken 高优先级任务唤醒标志指针
 */
#define xTimerResetFromISR( xTimer, pxHigherPriorityTaskWoken ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_RESET_FROM_ISR, ( xTaskGetTickCountFromISR() ), ( pxHigherPriorityTaskWoken ), 0U )

/* Exported functions --------------------------------------------------------*/
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	/**
	 * 创建定时器（动态内存分配）
	 * @param pcTimerName 定时器名称（用于调试）
	 * @param xTimerPeriodInTicks 定时器周期（滴答数）
	 * @param uxAutoReload 是否自动重载（pdTRUE为自动重载，pdFALSE为单次定时器）
	 * @param pvTimerID 定时器标识符
	 * @param pxCallbackFunction 定时器回调函数
	 * @return 成功返回定时器句柄，失败返回NULL
	 */
	TimerHandle_t xTimerCreate(	const char * const pcTimerName,
								const TickType_t xTimerPeriodInTicks,
								const UBaseType_t uxAutoReload,
								void * const pvTimerID,
								TimerCallbackFunction_t pxCallbackFunction ) PRIVILEGED_FUNCTION;
#endif

#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	/**
	 * 创建定时器（静态内存分配）
	 * @param pcTimerName 定时器名称（用于调试）
	 * @param xTimerPeriodInTicks 定时器周期（滴答数）
	 * @param uxAutoReload 是否自动重载（pdTRUE为自动重载，pdFALSE为单次定时器）
	 * @param pvTimerID 定时器标识符
	 * @param pxCallbackFunction 定时器回调函数
	 * @param pxTimerBuffer 静态定时器缓冲区指针
	 * @return 成功返回定时器句柄，失败返回NULL
	 */
	TimerHandle_t xTimerCreateStatic(	const char * const pcTimerName,
										const TickType_t xTimerPeriodInTicks,
										const UBaseType_t uxAutoReload,
										void * const pvTimerID,
										TimerCallbackFunction_t pxCallbackFunction,
										StaticTimer_t *pxTimerBuffer ) PRIVILEGED_FUNCTION;
#endif /* configSUPPORT_STATIC_ALLOCATION */

/**
 * 获取定时器ID
 * @param xTimer 定时器句柄
 * @return 定时器ID
 */
void *pvTimerGetTimerID( const TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/**
 * 设置定时器ID
 * @param xTimer 定时器句柄
 * @param pvNewID 新的定时器ID
 */
void vTimerSetTimerID( TimerHandle_t xTimer, void *pvNewID ) PRIVILEGED_FUNCTION;

/**
 * 检查定时器是否处于活动状态
 * @param xTimer 定时器句柄
 * @return 如果定时器处于活动状态返回非零值，否则返回pdFALSE
 */
BaseType_t xTimerIsTimerActive( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/**
 * 获取定时器守护任务句柄
 * @return 定时器守护任务句柄
 */
TaskHandle_t xTimerGetTimerDaemonTaskHandle( void ) PRIVILEGED_FUNCTION;

/**
 * 从ISR挂起函数调用
 * @param xFunctionToPend 要挂起的函数
 * @param pvParameter1 函数第一个参数
 * @param ulParameter2 函数第二个参数
 * @param pxHigherPriorityTaskWoken 高优先级任务唤醒标志指针
 * @return 成功返回pdPASS，失败返回pdFALSE
 */
BaseType_t xTimerPendFunctionCallFromISR( PendedFunction_t xFunctionToPend, void *pvParameter1, uint32_t ulParameter2, BaseType_t *pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;

/**
 * 挂起函数调用
 * @param xFunctionToPend 要挂起的函数
 * @param pvParameter1 函数第一个参数
 * @param ulParameter2 函数第二个参数
 * @param xTicksToWait 等待命令发送的最大滴答数
 * @return 成功返回pdPASS，失败返回pdFALSE
 */
BaseType_t xTimerPendFunctionCall( PendedFunction_t xFunctionToPend, void *pvParameter1, uint32_t ulParameter2, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/**
 * 获取定时器名称
 * @param xTimer 定时器句柄
 * @return 定时器名称
 */
const char * pcTimerGetName( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/**
 * 获取定时器周期
 * @param xTimer 定时器句柄
 * @return 定时器周期（滴答数）
 */
TickType_t xTimerGetPeriod( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/**
 * 获取定时器到期时间
 * @param xTimer 定时器句柄
 * @return 定时器到期时间（滴答数）
 */
TickType_t xTimerGetExpiryTime( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/* Private types -------------------------------------------------------------*/
/* 注：定时器模块没有私有类型定义 */

/* Private variables ---------------------------------------------------------*/
/* 注：定时器模块没有私有变量定义 */

/* Private constants ---------------------------------------------------------*/
/* 注：定时器模块没有私有常量定义 */

/* Private macros ------------------------------------------------------------*/
/* 注：私有宏定义已包含在导出宏中 */

/* Private functions ---------------------------------------------------------*/
/**
 * 创建定时器任务
 * @return 成功返回pdPASS，失败返回pdFAIL
 */
BaseType_t xTimerCreateTimerTask( void ) PRIVILEGED_FUNCTION;

/**
 * 通用定时器命令函数
 * @param xTimer 定时器句柄
 * @param xCommandID 命令ID
 * @param xOptionalValue 可选值
 * @param pxHigherPriorityTaskWoken 高优先级任务唤醒标志指针
 * @param xTicksToWait 等待命令发送的最大滴答数
 * @return 成功返回pdPASS，失败返回pdFAIL
 */
BaseType_t xTimerGenericCommand( TimerHandle_t xTimer, const BaseType_t xCommandID, const TickType_t xOptionalValue, BaseType_t * const pxHigherPriorityTaskWoken, const TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

#ifdef __cplusplus
}
#endif

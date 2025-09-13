/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： LM_task.h
 * 文件标识： 
 * 内容摘要： 任务模块声明
 * 其它说明： 无
 * 当前版本： FreeRTOS V9.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年09月13日
 *
 *******************************************************************************/


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef INC_TASK_H
#define INC_TASK_H

/* Includes ------------------------------------------------------------------*/
#ifndef INC_FREERTOS_H
	#error "include FreeRTOS.h must appear in source files before include task.h"
#endif

#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types ------------------------------------------------------------*/
/**
 * 任务句柄类型定义
 * 通过xTaskCreate返回的任务引用句柄
 */
typedef void * TaskHandle_t;

/**
 * 任务钩子函数原型定义
 * 应用程序任务钩子函数必须符合此原型
 */
typedef BaseType_t (*TaskHookFunction_t)( void * );

/**
 * 任务状态枚举定义
 * 由eTaskGetState函数返回的任务状态
 */
typedef enum
{
	eRunning = 0,	/* 任务正在运行 */
	eReady,			/* 任务处于就绪或待就绪状态 */
	eBlocked,		/* 任务处于阻塞状态 */
	eSuspended,		/* 任务处于挂起状态或具有无限超时的阻塞状态 */
	eDeleted,		/* 任务已被删除但其TCB尚未释放 */
	eInvalid		/* 无效状态值 */
} eTaskState;

/**
 * 通知动作枚举定义
 * vTaskNotify函数调用时可以执行的动作
 */
typedef enum
{
	eNoAction = 0,				/* 通知任务而不更新其通知值 */
	eSetBits,					/* 设置任务通知值中的位 */
	eIncrement,					/* 递增任务的通知值 */
	eSetValueWithOverwrite,		/* 即使任务尚未处理先前的通知，也将任务的通知值设置为特定值 */
	eSetValueWithoutOverwrite	/* 如果任务已读取先前的值，则设置任务的通知值 */
} eNotifyAction;

/**
 * 超时结构体定义
 * 内部使用
 */
typedef struct xTIME_OUT
{
	BaseType_t xOverflowCount;	/* 溢出计数 */
	TickType_t xTimeOnEntering;	/* 进入时的时间 */
} TimeOut_t;

/**
 * 内存区域结构体定义
 * 定义使用MPU时分配给任务的内存范围
 */
typedef struct xMEMORY_REGION
{
	void *pvBaseAddress;		/* 基地址 */
	uint32_t ulLengthInBytes;	/* 字节长度 */
	uint32_t ulParameters;		/* 参数 */
} MemoryRegion_t;

/**
 * 任务参数结构体定义
 * 创建MPU保护任务所需的参数
 */
typedef struct xTASK_PARAMETERS
{
	TaskFunction_t pvTaskCode;				/* 任务入口函数 */
	const char * const pcName;				/* 任务名称 */
	uint16_t usStackDepth;					/* 堆栈深度 */
	void *pvParameters;						/* 任务参数 */
	UBaseType_t uxPriority;					/* 任务优先级 */
	StackType_t *puxStackBuffer;			/* 堆栈缓冲区 */
	MemoryRegion_t xRegions[ portNUM_CONFIGURABLE_REGIONS ]; /* 内存区域配置 */
} TaskParameters_t;

/**
 * 任务状态结构体定义
 * 用于uxTaskGetSystemState函数返回系统中每个任务的状态
 */
typedef struct xTASK_STATUS
{
	TaskHandle_t xHandle;					/* 任务句柄 */
	const char *pcTaskName;					/* 任务名称指针 */
	UBaseType_t xTaskNumber;				/* 任务唯一编号 */
	eTaskState eCurrentState;				/* 任务当前状态 */
	UBaseType_t uxCurrentPriority;			/* 任务当前优先级（可能被继承） */
	UBaseType_t uxBasePriority;				/* 任务基础优先级 */
	uint32_t ulRunTimeCounter;				/* 任务总运行时间 */
	StackType_t *pxStackBase;				/* 堆栈基地址 */
	uint16_t usStackHighWaterMark;			/* 堆栈高水位标记 */
} TaskStatus_t;

/**
 * 睡眠模式状态枚举定义
 * eTaskConfirmSleepModeStatus函数的可能返回值
 */
typedef enum
{
	eAbortSleep = 0,		/* 中止进入睡眠模式 */
	eStandardSleep,			/* 进入标准睡眠模式 */
	eNoTasksWaitingTimeout	/* 进入可由外部中断唤醒的睡眠模式 */
} eSleepModeStatus;

/* Exported constants --------------------------------------------------------*/
/**
 * 空闲任务优先级定义
 * 不能被修改
 */
#define tskIDLE_PRIORITY			( ( UBaseType_t ) 0U )

/**
 * 调度器状态常量定义
 */
#define taskSCHEDULER_SUSPENDED		( ( BaseType_t ) 0 )  /* 调度器已挂起 */
#define taskSCHEDULER_NOT_STARTED	( ( BaseType_t ) 1 )  /* 调度器未启动 */
#define taskSCHEDULER_RUNNING		( ( BaseType_t ) 2 )  /* 调度器正在运行 */

/* Exported macro ------------------------------------------------------------*/
/**
 * 强制上下文切换宏
 */
#define taskYIELD()					portYIELD()

/**
 * 进入临界区宏
 * 禁止抢占式上下文切换
 */
#define taskENTER_CRITICAL()		portENTER_CRITICAL()
#define taskENTER_CRITICAL_FROM_ISR() portSET_INTERRUPT_MASK_FROM_ISR()

/**
 * 退出临界区宏
 */
#define taskEXIT_CRITICAL()			portEXIT_CRITICAL()
#define taskEXIT_CRITICAL_FROM_ISR( x ) portCLEAR_INTERRUPT_MASK_FROM_ISR( x )

/**
 * 禁用所有可屏蔽中断宏
 */
#define taskDISABLE_INTERRUPTS()	portDISABLE_INTERRUPTS()

/**
 * 使能微控制器中断宏
 */
#define taskENABLE_INTERRUPTS()		portENABLE_INTERRUPTS()

/* Exported functions --------------------------------------------------------*/
/* 任务创建API函数 */
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	BaseType_t xTaskCreate(	TaskFunction_t pxTaskCode,
							const char * const pcName,
							const uint16_t usStackDepth,
							void * const pvParameters,
							UBaseType_t uxPriority,
							TaskHandle_t * const pxCreatedTask ) PRIVILEGED_FUNCTION;
	void debugs_test( TaskHandle_t * pxCreatedTask_Debug1 ,TaskHandle_t * pxCreatedTask_Debug2 ,TaskHandle_t * pxCreatedTask_Debug3);						
#endif

#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	TaskHandle_t xTaskCreateStatic(	TaskFunction_t pxTaskCode,
									const char * const pcName,
									const uint32_t ulStackDepth,
									void * const pvParameters,
									UBaseType_t uxPriority,
									StackType_t * const puxStackBuffer,
									StaticTask_t * const pxTaskBuffer ) PRIVILEGED_FUNCTION;
#endif

#if( portUSING_MPU_WRAPPERS == 1 )
	BaseType_t xTaskCreateRestricted( const TaskParameters_t * const pxTaskDefinition, TaskHandle_t *pxCreatedTask ) PRIVILEGED_FUNCTION;
#endif

void vTaskAllocateMPURegions( TaskHandle_t xTask, const MemoryRegion_t * const pxRegions ) PRIVILEGED_FUNCTION;
void vTaskDelete( TaskHandle_t xTaskToDelete ) PRIVILEGED_FUNCTION;

/* 任务控制API函数 */
void vTaskDelay( const TickType_t xTicksToDelay ) PRIVILEGED_FUNCTION;
void vTaskDelayUntil( TickType_t * const pxPreviousWakeTime, const TickType_t xTimeIncrement ) PRIVILEGED_FUNCTION;
BaseType_t xTaskAbortDelay( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;
UBaseType_t uxTaskPriorityGet( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;
UBaseType_t uxTaskPriorityGetFromISR( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;
eTaskState eTaskGetState( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;
void vTaskGetInfo( TaskHandle_t xTask, TaskStatus_t *pxTaskStatus, BaseType_t xGetFreeStackSpace, eTaskState eState ) PRIVILEGED_FUNCTION;
void vTaskPrioritySet( TaskHandle_t xTask, UBaseType_t uxNewPriority ) PRIVILEGED_FUNCTION;
void vTaskSuspend( TaskHandle_t xTaskToSuspend ) PRIVILEGED_FUNCTION;
void vTaskResume( TaskHandle_t xTaskToResume ) PRIVILEGED_FUNCTION;
BaseType_t xTaskResumeFromISR( TaskHandle_t xTaskToResume ) PRIVILEGED_FUNCTION;

/* 调度器控制函数 */
void vTaskStartScheduler( void ) PRIVILEGED_FUNCTION;
void vTaskEndScheduler( void ) PRIVILEGED_FUNCTION;
void vTaskSuspendAll( void ) PRIVILEGED_FUNCTION;
BaseType_t xTaskResumeAll( void ) PRIVILEGED_FUNCTION;

/* 任务工具函数 */
TickType_t xTaskGetTickCount( void ) PRIVILEGED_FUNCTION;
TickType_t xTaskGetTickCountFromISR( void ) PRIVILEGED_FUNCTION;
UBaseType_t uxTaskGetNumberOfTasks( void ) PRIVILEGED_FUNCTION;
char *pcTaskGetName( TaskHandle_t xTaskToQuery ) PRIVILEGED_FUNCTION;
TaskHandle_t xTaskGetHandle( const char *pcNameToQuery ) PRIVILEGED_FUNCTION;
UBaseType_t uxTaskGetStackHighWaterMark( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

/* 任务通知函数 */
BaseType_t xTaskGenericNotify( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, uint32_t *pulPreviousNotificationValue ) PRIVILEGED_FUNCTION;
#define xTaskNotify( xTaskToNotify, ulValue, eAction ) xTaskGenericNotify( ( xTaskToNotify ), ( ulValue ), ( eAction ), NULL )
#define xTaskNotifyAndQuery( xTaskToNotify, ulValue, eAction, pulPreviousNotifyValue ) xTaskGenericNotify( ( xTaskToNotify ), ( ulValue ), ( eAction ), ( pulPreviousNotifyValue ) )

BaseType_t xTaskGenericNotifyFromISR( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, uint32_t *pulPreviousNotificationValue, BaseType_t *pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;
#define xTaskNotifyFromISR( xTaskToNotify, ulValue, eAction, pxHigherPriorityTaskWoken ) xTaskGenericNotifyFromISR( ( xTaskToNotify ), ( ulValue ), ( eAction ), NULL, ( pxHigherPriorityTaskWoken ) )
#define xTaskNotifyAndQueryFromISR( xTaskToNotify, ulValue, eAction, pulPreviousNotificationValue, pxHigherPriorityTaskWoken ) xTaskGenericNotifyFromISR( ( xTaskToNotify ), ( ulValue ), ( eAction ), ( pulPreviousNotificationValue ), ( pxHigherPriorityTaskWoken ) )

BaseType_t xTaskNotifyWait( uint32_t ulBitsToClearOnEntry, uint32_t ulBitsToClearOnExit, uint32_t *pulNotificationValue, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;
#define xTaskNotifyGive( xTaskToNotify ) xTaskGenericNotify( ( xTaskToNotify ), ( 0 ), eIncrement, NULL )
void vTaskNotifyGiveFromISR( TaskHandle_t xTaskToNotify, BaseType_t *pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;
uint32_t ulTaskNotifyTake( BaseType_t xClearCountOnExit, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;
BaseType_t xTaskNotifyStateClear( TaskHandle_t xTask );

/* 调度器内部函数（用于移植目的） */
BaseType_t xTaskIncrementTick( void ) PRIVILEGED_FUNCTION;
void vTaskPlaceOnEventList( List_t * const pxEventList, const TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;
void vTaskPlaceOnUnorderedEventList( List_t * pxEventList, const TickType_t xItemValue, const TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;
void vTaskPlaceOnEventListRestricted( List_t * const pxEventList, TickType_t xTicksToWait, const BaseType_t xWaitIndefinitely ) PRIVILEGED_FUNCTION;
BaseType_t xTaskRemoveFromEventList( const List_t * const pxEventList ) PRIVILEGED_FUNCTION;
BaseType_t xTaskRemoveFromUnorderedEventList( ListItem_t * pxEventListItem, const TickType_t xItemValue ) PRIVILEGED_FUNCTION;
void vTaskSwitchContext( void ) PRIVILEGED_FUNCTION;
TickType_t uxTaskResetEventItemValue( void ) PRIVILEGED_FUNCTION;
TaskHandle_t xTaskGetCurrentTaskHandle( void ) PRIVILEGED_FUNCTION;
void vTaskSetTimeOutState( TimeOut_t * const pxTimeOut ) PRIVILEGED_FUNCTION;
BaseType_t xTaskCheckForTimeOut( TimeOut_t * const pxTimeOut, TickType_t * const pxTicksToWait ) PRIVILEGED_FUNCTION;
void vTaskMissedYield( void ) PRIVILEGED_FUNCTION;
BaseType_t xTaskGetSchedulerState( void ) PRIVILEGED_FUNCTION;
void vTaskPriorityInherit( TaskHandle_t const pxMutexHolder ) PRIVILEGED_FUNCTION;
BaseType_t xTaskPriorityDisinherit( TaskHandle_t const pxMutexHolder ) PRIVILEGED_FUNCTION;
UBaseType_t uxTaskGetTaskNumber( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;
void vTaskSetTaskNumber( TaskHandle_t xTask, const UBaseType_t uxHandle ) PRIVILEGED_FUNCTION;
void vTaskStepTick( const TickType_t xTicksToJump ) PRIVILEGED_FUNCTION;
eSleepModeStatus eTaskConfirmSleepModeStatus( void ) PRIVILEGED_FUNCTION;
void *pvTaskIncrementMutexHeldCount( void ) PRIVILEGED_FUNCTION;

/* Private types -------------------------------------------------------------*/
/* 注：私有类型定义已包含在导出类型中 */

/* Private variables ---------------------------------------------------------*/
/* 注：头文件中不包含私有变量定义 */

/* Private constants ---------------------------------------------------------*/
/* 注：私有常量定义已包含在导出常量中 */

/* Private macros ------------------------------------------------------------*/
/* 注：私有宏定义已包含在导出宏中 */

/* Private functions ---------------------------------------------------------*/
/* 注：私有函数声明已包含在导出函数中 */

#ifdef __cplusplus
}
#endif
#endif /* INC_TASK_H */

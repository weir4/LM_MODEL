/*******************************************************************************
 * ��Ȩ���� (C)2025, CQG
 *
 * �ļ����ƣ� LM_task.h
 * �ļ���ʶ�� 
 * ����ժҪ�� ����ģ������
 * ����˵���� ��
 * ��ǰ�汾�� FreeRTOS V9.0.0
 * ��    �ߣ� Qiguo_Cui                   
 * ������ڣ� 2025��09��13��
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
 * ���������Ͷ���
 * ͨ��xTaskCreate���ص��������þ��
 */
typedef void * TaskHandle_t;

/**
 * �����Ӻ���ԭ�Ͷ���
 * Ӧ�ó��������Ӻ���������ϴ�ԭ��
 */
typedef BaseType_t (*TaskHookFunction_t)( void * );

/**
 * ����״̬ö�ٶ���
 * ��eTaskGetState�������ص�����״̬
 */
typedef enum
{
	eRunning = 0,	/* ������������ */
	eReady,			/* �����ھ����������״̬ */
	eBlocked,		/* ����������״̬ */
	eSuspended,		/* �����ڹ���״̬��������޳�ʱ������״̬ */
	eDeleted,		/* �����ѱ�ɾ������TCB��δ�ͷ� */
	eInvalid		/* ��Ч״ֵ̬ */
} eTaskState;

/**
 * ֪ͨ����ö�ٶ���
 * vTaskNotify��������ʱ����ִ�еĶ���
 */
typedef enum
{
	eNoAction = 0,				/* ֪ͨ�������������ֵ֪ͨ */
	eSetBits,					/* ��������ֵ֪ͨ�е�λ */
	eIncrement,					/* ���������ֵ֪ͨ */
	eSetValueWithOverwrite,		/* ��ʹ������δ������ǰ��֪ͨ��Ҳ�������ֵ֪ͨ����Ϊ�ض�ֵ */
	eSetValueWithoutOverwrite	/* ��������Ѷ�ȡ��ǰ��ֵ�������������ֵ֪ͨ */
} eNotifyAction;

/**
 * ��ʱ�ṹ�嶨��
 * �ڲ�ʹ��
 */
typedef struct xTIME_OUT
{
	BaseType_t xOverflowCount;	/* ������� */
	TickType_t xTimeOnEntering;	/* ����ʱ��ʱ�� */
} TimeOut_t;

/**
 * �ڴ�����ṹ�嶨��
 * ����ʹ��MPUʱ�����������ڴ淶Χ
 */
typedef struct xMEMORY_REGION
{
	void *pvBaseAddress;		/* ����ַ */
	uint32_t ulLengthInBytes;	/* �ֽڳ��� */
	uint32_t ulParameters;		/* ���� */
} MemoryRegion_t;

/**
 * ��������ṹ�嶨��
 * ����MPU������������Ĳ���
 */
typedef struct xTASK_PARAMETERS
{
	TaskFunction_t pvTaskCode;				/* ������ں��� */
	const char * const pcName;				/* �������� */
	uint16_t usStackDepth;					/* ��ջ��� */
	void *pvParameters;						/* ������� */
	UBaseType_t uxPriority;					/* �������ȼ� */
	StackType_t *puxStackBuffer;			/* ��ջ������ */
	MemoryRegion_t xRegions[ portNUM_CONFIGURABLE_REGIONS ]; /* �ڴ��������� */
} TaskParameters_t;

/**
 * ����״̬�ṹ�嶨��
 * ����uxTaskGetSystemState��������ϵͳ��ÿ�������״̬
 */
typedef struct xTASK_STATUS
{
	TaskHandle_t xHandle;					/* ������ */
	const char *pcTaskName;					/* ��������ָ�� */
	UBaseType_t xTaskNumber;				/* ����Ψһ��� */
	eTaskState eCurrentState;				/* ����ǰ״̬ */
	UBaseType_t uxCurrentPriority;			/* ����ǰ���ȼ������ܱ��̳У� */
	UBaseType_t uxBasePriority;				/* ����������ȼ� */
	uint32_t ulRunTimeCounter;				/* ����������ʱ�� */
	StackType_t *pxStackBase;				/* ��ջ����ַ */
	uint16_t usStackHighWaterMark;			/* ��ջ��ˮλ��� */
} TaskStatus_t;

/**
 * ˯��ģʽ״̬ö�ٶ���
 * eTaskConfirmSleepModeStatus�����Ŀ��ܷ���ֵ
 */
typedef enum
{
	eAbortSleep = 0,		/* ��ֹ����˯��ģʽ */
	eStandardSleep,			/* �����׼˯��ģʽ */
	eNoTasksWaitingTimeout	/* ��������ⲿ�жϻ��ѵ�˯��ģʽ */
} eSleepModeStatus;

/* Exported constants --------------------------------------------------------*/
/**
 * �����������ȼ�����
 * ���ܱ��޸�
 */
#define tskIDLE_PRIORITY			( ( UBaseType_t ) 0U )

/**
 * ������״̬��������
 */
#define taskSCHEDULER_SUSPENDED		( ( BaseType_t ) 0 )  /* �������ѹ��� */
#define taskSCHEDULER_NOT_STARTED	( ( BaseType_t ) 1 )  /* ������δ���� */
#define taskSCHEDULER_RUNNING		( ( BaseType_t ) 2 )  /* �������������� */

/* Exported macro ------------------------------------------------------------*/
/**
 * ǿ���������л���
 */
#define taskYIELD()					portYIELD()

/**
 * �����ٽ�����
 * ��ֹ��ռʽ�������л�
 */
#define taskENTER_CRITICAL()		portENTER_CRITICAL()
#define taskENTER_CRITICAL_FROM_ISR() portSET_INTERRUPT_MASK_FROM_ISR()

/**
 * �˳��ٽ�����
 */
#define taskEXIT_CRITICAL()			portEXIT_CRITICAL()
#define taskEXIT_CRITICAL_FROM_ISR( x ) portCLEAR_INTERRUPT_MASK_FROM_ISR( x )

/**
 * �������п������жϺ�
 */
#define taskDISABLE_INTERRUPTS()	portDISABLE_INTERRUPTS()

/**
 * ʹ��΢�������жϺ�
 */
#define taskENABLE_INTERRUPTS()		portENABLE_INTERRUPTS()

/* Exported functions --------------------------------------------------------*/
/* ���񴴽�API���� */
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

/* �������API���� */
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

/* ���������ƺ��� */
void vTaskStartScheduler( void ) PRIVILEGED_FUNCTION;
void vTaskEndScheduler( void ) PRIVILEGED_FUNCTION;
void vTaskSuspendAll( void ) PRIVILEGED_FUNCTION;
BaseType_t xTaskResumeAll( void ) PRIVILEGED_FUNCTION;

/* ���񹤾ߺ��� */
TickType_t xTaskGetTickCount( void ) PRIVILEGED_FUNCTION;
TickType_t xTaskGetTickCountFromISR( void ) PRIVILEGED_FUNCTION;
UBaseType_t uxTaskGetNumberOfTasks( void ) PRIVILEGED_FUNCTION;
char *pcTaskGetName( TaskHandle_t xTaskToQuery ) PRIVILEGED_FUNCTION;
TaskHandle_t xTaskGetHandle( const char *pcNameToQuery ) PRIVILEGED_FUNCTION;
UBaseType_t uxTaskGetStackHighWaterMark( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

/* ����֪ͨ���� */
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

/* �������ڲ�������������ֲĿ�ģ� */
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
/* ע��˽�����Ͷ����Ѱ����ڵ��������� */

/* Private variables ---------------------------------------------------------*/
/* ע��ͷ�ļ��в�����˽�б������� */

/* Private constants ---------------------------------------------------------*/
/* ע��˽�г��������Ѱ����ڵ��������� */

/* Private macros ------------------------------------------------------------*/
/* ע��˽�к궨���Ѱ����ڵ������� */

/* Private functions ---------------------------------------------------------*/
/* ע��˽�к��������Ѱ����ڵ��������� */

#ifdef __cplusplus
}
#endif
#endif /* INC_TASK_H */

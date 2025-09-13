/*******************************************************************************
 * ��Ȩ���� (C)2025, CQG
 *
 * �ļ����ƣ� LM_timers.h
 * �ļ���ʶ�� 
 * ����ժҪ�� ��ʱ��ģ������
 * ����˵���� ��
 * ��ǰ�汾�� FreeRTOS V9.0.0
 * ��    �ߣ� Qiguo_Cui                   
 * ������ڣ� 2025��09��01��
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
 * ��ʱ������ID����
 * �����ڶ�ʱ�������Ϸ���/���յ�����ID
 */
#define tmrCOMMAND_EXECUTE_CALLBACK_FROM_ISR 	( ( BaseType_t ) -2 )  /*< ��ISRִ�лص����� */
#define tmrCOMMAND_EXECUTE_CALLBACK				( ( BaseType_t ) -1 )  /*< ִ�лص����� */
#define tmrCOMMAND_START_DONT_TRACE				( ( BaseType_t ) 0 )   /*< ������ʱ���������٣� */
#define tmrCOMMAND_START					    ( ( BaseType_t ) 1 )   /*< ������ʱ�� */
#define tmrCOMMAND_RESET						( ( BaseType_t ) 2 )   /*< ���ö�ʱ�� */
#define tmrCOMMAND_STOP							( ( BaseType_t ) 3 )   /*< ֹͣ��ʱ�� */
#define tmrCOMMAND_CHANGE_PERIOD				( ( BaseType_t ) 4 )   /*< ���Ķ�ʱ������ */
#define tmrCOMMAND_DELETE						( ( BaseType_t ) 5 )   /*< ɾ����ʱ�� */

#define tmrFIRST_FROM_ISR_COMMAND				( ( BaseType_t ) 6 )   /*< ��һ��ISR���� */
#define tmrCOMMAND_START_FROM_ISR				( ( BaseType_t ) 6 )   /*< ��ISR������ʱ�� */
#define tmrCOMMAND_RESET_FROM_ISR				( ( BaseType_t ) 7 )   /*< ��ISR���ö�ʱ�� */
#define tmrCOMMAND_STOP_FROM_ISR				( ( BaseType_t ) 8 )   /*< ��ISRֹͣ��ʱ�� */
#define tmrCOMMAND_CHANGE_PERIOD_FROM_ISR		( ( BaseType_t ) 9 )   /*< ��ISR���Ķ�ʱ������ */

/**
 * ��ʱ��������Ͷ���
 * ͨ��xTimerCreate���صĶ�ʱ�����þ��
 */
typedef void * TimerHandle_t;

/**
 * ��ʱ���ص�����ԭ�Ͷ���
 * ��ʱ���ص�����������ϴ�ԭ��
 */
typedef void (*TimerCallbackFunction_t)( TimerHandle_t xTimer );

/**
 * ������ԭ�Ͷ���
 * ��xTimerPendFunctionCallFromISR()����һ��ʹ�õĺ���������ϴ�ԭ��
 */
typedef void (*PendedFunction_t)( void *, uint32_t );

/* Exported constants --------------------------------------------------------*/
/* ע����ʱ��ģ��û�е����ĳ������� */

/* Exported macro ------------------------------------------------------------*/
/**
 * ������ʱ����
 * @param xTimer Ҫ�����Ķ�ʱ�����
 * @param xTicksToWait �ȴ�����͵����δ���
 */
#define xTimerStart( xTimer, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_START, ( xTaskGetTickCount() ), NULL, ( xTicksToWait ) )

/**
 * ֹͣ��ʱ����
 * @param xTimer Ҫֹͣ�Ķ�ʱ�����
 * @param xTicksToWait �ȴ�����͵����δ���
 */
#define xTimerStop( xTimer, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_STOP, 0U, NULL, ( xTicksToWait ) )

/**
 * ���Ķ�ʱ�����ں�
 * @param xTimer Ҫ�������ڵĶ�ʱ�����
 * @param xNewPeriod �µĶ�ʱ������
 * @param xTicksToWait �ȴ�����͵����δ���
 */
#define xTimerChangePeriod( xTimer, xNewPeriod, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_CHANGE_PERIOD, ( xNewPeriod ), NULL, ( xTicksToWait ) )

/**
 * ɾ����ʱ����
 * @param xTimer Ҫɾ���Ķ�ʱ�����
 * @param xTicksToWait �ȴ�����͵����δ���
 */
#define xTimerDelete( xTimer, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_DELETE, 0U, NULL, ( xTicksToWait ) )

/**
 * ���ö�ʱ����
 * @param xTimer Ҫ���õĶ�ʱ�����
 * @param xTicksToWait �ȴ�����͵����δ���
 */
#define xTimerReset( xTimer, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_RESET, ( xTaskGetTickCount() ), NULL, ( xTicksToWait ) )

/**
 * ��ISR������ʱ����
 * @param xTimer Ҫ�����Ķ�ʱ�����
 * @param pxHigherPriorityTaskWoken �����ȼ������ѱ�־ָ��
 */
#define xTimerStartFromISR( xTimer, pxHigherPriorityTaskWoken ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_START_FROM_ISR, ( xTaskGetTickCountFromISR() ), ( pxHigherPriorityTaskWoken ), 0U )

/**
 * ��ISRֹͣ��ʱ����
 * @param xTimer Ҫֹͣ�Ķ�ʱ�����
 * @param pxHigherPriorityTaskWoken �����ȼ������ѱ�־ָ��
 */
#define xTimerStopFromISR( xTimer, pxHigherPriorityTaskWoken ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_STOP_FROM_ISR, 0, ( pxHigherPriorityTaskWoken ), 0U )

/**
 * ��ISR���Ķ�ʱ�����ں�
 * @param xTimer Ҫ�������ڵĶ�ʱ�����
 * @param xNewPeriod �µĶ�ʱ������
 * @param pxHigherPriorityTaskWoken �����ȼ������ѱ�־ָ��
 */
#define xTimerChangePeriodFromISR( xTimer, xNewPeriod, pxHigherPriorityTaskWoken ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_CHANGE_PERIOD_FROM_ISR, ( xNewPeriod ), ( pxHigherPriorityTaskWoken ), 0U )

/**
 * ��ISR���ö�ʱ����
 * @param xTimer Ҫ���õĶ�ʱ�����
 * @param pxHigherPriorityTaskWoken �����ȼ������ѱ�־ָ��
 */
#define xTimerResetFromISR( xTimer, pxHigherPriorityTaskWoken ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_RESET_FROM_ISR, ( xTaskGetTickCountFromISR() ), ( pxHigherPriorityTaskWoken ), 0U )

/* Exported functions --------------------------------------------------------*/
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	/**
	 * ������ʱ������̬�ڴ���䣩
	 * @param pcTimerName ��ʱ�����ƣ����ڵ��ԣ�
	 * @param xTimerPeriodInTicks ��ʱ�����ڣ��δ�����
	 * @param uxAutoReload �Ƿ��Զ����أ�pdTRUEΪ�Զ����أ�pdFALSEΪ���ζ�ʱ����
	 * @param pvTimerID ��ʱ����ʶ��
	 * @param pxCallbackFunction ��ʱ���ص�����
	 * @return �ɹ����ض�ʱ�������ʧ�ܷ���NULL
	 */
	TimerHandle_t xTimerCreate(	const char * const pcTimerName,
								const TickType_t xTimerPeriodInTicks,
								const UBaseType_t uxAutoReload,
								void * const pvTimerID,
								TimerCallbackFunction_t pxCallbackFunction ) PRIVILEGED_FUNCTION;
#endif

#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	/**
	 * ������ʱ������̬�ڴ���䣩
	 * @param pcTimerName ��ʱ�����ƣ����ڵ��ԣ�
	 * @param xTimerPeriodInTicks ��ʱ�����ڣ��δ�����
	 * @param uxAutoReload �Ƿ��Զ����أ�pdTRUEΪ�Զ����أ�pdFALSEΪ���ζ�ʱ����
	 * @param pvTimerID ��ʱ����ʶ��
	 * @param pxCallbackFunction ��ʱ���ص�����
	 * @param pxTimerBuffer ��̬��ʱ��������ָ��
	 * @return �ɹ����ض�ʱ�������ʧ�ܷ���NULL
	 */
	TimerHandle_t xTimerCreateStatic(	const char * const pcTimerName,
										const TickType_t xTimerPeriodInTicks,
										const UBaseType_t uxAutoReload,
										void * const pvTimerID,
										TimerCallbackFunction_t pxCallbackFunction,
										StaticTimer_t *pxTimerBuffer ) PRIVILEGED_FUNCTION;
#endif /* configSUPPORT_STATIC_ALLOCATION */

/**
 * ��ȡ��ʱ��ID
 * @param xTimer ��ʱ�����
 * @return ��ʱ��ID
 */
void *pvTimerGetTimerID( const TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/**
 * ���ö�ʱ��ID
 * @param xTimer ��ʱ�����
 * @param pvNewID �µĶ�ʱ��ID
 */
void vTimerSetTimerID( TimerHandle_t xTimer, void *pvNewID ) PRIVILEGED_FUNCTION;

/**
 * ��鶨ʱ���Ƿ��ڻ״̬
 * @param xTimer ��ʱ�����
 * @return �����ʱ�����ڻ״̬���ط���ֵ�����򷵻�pdFALSE
 */
BaseType_t xTimerIsTimerActive( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/**
 * ��ȡ��ʱ���ػ�������
 * @return ��ʱ���ػ�������
 */
TaskHandle_t xTimerGetTimerDaemonTaskHandle( void ) PRIVILEGED_FUNCTION;

/**
 * ��ISR����������
 * @param xFunctionToPend Ҫ����ĺ���
 * @param pvParameter1 ������һ������
 * @param ulParameter2 �����ڶ�������
 * @param pxHigherPriorityTaskWoken �����ȼ������ѱ�־ָ��
 * @return �ɹ�����pdPASS��ʧ�ܷ���pdFALSE
 */
BaseType_t xTimerPendFunctionCallFromISR( PendedFunction_t xFunctionToPend, void *pvParameter1, uint32_t ulParameter2, BaseType_t *pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;

/**
 * ����������
 * @param xFunctionToPend Ҫ����ĺ���
 * @param pvParameter1 ������һ������
 * @param ulParameter2 �����ڶ�������
 * @param xTicksToWait �ȴ�����͵����δ���
 * @return �ɹ�����pdPASS��ʧ�ܷ���pdFALSE
 */
BaseType_t xTimerPendFunctionCall( PendedFunction_t xFunctionToPend, void *pvParameter1, uint32_t ulParameter2, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/**
 * ��ȡ��ʱ������
 * @param xTimer ��ʱ�����
 * @return ��ʱ������
 */
const char * pcTimerGetName( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/**
 * ��ȡ��ʱ������
 * @param xTimer ��ʱ�����
 * @return ��ʱ�����ڣ��δ�����
 */
TickType_t xTimerGetPeriod( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/**
 * ��ȡ��ʱ������ʱ��
 * @param xTimer ��ʱ�����
 * @return ��ʱ������ʱ�䣨�δ�����
 */
TickType_t xTimerGetExpiryTime( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/* Private types -------------------------------------------------------------*/
/* ע����ʱ��ģ��û��˽�����Ͷ��� */

/* Private variables ---------------------------------------------------------*/
/* ע����ʱ��ģ��û��˽�б������� */

/* Private constants ---------------------------------------------------------*/
/* ע����ʱ��ģ��û��˽�г������� */

/* Private macros ------------------------------------------------------------*/
/* ע��˽�к궨���Ѱ����ڵ������� */

/* Private functions ---------------------------------------------------------*/
/**
 * ������ʱ������
 * @return �ɹ�����pdPASS��ʧ�ܷ���pdFAIL
 */
BaseType_t xTimerCreateTimerTask( void ) PRIVILEGED_FUNCTION;

/**
 * ͨ�ö�ʱ�������
 * @param xTimer ��ʱ�����
 * @param xCommandID ����ID
 * @param xOptionalValue ��ѡֵ
 * @param pxHigherPriorityTaskWoken �����ȼ������ѱ�־ָ��
 * @param xTicksToWait �ȴ�����͵����δ���
 * @return �ɹ�����pdPASS��ʧ�ܷ���pdFAIL
 */
BaseType_t xTimerGenericCommand( TimerHandle_t xTimer, const BaseType_t xCommandID, const TickType_t xOptionalValue, BaseType_t * const pxHigherPriorityTaskWoken, const TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

#ifdef __cplusplus
}
#endif

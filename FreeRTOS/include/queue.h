/*******************************************************************************
 * ��Ȩ���� (C)2025, CQG
 *
 * �ļ����ƣ� LM_queue.h
 * �ļ���ʶ�� 
 * ����ժҪ�� ����ģ�鶨��
 * ����˵���� ��
 * ��ǰ�汾�� FreeRTOS V9.0.0
 * ��    �ߣ� Qiguo_Cui                   
 * ������ڣ� 2025��09��01��
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
 * ���о�����Ͷ���
 * ͨ��xQueueCreate���صĶ������þ��
 */
typedef void * QueueHandle_t;

/**
 * ���м�������Ͷ���
 * ͨ��xQueueCreateSet���صĶ��м����þ��
 */
typedef void * QueueSetHandle_t;

/**
 * ���м���Ա��������Ͷ���
 * ���ڲ����򷵻�ֵ������QueueHandle_t��SemaphoreHandle_t�����
 */
typedef void * QueueSetMemberHandle_t;

/* Exported constants --------------------------------------------------------*/
/* ע������ģ��û�е����ĳ������� */

/* Exported macro ------------------------------------------------------------*/
/* �ڲ�ʹ�õķ���λ�ö��� */
#define	queueSEND_TO_BACK		( ( BaseType_t ) 0 )  /*< ���͵�����β�� */
#define	queueSEND_TO_FRONT		( ( BaseType_t ) 1 )  /*< ���͵�����ͷ�� */
#define queueOVERWRITE			( ( BaseType_t ) 2 )  /*< ���Ƕ������� */

/* �ڲ�ʹ�õĶ������Ͷ��� */
#define queueQUEUE_TYPE_BASE				( ( uint8_t ) 0U )  /*< ������������ */
#define queueQUEUE_TYPE_SET					( ( uint8_t ) 0U )  /*< ���м����� */
#define queueQUEUE_TYPE_MUTEX 				( ( uint8_t ) 1U )  /*< ���������� */
#define queueQUEUE_TYPE_COUNTING_SEMAPHORE	( ( uint8_t ) 2U )  /*< �����ź������� */
#define queueQUEUE_TYPE_BINARY_SEMAPHORE	( ( uint8_t ) 3U )  /*< �������ź������� */
#define queueQUEUE_TYPE_RECURSIVE_MUTEX		( ( uint8_t ) 4U )  /*< �ݹ黥�������� */

/**
 * �������к꣨��̬�ڴ���䣩
 * @param uxQueueLength ���г���
 * @param uxItemSize �������С
 */
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	#define xQueueCreate( uxQueueLength, uxItemSize ) xQueueGenericCreate( ( uxQueueLength ), ( uxItemSize ), ( queueQUEUE_TYPE_BASE ) )
#endif

/**
 * �������к꣨��̬�ڴ���䣩
 * @param uxQueueLength ���г���
 * @param uxItemSize �������С
 * @param pucQueueStorage ���д洢������
 * @param pxQueueBuffer ��̬���л�����
 */
#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	#define xQueueCreateStatic( uxQueueLength, uxItemSize, pucQueueStorage, pxQueueBuffer ) xQueueGenericCreateStatic( ( uxQueueLength ), ( uxItemSize ), ( pucQueueStorage ), ( pxQueueBuffer ), ( queueQUEUE_TYPE_BASE ) )
#endif /* configSUPPORT_STATIC_ALLOCATION */

/**
 * ���͵�����ͷ����
 * @param xQueue ���о��
 * @param pvItemToQueue Ҫ���͵�����ָ��
 * @param xTicksToWait �ȴ������δ���
 */
#define xQueueSendToFront( xQueue, pvItemToQueue, xTicksToWait ) xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), ( xTicksToWait ), queueSEND_TO_FRONT )

/**
 * ���͵�����β����
 * @param xQueue ���о��
 * @param pvItemToQueue Ҫ���͵�����ָ��
 * @param xTicksToWait �ȴ������δ���
 */
#define xQueueSendToBack( xQueue, pvItemToQueue, xTicksToWait ) xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), ( xTicksToWait ), queueSEND_TO_BACK )

/**
 * ���͵����к꣨�����ݣ�
 * @param xQueue ���о��
 * @param pvItemToQueue Ҫ���͵�����ָ��
 * @param xTicksToWait �ȴ������δ���
 */
#define xQueueSend( xQueue, pvItemToQueue, xTicksToWait ) xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), ( xTicksToWait ), queueSEND_TO_BACK )

/**
 * ���Ƕ������ݺ�
 * @param xQueue ���о��
 * @param pvItemToQueue Ҫ���͵�����ָ��
 */
#define xQueueOverwrite( xQueue, pvItemToQueue ) xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), 0, queueOVERWRITE )

/**
 * �鿴����ͷ�����ݺ�
 * @param xQueue ���о��
 * @param pvBuffer ���ݽ��ջ�����
 * @param xTicksToWait �ȴ������δ���
 */
#define xQueuePeek( xQueue, pvBuffer, xTicksToWait ) xQueueGenericReceive( ( xQueue ), ( pvBuffer ), ( xTicksToWait ), pdTRUE )

/**
 * ���ն������ݺ�
 * @param xQueue ���о��
 * @param pvBuffer ���ݽ��ջ�����
 * @param xTicksToWait �ȴ������δ���
 */
#define xQueueReceive( xQueue, pvBuffer, xTicksToWait ) xQueueGenericReceive( ( xQueue ), ( pvBuffer ), ( xTicksToWait ), pdFALSE )

/**
 * ��ISR���͵�����ͷ����
 * @param xQueue ���о��
 * @param pvItemToQueue Ҫ���͵�����ָ��
 * @param pxHigherPriorityTaskWoken �����ȼ������ѱ�־ָ��
 */
#define xQueueSendToFrontFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) xQueueGenericSendFromISR( ( xQueue ), ( pvItemToQueue ), ( pxHigherPriorityTaskWoken ), queueSEND_TO_FRONT )

/**
 * ��ISR���͵�����β����
 * @param xQueue ���о��
 * @param pvItemToQueue Ҫ���͵�����ָ��
 * @param pxHigherPriorityTaskWoken �����ȼ������ѱ�־ָ��
 */
#define xQueueSendToBackFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) xQueueGenericSendFromISR( ( xQueue ), ( pvItemToQueue ), ( pxHigherPriorityTaskWoken ), queueSEND_TO_BACK )

/**
 * ��ISR���͵����к�
 * @param xQueue ���о��
 * @param pvItemToQueue Ҫ���͵�����ָ��
 * @param pxHigherPriorityTaskWoken �����ȼ������ѱ�־ָ��
 */
#define xQueueSendFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) xQueueGenericSendFromISR( ( xQueue ), ( pvItemToQueue ), ( pxHigherPriorityTaskWoken ), queueSEND_TO_BACK )

/**
 * ��ISR���Ƕ������ݺ�
 * @param xQueue ���о��
 * @param pvItemToQueue Ҫ���͵�����ָ��
 * @param pxHigherPriorityTaskWoken �����ȼ������ѱ�־ָ��
 */
#define xQueueOverwriteFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) xQueueGenericSendFromISR( ( xQueue ), ( pvItemToQueue ), ( pxHigherPriorityTaskWoken ), queueOVERWRITE )

/**
 * ���ö��к�
 * @param xQueue ���о��
 */
#define xQueueReset( xQueue ) xQueueGenericReset( xQueue, pdFALSE )

/* Exported functions --------------------------------------------------------*/
/**
 * ͨ�ö��з��ͺ���
 * @param xQueue ���о��
 * @param pvItemToQueue Ҫ���͵�����ָ��
 * @param xTicksToWait �ȴ������δ���
 * @param xCopyPosition ����λ�ã�ͷ����β����
 * @return ���ͽ��
 */
BaseType_t xQueueGenericSend( QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait, const BaseType_t xCopyPosition ) PRIVILEGED_FUNCTION;

/**
 * ��ISR�鿴����ͷ�����ݺ���
 * @param xQueue ���о��
 * @param pvBuffer ���ݽ��ջ�����
 * @return �鿴���
 */
BaseType_t xQueuePeekFromISR( QueueHandle_t xQueue, void * const pvBuffer ) PRIVILEGED_FUNCTION;

/**
 * ͨ�ö��н��պ���
 * @param xQueue ���о��
 * @param pvBuffer ���ݽ��ջ�����
 * @param xTicksToWait �ȴ������δ���
 * @param xJustPeek �Ƿ�ֻ�鿴���Ƴ�
 * @return ���ս��
 */
BaseType_t xQueueGenericReceive( QueueHandle_t xQueue, void * const pvBuffer, TickType_t xTicksToWait, const BaseType_t xJustPeek ) PRIVILEGED_FUNCTION;

/**
 * ��ȡ��������Ϣ��������
 * @param xQueue ���о��
 * @return ��Ϣ����
 */
UBaseType_t uxQueueMessagesWaiting( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

/**
 * ��ȡ�����п��ÿռ���������
 * @param xQueue ���о��
 * @return ���ÿռ�����
 */
UBaseType_t uxQueueSpacesAvailable( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

/**
 * ɾ�����к���
 * @param xQueue ���о��
 */
void vQueueDelete( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

/**
 * ��ISRͨ�ö��з��ͺ���
 * @param xQueue ���о��
 * @param pvItemToQueue Ҫ���͵�����ָ��
 * @param pxHigherPriorityTaskWoken �����ȼ������ѱ�־ָ��
 * @param xCopyPosition ����λ�ã�ͷ����β����
 * @return ���ͽ��
 */
BaseType_t xQueueGenericSendFromISR( QueueHandle_t xQueue, const void * const pvItemToQueue, BaseType_t * const pxHigherPriorityTaskWoken, const BaseType_t xCopyPosition ) PRIVILEGED_FUNCTION;

/**
 * ��ISR�����ź�������
 * @param xQueue ���о��
 * @param pxHigherPriorityTaskWoken �����ȼ������ѱ�־ָ��
 * @return ������
 */
BaseType_t xQueueGiveFromISR( QueueHandle_t xQueue, BaseType_t * const pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;

/**
 * ��ISR���ն������ݺ���
 * @param xQueue ���о��
 * @param pvBuffer ���ݽ��ջ�����
 * @param pxHigherPriorityTaskWoken �����ȼ������ѱ�־ָ��
 * @return ���ս��
 */
BaseType_t xQueueReceiveFromISR( QueueHandle_t xQueue, void * const pvBuffer, BaseType_t * const pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;

/**
 * ��ISR�������Ƿ�Ϊ�պ���
 * @param xQueue ���о��
 * @return �Ƿ�Ϊ��
 */
BaseType_t xQueueIsQueueEmptyFromISR( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

/**
 * ��ISR�������Ƿ���������
 * @param xQueue ���о��
 * @return �Ƿ�����
 */
BaseType_t xQueueIsQueueFullFromISR( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

/**
 * ��ISR��ȡ��������Ϣ��������
 * @param xQueue ���о��
 * @return ��Ϣ����
 */
UBaseType_t uxQueueMessagesWaitingFromISR( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

/**
 * ��������������
 * @param ucQueueType ��������
 * @return ���������
 */
QueueHandle_t xQueueCreateMutex( const uint8_t ucQueueType ) PRIVILEGED_FUNCTION;

/**
 * ������̬����������
 * @param ucQueueType ��������
 * @param pxStaticQueue ��̬���л�����
 * @return ���������
 */
QueueHandle_t xQueueCreateMutexStatic( const uint8_t ucQueueType, StaticQueue_t *pxStaticQueue ) PRIVILEGED_FUNCTION;

/**
 * ���������ź�������
 * @param uxMaxCount ������ֵ
 * @param uxInitialCount ��ʼ����ֵ
 * @return �ź������
 */
QueueHandle_t xQueueCreateCountingSemaphore( const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount ) PRIVILEGED_FUNCTION;

/**
 * ������̬�����ź�������
 * @param uxMaxCount ������ֵ
 * @param uxInitialCount ��ʼ����ֵ
 * @param pxStaticQueue ��̬���л�����
 * @return �ź������
 */
QueueHandle_t xQueueCreateCountingSemaphoreStatic( const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount, StaticQueue_t *pxStaticQueue ) PRIVILEGED_FUNCTION;

/**
 * ��ȡ�����������ߺ���
 * @param xSemaphore �ź������
 * @return ������ָ��
 */
void* xQueueGetMutexHolder( QueueHandle_t xSemaphore ) PRIVILEGED_FUNCTION;

/**
 * �ݹ��ȡ����������
 * @param xMutex ���������
 * @param xTicksToWait �ȴ������δ���
 * @return ��ȡ���
 */
BaseType_t xQueueTakeMutexRecursive( QueueHandle_t xMutex, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/**
 * �ݹ��ͷŻ���������
 * @param pxMutex ���������
 * @return �ͷŽ��
 */
BaseType_t xQueueGiveMutexRecursive( QueueHandle_t pxMutex ) PRIVILEGED_FUNCTION;

#if( configQUEUE_REGISTRY_SIZE > 0 )
	/**
	 * ��Ӷ��е�ע�����
	 * @param xQueue ���о��
	 * @param pcName ��������
	 */
	void vQueueAddToRegistry( QueueHandle_t xQueue, const char *pcName ) PRIVILEGED_FUNCTION;

	/**
	 * ��ע����Ƴ����к���
	 * @param xQueue ���о��
	 */
	void vQueueUnregisterQueue( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

	/**
	 * ��ȡ�������ƺ���
	 * @param xQueue ���о��
	 * @return ��������
	 */
	const char *pcQueueGetName( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;
#endif

#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	/**
	 * ͨ�ö��д�����������̬�ڴ���䣩
	 * @param uxQueueLength ���г���
	 * @param uxItemSize �������С
	 * @param ucQueueType ��������
	 * @return ���о��
	 */
	QueueHandle_t xQueueGenericCreate( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, const uint8_t ucQueueType ) PRIVILEGED_FUNCTION;
#endif

#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	/**
	 * ͨ�ö��д�����������̬�ڴ���䣩
	 * @param uxQueueLength ���г���
	 * @param uxItemSize �������С
	 * @param pucQueueStorage ���д洢������
	 * @param pxStaticQueue ��̬���л�����
	 * @param ucQueueType ��������
	 * @return ���о��
	 */
	QueueHandle_t xQueueGenericCreateStatic( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, uint8_t *pucQueueStorage, StaticQueue_t *pxStaticQueue, const uint8_t ucQueueType ) PRIVILEGED_FUNCTION;
#endif

/**
 * �������м�����
 * @param uxEventQueueLength �¼����г���
 * @return ���м����
 */
QueueSetHandle_t xQueueCreateSet( const UBaseType_t uxEventQueueLength ) PRIVILEGED_FUNCTION;

/**
 * ��ӵ����м�����
 * @param xQueueOrSemaphore ���л��ź������
 * @param xQueueSet ���м����
 * @return ��ӽ��
 */
BaseType_t xQueueAddToSet( QueueSetMemberHandle_t xQueueOrSemaphore, QueueSetHandle_t xQueueSet ) PRIVILEGED_FUNCTION;

/**
 * �Ӷ��м��Ƴ�����
 * @param xQueueOrSemaphore ���л��ź������
 * @param xQueueSet ���м����
 * @return �Ƴ����
 */
BaseType_t xQueueRemoveFromSet( QueueSetMemberHandle_t xQueueOrSemaphore, QueueSetHandle_t xQueueSet ) PRIVILEGED_FUNCTION;

/**
 * �Ӷ��м�ѡ����
 * @param xQueueSet ���м����
 * @param xTicksToWait �ȴ������δ���
 * @return ѡ��Ķ��л��ź������
 */
QueueSetMemberHandle_t xQueueSelectFromSet( QueueSetHandle_t xQueueSet, const TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/**
 * ��ISR�Ӷ��м�ѡ����
 * @param xQueueSet ���м����
 * @return ѡ��Ķ��л��ź������
 */
QueueSetMemberHandle_t xQueueSelectFromSetFromISR( QueueSetHandle_t xQueueSet ) PRIVILEGED_FUNCTION;

/* Private types -------------------------------------------------------------*/
/* ע������ģ��û��˽�����Ͷ��� */

/* Private variables ---------------------------------------------------------*/
/* ע������ģ��û��˽�б������� */

/* Private constants ---------------------------------------------------------*/
/* ע������ģ��û��˽�г������� */

/* Private macros ------------------------------------------------------------*/
/* ע��˽�к궨���Ѱ����ڵ������� */

/* Private functions ---------------------------------------------------------*/
/**
 * ���޵ȴ�������Ϣ����
 * @param xQueue ���о��
 * @param xTicksToWait �ȴ������δ���
 * @param xWaitIndefinitely �Ƿ����޵ȴ�
 */
void vQueueWaitForMessageRestricted( QueueHandle_t xQueue, TickType_t xTicksToWait, const BaseType_t xWaitIndefinitely ) PRIVILEGED_FUNCTION;

/**
 * ͨ�ö������ú���
 * @param xQueue ���о��
 * @param xNewQueue �Ƿ�Ϊ�¶���
 * @return ���ý��
 */
BaseType_t xQueueGenericReset( QueueHandle_t xQueue, BaseType_t xNewQueue ) PRIVILEGED_FUNCTION;

/**
 * ���ö��б�ź���
 * @param xQueue ���о��
 * @param uxQueueNumber ���б��
 */
void vQueueSetQueueNumber( QueueHandle_t xQueue, UBaseType_t uxQueueNumber ) PRIVILEGED_FUNCTION;

/**
 * ��ȡ���б�ź���
 * @param xQueue ���о��
 * @return ���б��
 */
UBaseType_t uxQueueGetQueueNumber( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

/**
 * ��ȡ�������ͺ���
 * @param xQueue ���о��
 * @return ��������
 */
uint8_t ucQueueGetQueueType( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

#ifdef __cplusplus
}
#endif

#endif /* QUEUE_H */

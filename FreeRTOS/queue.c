/*******************************************************************************
 * ��Ȩ���� (C)2025, CQG
 *
 * �ļ����ƣ� LM_queue.c
 * �ļ���ʶ�� 
 * ����ժҪ�� ����ģ�鶨��
 * ����˵���� ��
 * ��ǰ�汾�� FreeRTOS V9.0.0
 * ��    �ߣ� Qiguo_Cui                   
 * ������ڣ� 2025��09��01��
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
 * ���нṹ�嶨��
 * ���ڱ�ʾFreeRTOS�еĶ��С��ź����ͻ�����
 */
typedef struct QueueDefinition
{
	int8_t *pcHead;					/*< ָ����д洢����Ŀ�ͷ */
	int8_t *pcTail;					/*< ָ����д洢����ĩβ���ֽ� */
	int8_t *pcWriteTo;				/*< ָ��洢�����е���һ������λ�� */

	union							/* ʹ��������ȷ����������Ľṹ��Ա����ͬʱ���֣���ʡRAM�� */
	{
		int8_t *pcReadFrom;			/*< ���ṹ��������ʱ��ָ�����һ����ȡ�������λ�� */
		UBaseType_t uxRecursiveCallCount;/*< ���ṹ����������ʱ��ά���ݹ黥�������ݹ�"��ȡ"�Ĵ��� */
	} u;

	List_t xTasksWaitingToSend;		/*< �����ȴ����͵��˶��е������б������ȼ����� */
	List_t xTasksWaitingToReceive;	/*< �����ȴ��Ӵ˶��ж�ȡ�������б������ȼ����� */

	volatile UBaseType_t uxMessagesWaiting;/*< ��ǰ�����е���Ŀ���� */
	UBaseType_t uxLength;			/*< ���еĳ��ȣ�����Ϊ�������ɵ���Ŀ�������������ֽ����� */
	UBaseType_t uxItemSize;			/*< ���н����ɵ�ÿ����Ŀ�Ĵ�С */

	volatile int8_t cRxLock;		/*< �洢��������ʱ�Ӷ��н��յ���Ŀ�������Ӷ������Ƴ��� */
	volatile int8_t cTxLock;		/*< �洢��������ʱ���䵽���е���Ŀ��������ӵ����У� */

	#if( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
		uint8_t ucStaticallyAllocated;	/*< �������ʹ�õ��ڴ��Ǿ�̬����ģ�������ΪpdTRUE��ȷ�����᳢���ͷ��ڴ� */
	#endif

	#if ( configUSE_QUEUE_SETS == 1 )
		struct QueueDefinition *pxQueueSetContainer; /*< ���м�����ָ�� */
	#endif

	#if ( configUSE_TRACE_FACILITY == 1 )
		UBaseType_t uxQueueNumber;	/*< ���б�� */
		uint8_t ucQueueType;		/*< �������� */
	#endif

} xQUEUE;

/**
 * �������Ͷ���
 * ������ɰ汾�ļ�����
 */
typedef xQUEUE Queue_t;

#if ( configQUEUE_REGISTRY_SIZE > 0 )
	/**
	 * ����ע�����ṹ�嶨��
	 * ����Ϊÿ�����з������ƣ�ʹ�ں˸�֪���Ը����û��Ѻ�
	 */
	typedef struct QUEUE_REGISTRY_ITEM
	{
		const char *pcQueueName; /*< �������� */
		QueueHandle_t xHandle;   /*< ���о�� */
	} xQueueRegistryItem;

	/**
	 * ����ע��������Ͷ���
	 * ������ɰ汾�ļ�����
	 */
	typedef xQueueRegistryItem QueueRegistryItem_t;
#endif /* configQUEUE_REGISTRY_SIZE */

/* Exported constants --------------------------------------------------------*/
/**
 * ��������״̬��������
 */
#define queueUNLOCKED					( ( int8_t ) -1 )  /*< ����δ����״̬ */
#define queueLOCKED_UNMODIFIED			( ( int8_t ) 0 )   /*< ����������δ�޸�״̬ */

/**
 * ��������س�������
 */
#define queueQUEUE_IS_MUTEX				NULL              /*< ��ʾ����ʵ�����ǻ������ı�� */
#define queueMUTEX_GIVE_BLOCK_TIME		 ( ( TickType_t ) 0U ) /*< ��������������ʱ�� */

/**
 * �ź�����س�������
 */
#define queueSEMAPHORE_QUEUE_ITEM_LENGTH ( ( UBaseType_t ) 0 ) /*< �ź���������ȣ��ź������洢���ݣ� */

/* Exported macro ------------------------------------------------------------*/
#if( configUSE_PREEMPTION == 0 )
	/* ���ʹ��Э������������Ӧ��Ϊ�����˸������ȼ��������ִ�� yield */
	#define queueYIELD_IF_USING_PREEMPTION()  /*< Э���������²����������л� */
#else
	#define queueYIELD_IF_USING_PREEMPTION() portYIELD_WITHIN_API()  /*< ��ռʽ�������½��������л� */
#endif

/**
 * ����������
 * �������пɷ�ֹISR���ʶ����¼��б�
 * @param pxQueue Ҫ�����Ķ���ָ��
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
/* ע������ģ���API������queue.h���������˴����ظ� */

/* Private types -------------------------------------------------------------*/
/* ע��˽�����Ͷ����Ѱ����ڵ��������� */

/* Private variables ---------------------------------------------------------*/
#if ( configQUEUE_REGISTRY_SIZE > 0 )
	/**
	 * ����ע�������
	 * ����ע���ֻ��һ�������ں˸�֪��������λ���нṹ�Ļ���
	 */
	PRIVILEGED_DATA QueueRegistryItem_t xQueueRegistry[ configQUEUE_REGISTRY_SIZE ];
#endif /* configQUEUE_REGISTRY_SIZE */

/* Private constants ---------------------------------------------------------*/
/* ע��˽�г��������Ѱ����ڵ��������� */

/* Private macros ------------------------------------------------------------*/
/* ע��˽�к궨���Ѱ����ڵ������� */

/* Private functions ---------------------------------------------------------*/
/**
 * ��������
 * ������prvLockQueue���������Ķ���
 * �������в�����ֹISR�������ӻ�ɾ����Ŀ��������ֹISR�Ӷ����¼��б���ɾ������
 * ���ISR���ֶ��б�����������������Ӧ�Ķ�������������ʾ������Ҫȡ����������
 * �����н���ʱ��������Щ����������ȡ�ʵ��Ĳ���
 * @param pxQueue Ҫ�����Ķ���ָ��
 */
static void prvUnlockQueue( Queue_t * const pxQueue ) PRIVILEGED_FUNCTION;

/**
 * �������Ƿ�Ϊ��
 * ʹ���ٽ���ȷ���������Ƿ����κ�����
 * @param pxQueue Ҫ���Ķ���ָ��
 * @return �������Ϊ�շ���pdTRUE�����򷵻�pdFALSE
 */
static BaseType_t prvIsQueueEmpty( const Queue_t *pxQueue ) PRIVILEGED_FUNCTION;

/**
 * �������Ƿ�����
 * ʹ���ٽ���ȷ���������Ƿ��пռ�
 * @param pxQueue Ҫ���Ķ���ָ��
 * @return ���û�пռ䷵��pdTRUE�����򷵻�pdFALSE
 */
static BaseType_t prvIsQueueFull( const Queue_t *pxQueue ) PRIVILEGED_FUNCTION;

/**
 * �����ݸ��Ƶ�����
 * ����Ŀ���Ƶ����е�ǰ�˻���
 * @param pxQueue Ŀ�����ָ��
 * @param pvItemToQueue Ҫ���л�����Ŀָ��
 * @param xPosition ����λ�ã�ǰ�˻��ˣ�
 * @return ���ƽ��
 */
static BaseType_t prvCopyDataToQueue( Queue_t * const pxQueue, const void *pvItemToQueue, const BaseType_t xPosition ) PRIVILEGED_FUNCTION;

/**
 * �Ӷ��и�������
 * �Ӷ����и���һ����Ŀ
 * @param pxQueue Դ����ָ��
 * @param pvBuffer Ŀ�껺����ָ��
 */
static void prvCopyDataFromQueue( Queue_t * const pxQueue, void * const pvBuffer ) PRIVILEGED_FUNCTION;

#if ( configUSE_QUEUE_SETS == 1 )
	/**
	 * ֪ͨ���м�����
	 * �������Ƿ��Ƕ��м��ĳ�Ա������ǣ���֪ͨ���м����а�������
	 * @param pxQueue ����ָ��
	 * @param xCopyPosition ����λ��
	 * @return ֪ͨ���
	 */
	static BaseType_t prvNotifyQueueSetContainer( const Queue_t * const pxQueue, const BaseType_t xCopyPosition ) PRIVILEGED_FUNCTION;
#endif

/**
 * ��ʼ���¶���
 * �ھ�̬��̬����Queue_t�ṹ����ã������ṹ�ĳ�Ա
 * @param uxQueueLength ���г���
 * @param uxItemSize ��Ŀ��С
 * @param pucQueueStorage ���д洢����ָ��
 * @param ucQueueType ��������
 * @param pxNewQueue �¶���ָ��
 */
static void prvInitialiseNewQueue( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, uint8_t *pucQueueStorage, const uint8_t ucQueueType, Queue_t *pxNewQueue ) PRIVILEGED_FUNCTION;

#if( configUSE_MUTEXES == 1 )
	/**
	 * ��ʼ��������
	 * ��������һ���������͵Ķ��С�����������ʱ�����ȴ������У�Ȼ�����prvInitialiseMutex()����������Ϊ������
	 * @param pxNewQueue �¶���ָ��
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
�������ƣ�xQueueGenericReset
����������    
    ������Ϣ��������ʼ״̬����ѡ���Ƿ��ʼ���ȴ��б������з��½�����᳢�Ի��ѵȴ�д�������
���������   
    xQueue: Ҫ���õ���Ϣ���о����ʵ��Ϊָ����нṹ���ָ��
    xNewQueue: ��ʶ�Ƿ�Ϊ�´������У�pdTRUE��ʾ�¶��У�pdFALSE��ʾ�Ѵ��ڶ���
���������    
    ��
�� �� ֵ��    
    �̶�����pdPASS����ʾ�����ɹ���Ϊ��ǰ���ݶ�������
����˵����    
    - ���ò����������ö�дָ�롢���м�������״̬
    - ���ڷ��¶��У������Ƿ�������������д��ȴ��б������Ի���һ������
    - ʹ���ٽ���������������
�޸�����      �汾��          �޸���            �޸�����
------------------------------------------------------------------------------
2025/09/02     V1.00          Qiguo_Cui          ����
*******************************************************************************/
BaseType_t xQueueGenericReset( QueueHandle_t xQueue, BaseType_t xNewQueue )
{
    /* �����о��ת��Ϊ���нṹ��ָ�� */
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;

    /* ����ȷ������ָ����Ч */
    configASSERT( pxQueue );

    /* �����ٽ����������в��� */
    taskENTER_CRITICAL();
    {
        /* ����βָ�룺ָ��洢����ĩβ��ͷָ�� + �ܳ���*��Ŀ��С�� */
        pxQueue->pcTail = pxQueue->pcHead + ( pxQueue->uxLength * pxQueue->uxItemSize );
        
        /* ��ն����еȴ�����Ϣ���� */
        pxQueue->uxMessagesWaiting = ( UBaseType_t ) 0U;
        
        /* ����д��λ��ָ�룺ָ�����ͷ�� */
        pxQueue->pcWriteTo = pxQueue->pcHead;
        
        /* ���ö�ȡλ��ָ�룺ָ����е�����һ��Ԫ��λ�ã�ʵ��ѭ�����У� */
        pxQueue->u.pcReadFrom = pxQueue->pcHead + ( ( pxQueue->uxLength - ( UBaseType_t ) 1U ) * pxQueue->uxItemSize );
        
        /* �����������������ȡ������ */
        pxQueue->cRxLock = queueUNLOCKED;
        
        /* ����������������д������� */
        pxQueue->cTxLock = queueUNLOCKED;

        /* ���ݶ������ͽ��в��컯���� */
        if( xNewQueue == pdFALSE )
        {
            /* ���½����У�����ȴ��е����� */
            
            /* ��鷢�͵ȴ��б��ȴ�д��������Ƿ�ǿ� */
            if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
            {
                /* �ӷ��͵ȴ��б����Ƴ�һ�����񲢾��� */
                if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                {
                    /* �������������������ȼ����ߣ��򴥷�������� */
                    queueYIELD_IF_USING_PREEMPTION();
                }
                else
                {
                    /* �շ�֧�����ڸ����ʲ��ԣ� */
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                /* �շ�֧�����ڸ����ʲ��ԣ� */
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            /* �½����У���ʼ������ȴ��б� */
            vListInitialise( &( pxQueue->xTasksWaitingToSend ) );
            vListInitialise( &( pxQueue->xTasksWaitingToReceive ) );
        }
    }
    /* �˳��ٽ��� */
    taskEXIT_CRITICAL();

    /* ���ع̶��ɹ�ֵ����ǰ���ݣ� */
    return pdPASS;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
�������ƣ�xQueueGenericCreateStatic
����������    
    ʹ�þ�̬�ڴ���䷽ʽ������Ϣ���У���Ҫ�ṩ���д洢���Ͷ��п��ƿ��ڴ�
���������   
    uxQueueLength: ���г��ȣ��������ɵ���Ϣ������
    uxItemSize: ÿ��������Ŀ�Ĵ�С�����ֽ�Ϊ��λ��
    pucQueueStorage: ָ����д洢����ָ�루�û��ṩ�ľ�̬�ڴ�飩
    pxStaticQueue: ָ��̬���п��ƿ��ָ�루�û��ṩ�ľ�̬�ڴ棩
    ucQueueType: �������ͱ�ʶ����ͨ���С��ź����ȣ�
���������    
    ��
�� �� ֵ��    
    �ɹ��������ض��о��(QueueHandle_t)��ʧ�ܷ���NULL
����˵����    
    - �˺�������configSUPPORT_STATIC_ALLOCATIONΪ1ʱ����
    - ��Ҫ�û�Ԥ�ȷ�����д洢���Ͷ��п��ƿ��ڴ�
    - ���д洢����С����ΪuxQueueLength * uxItemSize�ֽ�
    - ����ж��������Ч�Զ��Լ��
�޸�����      �汾��          �޸���            �޸�����
------------------------------------------------------------------------------
2025/09/02     V1.00          Qiguo_Cui          ����
*******************************************************************************/
#if( configSUPPORT_STATIC_ALLOCATION == 1 )

QueueHandle_t xQueueGenericCreateStatic( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, uint8_t *pucQueueStorage, StaticQueue_t *pxStaticQueue, const uint8_t ucQueueType )
{
    /* ��������ָ�� */
    Queue_t *pxNewQueue;

    /* ���Լ�飺���г��ȱ������0 */
    configASSERT( uxQueueLength > ( UBaseType_t ) 0 );

    /* ���Լ�飺��̬���п��ƿ�ָ�벻��Ϊ�� */
    configASSERT( pxStaticQueue != NULL );

    /* ���Լ�飺�����Ŀ��С��Ϊ0�������ṩ�洢���������Ŀ��СΪ0�������ṩ�洢�� */
    configASSERT( !( ( pucQueueStorage != NULL ) && ( uxItemSize == 0 ) ) );
    configASSERT( !( ( pucQueueStorage == NULL ) && ( uxItemSize != 0 ) ) );

    /* ���Զ��ԣ����StaticQueue_t�ṹ���С�Ƿ���Queue_tһ�� */
    #if( configASSERT_DEFINED == 1 )
    {
        /* ��ȡStaticQueue_t�ṹ���С�����бȽ� */
        volatile size_t xSize = sizeof( StaticQueue_t );
        configASSERT( xSize == sizeof( Queue_t ) );
    }
    #endif /* configASSERT_DEFINED */

    /* ����̬���п��ƿ�ת��ΪQueue_t����ָ�� */
    /* lintע�ͣ���Ѱ��������ת��������ģ���Ϊ�ṹ���������ͬ�Ķ��뷽ʽ���Ҵ�С��ͨ�����Լ�� */
    pxNewQueue = ( Queue_t * ) pxStaticQueue;

    /* ������ָ���Ƿ���Ч���ǿգ� */
    if( pxNewQueue != NULL )
    {
        /* ���֧�ֶ�̬���䣬��Ǵ˶���Ϊ��̬���� */
        #if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
        {
            /* ���þ�̬�����־���Ա����ɾ������ʱ֪������Ҫ�ͷ��ڴ� */
            pxNewQueue->ucStaticallyAllocated = pdTRUE;
        }
        #endif /* configSUPPORT_DYNAMIC_ALLOCATION */

        /* ��ʼ���¶��У����ö��г��ȡ���Ŀ��С���洢��ָ�롢���͵Ȳ��� */
        prvInitialiseNewQueue( uxQueueLength, uxItemSize, pucQueueStorage, ucQueueType, pxNewQueue );
    }

    /* ���ش����Ķ��о��������ΪNULL�������ʧ�ܣ� */
    return pxNewQueue;
}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

/*******************************************************************************
�������ƣ�xQueueGenericCreate
����������    
    ʹ�ö�̬�ڴ���䷽ʽ������Ϣ���У��Զ�������д洢���Ϳ��ƿ������ڴ�
���������   
    uxQueueLength: ���г��ȣ��������ɵ���Ϣ������
    uxItemSize: ÿ��������Ŀ�Ĵ�С�����ֽ�Ϊ��λ��
    ucQueueType: �������ͱ�ʶ����ͨ���С��ź������������ȣ�
���������    
    ��
�� �� ֵ��    
    �ɹ��������ض��о��(QueueHandle_t)��ʧ�ܷ���NULL
����˵����    
    - �˺�������configSUPPORT_DYNAMIC_ALLOCATIONΪ1ʱ����
    - �����ڲ����Զ����������ڴ沢����
    - ����ڴ����ʧ�ܣ��򷵻�NULL
    - ������Ŀ��СΪ0�Ķ��У����ź��������������洢��
�޸�����      �汾��          �޸���            �޸�����
------------------------------------------------------------------------------
2025/09/02     V1.00          Qiguo_Cui          ����
*******************************************************************************/
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

QueueHandle_t xQueueGenericCreate( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, const uint8_t ucQueueType )
{
    /* ��������ָ�� */
    Queue_t *pxNewQueue;
    
    /* �������д洢�������ֽ��� */
    size_t xQueueSizeInBytes;
    
    /* ����ָ����д洢����ָ�� */
    uint8_t *pucQueueStorage;

    /* ���Լ�飺���г��ȱ������0 */
    configASSERT( uxQueueLength > ( UBaseType_t ) 0 );

    /* ������д洢�������ֽ��� */
    if( uxItemSize == ( UBaseType_t ) 0 )
    {
        /* ��Ŀ��СΪ0�����ź�����������Ҫ����洢�� */
        xQueueSizeInBytes = ( size_t ) 0;
    }
    else
    {
        /* ����洢���ܴ�С�����г��� �� ÿ����Ŀ�Ĵ�С */
        xQueueSizeInBytes = ( size_t ) ( uxQueueLength * uxItemSize ); /*lint !e961 MISRA�쳣��ĳЩ�˿�������ת��������� */
    }

    /* �����ڴ棺���п��ƽṹ + ���д洢����һ���Է��䣩 */
    pxNewQueue = ( Queue_t * ) pvPortMalloc( sizeof( Queue_t ) + xQueueSizeInBytes );

    /* ����ڴ��Ƿ����ɹ� */
    if( pxNewQueue != NULL )
    {
        /* ����洢��λ�ã��������п��ƽṹ��ָ��洢����ʼλ�� */
        pucQueueStorage = ( ( uint8_t * ) pxNewQueue ) + sizeof( Queue_t );

        /* ���֧�־�̬���䣬��Ǵ˶���Ϊ��̬���� */
        #if( configSUPPORT_STATIC_ALLOCATION == 1 )
        {
            /* ���ö�̬�����־���Ա����ɾ������ʱ֪����Ҫ�ͷ��ڴ� */
            pxNewQueue->ucStaticallyAllocated = pdFALSE;
        }
        #endif /* configSUPPORT_STATIC_ALLOCATION */

        /* ��ʼ���¶��У����ö��г��ȡ���Ŀ��С���洢��ָ�롢���͵Ȳ��� */
        prvInitialiseNewQueue( uxQueueLength, uxItemSize, pucQueueStorage, ucQueueType, pxNewQueue );
    }

    /* ���ش����Ķ��о��������ΪNULL����ڴ����ʧ�ܣ� */
    return pxNewQueue;
}

#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
/*-----------------------------------------------------------*/

/*******************************************************************************
�������ƣ�prvInitialiseNewQueue
����������    
    ��ʼ���¶��еĸ�����Ա���������ö��еĻ������Ժʹ洢���򣬲��������ú�����ɳ�ʼ��
���������   
    uxQueueLength: ���г��ȣ��������ɵ���Ϣ������
    uxItemSize: ÿ��������Ŀ�Ĵ�С�����ֽ�Ϊ��λ��
    pucQueueStorage: ָ����д洢����ָ��
    ucQueueType: �������ͱ�ʶ����ͨ���С��ź������������ȣ�
    pxNewQueue: ָ��Ҫ��ʼ���Ķ��п��ƽṹ��ָ��
���������    
    �ޣ�ֱ���޸�pxNewQueueָ��Ķ��нṹ��
�� �� ֵ��    
    ��
����˵����    
    - �˺���Ϊ��̬���������ڵ�ǰ�ļ��ڿɼ�
    - �������Ŀ��С�Ƿ�Ϊ0�����ò�ͬ�Ĵ洢��ָ��
    - ����xQueueGenericReset��ɶ��е����ճ�ʼ��
    - ��������ѡ�����ö������ͺͶ��м�����
�޸�����      �汾��          �޸���            �޸�����
------------------------------------------------------------------------------
2025/09/02     V1.00          Qiguo_Cui          ����
*******************************************************************************/
static void prvInitialiseNewQueue( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, uint8_t *pucQueueStorage, const uint8_t ucQueueType, Queue_t *pxNewQueue )
{
    /* ��ֹ���������棺��configUSE_TRACE_FACILITYδ����Ϊ1ʱ��ucQueueType����δʹ�� */
    ( void ) ucQueueType;

    /* ������Ŀ��С���ö���ͷָ�� */
    if( uxItemSize == ( UBaseType_t ) 0 )
    {
        /* ��Ŀ��СΪ0�����ź�������û��Ϊ���д洢������RAM
           �����ܽ�pcHead����ΪNULL����ΪNULL������ʾ���б������������ļ�
           ��ˣ���pcHead����Ϊָ����б��������ֵ����ֵ��֪���ڴ�ӳ���� */
        pxNewQueue->pcHead = ( int8_t * ) pxNewQueue;
    }
    else
    {
        /* ��Ŀ��С��Ϊ0����ͷָ������Ϊ���д洢������ʼλ�� */
        pxNewQueue->pcHead = ( int8_t * ) pucQueueStorage;
    }

    /* ��ʼ�����г�Ա����������Ͷ��������� */
    pxNewQueue->uxLength = uxQueueLength;    /* ���ö��г��� */
    pxNewQueue->uxItemSize = uxItemSize;     /* ����ÿ����Ŀ�Ĵ�С */
    
    /* ����ͨ�����ú�����ʼ������״̬��pdTRUE��ʾ���¶��У� */
    ( void ) xQueueGenericReset( pxNewQueue, pdTRUE );

    /* ��������˸��ٹ��ܣ����ö������� */
    #if ( configUSE_TRACE_FACILITY == 1 )
    {
        pxNewQueue->ucQueueType = ucQueueType;
    }
    #endif /* configUSE_TRACE_FACILITY */

    /* ��������˶��м����ܣ���ʼ�����м�����ָ��ΪNULL */
    #if( configUSE_QUEUE_SETS == 1 )
    {
        pxNewQueue->pxQueueSetContainer = NULL;
    }
    #endif /* configUSE_QUEUE_SETS */

    /* ���д������ٺ꣨���ڵ��Ժ����ܷ����� */
    traceQUEUE_CREATE( pxNewQueue );
}
/*-----------------------------------------------------------*/

/*******************************************************************************
�������ƣ�prvInitialiseMutex
����������    
    ��ʼ�����������е��������ԣ����û����������ߡ����ͺ͵ݹ���������������ڿ���״̬
���������   
    pxNewQueue: ָ��Ҫ��ʼ���Ļ��������е�ָ��
���������    
    �ޣ�ֱ���޸�pxNewQueueָ��Ļ��������нṹ��
�� �� ֵ��    
    ��
����˵����    
    - �˺���Ϊ��̬���������ڵ�ǰ�ļ��ڿɼ�
    - ����configUSE_MUTEXESΪ1ʱ����
    - �Ḳ��ͨ�ö��г�ʼ���е�ĳЩ��Ա������Ӧ����������������
    - ����������ʼ��Ϊ����״̬��ͨ������һ������Ϣ��
�޸�����      �汾��          �޸���            �޸�����
------------------------------------------------------------------------------
2025/09/02     V1.00          Qiguo_Cui          ����
*******************************************************************************/
#if( configUSE_MUTEXES == 1 )

static void prvInitialiseMutex( Queue_t *pxNewQueue )
{
    /* ������ָ���Ƿ���Ч */
    if( pxNewQueue != NULL )
    {
        /* ���д��������Ѿ�Ϊͨ�ö�����ȷ���������ж��нṹ��Ա��
           ���˺������ڴ�������������Ҫ������Щ��Ҫ��ͬ���õĳ�Ա -
           �ر������ȼ��̳��������Ϣ�� */
        
        /* ��ʼ��������������Ϊ�գ���ǰ��������У� */
        pxNewQueue->pxMutexHolder = NULL;
        
        /* ���ö�������Ϊ������ */
        pxNewQueue->uxQueueType = queueQUEUE_IS_MUTEX;

        /* ����ǵݹ黥��������ʼ���ݹ���ü���Ϊ0 */
        pxNewQueue->u.uxRecursiveCallCount = 0;

        /* ���ٻ����������¼� */
        traceCREATE_MUTEX( pxNewQueue );

        /* ͨ������з��Ϳ���Ϣ������������ʼ��Ϊ����״̬
           ʹ��queueSEND_TO_BACKȷ������Ԥ�ڷ�ʽ����
           ��ʱ����Ϊ0����ʾ���ȴ� */
        ( void ) xQueueGenericSend( pxNewQueue, NULL, ( TickType_t ) 0U, queueSEND_TO_BACK );
    }
    else
    {
        /* ����������ʧ�ܸ��� */
        traceCREATE_MUTEX_FAILED();
    }
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

/*******************************************************************************
�������ƣ�xQueueCreateMutex
����������    
    ������̬��������ʹ�ö�̬�ڴ���䷽ʽ����ʼ�����������������Բ����ػ��������
���������   
    ucQueueType: �������ͱ�ʶ���������ֲ�ͬ���͵Ļ�������
���������    
    ��
�� �� ֵ��    
    �ɹ��������ػ��������(QueueHandle_t)��ʧ�ܷ���NULL
����˵����    
    - �˺�������configUSE_MUTEXES��configSUPPORT_DYNAMIC_ALLOCATION��Ϊ1ʱ����
    - ʹ�ö�̬�ڴ���䴴��������������Ԥ�ȷ����ڴ�
    - ��������һ������Ķ��У�����Ϊ1����Ŀ��СΪ0
    - �����ɹ��󣬻��������ڿ���״̬
�޸�����      �汾��          �޸���            �޸�����
------------------------------------------------------------------------------
2025/09/02     V1.00          Qiguo_Cui          ����
*******************************************************************************/
#if( ( configUSE_MUTEXES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

QueueHandle_t xQueueCreateMutex( const uint8_t ucQueueType )
{
    /* ��������ָ�� */
    Queue_t *pxNewQueue;
    
    /* ���廥�����Ĺ̶�����������Ϊ1����Ŀ��СΪ0 */
    const UBaseType_t uxMutexLength = ( UBaseType_t ) 1, uxMutexSize = ( UBaseType_t ) 0;

    /* ʹ��ͨ�ö��д������������������нṹ
       �������ǳ���Ϊ1����Ŀ��СΪ0��������� */
    pxNewQueue = ( Queue_t * ) xQueueGenericCreate( uxMutexLength, uxMutexSize, ucQueueType );
    
    /* ��ʼ�����������������ԣ������ߡ��ݹ�����ȣ� */
    prvInitialiseMutex( pxNewQueue );

    /* ���ش����Ļ��������������ΪNULL�������ʧ�ܣ� */
    return pxNewQueue;
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * ��������: xQueueCreateMutexStatic
 * ��������: ������̬����Ļ��������С��ú������ڴ���һ����̬����Ļ�������ʹ��Ԥ�ȷ�����ڴ�ռ䣬
 *           �����˶�̬�ڴ���䣬�����ڶ��ڴ�������ϸ�Ҫ���Ƕ��ʽϵͳ��
 * �������: 
 *   - ucQueueType: �������ͱ�ʶ�����������ֲ�ͬ���͵Ķ��У��绥�������ź����ȣ�
 *   - pxStaticQueue: ָ��̬���нṹ��ָ�룬�ṩ����������ڴ�ռ�
 * �������: ��
 * �� �� ֵ: 
 *   - �ɹ�: ���ش����Ļ��������о��
 *   - ʧ��: ����NULL����������Ч���ʼ��ʧ��ʱ��
 * ����˵��: 
 *   1. �˺������� configUSE_MUTEXES �� configSUPPORT_STATIC_ALLOCATION ��Ϊ 1 ʱ����
 *   2. ���������еĳ��ȹ̶�Ϊ1����СΪ0����Ϊ����������Ҫ�洢ʵ������
 *   3. ���������� prvInitialiseMutex �����Ի��������г�ʼ��
 * �޸�����      �汾��          �޸���            �޸�����
 * ----------------------------------------------------------------------------
 * 2024/06/02     V1.00          ChatGPT           �����������ϸע��
 *******************************************************************************/
#if( ( configUSE_MUTEXES == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )

QueueHandle_t xQueueCreateMutexStatic( const uint8_t ucQueueType, StaticQueue_t *pxStaticQueue )
{
Queue_t *pxNewQueue;                                       /* �����¶���ָ�� */
const UBaseType_t uxMutexLength = ( UBaseType_t ) 1;       /* ���������г��ȹ̶�Ϊ1 */
const UBaseType_t uxMutexSize = ( UBaseType_t ) 0;         /* �������������СΪ0�����洢���ݣ� */

    /* ��ֹ���������棺�� configUSE_TRACE_FACILITY ������ 1 ʱ������δʹ�ò����ľ��� */
    ( void ) ucQueueType;

    /* ����ͨ�ö��д�������������̬����
       ����˵����
       - uxMutexLength: ���г��ȣ��������̶�Ϊ1��
       - uxMutexSize: �������С������������Ҫ�洢���ݣ���Ϊ0��
       - NULL: ���д洢����ָ�루����������Ҫ���ݴ洢����ΪNULL��
       - pxStaticQueue: ��̬���нṹָ��
       - ucQueueType: �������ͱ�ʶ�� */
    pxNewQueue = ( Queue_t * ) xQueueGenericCreateStatic( uxMutexLength, uxMutexSize, NULL, pxStaticQueue, ucQueueType );

    /* ��ʼ���������ض����� */
    prvInitialiseMutex( pxNewQueue );

    /* ���ش����Ļ��������о�� */
    return pxNewQueue;
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xQueueGetMutexHolder
 * ������������ȡ�������ĵ�ǰ������������
 *           �˺������ڲ�ѯָ���������ĵ�ǰ�����ߣ���xSemaphoreGetMutexHolder���ڲ�ʵ��
 * ���������
 *   - xSemaphore: �ź��������ʵ����Ӧ���ǻ������ľ��
 * �����������
 * �� �� ֵ��
 *   - void*: �����������ߵ���������������ǻ�������û�г������򷵻�NULL
 * ����˵����
 *   - �˺�����xSemaphoreGetMutexHolder()���ã���Ӧֱ�ӵ���
 *   - ���ٽ�����ִ�У�ȷ����ѯ������ԭ����
 *   - ֻ������ȷ�����������Ƿ��ǻ����������ߣ����ʺ�����ȷ�����������
 *   - ��Ϊ�����߿������ٽ����˳��ͺ�������֮�䷢���仯
 *   - �������û�������xSemaphoreGetMutexHolder����ʱ����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
#if ( ( configUSE_MUTEXES == 1 ) && ( INCLUDE_xSemaphoreGetMutexHolder == 1 ) )

void* xQueueGetMutexHolder( QueueHandle_t xSemaphore )
{
    void *pxReturn;  /* ����ֵ���洢�����������ߵ������� */

    /* �˺�����xSemaphoreGetMutexHolder()���ã���Ӧֱ�ӵ��á�
       ע�⣺����ȷ�����������Ƿ��ǻ����������ߵĺ÷�����
       ������ȷ����������������ݵĺ÷�������Ϊ�����߿�����
       �����ٽ����˳��ͺ�������֮�䷢���仯 */
    
    /* �����ٽ��������������������߲�ѯ���� */
    taskENTER_CRITICAL();
    {
        /* ����ź��������Ƿ�Ϊ������ */
        if( ( ( Queue_t * ) xSemaphore )->uxQueueType == queueQUEUE_IS_MUTEX )
        {
            /* ��ȡ�������ĵ�ǰ������������ */
            pxReturn = ( void * ) ( ( Queue_t * ) xSemaphore )->pxMutexHolder;
        }
        else
        {
            /* ������ǻ�����������NULL */
            pxReturn = NULL;
        }
    }
    /* �˳��ٽ��� */
    taskEXIT_CRITICAL();

    /* ���ػ����������ߵ���������NULL */
    return pxReturn;
} /*lint !e818 xSemaphore������ָ��const��ָ�룬��Ϊ����һ��typedef */

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
 * �������ƣ�xQueueTakeMutexRecursive
 * �����������ݹ黥�����Ļ�ȡ����������ͬһ�������λ�ȡ�ݹ黥����
 *           �˺������ڻ�ȡ�ݹ黥��������������ѳ��л����������ӵݹ����
 * ���������
 *   - xMutex: �ݹ黥�����ľ��
 *   - xTicksToWait: �ȴ���ȡ�����������ʱ�䣨��ʱ�ӽ���Ϊ��λ��
 *     ����ֵ��portMAX_DELAY��ʾ�����ڵȴ�
 * �����������
 * �� �� ֵ��
 *   - BaseType_t: �������
 *     pdPASS: �ɹ���ȡ�ݹ黥����
 *     pdFAIL: ��ȡʧ�ܣ���ʱ��������
 * ����˵����
 *   - �˺����������õݹ黥��������(configUSE_RECURSIVE_MUTEXES == 1)ʱ����
 *   - �ݹ黥��������ͬһ�������λ�ȡ�������ͷ���ͬ�����Ż������ͷ�
 *   - ��������ѳ��л���������ֱ�����ӵݹ����������Ҫ�ȴ�
 *   - �������δ���л����������Ի�ȡ��������������Ҫ�ȴ�
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
BaseType_t xQueueTakeMutexRecursive( QueueHandle_t xMutex, TickType_t xTicksToWait )
{
    BaseType_t xReturn;               /* ����ֵ��������� */
    Queue_t * const pxMutex = ( Queue_t * ) xMutex;  /* �����������ת��Ϊ���нṹָ�� */

    /* ���Լ�黥����ָ����Ч�� */
    configASSERT( pxMutex );

    /* ���ڻ��� exclusion ��ע����xQueueGiveMutexRecursive()�е���ͬ */

    /* ���ٵݹ黥������ȡ�¼� */
    traceTAKE_MUTEX_RECURSIVE( pxMutex );

    /* ��鵱ǰ�����Ƿ��Ѿ��ǻ������ĳ����� */
    if( pxMutex->pxMutexHolder == ( void * ) xTaskGetCurrentTaskHandle() ) /*lint !e961 Cast is not redundant as TaskHandle_t is a typedef. */
    {
        /* ��ǰ�����ѳ��л����������ӵݹ���� */
        ( pxMutex->u.uxRecursiveCallCount )++;
        xReturn = pdPASS;
    }
    else
    {
        /* ��ǰ����δ���л����������Ի�ȡ������ */
        xReturn = xQueueGenericReceive( pxMutex, NULL, xTicksToWait, pdFALSE );

        /* ֻ�гɹ���ȡ������ʱ�Ż᷵��pdPASS��
           ���������ڵ���˴�֮ǰ�����ѽ�������״̬ */
        if( xReturn != pdFAIL )
        {
            /* �ɹ���ȡ�����������ӵݹ���� */
            ( pxMutex->u.uxRecursiveCallCount )++;
        }
        else
        {
            /* ��ȡ������ʧ�ܣ�����ʧ���¼� */
            traceTAKE_MUTEX_RECURSIVE_FAILED( pxMutex );
        }
    }

    return xReturn;
}

#endif /* configUSE_RECURSIVE_MUTEXES */
/*-----------------------------------------------------------*/

#if( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )

/*******************************************************************************
 * �������ƣ�xQueueCreateCountingSemaphoreStatic
 * �������������������ź����ľ�̬�汾��ʹ�þ�̬������ڴ�
 *           �˺�����������ʼ��һ�������ź�����ʹ��Ԥ�ȷ�����ڴ�ռ�
 * ���������
 *   - uxMaxCount: �ź�����������ֵ��Ҳ���ź����������Դ����
 *   - uxInitialCount: �ź����ĳ�ʼ����ֵ����ʾ��ʼ���õ���Դ����
 *   - pxStaticQueue: ָ��StaticQueue_t�ṹ��ָ�룬���ڴ洢����״̬������
 * �����������
 * �� �� ֵ��
 *   - QueueHandle_t: �����ļ����ź����ľ�����������ʧ���򷵻�NULL
 * ����˵����
 *   - �˺����������ü����ź����;�̬���书��ʱ����
 *   - ʹ�þ�̬������ڴ棬���⶯̬�ڴ����
 *   - �����ź������ڹ���������������Դ�����ٿ�����Դ������
 *   - ��ʼ����ֵ���ܳ���������ֵ
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
QueueHandle_t xQueueCreateCountingSemaphoreStatic( const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount, StaticQueue_t *pxStaticQueue )
{
    QueueHandle_t xHandle;  /* ����ֵ�������ļ����ź������ */

    /* ���Լ��������ֵ��Ϊ0 */
    configASSERT( uxMaxCount != 0 );
    /* ���Լ���ʼ����ֵ������������ֵ */
    configASSERT( uxInitialCount <= uxMaxCount );

    /* ʹ��ͨ�ö��д����������������ź��� */
    xHandle = xQueueGenericCreateStatic( uxMaxCount,                     /* ���г��ȣ�������ֵ�� */
                                        queueSEMAPHORE_QUEUE_ITEM_LENGTH, /* ������ȣ��ź���ʹ�ù̶����ȣ� */
                                        NULL,                            /* ������洢�����ź�������Ҫ�� */
                                        pxStaticQueue,                   /* ��̬���нṹ */
                                        queueQUEUE_TYPE_COUNTING_SEMAPHORE ); /* �������ͣ������ź��� */

    /* ����ź����Ƿ񴴽��ɹ� */
    if( xHandle != NULL )
    {
        /* �����ź����ĳ�ʼ����ֵ */
        ( ( Queue_t * ) xHandle )->uxMessagesWaiting = uxInitialCount;

        /* ���ټ����ź��������¼� */
        traceCREATE_COUNTING_SEMAPHORE();
    }
    else
    {
        /* ���ټ����ź�������ʧ���¼� */
        traceCREATE_COUNTING_SEMAPHORE_FAILED();
    }

    /* ���ش����ļ����ź������ */
    return xHandle;
}

#endif /* ( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) ) */
/*-----------------------------------------------------------*/

#if( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

/*******************************************************************************
 * �������ƣ�xQueueCreateCountingSemaphore
 * �������������������ź����Ķ�̬�汾��ʹ�ö�̬������ڴ�
 *           �˺�����������ʼ��һ�������ź�����ʹ�ö�̬������ڴ�ռ�
 * ���������
 *   - uxMaxCount: �ź�����������ֵ��Ҳ���ź����������Դ����
 *   - uxInitialCount: �ź����ĳ�ʼ����ֵ����ʾ��ʼ���õ���Դ����
 * �����������
 * �� �� ֵ��
 *   - QueueHandle_t: �����ļ����ź����ľ�����������ʧ���򷵻�NULL
 * ����˵����
 *   - �˺����������ü����ź����Ͷ�̬���书��ʱ����
 *   - ʹ�ö�̬������ڴ棬�Զ������ڴ������ͷ�
 *   - �����ź������ڹ���������������Դ�����ٿ�����Դ������
 *   - ��ʼ����ֵ���ܳ���������ֵ
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
QueueHandle_t xQueueCreateCountingSemaphore( const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount )
{
    QueueHandle_t xHandle;  /* ����ֵ�������ļ����ź������ */

    /* ���Լ��������ֵ��Ϊ0 */
    configASSERT( uxMaxCount != 0 );
    /* ���Լ���ʼ����ֵ������������ֵ */
    configASSERT( uxInitialCount <= uxMaxCount );

    /* ʹ��ͨ�ö��д����������������ź��� */
    xHandle = xQueueGenericCreate( uxMaxCount,                     /* ���г��ȣ�������ֵ�� */
                                  queueSEMAPHORE_QUEUE_ITEM_LENGTH, /* ������ȣ��ź���ʹ�ù̶����ȣ� */
                                  queueQUEUE_TYPE_COUNTING_SEMAPHORE ); /* �������ͣ������ź��� */

    /* ����ź����Ƿ񴴽��ɹ� */
    if( xHandle != NULL )
    {
        /* �����ź����ĳ�ʼ����ֵ */
        ( ( Queue_t * ) xHandle )->uxMessagesWaiting = uxInitialCount;

        /* ���ټ����ź��������¼� */
        traceCREATE_COUNTING_SEMAPHORE();
    }
    else
    {
        /* ���ټ����ź�������ʧ���¼� */
        traceCREATE_COUNTING_SEMAPHORE_FAILED();
    }

    /* ���ش����ļ����ź������ */
    return xHandle;
}

#endif /* ( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) ) */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xQueueGenericSend
 * ����������ͨ�ö��з��ͺ�����֧������з������ݵĶ��ַ�ʽ������ǰ�򡢸��ǣ�
 *           �˺�����FreeRTOS���в����ĺ��ĺ������������ݷ��͡����������ͻ��ѵȸ����߼�
 * ���������
 *   - xQueue: ���о����ָ��Ҫ�����Ķ���
 *   - pvItemToQueue: ָ��Ҫ�������ݵ�ָ�룬����������СΪ0�����ΪNULL
 *   - xTicksToWait: �ȴ������пռ�����ʱ�䣨��ʱ�ӽ���Ϊ��λ��
 *     ����ֵ��portMAX_DELAY��ʾ�����ڵȴ���0��ʾ���ȴ���������
 *   - xCopyPosition: ���ݸ���λ�ã�ָ�����ݷ��͵����е�λ��
 *     queueSEND_TO_BACK: ���͵�����β�����Ƚ��ȳ���
 *     queueSEND_TO_FRONT: ���͵�����ͷ��������ȳ���
 *     queueOVERWRITE: ���Ƕ���ͷ�������ݣ������ڳ���Ϊ1�Ķ��У�
 * �����������
 * �� �� ֵ��
 *   - BaseType_t: �������
 *     pdPASS: �ɹ��������ݵ�����
 *     errQUEUE_FULL: ���������ҵȴ���ʱ������ʧ��
 * ����˵����
 *   - �˺���������з��͵����������������ͨ���С��ź����ͻ�����
 *   - ֧�ֶ��ֵȴ����Ժ����ݴ���ʽ
 *   - ����������������ʱ�����ȼ����ѵȸ����߼�
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
BaseType_t xQueueGenericSend( QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait, const BaseType_t xCopyPosition )
{
    BaseType_t xEntryTimeSet = pdFALSE;  /* ��־���Ƿ������ó�ʱ״̬ */
    BaseType_t xYieldRequired;           /* ��־���Ƿ���Ҫ�����л� */
    TimeOut_t xTimeOut;                  /* ��ʱ״̬�ṹ */
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* �����о��ת��Ϊ���нṹָ�� */

    /* ���Լ�����ָ����Ч�� */
    configASSERT( pxQueue );
    /* ���Լ�飺����������С��Ϊ0��������ָ�벻��ΪNULL */
    configASSERT( !( ( pvItemToQueue == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );
    /* ���Լ�飺����ģʽֻ�����ڳ���Ϊ1�Ķ��� */
    configASSERT( !( ( xCopyPosition == queueOVERWRITE ) && ( pxQueue->uxLength != 1 ) ) );
    /* ��������˵�����״̬��ȡ��ʱ�����ܣ���������״̬ */
    #if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
    {
        /* ���Լ�飺����������ѹ������ܵȴ� */
        configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
    }
    #endif

    /* �˺�����΢�ſ��˱����׼�������ں����ڲ�ʹ��return��䡣
       ��������Ϊ�����ִ��ʱ��Ч�� */
    for( ;; )
    {
        /* �����ٽ��� */
        taskENTER_CRITICAL();
        {
            /* ���������пռ������������������Ҫ���ʶ��е�������ȼ�����
               ���Ҫ���Ƕ���ͷ������Ŀ��������Ƿ������޹ؽ�Ҫ */
            if( ( pxQueue->uxMessagesWaiting < pxQueue->uxLength ) || ( xCopyPosition == queueOVERWRITE ) )
            {
                /* ���ٶ��з����¼� */
                traceQUEUE_SEND( pxQueue );
                /* �������ݵ����в�����Ƿ���Ҫ�����л� */
                xYieldRequired = prvCopyDataToQueue( pxQueue, pvItemToQueue, xCopyPosition );

                /* ��������˶��м����� */
                #if ( configUSE_QUEUE_SETS == 1 )
                {
                    if( pxQueue->pxQueueSetContainer != NULL )
                    {
                        /* ֪ͨ���м����� */
                        if( prvNotifyQueueSetContainer( pxQueue, xCopyPosition ) != pdFALSE )
                        {
                            /* �����Ƕ��м��ĳ�Ա�����ҷ��������м����¸������ȼ���������������
                               ��Ҫ�������л� */
                            queueYIELD_IF_USING_PREEMPTION();
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    else
                    {
                        /* ������������ڵȴ������е����ݵ����������������� */
                        if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                        {
                            if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                            {
                                /* �������������������ȼ����������Լ������ȼ�����������ó���
                                   ���ٽ������������ǿ��Ե� - �ں˻ᴦ��������� */
                                queueYIELD_IF_USING_PREEMPTION();
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();
                            }
                        }
                        else if( xYieldRequired != pdFALSE )
                        {
                            /* ��·����һ�����������ֻ����������ж�����������һ����������ȡ˳��ͬ��˳�򷵻�ʱ�Ż�ִ�� */
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
                    /* ������������ڵȴ������е����ݵ����������������� */
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                    {
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                        {
                            /* �������������������ȼ����������Լ������ȼ�����������ó���
                               ���ٽ������������ǿ��Ե� - �ں˻ᴦ��������� */
                            queueYIELD_IF_USING_PREEMPTION();
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    else if( xYieldRequired != pdFALSE )
                    {
                        /* ��·����һ�����������ֻ����������ж�����������һ����������ȡ˳��ͬ��˳�򷵻�ʱ�Ż�ִ�� */
                        queueYIELD_IF_USING_PREEMPTION();
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                #endif /* configUSE_QUEUE_SETS */

                /* �˳��ٽ��������سɹ� */
                taskEXIT_CRITICAL();
                return pdPASS;
            }
            else
            {
                /* ��������������Ƿ���Ҫ�ȴ� */
                if( xTicksToWait == ( TickType_t ) 0 )
                {
                    /* ����������δָ������ʱ�䣨������ʱ���ѹ��ڣ��������뿪 */
                    taskEXIT_CRITICAL();

                    /* ���˳�����֮ǰ���ص�ԭʼ��Ȩ���� */
                    traceQUEUE_SEND_FAILED( pxQueue );
                    return errQUEUE_FULL;
                }
                else if( xEntryTimeSet == pdFALSE )
                {
                    /* ����������ָ��������ʱ�䣬���ó�ʱ�ṹ */
                    vTaskSetTimeOutState( &xTimeOut );
                    xEntryTimeSet = pdTRUE;
                }
                else
                {
                    /* ����ʱ�������� */
                    mtCOVERAGE_TEST_MARKER();
                }
            }
        }
        /* �˳��ٽ��� */
        taskEXIT_CRITICAL();

        /* �����ٽ������˳����жϺ����������������з��ͺʹӶ��н��� */

        /* �������������������� */
        vTaskSuspendAll();
        prvLockQueue( pxQueue );

        /* ���³�ʱ״̬�Բ鿴�Ƿ��ѹ��� */
        if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
        {
            /* �������Ƿ���Ȼ�� */
            if( prvIsQueueFull( pxQueue ) != pdFALSE )
            {
                /* ���ٶ��з��������¼� */
                traceBLOCKING_ON_QUEUE_SEND( pxQueue );
                /* ����ǰ������õ����͵ȴ��¼��б� */
                vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToSend ), xTicksToWait );

                /* ����������ζ�Ŷ����¼�����Ӱ���¼��б�
                   ���ڷ������жϿ��ܻ��ٴδӴ��¼��б����Ƴ������� -
                   �����ڵ������ѹ������񽫽���������б������ʵ�ʵľ����б� */
                prvUnlockQueue( pxQueue );

                /* �ָ���������������Ӵ������б��ƶ��������б� -
                   ��˿��ܴ��������ó�֮ǰ�Ѿ��ھ����б��� -
                   ����������£����Ǵ������б��л��и������ȼ������񣬷����ó����ᵼ���������л� */
                if( xTaskResumeAll() == pdFALSE )
                {
                    portYIELD_WITHIN_API();
                }
            }
            else
            {
                /* ���в����������� */
                prvUnlockQueue( pxQueue );
                ( void ) xTaskResumeAll();
            }
        }
        else
        {
            /* ��ʱ�ѹ��� */
            prvUnlockQueue( pxQueue );
            ( void ) xTaskResumeAll();

            /* ���ٶ��з���ʧ���¼� */
            traceQUEUE_SEND_FAILED( pxQueue );
            return errQUEUE_FULL;
        }
    }
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xQueueGenericSendFromISR
 * �������������жϷ������(ISR)����з������ݵ�ͨ�ú�����֧�ֶ��ַ��ͷ�ʽ
 *           �˺�����xQueueGenericSend���жϰ�ȫ�汾��רΪISR���
 * ���������
 *   - xQueue: ���о����ָ��Ҫ�����Ķ���
 *   - pvItemToQueue: ָ��Ҫ�������ݵ�ָ�룬����������СΪ0�����ΪNULL
 *   - pxHigherPriorityTaskWoken: ָ��������ȼ������ѱ�־��ָ��
 *     ����ָʾ�Ƿ��и������ȼ����񱻻��ѣ���Ҫ�������л�
 *   - xCopyPosition: ���ݸ���λ�ã�ָ�����ݷ��͵����е�λ��
 *     queueSEND_TO_BACK: ���͵�����β�����Ƚ��ȳ���
 *     queueSEND_TO_FRONT: ���͵�����ͷ��������ȳ���
 *     queueOVERWRITE: ���Ƕ���ͷ�������ݣ������ڳ���Ϊ1�Ķ��У�
 * ���������
 *   - pxHigherPriorityTaskWoken: �����õĸ������ȼ������ѱ�־
 * �� �� ֵ��
 *   - BaseType_t: �������
 *     pdPASS: �ɹ��������ݵ�����
 *     errQUEUE_FULL: ��������������ʧ��
 * ����˵����
 *   - �˺������жϰ�ȫ�汾��ֻ����ISR�е���
 *   - ��֤�ж����ȼ���ȷ������ӹ������ȼ����жϵ���
 *   - ʹ���жϰ�ȫ���ٽ����������в���
 *   - �����������״̬��֧�ֶ��м�����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
BaseType_t xQueueGenericSendFromISR( QueueHandle_t xQueue, const void * const pvItemToQueue, BaseType_t * const pxHigherPriorityTaskWoken, const BaseType_t xCopyPosition )
{
    BaseType_t xReturn;  /* ����ֵ��������� */
    UBaseType_t uxSavedInterruptStatus;  /* ������ж�״̬ */
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* �����о��ת��Ϊ���нṹָ�� */

    /* ���Լ�����ָ����Ч�� */
    configASSERT( pxQueue );
    /* ���Լ�飺����������С��Ϊ0��������ָ�벻��ΪNULL */
    configASSERT( !( ( pvItemToQueue == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );
    /* ���Լ�飺����ģʽֻ�����ڳ���Ϊ1�Ķ��� */
    configASSERT( !( ( xCopyPosition == queueOVERWRITE ) && ( pxQueue->uxLength != 1 ) ) );

    /* ֧���ж�Ƕ�׵�RTOS�˿ھ������ϵͳ���ã������API���ã��ж����ȼ��ĸ��
       �������ϵͳ�������ȼ����жϱ����������ã���ʹRTOS�ں˴����ٽ�����
       �����ܵ����κ�FreeRTOS API�����������FreeRTOSConfig.h�ж�����configASSERT()��
       ��portASSERT_IF_INTERRUPT_PRIORITY_INVALID()�����¶���ʧ�ܣ�
       ������ѱ�����������õ����ϵͳ�������ȼ����жϵ���FreeRTOS API������
       ֻ����FromISR��β��FreeRTOS�������Դ��ѱ��������ȼ����ڻ��߼��ϣ�
       �������ϵͳ�����ж����ȼ����жϵ��á�FreeRTOSά��һ���������жϰ�ȫAPI��
       ��ȷ���ж���ھ����ܿ��ٺͼ򵥡�������Ϣ��������Cortex-M�ض��ģ�
       �����������ṩ��http://www.freertos.org/RTOS-Cortex-M3-M4.html */
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    /* ������xQueueGenericSend�����ڶ���û�пռ�ʱ����������
       Ҳ����ֱ�ӻ��ѱ������ڶ��ж�ȡ�ϵ����񣬶��Ƿ���һ����־��ָʾ�Ƿ���Ҫ�������л�
       �����Ƿ��б��������ȼ����ߵ����񱻴˷��Ͳ������ѣ� */
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        /* �������Ƿ��пռ���Ƿ�ʹ�ø���ģʽ */
        if( ( pxQueue->uxMessagesWaiting < pxQueue->uxLength ) || ( xCopyPosition == queueOVERWRITE ) )
        {
            const int8_t cTxLock = pxQueue->cTxLock;  /* ���浱ǰ�ķ�������״̬ */

            /* ���ٴ�ISR���Ͷ����¼� */
            traceQUEUE_SEND_FROM_ISR( pxQueue );

            /* �ź���ʹ��xQueueGiveFromISR()�����pxQueue�������ź����򻥳�����
               ����ζ��prvCopyDataToQueue()���ᵼ������ʧȥ���ȼ��̳У�
               ���Ҽ�ʹʧȥ���ȼ��̳к����ڷ��ʾ����б�֮ǰ�����������Ƿ����
               Ҳ�������������prvCopyDataToQueue() */
            ( void ) prvCopyDataToQueue( pxQueue, pvItemToQueue, xCopyPosition );

            /* ������б��������򲻸����¼��б��⽫���Ժ���н���ʱ��� */
            if( cTxLock == queueUNLOCKED )
            {
                /* ��������˶��м����� */
                #if ( configUSE_QUEUE_SETS == 1 )
                {
                    if( pxQueue->pxQueueSetContainer != NULL )
                    {
                        /* ֪ͨ���м����� */
                        if( prvNotifyQueueSetContainer( pxQueue, xCopyPosition ) != pdFALSE )
                        {
                            /* �����Ƕ��м��ĳ�Ա�����ҷ��������м����¸������ȼ���������������
                               ��Ҫ�������л� */
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
                        /* ����Ƿ�������ȴ��������� */
                        if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                        {
                            /* ���¼��б����Ƴ��ȴ����յ����� */
                            if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                            {
                                /* �ȴ���������и��ߵ����ȼ�����˼�¼��Ҫ�������л� */
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
                    /* ����Ƿ�������ȴ��������� */
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                    {
                        /* ���¼��б����Ƴ��ȴ����յ����� */
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                        {
                            /* �ȴ���������и��ߵ����ȼ�����˼�¼��Ҫ�������л� */
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
                /* ���������������Ա�������е�����֪��������ʱ�����ݷ��� */
                pxQueue->cTxLock = ( int8_t ) ( cTxLock + 1 );
            }

            xReturn = pdPASS;
        }
        else
        {
            /* �������������ٷ���ʧ���¼� */
            traceQUEUE_SEND_FROM_ISR_FAILED( pxQueue );
            xReturn = errQUEUE_FULL;
        }
    }
    /* �ָ��ж�״̬ */
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

    return xReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xQueueGiveFromISR
 * �������������жϷ������ISR�����ͷ��ź��������������СΪ0�Ķ��У�
 *           �˺���ר�������ź���������ͨ�����Ӷ����е���Ϣ����ʵ���ź����ͷ�
 *           �����Ƿ���������ȴ����ź����������������ڱ�Ҫʱ���Ѹ������ȼ�����
 * ���������
 *   - xQueue: ���о����ʵ��ӦΪ�ź��������uxItemSize����Ϊ0��
 *   - pxHigherPriorityTaskWoken: ָ��������ȼ������ѱ�־��ָ��
 * ���������
 *   - pxHigherPriorityTaskWoken: ָʾ�Ƿ��и������ȼ����񱻻��ѣ���Ҫ�������л�
 * �� �� ֵ��
 *   - BaseType_t: �ɹ��ͷ��ź�������pdPASS��������������errQUEUE_FULL
 * ����˵����
 *   - �˺���ר�����ź����������������С����Ϊ0
 *   - ��Ӧ���ж����ͷŻ����ź�������Ϊ���ȼ��̳ж��ж������壩
 *   - �������ж����ȼ��Ϸ����ж��������е��ã��������ϵͳ�������ȼ�Ҫ��
 *   - ֧�ֶ��м���Queue Sets�����ܣ��ɻ��Ѷ��м������ĵȴ�����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
BaseType_t xQueueGiveFromISR( QueueHandle_t xQueue, BaseType_t * const pxHigherPriorityTaskWoken )
{
    BaseType_t xReturn;                              /* ��������ֵ */
    UBaseType_t uxSavedInterruptStatus;              /* ������ж�״̬�����ڻָ��ж����� */
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* �����о��ת��Ϊ���нṹ��ָ�� */

    /* ���Լ�飺ȷ������ָ����Ч */
    configASSERT( pxQueue );

    /* ���Լ�飺ȷ���������СΪ0���˺���ר�����ź����� */
    configASSERT( pxQueue->uxItemSize == 0 );

    /* ���Լ�飺�����ź�����Ӧ���ж��ͷţ��ر��Ǵ��ڻ��������ʱ�� */
    configASSERT( !( ( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX ) && ( pxQueue->pxMutexHolder != NULL ) ) );

    /* �ж����ȼ���֤��ȷ����ǰ�ж����ȼ��������FreeRTOS API���� */
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    /* ���浱ǰ�ж�״̬�������ж����룬�����ٽ��� */
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        /* ��ȡ��ǰ�����еȴ�����Ϣ���� */
        const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting;

        /* �������Ƿ��пռ䣨�����ź�����������Ƿ�δ�ﵽ������ֵ�� */
        if( uxMessagesWaiting < pxQueue->uxLength )
        {
            /* ��ȡ���еķ�������״̬ */
            const int8_t cTxLock = pxQueue->cTxLock;

            /* ���ٶ��з����¼������ڵ��Ժͷ����� */
            traceQUEUE_SEND_FROM_ISR( pxQueue );

            /* ���Ӷ����е���Ϣ���������ͷ��ź����� */
            pxQueue->uxMessagesWaiting = uxMessagesWaiting + 1;

            /* �������Ƿ�δ���� */
            if( cTxLock == queueUNLOCKED )
            {
                #if ( configUSE_QUEUE_SETS == 1 )  /* ��������˶��м����� */
                {
                    /* ���˶����Ƿ�����ĳ�����м� */
                    if( pxQueue->pxQueueSetContainer != NULL )
                    {
                        /* ֪ͨ���м���������Ϣ���� */
                        if( prvNotifyQueueSetContainer( pxQueue, queueSEND_TO_BACK ) != pdFALSE )
                        {
                            /* ���֪ͨ���¸������ȼ������������������������л���־ */
                            if( pxHigherPriorityTaskWoken != NULL )
                            {
                                *pxHigherPriorityTaskWoken = pdTRUE;
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                        }
                    }
                    else  /* ���в������κζ��м� */
                    {
                        /* ����Ƿ����������ڵȴ����մ˶��е���Ϣ */
                        if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                        {
                            /* �ӵȴ������б����Ƴ����񲢽������ */
                            if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                            {
                                /* �������������������ȼ����ߣ������������л���־ */
                                if( pxHigherPriorityTaskWoken != NULL )
                                {
                                    *pxHigherPriorityTaskWoken = pdTRUE;
                                }
                                else
                                {
                                    mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                                }
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                        }
                    }
                }
                #else  /* δ���ö��м����� */
                {
                    /* ����Ƿ����������ڵȴ����մ˶��е���Ϣ */
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                    {
                        /* �ӵȴ������б����Ƴ����񲢽������ */
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                        {
                            /* �������������������ȼ����ߣ������������л���־ */
                            if( pxHigherPriorityTaskWoken != NULL )
                            {
                                *pxHigherPriorityTaskWoken = pdTRUE;
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                        }
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                    }
                }
                #endif  /* configUSE_QUEUE_SETS */
            }
            else  /* ���д�������״̬ */
            {
                /* ���ӷ�����������������ʱ�ᴦ�����Ĳ��� */
                pxQueue->cTxLock = ( int8_t ) ( cTxLock + 1 );
            }

            /* ���÷���ֵΪ�ɹ� */
            xReturn = pdPASS;
        }
        else  /* �����������ź��������Ѵ����ֵ�� */
        {
            /* ���ٶ��з���ʧ���¼� */
            traceQUEUE_SEND_FROM_ISR_FAILED( pxQueue );
            /* ���÷���ֵΪ������������ */
            xReturn = errQUEUE_FULL;
        }
    }
    /* �ָ��ж�״̬���˳��ٽ��� */
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

    /* ���ز������ */
    return xReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xQueueGenericReceive
 * �����������Ӷ����н��������֧����ͨ���պͲ鿴ģʽ��
 *           �˺����Ƕ��н��ղ�����ͨ��ʵ�֣�֧�������ͷ�����ģʽ
 *           ��������Ϊ�Ƴ��������ͨ���գ�����鿴����������Ƴ���peekģʽ��
 * ���������
 *   - xQueue: ���о����ָ��Ҫ�����Ķ���
 *   - pvBuffer: ���ݻ�����ָ�룬���ڴ洢���յ�������
 *   - xTicksToWait: �������ʱ�䣨��ʱ�ӽ���Ϊ��λ��
 *   - xJustPeeking: ģʽѡ���־��pdTRUE��ʾ���鿴���Ƴ���pdFALSE��ʾ��ͨ����
 * ���������
 *   - pvBuffer: ���յ������ݽ��洢���˻�����
 * �� �� ֵ��
 *   - BaseType_t: �ɹ����յ����ݷ���pdPASS������Ϊ���ҳ�ʱ����errQUEUE_EMPTY
 * ����˵����
 *   - �˺����Ƕ��н��ղ����ĺ���ʵ�֣�֧�����������ͳ�ʱ����
 *   - ���ڻ����ź�����mutex�������⴦��֧�����ȼ��̳л���
 *   - ֧������ģʽ����ͨ���գ��Ƴ����ݣ��Ͳ鿴ģʽ�����Ƴ����ݣ�
 *   - �ڵ���������ʱ����ʹ�÷�������ʱ��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
BaseType_t xQueueGenericReceive( QueueHandle_t xQueue, void * const pvBuffer, TickType_t xTicksToWait, const BaseType_t xJustPeeking )
{
    BaseType_t xEntryTimeSet = pdFALSE;          /* ��ʱ�ṹ��ʼ����־ */
    TimeOut_t xTimeOut;                          /* ��ʱ״̬�ṹ */
    int8_t *pcOriginalReadPosition;              /* ԭʼ��ȡλ�ã�����peekģʽ�� */
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* �����о��ת��Ϊ���нṹ��ָ�� */

    /* ���Լ�飺ȷ������ָ����Ч */
    configASSERT( pxQueue );
    /* ���Լ�飺������������Ķ��У�������ָ�벻��ΪNULL */
    configASSERT( !( ( pvBuffer == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );
    #if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
    {
        /* ���Լ�飺����������ʱ����ʹ�÷�������ʱ�� */
        configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
    }
    #endif

    /* �˺����ſ��˱����׼�������ں����ڲ�ʹ��return��䣬����Ϊ��ִ��ʱ��Ч�� */

    /* ����ѭ����ֱ���ɹ��������ݻ�ʱ */
    for( ;; )
    {
        /* �����ٽ����������в��� */
        taskENTER_CRITICAL();
        {
            /* ��ȡ��ǰ�����еȴ�����Ϣ���� */
            const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting;

            /* ���������Ƿ������ݿ��ã����������������Ҫ���ʶ��е�������ȼ����� */
            if( uxMessagesWaiting > ( UBaseType_t ) 0 )
            {
                /* ����ԭʼ��ȡλ�ã��Ա���peekģʽ�»ָ� */
                pcOriginalReadPosition = pxQueue->u.pcReadFrom;

                /* �Ӷ����и������ݵ��ṩ�Ļ����� */
                prvCopyDataFromQueue( pxQueue, pvBuffer );

                /* ����Ƿ�Ϊ��ͨ����ģʽ����peekģʽ�� */
                if( xJustPeeking == pdFALSE )
                {
                    /* ���ٶ��н����¼� */
                    traceQUEUE_RECEIVE( pxQueue );

                    /* ʵ���Ƴ����ݣ����ٶ����е���Ϣ���� */
                    pxQueue->uxMessagesWaiting = uxMessagesWaiting - 1;

                    #if ( configUSE_MUTEXES == 1 )  /* ��������˻����ź������� */
                    {
                        /* �����������Ƿ�Ϊ�����ź��� */
                        if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
                        {
                            /* ��¼ʵ�����ȼ��̳��������Ϣ */
                            pxQueue->pxMutexHolder = ( int8_t * ) pvTaskIncrementMutexHeldCount(); /*lint !e961 ǿ��ת����������ģ���ΪTaskHandle_t��һ��typedef */
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                        }
                    }
                    #endif /* configUSE_MUTEXES */

                    /* ����Ƿ����������ڵȴ��������ݵ��˶��� */
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
                    {
                        /* �ӵȴ������б����Ƴ����񲢽������ */
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                        {
                            /* �������������������ȼ����ߣ����������л� */
                            queueYIELD_IF_USING_PREEMPTION();
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                        }
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                    }
                }
                else  /* peekģʽ�����鿴���ݶ����Ƴ� */
                {
                    /* ���ٶ��в鿴�¼� */
                    traceQUEUE_PEEK( pxQueue );

                    /* ���ݲ����Ƴ���������ö�ȡָ�뵽ԭʼλ�� */
                    pxQueue->u.pcReadFrom = pcOriginalReadPosition;

                    /* ���ݱ����ڶ����У�����Ƿ������������ڵȴ������� */
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                    {
                        /* �ӵȴ������б����Ƴ����񲢽������ */
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                        {
                            /* �ȴ����������ȼ����ڵ�ǰ���񣬴��������л� */
                            queueYIELD_IF_USING_PREEMPTION();
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                        }
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                    }
                }

                /* �˳��ٽ��������سɹ� */
                taskEXIT_CRITICAL();
                return pdPASS;
            }
            else  /* ����Ϊ�� */
            {
                /* ����Ƿ�ָ��������ʱ�� */
                if( xTicksToWait == ( TickType_t ) 0 )
                {
                    /* ����Ϊ����δָ������ʱ�䣨������ʱ���ѵ��ڣ����������� */
                    taskEXIT_CRITICAL();
                    traceQUEUE_RECEIVE_FAILED( pxQueue );
                    return errQUEUE_EMPTY;
                }
                else if( xEntryTimeSet == pdFALSE )
                {
                    /* ����Ϊ�յ�ָ��������ʱ�䣬���ó�ʱ�ṹ */
                    vTaskSetTimeOutState( &xTimeOut );
                    xEntryTimeSet = pdTRUE;
                }
                else
                {
                    /* ��ʱ�ṹ�����ã������ȴ� */
                    mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                }
            }
        }
        /* �˳��ٽ����������жϺ�����������ʶ��� */
        taskEXIT_CRITICAL();

        /* ������������׼����������״̬ */
        vTaskSuspendAll();
        /* ���������Է�ֹ�������� */
        prvLockQueue( pxQueue );

        /* ��鳬ʱ״̬�����Ƿ��ѳ�ʱ */
        if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
        {
            /* �ٴμ������Ƿ�Ϊ�� */
            if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
            {
                /* ���ٶ��н��������¼� */
                traceBLOCKING_ON_QUEUE_RECEIVE( pxQueue );

                #if ( configUSE_MUTEXES == 1 )  /* ��������˻����ź������� */
                {
                    /* �����������Ƿ�Ϊ�����ź��� */
                    if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
                    {
                        /* �����ٽ����������ȼ��̳� */
                        taskENTER_CRITICAL();
                        {
                            /* Ϊ�����ź���������ʵʩ���ȼ��̳� */
                            vTaskPriorityInherit( ( void * ) pxQueue->pxMutexHolder );
                        }
                        taskEXIT_CRITICAL();
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                    }
                }
                #endif

                /* ����ǰ������õ��ȴ������¼��б��� */
                vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait );
                /* �������� */
                prvUnlockQueue( pxQueue );
                /* �ָ��������񣬼���Ƿ���Ҫ�����л� */
                if( xTaskResumeAll() == pdFALSE )
                {
                    /* ��Ҫ�����л���ִ�ж˿���ص� yield ���� */
                    portYIELD_WITHIN_API();
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                }
            }
            else
            {
                /* ���в���Ϊ�գ����Խ��ղ��� */
                prvUnlockQueue( pxQueue );
                ( void ) xTaskResumeAll();
            }
        }
        else  /* �ѳ�ʱ */
        {
            /* �������в��ָ��������� */
            prvUnlockQueue( pxQueue );
            ( void ) xTaskResumeAll();

            /* ���ռ������Ƿ���ȻΪ�� */
            if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
            {
                /* ���ٶ��н���ʧ���¼� */
                traceQUEUE_RECEIVE_FAILED( pxQueue );
                return errQUEUE_EMPTY;
            }
            else
            {
                /* ���в���Ϊ�գ�����ѭ�����Խ��� */
                mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
            }
        }
    }
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xQueueReceiveFromISR
 * �������������жϷ������ISR���н��ն���������
 *           �˺������жϰ�ȫ�Ķ��н��ղ�����רΪ�ж����������
 *           �����������������ؽ��ս������֧�ֻ��Ѹ������ȼ�����
 * ���������
 *   - xQueue: ���о����ָ��Ҫ�����Ķ���
 *   - pvBuffer: ���ݻ�����ָ�룬���ڴ洢���յ�������
 * ���������
 *   - pxHigherPriorityTaskWoken: ָ��������ȼ������ѱ�־��ָ��
 *   - pvBuffer: ���յ������ݽ��洢���˻�����
 * �� �� ֵ��
 *   - BaseType_t: �ɹ����յ����ݷ���pdPASS������Ϊ�շ���pdFAIL
 * ����˵����
 *   - �˺���רΪ�ж���������ƣ��������ж�������
 *   - �������ж����ȼ��Ϸ����ж��������е���
 *   - ������������Ķ��У�������ָ�벻��ΪNULL
 *   - ֧�ֶ�������״̬�µ����ݽ��ղ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
BaseType_t xQueueReceiveFromISR( QueueHandle_t xQueue, void * const pvBuffer, BaseType_t * const pxHigherPriorityTaskWoken )
{
    BaseType_t xReturn;                              /* ��������ֵ */
    UBaseType_t uxSavedInterruptStatus;              /* ������ж�״̬�����ڻָ��ж����� */
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* �����о��ת��Ϊ���нṹ��ָ�� */

    /* ���Լ�飺ȷ������ָ����Ч */
    configASSERT( pxQueue );
    /* ���Լ�飺������������Ķ��У�������ָ�벻��ΪNULL */
    configASSERT( !( ( pvBuffer == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );

    /* �ж����ȼ���֤��ȷ����ǰ�ж����ȼ��������FreeRTOS API���� */
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    /* ���浱ǰ�ж�״̬�������ж����룬�����ٽ��� */
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        /* ��ȡ��ǰ�����еȴ�����Ϣ���� */
        const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting;

        /* ���������Ƿ������ݿ��ã��ж��в������������Ա���������飩 */
        if( uxMessagesWaiting > ( UBaseType_t ) 0 )
        {
            /* ��ȡ���еĽ�������״̬ */
            const int8_t cRxLock = pxQueue->cRxLock;

            /* ���ٶ��н����¼������ڵ��Ժͷ����� */
            traceQUEUE_RECEIVE_FROM_ISR( pxQueue );

            /* �Ӷ����и������ݵ��ṩ�Ļ����� */
            prvCopyDataFromQueue( pxQueue, pvBuffer );
            /* ���ٶ����е���Ϣ���� */
            pxQueue->uxMessagesWaiting = uxMessagesWaiting - 1;

            /* �������Ƿ�δ���� */
            if( cRxLock == queueUNLOCKED )
            {
                /* ����Ƿ����������ڵȴ��������ݵ��˶��� */
                if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
                {
                    /* �ӵȴ������б����Ƴ����񲢽������ */
                    if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                    {
                        /* �������������������ȼ����ߣ������������л���־ */
                        if( pxHigherPriorityTaskWoken != NULL )
                        {
                            *pxHigherPriorityTaskWoken = pdTRUE;
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                        }
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                    }
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                }
            }
            else  /* ���д�������״̬ */
            {
                /* ���ӽ�����������������ʱ�ᴦ�����Ĳ��� */
                pxQueue->cRxLock = ( int8_t ) ( cRxLock + 1 );
            }

            /* ���÷���ֵΪ�ɹ� */
            xReturn = pdPASS;
        }
        else  /* ����Ϊ�� */
        {
            /* ���÷���ֵΪʧ�� */
            xReturn = pdFAIL;
            /* ���ٶ��н���ʧ���¼� */
            traceQUEUE_RECEIVE_FROM_ISR_FAILED( pxQueue );
        }
    }
    /* �ָ��ж�״̬���˳��ٽ��� */
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

    /* ���ز������ */
    return xReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xQueuePeekFromISR
 * �������������жϷ������ISR���в鿴����ͷ��������������Ƴ�
 *           �˺������жϰ�ȫ�Ķ��в鿴������רΪ�ж����������
 *           �����������������ز鿴������Ҳ����޸Ķ���״̬
 * ���������
 *   - xQueue: ���о����ָ��Ҫ�����Ķ���
 * ���������
 *   - pvBuffer: ���ݻ�����ָ�룬���ڴ洢�鿴��������
 * �� �� ֵ��
 *   - BaseType_t: �ɹ��鿴���ݷ���pdPASS������Ϊ�շ���pdFAIL
 * ����˵����
 *   - �˺���רΪ�ж���������ƣ��������ж�������
 *   - �������ж����ȼ��Ϸ����ж��������е���
 *   - �������ڲ鿴�ź������ź������СΪ0��
 *   - ������������Ķ��У�������ָ�벻��ΪNULL
 *   - �鿴��������ı����״̬����Ϣ�����Ͷ�ȡλ�ñ��ֲ��䣩
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
BaseType_t xQueuePeekFromISR( QueueHandle_t xQueue, void * const pvBuffer )
{
    BaseType_t xReturn;                              /* ��������ֵ */
    UBaseType_t uxSavedInterruptStatus;              /* ������ж�״̬�����ڻָ��ж����� */
    int8_t *pcOriginalReadPosition;                  /* ԭʼ��ȡλ�ã����ڲ鿴��ָ� */
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* �����о��ת��Ϊ���нṹ��ָ�� */

    /* ���Լ�飺ȷ������ָ����Ч */
    configASSERT( pxQueue );
    /* ���Լ�飺������������Ķ��У�������ָ�벻��ΪNULL */
    configASSERT( !( ( pvBuffer == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );
    /* ���Լ�飺���ܲ鿴�ź������ź������СΪ0�� */
    configASSERT( pxQueue->uxItemSize != 0 );

    /* �ж����ȼ���֤��ȷ����ǰ�ж����ȼ��������FreeRTOS API���� */
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    /* ���浱ǰ�ж�״̬�������ж����룬�����ٽ��� */
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        /* ���������Ƿ������ݿ��ã��ж��в������������Ա���������飩 */
        if( pxQueue->uxMessagesWaiting > ( UBaseType_t ) 0 )
        {
            /* ���ٶ��в鿴�¼������ڵ��Ժͷ����� */
            traceQUEUE_PEEK_FROM_ISR( pxQueue );

            /* ����ԭʼ��ȡλ�ã��Ա�鿴��ָ�����Ϊʵ���ϲ��Ƴ����ݣ� */
            pcOriginalReadPosition = pxQueue->u.pcReadFrom;
            
            /* �Ӷ����и������ݵ��ṩ�Ļ����� */
            prvCopyDataFromQueue( pxQueue, pvBuffer );
            
            /* �ָ���ȡλ�ã���Ϊֻ�ǲ鿴����ʵ���Ƴ����ݣ� */
            pxQueue->u.pcReadFrom = pcOriginalReadPosition;

            /* ���÷���ֵΪ�ɹ� */
            xReturn = pdPASS;
        }
        else  /* ����Ϊ�� */
        {
            /* ���÷���ֵΪʧ�� */
            xReturn = pdFAIL;
            /* ���ٶ��в鿴ʧ���¼� */
            traceQUEUE_PEEK_FROM_ISR_FAILED( pxQueue );
        }
    }
    /* �ָ��ж�״̬���˳��ٽ��� */
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

    /* ���ز������ */
    return xReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�uxQueueSpacesAvailable
 * ������������ȡ�����е�ǰ���õĿ��пռ�����
 *           �˺������ڲ�ѯ�����л��������ɵ���Ϣ������
 * ���������
 *   - xQueue: ���о����ָ��Ҫ��ѯ�Ķ���
 * �� �� ֵ��
 *   - UBaseType_t: �����е�ǰ���õĿ��пռ�����
 * ����˵����
 *   - �˺������ٽ�����ִ�У�ȷ�������ԭ����
 *   - ���������������ģ��������жη��������ʹ��
 *   - ����ֵ = �����ܳ��� - ��ǰ��Ϣ����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
UBaseType_t uxQueueSpacesAvailable( const QueueHandle_t xQueue )
{
    UBaseType_t uxReturn;  /* ����ֵ���洢�����еĿ��пռ����� */
    Queue_t *pxQueue;      /* ���нṹ��ָ�� */

    /* �����о��ת��Ϊ���нṹ��ָ�� */
    pxQueue = ( Queue_t * ) xQueue;
    /* ���Լ�飺ȷ������ָ����Ч */
    configASSERT( pxQueue );

    /* �����ٽ����������в��� */
    taskENTER_CRITICAL();
    {
        /* ��������еĿ��пռ��������ܳ��ȼ�ȥ��ǰ��Ϣ���� */
        uxReturn = pxQueue->uxLength - pxQueue->uxMessagesWaiting;
    }
    /* �˳��ٽ��� */
    taskEXIT_CRITICAL();

    /* ���ض����еĿ��пռ����� */
    return uxReturn;
} /*lint !e818 ָ�벻������Ϊconst����ΪxQueue��typedef������ָ�� */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�uxQueueMessagesWaitingFromISR
 * �������������жϷ�������ȡ�����е�ǰ�ȴ�����Ϣ����
 *           �˺������жϰ�ȫ�汾��������ISR�в�ѯ�����е���Ϣ������
 * ���������
 *   - xQueue: ���о����ָ��Ҫ��ѯ�Ķ���
 * �� �� ֵ��
 *   - UBaseType_t: �����е�ǰ�ȴ�����Ϣ����
 * ����˵����
 *   - �˺���רΪ�ж���������ƣ��������ٽ���
 *   - �������жϷ�����򣬲�����������������ʹ��
 *   - ������ISR�е��ã�����Ҫ�ٽ�������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
UBaseType_t uxQueueMessagesWaitingFromISR( const QueueHandle_t xQueue )
{
    UBaseType_t uxReturn;  /* ����ֵ���洢�����е���Ϣ���� */

    /* ���Լ�飺ȷ�����о����Ч */
    configASSERT( xQueue );

    /* ֱ�ӴӶ��нṹ���л�ȡ��ǰ�ȴ�����Ϣ����������Ҫ�ٽ��������� */
    uxReturn = ( ( Queue_t * ) xQueue )->uxMessagesWaiting;

    /* ���ض����е���Ϣ���� */
    return uxReturn;
} /*lint !e818 ָ�벻������Ϊconst����ΪxQueue��typedef������ָ�� */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�vQueueDelete
 * ����������ɾ�����в��ͷ������Դ
 *           �˺���������ȫɾ�����в��ͷ���ռ�õ��ڴ���Դ
 * ���������
 *   - xQueue: ���о����ָ��Ҫɾ���Ķ���
 * ����˵����
 *   - �˺�������ݶ��еķ��䷽ʽ����̬��̬����������ͷ���Դ
 *   - ���ڶ�̬����Ķ��У����ͷ����ڴ�
 *   - ���ھ�̬����Ķ��У������ͷ��ڴ棨��Ϊ�ڴ治�����ں˷���ģ�
 *   - ��������˶���ע����ܣ����ȴ�ע�����ע������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
void vQueueDelete( QueueHandle_t xQueue )
{
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* �����о��ת��Ϊ���нṹ��ָ�� */

    /* ���Լ�飺ȷ������ָ����Ч */
    configASSERT( pxQueue );
    /* ���ٶ���ɾ���¼������ڵ��Ժͷ����� */
    traceQUEUE_DELETE( pxQueue );

    /* ��������˶���ע����� */
    #if ( configQUEUE_REGISTRY_SIZE > 0 )
    {
        /* �Ӷ���ע�����ע���ö��� */
        vQueueUnregisterQueue( pxQueue );
    }
    #endif

    /* �����ڴ�������ô������ɾ�� */
    #if( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 0 ) )
    {
        /* ����ֻ���Ƕ�̬����� - �ͷ����ڴ� */
        vPortFree( pxQueue );
    }
    #elif( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
    {
        /* ���п����Ǿ�̬��̬����ģ���Ҫ�����پ����Ƿ��ͷ��ڴ� */
        if( pxQueue->ucStaticallyAllocated == ( uint8_t ) pdFALSE )
        {
            /* ��̬����Ķ��У��ͷ����ڴ� */
            vPortFree( pxQueue );
        }
        else
        {
            /* ��̬����Ķ��У����ͷ��ڴ� */
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
        }
    }
    #else
    {
        /* ���б����Ǿ�̬����ģ���˲��ᱻɾ�����������������δʹ�ò����ľ��档 */
        ( void ) pxQueue;
    }
    #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
}
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )  /* ��������˸��ٹ��� */

/*******************************************************************************
 * �������ƣ�uxQueueGetQueueNumber
 * ������������ȡ���еı��
 *           �˺������ڻ�ȡ��������е�Ψһ���
 * ���������
 *   - xQueue: ���о����ָ��Ҫ��ѯ�Ķ���
 * �� �� ֵ��
 *   - UBaseType_t: ���е�Ψһ���
 * ����˵����
 *   - �˹��ܽ�������configUSE_TRACE_FACILITYʱ����
 *   - ���б�����ڵ��Ժ͸���Ŀ��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
UBaseType_t uxQueueGetQueueNumber( QueueHandle_t xQueue )
{
    /* �Ӷ��нṹ���л�ȡ���б�� */
    return ( ( Queue_t * ) xQueue )->uxQueueNumber;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )  /* ��������˸��ٹ��� */

/*******************************************************************************
 * �������ƣ�vQueueSetQueueNumber
 * �������������ö��еı��
 *           �˺�������Ϊ���з���һ��Ψһ���
 * ���������
 *   - xQueue: ���о����ָ��Ҫ���õĶ���
 *   - uxQueueNumber: Ҫ��������еı��
 * ����˵����
 *   - �˹��ܽ�������configUSE_TRACE_FACILITYʱ����
 *   - ���б�����ڵ��Ժ͸���Ŀ��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
void vQueueSetQueueNumber( QueueHandle_t xQueue, UBaseType_t uxQueueNumber )
{
    /* ���ö��нṹ���еĶ��б���ֶ� */
    ( ( Queue_t * ) xQueue )->uxQueueNumber = uxQueueNumber;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )  /* ��������˸��ٹ��� */

/*******************************************************************************
 * �������ƣ�ucQueueGetQueueType
 * ������������ȡ���е�����
 *           �˺������ڻ�ȡ���е����ͱ�ʶ
 * ���������
 *   - xQueue: ���о����ָ��Ҫ��ѯ�Ķ���
 * �� �� ֵ��
 *   - uint8_t: ���е����ͱ�ʶ
 * ����˵����
 *   - �˹��ܽ�������configUSE_TRACE_FACILITYʱ����
 *   - ������������������ͨ���С����������ź�����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
uint8_t ucQueueGetQueueType( QueueHandle_t xQueue )
{
    /* �Ӷ��нṹ���л�ȡ�������� */
    return ( ( Queue_t * ) xQueue )->ucQueueType;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvCopyDataToQueue
 * ���������������ݸ��Ƶ������е��ڲ�ʵ�ֺ���
 *           �˺�����������д����еĺ����߼���������ͬ���Ͷ��е����⴦��
 * ���������
 *   - pxQueue: ���нṹ��ָ�룬ָ��Ҫ������Ŀ�����
 *   - pvItemToQueue: Ҫд����е�����ָ��
 *   - xPosition: д��λ�ñ�ʶ��ָ������д����е�λ��
 * �� �� ֵ��
 *   - BaseType_t: ���ڻ��������У������Ƿ���Ҫ������ȣ������������pdFALSE
 * ����˵����
 *   - �˺������ٽ����ڵ��ã�ȷ��������ԭ����
 *   - �������ֶ������ͣ���ͨ���С����������ź���
 *   - ֧������д��λ�ã���β�����׺͸���д��
 *   - ���ڻ��������У��������ȼ��̳н���߼�
 *   - ������ͨ���У������λ�����ָ�����
 *   - ���ڸ���д�룬���⴦����Ϣ����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
static BaseType_t prvCopyDataToQueue( Queue_t * const pxQueue, const void *pvItemToQueue, const BaseType_t xPosition )
{
    BaseType_t xReturn = pdFALSE;          /* ����ֵ����ʼ��Ϊ����Ҫ���� */
    UBaseType_t uxMessagesWaiting;         /* ��ǰ�����е���Ϣ���� */

    /* �˺������ٽ����ڵ��� */

    /* ��ȡ��ǰ�����е���Ϣ���� */
    uxMessagesWaiting = pxQueue->uxMessagesWaiting;

    /* ���������С�Ƿ�Ϊ0���ź����򻥳����� */
    if( pxQueue->uxItemSize == ( UBaseType_t ) 0 )
    {
        #if ( configUSE_MUTEXES == 1 )  /* ��������˻��������� */
        {
            /* �����������Ƿ�Ϊ������ */
            if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
            {
                /* ���������ٱ����У�������ȼ��̳� */
                xReturn = xTaskPriorityDisinherit( ( void * ) pxQueue->pxMutexHolder );
                /* ��ջ�����������ָ�� */
                pxQueue->pxMutexHolder = NULL;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
            }
        }
        #endif /* configUSE_MUTEXES */
    }
    /* ����Ƿ�Ϊ���β������� */
    else if( xPosition == queueSEND_TO_BACK )
    {
        /* �����ݸ��Ƶ����е�д��λ�� */
        ( void ) memcpy( ( void * ) pxQueue->pcWriteTo, pvItemToQueue, ( size_t ) pxQueue->uxItemSize ); /*lint !e961 !e418 MISRA�쳣����Ϊת������ĳЩ�˿����࣬������ǰ���߼�ȷ��ֻ���ڸ��ƴ�СΪ0ʱ���ܽ���ָ�봫�ݸ�memcpy() */

        /* ����д��ָ�뵽��һ��λ�� */
        pxQueue->pcWriteTo += pxQueue->uxItemSize;
        
        /* ���д��ָ���Ƿ񳬳�����β������Ҫ���Ƶ�����ͷ�� */
        if( pxQueue->pcWriteTo >= pxQueue->pcTail ) /*lint !e946 MISRA�쳣��ָ��Ƚ����������Ľ������ */
        {
            pxQueue->pcWriteTo = pxQueue->pcHead;
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
        }
    }
    else  /* �����������ݻ򸲸�д�� */
    {
        /* �����ݸ��Ƶ����еĶ�ȡλ�ã����ڶ�����ӣ� */
        ( void ) memcpy( ( void * ) pxQueue->u.pcReadFrom, pvItemToQueue, ( size_t ) pxQueue->uxItemSize ); /*lint !e961 MISRA�쳣����Ϊת������ĳЩ�˿����� */

        /* ���¶�ȡָ�뵽ǰһ��λ�ã����ڶ�����ӣ� */
        pxQueue->u.pcReadFrom -= pxQueue->uxItemSize;
        
        /* ����ȡָ���Ƿ񳬳�����ͷ������Ҫ���Ƶ�����β�� */
        if( pxQueue->u.pcReadFrom < pxQueue->pcHead ) /*lint !e946 MISRA�쳣��ָ��Ƚ����������Ľ������ */
        {
            pxQueue->u.pcReadFrom = ( pxQueue->pcTail - pxQueue->uxItemSize );
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
        }

        /* ����Ƿ�Ϊ����д��ģʽ */
        if( xPosition == queueOVERWRITE )
        {
            /* ���������Ƿ�����Ϣ */
            if( uxMessagesWaiting > ( UBaseType_t ) 0 )
            {
                /* �������������Ǹ����������˴Ӽ�¼����Ϣ�����м�ȥ1��
                �����������ٴμ�1ʱ����¼����Ϣ����������ȷ */
                --uxMessagesWaiting;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
        }
    }

    /* ���¶����е���Ϣ����������1�� */
    pxQueue->uxMessagesWaiting = uxMessagesWaiting + 1;

    /* �����Ƿ���Ҫ������ȵı�־�����Ի����������壩 */
    return xReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvCopyDataFromQueue
 * �����������Ӷ����и������ݵ�ָ�����������ڲ�ʵ�ֺ���
 *           �˺�������Ӷ��ж�ȡ���ݵĺ����߼�������ָ����º����ݸ���
 * ���������
 *   - pxQueue: ���нṹ��ָ�룬ָ��Ҫ������Դ����
 *   - pvBuffer: Ŀ�껺����ָ�룬���ڴ洢�Ӷ��ж�ȡ������
 * ����˵����
 *   - �˺�������������Ѿ�ȷ�������������ݣ�uxMessagesWaiting > 0��
 *   - �������СΪ0�Ķ��У��ź���/������������ִ���κβ���
 *   - ����¶��еĶ�ָ�룬�����λ���������
 *   - �������ٽ����ڵ��ã�ȷ��������ԭ����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
static void prvCopyDataFromQueue( Queue_t * const pxQueue, void * const pvBuffer )
{
    /* ���������С�Ƿ�Ϊ0���ź����򻥳����� */
    if( pxQueue->uxItemSize != ( UBaseType_t ) 0 )
    {
        /* ���¶�ָ�뵽��һ��λ�� */
        pxQueue->u.pcReadFrom += pxQueue->uxItemSize;
        
        /* ����ָ���Ƿ񳬳�����β������Ҫ���Ƶ�����ͷ�� */
        if( pxQueue->u.pcReadFrom >= pxQueue->pcTail ) /*lint !e946 MISRA�쳣��ʹ�ù�ϵ��������������Ľ������ */
        {
            pxQueue->u.pcReadFrom = pxQueue->pcHead;
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
        }
        
        /* �Ӹ��º�Ķ�ָ��λ�ø������ݵ�Ŀ�껺���� */
        ( void ) memcpy( ( void * ) pvBuffer, ( void * ) pxQueue->u.pcReadFrom, ( size_t ) pxQueue->uxItemSize ); /*lint !e961 !e418 MISRA�쳣����Ϊת������ĳЩ�˿����ࡣ���⣬��ǰ���߼�ȷ��ֻ���ڼ���Ϊ0ʱ���ܽ���ָ�봫�ݸ�memcpy() */
    }
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvUnlockQueue
 * �����������������в������ڶ��������ڼ��ۻ��Ĵ��������
 *           �˺���������н����ĺ����߼����������������ڼ��ۻ��ķ��ͺͽ��ղ���
 * ���������
 *   - pxQueue: ���нṹ��ָ�룬ָ��Ҫ�����Ķ���
 * ����˵����
 *   - �˺��������ڵ��������������µ���
 *   - �������������а������ڶ��������ڼ���ӻ��Ƴ��Ķ���������
 *   - �����б�����ʱ��������ӻ��Ƴ�������ܸ����¼��б�
 *   - ����ʱ�ᴦ�����й���������Ѳ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
static void prvUnlockQueue( Queue_t * const pxQueue )
{
    /* �˺��������ڵ��������������µ��� */

    /* �������������ڶ��б������ڼ���ӻ��Ƴ��Ķ����������������
       �����б�����ʱ��������ӻ��Ƴ�����¼��б��ܸ��¡� */

    /* ������������TxLock�� */
    taskENTER_CRITICAL();
    {
        /* ��ȡ��ǰ�ķ����������� */
        int8_t cTxLock = pxQueue->cTxLock;

        /* ����ڶ��������ڼ��Ƿ������ݱ���ӵ����� */
        while( cTxLock > queueLOCKED_UNMODIFIED )
        {
            /* �ڶ��������ڼ������ݱ��������Ƿ����κ����������ȴ����ݿ��ã� */
            #if ( configUSE_QUEUE_SETS == 1 )  /* ��������˶��м����� */
            {
                /* �������Ƿ�����ĳ�����м� */
                if( pxQueue->pxQueueSetContainer != NULL )
                {
                    /* ֪ͨ���м����������ݿ��� */
                    if( prvNotifyQueueSetContainer( pxQueue, queueSEND_TO_BACK ) != pdFALSE )
                    {
                        /* �����Ƕ��м��ĳ�Ա�����ҷ��������м����¸������ȼ���������������
                           ��Ҫ�������л��� */
                        vTaskMissedYield();
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                    }
                }
                else  /* ���в������κζ��м� */
                {
                    /* ���¼��б����Ƴ������񽫱���ӵ���������б���Ϊ��������Ȼ���� */
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                    {
                        /* �ӵȴ������б����Ƴ����񲢽������ */
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                        {
                            /* �ȴ���������и��ߵ����ȼ�����˼�¼��Ҫ�������л��� */
                            vTaskMissedYield();
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                        }
                    }
                    else
                    {
                        /* û������ȴ����գ��˳�ѭ�� */
                        break;
                    }
                }
            }
            #else /* configUSE_QUEUE_SETS */  /* δ���ö��м����� */
            {
                /* ���¼��б����Ƴ������񽫱���ӵ���������б���Ϊ��������Ȼ���� */
                if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                {
                    /* �ӵȴ������б����Ƴ����񲢽������ */
                    if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                    {
                        /* �ȴ���������и��ߵ����ȼ�����˼�¼��Ҫ�������л��� */
                        vTaskMissedYield();
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                    }
                }
                else
                {
                    /* û������ȴ����գ��˳�ѭ�� */
                    break;
                }
            }
            #endif /* configUSE_QUEUE_SETS */

            /* �ݼ������������� */
            --cTxLock;
        }

        /* ��������������Ϊδ����״̬ */
        pxQueue->cTxLock = queueUNLOCKED;
    }
    taskEXIT_CRITICAL();

    /* �Խ���������RxLock��ִ����ͬ�Ĵ��� */
    taskENTER_CRITICAL();
    {
        /* ��ȡ��ǰ�Ľ����������� */
        int8_t cRxLock = pxQueue->cRxLock;

        /* ����ڶ��������ڼ��Ƿ������ݱ��Ƴ� */
        while( cRxLock > queueLOCKED_UNMODIFIED )
        {
            /* ����Ƿ����������ڵȴ��������� */
            if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
            {
                /* �ӵȴ������б����Ƴ����񲢽������ */
                if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                {
                    /* ��¼��Ҫ�������л� */
                    vTaskMissedYield();
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                }

                /* �ݼ������������� */
                --cRxLock;
            }
            else
            {
                /* û������ȴ����ͣ��˳�ѭ�� */
                break;
            }
        }

        /* ��������������Ϊδ����״̬ */
        pxQueue->cRxLock = queueUNLOCKED;
    }
    taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvIsQueueEmpty
 * �����������������Ƿ�Ϊ�յ��ڲ�ʵ�ֺ���
 *           �˺���ԭ���Եؼ������е���Ϣ�����Ƿ�Ϊ0
 * ���������
 *   - pxQueue: ���нṹ��ָ�룬ָ��Ҫ���Ķ���
 * �� �� ֵ��
 *   - BaseType_t: �������Ϊ�շ���pdTRUE�����򷵻�pdFALSE
 * ����˵����
 *   - �˺������ٽ�����ִ�У�ȷ����������ԭ����
 *   - ͨ�����uxMessagesWaiting�ֶ��ж϶����Ƿ�Ϊ��
 *   - ��������Ҫԭ���Լ�����״̬�ĳ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
static BaseType_t prvIsQueueEmpty( const Queue_t *pxQueue )
{
    BaseType_t xReturn;  /* ����ֵ����ʾ�����Ƿ�Ϊ�� */

    /* �����ٽ����������в��� */
    taskENTER_CRITICAL();
    {
        /* �������е���Ϣ�����Ƿ�Ϊ0 */
        if( pxQueue->uxMessagesWaiting == ( UBaseType_t ) 0 )
        {
            /* ����Ϊ�գ����÷���ֵΪ�� */
            xReturn = pdTRUE;
        }
        else
        {
            /* ���в�Ϊ�գ����÷���ֵΪ�� */
            xReturn = pdFALSE;
        }
    }
    /* �˳��ٽ��� */
    taskEXIT_CRITICAL();

    /* ���ض����Ƿ�Ϊ�յĽ�� */
    return xReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xQueueIsQueueEmptyFromISR
 * �������������жϷ������ISR���м������Ƿ�Ϊ��
 *           �˺������жϰ�ȫ�汾��������ISR�м������е���Ϣ�����Ƿ�Ϊ0
 * ���������
 *   - xQueue: ���о����ָ��Ҫ���Ķ���
 * �� �� ֵ��
 *   - BaseType_t: �������Ϊ�շ���pdTRUE�����򷵻�pdFALSE
 * ����˵����
 *   - �˺���רΪ�ж���������ƣ��������ٽ���
 *   - �������жϷ�����򣬲�����������������ʹ��
 *   - ������ISR�е��ã�����Ҫ�ٽ�������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
BaseType_t xQueueIsQueueEmptyFromISR( const QueueHandle_t xQueue )
{
    BaseType_t xReturn;  /* ����ֵ����ʾ�����Ƿ�Ϊ�� */

    /* ���Լ�飺ȷ�����о����Ч */
    configASSERT( xQueue );
    
    /* ֱ�Ӽ������е���Ϣ�����Ƿ�Ϊ0������Ҫ�ٽ��������� */
    if( ( ( Queue_t * ) xQueue )->uxMessagesWaiting == ( UBaseType_t ) 0 )
    {
        /* ����Ϊ�գ����÷���ֵΪ�� */
        xReturn = pdTRUE;
    }
    else
    {
        /* ���в�Ϊ�գ����÷���ֵΪ�� */
        xReturn = pdFALSE;
    }

    /* ���ض����Ƿ�Ϊ�յĽ�� */
    return xReturn;
} /*lint !e818 xQueue��������Ϊָ��const��ָ�룬��Ϊ����һ��typedef */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvIsQueueFull
 * �����������������Ƿ��������ڲ�ʵ�ֺ���
 *           �˺���ԭ���Եؼ������е���Ϣ�����Ƿ���ڶ��г���
 * ���������
 *   - pxQueue: ���нṹ��ָ�룬ָ��Ҫ���Ķ���
 * �� �� ֵ��
 *   - BaseType_t: ���������������pdTRUE�����򷵻�pdFALSE
 * ����˵����
 *   - �˺������ٽ�����ִ�У�ȷ����������ԭ����
 *   - ͨ���Ƚ�uxMessagesWaiting��uxLength�ֶ��ж϶����Ƿ�����
 *   - ��������Ҫԭ���Լ�����״̬�ĳ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
static BaseType_t prvIsQueueFull( const Queue_t *pxQueue )
{
    BaseType_t xReturn;  /* ����ֵ����ʾ�����Ƿ����� */

    /* �����ٽ����������в��� */
    taskENTER_CRITICAL();
    {
        /* �������е���Ϣ�����Ƿ���ڶ��г��� */
        if( pxQueue->uxMessagesWaiting == pxQueue->uxLength )
        {
            /* �������������÷���ֵΪ�� */
            xReturn = pdTRUE;
        }
        else
        {
            /* ����δ�������÷���ֵΪ�� */
            xReturn = pdFALSE;
        }
    }
    /* �˳��ٽ��� */
    taskEXIT_CRITICAL();

    /* ���ض����Ƿ������Ľ�� */
    return xReturn;
}
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )  /* ���������Э�̹��� */

/*******************************************************************************
 * �������ƣ�xQueueCRSend
 * ������������Э������з������ݵĺ���
 *           �˺�����Э��ר�õĶ��з��Ͳ�����֧�������ȴ�
 * ���������
 *   - xQueue: ���о����ָ��Ҫ�������ݵ�Ŀ�����
 *   - pvItemToQueue: Ҫ���͵�����ָ��
 *   - xTicksToWait: �������ʱ�䣨��ʱ�ӽ���Ϊ��λ��
 * �� �� ֵ��
 *   - BaseType_t: ���ͽ�������ܵ�ֵ����pdPASS��errQUEUE_BLOCKED��errQUEUE_FULL��errQUEUE_YIELD
 * ����˵����
 *   - �˺���רΪЭ����ƣ�������������ʹ��
 *   - �������������ָ��������ʱ�䣬Э�̻ᱻ��ӵ��ӳ��б�
 *   - ֧�ֻ��ѵȴ��������ݵ�Э��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
BaseType_t xQueueCRSend( QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait )
{
    BaseType_t xReturn;                              /* ��������ֵ */
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* �����о��ת��Ϊ���нṹ��ָ�� */

    /* ����������������ǿ�����Ҫ��������Ҫ�ٽ�������ֹ�ڼ������Ƿ�������
       �ڶ���������֮�䣬�жϴӶ������Ƴ�ĳЩ���ݡ� */
    portDISABLE_INTERRUPTS();
    {
        /* �������Ƿ����� */
        if( prvIsQueueFull( pxQueue ) != pdFALSE )
        {
            /* �������� - ��������Ҫ��������ֻ���뿪���������� */
            if( xTicksToWait > ( TickType_t ) 0 )
            {
                /* �������Ǵ�Э�̵��õģ����ǲ���ֱ�����������Ƿ���ָʾ������Ҫ������ */
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
        /* �������Ƿ��пռ� */
        if( pxQueue->uxMessagesWaiting < pxQueue->uxLength )
        {
            /* �������пռ䣬�����ݸ��Ƶ������С� */
            prvCopyDataToQueue( pxQueue, pvItemToQueue, queueSEND_TO_BACK );
            xReturn = pdPASS;

            /* �Ƿ����κ�Э���ڵȴ����ݿ��ã� */
            if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
            {
                /* ����������£�Э�̿���ֱ�ӷ�������б���Ϊ�������ٽ����ڡ�
                   �෴��ʹ������ж������¼���ͬ�Ĺ�������б���ơ� */
                if( xCoRoutineRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                {
                    /* �ȴ���Э�̾��и��ߵ����ȼ�����˼�¼������Ҫ�ò��� */
                    xReturn = errQUEUE_YIELD;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
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

#if ( configUSE_CO_ROUTINES == 1 )  /* ���������Э�̹��� */

/*******************************************************************************
 * �������ƣ�xQueueCRReceive
 * ������������Э�̽��ն������ݵĺ���
 *           �˺�����Э��ר�õĶ��н��ղ�����֧�������ȴ�
 * ���������
 *   - xQueue: ���о����ָ��Ҫ�������ݵ�Դ����
 *   - pvBuffer: ���ݻ�����ָ�룬���ڴ洢���յ�������
 *   - xTicksToWait: �������ʱ�䣨��ʱ�ӽ���Ϊ��λ��
 * �� �� ֵ��
 *   - BaseType_t: ���ս�������ܵ�ֵ����pdPASS��errQUEUE_BLOCKED��errQUEUE_FULL��errQUEUE_YIELD
 * ����˵����
 *   - �˺���רΪЭ����ƣ�������������ʹ��
 *   - �������Ϊ����ָ��������ʱ�䣬Э�̻ᱻ��ӵ��ӳ��б�
 *   - ֧�ֻ��ѵȴ��������ݵ�Э��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
BaseType_t xQueueCRReceive( QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait )
{
    BaseType_t xReturn;                              /* ��������ֵ */
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* �����о��ת��Ϊ���нṹ��ָ�� */

    /* ��������Ѿ�Ϊ�գ����ǿ�����Ҫ��������Ҫ�ٽ�������ֹ�ڼ������Ƿ�Ϊ�պ�
       �ڶ���������֮�䣬�ж���������ĳЩ���ݡ� */
    portDISABLE_INTERRUPTS();
    {
        /* �������Ƿ�Ϊ�� */
        if( pxQueue->uxMessagesWaiting == ( UBaseType_t ) 0 )
        {
            /* ������û����Ϣ����������Ҫ��������ֻ���뿪������ȡ�κ����ݣ� */
            if( xTicksToWait > ( TickType_t ) 0 )
            {
                /* ��������һ��Э�̣����ǲ���ֱ�����������Ƿ���ָʾ������Ҫ������ */
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
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
        }
    }
    portENABLE_INTERRUPTS();

    portDISABLE_INTERRUPTS();
    {
        /* ���������Ƿ������� */
        if( pxQueue->uxMessagesWaiting > ( UBaseType_t ) 0 )
        {
            /* ���ԴӶ����л�ȡ���ݡ� */
            pxQueue->u.pcReadFrom += pxQueue->uxItemSize;
            if( pxQueue->u.pcReadFrom >= pxQueue->pcTail )
            {
                pxQueue->u.pcReadFrom = pxQueue->pcHead;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
            }
            --( pxQueue->uxMessagesWaiting );
            ( void ) memcpy( ( void * ) pvBuffer, ( void * ) pxQueue->u.pcReadFrom, ( unsigned ) pxQueue->uxItemSize );

            xReturn = pdPASS;

            /* �Ƿ����κ�Э���ڵȴ��ռ���ã� */
            if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
            {
                /* ����������£�Э�̿���ֱ�ӷ�������б���Ϊ�������ٽ����ڡ�
                   �෴��ʹ������ж������¼���ͬ�Ĺ�������б���ơ� */
                if( xCoRoutineRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                {
                    xReturn = errQUEUE_YIELD;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
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

#if ( configUSE_CO_ROUTINES == 1 )  /* ���������Э�̹��� */

/*******************************************************************************
 * �������ƣ�xQueueCRSendFromISR
 * �������������жϷ����������з������ݵ�Э�̰汾
 *           �˺������жϰ�ȫ��Э�̶��з��Ͳ���
 * ���������
 *   - xQueue: ���о����ָ��Ҫ�������ݵ�Ŀ�����
 *   - pvItemToQueue: Ҫ���͵�����ָ��
 *   - xCoRoutinePreviouslyWoken: ָʾ�Ƿ��Ѿ���Э�̱����ѵı�־
 * �� �� ֵ��
 *   - BaseType_t: ���������Э�̷���pdTRUE�����򷵻�xCoRoutinePreviouslyWoken
 * ����˵����
 *   - �˺���רΪ�ж���������ƣ�������������ʹ��
 *   - �����������������ִ���κβ���
 *   - ÿ��ISRֻ����һ��Э��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
BaseType_t xQueueCRSendFromISR( QueueHandle_t xQueue, const void *pvItemToQueue, BaseType_t xCoRoutinePreviouslyWoken )
{
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* �����о��ת��Ϊ���нṹ��ָ�� */

    /* ������ISR��������������������û�пռ䣬���˳�����ִ���κβ����� */
    if( pxQueue->uxMessagesWaiting < pxQueue->uxLength )
    {
        /* �����ݸ��Ƶ������� */
        prvCopyDataToQueue( pxQueue, pvItemToQueue, queueSEND_TO_BACK );

        /* ����ÿ��ISRֻ�뻽��һ��Э�̣���˼���Ƿ��Ѿ���Э�̱����ѡ� */
        if( xCoRoutinePreviouslyWoken == pdFALSE )
        {
            /* ����Ƿ���Э���ڵȴ��������� */
            if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
            {
                /* ���¼��б����Ƴ�Э�̲�������� */
                if( xCoRoutineRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                {
                    return pdTRUE;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
        }
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
    }

    return xCoRoutinePreviouslyWoken;
}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/
#if ( configUSE_CO_ROUTINES == 1 )  /* ���������Э�̹��� */

/*******************************************************************************
 * �������ƣ�xQueueCRReceiveFromISR
 * �������������жϷ��������ն������ݵ�Э�̰汾
 *           �˺������жϰ�ȫ��Э�̶��н��ղ���
 * ���������
 *   - xQueue: ���о����ָ��Ҫ�������ݵ�Դ����
 *   - pvBuffer: ���ݻ�����ָ�룬���ڴ洢���յ�������
 *   - pxCoRoutineWoken: ָ��Э�̻��ѱ�־��ָ��
 * �� �� ֵ��
 *   - BaseType_t: �ɹ����յ����ݷ���pdPASS������Ϊ�շ���pdFAIL
 * ����˵����
 *   - �˺���רΪ�ж���������ƣ�������������ʹ��
 *   - �������Ϊ�գ�����ִ���κβ���
 *   - ֧�ֻ��ѵȴ��������ݵ�Э��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
BaseType_t xQueueCRReceiveFromISR( QueueHandle_t xQueue, void *pvBuffer, BaseType_t *pxCoRoutineWoken )
{
    BaseType_t xReturn;                              /* ��������ֵ */
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* �����о��ת��Ϊ���нṹ��ָ�� */

    /* ���ǲ��ܴ�ISR��������˼���Ƿ������ݿ��á����û�У����뿪����ִ���κβ����� */
    if( pxQueue->uxMessagesWaiting > ( UBaseType_t ) 0 )
    {
        /* �Ӷ����и������ݡ� */
        pxQueue->u.pcReadFrom += pxQueue->uxItemSize;
        if( pxQueue->u.pcReadFrom >= pxQueue->pcTail )
        {
            pxQueue->u.pcReadFrom = pxQueue->pcHead;
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
        }
        --( pxQueue->uxMessagesWaiting );
        ( void ) memcpy( ( void * ) pvBuffer, ( void * ) pxQueue->u.pcReadFrom, ( unsigned ) pxQueue->uxItemSize );

        /* ����Ƿ��Ѿ���Э�̱����� */
        if( ( *pxCoRoutineWoken ) == pdFALSE )
        {
            /* ����Ƿ���Э���ڵȴ��������� */
            if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
            {
                /* ���¼��б����Ƴ�Э�̲�������� */
                if( xCoRoutineRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                {
                    *pxCoRoutineWoken = pdTRUE;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
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

#if ( configQUEUE_REGISTRY_SIZE > 0 )  /* ��������˶���ע����� */

/*******************************************************************************
 * �������ƣ�vQueueAddToRegistry
 * ������������������ӵ�����ע�����
 *           �˺������ڽ������������ƹ��������ڵ��Ժ�ʶ��
 * ���������
 *   - xQueue: ���о����ָ��Ҫע��Ķ���
 *   - pcQueueName: ���������ַ���
 * ����˵����
 *   - �˹��ܽ���configQUEUE_REGISTRY_SIZE > 0ʱ����
 *   - ����ע������ڵ���Ŀ�ģ�����ʶ�����
 *   - �����ע�����ҿ�λ����������Ϣ�洢�ڵ�һ����λ��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
void vQueueAddToRegistry( QueueHandle_t xQueue, const char *pcQueueName ) /*lint !e971 ����δ�޶���char���ͽ������ַ����͵����ַ� */
{
    UBaseType_t ux;  /* ѭ�������� */

    /* �鿴ע������Ƿ��п�λ��NULL���Ʊ�ʾ���в�λ�� */
    for( ux = ( UBaseType_t ) 0U; ux < ( UBaseType_t ) configQUEUE_REGISTRY_SIZE; ux++ )
    {
        if( xQueueRegistry[ ux ].pcQueueName == NULL )
        {
            /* �洢�˶��е���Ϣ�� */
            xQueueRegistry[ ux ].pcQueueName = pcQueueName;
            xQueueRegistry[ ux ].xHandle = xQueue;

            /* ���ٶ���ע�������¼� */
            traceQUEUE_REGISTRY_ADD( xQueue, pcQueueName );
            break;
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
        }
    }
}

#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )  /* ��������˶���ע����� */

/*******************************************************************************
 * �������ƣ�pcQueueGetName
 * ������������ȡ���е�����
 *           �˺������ڴӶ���ע����в��Ҷ��е�����
 * ���������
 *   - xQueue: ���о����ָ��Ҫ��ѯ�Ķ���
 * �� �� ֵ��
 *   - const char *: ���е����ƣ����δ�ҵ��򷵻�NULL
 * ����˵����
 *   - �˹��ܽ���configQUEUE_REGISTRY_SIZE > 0ʱ����
 *   - ע������û�б������Ʒ�ֹ��������������ע���ʱ��ӻ�ɾ����Ŀ
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
const char *pcQueueGetName( QueueHandle_t xQueue ) /*lint !e971 ����δ�޶���char���ͽ������ַ����͵����ַ� */
{
    UBaseType_t ux;                                  /* ѭ�������� */
    const char *pcReturn = NULL;                     /* ����ֵ����ʼ��ΪNULL */

    /* ע�⣺����û�б������Ʒ�ֹ��������������ע���ʱ��ӻ�ɾ����Ŀ�� */
    for( ux = ( UBaseType_t ) 0U; ux < ( UBaseType_t ) configQUEUE_REGISTRY_SIZE; ux++ )
    {
        if( xQueueRegistry[ ux ].xHandle == xQueue )
        {
            /* �ҵ�ƥ��Ķ��о�������ض�Ӧ������ */
            pcReturn = xQueueRegistry[ ux ].pcQueueName;
            break;
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
        }
    }

    return pcReturn;
}

#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )  /* ��������˶���ע����� */

/*******************************************************************************
 * �������ƣ�vQueueUnregisterQueue
 * �����������Ӷ���ע�����ע������
 *           �˺������ڴӶ���ע������Ƴ����е�ע����Ϣ
 * ���������
 *   - xQueue: ���о����ָ��Ҫע���Ķ���
 * ����˵����
 *   - �˹��ܽ���configQUEUE_REGISTRY_SIZE > 0ʱ����
 *   - �����ע������ƥ��Ķ��о����Ȼ����ոò�λ
 *   - ����������ΪNULL��ʾ��λ���У����������ΪNULL��ֹ�ظ����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
void vQueueUnregisterQueue( QueueHandle_t xQueue )
{
    UBaseType_t ux;  /* ѭ�������� */

    /* �鿴����ע���Ķ��о���Ƿ�ʵ����ע����С� */
    for( ux = ( UBaseType_t ) 0U; ux < ( UBaseType_t ) configQUEUE_REGISTRY_SIZE; ux++ )
    {
        if( xQueueRegistry[ ux ].xHandle == xQueue )
        {
            /* ����������ΪNULL����ʾ�˲�λ�ٴο��С� */
            xQueueRegistry[ ux ].pcQueueName = NULL;

            /* ���������ΪNULL��ȷ����ͬ�Ķ��о���������γ�����ע����У�
               ���������ӡ��Ƴ���Ȼ���ٴ���ӡ� */
            xQueueRegistry[ ux ].xHandle = ( QueueHandle_t ) 0;
            break;
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
        }
    }

} /*lint !e818 xQueue��������Ϊָ��const��ָ�룬��Ϊ����һ��typedef */

#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configUSE_TIMERS == 1 )  /* ��������˶�ʱ������ */

/*******************************************************************************
 * �������ƣ�vQueueWaitForMessageRestricted
 * �������������޵صȴ�������Ϣ���ڲ�����
 *           �˺���רΪ�ں˴�����ƣ�������ĵ���Ҫ��
 * ���������
 *   - xQueue: ���о����ָ��Ҫ�ȴ��Ķ���
 *   - xTicksToWait: ���ȴ�ʱ�䣨��ʱ�ӽ���Ϊ��λ��
 *   - xWaitIndefinitely: �Ƿ������ڵȴ��ı�־
 * ����˵����
 *   - �˺�����Ӧ��Ӧ�ó��������ã������������'Restricted'
 *   - �����ǹ���API��һ���֣�רΪ�ں˴������
 *   - ����ʱӦ�����������������Ǵ��ٽ����ڵ���
 *   - ���������û����Ϣ���Ὣ��������������б��У���������������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
void vQueueWaitForMessageRestricted( QueueHandle_t xQueue, TickType_t xTicksToWait, const BaseType_t xWaitIndefinitely )
{
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;  /* �����о��ת��Ϊ���нṹ��ָ�� */

    /* �˺�����Ӧ��Ӧ�ó��������ã������������'Restricted'��
       �����ǹ���API��һ���֡���רΪ�ں˴�����ƣ�������ĵ���Ҫ��
       �����ܵ���vListInsert()��һ������ֻ��һ����Ŀ���б��ϱ����ã�
       ����б������ܿ죬����ʹ��ˣ�ҲӦ���ڵ���������������µ��ã�
       �����Ǵ��ٽ����ڵ��á� */

    /* ֻ���ڶ�����û����Ϣʱ��ִ���κβ������˺�������ʵ�ʵ�������������
       ֻ�ǽ�������������б��С�����������ֱ������������ - ��ʱ��ִ��yield��
       ����ڶ�������ʱ������ӵ����У����ҵ��������ڶ�����������
       �򵱶��н���ʱ����������������������� */
    prvLockQueue( pxQueue );
    if( pxQueue->uxMessagesWaiting == ( UBaseType_t ) 0U )
    {
        /* ������û���κ����ݣ�����ָ����ʱ��Ρ� */
        vTaskPlaceOnEventListRestricted( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait, xWaitIndefinitely );
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
    }
    prvUnlockQueue( pxQueue );
}

#endif /* configUSE_TIMERS */
/*-----------------------------------------------------------*/

#if( ( configUSE_QUEUE_SETS == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )  /* ��������˶��м����ܺͶ�̬���� */

/*******************************************************************************
 * �������ƣ�xQueueCreateSet
 * �����������������м�
 *           �˺������ڴ���һ�����м���������������ͬһ�����Ϸ�������
 * ���������
 *   - uxEventQueueLength: ���м����¼����г���
 * �� �� ֵ��
 *   - QueueSetHandle_t: �´����Ķ��м�������������ʧ���򷵻�NULL
 * ����˵����
 *   - �˹��ܽ������ö��м����ܺͶ�̬����ʱ����
 *   - ���м���һ������Ķ��У����ڼ���������е��¼�
 *   - ʹ��xQueueGenericCreate�������м���ָ����������ΪqueueQUEUE_TYPE_SET
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
QueueSetHandle_t xQueueCreateSet( const UBaseType_t uxEventQueueLength )
{
    QueueSetHandle_t pxQueue;  /* ���м���� */

    /* �������м������СΪQueue_tָ��Ĵ�С������Ϊ���м� */
    pxQueue = xQueueGenericCreate( uxEventQueueLength, sizeof( Queue_t * ), queueQUEUE_TYPE_SET );

    return pxQueue;
}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )  /* ��������˶��м����� */

/*******************************************************************************
 * �������ƣ�xQueueAddToSet
 * ���������������л��ź�����ӵ����м���
 *           �˺������ڽ����л��ź���ע�ᵽ���м���ʹ���Ϊ���м��ĳ�Ա
 * ���������
 *   - xQueueOrSemaphore: ���л��ź��������ָ��Ҫ��ӵ����м��ĳ�Ա
 *   - xQueueSet: ���м������ָ��Ŀ����м�
 * �� �� ֵ��
 *   - BaseType_t: �ɹ���ӵ����м�����pdPASS��ʧ�ܷ���pdFAIL
 * ����˵����
 *   - �˹��ܽ������ö��м�����ʱ����
 *   - һ������/�ź���ֻ������һ�����м�
 *   - �������/�ź��������������������ӵ����м�
 *   - �������ٽ�����ִ�У�ȷ��ԭ����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
BaseType_t xQueueAddToSet( QueueSetMemberHandle_t xQueueOrSemaphore, QueueSetHandle_t xQueueSet )
{
    BaseType_t xReturn;  /* ��������ֵ */

    /* �����ٽ����������� */
    taskENTER_CRITICAL();
    {
        /* ������/�ź����Ƿ��Ѿ�����ĳ�����м� */
        if( ( ( Queue_t * ) xQueueOrSemaphore )->pxQueueSetContainer != NULL )
        {
            /* ���ܽ�����/�ź�����ӵ�������м��С� */
            xReturn = pdFAIL;
        }
        /* ������/�ź������Ƿ��������� */
        else if( ( ( Queue_t * ) xQueueOrSemaphore )->uxMessagesWaiting != ( UBaseType_t ) 0 )
        {
            /* �������/�ź�����������������ܽ�����ӵ����м��С� */
            xReturn = pdFAIL;
        }
        else
        {
            /* ���ö���/�ź����Ķ��м�����ָ�� */
            ( ( Queue_t * ) xQueueOrSemaphore )->pxQueueSetContainer = xQueueSet;
            xReturn = pdPASS;
        }
    }
    /* �˳��ٽ��� */
    taskEXIT_CRITICAL();

    return xReturn;
}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )  /* ��������˶��м����� */

/*******************************************************************************
 * �������ƣ�xQueueRemoveFromSet
 * �����������Ӷ��м����Ƴ����л��ź���
 *           �˺������ڽ����л��ź����Ӷ��м����Ƴ���ȡ�����Ա�ʸ�
 * ���������
 *   - xQueueOrSemaphore: ���л��ź��������ָ��Ҫ�Ӷ��м��Ƴ��ĳ�Ա
 *   - xQueueSet: ���м������ָ��Դ���м�
 * �� �� ֵ��
 *   - BaseType_t: �ɹ��Ӷ��м��Ƴ�����pdPASS��ʧ�ܷ���pdFAIL
 * ����˵����
 *   - �˹��ܽ������ö��м�����ʱ����
 *   - �������/�ź�������ָ�����м��ĳ�Ա�����Ƴ�ʧ��
 *   - �������/�ź���������������Ƴ�ʧ�ܣ����������¼���
 *   - �������ٽ�����ִ�У�ȷ��ԭ����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
BaseType_t xQueueRemoveFromSet( QueueSetMemberHandle_t xQueueOrSemaphore, QueueSetHandle_t xQueueSet )
{
    BaseType_t xReturn;                              /* ��������ֵ */
    Queue_t * const pxQueueOrSemaphore = ( Queue_t * ) xQueueOrSemaphore;  /* �����ת��Ϊ���нṹ��ָ�� */

    /* ������/�ź����Ƿ�����ָ���Ķ��м� */
    if( pxQueueOrSemaphore->pxQueueSetContainer != xQueueSet )
    {
        /* �ö��в��Ǽ��ϵĳ�Ա�� */
        xReturn = pdFAIL;
    }
    /* ������/�ź������Ƿ��������� */
    else if( pxQueueOrSemaphore->uxMessagesWaiting != ( UBaseType_t ) 0 )
    {
        /* �����в�Ϊ��ʱ�Ӽ������Ƴ�������Σ�յģ���Ϊ���м��Խ�����ö��еĹ����¼��� */
        xReturn = pdFAIL;
    }
    else
    {
        /* �����ٽ����������� */
        taskENTER_CRITICAL();
        {
            /* ���в��ٰ����ڼ����С� */
            pxQueueOrSemaphore->pxQueueSetContainer = NULL;
        }
        /* �˳��ٽ��� */
        taskEXIT_CRITICAL();
        xReturn = pdPASS;
    }

    return xReturn;
} /*lint !e818 xQueueSet��������Ϊָ��const��ָ�룬��Ϊ����һ��typedef */

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xQueueSelectFromSet
 * �����������Ӷ��м���ѡ�������ݵĳ�Ա
 *           �˺������ڴӶ��м��н��������ݿ��õĶ��л��ź������
 * ���������
 *   - xQueueSet: ���м������ָ��Ҫ�����Ķ��м�
 *   - xTicksToWait: ���ȴ�ʱ�䣨��ʱ�ӽ���Ϊ��λ��
 * �� �� ֵ��
 *   - QueueSetMemberHandle_t: �����ݿ��õĶ��л��ź�������������ʱ�򷵻�NULL
 * ����˵����
 *   - �˹��ܽ������ö��м�����ʱ����
 *   - ʹ�ö��н��ղ����Ӷ��м��л�ȡ�����ݵĳ�Ա���
 *   - ֧�������ȴ���ֱ���г�Ա�����ݿ��û�ʱ
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
 #if ( configUSE_QUEUE_SETS == 1 )  /* ��������˶��м����� */
 
QueueSetMemberHandle_t xQueueSelectFromSet( QueueSetHandle_t xQueueSet, TickType_t const xTicksToWait )
{
    QueueSetMemberHandle_t xReturn = NULL;  /* ����ֵ����ʼ��ΪNULL */

    /* ʹ��ͨ�ö��н��պ����Ӷ��м��н������ݣ���Ա����� */
    ( void ) xQueueGenericReceive( ( QueueHandle_t ) xQueueSet, &xReturn, xTicksToWait, pdFALSE ); /*lint !e961 ��һ��typedefת������һ��typedef��������� */
    return xReturn;
}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/
#if ( configUSE_QUEUE_SETS == 1 )  /* ��������˶��м����� */

/*******************************************************************************
 * �������ƣ�xQueueSelectFromSetFromISR
 * �������������жϷ������Ӷ��м���ѡ�������ݵĳ�Ա
 *           �˺������жϰ�ȫ�汾��������ISR�дӶ��м���ȡ�����ݿ��õĳ�Ա���
 * ���������
 *   - xQueueSet: ���м������ָ��Ҫ�����Ķ��м�
 * �� �� ֵ��
 *   - QueueSetMemberHandle_t: �����ݿ��õĶ��л��ź�����������û���򷵻�NULL
 * ����˵����
 *   - �˹��ܽ������ö��м�����ʱ����
 *   - ʹ���жϰ�ȫ�Ķ��н��ղ����Ӷ��м��л�ȡ��Ա���
 *   - �����������������ؽ��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
QueueSetMemberHandle_t xQueueSelectFromSetFromISR( QueueSetHandle_t xQueueSet )
{
    QueueSetMemberHandle_t xReturn = NULL;  /* ����ֵ����ʼ��ΪNULL */

    /* ʹ���жϰ�ȫ�Ķ��н��պ����Ӷ��м��н������ݣ���Ա����� */
    ( void ) xQueueReceiveFromISR( ( QueueHandle_t ) xQueueSet, &xReturn, NULL ); /*lint !e961 ��һ��typedefת������һ��typedef��������� */
    return xReturn;
}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )  /* ��������˶��м����� */

/*******************************************************************************
 * �������ƣ�prvNotifyQueueSetContainer
 * ����������֪ͨ���м��������ڲ�����
 *           ������������ʱ���˺�������֪ͨ�������Ķ��м�
 * ���������
 *   - pxQueue: ���нṹ��ָ�룬ָ�������ݵĶ���
 *   - xCopyPosition: ���ݸ���λ�ñ�ʶ
 * �� �� ֵ��
 *   - BaseType_t: ��������˸������ȼ������񷵻�pdTRUE�����򷵻�pdFALSE
 * ����˵����
 *   - �˹��ܽ������ö��м�����ʱ����
 *   - �˺����������ٽ����ڵ���
 *   - �����о�����Ƶ����м��У���ʾ�ö��������ݿ���
 *   - ������м�����״̬�µ�֪ͨ����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/04     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
static BaseType_t prvNotifyQueueSetContainer( const Queue_t * const pxQueue, const BaseType_t xCopyPosition )
{
    Queue_t *pxQueueSetContainer = pxQueue->pxQueueSetContainer;  /* ���м�����ָ�� */
    BaseType_t xReturn = pdFALSE;                                 /* ����ֵ����ʼ��ΪpdFALSE */

    /* �˺���������ٽ����ڵ��� */

    /* ���Լ�飺ȷ�����м�������Ч */
    configASSERT( pxQueueSetContainer );
    /* ���Լ�飺ȷ�����м��пռ� */
    configASSERT( pxQueueSetContainer->uxMessagesWaiting < pxQueueSetContainer->uxLength );

    /* �ٴμ����м��Ƿ��пռ� */
    if( pxQueueSetContainer->uxMessagesWaiting < pxQueueSetContainer->uxLength )
    {
        /* ��ȡ���м��ķ�������״̬ */
        const int8_t cTxLock = pxQueueSetContainer->cTxLock;

        /* ���ٶ��з����¼� */
        traceQUEUE_SEND( pxQueueSetContainer );

        /* ���Ƶ������ǰ������ݵĶ��еľ���� */
        xReturn = prvCopyDataToQueue( pxQueueSetContainer, &pxQueue, xCopyPosition );

        /* �����м��Ƿ�δ���� */
        if( cTxLock == queueUNLOCKED )
        {
            /* ����Ƿ����������ڵȴ��Ӷ��м��������� */
            if( listLIST_IS_EMPTY( &( pxQueueSetContainer->xTasksWaitingToReceive ) ) == pdFALSE )
            {
                /* �ӵȴ������б����Ƴ����񲢽������ */
                if( xTaskRemoveFromEventList( &( pxQueueSetContainer->xTasksWaitingToReceive ) ) != pdFALSE )
                {
                    /* �ȴ���������и��ߵ����ȼ��� */
                    xReturn = pdTRUE;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
            }
        }
        else  /* ���м���������״̬ */
        {
            /* ���ӷ�����������������ʱ�ᴦ�����Ĳ��� */
            pxQueueSetContainer->cTxLock = ( int8_t ) ( cTxLock + 1 );
        }
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ��շ�֧�� */
    }

    return xReturn;
}

#endif /* configUSE_QUEUE_SETS */













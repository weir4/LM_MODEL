/*******************************************************************************
 * ��Ȩ���� (C)2025, CQG
 *
 * �ļ����ƣ� LM_timers.c
 * �ļ���ʶ�� 
 * ����ժҪ�� ��ʱ��ģ�鶨��
 * ����˵���� ��
 * ��ǰ�汾�� FreeRTOS V9.0.0
 * ��    �ߣ� Qiguo_Cui                   
 * ������ڣ� 2025��09��01��
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
 * ��ʱ�����ƽṹ�嶨��
 * ������ʱ�����������Ժ�������Ϣ
 */
 #if ( configUSE_TIMERS == 1 )
 
typedef struct tmrTimerControl
{
	const char				*pcTimerName;		/*<< �ı����ƣ����ڵ���Ŀ�� */
	ListItem_t				xTimerListItem;		/*<< ��׼����������¼����� */
	TickType_t				xTimerPeriodInTicks;/*<< ��ʱ�����ڵ�ʱ�������δ����� */
	UBaseType_t				uxAutoReload;		/*<< ����ΪpdTRUE��ʾ��ʱ���Զ����أ�pdFALSE��ʾ���ζ�ʱ�� */
	void 					*pvTimerID;			/*<< ��ʱ����ʶ������������ʱ��ʹ����ͬ�ص�����ʱ�������� */
	TimerCallbackFunction_t	pxCallbackFunction;	/*<< ��ʱ������ʱ���õĺ��� */
	#if( configUSE_TRACE_FACILITY == 1 )
		UBaseType_t			uxTimerNumber;		/*<< ���ٹ��߷����ID */
	#endif

	#if( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
		uint8_t 			ucStaticallyAllocated; /*<< �����ʱ���Ǿ�̬�����ģ�����ΪpdTRUE������ɾ��ʱ�����ͷ��ڴ� */
	#endif
} xTIMER;

/**
 * ��ʱ�����Ͷ���
 * ������ɰ汾�ļ�����
 */
typedef xTIMER Timer_t;

/**
 * ��ʱ�������ṹ�嶨��
 * ���ڶ�ʱ��������Ϣ�в�����ʱ���Ĳ���
 */
typedef struct tmrTimerParameters
{
	TickType_t			xMessageValue;		/*<< ��ѡֵ������ĳЩ�������Ķ�ʱ������ */
	Timer_t *			pxTimer;			/*<< ҪӦ������Ķ�ʱ�� */
} TimerParameter_t;

/**
 * �ص������ṹ�嶨��
 * ���ڶ�ʱ��������Ϣ��ִ�зǶ�ʱ����ػص��Ĳ���
 */
typedef struct tmrCallbackParameters
{
	PendedFunction_t	pxCallbackFunction;	/*<< Ҫִ�еĻص����� */
	void *pvParameter1;						/*<< �ص������ĵ�һ������ֵ */
	uint32_t ulParameter2;					/*<< �ص������ĵڶ�������ֵ */
} CallbackParameters_t;

/**
 * ��ʱ��������Ϣ�ṹ�嶨��
 * ����������Ϣ�����Լ�����ȷ��������Ϣ������Ч�ı�ʶ��
 */
typedef struct tmrTimerQueueMessage
{
	BaseType_t			xMessageID;			/*<< ���͸���ʱ��������������� */
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
 * ���ӳٳ�������
 * ���ڱ�ʾ���ӳٵĶ�ʱ������
 */
#define tmrNO_DELAY		( TickType_t ) 0U

/* Exported macro ------------------------------------------------------------*/
/* ע����ʱ��ģ��û�е����ĺ궨�� */

/* Exported functions --------------------------------------------------------*/
/* ע����ʱ��ģ���API������timers.h���������˴����ظ� */

/* Private types -------------------------------------------------------------*/
/* ע��˽�����Ͷ����Ѱ����ڵ��������� */

/* Private variables ---------------------------------------------------------*/
/*lint -e956 �ֶ�����ȷ����Щ��̬������������Ϊvolatile */

/**
 * ���ʱ���б�
 * �洢���ʱ�����б�������ʱ�������������ʱ����ǰ
 * ֻ�ж�ʱ������������Է�����Щ�б�
 */
PRIVILEGED_DATA static List_t xActiveTimerList1;
PRIVILEGED_DATA static List_t xActiveTimerList2;
PRIVILEGED_DATA static List_t *pxCurrentTimerList;
PRIVILEGED_DATA static List_t *pxOverflowTimerList;

/**
 * ��ʱ������
 * ������ʱ����������������Ķ���
 */
PRIVILEGED_DATA static QueueHandle_t xTimerQueue = NULL;

/**
 * ��ʱ��������
 * ��ʱ����������ľ��
 */
PRIVILEGED_DATA static TaskHandle_t xTimerTaskHandle = NULL;

/*lint +e956 */

/* Private constants ---------------------------------------------------------*/
/* ע����ʱ��ģ��û��˽�г������� */

/* Private macros ------------------------------------------------------------*/
/* ע����ʱ��ģ��û��˽�к궨�� */

/* Private functions ---------------------------------------------------------*/
/**
 * ��鶨ʱ���б�Ͷ��е���Ч��
 * �����δ��ʼ�������ʼ����ʱ����������ʹ�õĻ�����ʩ
 */
static void prvCheckForValidListAndQueue( void ) PRIVILEGED_FUNCTION;

/**
 * ��ʱ�����������ػ����̣�
 * ��ʱ�������ɴ�������ƣ���������ʹ��xTimerQueue�����붨ʱ����������ͨ��
 * @param pvParameters �������
 */
static void prvTimerTask( void *pvParameters ) PRIVILEGED_FUNCTION;

/**
 * ������յ�������
 * �ɶ�ʱ������������ã����ͺʹ����ڶ�ʱ�������Ͻ��յ�������
 */
static void prvProcessReceivedCommands( void ) PRIVILEGED_FUNCTION;

/**
 * ����ʱ�������б�
 * ���ݵ���ʱ���Ƿ��¶�ʱ�����������������ʱ������xActiveTimerList1��xActiveTimerList2
 * @param pxTimer Ҫ����Ķ�ʱ��
 * @param xNextExpiryTime ��һ������ʱ��
 * @param xTimeNow ��ǰʱ��
 * @param xCommandTime ����ʱ��
 * @return ������
 */
static BaseType_t prvInsertTimerInActiveList( Timer_t * const pxTimer, const TickType_t xNextExpiryTime, const TickType_t xTimeNow, const TickType_t xCommandTime ) PRIVILEGED_FUNCTION;

/**
 * �����ڶ�ʱ��
 * ���ʱ���Ѵﵽ�䵽��ʱ�䣬������Զ����ض�ʱ�������¼��أ�Ȼ�������ص�����
 * @param xNextExpireTime ��һ������ʱ��
 * @param xTimeNow ��ǰʱ��
 */
static void prvProcessExpiredTimer( const TickType_t xNextExpireTime, const TickType_t xTimeNow ) PRIVILEGED_FUNCTION;

/**
 * �л���ʱ���б�
 * �δ�������������ȷ����ǰ��ʱ���б�������ĳЩ��ʱ�����л���ʱ���б�
 */
static void prvSwitchTimerLists( void ) PRIVILEGED_FUNCTION;

/**
 * ��ȡ��ǰ�δ����
 * ������ϴε���prvSampleTimeNow()���������˵δ�����������pxTimerListsWereSwitched����ΪpdTRUE
 * @param pxTimerListsWereSwitched ָʾ�Ƿ����б��л���ָ��
 * @return ��ǰ�δ����
 */
static TickType_t prvSampleTimeNow( BaseType_t * const pxTimerListsWereSwitched ) PRIVILEGED_FUNCTION;

/**
 * ��ȡ��һ������ʱ��
 * �����ʱ���б�����κλ��ʱ�����򷵻����ȵ��ڵĶ�ʱ���ĵ���ʱ�䲢��pxListWasEmpty����Ϊfalse
 * �����ʱ���б������κζ�ʱ�����򷵻�0����pxListWasEmpty����ΪpdTRUE
 * @param pxListWasEmpty ָʾ�б��Ƿ�Ϊ�յ�ָ��
 * @return ��һ������ʱ��
 */
static TickType_t prvGetNextExpireTime( BaseType_t * const pxListWasEmpty ) PRIVILEGED_FUNCTION;

/**
 * ����ʱ������������
 * �����ʱ���ѵ��ڣ�������������������ʱ����������ֱ����ʱ�����ڻ��յ�����
 * @param xNextExpireTime ��һ������ʱ��
 * @param xListWasEmpty �б��Ƿ�Ϊ��
 */
static void prvProcessTimerOrBlockTask( const TickType_t xNextExpireTime, BaseType_t xListWasEmpty ) PRIVILEGED_FUNCTION;

static void prvInitialiseNewTimer(	const char * const pcTimerName,
									const TickType_t xTimerPeriodInTicks,
									const UBaseType_t uxAutoReload,
									void * const pvTimerID,
									TimerCallbackFunction_t pxCallbackFunction,
									Timer_t *pxNewTimer ) PRIVILEGED_FUNCTION; 
/*******************************************************************************
 * �������ƣ�xTimerCreateTimerTask
 * ��������������FreeRTOS��ʱ�������������ڴ��������ʱ���ĵ��ںͻص�����ִ��
 *           �˺����ڵ���������ʱ�Զ����ã������ʼ����ʱ��������������Ļ�����ʩ
 * �����������
 * �����������
 * �� �� ֵ��
 *   - BaseType_t: ���񴴽������pdPASS��ʾ�ɹ���pdFAIL��ʾʧ��
 * ����˵����
 *   - �˺����ڵ���������ʱ�Զ����ã���configUSE_TIMERS����Ϊ1ʱ����
 *   - ���ȼ�鶨ʱ��������������Ļ�����ʩ�Ƿ��Ѵ���/��ʼ��
 *   - ֧�־�̬�Ͷ�̬�����ڴ���䷽ʽ������ʱ������
 *   - ��ʱ����������Ȩģʽ���У����нϸߵ����ȼ�
 *   - �����ʱ�����д���ʧ�ܣ����޷�������ʱ������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
BaseType_t xTimerCreateTimerTask( void )
{
    BaseType_t xReturn = pdFAIL;  /* ����ֵ����ʼ��Ϊʧ��״̬ */

    /* ��������������configUSE_TIMERS����Ϊ1ʱ���ô˺�����
       ��鶨ʱ����������ʹ�õĻ�����ʩ�Ƿ��Ѵ���/��ʼ����
       �����ʱ���Ѿ����������ʼ���Ѿ���� */
    prvCheckForValidListAndQueue();  /* �����Ч���б�Ͷ��� */

    /* ��鶨ʱ�������Ƿ�ɹ����� */
    if( xTimerQueue != NULL )
    {
        /* ��������ѡ��̬��̬�ڴ���䷽ʽ */
        #if( configSUPPORT_STATIC_ALLOCATION == 1 )
        {
            StaticTask_t *pxTimerTaskTCBBuffer = NULL;  /* ��̬TCB������ָ�� */
            StackType_t *pxTimerTaskStackBuffer = NULL; /* ��̬ջ������ָ�� */
            uint32_t ulTimerTaskStackSize;              /* ��ʱ������ջ��С */

            /* ��ȡ��ʱ��������ڴ����ã�TCB��ջ�������� */
            vApplicationGetTimerTaskMemory( &pxTimerTaskTCBBuffer, &pxTimerTaskStackBuffer, &ulTimerTaskStackSize );
            
            /* ʹ�þ�̬���䷽ʽ������ʱ������ */
            xTimerTaskHandle = xTaskCreateStatic( prvTimerTask,              /* ��ʱ�������� */
                                                  "Tmr Svc",                 /* �������� */
                                                  ulTimerTaskStackSize,      /* ����ջ��С */
                                                  NULL,                      /* ������� */
                                                  ( ( UBaseType_t ) configTIMER_TASK_PRIORITY ) | portPRIVILEGE_BIT,  /* �������ȼ�������Ȩλ�� */
                                                  pxTimerTaskStackBuffer,    /* ջ������ */
                                                  pxTimerTaskTCBBuffer );    /* TCB������ */

            /* ��������Ƿ񴴽��ɹ� */
            if( xTimerTaskHandle != NULL )
            {
                xReturn = pdPASS;  /* ���÷���ֵΪ�ɹ� */
            }
        }
        #else
        {
            /* ʹ�ö�̬���䷽ʽ������ʱ������ */
            xReturn = xTaskCreate( prvTimerTask,              /* ��ʱ�������� */
                                   "Tmr Svc",                 /* �������� */
                                   configTIMER_TASK_STACK_DEPTH,  /* ����ջ��� */
                                   NULL,                      /* ������� */
                                   ( ( UBaseType_t ) configTIMER_TASK_PRIORITY ) | portPRIVILEGE_BIT,  /* �������ȼ�������Ȩλ�� */
                                   &xTimerTaskHandle );       /* ���������� */
        }
        #endif /* configSUPPORT_STATIC_ALLOCATION */
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
    }

    /* ���Լ��ȷ������ֵ��Ч */
    configASSERT( xReturn );
    
    /* �������񴴽���� */
    return xReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xTimerCreate
 * ������������̬���������ʱ���������ڴ沢��ʼ����ʱ������
 *           �˺�����������ʼ��һ�������ʱ����֧�ֵ��κ������Զ�ʱģʽ
 * ���������
 *   - pcTimerName: ��ʱ�������ַ��������ڵ��Ժ�ʶ��ʱ��
 *   - xTimerPeriodInTicks: ��ʱ�����ڣ���ʱ�ӽ���Ϊ��λ
 *   - uxAutoReload: ��ʱ������ģʽ��pdTRUEΪ�����Զ�ʱ����pdFALSEΪ���ζ�ʱ��
 *   - pvTimerID: ��ʱ����ʶ�����������ڻص�������ʶ��ʱ��
 *   - pxCallbackFunction: ��ʱ���ص�����ָ�룬��ʱ������ʱ����
 * �����������
 * �� �� ֵ��
 *   - TimerHandle_t: �ɹ�����ʱ���ض�ʱ�������ʧ��ʱ����NULL
 * ����˵����
 *   - �˺����������ö�̬����ʱ���루configSUPPORT_DYNAMIC_ALLOCATION == 1��
 *   - ��ʱ���ڴ�ʹ��pvPortMalloc��̬����
 *   - ��ʱ��������������״̬����Ҫ����xTimerStart�Ⱥ�������
 *   - ֧�־�̬�Ͷ�̬�����ʶ�����ں���ɾ��ʱ��ȷ�����ڴ�
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

TimerHandle_t xTimerCreate( const char * const pcTimerName,
                            const TickType_t xTimerPeriodInTicks,
                            const UBaseType_t uxAutoReload,
                            void * const pvTimerID,
                            TimerCallbackFunction_t pxCallbackFunction ) /*lint !e971 ����δ�޶���char���������ַ����͵����ַ� */
{
    Timer_t *pxNewTimer;  /* ָ���¶�ʱ���ṹ��ָ�� */

    /* Ϊ��ʱ���ṹ�����ڴ� */
    pxNewTimer = ( Timer_t * ) pvPortMalloc( sizeof( Timer_t ) );

    /* ����ڴ��Ƿ����ɹ� */
    if( pxNewTimer != NULL )
    {
        /* ��ʼ���¶�ʱ�������� */
        prvInitialiseNewTimer( pcTimerName,          /* ��ʱ������ */
                               xTimerPeriodInTicks,  /* ��ʱ������ */
                               uxAutoReload,         /* ����ģʽ */
                               pvTimerID,            /* ��ʱ��ID */
                               pxCallbackFunction,   /* �ص����� */
                               pxNewTimer );         /* ��ʱ���ṹָ�� */

        /* ���ͬʱ֧�־�̬���䣬��Ƕ�ʱ���ķ��䷽ʽ */
        #if( configSUPPORT_STATIC_ALLOCATION == 1 )
        {
            /* ��ʱ�����Ծ�̬��̬��������˱�Ǵ˶�ʱ���Ƕ�̬�����ģ�
               �Ա��ں���ɾ����ʱ��ʱ��ȷ�����ڴ� */
            pxNewTimer->ucStaticallyAllocated = pdFALSE;
        }
        #endif /* configSUPPORT_STATIC_ALLOCATION */
    }

    /* ���ض�ʱ�����������ΪNULL�������ʧ�ܣ� */
    return pxNewTimer;
}

#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xTimerCreateStatic
 * ������������̬���������ʱ����ʹ��Ԥ������ڴ滺������ʼ����ʱ������
 *           �˺�����������ʼ��һ�������ʱ����ʹ�þ�̬�ڴ���䣬���⶯̬�ڴ����
 * ���������
 *   - pcTimerName: ��ʱ�������ַ��������ڵ��Ժ�ʶ��ʱ��
 *   - xTimerPeriodInTicks: ��ʱ�����ڣ���ʱ�ӽ���Ϊ��λ
 *   - uxAutoReload: ��ʱ������ģʽ��pdTRUEΪ�����Զ�ʱ����pdFALSEΪ���ζ�ʱ��
 *   - pvTimerID: ��ʱ����ʶ�����������ڻص�������ʶ��ʱ��
 *   - pxCallbackFunction: ��ʱ���ص�����ָ�룬��ʱ������ʱ����
 *   - pxTimerBuffer: ָ��̬��ʱ���ڴ滺������ָ��
 * �����������
 * �� �� ֵ��
 *   - TimerHandle_t: �ɹ�����ʱ���ض�ʱ�������ʧ��ʱ����NULL
 * ����˵����
 *   - �˺����������þ�̬����ʱ���루configSUPPORT_STATIC_ALLOCATION == 1��
 *   - ��ʱ���ڴ����û�Ԥ�ȷ��䣬���漰��̬�ڴ����
 *   - �������Լ�飬ȷ��StaticTimer_t��Timer_t�ṹ��Сһ��
 *   - ֧�־�̬�Ͷ�̬�����ʶ�����ں���ɾ��ʱ��ȷ�����ڴ�
 *   - �������ڴ����޻���Ҫȷ���Ե�Ƕ��ʽϵͳ
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
#if( configSUPPORT_STATIC_ALLOCATION == 1 )

TimerHandle_t xTimerCreateStatic( const char * const pcTimerName,
                                  const TickType_t xTimerPeriodInTicks,
                                  const UBaseType_t uxAutoReload,
                                  void * const pvTimerID,
                                  TimerCallbackFunction_t pxCallbackFunction,
                                  StaticTimer_t *pxTimerBuffer ) /*lint !e971 ����δ�޶���char���������ַ����͵����ַ� */
{
    Timer_t *pxNewTimer;  /* ָ���¶�ʱ���ṹ��ָ�� */

    /* ��������˶��Լ�飬��֤StaticTimer_t�ṹ�Ĵ�С�Ƿ���Timer_t�ṹ��ͬ */
    #if( configASSERT_DEFINED == 1 )
    {
        /* ��ȫ�Լ�飬��������StaticTimer_t���ͱ����Ľṹ���С��ʵ�ʶ�ʱ���ṹ�Ĵ�С��� */
        volatile size_t xSize = sizeof( StaticTimer_t );
        configASSERT( xSize == sizeof( Timer_t ) );  /* ���Լ��ṹ��Сһ�� */
    }
    #endif /* configASSERT_DEFINED */

    /* �����ṩָ��StaticTimer_t�ṹ��ָ�룬ʹ���� */
    configASSERT( pxTimerBuffer );  /* ���Լ�黺����ָ�벻ΪNULL */
    pxNewTimer = ( Timer_t * ) pxTimerBuffer; /*lint !e740 ��Ѱ����ת���ǿ��Եģ���Ϊ�ṹ���Ϊ������ͬ�Ķ��뷽ʽ�����Ҵ�С�ɶ��Լ�� */

    /* ���ָ���Ƿ���Ч */
    if( pxNewTimer != NULL )
    {
        /* ��ʼ���¶�ʱ�������� */
        prvInitialiseNewTimer( pcTimerName,          /* ��ʱ������ */
                               xTimerPeriodInTicks,  /* ��ʱ������ */
                               uxAutoReload,         /* ����ģʽ */
                               pvTimerID,            /* ��ʱ��ID */
                               pxCallbackFunction,   /* �ص����� */
                               pxNewTimer );         /* ��ʱ���ṹָ�� */

        /* ���ͬʱ֧�ֶ�̬���䣬��Ƕ�ʱ���ķ��䷽ʽ */
        #if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
        {
            /* ��ʱ�����Ծ�̬��̬��������˱�Ǵ˶�ʱ���Ǿ�̬�����ģ�
               �Ա��ں���ɾ����ʱ��ʱ��ȷ�����ڴ� */
            pxNewTimer->ucStaticallyAllocated = pdTRUE;
        }
        #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
    }

    /* ���ض�ʱ����� */
    return pxNewTimer;
}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvInitialiseNewTimer
 * ������������ʼ���¶�ʱ���ṹ��ĸ�����Ա�����ö�ʱ�����Ժͻص�����
 *           �˺����Ƕ�ʱ�������ĺ��ĳ�ʼ���������������ö�ʱ���Ļ������Ժ�״̬
 * ���������
 *   - pcTimerName: ��ʱ�������ַ��������ڵ��Ժ�ʶ��ʱ��
 *   - xTimerPeriodInTicks: ��ʱ�����ڣ���ʱ�ӽ���Ϊ��λ
 *   - uxAutoReload: ��ʱ������ģʽ��pdTRUEΪ�����Զ�ʱ����pdFALSEΪ���ζ�ʱ��
 *   - pvTimerID: ��ʱ����ʶ�����������ڻص�������ʶ��ʱ��
 *   - pxCallbackFunction: ��ʱ���ص�����ָ�룬��ʱ������ʱ����
 *   - pxNewTimer: ָ��Ҫ��ʼ���Ķ�ʱ���ṹ���ָ��
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺����Ǿ�̬����������FreeRTOS��ʱ��ģ���ڲ�ʹ��
 *   - ����������֤��ȷ����ʱ�����ڴ���0
 *   - ��ʼ����ʱ���б��Ϊ����ʱ�����붨ʱ���б���׼��
 *   - ��鶨ʱ��������������Ļ�����ʩ�Ƿ��Ѵ���/��ʼ��
 *   - �ṩ��ʱ���������٣����ڵ��Ժ����ܷ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
static void prvInitialiseNewTimer( const char * const pcTimerName,
                                   const TickType_t xTimerPeriodInTicks,
                                   const UBaseType_t uxAutoReload,
                                   void * const pvTimerID,
                                   TimerCallbackFunction_t pxCallbackFunction,
                                   Timer_t *pxNewTimer ) /*lint !e971 ����δ�޶���char���������ַ����͵����ַ� */
{
    /* 0����xTimerPeriodInTicks����Чֵ��ʹ�ö��Լ�������Ƿ����0 */
    configASSERT( ( xTimerPeriodInTicks > 0 ) );

    /* ��鶨ʱ���ṹָ���Ƿ���Ч */
    if( pxNewTimer != NULL )
    {
        /* ȷ����ʱ����������ʹ�õĻ�����ʩ�Ѵ���/��ʼ�� */
        prvCheckForValidListAndQueue();

        /* ʹ�ú���������ʼ����ʱ���ṹ��Ա */
        pxNewTimer->pcTimerName = pcTimerName;                /* ���ö�ʱ������ */
        pxNewTimer->xTimerPeriodInTicks = xTimerPeriodInTicks; /* ���ö�ʱ������ */
        pxNewTimer->uxAutoReload = uxAutoReload;              /* ��������ģʽ */
        pxNewTimer->pvTimerID = pvTimerID;                    /* ���ö�ʱ��ID */
        pxNewTimer->pxCallbackFunction = pxCallbackFunction;  /* ���ûص����� */
        
        /* ��ʼ����ʱ���б��Ϊ����ʱ�����붨ʱ���б���׼�� */
        vListInitialiseItem( &( pxNewTimer->xTimerListItem ) );
        
        /* ���ٶ�ʱ�������¼� */
        traceTIMER_CREATE( pxNewTimer );
    }
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xTimerGenericCommand
 * ������������ʱ������������ͨ�����ִ���ض��Ķ�ʱ������
 *           �˺����Ƕ�ʱ��������ͳһ�ӿڣ�֧�ֶ����������ͺ͵���������
 * ���������
 *   - xTimer: ��ʱ�������ָ��Ҫ�����Ķ�ʱ��
 *   - xCommandID: ����ID��ָ��Ҫִ�еĶ�ʱ����������
 *   - xOptionalValue: ��ѡֵ���������ͬ�����в�ͬ�ĺ���
 *   - pxHigherPriorityTaskWoken: ָ��������ȼ������ѱ�־��ָ�루����ISR��
 *   - xTicksToWait: ���������ʱ�����е����ȴ�ʱ�䣨��ʱ�ӽ���Ϊ��λ��
 * ���������
 *   - pxHigherPriorityTaskWoken: ��ISR����ʱ��ָʾ�Ƿ��и������ȼ����񱻻���
 * �� �� ֵ��
 *   - BaseType_t: �������ɹ����͵���ʱ�������򷵻�pdPASS�����򷵻�pdFAIL
 * ����˵����
 *   - �˺����Ƕ�ʱ��������ͳһ��ڵ㣬֧��������ж����ֵ���������
 *   - ��������ID���ֲ�ͬ�Ķ�ʱ��������������ֹͣ�����á�ɾ�����ı����ڵȣ�
 *   - ���ݵ���������ѡ���ʵ��Ķ��з��ͺ����������ISR�汾��
 *   - �ṩ����͸��٣����ڵ��Ժ����ܷ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/02     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
BaseType_t xTimerGenericCommand( TimerHandle_t xTimer, const BaseType_t xCommandID, const TickType_t xOptionalValue, BaseType_t * const pxHigherPriorityTaskWoken, const TickType_t xTicksToWait )
{
    BaseType_t xReturn = pdFAIL;            /* ����ֵ����ʼ��Ϊʧ��״̬ */
    DaemonTaskMessage_t xMessage;           /* ��ʱ������������Ϣ�ṹ */

    /* ���Լ�鶨ʱ�������Ч�� */
    configASSERT( xTimer );

    /* ��ʱ��������������Ϣ���Զ��ض���ʱ������ִ���ض����� */
    if( xTimerQueue != NULL )
    {
        /* ��ʱ�����������������Բ���xTimer��ʱ�� */
        xMessage.xMessageID = xCommandID;                                   /* ������ϢID���������ͣ� */
        xMessage.u.xTimerParameters.xMessageValue = xOptionalValue;         /* ������Ϣֵ����ѡ������ */
        xMessage.u.xTimerParameters.pxTimer = ( Timer_t * ) xTimer;         /* ���ö�ʱ��ָ�� */

        /* ��������ID�жϵ��������ģ�������жϣ� */
        if( xCommandID < tmrFIRST_FROM_ISR_COMMAND )
        {
            /* ���������ģ�ʹ����ͨ���з��ͺ��� */
            if( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING )
            {
                /* �������������У�ʹ��ָ���ĵȴ�ʱ�� */
                xReturn = xQueueSendToBack( xTimerQueue, &xMessage, xTicksToWait );
            }
            else
            {
                /* ������δ���У����ȴ��������� */
                xReturn = xQueueSendToBack( xTimerQueue, &xMessage, tmrNO_DELAY );
            }
        }
        else
        {
            /* �ж������ģ�ʹ��ISR���з��ͺ��� */
            xReturn = xQueueSendToBackFromISR( xTimerQueue, &xMessage, pxHigherPriorityTaskWoken );
        }

        /* ���ٶ�ʱ��������¼� */
        traceTIMER_COMMAND_SEND( xTimer, xCommandID, xOptionalValue, xReturn );
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
    }

    /* ��������ͽ�� */
    return xReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
�������ƣ�xTimerGetTimerDaemonTaskHandle
������������ȡFreeRTOS��ʱ���ػ���������ľ��    
���������void - ���������   
�����������    
����ֵ��TimerHandle_t - �ɹ����ض�ʱ���ػ�������������ʧ�ܴ�������    
����˵�����˺���Ӧ�ڵ�������������ã�����xTimerTaskHandleΪNULL����������
�޸�����      �汾��          �޸���            �޸�����
------------------------------------------------------------------------------
2025/09/03     V1.00          Your Name          ����
*******************************************************************************/
TaskHandle_t xTimerGetTimerDaemonTaskHandle( void )
{
	/* ���������Ƿ���������δ����ʱxTimerTaskHandleΪNULL�ᴥ������ */
	configASSERT( ( xTimerTaskHandle != NULL ) );
	/* ���ض�ʱ���ػ����������� */
	return xTimerTaskHandle;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
�������ƣ�xTimerGetPeriod
������������ȡָ�������ʱ���Ķ�ʱ����ֵ    
���������xTimer - Ҫ��ѯ�Ķ�ʱ�����   
�����������    
����ֵ��TickType_t - ���ض�ʱ��������ֵ����ϵͳ��������ʾ��    
����˵���������������Ϊ��Ч�Ķ�ʱ����������򴥷�����
�޸�����      �汾��          �޸���            �޸�����
------------------------------------------------------------------------------
2025/09/03     V1.00          Your Name          ����
*******************************************************************************/
TickType_t xTimerGetPeriod( TimerHandle_t xTimer )
{
	/* ����ʱ�����ת��Ϊ��ʱ���ṹ��ָ�� */
	Timer_t *pxTimer = ( Timer_t * ) xTimer;

	/* ���Լ��ȷ����ʱ�������Ч */
	configASSERT( xTimer );
	/* ���ض�ʱ���ṹ���д洢������ֵ */
	return pxTimer->xTimerPeriodInTicks;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
�������ƣ�xTimerGetExpiryTime
������������ȡָ����ʱ������һ�ε���ʱ���    
���������xTimer - Ҫ��ѯ�Ķ�ʱ�����   
�����������    
����ֵ��TickType_t - ���ض�ʱ����һ�ε���ʱ��ϵͳ���ļ���ֵ    
����˵���������������Ϊ��Ч�Ķ�ʱ����������򴥷�����
�޸�����      �汾��          �޸���            �޸�����
------------------------------------------------------------------------------
2025/09/03     V1.00          Your Name          ����
*******************************************************************************/
TickType_t xTimerGetExpiryTime( TimerHandle_t xTimer )
{
	/* ����ʱ�����ת��Ϊ��ʱ���ṹ��ָ�� */
	Timer_t * pxTimer = ( Timer_t * ) xTimer;
	TickType_t xReturn;

	/* ���Լ��ȷ����ʱ�������Ч */
	configASSERT( xTimer );
	/* �Ӷ�ʱ���������л�ȡ����ʱ��ֵ */
	xReturn = listGET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ) );
	/* ���ػ�ȡ���ĵ���ʱ��ֵ */
	return xReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
�������ƣ�pcTimerGetName
������������ȡָ����ʱ���������ַ���    
���������xTimer - Ҫ��ѯ�Ķ�ʱ�����   
�����������    
����ֵ��const char* - ����ָ��ʱ�������ַ�����ָ��    
����˵���������������Ϊ��Ч�Ķ�ʱ����������򴥷�����
�޸�����      �汾��          �޸���            �޸�����
------------------------------------------------------------------------------
2025/09/03     V1.00          Your Name          ����
*******************************************************************************/
const char * pcTimerGetName( TimerHandle_t xTimer )
{
	/* ����ʱ�����ת��Ϊ��ʱ���ṹ��ָ�� */
	Timer_t *pxTimer = ( Timer_t * ) xTimer;

	/* ���Լ��ȷ����ʱ�������Ч */
	configASSERT( xTimer );
	/* ���ض�ʱ���ṹ���д洢�������ַ��� */
	return pxTimer->pcTimerName;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvProcessExpiredTimer
 * ���������������ѵ��ڶ�ʱ�����ڲ����ĺ���
 *           ����ӻ��ʱ���б����Ƴ����ڶ�ʱ���������Զ����ض�ʱ�������²��룬
 *           ��ִ�ж�ʱ�������Ļص�����
 * ���������
 *   - xNextExpireTime: ��һ������ʱ��㣨ϵͳ������������ʾ��ǰ�����ʱ���׼
 *   - xTimeNow: ��ǰϵͳʱ�䣨ϵͳ�������������ڼ��㶨ʱ�����²����ʱ��
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺���Ϊ��̬���������ڶ�ʱ���ػ������ڲ�ʹ�ã�����ʱ�����ں����ز���
 *   - �����������ʱ��ǰ���ʱ���б�Ϊ�գ��ҵ�һ����ʱ���ѵ���
 *   - �����Զ����ض�ʱ�����������һ�ε���ʱ�䲢���²��뵽���ʱ���б�
 *   - ���۶�ʱ��������Σ����ն���ִ��������Ļص�����
 *   - ʹ�ô��븲�Ǳ�Ǳ�ʶ���Ը����������ߴ���ɲ�����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
static void prvProcessExpiredTimer( const TickType_t xNextExpireTime, const TickType_t xTimeNow )
{
    /* �洢�������ý�� */
    BaseType_t xResult;
    /* �ӵ�ǰ���ʱ���б��л�ȡ��һ����ʱ���������絽�ڵĶ�ʱ������ת��Ϊ��ʱ���ṹ��ָ�� */
    Timer_t * const pxTimer = ( Timer_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxCurrentTimerList );

    /* �ӻ��ʱ���б����Ƴ��ö�ʱ��������ǰ��ȷ���б�Ϊ�� */
    ( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
    /* ��¼��ʱ�����ڸ�����Ϣ����������˸��ٹ��ܣ� */
    traceTIMER_EXPIRED( pxTimer );

    /* ��鶨ʱ���Ƿ�Ϊ�Զ��������ͣ�������������һ�ε���ʱ�䲢���²�����ʱ���б� */
    if( pxTimer->uxAutoReload == ( UBaseType_t ) pdTRUE )
    {
        /* ��ʱ��ʹ������ڵ�ǰʱ��֮���ʱ������б���˻����������Ϊ�ĵ�ǰʱ����ȷ���뵽��Ӧ�б� */
        if( prvInsertTimerInActiveList( pxTimer, ( xNextExpireTime + pxTimer->xTimerPeriodInTicks ), xTimeNow, xNextExpireTime ) != pdFALSE )
        {
            /* ��ʱ������ӵ����ʱ���б�֮ǰ���ѵ��ڣ��������¼����� */
            xResult = xTimerGenericCommand( pxTimer, tmrCOMMAND_START_DONT_TRACE, xNextExpireTime, NULL, tmrNO_DELAY );
            /* ����ȷ������ִ�гɹ� */
            configASSERT( xResult );
            /* ���Է���ֵ��������������棩 */
            ( void ) xResult;
        }
        else
        {
            /* ���븲�ǲ��Ա�ǣ�����ִ��·���� */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    else
    {
        /* ���븲�ǲ��Ա�ǣ����ζ�ʱ������� */
        mtCOVERAGE_TEST_MARKER();
    }

    /* ���ö�ʱ���Ļص����������ݶ�ʱ�������Ϊ���� */
    pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvTimerTask
 * ����������FreeRTOS��ʱ���ػ������������������������������ʱ������������
 *           ������ʱ�����ڴ�����������������/���ѵȺ��Ĺ���
 * ���������
 *   - pvParameters: �������ָ�루��ǰ�汾δʹ�ã���Ϊ������������棩
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺���ΪFreeRTOS��ʱ��ϵͳ�ĺ�����������������ѭ����ʽ����
 *   - ������ʱ�������¼���ִ�ж�ʱ����صĸ�������
 *   - ֧��Ӧ�ò��������Ӻ���������Ӧ���ڶ�ʱ����������ʱִ���ض���ʼ��
 *   - ���ø�Ч�������������ƣ���û�ж�ʱ������ʱ�Զ������Խ�ʡCPU��Դ
 *   - ͨ�����������Ӻ���ʵ�������Ķ�ʱ��������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
static void prvTimerTask( void *pvParameters )
{
    /* �洢��һ����ʱ���ĵ���ʱ�� */
    TickType_t xNextExpireTime;
    /* �洢��ʱ���б��Ƿ�Ϊ�յı�־ */
    BaseType_t xListWasEmpty;

    /* ��Ϊ������������棬�����ڵ�ǰʵ����δʹ�� */
    ( void ) pvParameters;

    /* ����������ػ������������ӣ���ִ��Ӧ�ò㶨���������ʼ������ */
    #if( configUSE_DAEMON_TASK_STARTUP_HOOK == 1 )
    {
        /* �����ⲿ�������Ӻ��� */
        extern void vApplicationDaemonTaskStartupHook( void );

        /* ����Ӧ�ó����д���ڶ�ʱ������ʼִ��ʱִ��һЩ����
           �������Ҫ�ڵ�����������ִ�еĳ�ʼ������ǳ����� */
        vApplicationDaemonTaskStartupHook();
    }
    #endif /* configUSE_DAEMON_TASK_STARTUP_HOOK */

    /* ��ʱ���ػ��������ѭ��������ѭ������ʱ���¼������� */
    for( ;; )
    {
        /* ��ѯ��ʱ���б�����Ƿ�����κζ�ʱ��
           ������ڶ�ʱ�������ȡ��һ����ʱ���ĵ���ʱ�� */
        xNextExpireTime = prvGetNextExpireTime( &xListWasEmpty );

        /* ����ж�ʱ���ѵ��ڣ�������
           ��������������ֱ���ж�ʱ�����ڻ���յ����� */
        prvProcessTimerOrBlockTask( xNextExpireTime, xListWasEmpty );

        /* ������յ�������������������� */
        prvProcessReceivedCommands();
    }
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvProcessTimerOrBlockTask
 * ��������������ʱ�����ڻ�������ʱ������ĺ��ĺ���
 *           �����ж϶�ʱ���Ƿ��ڲ������������Ҫ��������ȴ���һ���¼�
 * ���������
 *   - xNextExpireTime: ��һ����ʱ���ĵ���ʱ�䣨ϵͳ��������
 *   - xListWasEmpty: ָʾ��ʱ���б��Ƿ�Ϊ�յ�״̬��־
 *                      pdFALSE��ʾ�б�ǿգ�pdTRUE��ʾ�б�Ϊ��
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺����Ƕ�ʱ���ػ�����ĺ��Ĵ����߼�����������Ǵ����ڶ�ʱ��������������
 *   - ʹ���ٽ�������ʱ��������б��л��жϣ�ȷ��������ԭ����
 *   - ����ϵͳ���ļ���������µĶ�ʱ���б��л����
 *   - ���ø�Ч�������������ƣ���ȷ�ȴ���һ����ʱ�����ڻ������
 *   - ֧������ָ�������ܵ��Ⱦ��ߣ��Ż�ϵͳ��Ӧ����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
static void prvProcessTimerOrBlockTask( const TickType_t xNextExpireTime, BaseType_t xListWasEmpty )
{
    /* �洢��ǰϵͳʱ�� */
    TickType_t xTimeNow;
    /* �洢��ʱ���б��Ƿ����л��ı�־ */
    BaseType_t xTimerListsWereSwitched;

    /* �����������񣬽����ٽ�����ȷ��ʱ�������״̬�жϵ�ԭ���� */
    vTaskSuspendAll();
    {
        /* ��ȡ��ǰʱ����������ʱ���Ƿ��ѵ���
           �����ȡʱ�䵼���б��л����򲻴���˶�ʱ������Ϊ���б��л�ʱ�������б��е�
           �κζ�ʱ��������prvSampleTimeNow()�����д������ */
        xTimeNow = prvSampleTimeNow( &xTimerListsWereSwitched );
        /* ��鶨ʱ���б��Ƿ������л���ͨ�����ڽ��ļ���������� */
        if( xTimerListsWereSwitched == pdFALSE )
        {
            /* ���ļ���δ�������鶨ʱ���Ƿ��ѵ��� */
            if( ( xListWasEmpty == pdFALSE ) && ( xNextExpireTime <= xTimeNow ) )
            {
                /* �ָ�������� */
                ( void ) xTaskResumeAll();
                /* �����ѵ��ڵĶ�ʱ�� */
                prvProcessExpiredTimer( xNextExpireTime, xTimeNow );
            }
            else
            {
                /* ���ļ���δ���������һ������ʱ����δ����
                   ������Ӧ�����Եȴ���һ������ʱ�����յ����� - ���ȵ���Ϊ׼
                   ����xNextExpireTime > xTimeNow�������������޷�������ǵ�ǰ��ʱ���б�Ϊ�� */
                if( xListWasEmpty != pdFALSE )
                {
                    /* ��ǰ��ʱ���б�Ϊ�� - �������б��Ƿ�ҲΪ�գ� */
                    xListWasEmpty = listLIST_IS_EMPTY( pxOverflowTimerList );
                }

                /* �����Եصȴ�������Ϣ�����ȴ�����һ������ʱ�� */
                vQueueWaitForMessageRestricted( xTimerQueue, ( xNextExpireTime - xTimeNow ), xListWasEmpty );

                /* �ָ�������ȣ�������Ƿ���Ҫ�����ó�CPU */
                if( xTaskResumeAll() == pdFALSE )
                {
                    /* �ó�CPU�Եȴ�����������ʱ�䵽��
                       ������ٽ����˳��ʹ��ó�֮�������������ó����ᵼ���������� */
                    portYIELD_WITHIN_API();
                }
                else
                {
                    /* ���븲�ǲ��Ա�ǣ�����ִ��·���� */
                    mtCOVERAGE_TEST_MARKER();
                }
            }
        }
        else
        {
            /* ��ʱ���б����л���ֻ��ָ�������� */
            ( void ) xTaskResumeAll();
        }
    }
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvGetNextExpireTime
 * ������������ȡ��һ����ʱ���ĵ���ʱ�䲢��鶨ʱ���б��Ƿ�Ϊ��
 *           �˺����Ƕ�ʱ���ػ�����ĺ��ĸ�������������ȷ����һ����Ҫ����Ķ�ʱ���¼�
 * ���������
 *   - pxListWasEmpty: ָ���б��״̬��־��ָ�룬���������ǰ��ʱ���б��Ƿ�Ϊ�յ�״̬
 * ���������
 *   - pxListWasEmpty: ���ص�ǰ��ʱ���б��Ƿ�Ϊ�յ�״̬
 *                     pdFALSE��ʾ�б�ǿգ�pdTRUE��ʾ�б�Ϊ��
 * ����ֵ��
 *   - TickType_t: ������һ����ʱ���ĵ���ʱ�䣨ϵͳ��������
 *                 ����б�Ϊ�գ�����0��ʾ�ڽ��ļ������ʱ�����������
 * ����˵����
 *   - �˺����Ƕ�ʱ������ĺ��Ĳ�ѯ������Ϊ��ʱ���ػ������ṩ��������
 *   - ��ʱ��������ʱ��˳�����У��б�ͷ���������ȵ��ڵĶ�ʱ��
 *   - ��û�л��ʱ��ʱ��������һ������ʱ��Ϊ0��ʹ�����ڽ��ļ������ʱ�������
 *   - ͨ��ָ����������б�״̬�������β�ѯ�б�״̬��ɵ����ܿ���
 *   - ȷ���ڽ��ļ�����תʱ�����ܹ���ȷ�������������������һ������ʱ��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
static TickType_t prvGetNextExpireTime( BaseType_t * const pxListWasEmpty )
{
    /* �洢��һ����ʱ���ĵ���ʱ�� */
    TickType_t xNextExpireTime;

    /* ��ʱ��������ʱ��˳�����У��б�ͷ���������ȵ��ڵĶ�ʱ��
       ��ȡ�����������ʱ��Ķ�ʱ���ĵ���ʱ��
       ���û�л��ʱ��������һ������ʱ������Ϊ0���⽫����
       �������ڽ��ļ������ʱ�����������ʱ��ʱ���б��л�
       ���ҿ�������������һ������ʱ�� */
    /* ��鵱ǰ��ʱ���б��Ƿ�Ϊ�գ����ͨ��ָ��������� */
    *pxListWasEmpty = listLIST_IS_EMPTY( pxCurrentTimerList );
    /* �����ʱ���б�Ϊ�� */
    if( *pxListWasEmpty == pdFALSE )
    {
        /* ��ȡ�б�ͷ����Ŀ��ֵ������һ����ʱ���ĵ���ʱ�䣩 */
        xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxCurrentTimerList );
    }
    else
    {
        /* ȷ�������ڽ��ļ�����תʱ������� */
        xNextExpireTime = ( TickType_t ) 0U;
    }

    /* ������һ����ʱ���ĵ���ʱ�� */
    return xNextExpireTime;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvSampleTimeNow
 * ����������������ǰϵͳʱ�䲢����Ƿ�����ʱ���б��л�
 *           �˺����Ƕ�ʱ��ϵͳ�ĺ���ʱ���������������ϵͳ���ļ���������������
 * ���������
 *   - pxTimerListsWereSwitched: ָ��ʱ���б��л���־��ָ�룬��������б��Ƿ����л�
 * ���������
 *   - pxTimerListsWereSwitched: ���ض�ʱ���б��Ƿ����л���״̬
 *                               pdTRUE��ʾ�б����л���pdFALSE��ʾ�б�δ�л�
 * ����ֵ��
 *   - TickType_t: ���ص�ǰϵͳʱ�䣨ϵͳ��������
 * ����˵����
 *   - �˺����Ƕ�ʱ��ʱ�����ĺ��ģ�����ϵͳ���ļ�������Ĺؼ��߼�
 *   - ʹ�þ�̬����������һ�β���ʱ�䣬���ڼ����ļ������
 *   - ����⵽���ļ������ʱ���Զ��л���ʱ���б�
 *   - ȷ����ʱ��ϵͳ�ܹ���ȷ����32λ���ļ�������������
 *   - ʹ����Ȩ�������η���ȷ����������һ��������ʣ������Ǿ�̬������
 *   - �ṩ׼ȷ��ʱ��������б��л���⣬Ϊ��ʱ�������ṩ�ɿ���ʱ���׼
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
static TickType_t prvSampleTimeNow( BaseType_t * const pxTimerListsWereSwitched )
{
    /* �洢��ǰϵͳʱ�� */
    TickType_t xTimeNow;
    /* ��̬�����洢��һ�β���ʱ�䣬��ʼ��Ϊ0������һ��������� */
    PRIVILEGED_DATA static TickType_t xLastTime = ( TickType_t ) 0U; /*lint !e956 Variable is only accessible to one task. */

    /* ��ȡ��ǰϵͳ���ļ��� */
    xTimeNow = xTaskGetTickCount();

    /* ��鵱ǰʱ���Ƿ�С����һ��ʱ�䣨�����ļ�������� */
    if( xTimeNow < xLastTime )
    {
        /* ���ļ�����������л���ʱ���б� */
        prvSwitchTimerLists();
        /* �����б��л���־Ϊ�� */
        *pxTimerListsWereSwitched = pdTRUE;
    }
    else
    {
        /* ���ļ���δ����������б��л���־Ϊ�� */
        *pxTimerListsWereSwitched = pdFALSE;
    }

    /* ������һ�β���ʱ��Ϊ��ǰʱ�� */
    xLastTime = xTimeNow;

    /* ���ص�ǰϵͳʱ�� */
    return xTimeNow;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvInsertTimerInActiveList
 * ��������������ʱ�������б�ĺ��ĺ�����������ݶ�ʱ���ĵ���ʱ�佫����뵽���ʵ��б���
 *           ���ж϶�ʱ���Ƿ���Ҫ��������������ļ���������������
 * ���������
 *   - pxTimer: ָ��Ҫ����Ķ�ʱ���ṹ��ָ��
 *   - xNextExpiryTime: ��ʱ������һ�ε���ʱ�䣨ϵͳ��������
 *   - xTimeNow: ��ǰϵͳʱ�䣨ϵͳ��������
 *   - xCommandTime: �����ʱ��ʱ�䣨ϵͳ��������
 * �����������
 * ����ֵ��
 *   - BaseType_t: �����Ƿ���Ҫ��������ʱ���ı�־
 *                 pdTRUE��ʾ��Ҫ��������ʱ����pdFALSE��ʾ��ʱ���Ѳ����б�
 * ����˵����
 *   - �˺����Ƕ�ʱ���б����ĺ��ģ��������ʱ����������ʱ��������ȷ���б�
 *   - ���������������֮����ܷ�����ʱ�����źͽ��ļ���������
 *   - ���ݶ�ʱ������ʱ���뵱ǰʱ��ıȽϣ��������뵱ǰ�б�������б�
 *   - ���ض������£���ʹ��ʱ����δ����Ҳ������Ҫ��������
 *   - ʹ���������ά����ʱ���������ԣ�ȷ�����絽�ڵĶ�ʱ�����б�ͷ��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
static BaseType_t prvInsertTimerInActiveList( Timer_t * const pxTimer, const TickType_t xNextExpiryTime, const TickType_t xTimeNow, const TickType_t xCommandTime )
{
    /* ��ʼ���Ƿ���Ҫ��������ʱ���ı�־��Ĭ��Ϊ����Ҫ */
    BaseType_t xProcessTimerNow = pdFALSE;

    /* ���ö�ʱ���������ֵΪ��һ�ε���ʱ�� */
    listSET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ), xNextExpiryTime );
    /* ���ö�ʱ���������������Ϊ��ǰ��ʱ�� */
    listSET_LIST_ITEM_OWNER( &( pxTimer->xTimerListItem ), pxTimer );

    /* ��鶨ʱ������һ�ε���ʱ���Ƿ��Ѿ�С�ڻ���ڵ�ǰʱ�� */
    if( xNextExpiryTime <= xTimeNow )
    {
        /* ����������������֮���ʱ���Ƿ��Ѿ�������ʱ��������
           ����������ܷ�����������ӳٽϴ������� */
        if( ( ( TickType_t ) ( xTimeNow - xCommandTime ) ) >= pxTimer->xTimerPeriodInTicks ) /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
        {
            /* ������������֮���ʱ��ʵ�����Ѿ������˶�ʱ��������
               ��Ҫ��������ʱ�� */
            xProcessTimerNow = pdTRUE;
        }
        else
        {
            /* ����ʱ�����������ʱ���б� */
            vListInsert( pxOverflowTimerList, &( pxTimer->xTimerListItem ) );
        }
    }
    else
    {
        /* ����Ƿ����˽��ļ���������Ҷ�ʱ���ĵ���ʱ����δ���� */
        if( ( xTimeNow < xCommandTime ) && ( xNextExpiryTime >= xCommandTime ) )
        {
            /* �������������������ļ����Ѿ����������ʱ����δ����
               ��ô��ʱ��ʵ�����Ѿ����������ĵ���ʱ�䣬Ӧ���������� */
            xProcessTimerNow = pdTRUE;
        }
        else
        {
            /* ����ʱ�����뵱ǰ��ʱ���б� */
            vListInsert( pxCurrentTimerList, &( pxTimer->xTimerListItem ) );
        }
    }

    /* �����Ƿ���Ҫ��������ʱ���ı�־ */
    return xProcessTimerNow;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvProcessReceivedCommands
 * ��������������Ӷ�ʱ�����н��յ�����������ĺ��ĺ���
 *           ���������ִ�ж�ʱ���������������ֹͣ�����á�ɾ����ʱ���Լ��ı����ڵȲ���
 *           ͬʱ֧�� pend �������ù��ܣ��ṩ������������
 * ���������void - ��ֱ�����������ͨ��ȫ�ֶ��� xTimerQueue ����������Ϣ
 * �����������
 * ����ֵ����
 * ����˵����
 *   - �˺����Ƕ�ʱ���ػ�����ĺ��������ѭ���������������첽��ʱ������
 *   - ֧�ֶ��ֶ�ʱ���������������������ж������ķ���������
 *   - ���� pend �������ù��ܣ������ڶ�ʱ��������������ִ���û��Զ��庯��
 *   - ʹ�ö��л���ȷ���������̰߳�ȫ�ԣ����⾺̬����
 *   - ������ϸ�Ĵ�����Ͷ��ԣ�ȷ�������Ŀɿ���
 *   - ����ʱ���б����ȷ����ʱ������ȷ��ʱ�䱻������Ƴ�
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
static void prvProcessReceivedCommands( void )
{
    /* ������Ϣ�ṹ���ڽ��ն����е����� */
    DaemonTaskMessage_t xMessage;
    /* ָ��ʱ���ṹ��ָ�� */
    Timer_t *pxTimer;
    /* �洢��ʱ���б��Ƿ��л��ı�־ */
    BaseType_t xTimerListsWereSwitched, xResult;
    /* �洢��ǰʱ�� */
    TickType_t xTimeNow;

    /* ѭ���Ӷ�ʱ�����н�����Ϣ���������������� */
    while( xQueueReceive( xTimerQueue, &xMessage, tmrNO_DELAY ) != pdFAIL ) /*lint !e603 xMessage does not have to be initialised as it is passed out, not in, and it is not used unless xQueueReceive() returns pdTRUE. */
    {
        /* ����Ƿ������� pend �������ù��� */
        #if ( INCLUDE_xTimerPendFunctionCall == 1 )
        {
            /* ����������ID��ʾ�� pend �������ö����Ƕ�ʱ������ */
            if( xMessage.xMessageID < ( BaseType_t ) 0 )
            {
                /* ��ȡ�ص������ṹָ�� */
                const CallbackParameters_t * const pxCallback = &( xMessage.u.xCallbackParameters );

                /* ���Լ��ص�������Ч�� */
                configASSERT( pxCallback );

                /* ���� pend ���� */
                pxCallback->pxCallbackFunction( pxCallback->pvParameter1, pxCallback->ulParameter2 );
            }
            else
            {
                /* ���븲�ǲ��Ա�ǣ��� pend ������������� */
                mtCOVERAGE_TEST_MARKER();
            }
        }
        #endif /* INCLUDE_xTimerPendFunctionCall */

        /* ����������ID��ʾ�Ƕ�ʱ����������� pend �������� */
        if( xMessage.xMessageID >= ( BaseType_t ) 0 )
        {
            /* ����Ϣ�л�ȡ��ʱ��ָ�� */
            pxTimer = xMessage.u.xTimerParameters.pxTimer;

            /* ��鶨ʱ���Ƿ���ĳ���б��� */
            if( listIS_CONTAINED_WITHIN( NULL, &( pxTimer->xTimerListItem ) ) == pdFALSE )
            {
                /* ��ʱ�����б��У��Ƚ����Ƴ� */
                ( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
            }
            else
            {
                /* ���븲�ǲ��Ա�ǣ���ʱ�������б��е������ */
                mtCOVERAGE_TEST_MARKER();
            }

            /* ��¼��ʱ��������ո�����Ϣ */
            traceTIMER_COMMAND_RECEIVED( pxTimer, xMessage.xMessageID, xMessage.u.xTimerParameters.xMessageValue );

            /* ��ȡ��ǰʱ�䣬�����ڴӶ��н�����Ϣ����ã���ȷ��ʱ��׼ȷ��
               ��������ȼ����������� xTimeNow ֵ����ռ��ʱ���ػ�����
               ������Ϣ������Ӿ��и���ʱ�����Ϣ */
            xTimeNow = prvSampleTimeNow( &xTimerListsWereSwitched );

            /* ��������IDִ����Ӧ�Ĳ��� */
            switch( xMessage.xMessageID )
            {
                case tmrCOMMAND_START :
                case tmrCOMMAND_START_FROM_ISR :
                case tmrCOMMAND_RESET :
                case tmrCOMMAND_RESET_FROM_ISR :
                case tmrCOMMAND_START_DONT_TRACE :
                    /* ������������ʱ�� */
                    if( prvInsertTimerInActiveList( pxTimer,  xMessage.u.xTimerParameters.xMessageValue + pxTimer->xTimerPeriodInTicks, xTimeNow, xMessage.u.xTimerParameters.xMessageValue ) != pdFALSE )
                    {
                        /* ��ʱ������ӵ����ʱ���б�֮ǰ���ѵ��ڣ����������� */
                        pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );
                        /* ��¼��ʱ�����ڸ�����Ϣ */
                        traceTIMER_EXPIRED( pxTimer );

                        /* ������Զ����ض�ʱ�������������� */
                        if( pxTimer->uxAutoReload == ( UBaseType_t ) pdTRUE )
                        {
                            xResult = xTimerGenericCommand( pxTimer, tmrCOMMAND_START_DONT_TRACE, xMessage.u.xTimerParameters.xMessageValue + pxTimer->xTimerPeriodInTicks, NULL, tmrNO_DELAY );
                            /* ����ȷ������ִ�гɹ� */
                            configASSERT( xResult );
                            ( void ) xResult;
                        }
                        else
                        {
                            /* ���븲�ǲ��Ա�ǣ����ζ�ʱ������� */
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    else
                    {
                        /* ���븲�ǲ��Ա�ǣ���ʱ���ɹ������б������� */
                        mtCOVERAGE_TEST_MARKER();
                    }
                    break;

                case tmrCOMMAND_STOP :
                case tmrCOMMAND_STOP_FROM_ISR :
                    /* ��ʱ���Ѿ��ӻ�б����Ƴ������ﲻ��Ҫ���κβ��� */
                    break;

                case tmrCOMMAND_CHANGE_PERIOD :
                case tmrCOMMAND_CHANGE_PERIOD_FROM_ISR :
                    /* �ı䶨ʱ�������� */
                    pxTimer->xTimerPeriodInTicks = xMessage.u.xTimerParameters.xMessageValue;
                    /* ����ȷ���µ�����ֵ����0 */
                    configASSERT( ( pxTimer->xTimerPeriodInTicks > 0 ) );

                    /* ������û�������Ĳο��㣬���ԱȾ����ڳ����
                       ��˽�����ʱ������Ϊ��ǰʱ�䣬�����������ڲ���Ϊ��
                       ��һ�ε���ʱ��ֻ����δ��������ζ�ţ�������� xTimerStart() �����ͬ��
                       ���ﲻ��Ҫ����ʧ����� */
                    ( void ) prvInsertTimerInActiveList( pxTimer, ( xTimeNow + pxTimer->xTimerPeriodInTicks ), xTimeNow, xTimeNow );
                    break;

                case tmrCOMMAND_DELETE :
                    /* ��ʱ���Ѿ��ӻ�б����Ƴ���ֻ���ͷ��ڴ棨����Ƕ�̬����ģ� */
                    #if( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 0 ) )
                    {
                        /* ��ʱ��ֻ���Ƕ�̬����� - �ͷ��� */
                        vPortFree( pxTimer );
                    }
                    #elif( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
                    {
                        /* ��ʱ�������Ǿ�̬��̬����ģ�����ڳ����ͷ��ڴ�ǰ��� */
                        if( pxTimer->ucStaticallyAllocated == ( uint8_t ) pdFALSE )
                        {
                            vPortFree( pxTimer );
                        }
                        else
                        {
                            /* ���븲�ǲ��Ա�ǣ���̬���䶨ʱ������� */
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
                    break;

                default :
                    /* ��Ӧ�õ����������δ֪���� */
                    break;
            }
        }
    }
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvSwitchTimerLists
 * �����������л���ʱ���б���ڲ�����������ϵͳ���ļ������ʱ�Ķ�ʱ������
 *           ��ϵͳ���ļ������ʱ�������л���ʱ���б������������ѵ��ڵĶ�ʱ��
 * ���������void - ���������
 * �����������
 * ����ֵ����
 * ����˵����
 *   - �˺����Ǵ���ϵͳ���ļ�������ĺ��ĺ�����ȷ����ʱ��ϵͳ�ڽ��������������ȷ����
 *   - �ڽ��ļ������ʱ�����е�ǰ��ʱ���б��еĶ�ʱ�������뱻������Ϊ���Ƕ��ѵ���
 *   - �����Զ����ض�ʱ���������߼���ȷ�������ڽ������������ȷ��������
 *   - ��󽻻���ǰ��ʱ���б�������ʱ���б��ָ�룬����б��л�
 *   - ʹ�ø��ٹ��ܼ�¼��ʱ�������¼������ڵ��Ժ����ܷ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
static void prvSwitchTimerLists( void )
{
    /* �洢��һ������ʱ�������ʱ�� */
    TickType_t xNextExpireTime, xReloadTime;
    /* ��ʱָ�������б��� */
    List_t *pxTemp;
    /* ָ��ʱ����ָ�� */
    Timer_t *pxTimer;
    /* �洢�������ý�� */
    BaseType_t xResult;

    /* ���ļ���������������л���ʱ���б�
       �����ǰ��ʱ���б��������κζ�ʱ�������ã������Ǳ����ѵ���
       Ӧ�����б��л�֮ǰ�������� */
    /* ѭ������ǰ��ʱ���б��е����ж�ʱ����ֱ���б�Ϊ�� */
    while( listLIST_IS_EMPTY( pxCurrentTimerList ) == pdFALSE )
    {
        /* ��ȡ�б�ͷ����Ŀ��ֵ������һ����ʱ���ĵ���ʱ�䣩 */
        xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxCurrentTimerList );

        /* ���б����Ƴ���ʱ�� */
        pxTimer = ( Timer_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxCurrentTimerList );
        ( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
        /* ��¼��ʱ�����ڸ�����Ϣ */
        traceTIMER_EXPIRED( pxTimer );

        /* ִ�����Ļص�������Ȼ��������Զ����ض�ʱ��������������������
           ����������������������Ϊ�б���δ�л� */
        pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );

        /* ��鶨ʱ���Ƿ�Ϊ�Զ��������� */
        if( pxTimer->uxAutoReload == ( UBaseType_t ) pdTRUE )
        {
            /* ��������ֵ���������ֵ���¶�ʱ��������ͬ�Ķ�ʱ���б�
               ��ô���Ѿ����ڣ���ʱ��Ӧ�����²��뵱ǰ�б��Ա��ڴ�ѭ�����ٴδ���
               ����Ӧ�÷�����������������ʱ����ȷ����ֻ���б�������뵽�б��� */
            xReloadTime = ( xNextExpireTime + pxTimer->xTimerPeriodInTicks );
            /* �������ʱ���Ƿ������һ������ʱ�䣨������������ */
            if( xReloadTime > xNextExpireTime )
            {
                /* ���ö�ʱ���������ֵ�������� */
                listSET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ), xReloadTime );
                listSET_LIST_ITEM_OWNER( &( pxTimer->xTimerListItem ), pxTimer );
                /* ����ʱ�����²��뵱ǰ�б� */
                vListInsert( pxCurrentTimerList, &( pxTimer->xTimerListItem ) );
            }
            else
            {
                /* ������������������ʱ�� */
                xResult = xTimerGenericCommand( pxTimer, tmrCOMMAND_START_DONT_TRACE, xNextExpireTime, NULL, tmrNO_DELAY );
                /* ����ȷ������ִ�гɹ� */
                configASSERT( xResult );
                ( void ) xResult;
            }
        }
        else
        {
            /* ���븲�ǲ��Ա�ǣ����ζ�ʱ������� */
            mtCOVERAGE_TEST_MARKER();
        }
    }

    /* ������ǰ��ʱ���б�������ʱ���б��ָ�� */
    pxTemp = pxCurrentTimerList;
    pxCurrentTimerList = pxOverflowTimerList;
    pxOverflowTimerList = pxTemp;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvCheckForValidListAndQueue
 * ������������鲢ȷ����ʱ���б�Ͷ�������ȷ��ʼ�����ڲ�����
 *           �����ʱ������δ��ʼ�������ʼ����ʱ���б�Ͷ��У�ȷ����ʱ��ϵͳ����
 * ���������void - ���������
 * �����������
 * ����ֵ����
 * ����˵����
 *   - �˺����Ƕ�ʱ��ϵͳ�ĳ�ʼ�����Ϻ�����ȷ����ʱ���б�Ͷ�����ʹ��ǰ����ȷ��ʼ��
 *   - ʹ���ٽ���������ʼ�����̣�ȷ�������񻷾��µ��̰߳�ȫ
 *   - ֧�־�̬�Ͷ�̬�����ڴ���䷽ʽ����������ѡ���ʵ��ĳ�ʼ����ʽ
 *   - ��ʼ��������ʱ���б���ǰ�б������б������ó�ʼָ��
 *   - ������ʱ��������У������붨ʱ����������ͨ��
 *   - ��ѡ�ؽ�������ӵ�ע������ڵ��Ժͼ��
 *   - ʹ�ô��븲�Ǳ�Ǳ�ʶ���Ը����������ߴ���ɲ�����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
static void prvCheckForValidListAndQueue( void )
{
    /* �����ʱ�����õ��б�������붨ʱ������ͨ�ŵĶ����Ƿ��ѳ�ʼ�� */
    /* �����ٽ�����ȷ����ʼ�����̵�ԭ���� */
    taskENTER_CRITICAL();
    {
        /* ��鶨ʱ�������Ƿ�Ϊ�գ�δ��ʼ���� */
        if( xTimerQueue == NULL )
        {
            /* ��ʼ��������ʱ���б� */
            vListInitialise( &xActiveTimerList1 );
            vListInitialise( &xActiveTimerList2 );
            /* ���õ�ǰ��ʱ���б�������ʱ���б�ĳ�ʼָ�� */
            pxCurrentTimerList = &xActiveTimerList1;
            pxOverflowTimerList = &xActiveTimerList2;

            /* ����Ƿ�֧�־�̬�ڴ���� */
            #if( configSUPPORT_STATIC_ALLOCATION == 1 )
            {
                /* ���configSUPPORT_DYNAMIC_ALLOCATIONΪ0����̬���䶨ʱ������ */
                /* ���徲̬���нṹ */
                static StaticQueue_t xStaticTimerQueue;
                /* ���徲̬���д洢�ռ� */
                static uint8_t ucStaticTimerQueueStorage[ configTIMER_QUEUE_LENGTH * sizeof( DaemonTaskMessage_t ) ];

                /* ������̬���� */
                xTimerQueue = xQueueCreateStatic( ( UBaseType_t ) configTIMER_QUEUE_LENGTH, sizeof( DaemonTaskMessage_t ), &( ucStaticTimerQueueStorage[ 0 ] ), &xStaticTimerQueue );
            }
            #else
            {
                /* ��̬�������� */
                xTimerQueue = xQueueCreate( ( UBaseType_t ) configTIMER_QUEUE_LENGTH, sizeof( DaemonTaskMessage_t ) );
            }
            #endif

            /* ����Ƿ������˶���ע����� */
            #if ( configQUEUE_REGISTRY_SIZE > 0 )
            {
                /* ������д����ɹ���������ӵ�����ע��� */
                if( xTimerQueue != NULL )
                {
                    vQueueAddToRegistry( xTimerQueue, "TmrQ" );
                }
                else
                {
                    /* ���븲�ǲ��Ա�ǣ����д���ʧ������� */
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            #endif /* configQUEUE_REGISTRY_SIZE */
        }
        else
        {
            /* ���븲�ǲ��Ա�ǣ������ѳ�ʼ������� */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    /* �˳��ٽ��� */
    taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xTimerIsTimerActive
 * �������������ָ����ʱ���Ƿ��ڻ״̬���Ƿ��ڻ��ʱ���б��У�
 *           �˺������ڲ�ѯ��ʱ���ļ���״̬���ж϶�ʱ���Ƿ��������л�ȴ�����
 * ���������
 *   - xTimer: Ҫ���Ķ�ʱ�������ָ����Ҫ��ѯ״̬�Ķ�ʱ������
 * �����������
 * ����ֵ��
 *   - BaseType_t: ���ض�ʱ���Ļ״̬
 *                 pdTRUE��ʾ��ʱ�����ڻ״̬���ڻ�б��У�
 *                 pdFALSE��ʾ��ʱ�����ڷǻ״̬�����ڻ�б��У�
 * ����˵����
 *   - �˺����Ƕ�ʱ��״̬��ѯ�ӿڣ��ṩ��һ�ְ�ȫ�ķ�ʽ��鶨ʱ���Ƿ��ڻ״̬
 *   - ʹ���ٽ�������״̬�����̣�ȷ���ڶ����񻷾��µ��̰߳�ȫ��
 *   - ͨ����鶨ʱ�����������Ƿ������ĳ���б������ж϶�ʱ��״̬
 *   - �����ڲ�����������Ч�Զ��ԣ�ȷ������Ķ�ʱ�������Ч
 *   - ����ֵ���߼���Ҫ��ת����ΪlistIS_CONTAINED_WITHIN�ڲ����б���ʱ����pdTRUE
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
BaseType_t xTimerIsTimerActive( TimerHandle_t xTimer )
{
    /* �洢��ʱ���Ƿ��ڻ�б��е�״̬ */
    BaseType_t xTimerIsInActiveList;
    /* ����ʱ�����ת��Ϊ��ʱ���ṹ��ָ�� */
    Timer_t *pxTimer = ( Timer_t * ) xTimer;

    /* ���Լ��ȷ����ʱ�������Ч */
    configASSERT( xTimer );

    /* ��鶨ʱ���Ƿ��ڻ��ʱ���б��У� */
    /* �����ٽ�����ȷ��״̬����ԭ���� */
    taskENTER_CRITICAL();
    {
        /* ��鶨ʱ���������Ƿ���NULL�б��У���ʵ������һ���Լ�����Ƿ�
           ����ǰ�������ʱ���б����ã����߼����뷴ת�������'!'���� */
        xTimerIsInActiveList = ( BaseType_t ) !( listIS_CONTAINED_WITHIN( NULL, &( pxTimer->xTimerListItem ) ) );
    }
    /* �˳��ٽ��� */
    taskEXIT_CRITICAL();

    /* ���ض�ʱ���Ƿ��ڻ�б��е�״̬ */
    return xTimerIsInActiveList;
} /*lint !e818 Can't be pointer to const due to the typedef. */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�pvTimerGetTimerID
 * ������������ȡ��ʱ����IDֵ����ID���û��붨ʱ���������Զ����ʶ��
 *           �˺������ڼ����붨ʱ���������û�����ID�������ڴ洢��������Ϣ
 * ���������
 *   - xTimer: ��ʱ�������ָ����Ҫ��ȡID�Ķ�ʱ������
 * �����������
 * ����ֵ��
 *   - void*: �����붨ʱ���������û�����IDֵ
 *            ���δ����ID���򷵻�NULL���û����õ��ض�ֵ
 * ����˵����
 *   - �˺����ṩ�˻�ȡ��ʱ���û�ID�İ�ȫ��ʽ��ʹ���ٽ�������ȷ���̰߳�ȫ
 *   - IDֵ���û��Զ��������ָ���������ݣ��������ڴ洢��������Ϣ���ʶ��ʱ��
 *   - �����ڲ�����������Ч�Զ��ԣ�ȷ������Ķ�ʱ�������Ч
 *   - ʹ���ٽ�������ID��ȡ��������ֹ�ڶ����񻷾��³������ݾ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
void *pvTimerGetTimerID( const TimerHandle_t xTimer )
{
    /* ����ʱ�����ת��Ϊ��ʱ���ṹ��ָ�� */
    Timer_t * const pxTimer = ( Timer_t * ) xTimer;
    /* �洢���ص�IDֵ */
    void *pvReturn;

    /* ���Լ��ȷ����ʱ�������Ч */
    configASSERT( xTimer );

    /* �����ٽ���������ID��ȡ������ԭ���� */
    taskENTER_CRITICAL();
    {
        /* �Ӷ�ʱ���ṹ�л�ȡ�û������IDֵ */
        pvReturn = pxTimer->pvTimerID;
    }
    /* �˳��ٽ��� */
    taskEXIT_CRITICAL();

    /* ���ػ�ȡ����IDֵ */
    return pvReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�vTimerSetTimerID
 * �������������ö�ʱ����IDֵ����ID���û��붨ʱ���������Զ����ʶ��
 *           �˺����������û�����붨ʱ���������û�����ID�������ڴ洢��������Ϣ
 * ���������
 *   - xTimer: ��ʱ�������ָ����Ҫ����ID�Ķ�ʱ������
 *   - pvNewID: Ҫ���õ���IDֵ������������ָ�����͵�����
 * �����������
 * ����ֵ����
 * ����˵����
 *   - �˺����ṩ�����ö�ʱ���û�ID�İ�ȫ��ʽ��ʹ���ٽ�������ȷ���̰߳�ȫ
 *   - IDֵ���û��Զ��������ָ���������ݣ��������ڴ洢��������Ϣ���ʶ��ʱ��
 *   - �����ڲ�����������Ч�Զ��ԣ�ȷ������Ķ�ʱ�������Ч
 *   - ʹ���ٽ�������ID���ò�������ֹ�ڶ����񻷾��³������ݾ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
void vTimerSetTimerID( TimerHandle_t xTimer, void *pvNewID )
{
    /* ����ʱ�����ת��Ϊ��ʱ���ṹ��ָ�� */
    Timer_t * const pxTimer = ( Timer_t * ) xTimer;

    /* ���Լ��ȷ����ʱ�������Ч */
    configASSERT( xTimer );

    /* �����ٽ���������ID���ò�����ԭ���� */
    taskENTER_CRITICAL();
    {
        /* ����IDֵ���õ���ʱ���ṹ�� */
        pxTimer->pvTimerID = pvNewID;
    }
    /* �˳��ٽ��� */
    taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

#if( INCLUDE_xTimerPendFunctionCall == 1 )

/*******************************************************************************
 * �������ƣ�xTimerPendFunctionCallFromISR
 * �������������жϷ������(ISR)�й���һ�������������󵽶�ʱ���ػ�����
 *           �˺���������ISR����������һ���������ã��ú������ڶ�ʱ��������������ִ��
 * ���������
 *   - xFunctionToPend: Ҫ����ĺ���ָ�룬ָ����Ҫ�ڶ�ʱ��������ִ�еĺ���
 *   - pvParameter1: ���ݸ��������ĵ�һ���������������������͵�ָ��
 *   - ulParameter2: ���ݸ��������ĵڶ���������32λ�޷�������ֵ
 *   - pxHigherPriorityTaskWoken: ָ��������ȼ������ѱ�־��ָ��
 * ���������
 *   - pxHigherPriorityTaskWoken: �������������¸������ȼ����������������ΪpdTRUE
 * ����ֵ��
 *   - BaseType_t: ���غ�����������Ľ��
 *                 pdPASS��ʾ�ɹ����������������͵���ʱ������
 *                 pdFAIL��ʾ����ʧ�ܣ�ͨ����Ϊ����������
 * ����˵����
 *   - �˺���רΪ�ж���������ƣ�ʹ��FromISR�汾����Ϣ���ͺ���
 *   - ����ĺ������ڶ�ʱ���ػ�������������ִ�У���������ISR��������
 *   - �ṩ���ٹ��ܣ����ڵ��Ժ����ܷ���
 *   - ����ִ�����첽�ģ������ͺ��������أ�ʵ�ʺ���ִ��ʱ��ȡ���ڶ�ʱ���������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
BaseType_t xTimerPendFunctionCallFromISR( PendedFunction_t xFunctionToPend, void *pvParameter1, uint32_t ulParameter2, BaseType_t *pxHigherPriorityTaskWoken )
{
    /* ������Ϣ�ṹ���ڴ洢�������ò��� */
    DaemonTaskMessage_t xMessage;
    /* �洢�������ؽ�� */
    BaseType_t xReturn;

    /* ʹ�ú������������Ϣ�ṹ�����䷢�͵��ػ����� */
    xMessage.xMessageID = tmrCOMMAND_EXECUTE_CALLBACK_FROM_ISR;
    xMessage.u.xCallbackParameters.pxCallbackFunction = xFunctionToPend;
    xMessage.u.xCallbackParameters.pvParameter1 = pvParameter1;
    xMessage.u.xCallbackParameters.ulParameter2 = ulParameter2;

    /* ���жϷ����������Ϣ����ʱ������ */
    xReturn = xQueueSendFromISR( xTimerQueue, &xMessage, pxHigherPriorityTaskWoken );

    /* ��¼���������õĸ�����Ϣ */
    tracePEND_FUNC_CALL_FROM_ISR( xFunctionToPend, pvParameter1, ulParameter2, xReturn );

    /* ���ط��Ͳ����Ľ�� */
    return xReturn;
}

#endif /* INCLUDE_xTimerPendFunctionCall */
/*-----------------------------------------------------------*/

#if( INCLUDE_xTimerPendFunctionCall == 1 )

/*******************************************************************************
 * �������ƣ�xTimerPendFunctionCall
 * �����������������������й���һ�������������󵽶�ʱ���ػ�����
 *           �˺�����������������������һ���������ã��ú������ڶ�ʱ��������������ִ��
 * ���������
 *   - xFunctionToPend: Ҫ����ĺ���ָ�룬ָ����Ҫ�ڶ�ʱ��������ִ�еĺ���
 *   - pvParameter1: ���ݸ��������ĵ�һ���������������������͵�ָ��
 *   - ulParameter2: ���ݸ��������ĵڶ���������32λ�޷�������ֵ
 *   - xTicksToWait: �������󵽶�ʱ�����е����ȴ�ʱ�䣨��ϵͳ����Ϊ��λ��
 * �����������
 * ����ֵ��
 *   - BaseType_t: ���غ�����������Ľ��
 *                 pdPASS��ʾ�ɹ����������������͵���ʱ������
 *                 pdFAIL��ʾ����ʧ�ܣ������������ڵȴ�ʱ����û�пռ���ã�
 * ����˵����
 *   - �˺���ֻ���ڶ�ʱ����������������������ã���Ϊ�ڴ�֮ǰ��ʱ�����в�����
 *   - ����ĺ������ڶ�ʱ���ػ�������������ִ�У��������ڵ���������������
 *   - �ṩ���ٹ��ܣ����ڵ��Ժ����ܷ���
 *   - ����ִ�����첽�ģ������ͺ��������أ�ʵ�ʺ���ִ��ʱ��ȡ���ڶ�ʱ���������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
BaseType_t xTimerPendFunctionCall( PendedFunction_t xFunctionToPend, void *pvParameter1, uint32_t ulParameter2, TickType_t xTicksToWait )
{
    /* ������Ϣ�ṹ���ڴ洢�������ò��� */
    DaemonTaskMessage_t xMessage;
    /* �洢�������ؽ�� */
    BaseType_t xReturn;

    /* �˺���ֻ���ڶ�ʱ����������������������ã���Ϊ�ڴ�֮ǰ��ʱ�����в����� */
    configASSERT( xTimerQueue );

    /* ʹ�ú������������Ϣ�ṹ�����䷢�͵��ػ����� */
    xMessage.xMessageID = tmrCOMMAND_EXECUTE_CALLBACK;
    xMessage.u.xCallbackParameters.pxCallbackFunction = xFunctionToPend;
    xMessage.u.xCallbackParameters.pvParameter1 = pvParameter1;
    xMessage.u.xCallbackParameters.ulParameter2 = ulParameter2;

    /* ������Ϣ����ʱ������β�� */
    xReturn = xQueueSendToBack( xTimerQueue, &xMessage, xTicksToWait );

    /* ��¼���������õĸ�����Ϣ */
    tracePEND_FUNC_CALL( xFunctionToPend, pvParameter1, ulParameter2, xReturn );

    /* ���ط��Ͳ����Ľ�� */
    return xReturn;
}

#endif /* INCLUDE_xTimerPendFunctionCall */
/*-----------------------------------------------------------*/

/* This entire source file will be skipped if the application is not configured
to include software timer functionality.  If you want to include software timer
functionality then ensure configUSE_TIMERS is set to 1 in FreeRTOSConfig.h. */
#endif /* configUSE_TIMERS == 1 */




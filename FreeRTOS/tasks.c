/*******************************************************************************
 * ��Ȩ���� (C)2025, CQG
 *
 * �ļ����ƣ� LM_task.c
 * �ļ���ʶ�� 
 * ����ժҪ�� ����ģ�鶨��
 * ����˵���� ��
 * ��ǰ�汾�� FreeRTOS V9.0.0
 * ��    �ߣ� Qiguo_Cui                   
 * ������ڣ� 2025��09��13��
 *
 *******************************************************************************/


/* Define to prevent recursive inclusion -------------------------------------*/

/* Includes ------------------------------------------------------------------*/
/* Standard includes. */
#include <stdlib.h>
#include <string.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "StackMacros.h"

/* Exported types ------------------------------------------------------------*/
/*
 * Task control block.  A task control block (TCB) is allocated for each task,
 * and stores task state information, including a pointer to the task's context
 * (the task's run time environment, including register values)
 */
typedef struct tskTaskControlBlock
{
	volatile StackType_t	*pxTopOfStack;	/*< ָ������ջ����λ�ã�������TCB�ṹ�ĵ�һ����Ա */

	#if ( portUSING_MPU_WRAPPERS == 1 )
		xMPU_SETTINGS	xMPUSettings;		/*< MPU���ã�������TCB�ṹ�ĵڶ�����Ա */
	#endif

	ListItem_t			xStateListItem;	/*< ״̬�б�����ڱ�ʾ����״̬������������������ */
	ListItem_t			xEventListItem;		/*< �¼��б�����ڴ��¼��б����������� */
	UBaseType_t			uxPriority;			/*< �������ȼ���0Ϊ������ȼ� */
	StackType_t			*pxStack;			/*< ָ���ջ��ʼλ�� */
	char				pcTaskName[ configMAX_TASK_NAME_LEN ];/*< ���񴴽�ʱ���������������ƣ����ڵ��� */

	#if ( portSTACK_GROWTH > 0 )
		StackType_t		*pxEndOfStack;		/*< �ڶ�ջ�ӵ��ڴ����������ļܹ���ָ���ջĩ�� */
	#endif

	#if ( portCRITICAL_NESTING_IN_TCB == 1 )
		UBaseType_t		uxCriticalNesting;	/*< ����ؼ���Ƕ����� */
	#endif

	#if ( configUSE_TRACE_FACILITY == 1 )
		UBaseType_t		uxTCBNumber;		/*< �洢TCB����ʱ���������֣����ڵ������������ɾ�����ؽ� */
		UBaseType_t		uxTaskNumber;		/*< ר�Ź����������ٴ���ʹ�õ����� */
	#endif

	#if ( configUSE_MUTEXES == 1 )
		UBaseType_t		uxBasePriority;		/*< ���������������ȼ� - �������ȼ��̳л��� */
		UBaseType_t		uxMutexesHeld;      /*< ���еĻ��������� */
	#endif

	#if ( configUSE_APPLICATION_TASK_TAG == 1 )
		TaskHookFunction_t pxTaskTag;       /*< �����ǩ���� */
	#endif

	#if( configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0 )
		void *pvThreadLocalStoragePointers[ configNUM_THREAD_LOCAL_STORAGE_POINTERS ]; /*< �̱߳��ش洢ָ�� */
	#endif

	#if( configGENERATE_RUN_TIME_STATS == 1 )
		uint32_t		ulRunTimeCounter;	/*< �洢����������״̬�»��ѵ�ʱ���� */
	#endif

	#if ( configUSE_NEWLIB_REENTRANT == 1 )
		struct	_reent xNewLib_reent;       /*< Newlib����ṹ */
	#endif

	#if( configUSE_TASK_NOTIFICATIONS == 1 )
		volatile uint32_t ulNotifiedValue;  /*< ֵ֪ͨ */
		volatile uint8_t ucNotifyState;     /*< ֪ͨ״̬ */
	#endif

	#if( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 )
		uint8_t	ucStaticallyAllocated; 		/*< ��������Ǿ�̬����ģ�������ΪpdTRUE��ȷ�����᳢���ͷ��ڴ� */
	#endif

	#if( INCLUDE_xTaskAbortDelay == 1 )
		uint8_t ucDelayAborted;             /*< �ӳ���ֹ��־ */
	#endif

} tskTCB;

/* The old tskTCB name is maintained above then typedefed to the new TCB_t name
below to enable the use of older kernel aware debuggers. */
typedef tskTCB TCB_t;  /*< TCB���Ͷ��壬���ݾɰ��ں˸�֪������ */

/* Exported constants --------------------------------------------------------*/
/* Values that can be assigned to the ucNotifyState member of the TCB. */
#define taskNOT_WAITING_NOTIFICATION	( ( uint8_t ) 0 )  /*< ����δ�ȴ�֪ͨ */
#define taskWAITING_NOTIFICATION		( ( uint8_t ) 1 )  /*< �������ڵȴ�֪ͨ */
#define taskNOTIFICATION_RECEIVED		( ( uint8_t ) 2 )  /*< �������յ�֪ͨ */

/*
 * The value used to fill the stack of a task when the task is created.  This
 * is used purely for checking the high water mark for tasks.
 */
#define tskSTACK_FILL_BYTE	( 0xa5U )  /*< ����ջ����ֽڣ����ڼ��ջʹ�ø�ˮλ�� */

/*
 * Macros used by vListTask to indicate which state a task is in.
 */
#define tskBLOCKED_CHAR		( 'B' )  /*< ����״̬�ַ���ʾ */
#define tskREADY_CHAR		( 'R' )  /*< ����״̬�ַ���ʾ */
#define tskDELETED_CHAR		( 'D' )  /*< ��ɾ��״̬�ַ���ʾ */
#define tskSUSPENDED_CHAR	( 'S' )  /*< ����״̬�ַ���ʾ */

/* Exported macro ------------------------------------------------------------*/
#if( configUSE_PREEMPTION == 0 )
	/* If the cooperative scheduler is being used then a yield should not be
	performed just because a higher priority task has been woken. */
	#define taskYIELD_IF_USING_PREEMPTION()  /*< Э���������²����������л� */
#else
	#define taskYIELD_IF_USING_PREEMPTION() portYIELD_WITHIN_API()  /*< ��ռʽ�������½��������л� */
#endif

/* Exported functions --------------------------------------------------------*/
/* ע���˴��г�������ͷ�ļ����������ⲿ�ɼ�������ʵ�ʶ������ļ����� */

/* Private types -------------------------------------------------------------*/
/* ע���˴�û�ж����˽�����Ͷ��壬�����������ڵ��������ж��� */

/* Private variables ---------------------------------------------------------*/

PRIVILEGED_DATA TCB_t * volatile pxCurrentTCB = NULL;  /*< ָ��ǰ���������TCB */

/* Lists for ready and blocked tasks. --------------------*/
PRIVILEGED_DATA static List_t pxReadyTasksLists[ configMAX_PRIORITIES ];  /*< ���ȼ����������б� */
PRIVILEGED_DATA static List_t xDelayedTaskList1;                         /*< �ӳ������б�1 */
PRIVILEGED_DATA static List_t xDelayedTaskList2;                         /*< �ӳ������б�2�����ڴ��������������ӳ٣� */
PRIVILEGED_DATA static List_t * volatile pxDelayedTaskList;              /*< ָ��ǰʹ�õ��ӳ������б� */
PRIVILEGED_DATA static List_t * volatile pxOverflowDelayedTaskList;      /*< ָ���������������ӳ������б� */
PRIVILEGED_DATA static List_t xPendingReadyList;                         /*< ����������ʱ�Ѿ����������б� */

#if( INCLUDE_vTaskDelete == 1 )
	PRIVILEGED_DATA static List_t xTasksWaitingTermination;              /*< �ȴ���ֹ�������б� */
	PRIVILEGED_DATA static volatile UBaseType_t uxDeletedTasksWaitingCleanUp = ( UBaseType_t ) 0U;  /*< �ȴ��������ɾ��������� */
#endif

#if ( INCLUDE_vTaskSuspend == 1 )
	PRIVILEGED_DATA static List_t xSuspendedTaskList;                    /*< ����������б� */
#endif

/* Other file private variables. --------------------------------*/
PRIVILEGED_DATA static volatile UBaseType_t uxCurrentNumberOfTasks = ( UBaseType_t ) 0U;  /*< ��ǰ�������� */
PRIVILEGED_DATA static volatile TickType_t xTickCount = ( TickType_t ) 0U;                /*< ϵͳ�δ������ */
PRIVILEGED_DATA static volatile UBaseType_t uxTopReadyPriority = tskIDLE_PRIORITY;       /*< ��߾������ȼ� */
PRIVILEGED_DATA static volatile BaseType_t xSchedulerRunning = pdFALSE;                   /*< ���������б�־ */
PRIVILEGED_DATA static volatile UBaseType_t uxPendedTicks = ( UBaseType_t ) 0U;          /*< ����ĵδ���� */
PRIVILEGED_DATA static volatile BaseType_t xYieldPending = pdFALSE;                      /*< ������������л���־ */
PRIVILEGED_DATA static volatile BaseType_t xNumOfOverflows = ( BaseType_t ) 0;           /*< ������������� */
PRIVILEGED_DATA static UBaseType_t uxTaskNumber = ( UBaseType_t ) 0U;                    /*< �����ż����� */
PRIVILEGED_DATA static volatile TickType_t xNextTaskUnblockTime = ( TickType_t ) 0U;     /*< ��һ��������������ʱ�� */
PRIVILEGED_DATA static TaskHandle_t xIdleTaskHandle = NULL;                              /*< ���������� */

PRIVILEGED_DATA static volatile UBaseType_t uxSchedulerSuspended = ( UBaseType_t ) pdFALSE;  /*< �����������־ */

#if ( configGENERATE_RUN_TIME_STATS == 1 )
	PRIVILEGED_DATA static uint32_t ulTaskSwitchedInTime = 0UL;  /*< �ϴ������л�ʱ��ʱ��/������ֵ */
	PRIVILEGED_DATA static uint32_t ulTotalRunTime = 0UL;        /*< ����ʱ�������ʱ�Ӷ������ִ��ʱ�� */
#endif

/* Private constants ---------------------------------------------------------*/
/* ע���˴�û�ж����˽�г������壬���г������ڵ��������ж��� */

/* Private macros ------------------------------------------------------------*/

#ifdef portREMOVE_STATIC_QUALIFIER
	#define static  /*< �Ƴ���̬�޶���������ĳЩ������������ */
#endif

#if ( configUSE_PORT_OPTIMISED_TASK_SELECTION == 0 )
	/* �Ƕ˿��Ż�������ѡ��ʵ�� */
	#define taskRECORD_READY_PRIORITY( uxPriority )                      \
	{                                                                   \
		if( ( uxPriority ) > uxTopReadyPriority )                       \
		{                                                               \
			uxTopReadyPriority = ( uxPriority );                        \
		}                                                               \
	} /* taskRECORD_READY_PRIORITY */

	#define taskSELECT_HIGHEST_PRIORITY_TASK()                          \
	{                                                                   \
	UBaseType_t uxTopPriority = uxTopReadyPriority;                     \
                                                                        \
		while( listLIST_IS_EMPTY( &( pxReadyTasksLists[ uxTopPriority ] ) ) ) \
		{                                                               \
			configASSERT( uxTopPriority );                              \
			--uxTopPriority;                                            \
		}                                                               \
                                                                        \
		listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &( pxReadyTasksLists[ uxTopPriority ] ) ); \
		uxTopReadyPriority = uxTopPriority;                             \
	} /* taskSELECT_HIGHEST_PRIORITY_TASK */

	#define taskRESET_READY_PRIORITY( uxPriority )
	#define portRESET_READY_PRIORITY( uxPriority, uxTopReadyPriority )

#else /* configUSE_PORT_OPTIMISED_TASK_SELECTION */
	/* �˿��Ż�������ѡ��ʵ�� */
	#define taskRECORD_READY_PRIORITY( uxPriority )	portRECORD_READY_PRIORITY( uxPriority, uxTopReadyPriority )

	#define taskSELECT_HIGHEST_PRIORITY_TASK()                          \
	{                                                                   \
	UBaseType_t uxTopPriority;                                          \
                                                                        \
		portGET_HIGHEST_PRIORITY( uxTopPriority, uxTopReadyPriority );  \
		configASSERT( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ uxTopPriority ] ) ) > 0 ); \
		listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &( pxReadyTasksLists[ uxTopPriority ] ) ); \
	} /* taskSELECT_HIGHEST_PRIORITY_TASK() */

	#define taskRESET_READY_PRIORITY( uxPriority )                      \
	{                                                                   \
		if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ ( uxPriority ) ] ) ) == ( UBaseType_t ) 0 ) \
		{                                                               \
			portRESET_READY_PRIORITY( ( uxPriority ), ( uxTopReadyPriority ) ); \
		}                                                               \
	}

#endif /* configUSE_PORT_OPTIMISED_TASK_SELECTION */

/* pxDelayedTaskList and pxOverflowDelayedTaskList are switched when the tick
count overflows. */
#define taskSWITCH_DELAYED_LISTS()                                      \
{                                                                       \
	List_t *pxTemp;                                                     \
                                                                        \
	configASSERT( ( listLIST_IS_EMPTY( pxDelayedTaskList ) ) );         \
                                                                        \
	pxTemp = pxDelayedTaskList;                                         \
	pxDelayedTaskList = pxOverflowDelayedTaskList;                      \
	pxOverflowDelayedTaskList = pxTemp;                                 \
	xNumOfOverflows++;                                                  \
	prvResetNextTaskUnblockTime();                                      \
}


#define prvAddTaskToReadyList( pxTCB )                                  \
	traceMOVED_TASK_TO_READY_STATE( pxTCB );                            \
	taskRECORD_READY_PRIORITY( ( pxTCB )->uxPriority );                 \
	vListInsertEnd( &( pxReadyTasksLists[ ( pxTCB )->uxPriority ] ), &( ( pxTCB )->xStateListItem ) ); \
	tracePOST_MOVED_TASK_TO_READY_STATE( pxTCB )


#define prvGetTCBFromHandle( pxHandle ) ( ( ( pxHandle ) == NULL ) ? ( TCB_t * ) pxCurrentTCB : ( TCB_t * ) ( pxHandle ) )


#if( configUSE_16_BIT_TICKS == 1 )
	#define taskEVENT_LIST_ITEM_VALUE_IN_USE	0x8000U      /*< �¼��б���ֵʹ���б�־(16λ) */
#else
	#define taskEVENT_LIST_ITEM_VALUE_IN_USE	0x80000000UL /*< �¼��б���ֵʹ���б�־(32λ) */
#endif

/* Private functions ---------------------------------------------------------*/
/* Callback function prototypes. --------------------------*/
#if( configCHECK_FOR_STACK_OVERFLOW > 0 )
	extern void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName );  /*< ջ������Ӻ��� */
#endif

#if( configUSE_TICK_HOOK > 0 )
	extern void vApplicationTickHook( void );  /*< �δ��Ӻ��� */
#endif

#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	extern void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );  /*< ��ȡ���������ڴ� */
#endif


#if ( INCLUDE_vTaskSuspend == 1 )
	static BaseType_t prvTaskIsTaskSuspended( const TaskHandle_t xTask ) PRIVILEGED_FUNCTION;  /*< ��������Ƿ���� */
#endif /* INCLUDE_vTaskSuspend */


static void prvInitialiseTaskLists( void ) PRIVILEGED_FUNCTION;  /*< ��ʼ�������б� */


static portTASK_FUNCTION_PROTO( prvIdleTask, pvParameters );  /*< ���������� */


#if ( INCLUDE_vTaskDelete == 1 )
	static void prvDeleteTCB( TCB_t *pxTCB ) PRIVILEGED_FUNCTION;  /*< ɾ��TCB */
#endif


static void prvCheckTasksWaitingTermination( void ) PRIVILEGED_FUNCTION;  /*< ���ȴ���ֹ������ */


static void prvAddCurrentTaskToDelayedList( TickType_t xTicksToWait, const BaseType_t xCanBlockIndefinitely ) PRIVILEGED_FUNCTION;  /*< ����ǰ������ӵ��ӳ��б� */


#if ( configUSE_TRACE_FACILITY == 1 )
	static UBaseType_t prvListTasksWithinSingleList( TaskStatus_t *pxTaskStatusArray, List_t *pxList, eTaskState eState ) PRIVILEGED_FUNCTION;  /*< �г������б��е����� */
#endif


#if ( INCLUDE_xTaskGetHandle == 1 )
	static TCB_t *prvSearchForNameWithinSingleList( List_t *pxList, const char pcNameToQuery[] ) PRIVILEGED_FUNCTION;  /*< ���б��а������������� */
#endif


#if ( ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) )
	static uint16_t prvTaskCheckFreeStackSpace( const uint8_t * pucStackByte ) PRIVILEGED_FUNCTION;  /*< �������ջ���пռ� */
#endif


#if ( configUSE_TICKLESS_IDLE != 0 )
	static TickType_t prvGetExpectedIdleTime( void ) PRIVILEGED_FUNCTION;  /*< ��ȡԤ�ڿ���ʱ�� */
#endif


static void prvResetNextTaskUnblockTime( void );  /*< ������һ������������ʱ�� */

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )
	static char *prvWriteNameToBuffer( char *pcBuffer, const char *pcTaskName ) PRIVILEGED_FUNCTION;  /*< ����������д�뻺���� */
#endif

static void prvInitialiseNewTask( TaskFunction_t pxTaskCode,
									const char * const pcName,
									const uint32_t ulStackDepth,
									void * const pvParameters,
									UBaseType_t uxPriority,
									TaskHandle_t * const pxCreatedTask,
									TCB_t *pxNewTCB,
									const MemoryRegion_t * const xRegions ) PRIVILEGED_FUNCTION;  /*< ��ʼ�������� */

static void prvAddNewTaskToReadyList( TCB_t *pxNewTCB ) PRIVILEGED_FUNCTION;  /*< ����������ӵ������б� */

/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xTaskCreateStatic
 * ������������̬������������ӵ������б�ʹ��Ԥ�����TCB��ջ�ڴ�
 *           �����ڲ�֧�ֶ�̬�ڴ�������Ҫ��ȷ�����ڴ沼�ֵ�Ƕ��ʽϵͳ
 * ���������
 *   - pxTaskCode:     ������ָ�룬ָ������ľ���ʵ�ֺ���
 *   - pcName:         ���������ַ��������ڵ��Ժ�ʶ������
 *   - ulStackDepth:   ����ջ��ȣ�����Ϊ��λ�������ֽ�����
 *   - pvParameters:   ���ݸ��������Ĳ���ָ��
 *   - uxPriority:     �������ȼ���0Ϊ������ȼ���configMAX_PRIORITIES-1Ϊ��ߣ�
 *   - puxStackBuffer: ָ��Ԥ�����ջ�ڴ滺������ָ��
 *   - pxTaskBuffer:   ָ��Ԥ����ľ�̬������ƿ�(TCB)�ڴ��ָ��
 * �����������
 * �� �� ֵ��
 *   - TaskHandle_t:   �ɹ�����ʱ������������ʧ��ʱ����NULL
 * ����˵����
 *   - �˺����������þ�̬����ʱ���루configSUPPORT_STATIC_ALLOCATION == 1��
 *   - �����TCB��ջ�ռ䶼���û�Ԥ�ȷ��䣬���漰��̬�ڴ����
 *   - �������ڴ����޻���Ҫ��ȷ�ڴ����ĳ���
 *   - ������������Զ���ӵ������б��ȴ�����������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
#if( configSUPPORT_STATIC_ALLOCATION == 1 )

TaskHandle_t xTaskCreateStatic( TaskFunction_t pxTaskCode,
                                const char * const pcName,
                                const uint32_t ulStackDepth,
                                void * const pvParameters,
                                UBaseType_t uxPriority,
                                StackType_t * const puxStackBuffer,
                                StaticTask_t * const pxTaskBuffer ) /*lint !e971 ����δ�޶���char���������ַ����͵����ַ� */
{
    TCB_t *pxNewTCB;          /* ָ����������ƿ��ָ�� */
    TaskHandle_t xReturn;     /* ��������ֵ���������� */

    /* ���Լ�飺ȷ��ջ�����������񻺳���ָ�벻ΪNULL */
    configASSERT( puxStackBuffer != NULL );
    configASSERT( pxTaskBuffer != NULL );

    /* �ٴμ��ջ�����������񻺳���ָ���Ƿ���Ч */
    if( ( pxTaskBuffer != NULL ) && ( puxStackBuffer != NULL ) )
    {
        /* �����TCB��ջ�ڴ��ɴ˺������� - ֱ��ʹ������ */
        pxNewTCB = ( TCB_t * ) pxTaskBuffer; /*lint !e740 ��Ѱ����ת���ǿ��Եģ���Ϊ�ṹ���Ϊ������ͬ�Ķ��뷽ʽ�����Ҵ�С�ɶ��Լ�� */
        pxNewTCB->pxStack = ( StackType_t * ) puxStackBuffer;

        /* ���ͬʱ֧�־�̬�Ͷ�̬���䣬�������ķ��䷽ʽ */
        #if( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 )
        {
            /* ������Ծ�̬��̬��������˱�Ǵ������Ǿ�̬�����ģ�
               �Ա����ɾ��ʱ��ȷ���� */
            pxNewTCB->ucStaticallyAllocated = tskSTATICALLY_ALLOCATED_STACK_AND_TCB;
        }
        #endif /* configSUPPORT_DYNAMIC_ALLOCATION */

        /* ��ʼ�����������������������ơ�ջ��ȡ����������ȼ��� */
        prvInitialiseNewTask( pxTaskCode,      /* ������ָ�� */
                              pcName,          /* �������� */
                              ulStackDepth,    /* ջ��� */
                              pvParameters,    /* ������� */
                              uxPriority,      /* �������ȼ� */
                              &xReturn,        /* ���������� */
                              pxNewTCB,        /* �������TCB */
                              NULL );          /* ��MPU�������� */

        /* ����������ӵ������б�ʹ��ɱ����������� */
        prvAddNewTaskToReadyList( pxNewTCB );
    }
    else
    {
        /* ���÷���ֵΪNULL��ʾ����ʧ�� */
        xReturn = NULL;
    }

    /* ������������NULL */
    return xReturn;
}

#endif /* SUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xTaskCreateRestricted
 * ����������������MPU���ڴ汣����Ԫ������������ʹ�þ�̬�����ջ������
 *           �ú���������MPU��װ��ʱ���ã����ڴ��������ڴ汣�����Ե�����
 * ���������
 *   - pxTaskDefinition: ָ����������ṹ���ָ�룬�������������������Ϣ
 *   - pxCreatedTask:    ���ڷ�����������ָ�������ַ
 * ���������
 *   - pxCreatedTask:    �ɹ���������󣬽�������д��˵�ַ
 * �� �� ֵ��
 *   - pdPASS:           ���񴴽��ɹ�
 *   - errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY: �ڴ����ʧ�ܣ����񴴽�ʧ��
 * ����˵����
 *   - �˺�����������MPU��װ��ʱ���루portUSING_MPU_WRAPPERS == 1��
 *   - ����ʹ�þ�̬�����ջ��������TCB��������ƿ飩��̬����
 *   - ������������Զ���ӵ������б��ȴ�����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
#if( portUSING_MPU_WRAPPERS == 1 )

BaseType_t xTaskCreateRestricted( const TaskParameters_t * const pxTaskDefinition, TaskHandle_t *pxCreatedTask )
{
    TCB_t *pxNewTCB;                                                /* ָ����������ƿ��ָ�� */
    BaseType_t xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;     /* ����ֵ����ʼ��Ϊ�ڴ������� */

    /* ���Լ�飺ȷ���������е�ջ������ָ�벻ΪNULL */
    configASSERT( pxTaskDefinition->puxStackBuffer );

    /* �ٴμ��ջ������ָ���Ƿ���Ч */
    if( pxTaskDefinition->puxStackBuffer != NULL )
    {
        /* ΪTCB�����ڴ档�ڴ���Դȡ���ڶ˿�malloc������ʵ���Լ��Ƿ�ʹ�þ�̬���� */
        pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) );

        /* ���TCB�ڴ��Ƿ����ɹ� */
        if( pxNewTCB != NULL )
        {
            /* ��TCB�д洢ջλ�ã�ʹ�þ�̬�����ջ�������� */
            pxNewTCB->pxStack = pxTaskDefinition->puxStackBuffer;

            /* ������Ծ�̬��̬������������ʹ�þ�̬�����ջ����TCB�Ƕ�̬����ģ�
               ��Ǵ�����ķ��䷽ʽ�Ա����ɾ��ʱ��ȷ���� */
            pxNewTCB->ucStaticallyAllocated = tskSTATICALLY_ALLOCATED_STACK_ONLY;

            /* ��ʼ�����������������������ơ�ջ��ȡ����������ȼ��� */
            prvInitialiseNewTask( pxTaskDefinition->pvTaskCode,          /* ������ָ�� */
                                  pxTaskDefinition->pcName,              /* �������� */
                                  ( uint32_t ) pxTaskDefinition->usStackDepth, /* ջ��� */
                                  pxTaskDefinition->pvParameters,        /* ������� */
                                  pxTaskDefinition->uxPriority,          /* �������ȼ� */
                                  pxCreatedTask,                         /* ���������� */
                                  pxNewTCB,                              /* �������TCB */
                                  pxTaskDefinition->xRegions );          /* MPU�ڴ��������� */

            /* ����������ӵ������б�ʹ��ɱ����������� */
            prvAddNewTaskToReadyList( pxNewTCB );

            /* ���÷���ֵΪ�ɹ� */
            xReturn = pdPASS;
        }
    }

    /* �������񴴽���� */
    return xReturn;
}

#endif /* portUSING_MPU_WRAPPERS */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xTaskCreate
 * ������������̬������������ӵ������б���FreeRTOS����õ����񴴽�����
 *           ����ջ���������Զ������ڴ����˳��֧�ֶ�̬�ڴ���䷽ʽ
 * ���������
 *   - pxTaskCode:   ������ָ�룬ָ������ľ���ʵ�ֺ���
 *   - pcName:       ���������ַ��������ڵ��Ժ�ʶ������
 *   - usStackDepth: ����ջ��ȣ�����Ϊ��λ�������ֽ�����
 *   - pvParameters: ���ݸ��������Ĳ���ָ��
 *   - uxPriority:   �������ȼ���0Ϊ������ȼ���configMAX_PRIORITIES-1Ϊ��ߣ�
 *   - pxCreatedTask: ���ڷ�����������ָ�������ַ
 * ���������
 *   - pxCreatedTask: �ɹ���������󣬽�������д��˵�ַ
 * �� �� ֵ��
 *   - pdPASS:       ���񴴽��ɹ�
 *   - errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY: �ڴ����ʧ�ܣ����񴴽�ʧ��
 * ����˵����
 *   - �˺����������ö�̬����ʱ���루configSUPPORT_DYNAMIC_ALLOCATION == 1��
 *   - �����TCB��ջ�ռ䶼�Ƕ�̬�����
 *   - ���ݶ˿ڵĲ�ͬջ�����������ϻ����£�������˳��������ͬ
 *   - ������������Զ���ӵ������б��ȴ�����������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

BaseType_t xTaskCreate( TaskFunction_t pxTaskCode,
                        const char * const pcName,
                        const uint16_t usStackDepth,
                        void * const pvParameters,
                        UBaseType_t uxPriority,
                        TaskHandle_t * const pxCreatedTask ) /*lint !e971 ����δ�޶���char���������ַ����͵����ַ� */
{
    TCB_t *pxNewTCB;          /* ָ����������ƿ��ָ�� */
    BaseType_t xReturn;       /* ��������ֵ */

    /* ���ջ�������������ȷ���ջ�ռ��ٷ���TCB������ջ����������TCB�С�
       ͬ�������ջ�������������ȷ���TCB�ٷ���ջ�ռ䡣 */
    #if( portSTACK_GROWTH > 0 )
    {
        /* ΪTCB����ռ䡣�ڴ���Դȡ���ڶ˿�malloc������ʵ���Լ��Ƿ�ʹ�þ�̬���� */
        pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) );

        /* ���TCB�Ƿ����ɹ� */
        if( pxNewTCB != NULL )
        {
            /* Ϊ���ڴ������������ջ�ռ䡣
               ջ�ڴ�Ļ���ַ�洢��TCB�У��Ա������Ҫʱ����ɾ������ */
            pxNewTCB->pxStack = ( StackType_t * ) pvPortMalloc( ( ( ( size_t ) usStackDepth ) * sizeof( StackType_t ) ) ); /*lint !e961 MISRA�쳣����Ϊת������ĳЩ�˿�������� */

            /* ���ջ�ռ��Ƿ����ɹ� */
            if( pxNewTCB->pxStack == NULL )
            {
                /* �޷�����ջ�ռ䣬ɾ���ѷ����TCB */
                vPortFree( pxNewTCB );
                pxNewTCB = NULL;
            }
        }
    }
    #else /* portSTACK_GROWTH */
    {
        StackType_t *pxStack;  /* ջָ�� */

        /* Ϊ���ڴ������������ջ�ռ� */
        pxStack = ( StackType_t * ) pvPortMalloc( ( ( ( size_t ) usStackDepth ) * sizeof( StackType_t ) ) ); /*lint !e961 MISRA�쳣����Ϊת������ĳЩ�˿�������� */

        /* ���ջ�ռ��Ƿ����ɹ� */
        if( pxStack != NULL )
        {
            /* ΪTCB����ռ� */
            pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) ); /*lint !e961 MISRA�쳣����Ϊת������ĳЩ·��������� */

            /* ���TCB�Ƿ����ɹ� */
            if( pxNewTCB != NULL )
            {
                /* ��TCB�д洢ջλ�� */
                pxNewTCB->pxStack = pxStack;
            }
            else
            {
                /* ����TCBδ������ջ�ռ��޷�ʹ�ã���Ҫ�ٴ��ͷ� */
                vPortFree( pxStack );
            }
        }
        else
        {
            pxNewTCB = NULL;
        }
    }
    #endif /* portSTACK_GROWTH */

    /* ���TCB��ջ�ռ��Ƿ񶼷���ɹ� */
    if( pxNewTCB != NULL )
    {
        #if( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 )
        {
            /* ������Ծ�̬��̬��������˱�Ǵ������Ƕ�̬�����ģ�
               �Ա����ɾ��ʱ��ȷ���� */
            pxNewTCB->ucStaticallyAllocated = tskDYNAMICALLY_ALLOCATED_STACK_AND_TCB;
        }
        #endif /* configSUPPORT_STATIC_ALLOCATION */

        /* ��ʼ�����������������������ơ�ջ��ȡ����������ȼ��� */
        prvInitialiseNewTask( pxTaskCode,      /* ������ָ�� */
                              pcName,          /* �������� */
                              ( uint32_t ) usStackDepth, /* ջ��� */
                              pvParameters,    /* ������� */
                              uxPriority,      /* �������ȼ� */
                              pxCreatedTask,   /* ���������� */
                              pxNewTCB,        /* �������TCB */
                              NULL );          /* ��MPU�������� */

        /* ����������ӵ������б�ʹ��ɱ����������� */
        prvAddNewTaskToReadyList( pxNewTCB );

        /* ���÷���ֵΪ�ɹ� */
        xReturn = pdPASS;
    }
    else
    {
        /* ���÷���ֵΪ�ڴ������� */
        xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    /* �������񴴽���� */
    return xReturn;
}

#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvInitialiseNewTask
 * ������������ʼ���������TCB��������ƿ飩��ջ�ռ䣬��������ĸ������Ժ�״̬
 *           ����FreeRTOS���񴴽��ĺ����ڲ�����������������ƿ��ȫ���ʼ��
 * ���������
 *   - pxTaskCode:     ������ָ�룬ָ������ľ���ʵ�ֺ���
 *   - pcName:         ���������ַ��������ڵ��Ժ�ʶ������
 *   - ulStackDepth:   ����ջ��ȣ�����Ϊ��λ�������ֽ�����
 *   - pvParameters:   ���ݸ��������Ĳ���ָ��
 *   - uxPriority:     �������ȼ���0Ϊ������ȼ���configMAX_PRIORITIES-1Ϊ��ߣ�
 *   - pxCreatedTask:  ���ڷ�����������ָ�������ַ
 *   - pxNewTCB:       ָ����������ƿ�(TCB)��ָ��
 *   - xRegions:       MPU�ڴ���������ָ�루���ʹ��MPU��
 * ���������
 *   - pxCreatedTask:  �ɹ���ʼ������󣬽�������д��˵�ַ
 * �� �� ֵ���ޣ���̬�����������ⲿ�ɼ���
 * ����˵����
 *   - �˺����Ǿ�̬����������FreeRTOS�ں��ڲ�ʹ��
 *   - ���������ȫ���ʼ��������ջ���á����ȼ����á��б����ʼ����
 *   - ���ݲ�ͬ������ѡ�MPU��ջ��顢������ʩ�ȣ�ִ�в�ͬ�ĳ�ʼ������
 *   - ����ͨ��pxPortInitialiseStack������ʼ������ջ��ʹ�俴�������Ѿ����жϹ�
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
static void prvInitialiseNewTask( TaskFunction_t pxTaskCode,
                                  const char * const pcName,
                                  const uint32_t ulStackDepth,
                                  void * const pvParameters,
                                  UBaseType_t uxPriority,
                                  TaskHandle_t * const pxCreatedTask,
                                  TCB_t *pxNewTCB,
                                  const MemoryRegion_t * const xRegions ) /*lint !e971 ����δ�޶���char���������ַ����͵����ַ� */
{
    StackType_t *pxTopOfStack;  /* ջ��ָ�� */
    UBaseType_t x;              /* ѭ�������� */

    /* ���ʹ��MPU��װ������������Ƿ�Ӧ������Ȩģʽ�´��� */
    #if( portUSING_MPU_WRAPPERS == 1 )
        BaseType_t xRunPrivileged;  /* �Ƿ�����Ȩģʽ���еı�־ */
        
        /* ������ȼ����Ƿ���������Ȩλ */
        if( ( uxPriority & portPRIVILEGE_BIT ) != 0U )
        {
            xRunPrivileged = pdTRUE;  /* ����Ϊ��Ȩģʽ */
        }
        else
        {
            xRunPrivileged = pdFALSE; /* ����Ϊ����Ȩģʽ */
        }
        
        /* ������ȼ��е���Ȩλ��ֻ���������ȼ�ֵ */
        uxPriority &= ~portPRIVILEGE_BIT;
    #endif /* portUSING_MPU_WRAPPERS == 1 */

    /* �������Ҫmemset()������������� */
    #if( ( configCHECK_FOR_STACK_OVERFLOW > 1 ) || ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) )
    {
        /* ����ֵ֪���ջ���Ը������Ժ�ջ������ */
        ( void ) memset( pxNewTCB->pxStack, ( int ) tskSTACK_FILL_BYTE, ( size_t ) ulStackDepth * sizeof( StackType_t ) );
    }
    #endif /* ջ���������ʩ������� */

    /* ����ջ����ַ����ȡ����ջ�ǴӸ��ڴ�����ڴ���������80x86�������෴��
       portSTACK_GROWTH���ڸ�����Ҫʹ���Ϊ���� */
    #if( portSTACK_GROWTH < 0 )  /* ջ�������� */
    {
        /* ����ջ����ַ��ջ��������ʱ��ջ����ջ��������ĩβ�� */
        pxTopOfStack = pxNewTCB->pxStack + ( ulStackDepth - ( uint32_t ) 1 );
        
        /* ����ջ����ַ��Ҫ����ֽڶ��� */
        pxTopOfStack = ( StackType_t * ) ( ( ( portPOINTER_SIZE_TYPE ) pxTopOfStack ) & ( ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) ) ); /*lint !e923 MISRA�쳣��������ָ�������֮�����ת����ʵ�ʡ�ʹ��portPOINTER_SIZE_TYPE���ʹ����С���� */
        
        /* ���������ջ����ַ�Ķ����Ƿ���ȷ */
        configASSERT( ( ( ( portPOINTER_SIZE_TYPE ) pxTopOfStack & ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) == 0UL ) );
    }
    #else /* portSTACK_GROWTH */  /* ջ�������� */
    {
        /* ջ��������ʱ��ջ������ջ����������ʼ��ַ */
        pxTopOfStack = pxNewTCB->pxStack;

        /* ���ջ�������Ķ����Ƿ���ȷ */
        configASSERT( ( ( ( portPOINTER_SIZE_TYPE ) pxNewTCB->pxStack & ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) == 0UL ) );

        /* ���ִ��ջ��飬��Ҫ֪��ջ�ռ����һ�� */
        pxNewTCB->pxEndOfStack = pxNewTCB->pxStack + ( ulStackDepth - ( uint32_t ) 1 );
    }
    #endif /* portSTACK_GROWTH */

    /* ���������ƴ洢��TCB�� */
    for( x = ( UBaseType_t ) 0; x < ( UBaseType_t ) configMAX_TASK_NAME_LEN; x++ )
    {
        /* �������������ַ� */
        pxNewTCB->pcTaskName[ x ] = pcName[ x ];

        /* ����ַ�����configMAX_TASK_NAME_LEN�ַ��̣���Ҫ��������configMAX_TASK_NAME_LEN���ַ���
           �Է��ַ���֮����ڴ治�ɷ��ʣ��������ܣ� */
        if( pcName[ x ] == 0x00 )
        {
            break;  /* �����ַ������������˳�ѭ�� */
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
        }
    }

    /* ȷ�������ַ������ַ������ȴ��ڻ����configMAX_TASK_NAME_LEN������±���ȷ��ֹ */
    pxNewTCB->pcTaskName[ configMAX_TASK_NAME_LEN - 1 ] = '\0';

    /* ���ȼ�����������������˱���ȷ��������̫�������Ƴ����ܴ��ڵ���Ȩλ */
    if( uxPriority >= ( UBaseType_t ) configMAX_PRIORITIES )
    {
        /* ������ȼ��������ֵ����������Ϊ������ȼ� */
        uxPriority = ( UBaseType_t ) configMAX_PRIORITIES - ( UBaseType_t ) 1U;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
    }

    /* �����������ȼ� */
    pxNewTCB->uxPriority = uxPriority;
    
    /* ���ʹ�û����������û������ȼ��ͻ��������м��� */
    #if ( configUSE_MUTEXES == 1 )
    {
        pxNewTCB->uxBasePriority = uxPriority;  /* ���û������ȼ� */
        pxNewTCB->uxMutexesHeld = 0;            /* ��ʼ�����������м���Ϊ0 */
    }
    #endif /* configUSE_MUTEXES */

    /* ��ʼ�������״̬�б�����¼��б��� */
    vListInitialiseItem( &( pxNewTCB->xStateListItem ) );
    vListInitialiseItem( &( pxNewTCB->xEventListItem ) );

    /* ���ô�ListItem_t���ص�pxNewTCB�����ӡ��������ǿ��Դ��б��е�ͨ����ص�������TCB */
    listSET_LIST_ITEM_OWNER( &( pxNewTCB->xStateListItem ), pxNewTCB );

    /* �¼��б����ǰ����ȼ�˳������ */
    listSET_LIST_ITEM_VALUE( &( pxNewTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) uxPriority ); /*lint !e961 MISRA�쳣����Ϊת������ĳЩ�˿�������� */
    listSET_LIST_ITEM_OWNER( &( pxNewTCB->xEventListItem ), pxNewTCB );

    /* ���TCB�а����ؼ���Ƕ�׼�������ʼ��Ϊ0 */
    #if ( portCRITICAL_NESTING_IN_TCB == 1 )
    {
        pxNewTCB->uxCriticalNesting = ( UBaseType_t ) 0U;
    }
    #endif /* portCRITICAL_NESTING_IN_TCB */

    /* ���ʹ��Ӧ�ó��������ǩ����ʼ��ΪNULL */
    #if ( configUSE_APPLICATION_TASK_TAG == 1 )
    {
        pxNewTCB->pxTaskTag = NULL;
    }
    #endif /* configUSE_APPLICATION_TASK_TAG */

    /* �����������ʱͳ����Ϣ����ʼ������ʱ�������Ϊ0 */
    #if ( configGENERATE_RUN_TIME_STATS == 1 )
    {
        pxNewTCB->ulRunTimeCounter = 0UL;
    }
    #endif /* configGENERATE_RUN_TIME_STATS */

    /* ���ʹ��MPU��װ�����洢�����MPU���� */
    #if ( portUSING_MPU_WRAPPERS == 1 )
    {
        vPortStoreTaskMPUSettings( &( pxNewTCB->xMPUSettings ), xRegions, pxNewTCB->pxStack, ulStackDepth );
    }
    #else
    {
        /* �������������δ���ò����ľ��� */
        ( void ) xRegions;
    }
    #endif

    /* ����������̱߳��ش洢ָ�룬��ʼ��ΪNULL */
    #if( configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0 )
    {
        for( x = 0; x < ( UBaseType_t ) configNUM_THREAD_LOCAL_STORAGE_POINTERS; x++ )
        {
            pxNewTCB->pvThreadLocalStoragePointers[ x ] = NULL;
        }
    }
    #endif

    /* ���ʹ������֪ͨ����ʼ��ֵ֪ͨ��״̬ */
    #if ( configUSE_TASK_NOTIFICATIONS == 1 )
    {
        pxNewTCB->ulNotifiedValue = 0;                           /* ��ʼ��ֵ֪ͨΪ0 */
        pxNewTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION;  /* ��ʼ��֪ͨ״̬Ϊ���ȴ�֪ͨ */
    }
    #endif

    /* ���ʹ��Newlib�����룬��ʼ��Newlib������ṹ */
    #if ( configUSE_NEWLIB_REENTRANT == 1 )
    {
        /* ��ʼ���������Newlib������ṹ */
        _REENT_INIT_PTR( ( &( pxNewTCB->xNewLib_reent ) ) );
    }
    #endif

    /* �������������ֹ�ӳٹ��ܣ���ʼ���ӳ���ֹ��־ */
    #if( INCLUDE_xTaskAbortDelay == 1 )
    {
        pxNewTCB->ucDelayAborted = pdFALSE;  /* ��ʼ���ӳ���ֹ��־ΪFALSE */
    }
    #endif

    /* ��ʼ��TCBջ��ʹ�俴�����������Ѿ������е����������жϡ�
       ���ص�ַ����Ϊ����������ʼ��ַ��һ��ջ����ʼ����ջ�������ͻᱻ���� */
    #if( portUSING_MPU_WRAPPERS == 1 )
    {
        /* ʹ��MPUʱ����Ҫ������Ȩģʽ��Ϣ */
        pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxTaskCode, pvParameters, xRunPrivileged );
    }
    #else /* portUSING_MPU_WRAPPERS */
    {
        /* ��ʹ��MPUʱ������Ҫ������Ȩģʽ��Ϣ */
        pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxTaskCode, pvParameters );
    }
    #endif /* portUSING_MPU_WRAPPERS */

    /* ����ṩ��������ָ�룬���������TCBָ�븳���� */
    if( ( void * ) pxCreatedTask != NULL )
    {
        /* ��������ʽ���ݾ�����þ�������ڸ��Ĵ�����������ȼ���ɾ������������� */
        *pxCreatedTask = ( TaskHandle_t ) pxNewTCB;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
    }
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvAddNewTaskToReadyList
 * �������������´�����������ӵ������б���������س�ʼ���͵����߼�
 *           �˺��������񴴽����̵����һ�������������������ϵͳ����
 * ���������
 *   - pxNewTCB: ָ����������ƿ�(TCB)��ָ�룬����������������Ժ�״̬��Ϣ
 * �����������
 * �� �� ֵ���ޣ���̬�����������ⲿ�ɼ���
 * ����˵����
 *   - �˺����Ǿ�̬����������FreeRTOS�ں��ڲ�ʹ��
 *   - ���ٽ����ڲ��������б���ֹ�жϷ��ʵ��µ����ݲ�һ��
 *   - �����һ�����񴴽��������������ʼ�������б�
 *   - �������������������
 *   - �����������ȼ������Ƿ��������������л�
 *   - ֧�ָ�����ʩ��Ϊ�����ṩ������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
static void prvAddNewTaskToReadyList( TCB_t *pxNewTCB )
{
    /* ȷ���ڸ��������б�ʱ�жϲ��������Щ�б������ٽ��� */
    taskENTER_CRITICAL();
    {
        /* ���ӵ�ǰ�������������� */
        uxCurrentNumberOfTasks++;
        
        /* ��鵱ǰ�Ƿ��������е����� */
        if( pxCurrentTCB == NULL )
        {
            /* û���������񣬻��������������񶼴��ڹ���״̬ - ʹ�������Ϊ��ǰ���� */
            pxCurrentTCB = pxNewTCB;

            /* ����Ƿ��Ǵ����ĵ�һ������ */
            if( uxCurrentNumberOfTasks == ( UBaseType_t ) 1 )
            {
                /* ���Ǵ����ĵ�һ���������ִ������ĳ�����ʼ����
                   ����˵���ʧ�ܣ����ǽ��޷��ָ������ᱨ��ʧ�� */
                prvInitialiseTaskLists();  /* ��ʼ�������б� */
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
            }
        }
        else
        {
            /* ������ȳ�����δ���У��Ҵ�����������Ϊֹ���������ȼ���ߵ�������ʹ���Ϊ��ǰ���� */
            if( xSchedulerRunning == pdFALSE )
            {
                /* �Ƚ�������͵�ǰ��������ȼ� */
                if( pxCurrentTCB->uxPriority <= pxNewTCB->uxPriority )
                {
                    pxCurrentTCB = pxNewTCB;  /* ���������ȼ����ߣ���Ϊ��ǰ���� */
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
            }
        }

        /* ����ȫ�������ż����� */
        uxTaskNumber++;

        /* ������ø�����ʩ��ΪTCB���һ�����������ڸ��� */
        #if ( configUSE_TRACE_FACILITY == 1 )
        {
            /* ��TCB�����һ�����ڸ��ٵļ����� */
            pxNewTCB->uxTCBNumber = uxTaskNumber;
        }
        #endif /* configUSE_TRACE_FACILITY */
        
        /* �������񴴽��¼� */
        traceTASK_CREATE( pxNewTCB );

        /* ����������ӵ������б� */
        prvAddTaskToReadyList( pxNewTCB );

        /* �˿��ض���TCB���ã�����еĻ��� */
        portSETUP_TCB( pxNewTCB );
    }
    /* �˳��ٽ��� */
    taskEXIT_CRITICAL();

    /* �����ȳ����Ƿ��Ѿ������� */
    if( xSchedulerRunning != pdFALSE )
    {
        /* ����������������ȼ����ڵ�ǰ��������Ӧ���������� */
        if( pxCurrentTCB->uxPriority < pxNewTCB->uxPriority )
        {
            /* ���ʹ����ռʽ���ȣ�ִ�������ò� */
            taskYIELD_IF_USING_PREEMPTION();
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
        }
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
    }
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�vTaskDelete
 * ����������ɾ��ָ�����񣬽���������б����Ƴ����ͷ������Դ
 *           ֧��ɾ�������������������ɾ��������������ֹ����������
 * ���������
 *   - xTaskToDelete: Ҫɾ���������������ΪNULL��ɾ��������������
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺���������������ɾ������ʱ���루INCLUDE_vTaskDelete == 1��
 *   - ����ɾ�������������������ɾ��
 *   - ��������ɾ����������Ҫ����������������Դ
 *   - �Ӿ����б��¼��б�����Ƴ�����
 *   - �������������������
 *   - ���ܴ����������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
#if ( INCLUDE_vTaskDelete == 1 )

void vTaskDelete( TaskHandle_t xTaskToDelete )
{
    TCB_t *pxTCB;  /* ָ��Ҫɾ����������ƿ��ָ�� */

    /* �����ٽ��������������б���� */
    taskENTER_CRITICAL();
    {
        /* �������NULL�����ʾҪɾ�����ô˺������������� */
        pxTCB = prvGetTCBFromHandle( xTaskToDelete );

        /* �Ӿ����б����Ƴ����� */
        if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
        {
            /* ��������б��Ϊ�գ����ø����ȼ��ľ���λ */
            taskRESET_READY_PRIORITY( pxTCB->uxPriority );
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
        }

        /* ��������Ƿ�Ҳ���¼��б��еȴ� */
        if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
        {
            /* ���¼��б����Ƴ����� */
            ( void ) uxListRemove( &( pxTCB->xEventListItem ) );
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
        }

        /* ���������ţ��Ա��ں˸�֪���������Լ�⵽�����б���Ҫ�������ɡ�
           ��portPRE_TASK_DELETE_HOOK()֮ǰ��ɴ˲�������Ϊ��Windows�˿��иú겻�᷵�� */
        uxTaskNumber++;

        /* ����Ƿ�Ҫɾ����ǰ�������е����� */
        if( pxTCB == pxCurrentTCB )
        {
            /* ��������ɾ�������ⲻ��������������ɣ���Ϊ��Ҫ�л�����һ������
               �����������ֹ�б��С��������񽫼����ֹ�б��ͷŵ�����Ϊ��ɾ�������TCB��ջ����������ڴ� */
            vListInsertEnd( &xTasksWaitingTermination, &( pxTCB->xStateListItem ) );

            /* ����ucTasksDeleted�������Ա��������֪���������ѱ�ɾ�������Ӧ�ü��xTasksWaitingTermination�б� */
            ++uxDeletedTasksWaitingCleanUp;

            /* Ԥɾ��������Ҫ����Windowsģ������������ִ��Windows�ض������������
               ֮���޷��Ӵ������ò� - ���ʹ��xYieldPending��������Ҫ�������л� */
            portPRE_TASK_DELETE_HOOK( pxTCB, &xYieldPending );
        }
        else
        {
            /* ���ٵ�ǰ�������� */
            --uxCurrentNumberOfTasks;
            
            /* ֱ��ɾ�������TCB��������ƿ飩 */
            prvDeleteTCB( pxTCB );

            /* ������һ��Ԥ�ڵĽ������ʱ�䣬�Է��������˸ո�ɾ�������� */
            prvResetNextTaskUnblockTime();
        }

        /* ��������ɾ���¼� */
        traceTASK_DELETE( pxTCB );
    }
    /* �˳��ٽ��� */
    taskEXIT_CRITICAL();

    /* ����ո�ɾ�����ǵ�ǰ�������е�����ǿ�����µ��� */
    if( xSchedulerRunning != pdFALSE )
    {
        if( pxTCB == pxCurrentTCB )
        {
            /* ����ȷ��������û�б����� */
            configASSERT( uxSchedulerSuspended == 0 );
            
            /* ��API�ڲ����������ò��������������л� */
            portYIELD_WITHIN_API();
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
        }
    }
}

#endif /* INCLUDE_vTaskDelete */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskDelayUntil == 1 ) /* �������룺ֻ�е�FreeRTOS������������vTaskDelayUntil����ʱ���˺�������ű��������� */

/*******************************************************************************
 �������ƣ� vTaskDelayUntil
 ����������    ʵ�־�ȷ�������������ӳ٣�ȷ�������Թ̶�Ƶ��ִ�С��˺������ھ���ʱ��������ʱ�䣬
               �ܹ��Զ���������ִ��ʱ��ı仯�������ۻ����ṩ��vTaskDelay���ߵ�ʱ�侫�ȡ�
 ���������   pxPreviousWakeTime - ָ��TickType_t������ָ�룬���ڴ洢/�����ϴλ���ʱ�䡣
               �״ε���ǰ�����ʼ��Ϊ��ǰʱ�䣨��xTaskGetTickCount()����
               xTimeIncrement - ����ִ�еĹ̶����ڣ���ʱ�ӽ�����Ϊ��λ��
 ���������    pxPreviousWakeTime - �������Զ����´˱���Ϊ��һ�λ��ѵ�ʱ�䡣
 �� �� ֵ��    ��
 ����˵����    �˺���ΪFreeRTOS����API�������ں�INCLUDE_vTaskDelayUntil����Ϊ1ʱ���á�
               �����ڲ��ᴦ��ϵͳʱ�Ӽ�������������������ȷ�����κ�����¶�����ȷ�����ӳ١�

 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2024/06/05     V1.00          AI Assistant       �����������ϸע��
 *******************************************************************************/

	void vTaskDelayUntil( TickType_t * const pxPreviousWakeTime, const TickType_t xTimeIncrement )
	{
	TickType_t xTimeToWake; /* ���������һ������Ӧ�û��ѵľ���ʱ��� */
	BaseType_t xAlreadyYielded, xShouldDelay = pdFALSE; /* xAlreadyYielded: ��¼xTaskResumeAll�Ƿ��Ѵ�������; xShouldDelay: ����Ƿ���Ҫ�ӳ� */

		/* �������Լ��: ȷ��pxPreviousWakeTimeָ����Ч */
		configASSERT( pxPreviousWakeTime );
		/* �������Լ��: ȷ��ʱ����������0 */
		configASSERT( ( xTimeIncrement > 0U ) );
		/* ���Լ��: ȷ��������δ������ */
		configASSERT( uxSchedulerSuspended == 0 );

		/* �����������񣬷�ֹ�ڼ�������б�����������жϸ��� */
		vTaskSuspendAll();
		{
			/* С�Ż�: �ڴ˴�����ڣ�ʱ�ӽ��ļ�������仯�����䱣��Ϊ���������Ч�ʺ�ȷ��һ���� */
			const TickType_t xConstTickCount = xTickCount;

			/* ����������һ��ϣ�����ѵľ���ʱ��: �ϴλ���ʱ�� + ���� */
			xTimeToWake = *pxPreviousWakeTime + xTimeIncrement;

			/* ����ʱ�Ӽ���������ĸ������ */
			if( xConstTickCount < *pxPreviousWakeTime )
			{
				/* ���1: ���ϴε��ô˺�����ʱ�ӽ��ļ������Ѿ������
				   ����������£�ֻ�е�����ʱ��Ҳ����������һ���ʱ����ڵ�ǰ���ļ���ʱ������Ҫ�ӳ١�
				   ��������ͺ�������ʱ�䶼û�����һ������ */
				if( ( xTimeToWake < *pxPreviousWakeTime ) && ( xTimeToWake > xConstTickCount ) )
				{
					xShouldDelay = pdTRUE; /* ��Ҫ�ӳ� */
				}
				else
				{
					/* ���븲�ǲ��Ա��: ����Ҫ�ӳٵ���� */
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				/* ���2: ʱ�ӽ��ļ�����û�������
				   ����������£��������ʱ�����������/��ǰ����ʱ��С�ڻ���ʱ�䣬����Ҫ�ӳ١� */
				if( ( xTimeToWake < *pxPreviousWakeTime ) || ( xTimeToWake > xConstTickCount ) )
				{
					xShouldDelay = pdTRUE; /* ��Ҫ�ӳ� */
				}
				else
				{
					/* ���븲�ǲ��Ա��: ����Ҫ�ӳٵ���� */
					mtCOVERAGE_TEST_MARKER();
				}
			}

			/* ���»���ʱ�䣬Ϊ��һ�ε�������׼�� */
			*pxPreviousWakeTime = xTimeToWake;

			/* �����жϾ����Ƿ���Ҫ�����ӳ� */
			if( xShouldDelay != pdFALSE )
			{
				/* ����trace�꣬��������ӳٵ�����Ϣ */
				traceTASK_DELAY_UNTIL( xTimeToWake );

				/* prvAddCurrentTaskToDelayedList()��Ҫ�����������ʱ�䣬�����Ǿ��Ի���ʱ�䣬
				   �����Ҫ��ȥ��ǰ���ļ������������Ҫ�ӳٵĽ����� */
				prvAddCurrentTaskToDelayedList( xTimeToWake - xConstTickCount, pdFALSE );
			}
			else
			{
				/* ���븲�ǲ��Ա��: ����Ҫ�ӳٵ���� */
				mtCOVERAGE_TEST_MARKER();
			}
		}
		/* �ָ��������񣬲���ȡ����ֵ���Ƿ��Ѵ���������ȣ� */
		xAlreadyYielded = xTaskResumeAll();

		/* ���xTaskResumeAllû���Ѿ��������ȣ��������ǿ����Ѿ����Լ���Ϊ˯��״̬����ǿ�ƽ���һ�ε��� */
		if( xAlreadyYielded == pdFALSE )
		{
			/* ��API�ڲ�����һ��������� */
			portYIELD_WITHIN_API();
		}
		else
		{
			/* ���븲�ǲ��Ա��: �Ѿ����������ȵ���� */
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* INCLUDE_vTaskDelayUntil */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskDelay == 1 ) /* �������룺ֻ�е�FreeRTOS������������vTaskDelay����ʱ���˺�������ű��������� */

/*******************************************************************************
 �������ƣ� vTaskDelay
 ����������    ����ǰ�����ӳ٣�������ָ����ʱ�ӽ�����������һ������ӳ٣��ӵ���vTaskDelay��ʱ�̿�ʼ�����ӳ�ʱ�䡣
               �������ӳ��ڼ䴦������״̬������ռ��CPUʱ�䣬���������������С�
 ���������   xTicksToDelay - Ҫ�ӳٵ�ʱ�ӽ����������Ϊ0����ǿ�ƽ���һ�������л������ó�CPU����
 ���������    ��
 �� �� ֵ��    ��
 ����˵����    �˺���ΪFreeRTOS����API�������ں�INCLUDE_vTaskDelay����Ϊ1ʱ���á�
               ��vTaskDelayUntil��ͬ���˺����ṩ��������ӳ٣����ʺ���Ҫ��ȷ���ڵ�Ӧ�á�

 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2024/06/05     V1.00          AI Assistant       �����������ϸע��
 *******************************************************************************/

	void vTaskDelay( const TickType_t xTicksToDelay )
	{
	BaseType_t xAlreadyYielded = pdFALSE; /* ���xTaskResumeAll�Ƿ��Ѿ�������������� */

		/* ����ӳ�ʱ��Ϊ0��ֻ��ǿ�ƽ���һ�����µ��ȣ������л��� */
		if( xTicksToDelay > ( TickType_t ) 0U )
		{
			/* ���Լ�飺ȷ��������û�б����� */
			configASSERT( uxSchedulerSuspended == 0 );
			/* �����������񣬷�ֹ�ڲ��������б�����������жϸ��� */
			vTaskSuspendAll();
			{
				/* ����trace�꣬��������ӳٵ�����Ϣ */
				traceTASK_DELAY();

				/* ��������������ʱ���¼��б����Ƴ��������ڵ������ָ�֮ǰ���ᱻ��������б��������б����Ƴ���
				
				��ǰִ�е����񲻿������¼��б��У���Ϊ���ǵ�ǰ����ִ�е����� */
				/* ����ǰ������ӵ��ӳ��б�ָ���ӳٵĽ����� */
				prvAddCurrentTaskToDelayedList( xTicksToDelay, pdFALSE );
			}
			/* �ָ��������񣬲���ȡ����ֵ���Ƿ��Ѵ���������ȣ� */
			xAlreadyYielded = xTaskResumeAll();
		}
		else
		{
			/* ���븲�ǲ��Ա�ǣ��ӳ�ʱ��Ϊ0����� */
			mtCOVERAGE_TEST_MARKER();
		}

		/* ���xTaskResumeAllû���Ѿ��������ȣ��������ǿ����Ѿ����Լ���Ϊ˯��״̬����ǿ�ƽ���һ�ε��� */
		if( xAlreadyYielded == pdFALSE )
		{
			/* ��API�ڲ�����һ��������� */
			portYIELD_WITHIN_API();
		}
		else
		{
			/* ���븲�ǲ��Ա�ǣ��Ѿ����������ȵ���� */
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* INCLUDE_vTaskDelay */
/*-----------------------------------------------------------*/

#if( ( INCLUDE_eTaskGetState == 1 ) || ( configUSE_TRACE_FACILITY == 1 ) )

/*******************************************************************************
 * ��������: eTaskGetState
 * ��������: ��ȡָ�������״̬��Ϣ���ú���ͨ����ѯ�������ڵ�״̬�б���ȷ������ǰ״̬��
 *           ��������̬������̬������̬������̬����ɾ��״̬��
 * �������: xTask - Ҫ��ѯ״̬����������������ƿ�ָ�룩
 * �������: ��
 * �� �� ֵ: eTaskStateö�����ͣ���ʾ����ĵ�ǰ״̬��
 *           eRunning   - ������������
 *           eReady     - �����ھ���״̬
 *           eBlocked   - ����������״̬
 *           eSuspended - �����ڹ���״̬
 *           eDeleted   - �����ѱ�ɾ��
 * ����˵��: 1.�˺�����Ҫ��FreeRTOS����������INCLUDE_eTaskGetState��configUSE_TRACE_FACILITY
 *          2.�����ڲ�ʹ���ٽ�������״̬��ѯ����
 *          3.�����ѯ���ǵ�ǰ�������е�����ֱ�ӷ���eRunning
 *          4.����״̬ͨ�����������������ȷ��
 * �޸�����      �汾��          �޸���            �޸�����
 * ----------------------------------------------------------------------------
 * 2024/06/02     V1.00          ChatGPT            �����������ϸע��
 *******************************************************************************/
eTaskState eTaskGetState( TaskHandle_t xTask )
{
eTaskState eReturn;                         /* ���巵��ֵ���� */
List_t *pxStateList;                        /* ָ����������״̬�����ָ�� */
const TCB_t * const pxTCB = ( TCB_t * ) xTask;  /* ��������ת��Ϊ������ƿ�ָ�� */

    configASSERT( pxTCB );                  /* ��֤������ƿ�ָ����Ч�� */

    if( pxTCB == pxCurrentTCB )             /* ����Ƿ�Ϊ��ǰ�������� */
    {
        /* �������ڲ�ѯ����״̬��ֱ�ӷ�������״̬ */
        eReturn = eRunning;
    }
    else
    {
        taskENTER_CRITICAL();               /* �����ٽ�������״̬��ѯ */
        {
            /* ��ȡ����״̬�б������ڵ����� */
            pxStateList = ( List_t * ) listLIST_ITEM_CONTAINER( &( pxTCB->xStateListItem ) );
        }
        taskEXIT_CRITICAL();                /* �˳��ٽ��� */

        if( ( pxStateList == pxDelayedTaskList ) || ( pxStateList == pxOverflowDelayedTaskList ) )
        {
            /* �������ӳ�����������ӳ������б��� */
            eReturn = eBlocked;
        }

        #if ( INCLUDE_vTaskSuspend == 1 )   /* ������������������ */
            else if( pxStateList == &xSuspendedTaskList )  /* �����ڹ��������б� */
            {
                /* ����������������������������� */
                if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) == NULL )
                {
                    /* �¼��б�����κ������У���ʾ������������ */
                    eReturn = eSuspended;
                }
                else
                {
                    /* �¼��б����������У���ʾ����������״̬ */
                    eReturn = eBlocked;
                }
            }
        #endif  /* INCLUDE_vTaskSuspend */

        #if ( INCLUDE_vTaskDelete == 1 )    /* �������������ɾ������ */
            else if( ( pxStateList == &xTasksWaitingTermination ) || ( pxStateList == NULL ) )
            {
                /* �����ڴ���ֹ�б��δ���κ��б��У���ɾ��״̬�� */
                eReturn = eDeleted;
            }
        #endif  /* INCLUDE_vTaskDelete */

        else /*lint !e525 �����������棬ʹԤ����ṹ������ */
        {
            /* ���񲻴��������κ�״̬�����ھ���״̬������������״̬�� */
            eReturn = eReady;
        }
    }

    return eReturn;  /* ��������״̬��ѯ��� */
} /*lint !e818 xTask����Ϊconstָ�룬��Ϊ����typedef���� */

#endif /* INCLUDE_eTaskGetState��configUSE_TRACE_FACILITY���ü�� */
/*-----------------------------------------------------------*/

#if ( INCLUDE_uxTaskPriorityGet == 1 ) /* �������룺ֻ�е�FreeRTOS������������uxTaskPriorityGet����ʱ���˺�������ű��������� */

/*******************************************************************************
 �������ƣ� uxTaskPriorityGet
 ����������    ��ȡָ������ĵ�ǰ���ȼ������Բ�ѯ������������ȼ���������������
 ���������   xTask - Ҫ��ѯ���ȼ������������������NULL�����ʾ��ѯ����������������ȼ���
 ���������    ��
 �� �� ֵ��    UBaseType_t���� - ����ָ������ĵ�ǰ���ȼ�ֵ��
 ����˵����    �˺���ΪFreeRTOS����API�������ں�INCLUDE_uxTaskPriorityGet����Ϊ1ʱ���á�
               �����ڲ�ʹ���ٽ���������ȷ���ڶ�ȡ�������ȼ�ʱ���ᱻ����������жϸ��š�

 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2024/06/05     V1.00          AI Assistant       �����������ϸע��
 *******************************************************************************/

	UBaseType_t uxTaskPriorityGet( TaskHandle_t xTask )
	{
	TCB_t *pxTCB; /* ָ��������ƿ�(TCB)��ָ�� */
	UBaseType_t uxReturn; /* �洢Ҫ���ص����ȼ�ֵ */

		/* �����ٽ�����������������ƿ�ķ��� */
		taskENTER_CRITICAL();
		{
			/* �������NULL�����ʾ��ѯ����uxTaskPriorityGet()��������������ȼ� */
			pxTCB = prvGetTCBFromHandle( xTask ); /* ͨ����������ȡ��Ӧ��������ƿ� */
			uxReturn = pxTCB->uxPriority; /* ��������ƿ��ж�ȡ���ȼ��ֶ� */
		}
		/* �˳��ٽ��� */
		taskEXIT_CRITICAL();

		/* ���ػ�ȡ�������ȼ�ֵ */
		return uxReturn;
	}

#endif /* INCLUDE_uxTaskPriorityGet */
/*-----------------------------------------------------------*/

#if ( INCLUDE_uxTaskPriorityGet == 1 ) /* �������룺ֻ�е�FreeRTOS������������uxTaskPriorityGet����ʱ���˺�������ű��������� */

/*******************************************************************************
 �������ƣ� uxTaskPriorityGetFromISR
 ����������    ���жϷ�������(ISR)�л�ȡָ������ĵ�ǰ���ȼ�������uxTaskPriorityGet()���жϰ�ȫ�汾��
               ���Բ�ѯ������������ȼ���רΪ���ж��������е��ö���ơ�
 ���������   xTask - Ҫ��ѯ���ȼ������������������NULL�����ʾ��ѯ��ǰ�������е���������ȼ���
 ���������    ��
 �� �� ֵ��    UBaseType_t���� - ����ָ������ĵ�ǰ���ȼ�ֵ��
 ����˵����    �˺���ΪFreeRTOS�жϰ�ȫAPI��ֻ�����жϷ��������е��á�
               ����ʹ���ж����ζ��������ٽ����������ؼ�����Σ�ȷ�����ж��������еİ�ȫ���ʡ�

 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2024/06/05     V1.00          AI Assistant       �����������ϸע��
 *******************************************************************************/

	UBaseType_t uxTaskPriorityGetFromISR( TaskHandle_t xTask )
	{
	TCB_t *pxTCB; /* ָ��������ƿ�(TCB)��ָ�� */
	UBaseType_t uxReturn, uxSavedInterruptState; /* uxReturn: �洢Ҫ���ص����ȼ�ֵ; uxSavedInterruptState: �����ж�״̬ */

		/* ����ж����ȼ���Ч�ԣ�
		   FreeRTOS֧���ж�Ƕ�׵Ķ˿������ϵͳ�����ж����ȼ��ĸ��
		   ���ڴ����ȼ����жϼ�ʹ���ں��ٽ���Ҳ�������ã������ܵ����κ�FreeRTOS API������
		   ֻ����FromISR��β��FreeRTOS�������Դ����ȼ����ڻ�������ϵͳ�����ж����ȼ����ж��е��á�
		   �˺����֤��ǰ�ж����ȼ��Ƿ���Ч�������Ч��ᴥ������ʧ�ܡ� */
		portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

		/* ���浱ǰ�ж�״̬�������жϣ��жϰ�ȫ�汾�������������ؼ������ */
		uxSavedInterruptState = portSET_INTERRUPT_MASK_FROM_ISR();
		{
			/* �������NULL�����ʾ��ѯ��ǰ�������е���������ȼ� */
			pxTCB = prvGetTCBFromHandle( xTask ); /* ͨ����������ȡ��Ӧ��������ƿ� */
			uxReturn = pxTCB->uxPriority; /* ��������ƿ��ж�ȡ���ȼ��ֶ� */
		}
		/* �ָ�֮ǰ������ж�״̬ */
		portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptState );

		/* ���ػ�ȡ�������ȼ�ֵ */
		return uxReturn;
	}

#endif /* INCLUDE_uxTaskPriorityGet */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskPrioritySet == 1 ) /* �������룺ֻ�е�FreeRTOS������������vTaskPrioritySet����ʱ���˺�������ű��������� */

/*******************************************************************************
 �������ƣ� vTaskPrioritySet
 ����������    ����ָ����������ȼ������Զ�̬�ı���������ȼ���������������
               ����������µ����ȼ����µ��������ھ����б��е�λ�ã����ڱ�Ҫʱ����������ȡ�
 ���������   xTask - Ҫ�������ȼ������������������NULL�����ʾ���õ���������������ȼ���
              uxNewPriority - �µ����ȼ�ֵ������С��configMAX_PRIORITIES��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����    �˺���ΪFreeRTOS����API�������ں�INCLUDE_vTaskPrioritySet����Ϊ1ʱ���á�
               �����ڲ�ʹ���ٽ���������ȷ�����޸��������ȼ�ʱ���ᱻ����������жϸ��š�
               ��ʹ�û�����ʱ����������ȷ�������ȼ��̳л��ơ�

 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2024/06/05     V1.00          AI Assistant       �����������ϸע��
 *******************************************************************************/

	void vTaskPrioritySet( TaskHandle_t xTask, UBaseType_t uxNewPriority )
	{
	TCB_t *pxTCB; /* ָ��������ƿ�(TCB)��ָ�� */
	UBaseType_t uxCurrentBasePriority, uxPriorityUsedOnEntry; /* uxCurrentBasePriority: ��ǰ�������ȼ�; uxPriorityUsedOnEntry: ����ʱ�����ȼ� */
	BaseType_t xYieldRequired = pdFALSE; /* ����Ƿ���Ҫ����������� */

		/* ���Լ�飺ȷ�������ȼ�ֵ��Ч��С�����õ�������ȼ��� */
		configASSERT( ( uxNewPriority < configMAX_PRIORITIES ) );

		/* ȷ�������ȼ���Ч�����������Χ������Ϊ�������ֵ */
		if( uxNewPriority >= ( UBaseType_t ) configMAX_PRIORITIES )
		{
			uxNewPriority = ( UBaseType_t ) configMAX_PRIORITIES - ( UBaseType_t ) 1U;
		}
		else
		{
			/* ���븲�ǲ��Ա�ǣ����ȼ�ֵ��Ч����� */
			mtCOVERAGE_TEST_MARKER();
		}

		/* �����ٽ�����������������ƿ�ķ��� */
		taskENTER_CRITICAL();
		{
			/* �������NULL�����ʾҪ�ı������������ȼ� */
			pxTCB = prvGetTCBFromHandle( xTask ); /* ͨ����������ȡ��Ӧ��������ƿ� */

			/* ����trace�꣬����������ȼ����õ�����Ϣ */
			traceTASK_PRIORITY_SET( pxTCB, uxNewPriority );

			/* �����Ƿ�ʹ�û���������ȡ��ǰ�Ļ������ȼ� */
			#if ( configUSE_MUTEXES == 1 )
			{
				uxCurrentBasePriority = pxTCB->uxBasePriority; /* ��ȡ�������ȼ����������ȼ��̳У� */
			}
			#else
			{
				uxCurrentBasePriority = pxTCB->uxPriority; /* ֱ�ӻ�ȡ�������ȼ� */
			}
			#endif

			/* ������ȼ��Ƿ�ȷʵ��Ҫ�ı� */
			if( uxCurrentBasePriority != uxNewPriority )
			{
				/* ���ȼ��ı���ܻᵼ��һ���ȵ����������ȼ����ߵ������Ϊ����״̬ */

				/* �������ȼ���ߵ���� */
				if( uxNewPriority > uxCurrentBasePriority )
				{
					/* ����޸ĵĲ��ǵ�ǰ���е����� */
					if( pxTCB != pxCurrentTCB )
					{
						/* �������һ���ǵ�ǰ������������ȼ���
						   ���������ȼ��Ƿ���ߵ����ڻ���ڵ�ǰ������������ȼ��� */
						if( uxNewPriority >= pxCurrentTCB->uxPriority )
						{
							/* ��Ҫ����������ȣ���Ϊ�����и������ȼ���������� */
							xYieldRequired = pdTRUE;
						}
						else
						{
							/* ���븲�ǲ��Ա�ǣ������ȼ��Ե��ڵ�ǰ������������ */
							mtCOVERAGE_TEST_MARKER();
						}
					}
					else
					{
						/* ������ߵ�ǰ������������ȼ�������ǰ���������Ѿ���������ȼ�����
						   ���Բ���Ҫ�������� */
					}
				}
				/* �������ȼ����͵���� */
				else if( pxTCB == pxCurrentTCB )
				{
					/* ���͵�ǰ������������ȼ���ζ�����ڿ�������һ���������ȼ������������ִ�� */
					xYieldRequired = pdTRUE;
				}
				else
				{
					/* �����κ�������������ȼ�����Ҫ�������ȣ���Ϊ������������ȼ�
					   ������ڱ��޸�����������ȼ� */
				}

				/* ���޸�uxPriority��Ա֮ǰ����ס������ܱ����õľ����б�
				   �Ա�taskRESET_READY_PRIORITY()������ȷ���� */
				uxPriorityUsedOnEntry = pxTCB->uxPriority;

				/* �����Ƿ�ʹ�û��������������ȼ� */
				#if ( configUSE_MUTEXES == 1 )
				{
					/* ֻ�е�����ǰû��ʹ�ü̳����ȼ�ʱ���Ÿ�������ʹ�õ����ȼ� */
					if( pxTCB->uxBasePriority == pxTCB->uxPriority )
					{
						pxTCB->uxPriority = uxNewPriority; /* ���õ�ǰ���ȼ� */
					}
					else
					{
						/* ���븲�ǲ��Ա�ǣ���������ʹ�ü̳����ȼ������ */
						mtCOVERAGE_TEST_MARKER();
					}

					/* ���ۺ���������������ȼ���Ҫ���� */
					pxTCB->uxBasePriority = uxNewPriority; /* ���û������ȼ� */
				}
				#else
				{
					/* ��ʹ�û�����ʱ��ֱ���������ȼ� */
					pxTCB->uxPriority = uxNewPriority;
				}
				#endif

				/* ֻ�е��¼��б����ֵû�б�����������;ʱ���������� */
				if( ( listGET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ) ) & taskEVENT_LIST_ITEM_VALUE_IN_USE ) == 0UL )
				{
					/* �����¼��б����ֵ���������¼��б��а����ȼ����� */
					listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), ( ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) uxNewPriority ) ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
				}
				else
				{
					/* ���븲�ǲ��Ա�ǣ��¼��б����ֵ���ڱ�ʹ�õ���� */
					mtCOVERAGE_TEST_MARKER();
				}

				/* �������������������б��У�����ֻ��Ҫ���������ȼ�������
				   ���ǣ���������ھ����б��У���Ҫ�����Ƴ����������������ȼ���Ӧ���б��� */
				if( listIS_CONTAINED_WITHIN( &( pxReadyTasksLists[ uxPriorityUsedOnEntry ] ), &( pxTCB->xStateListItem ) ) != pdFALSE )
				{
					/* ����ǰ��������б��� - �ڽ�����ӵ��µľ����б�֮ǰ���Ƴ���
					   �����������ٽ����ڣ���ʹ������������Ҳ����ִ�д˲��� */
					if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
					{
						/* ��֪������������б��У���������ٴμ�飬
						   ����ֱ�ӵ��ö˿ڼ�������ú� */
						portRESET_READY_PRIORITY( uxPriorityUsedOnEntry, uxTopReadyPriority );
					}
					else
					{
						/* ���븲�ǲ��Ա�ǣ��б��Ƴ������������������� */
						mtCOVERAGE_TEST_MARKER();
					}
					/* ��������ӵ��µľ����б��� */
					prvAddTaskToReadyList( pxTCB );
				}
				else
				{
					/* ���븲�ǲ��Ա�ǣ������ھ����б��е���� */
					mtCOVERAGE_TEST_MARKER();
				}

				/* �����Ҫ������������� */
				if( xYieldRequired != pdFALSE )
				{
					taskYIELD_IF_USING_PREEMPTION();
				}
				else
				{
					/* ���븲�ǲ��Ա�ǣ�����Ҫ���ȵ���� */
					mtCOVERAGE_TEST_MARKER();
				}

				/* ���˿��Ż�������ѡ��δʹ��ʱ���Ƴ�����δʹ�ñ����ı��������� */
				( void ) uxPriorityUsedOnEntry;
			}
		}
		/* �˳��ٽ��� */
		taskEXIT_CRITICAL();
	}

#endif /* INCLUDE_vTaskPrioritySet */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskSuspend == 1 ) /* �������룺ֻ�е�FreeRTOS���������������������ʱ���˺�������ű��������� */

/*******************************************************************************
 �������ƣ� vTaskSuspend
 ����������    ��һ���������ڹ���״̬������������񽫲��ٱ�ִ�У�ֱ��ͨ��`vTaskResume()`��`xTaskResumeFromISR()`����ָ���
               �˺������Թ����������񣬰�����ǰ�������е�����ͨ������NULL��������
 ���������   xTaskToSuspend - Ҫ���������ľ����TaskHandle_t���ͣ����������NULL�����ʾ����ǰ�������е�������
 ���������    ��
 �� �� ֵ��    ��
 ����˵����    �˺���ΪFreeRTOS����API�������ں�`INCLUDE_vTaskSuspend`����Ϊ1ʱ���á�
               �����ڲ������ٽ��������Ϳ��ܵ�������ȡ�

 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2024/06/05     V1.00          AI Assistant       �����������ϸע��
 *******************************************************************************/

	void vTaskSuspend( TaskHandle_t xTaskToSuspend )
	{
	TCB_t *pxTCB; /* ����һ��ָ�룬����ָ��Ҫ���������������ƿ�(TCB) */

		/* �����ٽ����������������������ж� */
		taskENTER_CRITICAL();
		{
			/* ͨ����������ȡ��Ӧ��������ƿ�(TCB)��ַ��
			   �������Ĳ�����NULL�����ڲ�����prvGetTCBFromHandle�᷵�ص�ǰ���������TCB */
			pxTCB = prvGetTCBFromHandle( xTaskToSuspend );

			/* ���� trace �꣬�������������������Ϣ��������˸��ٹ��ܣ� */
			traceTASK_SUSPEND( pxTCB );

			/* ������Ӿ����б���ӳ��б����Ƴ���
			   uxListRemove ����ֵ��ʾԭ�б�ʣ�������������Ϊ0��˵�������ȼ������б��������������� */
			if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
			{
				/* ��������ȼ��ľ����б�Ϊ�գ������õ������еľ������ȼ�λͼ��
				   ��������ȼ���Ӧ��λ����ʾ�����ȼ�û�о������� */
				taskRESET_READY_PRIORITY( pxTCB->uxPriority );
			}
			else
			{
				/* ���븲�ǲ��Ա�ǣ���ʾ else ��֧���ڣ���ͨ����ʵ�ʹ��ܴ��� */
				mtCOVERAGE_TEST_MARKER();
			}

			/* ��������Ƿ�Ҳ���¼��б��еȴ�������ȴ����С��ź����ȣ� */
			if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
			{
				/* ����������ڵȴ��¼���������¼��б����Ƴ� */
				( void ) uxListRemove( &( pxTCB->xEventListItem ) );
			}
			else
			{
				/* ���븲�ǲ��Ա�� */
				mtCOVERAGE_TEST_MARKER();
			}

			/* �������״̬�б�����뵽�ѹ��������б�xSuspendedTaskList����ĩβ */
			vListInsertEnd( &xSuspendedTaskList, &( pxTCB->xStateListItem ) );
		}
		/* �˳��ٽ��� */
		taskEXIT_CRITICAL();

		/* ���������Ƿ��Ѿ��������� */
		if( xSchedulerRunning != pdFALSE )
		{
			/* ����һ�����񱻹���������ԭ������һ��Ҫ�������������
			   �����Ҫ������һ������������ʱ�䣬���¼�����һ����쵽�ڵ�ʱ�� */
			taskENTER_CRITICAL();
			{
				prvResetNextTaskUnblockTime();
			}
			taskEXIT_CRITICAL();
		}
		else
		{
			/* ���븲�ǲ��Ա�ǣ�������δ���е������ */
			mtCOVERAGE_TEST_MARKER();
		}

		/* �жϱ�����������Ƿ��ǵ�ǰ�������е����� */
		if( pxTCB == pxCurrentTCB )
		{
			/* ��������ȷʵ�ǵ�ǰ���� */
			if( xSchedulerRunning != pdFALSE )
			{
				/* ȷ���ڹ���ǰ����ʱ��������û�б����𣨴�������״̬�� */
				configASSERT( uxSchedulerSuspended == 0 );
				/* ��������һ��������ȣ�portYIELD_WITHIN_API����
				   ��Ϊ��ǰ���񼴽�������CPU��Ҫ�л��������������� */
				portYIELD_WITHIN_API();
			}
			else
			{
				/* �����������δ����������ǰ����TCBָ������񱻹����ˣ�
				   ��Ҫ����pxCurrentTCBָ��һ����ͬ������ */

				/* ����ѹ��������б��е��������Ƿ������������ */
				if( listCURRENT_LIST_LENGTH( &xSuspendedTaskList ) == uxCurrentNumberOfTasks )
				{
					/* ����������񶼱������ˣ�û���������������
					   ��pxCurrentTCB����ΪNULL��
					   ��������һ�����񴴽�ʱ��pxCurrentTCB���Զ�ָ���� */
					pxCurrentTCB = NULL;
				}
				else
				{
					/* ����������������������ִ�������л���ѡ����һ��Ҫ���е����� */
					vTaskSwitchContext();
				}
			}
		}
		else
		{
			/* ���븲�ǲ��Ա�ǣ�����Ĳ��ǵ�ǰ���������� */
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* INCLUDE_vTaskSuspend */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskSuspend == 1 ) /* �������룺ֻ�е�FreeRTOS���������������������ʱ���˺�������ű��������� */

/*******************************************************************************
 �������ƣ� prvTaskIsTaskSuspended
 ����������    ��̬�������������ڼ��ָ�������Ƿ��������ڹ���״̬��
               �˺�������֤�����Ƿ��ڹ����б�����δ��ISR�ָ�����ȷ����������Ϊ�޳�ʱ���������ڹ����б�
 ���������   xTask - Ҫ������������TaskHandle_t���ͣ�������ΪNULL�Ҳ����ǵ�����������
 ���������    ��
 �� �� ֵ��    BaseType_t���� 
               - pdTRUE: ����ȷʵ���ڹ���״̬
               - pdFALSE: ���񲻴��ڹ���״̬
 ����˵����    �˺���ΪFreeRTOS�ڲ���̬���������ڹ���������ʱ���á�
               �������ٽ����ڵ��ã���Ϊ�������xPendingReadyList�ȹ�����Դ��
               ʹ��configASSERTȷ���������������ΪNULL��

 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2024/06/05     V1.00          AI Assistant       �����������ϸע��
 *******************************************************************************/

	static BaseType_t prvTaskIsTaskSuspended( const TaskHandle_t xTask )
	{
	BaseType_t xReturn = pdFALSE; /* ��ʼ������ֵΪpdFALSE����ʾ����δ���� */
	const TCB_t * const pxTCB = ( TCB_t * ) xTask; /* ��������ת��Ϊ������ƿ�(TCB)ָ�� */

		/* �˺��������xPendingReadyList�ȹ�����Դ����˱�����ٽ����ڵ��� */
		/* ע�ͣ����ô˺���ǰӦȷ���ѽ����ٽ��� */

		/* ��鴫�������������ΪNULL����Ϊ���������������Ƿ����û������ */
		configASSERT( xTask );

		/* ���Ҫ�ָ��������Ƿ�ȷʵ�ڹ��������б�(xSuspendedTaskList)�� */
		if( listIS_CONTAINED_WITHIN( &xSuspendedTaskList, &( pxTCB->xStateListItem ) ) != pdFALSE )
		{
			/* ��һ����飺�����Ƿ��Ѿ����жϷ������(ISR)�б��ָ������Ƿ��ڴ���������б�xPendingReadyList�У� */
			if( listIS_CONTAINED_WITHIN( &xPendingReadyList, &( pxTCB->xEventListItem ) ) == pdFALSE )
			{
				/* �ٴμ�飺�����Ƿ���Ϊ�������ڹ���״̬���ڹ����б��У�
				   ��������Ϊ�޳�ʱ�������¼��б�������ΪNULL��ʾ�������� */
				if( listIS_CONTAINED_WITHIN( NULL, &( pxTCB->xEventListItem ) ) != pdFALSE )
				{
					/* ͨ�����м�飬ȷ������ȷʵ���ڹ���״̬ */
					xReturn = pdTRUE;
				}
				else
				{
					/* ���븲�ǲ��Ա�ǣ������ڹ����б��е��¼��б�����������
					   ��ʾ��������Ϊ�޳�ʱ���������������� */
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				/* ���븲�ǲ��Ա�ǣ���������ISR�б��ָ���λ�ڴ���������б��У� */
				mtCOVERAGE_TEST_MARKER();
			}
		}
		else
		{
			/* ���븲�ǲ��Ա�ǣ������ڹ����б��� */
			mtCOVERAGE_TEST_MARKER();
		}

		/* ���ؼ���� */
		return xReturn;
	} /*lint !e818 xTask cannot be a pointer to const because it is a typedef. */
    /* Lintע�ͣ�����lint���棬xTask������ָ��const��ָ�룬��Ϊ����һ�����Ͷ��� */

#endif /* INCLUDE_vTaskSuspend */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskSuspend == 1 ) /* �������룺ֻ�е�FreeRTOS���������������������ʱ���˺�������ű��������� */

/*******************************************************************************
 �������ƣ� vTaskResume
 ����������    �ָ�һ�������������ʹ�����½������״̬���ܹ���������ѡ��ִ�С�
               �˺����������ڻָ���ǰ�������е�������Ҳ���ָܻ����ӳ١�������ԭ�����ͣ������
 ���������   xTaskToResume - Ҫ�ָ�����������TaskHandle_t���ͣ�������ΪNULL�Ҳ���ָ��ǰ����
 ���������    ��
 �� �� ֵ��    ��
 ����˵����    �˺���ΪFreeRTOS����API�������ں�`INCLUDE_vTaskSuspend`����Ϊ1ʱ���á�
               �����ڲ����������Ƿ��������ڹ���״̬����������ܵ����ȼ���ռ��
               ����ָ����������ȼ����ڵ�ǰ���񣬿��ܻᴥ�������л���

 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2024/06/05     V1.00          AI Assistant       �����������ϸע��
 *******************************************************************************/

	void vTaskResume( TaskHandle_t xTaskToResume )
	{
	/* ��������ת��Ϊ������ƿ�(TCB)ָ�룬ʹ��constȷ��ָ��ָ������ݲ����޸� */
	TCB_t * const pxTCB = ( TCB_t * ) xTaskToResume;

		/* ���Լ�飺�ָ���������������û������ģ���˴����������ΪNULL */
		configASSERT( xTaskToResume );

		/* ������飺����������ΪNULL���Ҳ����ǵ�ǰ����ִ�е�����
		   ��Ϊ�޷��ָ���ǰ�������е�����������Ͳ��ǹ���״̬�� */
		if( ( pxTCB != NULL ) && ( pxTCB != pxCurrentTCB ) )
		{
			/* �����ٽ����������������б�ķ��� */
			taskENTER_CRITICAL();
			{
				/* ʹ���ڲ�������������Ƿ��������ڹ���״̬ */
				if( prvTaskIsTaskSuspended( pxTCB ) != pdFALSE )
				{
					/* ���� trace �꣬�����������ָ�������Ϣ����������˸��ٹ��ܣ� */
					traceTASK_RESUME( pxTCB );

					/* �������ٽ����ڣ���ʹ����������������Ҳ���԰�ȫ�ط��ʾ����б� */
					
					/* ������ӹ����б����Ƴ� */
					( void ) uxListRemove(  &( pxTCB->xStateListItem ) );
					/* ��������ӵ������б��У�ʹ���ܹ���������ѡ��ִ�� */
					prvAddTaskToReadyList( pxTCB );

					/* ���ָ����������ȼ��Ƿ���ڻ���ڵ�ǰ������������ȼ� */
					if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
					{
						/* �ָ����������ȼ������ڵ�ǰ���񣬿�����Ҫ���������л� */
						
						/* ��Yield�������ܲ�������ʹ�ջָ����������У�
						   ����ʹ�����б�����ȷ״̬��Ϊ��һ��Yield����׼�� */
						taskYIELD_IF_USING_PREEMPTION();
					}
					else
					{
						/* ���븲�ǲ��Ա�ǣ��ָ����������ȼ����ڵ�ǰ���񣬲���Ҫ�����л� */
						mtCOVERAGE_TEST_MARKER();
					}
				}
				else
				{
					/* ���븲�ǲ��Ա�ǣ����񲻴��ڹ���״̬���������Ѿ��ָ����δ���� */
					mtCOVERAGE_TEST_MARKER();
				}
			}
			/* �˳��ٽ��� */
			taskEXIT_CRITICAL();
		}
		else
		{
			/* ���븲�ǲ��Ա�ǣ�������Ч��ΪNULL���ǵ�ǰ���� */
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* INCLUDE_vTaskSuspend */

/*-----------------------------------------------------------*/

#if ( ( INCLUDE_xTaskResumeFromISR == 1 ) && ( INCLUDE_vTaskSuspend == 1 ) )
/* �������룺ֻ�е�FreeRTOS������ͬʱ�����˴��жϻָ������ܺ����������ʱ���˺�������ű��������� */

/*******************************************************************************
 �������ƣ� xTaskResumeFromISR
 ����������    ���жϷ�������(ISR)�лָ�һ���������������vTaskResume()���жϰ�ȫ�汾��
               �˺����������ж������ĵ��ã����ڽ����������������Ϊ����״̬��
 ���������   xTaskToResume - Ҫ�ָ�����������TaskHandle_t���ͣ�������ΪNULL
 ���������    ��
 �� �� ֵ��    BaseType_t���� 
               - pdTRUE: ��Ҫ���ж��˳�ǰִ���������л�
               - pdFALSE: ����Ҫ���ж��˳�ǰִ���������л�
 ����˵����    �˺���ΪFreeRTOS�жϰ�ȫAPI��ֻ�����жϷ��������е��á�
               ����ָ����������ȼ����ڵ�ǰ���񣬿�����Ҫ�ֶ������������л���
               ʹ�ô˺���ǰ��ȷ��������INCLUDE_xTaskResumeFromISR��INCLUDE_vTaskSuspend��Ϊ1��

 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2024/06/05     V1.00          AI Assistant       �����������ϸע��
 *******************************************************************************/

	BaseType_t xTaskResumeFromISR( TaskHandle_t xTaskToResume )
	{
	BaseType_t xYieldRequired = pdFALSE; /* ��ʼ������ֵ��Ĭ�ϲ���Ҫ�������л� */
	TCB_t * const pxTCB = ( TCB_t * ) xTaskToResume; /* ��������ת��Ϊ������ƿ�(TCB)ָ�� */
	UBaseType_t uxSavedInterruptStatus; /* ���ڱ����ж�״̬���Ա�ָ� */

		/* ���Լ�飺���������������ΪNULL */
		configASSERT( xTaskToResume );

		/* ����ж����ȼ���Ч�ԣ�
		   FreeRTOS֧���ж�Ƕ�׵Ķ˿������ϵͳ�����ж����ȼ��ĸ��
		   ���ڴ����ȼ����жϼ�ʹ���ں��ٽ���Ҳ�������ã������ܵ����κ�FreeRTOS API������
		   ֻ����FromISR��β��FreeRTOS�������Դ����ȼ����ڻ�������ϵͳ�����ж����ȼ����ж��е��á�
		   �˺����֤��ǰ�ж����ȼ��Ƿ���Ч�������Ч��ᴥ������ʧ�ܡ� */
		portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

		/* ���浱ǰ�ж�״̬�������жϣ��жϰ�ȫ�汾���������������� */
		uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
		{
			/* ʹ���ڲ�������������Ƿ��������ڹ���״̬ */
			if( prvTaskIsTaskSuspended( pxTCB ) != pdFALSE )
			{
				/* ���� trace �꣬����������жϻָ�����ĵ�����Ϣ */
				traceTASK_RESUME_FROM_ISR( pxTCB );

				/* �������б��Ƿ���Ա����ʣ��������Ƿ�δ������ */
				if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
				{
					/* �����б���Է��ʣ�ֱ�ӽ�����ӹ����б��ƶ��������б� */
					
					/* ���ָ����������ȼ��Ƿ���ڻ���ڵ�ǰ������������ȼ� */
					if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
					{
						/* �ָ����������ȼ������ڵ�ǰ������Ҫ���ж��˳�ǰ�����������л� */
						xYieldRequired = pdTRUE;
					}
					else
					{
						/* ���븲�ǲ��Ա�ǣ��ָ����������ȼ����ڵ�ǰ���񣬲���Ҫ�������л� */
						mtCOVERAGE_TEST_MARKER();
					}

					/* ������ӹ����б����Ƴ� */
					( void ) uxListRemove( &( pxTCB->xStateListItem ) );
					/* ��������ӵ������б��У�ʹ���ܹ���������ѡ��ִ�� */
					prvAddTaskToReadyList( pxTCB );
				}
				else
				{
					/* �ӳ��б������б�ǰ�޷����ʣ������������𣩣�
					   ��˽�������ڴ���������б��У�ֱ���������ָ� */
					vListInsertEnd( &( xPendingReadyList ), &( pxTCB->xEventListItem ) );
				}
			}
			else
			{
				/* ���븲�ǲ��Ա�ǣ����񲻴��ڹ���״̬���������Ѿ��ָ����δ���� */
				mtCOVERAGE_TEST_MARKER();
			}
		}
		/* �ָ�֮ǰ������ж�״̬ */
		portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

		/* �����Ƿ���Ҫ�������л��ı�־ */
		return xYieldRequired;
	}

#endif /* ( ( INCLUDE_xTaskResumeFromISR == 1 ) && ( INCLUDE_vTaskSuspend == 1 ) ) */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * ��������: vTaskStartScheduler
 * ��������: ����FreeRTOS�������������ʼ��ϵͳ�ں������������������Ͷ�ʱ�����񣨿�ѡ����
 *           ����Ӳ����ʱ������ʼ������ȡ��ú������᷵�أ����ǵ���xTaskEndScheduler()��
 * �������: ��
 * �������: ��
 * �� �� ֵ: ��
 * ����˵��: 1.����configSUPPORT_STATIC_ALLOCATION����ѡ��̬��̬������������
 *          2.����configUSE_TIMERS���þ����Ƿ񴴽���ʱ������
 *          3.���ö˿��ض���xPortStartScheduler()����Ӳ��������
 * �޸�����      �汾��          �޸���            �޸�����
 * ----------------------------------------------------------------------------
 * 2025/09/02     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
void vTaskStartScheduler(void)
{
    BaseType_t xReturn; /* �������״̬�� */

    /* ��������ȼ������������� */
    #if (configSUPPORT_STATIC_ALLOCATION == 1) /* ��̬�ڴ�������� */
    {
        StaticTask_t *pxIdleTaskTCBBuffer = NULL;   /* ����������ƿ��ڴ�ָ�� */
        StackType_t *pxIdleTaskStackBuffer = NULL;  /* ���������ջ�ڴ�ָ�� */
        uint32_t ulIdleTaskStackSize;               /* ���������ջ��С */

        /* ͨ��Ӧ�ûص�������ȡ�û��ṩ�Ŀ��������ڴ��ַ */
        vApplicationGetIdleTaskMemory(&pxIdleTaskTCBBuffer, 
                                     &pxIdleTaskStackBuffer, 
                                     &ulIdleTaskStackSize);
        
        /* ʹ�þ�̬���������������� */
        xIdleTaskHandle = xTaskCreateStatic(
            prvIdleTask,                       /* ��������� */
            "IDLE",                            /* �������� */
            ulIdleTaskStackSize,               /* ��ջ��� */
            (void *)NULL,                      /* ������� */
            (tskIDLE_PRIORITY | portPRIVILEGE_BIT), /* �������ȼ�������Ȩλ�� */
            pxIdleTaskStackBuffer,             /* ��ջ������ָ�� */
            pxIdleTaskTCBBuffer                /* ������ƿ黺����ָ�� */
        );

        if (xIdleTaskHandle != NULL) /* ������񴴽���� */
        {
            xReturn = pdPASS; /* �����ɹ� */
        }
        else
        {
            xReturn = pdFAIL; /* ����ʧ�� */
        }
    }
    #else /* ��̬�ڴ�������� */
    {
        /* ʹ�ö�̬�ڴ���䷽�������������� */
        xReturn = xTaskCreate(
            prvIdleTask,                       /* ��������� */
            "IDLE",                            /* �������� */
            configMINIMAL_STACK_SIZE,          /* ʹ��Ĭ����С��ջ��� */
            (void *)NULL,                      /* ������� */
            (tskIDLE_PRIORITY | portPRIVILEGE_BIT), /* �������ȼ� */
            &xIdleTaskHandle                   /* ������ָ�� */
        );
    }
    #endif /* configSUPPORT_STATIC_ALLOCATION */

    #if (configUSE_TIMERS == 1) /* ��ʱ���������� */
    {
        if (xReturn == pdPASS) /* ���������񴴽��ɹ� */
        {
            xReturn = xTimerCreateTimerTask(); /* ������ʱ���������� */
        }
        else
        {
            mtCOVERAGE_TEST_MARKER(); /* ���븲�ǲ��Ա�� */
        }
    }
    #endif /* configUSE_TIMERS */

    if (xReturn == pdPASS) /* ���ǰ�����񴴽��Ƿ�ȫ���ɹ� */
    {
        /* �ر��ж�ȷ���ڵ���xPortStartScheduler()ǰ���ᷢ��ʱ�ӵδ��жϡ�
           �Ѵ�������Ķ�ջ�а����ж�����״̬�֣���˵���һ������ʼ����ʱ�жϻ��Զ��������� */
        portDISABLE_INTERRUPTS();

        #if (configUSE_NEWLIB_REENTRANT == 1) /* Newlib���������� */
        {
            /* ��Newlib��_impure_ptrָ�򼴽����е��׸������_reent�ṹ */
            _impure_ptr = &(pxCurrentTCB->xNewLib_reent);
        }
        #endif /* configUSE_NEWLIB_REENTRANT */

        xNextTaskUnblockTime = portMAX_DELAY; /* ��ʼ����һ������������ʱ�� */
        xSchedulerRunning = pdTRUE;           /* ���õ��������б�־ */
        xTickCount = (TickType_t)0U;          /* ��ʼ��ϵͳʱ�Ӽ����� */

        /* ��������configGENERATE_RUN_TIME_STATS������������ʱͳ�Ƽ�ʱ�� */
        portCONFIGURE_TIMER_FOR_RUN_TIME_STATS();

        /* ����Ӳ����صĶ�ʱ����ʼ���������˺������᷵�أ� */
        if (xPortStartScheduler() != pdFALSE)
        {
            /* ��������²�Ӧִ�����ˣ���Ϊ���������к������᷵�� */
        }
        else
        {
            /* ��������xTaskEndScheduler()ʱ�Ż�ִ������ */
        }
    }
    else
    {
        /* �ں�����ʧ�ܣ��޷������㹻�ڴ洴�����������ʱ������ */
        configASSERT(xReturn != errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY);
    }

    /* ��ֹ���������棨��INCLUDE_xTaskGetIdleTaskHandleΪ0ʱxIdleTaskHandleδ��ʹ�ã� */
    (void)xIdleTaskHandle;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�vTaskEndScheduler
 * ����������ֹͣFreeRTOS���������У��ָ�ԭʼ���жϷ������
 *           �˺���������ȫֹͣRTOS��������ͨ�����ڴ�RTOSģʽ�л������ģʽ
 * �����������
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - ֹͣ�������жϲ����ÿ���ֲ�ĵ�������������
 *   - �˿ڲ����ȷ���ж�ʹ��λ������ȷ״̬
 *   - �˲��������棬һ��ֹͣ��������RTOS���ܽ����ٿ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
void vTaskEndScheduler( void )
{
    /* ֹͣ�������жϲ����ÿ���ֲ�ĵ������������̣�
       �Ա��Ҫʱ���Իָ�ԭʼ���жϷ�����򡣶˿ڲ����ȷ���ж�ʹ��λ������ȷ״̬ */
    portDISABLE_INTERRUPTS();
    xSchedulerRunning = pdFALSE;
    vPortEndScheduler();
}
/*----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�vTaskSuspendAll
 * ������������������������ȣ����ӵ������������
 *           �˺���������ʱ����������ȣ�ȷ���ٽ������ԭ����
 * �����������
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - ����Ҫ�ٽ�������Ϊ����������BaseType_t
 *   - �������������Ƕ�ף���Ҫ��ͬ������vTaskResumeAll�������ָ�
 *   - �ڹ����ڼ䣬���񲻻��л������ж���Ȼ����ִ��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
void vTaskSuspendAll( void )
{
    /* ����Ҫ�ٽ�������Ϊ����������BaseType_t���ڽ��˱���Ϊ����֮ǰ��
       ���Ķ�Richard Barry��FreeRTOS֧����̳�еĻظ� -
       http://goo.gl/wu4acr */
    ++uxSchedulerSuspended;
}
/*----------------------------------------------------------*/

#if ( configUSE_TICKLESS_IDLE != 0 )

/*******************************************************************************
 * �������ƣ�prvGetExpectedIdleTime
 * ������������ȡԤ�ڵĿ���ʱ�䣬���ھ���ϵͳ���Խ���͹���״̬��ʱ�䳤��
 *           �˺���������һ������������֮ǰ��ʱ�䣬�����޿��еδ�ģʽ
 * �����������
 * �����������
 * �� �� ֵ��
 *   - TickType_t: Ԥ�ڵĿ���ʱ�䣨�δ������������Ӧ����͹���״̬�򷵻�0
 * ����˵����
 *   - �˺������������޿��еδ�ģʽ(configUSE_TICKLESS_IDLE != 0)ʱ����
 *   - ���Ƕ�����������Ƿ�Ӧ�ý���͹���״̬
 *   - ����ͬ���ã���ռʽ/Э��ʽ���ȣ��˿��Ż�����ѡ�񣩵����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
static TickType_t prvGetExpectedIdleTime( void )
{
    TickType_t xReturn;  /* ����ֵ��Ԥ�ڵĿ���ʱ�� */
    UBaseType_t uxHigherPriorityReadyTasks = pdFALSE;  /* ��־���Ƿ��и������ȼ��ľ������� */

    /* uxHigherPriorityReadyTasks����configUSE_PREEMPTIONΪ0�������
       ��˼�ʹ���������������У�Ҳ�����и��ڿ������ȼ��������ھ���״̬ */
    
    /* ���û��ʹ�ö˿��Ż�������ѡ�� */
    #if( configUSE_PORT_OPTIMISED_TASK_SELECTION == 0 )
    {
        /* ����Ƿ������ȼ����ڿ������ȼ������� */
        if( uxTopReadyPriority > tskIDLE_PRIORITY )
        {
            uxHigherPriorityReadyTasks = pdTRUE;
        }
    }
    #else
    {
        const UBaseType_t uxLeastSignificantBit = ( UBaseType_t ) 0x01;

        /* ��ʹ�ö˿��Ż�����ѡ��ʱ��uxTopReadyPriority��������λͼ��
           ��������˳������Чλ�����λ�����ʾ�����ȼ����ڿ������ȼ��������ھ���״̬��
           �⴦����ʹ��Э��ʽ����������� */
        if( uxTopReadyPriority > uxLeastSignificantBit )
        {
            uxHigherPriorityReadyTasks = pdTRUE;
        }
    }
    #endif

    /* ��鵱ǰ�������ȼ��Ƿ���ڿ������ȼ� */
    if( pxCurrentTCB->uxPriority > tskIDLE_PRIORITY )
    {
        /* ��ǰ�������ȼ����ڿ������ȼ�����Ӧ����͹���״̬ */
        xReturn = 0;
    }
    /* �������б��п������ȼ�����������Ƿ����1 */
    else if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ tskIDLE_PRIORITY ] ) ) > 1 )
    {
        /* ����״̬���������������ȼ��������ʹ��ʱ����Ƭ��
           ����봦����һ���δ��ж� */
        xReturn = 0;
    }
    /* ����Ƿ������ȼ����ڿ������ȼ��ľ������� */
    else if( uxHigherPriorityReadyTasks != pdFALSE )
    {
        /* �����ȼ����ڿ������ȼ��������ھ���״̬��
           ֻ����configUSE_PREEMPTIONΪ0ʱ���ܴﵽ��·�� */
        xReturn = 0;
    }
    else
    {
        /* ������һ������������ʱ���뵱ǰ�δ�����Ĳ�ֵ */
        xReturn = xNextTaskUnblockTime - xTickCount;
    }

    /* ����Ԥ�ڵĿ���ʱ�� */
    return xReturn;
}

#endif /* configUSE_TICKLESS_IDLE */
/*----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xTaskResumeAll
 * �����������ָ����������У������ڵ����������ڼ���۵Ĵ���������͵δ��¼�
 *           �˺�����vTaskSuspendAll����Ժ��������ڻָ�������ĵ�����
 * �����������
 * �����������
 * �� �� ֵ��
 *   - BaseType_t: ָʾ�Ƿ��Ѿ�ִ���������л�
 *     pdTRUE: �Ѿ�ִ���������л�
 *     pdFALSE: û��ִ�������л�
 * ����˵����
 *   - �˺��������ڵ����������ڼ���۵Ĵ���������͵δ��¼�
 *   - �������������ƶ����ʵ��ľ����б����������ĵδ��¼�
 *   - ������Ҫ���¼�����һ������������ʱ�䣬�ر��Ƕ��ڵ͹���ʵ��
 *   - �����Ҫ����ִ�������л�
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
BaseType_t xTaskResumeAll( void )
{
    TCB_t *pxTCB = NULL;              /* ָ��������ƿ��ָ�� */
    BaseType_t xAlreadyYielded = pdFALSE;  /* ����ֵ���Ƿ��Ѿ�ִ���������л� */

    /* ���uxSchedulerSuspendedΪ�㣬��˺�����֮ǰ��vTaskSuspendAll()�ĵ��ò�ƥ�� */
    configASSERT( uxSchedulerSuspended );

    /* �����ڵ����������ڼ䣬ISR����������¼��б����Ƴ���
       �����������������Ƴ������񽫱���ӵ�xPendingReadyList��
       һ���������ָ����Ϳ��԰�ȫ�ؽ����д���������Ӵ��б��ƶ����ʵ��ľ����б� */
    taskENTER_CRITICAL();
    {
        /* ���ٵ������������ */
        --uxSchedulerSuspended;

        /* ����������������Ƿ�����㣨��ȫ�ָ��� */
        if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
        {
            /* ���ϵͳ���Ƿ�������� */
            if( uxCurrentNumberOfTasks > ( UBaseType_t ) 0U )
            {
                /* ���������б��е��κξ��������ƶ����ʵ��ľ����б� */
                while( listLIST_IS_EMPTY( &xPendingReadyList ) == pdFALSE )
                {
                    /* ��ȡ�������б��еĵ�һ������ */
                    pxTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( ( &xPendingReadyList ) );
                    
                    /* ���¼��б��״̬�б����Ƴ����� */
                    ( void ) uxListRemove( &( pxTCB->xEventListItem ) );
                    ( void ) uxListRemove( &( pxTCB->xStateListItem ) );
                    
                    /* ��������ӵ������б� */
                    prvAddTaskToReadyList( pxTCB );

                    /* ����ƶ�����������ȼ����ڻ���ڵ�ǰ���������ִ��yield */
                    if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
                    {
                        xYieldPending = pdTRUE;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }

                /* ����������ƶ�����pxTCB��ΪNULL�� */
                if( pxTCB != NULL )
                {
                    /* �ڵ����������ڼ������񱻽���������������ֹ����һ���������ʱ������¼��㣬
                       ������������������¼���������Ҫ�Ե͹����޿��еδ�ʵ�ֺ���Ҫ��
                       ����Ա��ⲻ��Ҫ�ĵ͹���״̬�˳� */
                    prvResetNextTaskUnblockTime();
                }

                /* ����ڵ����������ڼ䷢�����κεδ�����Ӧ�ô������ǡ�
                   ��ȷ���δ�������Ử���������κ��ӳٵ���������ȷ��ʱ��ָ� */
                {
                    UBaseType_t uxPendedCounts = uxPendedTicks; /* ����ʧ�Ը��� */

                    if( uxPendedCounts > ( UBaseType_t ) 0U )
                    {
                        /* �������й���ĵδ� */
                        do
                        {
                            /* ���ӵδ����������Ƿ���Ҫ�����л� */
                            if( xTaskIncrementTick() != pdFALSE )
                            {
                                xYieldPending = pdTRUE;
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();
                            }
                            --uxPendedCounts;
                        } while( uxPendedCounts > ( UBaseType_t ) 0U );

                        /* ���ù���ĵδ���� */
                        uxPendedTicks = 0;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }

                /* ����Ƿ���yield���� */
                if( xYieldPending != pdFALSE )
                {
                    #if( configUSE_PREEMPTION != 0 )
                    {
                        /* ���ʹ����ռʽ���ȣ�����Ѿ�ִ����yield */
                        xAlreadyYielded = pdTRUE;
                    }
                    #endif
                    
                    /* ���ʹ����ռʽ���ȣ�ִ�������л� */
                    taskYIELD_IF_USING_PREEMPTION();
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }
    taskEXIT_CRITICAL();

    /* �����Ƿ��Ѿ�ִ���������л� */
    return xAlreadyYielded;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xTaskGetTickCount
 * ������������ȡ��ǰϵͳ�δ��������ֵ����ʾϵͳ�����󾭹���ʱ�ӽ�����
 *           �˺������ڻ�ȡϵͳ�ĵ�ǰʱ�䣬����ʱ������ͳ�ʱ����
 * �����������
 * �����������
 * �� �� ֵ��
 *   - TickType_t: ��ǰ��ϵͳ�δ����ֵ
 * ����˵����
 *   - ʹ���ٽ��������δ�������Ķ�ȡ��ȷ����16λ�������ϵ�ԭ����
 *   - �δ��������ϵͳ������ʼ��������ʱ�����
 *   - �����ڼ���ʱ������ʵ�ֳ�ʱ����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
TickType_t xTaskGetTickCount( void )
{
    TickType_t xTicks;  /* �洢�δ����ֵ�ı��� */

    /* �����16λ�����������У���Ҫ�ٽ������� */
    portTICK_TYPE_ENTER_CRITICAL();
    {
        /* ��ȡ��ǰ�ĵδ����ֵ */
        xTicks = xTickCount;
    }
    portTICK_TYPE_EXIT_CRITICAL();

    /* ���صδ����ֵ */
    return xTicks;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xTaskGetTickCountFromISR
 * �������������жϷ�������л�ȡ��ǰϵͳ�δ��������ֵ
 *           �˺�����xTaskGetTickCount���жϰ�ȫ�汾������ISR�е���
 * �����������
 * �����������
 * �� �� ֵ��
 *   - TickType_t: ��ǰ��ϵͳ�δ����ֵ
 * ����˵����
 *   - �˺������жϰ�ȫ�汾�������жϷ�������е���
 *   - ��֤�ж����ȼ���ȷ������ӹ������ȼ����жϵ���
 *   - ʹ���жϰ�ȫ���ٽ��������δ�������Ķ�ȡ
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
TickType_t xTaskGetTickCountFromISR( void )
{
    TickType_t xReturn;               /* ����ֵ���δ����ֵ */
    UBaseType_t uxSavedInterruptStatus;  /* ������ж�״̬ */

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

    /* �����ж�״̬�������ж� */
    uxSavedInterruptStatus = portTICK_TYPE_SET_INTERRUPT_MASK_FROM_ISR();
    {
        /* ��ȡ��ǰ�ĵδ����ֵ */
        xReturn = xTickCount;
    }
    /* �ָ��ж�״̬ */
    portTICK_TYPE_CLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

    /* ���صδ����ֵ */
    return xReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�uxTaskGetNumberOfTasks
 * ������������ȡϵͳ�е�ǰ���ڵ���������
 *           �˺�������ϵͳ����������������������������������������ɾ����δ���������
 * �����������
 * �����������
 * �� �� ֵ��
 *   - UBaseType_t: ϵͳ�е�ǰ���ڵ���������
 * ����˵����
 *   - ����Ҫ�ٽ�����������Ϊ����������BaseType_t
 *   - ���ص�ֵ��������״̬�����񣨾�����������������ɾ����δ����
 *   - ������ϵͳ��غ���Դ����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
UBaseType_t uxTaskGetNumberOfTasks( void )
{
    /* ����Ҫ�ٽ�������Ϊ����������BaseType_t */
    return uxCurrentNumberOfTasks;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�pcTaskGetName
 * ������������ȡָ������������ַ���
 *           �˺�������ָ�����������ַ�����ָ�룬�����ڱ�ʶ����ʾ������Ϣ
 * ���������
 *   - xTaskToQuery: Ҫ��ѯ��������
 *     ����ֵ������NULL��ʾ��ѯ��ǰ���������
 * �����������
 * �� �� ֵ��
 *   - char*: ָ�����������ַ�����ָ��
 * ����˵����
 *   - ���������ڴ�������ʱ���ã���󳤶�ΪconfigMAX_TASK_NAME_LEN
 *   - ���ص�ָ��ָ��������ƿ��е������ַ�������Ӧ���޸�
 *   - �����ڵ��ԡ���־��¼�������ʶ
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
char *pcTaskGetName( TaskHandle_t xTaskToQuery ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
    TCB_t *pxTCB;  /* ָ��������ƿ��ָ�� */

    /* �������null�����ѯ������������� */
    pxTCB = prvGetTCBFromHandle( xTaskToQuery );
    configASSERT( pxTCB );
    
    /* �������������ַ�����ָ�� */
    return &( pxTCB->pcTaskName[ 0 ] );
}
/*-----------------------------------------------------------*/

#if ( INCLUDE_xTaskGetHandle == 1 )

/*******************************************************************************
 * �������ƣ�prvSearchForNameWithinSingleList
 * �����������ڵ��������б�������ָ�����Ƶ�������ƿ�
 *           �˺�����xTaskGetHandle���ڲ�ʵ�֣��������ض��б��а�������������
 * ���������
 *   - pxList: Ҫ�����������б�ָ��
 *   - pcNameToQuery: Ҫ��ѯ�����������ַ���
 * �����������
 * �� �� ֵ��
 *   - TCB_t*: �ҵ���������ƿ�ָ�룬���δ�ҵ��򷵻�NULL
 * ����˵����
 *   - �˺����ڵ��������������µ��ã�ȷ���������̵�ԭ����
 *   - ʹ���ַ��Ƚ����ַ�ƥ����������
 *   - ֧���������ƵĲ���ƥ�������ƥ��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
static TCB_t *prvSearchForNameWithinSingleList( List_t *pxList, const char pcNameToQuery[] )
{
    TCB_t *pxNextTCB;        /* ָ����һ��������ƿ��ָ�� */
    TCB_t *pxFirstTCB;       /* ָ���б��һ��������ƿ��ָ�루���ڼ��ѭ�������� */
    TCB_t *pxReturn = NULL;  /* ����ֵ���ҵ���������ƿ�ָ�� */
    UBaseType_t x;           /* ѭ���������������ַ��Ƚ� */
    char cNextChar;          /* ��ǰ�Ƚϵ��ַ� */

    /* �˺����ڵ��������������µ��� */

    /* ����б��Ƿ���������б��ȴ���0�� */
    if( listCURRENT_LIST_LENGTH( pxList ) > ( UBaseType_t ) 0 )
    {
        /* ��ȡ�б��еĵ�һ��������ƿ飬��Ϊѭ�������ı�� */
        listGET_OWNER_OF_NEXT_ENTRY( pxFirstTCB, pxList );

        /* �����б��е�ÿ����������ָ�����Ƶ����� */
        do
        {
            /* ��ȡ�б��е���һ��������ƿ� */
            listGET_OWNER_OF_NEXT_ENTRY( pxNextTCB, pxList );

            /* ��������е�ÿ���ַ���Ѱ��ƥ���ƥ�� */
            for( x = ( UBaseType_t ) 0; x < ( UBaseType_t ) configMAX_TASK_NAME_LEN; x++ )
            {
                /* ��ȡ���������е���һ���ַ� */
                cNextChar = pxNextTCB->pcTaskName[ x ];

                /* ����ַ��Ƿ�ƥ�� */
                if( cNextChar != pcNameToQuery[ x ] )
                {
                    /* �ַ���ƥ�䣬�����ַ��Ƚ�ѭ�� */
                    break;
                }
                /* ����Ƿ񵽴��ַ�����β */
                else if( cNextChar == 0x00 )
                {
                    /* �����ַ�������ֹ��һ���ҵ���ƥ�� */
                    pxReturn = pxNextTCB;
                    break;
                }
                else
                {
                    /* �ַ�ƥ�䵫δ���ַ�����β����Ӳ��Ը����ʱ�� */
                    mtCOVERAGE_TEST_MARKER();
                }
            }

            /* ����ҵ���ƥ�����������ѭ�� */
            if( pxReturn != NULL )
            {
                /* ���ҵ������� */
                break;
            }

        } while( pxNextTCB != pxFirstTCB );  /* ѭ��ֱ���ص���һ������ */
    }
    else
    {
        /* �б�Ϊ�գ���Ӳ��Ը����ʱ�� */
        mtCOVERAGE_TEST_MARKER();
    }

    /* �����ҵ���������ƿ�ָ�루���δ�ҵ��򷵻�NULL�� */
    return pxReturn;
}

#endif /* INCLUDE_xTaskGetHandle */
/*-----------------------------------------------------------*/

#if ( INCLUDE_xTaskGetHandle == 1 )

/*******************************************************************************
 * �������ƣ�xTaskGetHandle
 * ���������������������Ʋ�ѯ������������ͨ���������ƻ�ȡ����Ŀ��ƾ��
 *           �˺�����ϵͳ������ָ�����Ƶ����񣬲�������������
 * ���������
 *   - pcNameToQuery: Ҫ��ѯ�����������ַ���
 * �����������
 * �� �� ֵ��
 *   - TaskHandle_t: �ҵ��������������δ�ҵ��򷵻�NULL
 * ����˵����
 *   - �˺����������û�ȡ����������(INCLUDE_xTaskGetHandle == 1)ʱ����
 *   - �������ƽ����ض�ΪconfigMAX_TASK_NAME_LEN - 1�ֽ�
 *   - ��Ҫ�ڹ�����������������ִ�У���ȷ�����������е�����һ����
 *   - �����п��ܵ�״̬�б����������񣨾������ӳ١�������ɾ����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
TaskHandle_t xTaskGetHandle( const char *pcNameToQuery ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
    UBaseType_t uxQueue = configMAX_PRIORITIES;  /* ѭ�������������ڱ������ȼ����� */
    TCB_t* pxTCB = NULL;                         /* ָ���ҵ���������ƿ��ָ�� */

    /* �������ƽ����ض�ΪconfigMAX_TASK_NAME_LEN - 1�ֽ� */
    configASSERT( strlen( pcNameToQuery ) < configMAX_TASK_NAME_LEN );

    /* ������������ȷ������������������״̬����ı� */
    vTaskSuspendAll();
    {
        /* ���������б� */
        do
        {
            uxQueue--;  /* �ݼ����ȼ������� */
            
            /* �ڵ�ǰ���ȼ�����������ָ�����Ƶ����� */
            pxTCB = prvSearchForNameWithinSingleList( ( List_t * ) &( pxReadyTasksLists[ uxQueue ] ), pcNameToQuery );

            if( pxTCB != NULL )
            {
                /* �ҵ���������������ѭ�� */
                break;
            }

        } while( uxQueue > ( UBaseType_t ) tskIDLE_PRIORITY ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */

        /* �����ӳ��б� */
        if( pxTCB == NULL )
        {
            pxTCB = prvSearchForNameWithinSingleList( ( List_t * ) pxDelayedTaskList, pcNameToQuery );
        }

        if( pxTCB == NULL )
        {
            pxTCB = prvSearchForNameWithinSingleList( ( List_t * ) pxOverflowDelayedTaskList, pcNameToQuery );
        }

        /* ������������������ */
        #if ( INCLUDE_vTaskSuspend == 1 )
        {
            if( pxTCB == NULL )
            {
                /* ���������б� */
                pxTCB = prvSearchForNameWithinSingleList( &xSuspendedTaskList, pcNameToQuery );
            }
        }
        #endif

        /* �������������ɾ������ */
        #if( INCLUDE_vTaskDelete == 1 )
        {
            if( pxTCB == NULL )
            {
                /* ������ɾ�������б� */
                pxTCB = prvSearchForNameWithinSingleList( &xTasksWaitingTermination, pcNameToQuery );
            }
        }
        #endif
    }
    /* �ָ��������� */
    ( void ) xTaskResumeAll();

    /* �����ҵ��������������δ�ҵ��򷵻�NULL�� */
    return ( TaskHandle_t ) pxTCB;
}

#endif /* INCLUDE_xTaskGetHandle */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

/*******************************************************************************
 * �������ƣ�uxTaskGetSystemState
 * ������������ȡϵͳ�����������״̬��Ϣ�������������������������ɾ��������
 *           �˺����ṩϵͳ���������գ����ڼ�ء����Ժ����ܷ���
 * ���������
 *   - pxTaskStatusArray: ָ��TaskStatus_t�����ָ�룬���ڴ洢����״̬��Ϣ
 *   - uxArraySize: �����С���������ٵ��ڵ�ǰ��������
 *   - pulTotalRunTime: ָ��������ʱ���ָ�룬���ڴ洢ϵͳ������ʱ�䣨�����������ʱͳ�ƣ�
 * ���������
 *   - pxTaskStatusArray: ����������״̬����
 *   - pulTotalRunTime: �����õ�������ʱ��ֵ
 * �� �� ֵ��
 *   - UBaseType_t: �ɹ���������״̬�ṹ����
 * ����˵����
 *   - �˺����������ø��ٹ���(configUSE_TRACE_FACILITY == 1)ʱ����
 *   - ��Ҫ�ڹ�����������������ִ�У���ȷ������һ����
 *   - �ṩϵͳ���������գ���������״̬��������Ϣ
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
UBaseType_t uxTaskGetSystemState( TaskStatus_t * const pxTaskStatusArray, const UBaseType_t uxArraySize, uint32_t * const pulTotalRunTime )
{
    UBaseType_t uxTask = 0;      /* ���������Ѵ������������ */
    UBaseType_t uxQueue = configMAX_PRIORITIES;  /* ѭ�������������ڱ������ȼ����� */

    /* ������������ȷ���ڻ�ȡϵͳ״̬ʱ����״̬����ı� */
    vTaskSuspendAll();
    {
        /* ��������С�Ƿ��㹻����ϵͳ�е��������� */
        if( uxArraySize >= uxCurrentNumberOfTasks )
        {
            /* ���TaskStatus_t�ṹ����������״̬��ÿ���������Ϣ */
            do
            {
                uxQueue--;  /* �ݼ����ȼ������� */
                
                /* ����ǰ���ȼ������е�������ӵ�����״̬���� */
                uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), 
                                                       &( pxReadyTasksLists[ uxQueue ] ), 
                                                       eReady );

            } while( uxQueue > ( UBaseType_t ) tskIDLE_PRIORITY ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */

            /* ���TaskStatus_t�ṹ����������״̬��ÿ���������Ϣ */
            uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), 
                                                   ( List_t * ) pxDelayedTaskList, 
                                                   eBlocked );
            
            /* ��������ӳ������б��е��������� */
            uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), 
                                                   ( List_t * ) pxOverflowDelayedTaskList, 
                                                   eBlocked );

            /* �������������ɾ������ */
            #if( INCLUDE_vTaskDelete == 1 )
            {
                /* ���TaskStatus_t�ṹ��������ɾ������δ�����ÿ���������Ϣ */
                uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), 
                                                       &xTasksWaitingTermination, 
                                                       eDeleted );
            }
            #endif

            /* ������������������ */
            #if ( INCLUDE_vTaskSuspend == 1 )
            {
                /* ���TaskStatus_t�ṹ����������״̬��ÿ���������Ϣ */
                uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), 
                                                       &xSuspendedTaskList, 
                                                       eSuspended );
            }
            #endif

            /* �������������ʱͳ�ƹ��� */
            #if ( configGENERATE_RUN_TIME_STATS == 1)
            {
                if( pulTotalRunTime != NULL )
                {
                    /* ��ȡ������ʱ�������ֵ */
                    #ifdef portALT_GET_RUN_TIME_COUNTER_VALUE
                        portALT_GET_RUN_TIME_COUNTER_VALUE( ( *pulTotalRunTime ) );
                    #else
                        *pulTotalRunTime = portGET_RUN_TIME_COUNTER_VALUE();
                    #endif
                }
            }
            #else
            {
                if( pulTotalRunTime != NULL )
                {
                    /* δ��������ʱͳ�ƣ���������ʱ����Ϊ0 */
                    *pulTotalRunTime = 0;
                }
            }
            #endif
        }
        else
        {
            /* �����С���㣺��Ӳ��Ը����ʱ�� */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    /* �ָ��������� */
    ( void ) xTaskResumeAll();

    /* ���سɹ���������״̬�ṹ���� */
    return uxTask;
}

#endif /* configUSE_TRACE_FACILITY */
/*----------------------------------------------------------*/

#if ( INCLUDE_xTaskGetIdleTaskHandle == 1 )

/*******************************************************************************
 * �������ƣ�xTaskGetIdleTaskHandle
 * ������������ȡ��������ľ��������ֱ�ӷ���FreeRTOS�Ŀ�������
 *           �˺����ṩ��ϵͳ�Զ������Ŀ�������ķ�������
 * �����������
 * �����������
 * �� �� ֵ��
 *   - TaskHandle_t: ��������ľ�������������δ���������ΪNULL
 * ����˵����
 *   - �˺����������û�ȡ��������������(INCLUDE_xTaskGetIdleTaskHandle == 1)ʱ����
 *   - ʹ�ö���ȷ���ڵ���ʱ������������ΪNULL����������������
 *   - ��Ҫ���ڸ߼����Ժͼ�س�������ͨӦ�ó���ͨ������Ҫֱ�ӷ��ʿ�������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
TaskHandle_t xTaskGetIdleTaskHandle( void )
{
    /* ����ڵ���������֮ǰ����xTaskGetIdleTaskHandle()����xIdleTaskHandle��ΪNULL */
    configASSERT( ( xIdleTaskHandle != NULL ) );
    
    /* ���ؿ�������ľ�� */
    return xIdleTaskHandle;
}

#endif /* INCLUDE_xTaskGetIdleTaskHandle */
/*----------------------------------------------------------*/

#if ( configUSE_TICKLESS_IDLE != 0 )

/*******************************************************************************
 * �������ƣ�vTaskStepTick
 * ������������ tick ������һ��ʱ���У�� tick ����ֵ
 *           �˺��������޿��еδ�ģʽ����ϵͳ�ӵ͹���˯�߻��Ѻ�����δ������
 * ���������
 *   - xTicksToJump: Ҫ������ tick ��������ϵͳ���ڵ͹���״̬�� tick ������
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺������������޿��еδ�ģʽ(configUSE_TICKLESS_IDLE != 0)ʱ����
 *   - ����Ϊÿ�������� tick ���� tick ���Ӻ���
 *   - ʹ�ö���ȷ�������� tick �������ᵼ�� tick ����������һ������������ʱ��
 *   - ���� tick ���������Ӹ��ټ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
void vTaskStepTick( const TickType_t xTicksToJump )
{
    /* �� tick ������һ��ʱ���У�� tick ����ֵ��
       ע�⣺�ⲻ��Ϊÿ�������� tick ���� tick ���Ӻ��� */
    configASSERT( ( xTickCount + xTicksToJump ) <= xNextTaskUnblockTime );
    
    /* ���� tick ����ֵ������ָ���� tick ���� */
    xTickCount += xTicksToJump;
    
    /* ���� tick ���������ӣ����ڵ��Ժ����ܷ��� */
    traceINCREASE_TICK_COUNT( xTicksToJump );
}

#endif /* configUSE_TICKLESS_IDLE */
/*----------------------------------------------------------*/

#if ( INCLUDE_xTaskAbortDelay == 1 )

/*******************************************************************************
 * �������ƣ�xTaskAbortDelay
 * ������������ָֹ��������ӳ�״̬����ǰ���������״̬���
 *           �˺�������ǿ�ƽ����������״̬���ѣ�������Ҫ�ȴ���ʱ���¼�����
 * ���������
 *   - xTask: ��������ָ��Ҫ��ֹ�ӳٵ�����
 * �����������
 * �� �� ֵ��
 *   - BaseType_t: �������
 *     pdTRUE: �ɹ���ֹ������ӳ�״̬
 *     pdFALSE: ����������״̬���޷���ֹ�ӳ�
 * ����˵����
 *   - �˺����������������ӳ���ֹ����(INCLUDE_xTaskAbortDelay == 1)ʱ����
 *   - ʹ�õ�����������ٽ�������ȷ��������ԭ����
 *   - ��������������б���¼��б��е��Ƴ�
 *   - ������ռʽ�����µ��������л�����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
BaseType_t xTaskAbortDelay( TaskHandle_t xTask )
{
    TCB_t *pxTCB = ( TCB_t * ) xTask;  /* ָ��������ƿ��ָ�� */
    BaseType_t xReturn = pdFALSE;      /* ����ֵ��������� */

    /* ���Լ��������ƿ�ָ����Ч�� */
    configASSERT( pxTCB );

    /* ������������ȷ��������ԭ���� */
    vTaskSuspendAll();
    {
        /* ����ֻ��������������״̬ʱ���ܱ���ǰ������״̬�Ƴ� */
        if( eTaskGetState( xTask ) == eBlocked )
        {
            /* �������б����Ƴ�����������á��жϲ��ᴥ��xStateListItem��
               ��Ϊ�������ѹ��� */
            ( void ) uxListRemove( &( pxTCB->xStateListItem ) );

            /* �����Ƿ�Ҳ�ڵȴ��¼�������ǣ�Ҳ���¼��б����Ƴ���
               ��ʹ�������ѹ����ж�Ҳ���ܴ����¼��б��
               ���ʹ���ٽ��� */
            taskENTER_CRITICAL();
            {
                if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
                {
                    /* ���¼��б����Ƴ�������¼��б��� */
                    ( void ) uxListRemove( &( pxTCB->xEventListItem ) );
                    /* �����ӳ���ֹ��־ */
                    pxTCB->ucDelayAborted = pdTRUE;
                }
                else
                {
                    /* �������¼��б��У���Ӳ��Ը����ʱ�� */
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            taskEXIT_CRITICAL();

            /* ���ѽ����������������ʵ��ľ����б� */
            prvAddTaskToReadyList( pxTCB );

            /* �����ռ�رգ���������������񲻻ᵼ�������������л� */
            #if (  configUSE_PREEMPTION == 1 )
            {
                /* ��ռ��������ֻ���ڱ������������������ȼ�
                   ���ڻ���ڵ�ǰ����ִ�е�����ʱ��Ӧִ���������л� */
                if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
                {
                    /* ����yield�������ڵ������ָ�ʱִ�� */
                    xYieldPending = pdTRUE;
                }
                else
                {
                    /* ���ȼ������ڵ�ǰ������Ӳ��Ը����ʱ�� */
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            #endif /* configUSE_PREEMPTION */

            /* �����ɹ������÷���ֵΪpdTRUE */
            xReturn = pdTRUE;
        }
        else
        {
            /* ����������״̬����Ӳ��Ը����ʱ�� */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    /* �ָ��������� */
    xTaskResumeAll();

    /* ���ز������ */
    return xReturn;
}

#endif /* INCLUDE_xTaskAbortDelay */
/*----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xTaskIncrementTick
 * ��������������ϵͳʱ�ӽ��ļ�����������Ƿ���������Ҫ���������������ʱ��Ƭ���ȡ�
 *           �ú�������ֲ����ÿ��ʱ�ӽ����ж�ʱ���ã���FreeRTOS�������ĺ��ĺ���֮һ��
 * �����������
 * �����������
 * �� �� ֵ��BaseType_t 
 *           - pdTRUE: ��Ҫ�����������л�
 *           - pdFALSE: ����Ҫ�����������л�
 * ����˵����
 *   - ������������ʱ(uxSchedulerSuspended != pdFALSE)�����ļ����ᱻ�ݴ浽uxPendedTicks
 *   - �ᴦ���ӳ������б������ӳ��б���л�
 *   - ���鲢����������ڵ�����
 *   - �ᴥ��Tick���Ӻ���
 *   - ֧��ʱ��Ƭ��ת���Ⱥ���ռʽ����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
BaseType_t xTaskIncrementTick( void )
{
    TCB_t * pxTCB;                          /* ָ��������ƿ��ָ�� */
    TickType_t xItemValue;                  /* �洢�б���ֵ������������ʱ�䣩 */
    BaseType_t xSwitchRequired = pdFALSE;   /* �Ƿ���Ҫ�������л��ı�־����ʼ��ΪpdFALSE */

    /* ÿ�η��������ж�ʱ����ֲ����á�
       �������ļ�������Ȼ�����µĽ���ֵ�Ƿ�ᵼ���κ������������� */
    traceTASK_INCREMENT_TICK( xTickCount ); /* ���ٵ��ԣ���¼���ĵ����¼� */

    /* ���������Ƿ�δ������ */
    if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
    {
        /* С�Ż����ڴ˴�����ڽ��ļ�������ı䣬ʹ�ó�����ߴ���Ч�� */
        const TickType_t xConstTickCount = xTickCount + 1;

        /* ����RTOS���ļ����������������������㣬���л��ӳٺ�����ӳ��б� */
        xTickCount = xConstTickCount;       /* ����ȫ�ֽ��ļ����� */

        /* �����ļ������Ƿ���������㣩 */
        if( xConstTickCount == ( TickType_t ) 0U )
        {
            taskSWITCH_DELAYED_LISTS();     /* �л��ӳ��б������ӳ��б� */
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();       /* ���ڲ��Ը����ʣ���ʵ�ʹ��� */
        }

        /* ��鵱ǰ�������Ƿ�ﵽ��һ������������ʱ�� */
        if( xConstTickCount >= xNextTaskUnblockTime )
        {
            /* ѭ������������Ҫ������������� */
            for( ;; )
            {
                /* ����ӳ������б��Ƿ�Ϊ�� */
                if( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )
                {
                    /* �ӳ��б�Ϊ�գ���xNextTaskUnblockTime����Ϊ������ֵ��
                       ʹ���´κ���ͨ��xTickCount >= xNextTaskUnblockTime��� */
                    xNextTaskUnblockTime = portMAX_DELAY; /*lint !e961 ����MISRA���� */
                    break;                                /* �˳�ѭ�� */
                }
                else
                {
                    /* �ӳ��б�Ϊ�գ���ȡ�б������������ƿ���б���ֵ */
                    pxTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList );
                    xItemValue = listGET_LIST_ITEM_VALUE( &( pxTCB->xStateListItem ) );

                    /* ��鵱ǰ�������Ƿ�С������Ľ������ʱ�� */
                    if( xConstTickCount < xItemValue )
                    {
                        /* ��δ��������������ʱ�䣬����Ҫ������һ���������ʱ��Ϊ�����������ʱ�� */
                        xNextTaskUnblockTime = xItemValue;
                        break;                            /* �˳�ѭ�� */
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();         /* ���Ը����ʱ�� */
                    }

                    /* ������״̬�Ƴ����񣺽�����������б��Ƴ� */
                    ( void ) uxListRemove( &( pxTCB->xStateListItem ) );

                    /* ��������Ƿ�ͬʱ���¼��б��еȴ������ź��������еȣ� */
                    if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
                    {
                        /* ���¼��б����Ƴ������� */
                        ( void ) uxListRemove( &( pxTCB->xEventListItem ) );
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();         /* ���Ը����ʱ�� */
                    }

                    /* ���ѽ��������������ӵ������б� */
                    prvAddTaskToReadyList( pxTCB );

                    /* ���������ռʽ���ȣ�����Ƿ���Ҫ�������л� */
                    #if (  configUSE_PREEMPTION == 1 )
                    {
                        /* ֻ�е�����������������ȼ����ڵ��ڵ�ǰ����ʱ����Ҫ�л� */
                        if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
                        {
                            xSwitchRequired = pdTRUE;     /* �����л���־ */
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();     /* ���Ը����ʱ�� */
                        }
                    }
                    #endif /* configUSE_PREEMPTION */
                }
            }
        }

        /* ���������ռʽ���Ⱥ�ʱ��Ƭ��ת�����ͬ���ȼ������Ƿ���Ҫʱ��Ƭ�л� */
        #if ( ( configUSE_PREEMPTION == 1 ) && ( configUSE_TIME_SLICING == 1 ) )
        {
            /* ��鵱ǰ���ȼ������б��е����������Ƿ����1 */
            if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ pxCurrentTCB->uxPriority ] ) ) > ( UBaseType_t ) 1 )
            {
                xSwitchRequired = pdTRUE;                 /* �����л���־ */
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();                 /* ���Ը����ʱ�� */
            }
        }
        #endif /* ( ( configUSE_PREEMPTION == 1 ) && ( configUSE_TIME_SLICING == 1 ) ) */

        /* �������Tick���Ӻ�����ִ��Ӧ�ó�����Ĺ��Ӻ��� */
        #if ( configUSE_TICK_HOOK == 1 )
        {
            /* �ڵ������������������Ľ��ģ�ʱ�����ù��Ӻ��� */
            if( uxPendedTicks == ( UBaseType_t ) 0U )
            {
                vApplicationTickHook();                   /* ����Ӧ�ù��Ӻ��� */
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();                 /* ���Ը����ʱ�� */
            }
        }
        #endif /* configUSE_TICK_HOOK */
    }
    else
    {
        /* ����������ʱ�����ӹ���Ľ��ļ��� */
        ++uxPendedTicks;

        /* ��ʹ������������Tick���Ӻ���Ҳ�ᶨ�ڵ��� */
        #if ( configUSE_TICK_HOOK == 1 )
        {
            vApplicationTickHook();                       /* ����Ӧ�ù��Ӻ��� */
        }
        #endif
    }

    /* ���������ռʽ���ȣ�����Ƿ����ӳٵ��л����� */
    #if ( configUSE_PREEMPTION == 1 )
    {
        if( xYieldPending != pdFALSE )
        {
            xSwitchRequired = pdTRUE;                     /* �����л���־ */
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();                     /* ���Ը����ʱ�� */
        }
    }
    #endif /* configUSE_PREEMPTION */

    return xSwitchRequired;                               /* �����Ƿ���Ҫ�������л� */
}
/*-----------------------------------------------------------*/

#if ( configUSE_APPLICATION_TASK_TAG == 1 )

/*******************************************************************************
 * �������ƣ�vTaskSetApplicationTaskTag
 * ��������������ָ�������Ӧ�ó��������ǩ�����Ӻ�����
 *           �˺�������Ϊ��������Ӧ�ó�����Ĺ��Ӻ���
 * ���������
 *   - xTask: ��������ָ��Ҫ���ù��Ӻ���������
 *     ����ֵ������NULL��ʾ���õ�ǰ����Ĺ��Ӻ���
 *   - pxHookFunction: Ҫ���õĹ��Ӻ���ָ��
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺�����������Ӧ�ó��������ǩ����(configUSE_APPLICATION_TASK_TAG == 1)ʱ����
 *   - ʹ���ٽ����������Ӻ��������ã���Ϊ��ֵ���ܱ��жϷ���
 *   - �ṩ��һ��Ϊ���������Զ��平�Ӻ����Ļ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
void vTaskSetApplicationTaskTag( TaskHandle_t xTask, TaskHookFunction_t pxHookFunction )
{
    TCB_t *xTCB;  /* ָ��������ƿ��ָ�� */

    /* ���xTaskΪNULL�������õ��ǵ�������Ĺ��Ӻ��� */
    if( xTask == NULL )
    {
        xTCB = ( TCB_t * ) pxCurrentTCB;
    }
    else
    {
        xTCB = ( TCB_t * ) xTask;
    }

    /* ����TCB�еĹ��Ӻ�������Ҫ�ٽ�������Ϊ��ֵ���ܱ��жϷ��� */
    taskENTER_CRITICAL();
    {
        /* �����Ӻ���ָ�뱣�浽������ƿ��� */
        xTCB->pxTaskTag = pxHookFunction;
    }
    taskEXIT_CRITICAL();
}
#endif
/*-----------------------------------------------------------*/

#if ( configUSE_APPLICATION_TASK_TAG == 1 )

/*******************************************************************************
 * �������ƣ�xTaskGetApplicationTaskTag
 * ������������ȡָ�������Ӧ�ó��������ǩ�����Ӻ�����
 *           �˺������ڼ��������������Ӧ�ó�����Ĺ��Ӻ���
 * ���������
 *   - xTask: ��������ָ��Ҫ��ȡ���Ӻ���������
 *     ����ֵ������NULL��ʾ��ȡ��ǰ����Ĺ��Ӻ���
 * �����������
 * �� �� ֵ��
 *   - TaskHookFunction_t: �����Ӧ�ó����Ӻ���ָ�룬���δ�����򷵻�NULL
 * ����˵����
 *   - �˺�����������Ӧ�ó��������ǩ����(configUSE_APPLICATION_TASK_TAG == 1)ʱ����
 *   - ʹ���ٽ����������Ӻ����ķ��ʣ���Ϊ��ֵ���ܱ��жϷ���
 *   - �ṩ��һ�ּ��������ض����Ӻ����Ļ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
TaskHookFunction_t xTaskGetApplicationTaskTag( TaskHandle_t xTask )
{
    TCB_t *xTCB;                /* ָ��������ƿ��ָ�� */
    TaskHookFunction_t xReturn;  /* ����ֵ�����Ӻ���ָ�� */

    /* ���xTaskΪNULL�����ȡ��ǰ����Ĺ��Ӻ��� */
    if( xTask == NULL )
    {
        xTCB = ( TCB_t * ) pxCurrentTCB;
    }
    else
    {
        xTCB = ( TCB_t * ) xTask;
    }

    /* ����TCB�еĹ��Ӻ�������Ҫ�ٽ�������Ϊ��ֵ���ܱ��жϷ��� */
    taskENTER_CRITICAL();
    {
        /* ��������ƿ��л�ȡ���Ӻ���ָ�� */
        xReturn = xTCB->pxTaskTag;
    }
    taskEXIT_CRITICAL();

    /* ���ع��Ӻ���ָ�� */
    return xReturn;
}

#endif /* configUSE_APPLICATION_TASK_TAG */
/*-----------------------------------------------------------*/

#if ( configUSE_APPLICATION_TASK_TAG == 1 )

/*******************************************************************************
 * �������ƣ�xTaskCallApplicationTaskHook
 * ��������������Ӧ�ó��������Ӻ���������Ӧ�ó���Ϊ�ض�����ע���Զ���ص�����
 *           �˺����ṩ��һ�ֻ��ƣ�����Ӧ�ó���Ϊ����ע���Զ���Ļص�����
 * ���������
 *   - xTask: ��������ָ��Ҫ���ù��Ӻ���������
 *     ����ֵ������NULL��ʾ���õ�ǰ����Ĺ��Ӻ���
 *   - pvParameter: ���ݸ����Ӻ����Ĳ���
 * �����������
 * �� �� ֵ��
 *   - BaseType_t: ���Ӻ����ķ���ֵ�����û�й��Ӻ����򷵻�pdFAIL
 * ����˵����
 *   - �˺�����������Ӧ�ó��������ǩ����(configUSE_APPLICATION_TASK_TAG == 1)ʱ����
 *   - �ṩ��һ����չ���ƣ�����Ӧ�ó���Ϊ����ע���Զ���Ļص�����
 *   - ��������ʵ�������ض��Ĺ�����չ����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
BaseType_t xTaskCallApplicationTaskHook( TaskHandle_t xTask, void *pvParameter )
{
    TCB_t *xTCB;        /* ָ��������ƿ��ָ�� */
    BaseType_t xReturn; /* ����ֵ�����Ӻ����ķ���ֵ */

    /* ���xTaskΪNULL������õ�ǰ����Ĺ��Ӻ��� */
    if( xTask == NULL )
    {
        xTCB = ( TCB_t * ) pxCurrentTCB;
    }
    else
    {
        xTCB = ( TCB_t * ) xTask;
    }

    /* ��������Ƿ�ע���˹��Ӻ��� */
    if( xTCB->pxTaskTag != NULL )
    {
        /* ��������Ĺ��Ӻ��������ݲ��� */
        xReturn = xTCB->pxTaskTag( pvParameter );
    }
    else
    {
        /* ����û��ע�ṳ�Ӻ���������ʧ�� */
        xReturn = pdFAIL;
    }

    /* ���ع��Ӻ����ķ���ֵ��ʧ��״̬ */
    return xReturn;
}

#endif /* configUSE_APPLICATION_TASK_TAG */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�vTaskSwitchContext
 * ����������ִ�������������л���ѡ���л���������ȼ��ľ�������
 *           �˺�����FreeRTOS�������ĺ��ģ�����������л��͹���
 * �����������
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺�����FreeRTOS�������ĺ���ʵ�֣�����������������л�
 *   - �������������״̬����ֹ�ڹ���ʱ�����������л�
 *   - ֧������ʱͳ�ơ�ջ��������¿�����ȹ���
 *   - ��������ѡ��ͨ��C�����˿��Ż��Ļ������������ѡ��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
void vTaskSwitchContext( void )
{
    /* ���������Ƿ񱻹��� */
    if( uxSchedulerSuspended != ( UBaseType_t ) pdFALSE )
    {
        /* ��������ǰ������ - �������������л� */
        xYieldPending = pdTRUE;
    }
    else
    {
        /* ������δ���𣬿���ִ���������л� */
        xYieldPending = pdFALSE;
        
        /* ���������л����¼� */
        traceTASK_SWITCHED_OUT();

        /* �������������ʱͳ�ƹ��� */
        #if ( configGENERATE_RUN_TIME_STATS == 1 )
        {
            /* ��ȡ��ǰ����ʱ�������ֵ */
            #ifdef portALT_GET_RUN_TIME_COUNTER_VALUE
                portALT_GET_RUN_TIME_COUNTER_VALUE( ulTotalRunTime );
            #else
                ulTotalRunTime = portGET_RUN_TIME_COUNTER_VALUE();
            #endif

            /* ���������е�ʱ������ӵ��ۼ�ʱ���С�
               ����ʼ���е�ʱ��洢��ulTaskSwitchedInTime�С�
               ע�⣺����û�������������˼���ֵ���ڶ�ʱ�����֮ǰ��Ч��
               �Ը�ֵ�ķ�����Ϊ�˷�ֹ���ɵ�����ʱͳ�Ƽ�����ʵ�� -
               ��Щʵ����Ӧ�ó����ṩ���������ں� */
            if( ulTotalRunTime > ulTaskSwitchedInTime )
            {
                /* ������������ʱ�䲢��ӵ�����ʱ������� */
                pxCurrentTCB->ulRunTimeCounter += ( ulTotalRunTime - ulTaskSwitchedInTime );
            }
            else
            {
                /* ʱ��ֵ�쳣����Ӳ��Ը����ʱ�� */
                mtCOVERAGE_TEST_MARKER();
            }
            
            /* ���������л�ʱ���Ϊ��ǰʱ�� */
            ulTaskSwitchedInTime = ulTotalRunTime;
        }
        #endif /* configGENERATE_RUN_TIME_STATS */

        /* ���ջ�������������ˣ� */
        taskCHECK_FOR_STACK_OVERFLOW();

        /* ʹ��ͨ��C�����˿��Ż��Ļ�����ѡ��Ҫ���е������� */
        taskSELECT_HIGHEST_PRIORITY_TASK();
        
        /* ���������л����¼� */
        traceTASK_SWITCHED_IN();

        /* ���ʹ��NewLib������Ϊ������ */
        #if ( configUSE_NEWLIB_REENTRANT == 1 )
        {
            /* �л�Newlib��_impure_ptr������ָ��������ض���_reent�ṹ */
            _impure_ptr = &( pxCurrentTCB->xNewLib_reent );
        }
        #endif /* configUSE_NEWLIB_REENTRANT */
    }
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�vTaskPlaceOnEventList
 * ��������������ǰ������õ��¼��б��У������õȴ�ʱ��
 *           �˺������ڽ��������ȼ�˳������¼��б�����ӵ��ӳ��б�ʵ�ֳ�ʱ
 * ���������
 *   - pxEventList: ָ���¼��б��ָ�룬���񽫱������ȼ�˳�������б�
 *   - xTicksToWait: �ȴ��ĳ�ʱʱ�䣨��ʱ�ӽ���Ϊ��λ��
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺����������жϽ��û�����������Ҷ��б�����������µ���
 *   - �������ȼ�˳������¼��б�ȷ��������ȼ��������ȱ��¼�����
 *   - �����¼��б�Ķ��б���������ֹ�ж�ͬʱ����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
void vTaskPlaceOnEventList( List_t * const pxEventList, const TickType_t xTicksToWait )
{
    /* ���Լ���¼��б�ָ����Ч�� */
    configASSERT( pxEventList );

    /* �˺����������жϽ��û�����������Ҷ��б�����������µ��� */

    /* ��TCB���¼��б���������ʵ����¼��б��С�
       �����ȼ�˳��������б��У����������ȼ����������ȱ��¼����ѡ�
       �����¼��б�Ķ��б���������ֹ�ж�ͬʱ���� */
    vListInsert( pxEventList, &( pxCurrentTCB->xEventListItem ) );

    /* ����ǰ������ӵ��ӳ��б�ʵ�ֳ�ʱ���� */
    prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�vTaskPlaceOnUnorderedEventList
 * ��������������ǰ������õ������¼��б��У��������¼���ֵ�͵ȴ�ʱ��
 *           �˺����ڵ���������ʱ���ã������¼���ʵ���е�������������
 * ���������
 *   - pxEventList: ָ���¼��б��ָ�룬���񽫱����õ����б���
 *   - xItemValue: Ҫ���õ��¼��б���ֵ��ͨ�������¼���־��Ϣ
 *   - xTicksToWait: �ȴ��ĳ�ʱʱ�䣨��ʱ�ӽ���Ϊ��λ��
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺��������ڵ��������������µ��ã������¼���ʵ��
 *   - �¼��б�������ģ��������ȼ�����
 *   - �����¼��б���ֵ�����Ϊ����ʹ��
 *   - ��������ӵ��ӳ��б�ʵ�ֳ�ʱ����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
void vTaskPlaceOnUnorderedEventList( List_t * pxEventList, const TickType_t xItemValue, const TickType_t xTicksToWait )
{
    /* ���Լ���¼��б�ָ����Ч�� */
    configASSERT( pxEventList );

    /* �˺��������ڵ��������������µ��á������¼���ʵ�� */
    configASSERT( uxSchedulerSuspended != 0 );

    /* ���¼��б����д洢��ֵ�����ﰲȫ�ط����¼��б��
       ��Ϊ�жϲ�����ʲ�������״̬��������¼��б��� */
    listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xEventListItem ), xItemValue | taskEVENT_LIST_ITEM_VALUE_IN_USE );

    /* ��TCB���¼��б���������ʵ����¼��б��ĩβ��
       ���ﰲȫ�ط����¼��б���Ϊ�����¼���ʵ�ֵ�һ���� -
       �жϲ���ֱ�ӷ����¼��飨����ͨ�����������ù������񼶱�����ӷ��ʣ� */
    vListInsertEnd( pxEventList, &( pxCurrentTCB->xEventListItem ) );

    /* ����ǰ������ӵ��ӳ��б�ʵ�ֳ�ʱ���� */
    prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
}
/*-----------------------------------------------------------*/

#if( configUSE_TIMERS == 1 )

/*******************************************************************************
 * �������ƣ�vTaskPlaceOnEventListRestricted
 * ��������������ǰ������õ������¼��б��У������õȴ�ʱ��������ڵȴ�
 *           �˺������ں˴���ר�õ����޺�������Ӧ��Ӧ�ó���������
 * ���������
 *   - pxEventList: ָ���¼��б��ָ�룬���񽫱����õ����б���
 *   - xTicksToWait: �ȴ��ĳ�ʱʱ�䣨��ʱ�ӽ���Ϊ��λ��
 *   - xWaitIndefinitely: �Ƿ������ڵȴ��ı�־
 *     pdTRUE: �����ڵȴ�������xTicksToWait����
 *     pdFALSE: ʹ��xTicksToWait����ָ���ĳ�ʱʱ��
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺������ں˴���ר�õ����޺�������Ӧ��Ӧ�ó���������
 *   - ��Ҫ�ڵ��������������µ���
 *   - ����ֻ��һ������ȴ����¼��б����ʹ��vListInsertEnd����vListInsert
 *   - ��Ҫ���ڶ�ʱ��ʵ��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
void vTaskPlaceOnEventListRestricted( List_t * const pxEventList, TickType_t xTicksToWait, const BaseType_t xWaitIndefinitely )
{
    /* ���Լ���¼��б�ָ����Ч�� */
    configASSERT( pxEventList );

    /* �˺�����Ӧ��Ӧ�ó��������ã���������а���"Restricted"��
       �����ǹ���API��һ���֡���רΪ�ں˴���ʹ�ö���ƣ�
       ����������ĵ���Ҫ�� - Ӧ�ڵ��������������µ��� */

    /* ��TCB���¼��б���������ʵ����¼��б��С�
       ����������£���������Ψһ�ȴ����¼��б������
       ��˿���ʹ�ø����vListInsertEnd()��������vListInsert */
    vListInsertEnd( pxEventList, &( pxCurrentTCB->xEventListItem ) );

    /* �������Ӧ��������������������ʱ������Ϊһ��ֵ��
       ��ֵ����prvAddCurrentTaskToDelayedList()�����б�ʶ��Ϊ�����ӳ� */
    if( xWaitIndefinitely != pdFALSE )
    {
        xTicksToWait = portMAX_DELAY;
    }

    /* ���������ӳ�ֱ��ָ��ʱ��� */
    traceTASK_DELAY_UNTIL( ( xTickCount + xTicksToWait ) );
    
    /* ����ǰ������ӵ��ӳ��б� */
    prvAddCurrentTaskToDelayedList( xTicksToWait, xWaitIndefinitely );
}

#endif /* configUSE_TIMERS */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xTaskRemoveFromEventList
 * �������������¼��б����Ƴ����ȼ���ߵ����񲢽�����ӵ������б�
 *           �˺������ٽ����ڵ��ã����ڴ����¼���ص���������������
 * ���������
 *   - pxEventList: ָ���¼��б��ָ�룬���б����ȼ�����
 * �����������
 * �� �� ֵ��
 *   - BaseType_t: ָʾ�Ƿ���Ҫ���������������л�
 *     pdTRUE: ���Ƴ����������ȼ����ڵ�ǰ������Ҫ�����������л�
 *     pdFALSE: ���Ƴ����������ȼ������ڵ�ǰ���񣬲���Ҫ�����������л�
 * ����˵����
 *   - �˺����������ٽ����ڵ��ã�Ҳ���Դ��жϷ��������ٽ����ڵ���
 *   - �¼��б����ȼ�������˿��԰�ȫ���Ƴ���һ���������ȼ���ߣ�
 *   - ���ݵ�����״̬������������������ӵ������б����ݹҵ��������б�
 *   - ����������޿��еδ�ģʽ����������һ������������ʱ��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
BaseType_t xTaskRemoveFromEventList( const List_t * const pxEventList )
{
    TCB_t *pxUnblockedTCB;  /* ָ�򱻽��������������ƿ� */
    BaseType_t xReturn;     /* ����ֵ���Ƿ���Ҫ�����������л� */

    /* �˺����������ٽ����ڵ��á�Ҳ���Դ��жϷ��������ٽ����ڵ��� */

    /* �¼��б����ȼ�������˿����Ƴ��б��еĵ�һ������
       ��Ϊ����֪��������ȼ��ġ���TCB���ӳ��б����Ƴ���
       ��������ӵ������б�

       ����¼�����Ա������Ķ��У�����Զ������ô˺��� -
       �����ϵ��������������޸ġ�����ζ�����ﱣ֤���¼��б�Ķ�ռ���ʡ�

       �˺����ٶ��Ѿ������˼����ȷ��pxEventList��Ϊ�� */
    pxUnblockedTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxEventList );
    configASSERT( pxUnblockedTCB );
    ( void ) uxListRemove( &( pxUnblockedTCB->xEventListItem ) );

    /* ���������Ƿ�δ���� */
    if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
    {
        /* ������δ���𣺽������״̬�б����Ƴ�����ӵ������б� */
        ( void ) uxListRemove( &( pxUnblockedTCB->xStateListItem ) );
        prvAddTaskToReadyList( pxUnblockedTCB );
    }
    else
    {
        /* �������ѹ����޷������ӳٺ;����б�
           ��˽��������ݹ�ֱ���������ָ� */
        vListInsertEnd( &( xPendingReadyList ), &( pxUnblockedTCB->xEventListItem ) );
    }

    /* ��鱻����������������ȼ��Ƿ���ڵ�ǰ���� */
    if( pxUnblockedTCB->uxPriority > pxCurrentTCB->uxPriority )
    {
        /* ������¼��б����Ƴ����������ȼ����ڵ��������򷵻�true��
           �������������֪���Ƿ�Ӧ������ǿ���������л� */
        xReturn = pdTRUE;

        /* �����yield�����Է��û�û��ʹ��ISR��ȫ��FreeRTOS�����е�
           "xHigherPriorityTaskWoken"���� */
        xYieldPending = pdTRUE;
    }
    else
    {
        xReturn = pdFALSE;
    }

    /* ����������޿��еδ�ģʽ */
    #if( configUSE_TICKLESS_IDLE != 0 )
    {
        /* ����������ں˶�������������xNextTaskUnblockTime��������Ϊ
           ����������ĳ�ʱʱ�䡣���������ʱ�����ԭ����������
           xNextTaskUnblockTimeͨ�����ֲ��䣬��Ϊ���δ��������
           xNextTaskUnblockTimeʱ���Զ�����Ϊ��ֵ�����ǣ����ʹ��
           �޿��еδ�ģʽ�����ܸ���Ҫ�����ھ��������ʱ�����˯��ģʽ -
           �������������xNextTaskUnblockTime��ȷ���ھ��������ʱ����� */
        prvResetNextTaskUnblockTime();
    }
    #endif

    return xReturn;
}
/*-----------------------------------------------------------*/
/*******************************************************************************
 * �������ƣ�xTaskRemoveFromUnorderedEventList
 * �����������������¼��б����Ƴ����񲢽�����ӵ������б������¼���־ʵ��
 *           �˺����ڵ���������ʱ���ã����ڴ����¼���־��ص���������������
 * ���������
 *   - pxEventListItem: ָ���¼��б����ָ�룬��ʾҪ�Ƴ����¼��б���
 *   - xItemValue: Ҫ���õ��¼��б���ֵ��ͨ�������¼���־��Ϣ
 * �����������
 * �� �� ֵ��
 *   - BaseType_t: ָʾ�Ƿ���Ҫ���������������л�
 *     pdTRUE: ���Ƴ����������ȼ����ڵ�ǰ������Ҫ�����������л�
 *     pdFALSE: ���Ƴ����������ȼ������ڵ�ǰ���񣬲���Ҫ�����������л�
 * ����˵����
 *   - �˺��������ڵ��������������µ��ã������¼���־ʵ��
 *   - �����漰����б���޸ģ���Ҫȷ��ԭ����
 *   - ���������������������ȼ����ߣ�������yield�����־
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
BaseType_t xTaskRemoveFromUnorderedEventList( ListItem_t * pxEventListItem, const TickType_t xItemValue )
{
    TCB_t *pxUnblockedTCB;  /* ָ�򱻽��������������ƿ� */
    BaseType_t xReturn;     /* ����ֵ���Ƿ���Ҫ�����������л� */

    /* �˺��������ڵ��������������µ��á������¼���־ʵ�� */
    configASSERT( uxSchedulerSuspended != pdFALSE );

    /* ���¼��б��д洢�µ���ֵ�������Ϊ����ʹ�� */
    listSET_LIST_ITEM_VALUE( pxEventListItem, xItemValue | taskEVENT_LIST_ITEM_VALUE_IN_USE );

    /* ���¼���־���Ƴ��¼��б��жϲ�������¼���־ */
    pxUnblockedTCB = ( TCB_t * ) listGET_LIST_ITEM_OWNER( pxEventListItem );
    configASSERT( pxUnblockedTCB );
    ( void ) uxListRemove( pxEventListItem );

    /* ��������ӳ��б����Ƴ�����ӵ������б��������ѹ���
       ����жϲ�����ʾ����б� */
    ( void ) uxListRemove( &( pxUnblockedTCB->xStateListItem ) );
    prvAddTaskToReadyList( pxUnblockedTCB );

    /* ��鱻����������������ȼ��Ƿ���ڵ�ǰ���� */
    if( pxUnblockedTCB->uxPriority > pxCurrentTCB->uxPriority )
    {
        /* ������¼��б����Ƴ����������ȼ����ڵ��������򷵻�true��
           �������������֪���Ƿ�Ӧ������ǿ���������л� */
        xReturn = pdTRUE;

        /* �����yield�����Է��û�û��ʹ��ISR��ȫ��FreeRTOS�����е�
           "xHigherPriorityTaskWoken"���� */
        xYieldPending = pdTRUE;
    }
    else
    {
        xReturn = pdFALSE;
    }

    return xReturn;
	}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�vTaskSetTimeOutState
 * �������������ó�ʱ״̬�ṹ����¼��ǰʱ���͵δ�������������
 *           �˺������ڳ�ʼ�������ó�ʱ״̬��Ϊ�����ĳ�ʱ����ṩ��׼ʱ��
 * ���������
 *   - pxTimeOut: ָ��TimeOut_t�ṹ��ָ�룬���ڴ洢��ʱ״̬��Ϣ
 * ���������
 *   - pxTimeOut: ����ʼ���ĳ�ʱ״̬�ṹ��������ǰʱ�����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺�����¼��ǰ�δ�����������������Ϊ��ʱ�������ʼ��
 *   - ͨ����xTaskCheckForTimeOut���ʹ�ã�ʵ�ֿ��жϵ���������
 *   - ��Ҫ���ٽ�������ã���Ϊ��ֻ�Ǽ򵥼�¼��ǰ״̬
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
void vTaskSetTimeOutState( TimeOut_t * const pxTimeOut )
{
    /* �������Լ�飬ȷ��ָ����Ч */
    configASSERT( pxTimeOut );
    
    /* ��¼��ǰ�ĵδ������������� */
    pxTimeOut->xOverflowCount = xNumOfOverflows;
    
    /* ��¼��ǰ�ĵδ����ֵ */
    pxTimeOut->xTimeOnEntering = xTickCount;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xTaskCheckForTimeOut
 * ������������鳬ʱ״̬������ʣ��ȴ�ʱ�䣬����ʵ�ֿ��жϵ���������
 *           �˺���������������ʱ�ĳ�ʱ��飬֧���ӳ���ֹ�����޵ȴ��������
 * ���������
 *   - pxTimeOut: ָ��TimeOut_t�ṹ��ָ�룬������ʱ״̬��Ϣ
 *   - pxTicksToWait: ָ��ʣ��ȴ�ʱ���ָ�룬��������´�ֵ
 * ���������
 *   - pxTicksToWait: ���µ�ʣ��ȴ�ʱ�䣨���û�г�ʱ��
 * �� �� ֵ��
 *   - BaseType_t: ��ʱ״̬
 *     pdTRUE: �ѳ�ʱ���ӳٱ���ֹ
 *     pdFALSE: δ��ʱ��ʣ��ȴ�ʱ���Ѹ���
 * ����˵����
 *   - �˺������ٽ�����ִ�У�ȷ����ʱ����ԭ����
 *   - ����δ������������������
 *   - ֧���ӳ���ֹ�����޵ȴ����ܣ�������ã�
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
BaseType_t xTaskCheckForTimeOut( TimeOut_t * const pxTimeOut, TickType_t * const pxTicksToWait )
{
    BaseType_t xReturn;  /* ����ֵ����ʱ״̬ */

    /* �������Լ�飬ȷ��ָ����Ч */
    configASSERT( pxTimeOut );
    configASSERT( pxTicksToWait );

    /* �����ٽ�����ȷ����ʱ����ԭ���� */
    taskENTER_CRITICAL();
    {
        /* С�Ż����ڴ˿��ڵδ��������ı䣬ʹ�ó������浱ǰ�δ���� */
        const TickType_t xConstTickCount = xTickCount;

        /* ����������ӳ���ֹ���� */
        #if( INCLUDE_xTaskAbortDelay == 1 )
            if( pxCurrentTCB->ucDelayAborted != pdFALSE )
            {
                /* �ӳٱ���ֹ�����볬ʱ��ͬ���������ͬ */
                pxCurrentTCB->ucDelayAborted = pdFALSE;  /* �����ֹ��־ */
                xReturn = pdTRUE;  /* ���س�ʱ״̬ */
            }
            else
        #endif

        /* ������������������ */
        #if ( INCLUDE_vTaskSuspend == 1 )
            if( *pxTicksToWait == portMAX_DELAY )
            {
                /* ���������vTaskSuspend��ָ��������ʱ�����������ʱ�䣬
                   ������Ӧ�����������������Զ���ᳬʱ */
                xReturn = pdFALSE;  /* ����δ��ʱ״̬ */
            }
            else
        #endif

        /* ���δ�������Ƿ�����ҵ�ǰʱ����ڵ��ڽ���ʱ�� */
        if( ( xNumOfOverflows != pxTimeOut->xOverflowCount ) && ( xConstTickCount >= pxTimeOut->xTimeOnEntering ) ) /*lint !e525 Indentation preferred as is to make code within pre-processor directives clearer. */
        {
            /* �δ�������ڵ���vTaskSetTimeout()ʱ��ʱ�䣬�����Ե���vTaskSetTimeOut()����
               �Ѿ�������������Ѿ�������һȦ���ٴξ�����������Ե���vTaskSetTimeout()����
               �Ѿ���ʱ */
            xReturn = pdTRUE;  /* ���س�ʱ״̬ */
        }
        /* ����Ƿ���δ��ʱ */
        else if( ( ( TickType_t ) ( xConstTickCount - pxTimeOut->xTimeOnEntering ) ) < *pxTicksToWait ) /*lint !e961 Explicit casting is only redundant with some compilers, whereas others require it to prevent integer conversion errors. */
        {
            /* ���������ĳ�ʱ������ʣ��ʱ����� */
            *pxTicksToWait -= ( xConstTickCount - pxTimeOut->xTimeOnEntering );  /* ����ʣ��ȴ�ʱ�� */
            vTaskSetTimeOutState( pxTimeOut );  /* ���ó�ʱ״̬ */
            xReturn = pdFALSE;  /* ����δ��ʱ״̬ */
        }
        else
        {
            /* ����������ѳ�ʱ�� */
            xReturn = pdTRUE;  /* ���س�ʱ״̬ */
        }
    }
    /* �˳��ٽ��� */
    taskEXIT_CRITICAL();

    /* ���س�ʱ״̬ */
    return xReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�vTaskMissedYield
 * ��������������Yield�����־��ָʾ���жϷ�������з�������Ҫ�����л������
 *           �˺���ͨ�����жϷ������(ISR)�е��ã������ӳ������л�����
 * �����������
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺�������ȫ�ֱ�־xYieldPending��ָʾ��Ҫ�ӳ�ִ�������л�
 *   - ʵ�ʵ������л������ʵ���ʱ�������˳��ٽ������ɵ�����ִ��
 *   - ������ISR�а�ȫ�����������л���������ISR��ֱ�ӽ����������л�
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
void vTaskMissedYield( void )
{
    /* ����Yield�����־��ָʾ��Ҫ�ӳ�ִ�������л� */
    xYieldPending = pdTRUE;
}
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

/*******************************************************************************
 * �������ƣ�uxTaskGetTaskNumber
 * ������������ȡָ������������ţ����������ʶ�͸���
 *           �������������Ψһ��ʶ���������ڵ��Ժ͸���Ŀ��
 * ���������
 *   - xTask: Ҫ��ȡ�����ŵ�������
 *     ����ֵ������NULL������0
 * �����������
 * �� �� ֵ��
 *   - UBaseType_t: ����ı�ţ������������Ч�򷵻�0
 * ����˵����
 *   - �˺����������ø��ٹ���(configUSE_TRACE_FACILITY == 1)ʱ����
 *   - �������������Ψһ��ʶ������ͬ���������ȼ�
 *   - �����ڵ��ԡ����ٺ����ܷ����ȳ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
UBaseType_t uxTaskGetTaskNumber( TaskHandle_t xTask )
{
    UBaseType_t uxReturn;  /* ����ֵ�������� */
    TCB_t *pxTCB;          /* ָ��������ƿ��ָ�� */

    /* ����������Ƿ���Ч */
    if( xTask != NULL )
    {
        /* ��������ת��Ϊ������ƿ�ָ�� */
        pxTCB = ( TCB_t * ) xTask;
        /* ��ȡ������ */
        uxReturn = pxTCB->uxTaskNumber;
    }
    else
    {
        /* ��������Ч������0 */
        uxReturn = 0U;
    }

    /* ���������� */
    return uxReturn;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

/*******************************************************************************
 * �������ƣ�vTaskSetTaskNumber
 * ��������������ָ������������ţ����������ʶ�͸���
 *           �������������Ψһ��ʶ���������ڵ��Ժ͸���Ŀ��
 * ���������
 *   - xTask: Ҫ���������ŵ�������
 *     ����ֵ������NULL����ִ���κβ���
 *   - uxHandle: Ҫ���õ�������ֵ
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺����������ø��ٹ���(configUSE_TRACE_FACILITY == 1)ʱ����
 *   - �������������Ψһ��ʶ������ͬ���������ȼ�
 *   - �����ڵ��ԡ����ٺ����ܷ����ȳ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
void vTaskSetTaskNumber( TaskHandle_t xTask, const UBaseType_t uxHandle )
{
    TCB_t *pxTCB;  /* ָ��������ƿ��ָ�� */

    /* ����������Ƿ���Ч */
    if( xTask != NULL )
    {
        /* ��������ת��Ϊ������ƿ�ָ�� */
        pxTCB = ( TCB_t * ) xTask;
        /* ���������� */
        pxTCB->uxTaskNumber = uxHandle;
    }
    /* �����������Ч����Ĭʧ�ܣ���ִ���κβ����� */
}

#endif /* configUSE_TRACE_FACILITY */

/*******************************************************************************
 * �������ƣ�prvIdleTask
 * ����������FreeRTOS����������ϵͳ�Զ������ĵ����ȼ�������ϵͳû��������������ʱִ��
 *           ������������ֹ������ִ�е͹��Ĵ��������û����й��Ӻ�����ϵͳά������
 * ���������
 *   - pvParameters: ���������δʹ�ã���Ϊ�������������棩
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - ��������RTOS�Զ������Ŀ��������ڵ���������ʱ����
 *   - ���ȼ�Ϊ������ȼ���tskIDLE_PRIORITY��
 *   - ��ϵͳû��������������ʱ����
 *   - �������������͹��Ĵ�����û����Ӻ������õȹ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
static portTASK_FUNCTION( prvIdleTask, pvParameters )
{
    /* ����δʹ�ò����ı��������� */
    ( void ) pvParameters;

    /** ����RTOS�������� - �ڵ���������ʱ�Զ����� **/

    /* ����ѭ��������ִ�п�������Ĺ��� */
    for( ;; )
    {
        /* ����Ƿ���������ɾ������ - ����У������������ͷ���ɾ�������TCB��ջ */
        prvCheckTasksWaitingTermination();

        /* ���δʹ����ռʽ���ȣ���Ҫ�����ó�CPU */
        #if ( configUSE_PREEMPTION == 0 )
        {
            /* �����ʹ����ռʽ���ȣ�������Ҫǿ�������л���
               ����Ƿ������������Ϊ���á����ʹ����ռʽ���ȣ�
               ����Ҫ����������Ϊ�κο��õ����񶼻��Զ���ô����� */
            taskYIELD();
        }
        #endif /* configUSE_PREEMPTION */

        /* ���ʹ����ռʽ�����������˿��������ó� */
        #if ( ( configUSE_PREEMPTION == 1 ) && ( configIDLE_SHOULD_YIELD == 1 ) )
        {
            /* ʹ����ռʽ����ʱ����ͬ���ȼ�����������ʱ��Ƭ��ת��
               �������������ȼ�������׼�����У���������Ӧ��ʱ��Ƭ����ǰ�ó�CPU��

               ���ﲻ��Ҫ�ٽ�������Ϊ����ֻ�Ǵ��б��ж�ȡ���ݣ�
               ż���Ĵ���ֵ�������Ӱ�졣��������б��п������ȼ�
               ��������������1�����ʾ�п����������������׼��ִ�� */
            if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ tskIDLE_PRIORITY ] ) ) > ( UBaseType_t ) 1 )
            {
                taskYIELD();  /* �ó�CPU */
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
            }
        }
        #endif /* ( ( configUSE_PREEMPTION == 1 ) && ( configIDLE_SHOULD_YIELD == 1 ) ) */

        /* ��������˿��й��Ӻ��� */
        #if ( configUSE_IDLE_HOOK == 1 )
        {
            /* �����ⲿ����Ŀ��й��Ӻ��� */
            extern void vApplicationIdleHook( void );

            /* �ڿ��������е����û�����ĺ�����������Ӧ�ó����������Ӻ�̨���ܣ�
               �������������Ŀ�����
               ע�⣺vApplicationIdleHook()���κ�����¶����õ��ÿ��������ĺ��� */
            vApplicationIdleHook();
        }
        #endif /* configUSE_IDLE_HOOK */

        /* ����������Ӧʹ�ò�����0�������ǵ���1������Ϊ��ȷ�����û�����ĵ͹���ģʽ
           ʵ����Ҫ��configUSE_TICKLESS_IDLE����Ϊ1�����ֵʱ��Ҳ�ܵ���
           portSUPPRESS_TICKS_AND_SLEEP() */
        #if ( configUSE_TICKLESS_IDLE != 0 )
        {
        TickType_t xExpectedIdleTime;  /* Ԥ�ڿ���ʱ�� */

            /* ��ϣ��ÿ�ο����������������Ȼ��ָ���������
               ��ˣ����ڲ����������������½���Ԥ�ڿ���ʱ��ĳ������ԡ�
               ����Ľ����һ����Ч */
            xExpectedIdleTime = prvGetExpectedIdleTime();

            if( xExpectedIdleTime >= configEXPECTED_IDLE_TIME_BEFORE_SLEEP )
            {
                /* ������������ */
                vTaskSuspendAll();
                {
                    /* ���ڵ������ѹ��𣬿����ٴβ���Ԥ�ڿ���ʱ�䣬
                       ��ο���ʹ����ֵ */
                    configASSERT( xNextTaskUnblockTime >= xTickCount );
                    xExpectedIdleTime = prvGetExpectedIdleTime();

                    if( xExpectedIdleTime >= configEXPECTED_IDLE_TIME_BEFORE_SLEEP )
                    {
                        /* ���ٵ͹��Ŀ��п�ʼ */
                        traceLOW_POWER_IDLE_BEGIN();
                        /* ����ʱ�ӵδ𲢽���˯�� */
                        portSUPPRESS_TICKS_AND_SLEEP( xExpectedIdleTime );
                        /* ���ٵ͹��Ŀ��н��� */
                        traceLOW_POWER_IDLE_END();
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
                    }
                }
                /* �ָ��������� */
                ( void ) xTaskResumeAll();
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
            }
        }
        #endif /* configUSE_TICKLESS_IDLE */
    }
}
/*-----------------------------------------------------------*/

#if( configUSE_TICKLESS_IDLE != 0 )

	eSleepModeStatus eTaskConfirmSleepModeStatus( void )
	{
	/* The idle task exists in addition to the application tasks. */
	const UBaseType_t uxNonApplicationTasks = 1;
	eSleepModeStatus eReturn = eStandardSleep;

		if( listCURRENT_LIST_LENGTH( &xPendingReadyList ) != 0 )
		{
			/* A task was made ready while the scheduler was suspended. */
			eReturn = eAbortSleep;
		}
		else if( xYieldPending != pdFALSE )
		{
			/* A yield was pended while the scheduler was suspended. */
			eReturn = eAbortSleep;
		}
		else
		{
			/* If all the tasks are in the suspended list (which might mean they
			have an infinite block time rather than actually being suspended)
			then it is safe to turn all clocks off and just wait for external
			interrupts. */
			if( listCURRENT_LIST_LENGTH( &xSuspendedTaskList ) == ( uxCurrentNumberOfTasks - uxNonApplicationTasks ) )
			{
				eReturn = eNoTasksWaitingTimeout;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}

		return eReturn;
	}

#endif /* configUSE_TICKLESS_IDLE */
/*-----------------------------------------------------------*/

#if ( configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0 )

/*******************************************************************************
 * �������ƣ�vTaskSetThreadLocalStoragePointer
 * ��������������ָ��������̱߳��ش洢(TLS)ָ�룬���ڴ洢�����ض�������
 *           �˺����ṩ��һ�ֻ��ƣ�����ÿ������ӵ���Լ������ݴ洢�ռ�
 * ���������
 *   - xTaskToSet: Ҫ���õ�������
 *     ����ֵ������NULL��ʾ���õ�ǰ�����TLSָ��
 *   - xIndex: TLSָ�����������Χ��0��configNUM_THREAD_LOCAL_STORAGE_POINTERS-1
 *   - pvValue: Ҫ���õ�ָ��ֵ���������������͵�����ָ��
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺��������������̱߳��ش洢ָ��(configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0)ʱ����
 *   - �ṩ��һ�������ض������ݴ洢���ƣ��������̱߳��ش洢(TLS)�ĸ���
 *   - ÿ����������ж��TLSָ�룬ͨ����������
 *   - ���������Ч����������ִ���κβ�������Ĭʧ�ܣ�
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
void vTaskSetThreadLocalStoragePointer( TaskHandle_t xTaskToSet, BaseType_t xIndex, void *pvValue )
{
    TCB_t *pxTCB;  /* ָ��������ƿ��ָ�� */

    /* ��������Ƿ�����Ч��Χ�� */
    if( xIndex < configNUM_THREAD_LOCAL_STORAGE_POINTERS )
    {
        /* ������������ȡ������ƿ� */
        pxTCB = prvGetTCBFromHandle( xTaskToSet );
        /* ����ָ��������TLSָ��ֵ */
        pxTCB->pvThreadLocalStoragePointers[ xIndex ] = pvValue;
    }
    /* �������������Χ����Ĭʧ�ܣ���ִ���κβ����� */
}

#endif /* configNUM_THREAD_LOCAL_STORAGE_POINTERS */
/*-----------------------------------------------------------*/

#if ( configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0 )

/*******************************************************************************
 * �������ƣ�pvTaskGetThreadLocalStoragePointer
 * ������������ȡָ��������̱߳��ش洢(TLS)ָ�룬���ڷ��������ض������ݴ洢
 *           �˺����ṩ��һ�ֻ��ƣ�����ÿ������ӵ���Լ������ݴ洢�ռ�
 * ���������
 *   - xTaskToQuery: Ҫ��ѯ��������
 *     ����ֵ������NULL��ʾ��ѯ��ǰ�����TLSָ��
 *   - xIndex: TLSָ�����������Χ��0��configNUM_THREAD_LOCAL_STORAGE_POINTERS-1
 * �����������
 * �� �� ֵ��
 *   - void*: ָ���̱߳��ش洢���ݵ�ָ�룬���������Ч�򷵻�NULL
 * ����˵����
 *   - �˺��������������̱߳��ش洢ָ��(configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0)ʱ����
 *   - �ṩ��һ�������ض������ݴ洢���ƣ��������̱߳��ش洢(TLS)�ĸ���
 *   - ÿ����������ж��TLSָ�룬ͨ����������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
void *pvTaskGetThreadLocalStoragePointer( TaskHandle_t xTaskToQuery, BaseType_t xIndex )
{
    void *pvReturn = NULL;  /* ����ֵ��TLSָ�룬��ʼ��ΪNULL */
    TCB_t *pxTCB;           /* ָ��������ƿ��ָ�� */

    /* ��������Ƿ�����Ч��Χ�� */
    if( xIndex < configNUM_THREAD_LOCAL_STORAGE_POINTERS )
    {
        /* ������������ȡ������ƿ� */
        pxTCB = prvGetTCBFromHandle( xTaskToQuery );
        /* ��ȡָ��������TLSָ�� */
        pvReturn = pxTCB->pvThreadLocalStoragePointers[ xIndex ];
    }
    else
    {
        /* ����������Χ������NULL */
        pvReturn = NULL;
    }

    /* ����TLSָ�� */
    return pvReturn;
}

#endif /* configNUM_THREAD_LOCAL_STORAGE_POINTERS */
/*-----------------------------------------------------------*/

#if ( portUSING_MPU_WRAPPERS == 1 )

/*******************************************************************************
 * �������ƣ�vTaskAllocateMPURegions
 * ����������Ϊָ���������MPU���ڴ汣����Ԫ����������������ڴ����Ȩ��
 *           �˺�������̬�޸������MPU���ã��ṩ�ڴ汣������
 * ���������
 *   - xTaskToModify: Ҫ�޸�MPU���õ�������
 *     ����ֵ������NULL��ʾ�޸ĵ��������MPU����
 *   - xRegions: ָ��MPU�������������ָ�룬�����ڴ�����ĵ�ַ����С��Ȩ������
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺�����������MPU��װ��(portUSING_MPU_WRAPPERS == 1)ʱ����
 *   - ���ڶ�̬����������ڴ汣��������ǿϵͳ�İ�ȫ�Ժ��ȶ���
 *   - ������MPU֧�ֵ�Ӳ��ƽ̨��ʹ��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
void vTaskAllocateMPURegions( TaskHandle_t xTaskToModify, const MemoryRegion_t * const xRegions )
{
    TCB_t *pxTCB;  /* ָ��������ƿ��ָ�� */

    /* �������NULL�����޸ĵ��������MPU���� */
    pxTCB = prvGetTCBFromHandle( xTaskToModify );

    /* �洢�����MPU���õ�������ƿ��� */
    vPortStoreTaskMPUSettings( &( pxTCB->xMPUSettings ),  /* Ŀ��MPU���ýṹ */
                              xRegions,                   /* MPU������������ */
                              NULL,                       /* ��ѡ�Ķ��������δʹ�ã� */
                              0 );                        /* ��������Ĵ�С��δʹ�ã� */
}

#endif /* portUSING_MPU_WRAPPERS */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvInitialiseTaskLists
 * ������������ʼ��FreeRTOS�е����������б����������б��ӳ��б������б��
 *           �˺����ڵ���������ǰ���ã�Ϊ�������׼����������ݽṹ
 * �����������
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺�����FreeRTOS�ں˳�ʼ�������е��ã�Ϊ�������׼����Ҫ���б�ṹ
 *   - ��������ѡ���ʼ����ͬ�������б���ɾ��������ȹ�����ص��б�
 *   - �����ӳ������б������ӳ������б�ĳ�ʼָ��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
static void prvInitialiseTaskLists( void )
{
    UBaseType_t uxPriority;  /* ѭ�������������ڱ����������ȼ� */

    /* ��ʼ���������ȼ��ľ��������б� */
    for( uxPriority = ( UBaseType_t ) 0U; uxPriority < ( UBaseType_t ) configMAX_PRIORITIES; uxPriority++ )
    {
        /* ��ʼ����ǰ���ȼ��ľ��������б� */
        vListInitialise( &( pxReadyTasksLists[ uxPriority ] ) );
    }

    /* ��ʼ�������ӳ������б�����ʵ��ʱ��Ƭ��ת�� */
    vListInitialise( &xDelayedTaskList1 );  /* ��ʼ����һ���ӳ������б� */
    vListInitialise( &xDelayedTaskList2 );  /* ��ʼ���ڶ����ӳ������б� */
    
    /* ��ʼ�������������б����ڴ�ŵȴ�������Ϊ����״̬������ */
    vListInitialise( &xPendingReadyList );  /* ��ʼ�������������б� */

    /* �������������ɾ�����ܣ���ʼ���ȴ���ֹ�������б� */
    #if ( INCLUDE_vTaskDelete == 1 )
    {
        /* ��ʼ���ȴ���ֹ�������б������ɾ������δ��������� */
        vListInitialise( &xTasksWaitingTermination );
    }
    #endif /* INCLUDE_vTaskDelete */

    /* �����������������ܣ���ʼ������������б� */
    #if ( INCLUDE_vTaskSuspend == 1 )
    {
        /* ��ʼ������������б���ű���������� */
        vListInitialise( &xSuspendedTaskList );
    }
    #endif /* INCLUDE_vTaskSuspend */

    /* ��ʼ���ã�pxDelayedTaskListʹ��list1��pxOverflowDelayedTaskListʹ��list2 */
    pxDelayedTaskList = &xDelayedTaskList1;          /* ���õ�ǰ�ӳ������б�ָ��list1 */
    pxOverflowDelayedTaskList = &xDelayedTaskList2;  /* ��������ӳ������б�ָ��list2 */
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvCheckTasksWaitingTermination
 * ������������鲢����ȴ���ֹ�����񣬴ӿ��������е����԰�ȫɾ������ֹ������
 *           �˺�������������ɾ������δ�ͷ���Դ�����񣬷�ֹ��Դй©
 * �����������
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺�����RTOS���������е��ã�ȷ����ϵͳ����ʱִ���������
 *   - ������������ɾ������(INCLUDE_vTaskDelete == 1)ʱ����
 *   - ʹ������������ȷ���б���ʵ�ԭ����
 *   - ͨ���������Ż���ֹƵ�����������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
static void prvCheckTasksWaitingTermination( void )
{
    /** �˺�����RTOS���������е��� **/

    #if ( INCLUDE_vTaskDelete == 1 )
    {
        BaseType_t xListIsEmpty;  /* ��־λ��ָʾ��ֹ�����б��Ƿ�Ϊ�� */

        /* uxDeletedTasksWaitingCleanUp���ڷ�ֹ�ڿ��������й���Ƶ���ص���vTaskSuspendAll() */
        while( uxDeletedTasksWaitingCleanUp > ( UBaseType_t ) 0U )
        {
            /* ��������������ȷ������ֹ�����б��ԭ�ӷ��� */
            vTaskSuspendAll();
            {
                /* �����ֹ�����б��Ƿ�Ϊ�� */
                xListIsEmpty = listLIST_IS_EMPTY( &xTasksWaitingTermination );
            }
            /* �ָ�������� */
            ( void ) xTaskResumeAll();

            /* �����ֹ�����б�Ϊ�գ������б��еĵ�һ������ */
            if( xListIsEmpty == pdFALSE )
            {
                TCB_t *pxTCB;  /* ָ��Ҫ�����������ƿ� */

                /* �����ٽ����Ա����������б�ķ��� */
                taskENTER_CRITICAL();
                {
                    /* ��ȡ��ֹ�����б��е�һ�������TCB */
                    pxTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( ( &xTasksWaitingTermination ) );
                    /* �������״̬�б����Ƴ� */
                    ( void ) uxListRemove( &( pxTCB->xStateListItem ) );
                    /* ���ٵ�ǰ������� */
                    --uxCurrentNumberOfTasks;
                    /* ���ٵȴ������������� */
                    --uxDeletedTasksWaitingCleanUp;
                }
                /* �˳��ٽ��� */
                taskEXIT_CRITICAL();

                /* ɾ��������ƿ鲢�ͷ������Դ */
                prvDeleteTCB( pxTCB );
            }
            else
            {
                /* �б�Ϊ�գ���Ӳ��Ը����ʱ�� */
                mtCOVERAGE_TEST_MARKER();
            }
        }
    }
    #endif /* INCLUDE_vTaskDelete */
}
/*-----------------------------------------------------------*/
#if( configUSE_TRACE_FACILITY == 1 )

/*******************************************************************************
 * �������ƣ�vTaskGetInfo
 * ������������ȡָ���������ϸ��Ϣ����䵽����״̬�ṹ��
 *           �˺����ṩ���������״̬��Ϣ�������������ԡ�����ʱͳ�ƺ�ջʹ�����
 * ���������
 *   - xTask: ��������ָ��Ҫ��ȡ��Ϣ������
 *     ����ֵ������NULL��ʾ��ȡ��ǰ�������Ϣ
 *   - pxTaskStatus: ָ������״̬�ṹ��ָ�룬���ڴ洢��ȡ��������Ϣ
 *   - xGetFreeStackSpace: ��־λ��ָʾ�Ƿ��ȡջ��ˮλ�����Ϣ
 *     pdTRUE: ��ȡջ��ˮλ��ǣ�pdFALSE: ����ȡ��������ܣ�
 *   - eState: ����״̬���������eInvalid���Զ���ȡ����״̬
 *     ����ʹ�ô����״ֵ̬�������Ż����ܣ�
 * ���������
 *   - pxTaskStatus: ����������״̬�ṹ�������������ϸ��Ϣ
 * �� �� ֵ����
 * ����˵����
 *   - �˺����������ø��ٹ���(configUSE_TRACE_FACILITY == 1)ʱ����
 *   - �ṩ������������Ϣ�������������ԡ�����ʱͳ�ƺ�ջʹ�����
 *   - ֧��ѡ���Ի�ȡ��Ϣ���Ż����ܣ�������ջ�ռ���㣩
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
void vTaskGetInfo( TaskHandle_t xTask, TaskStatus_t *pxTaskStatus, BaseType_t xGetFreeStackSpace, eTaskState eState )
{
    TCB_t *pxTCB;  /* ָ��������ƿ��ָ�� */

    /* ���xTaskΪNULL�����ȡ���������״̬��Ϣ */
    pxTCB = prvGetTCBFromHandle( xTask );

    /* �������״̬�ṹ�Ļ�����Ϣ */
    pxTaskStatus->xHandle = ( TaskHandle_t ) pxTCB;                  /* ������ */
    pxTaskStatus->pcTaskName = ( const char * ) &( pxTCB->pcTaskName [ 0 ] ); /* �������� */
    pxTaskStatus->uxCurrentPriority = pxTCB->uxPriority;             /* ��ǰ���ȼ� */
    pxTaskStatus->pxStackBase = pxTCB->pxStack;                      /* ջ����ַ */
    pxTaskStatus->xTaskNumber = pxTCB->uxTCBNumber;                  /* ������ */

    /* �����������������ܣ��������״̬��������� */
    #if ( INCLUDE_vTaskSuspend == 1 )
    {
        /* ��������ڹ���״̬������Ƿ�ʵ������������������Ӧ����Ϊ����״̬�� */
        if( pxTaskStatus->eCurrentState == eSuspended )
        {
            /* ��������������ȷ���б���ʵ�ԭ���� */
            vTaskSuspendAll();
            {
                /* ���������¼��б����Ƿ���ĳ���б��У���ʾ����ʵ�������ڵȴ��¼��� */
                if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
                {
                    pxTaskStatus->eCurrentState = eBlocked;  /* ��״̬��Ϊ���� */
                }
            }
            /* �ָ�������� */
            xTaskResumeAll();
        }
    }
    #endif /* INCLUDE_vTaskSuspend */

    /* ����������ȼ���Ϣ�����ʹ�û������� */
    #if ( configUSE_MUTEXES == 1 )
    {
        /* ʹ�û�����ʱ��������������ȼ��̳У���¼�������ȼ� */
        pxTaskStatus->uxBasePriority = pxTCB->uxBasePriority;
    }
    #else
    {
        /* δʹ�û�����ʱ���������ȼ���Ϊ0 */
        pxTaskStatus->uxBasePriority = 0;
    }
    #endif

    /* ��������ʱͳ����Ϣ�������������ʱͳ�ƣ� */
    #if ( configGENERATE_RUN_TIME_STATS == 1 )
    {
        /* ��¼���������ʱ������� */
        pxTaskStatus->ulRunTimeCounter = pxTCB->ulRunTimeCounter;
    }
    #else
    {
        /* δ��������ʱͳ��ʱ������ʱ���������Ϊ0 */
        pxTaskStatus->ulRunTimeCounter = 0;
    }
    #endif

    /* ��ȡ����״̬����������eState����eInvalid����ʹ�ô����״̬ */
    /* �������eTaskGetState��ȡʵ��״̬������Ҫ����ʱ�䣩 */
    if( eState != eInvalid )
    {
        /* ʹ�ô��������״̬�������Ż��� */
        pxTaskStatus->eCurrentState = eState;
    }
    else
    {
        /* ����eTaskGetState��ȡ�����ʵ��״̬ */
        pxTaskStatus->eCurrentState = eTaskGetState( xTask );
    }

    /* ��ȡջ��ˮλ��ǣ�����xGetFreeStackSpace���������Ƿ���� */
    /* ����ջ�ռ���Ҫʱ�䣬����ṩ�������������˲��� */
    if( xGetFreeStackSpace != pdFALSE )
    {
        /* ����ջ������������ʵ��ĺ�������ջ��ˮλ��� */
        #if ( portSTACK_GROWTH > 0 )
        {
            /* ����������ջ��ʹ��ջ������ַ��Ϊ���� */
            pxTaskStatus->usStackHighWaterMark = prvTaskCheckFreeStackSpace( ( uint8_t * ) pxTCB->pxEndOfStack );
        }
        #else
        {
            /* ����������ջ��ʹ��ջ��ʼ��ַ��Ϊ���� */
            pxTaskStatus->usStackHighWaterMark = prvTaskCheckFreeStackSpace( ( uint8_t * ) pxTCB->pxStack );
        }
        #endif
    }
    else
    {
        /* ������ջ��ˮλ��ǣ���Ϊ0 */
        pxTaskStatus->usStackHighWaterMark = 0;
    }
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/
#if ( configUSE_TRACE_FACILITY == 1 )

/*******************************************************************************
 * �������ƣ�prvListTasksWithinSingleList
 * �����������������������б��������״̬���飬�ռ��б������������״̬��Ϣ
 *           �˺�����vTaskList��uxTaskGetSystemState���ڲ�ʵ�֣������ռ�������Ϣ
 * ���������
 *   - pxTaskStatusArray: ָ������״̬�����ָ�룬���ڴ洢�ռ�����������Ϣ
 *   - pxList: ָ��Ҫ�����������б��ָ�루������б������б�ȣ�
 *   - eState: �б������������״̬����Ϊͬһ�б��е�����״̬��ͬ��
 * ���������
 *   - pxTaskStatusArray: ����������״̬���飬�����б��������������Ϣ
 * �� �� ֵ��
 *   - UBaseType_t: �ɹ���������״̬�ṹ����
 * ����˵����
 *   - �˺����������ø��ٹ���(configUSE_TRACE_FACILITY == 1)ʱ����
 *   - ͨ��ѭ�������б��е�ÿ�������ռ���״̬��Ϣ
 *   - ʹ��vTaskGetInfo������ȡÿ���������ϸ��Ϣ
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
static UBaseType_t prvListTasksWithinSingleList( TaskStatus_t *pxTaskStatusArray, List_t *pxList, eTaskState eState )
{
    volatile TCB_t *pxNextTCB;      /* ָ����һ��������ƿ��ָ�루volatile��ֹ�Ż��� */
    volatile TCB_t *pxFirstTCB;     /* ָ���б��һ��������ƿ��ָ�루���ڼ��ѭ�������� */
    UBaseType_t uxTask = 0;         /* ���������Ѵ������������ */

    /* ����б��Ƿ���������б��ȴ���0�� */
    if( listCURRENT_LIST_LENGTH( pxList ) > ( UBaseType_t ) 0 )
    {
        /* ��ȡ�б��еĵ�һ��������ƿ飬��Ϊѭ�������ı�� */
        listGET_OWNER_OF_NEXT_ENTRY( pxFirstTCB, pxList );

        /* �����б��е�ÿ�������������״̬���� */
        do
        {
            /* ��ȡ�б��е���һ��������ƿ� */
            listGET_OWNER_OF_NEXT_ENTRY( pxNextTCB, pxList );
            
            /* ��ȡ�������ϸ��Ϣ����䵽����״̬������ */
            vTaskGetInfo( ( TaskHandle_t ) pxNextTCB,            /* ������ */
                         &( pxTaskStatusArray[ uxTask ] ),       /* ����״̬�ṹָ�� */
                         pdTRUE,                                 /* ��������ʱͳ����Ϣ */
                         eState );                               /* ����״̬ */
            
            /* �����Ѵ������������ */
            uxTask++;
            
        } while( pxNextTCB != pxFirstTCB );  /* ѭ��ֱ���ص���һ������ */
    }
    else
    {
        /* �б�Ϊ�գ���Ӳ��Ը����ʱ�� */
        mtCOVERAGE_TEST_MARKER();
    }

    /* ���سɹ���������״̬�ṹ���� */
    return uxTask;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) )

  /*******************************************************************************
  * �������ƣ�prvTaskCheckFreeStackSpace
  * �����������������ջ�Ŀ��пռ��С��ͨ��ͳ����������ֽڵ���������ʣ��ջ�ռ�
  *           �˺�����uxTaskGetStackHighWaterMark���ڲ�ʵ�֣����ڼ���ջ��ˮλ���
  * ���������
  *   - pucStackByte: ָ��ջ�����ʼλ�õ�ָ�룬ͨ����ջ�Ľ���λ��
  *     ע���������ָ����Ч��ջ�ڴ�����
  * �����������
  * �� �� ֵ��
  *   - uint16_t: ����ջ��ʣ��ռ��С����StackType_tΪ��λ��
  *     ��ʾ�Ӽ����ʼλ�õ���һ��������ֽ�֮��Ŀ���ջ�ռ�
  * ����˵����
  *   - �˺����������ø��ٹ��ܻ�ջ��ˮλ��ǹ���ʱ����
  *   - ͨ�����������tskSTACK_FILL_BYTE����ֽ����������ջ�ռ�
  *   - �����StackType_tΪ��λ������ֱ������ջ�ռ����
  * 
  * �޸�����      �汾��          �޸���            �޸�����
  * -----------------------------------------------------------------------------
  * 2025/09/03     V1.00          DeepSeek          ������ע��
  *******************************************************************************/
  static uint16_t prvTaskCheckFreeStackSpace( const uint8_t * pucStackByte )
  {
      uint32_t ulCount = 0U;  /* ��������ͳ������������ֽ����� */

      /* ����ջ�ڴ棬ͳ�ƴ���ʼλ�ÿ�ʼ����������ֽ����� */
      while( *pucStackByte == ( uint8_t ) tskSTACK_FILL_BYTE )
      {
          /* ����ջ���������ƶ�ָ�룺������������������������ */
          pucStackByte -= portSTACK_GROWTH;
          /* ���Ӽ�������ÿ�ҵ�һ������ֽڼ�����1 */
          ulCount++;
      }

      /* ���ֽڼ���ת��ΪStackType_t��λ����������StackType_t�Ĵ�С */
      ulCount /= ( uint32_t ) sizeof( StackType_t ); /*lint !e961 Casting is not redundant on smaller architectures. */

      /* ���ؼ���õ��Ŀ���ջ�ռ��С����StackType_tΪ��λ�� */
      return ( uint16_t ) ulCount;
  }

#endif /* ( ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) ) */
/*-----------------------------------------------------------*/

#if ( INCLUDE_uxTaskGetStackHighWaterMark == 1 )

/*******************************************************************************
 * �������ƣ�uxTaskGetStackHighWaterMark
 * ������������ȡָ�������ջ��ˮλ��ǣ����������й�����ջ�ռ����Сʣ����
 *           �˺������ڼ������ջʹ�������Ԥ��ջ�������
 * ���������
 *   - xTask: ��������ָ��Ҫ��������
 *     ����ֵ������NULL��ʾ��鵱ǰ����
 * �����������
 * �� �� ֵ��
 *   - UBaseType_t: ����ջ�ĸ�ˮλ���ֵ����λȡ���ھ���ʵ�֣�ͨ�����ֽ�����
 *     ����ֵԽС��ʾջʹ����Խ�ߣ�0��ʾջ������ӽ���
 * ����˵����
 *   - �˺�������INCLUDE_uxTaskGetStackHighWaterMarkΪ1ʱ����
 *   - ��ˮλ��Ǳ�ʾ�������й�����ջ�ռ����Сʣ����
 *   - �����ڼ������ջʹ�������Ԥ��ջ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          DeepSeek          ������ע��
 *******************************************************************************/
UBaseType_t uxTaskGetStackHighWaterMark( TaskHandle_t xTask )
{
    TCB_t *pxTCB;                   /* ָ��������ƿ��ָ�� */
    uint8_t *pucEndOfStack;         /* ָ��ջ����λ�õ�ָ�� */
    UBaseType_t uxReturn;           /* ����ֵ����ˮλ��� */

    /* ������������ȡ������ƿ�(TCB) */
    pxTCB = prvGetTCBFromHandle( xTask );

    /* ����ջ��������ȷ��ջ����λ�� */
    #if portSTACK_GROWTH < 0
    {
        /* ����������ջ��ջ����λ����ջ�������ʼ��ַ */
        pucEndOfStack = ( uint8_t * ) pxTCB->pxStack;
    }
    #else
    {
        /* ����������ջ��ջ����λ����ջ����Ľ�����ַ */
        pucEndOfStack = ( uint8_t * ) pxTCB->pxEndOfStack;
    }
    #endif

    /* ���㲢����ջ�ռ�ĸ�ˮλ��� */
    uxReturn = ( UBaseType_t ) prvTaskCheckFreeStackSpace( pucEndOfStack );

    /* ���ظ�ˮλ���ֵ */
    return uxReturn;
}

#endif /* INCLUDE_uxTaskGetStackHighWaterMark */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskDelete == 1 )

/*******************************************************************************
 * �������ƣ�prvDeleteTCB
 * ����������ɾ��������������ƿ�(TCB)�����������ͷŶ�̬������ڴ����̬������Դ
 *           1. ִ�ж˿��ض���TCB�������
 *           2. ����NewLib������ṹ�Ļ��գ�������ã�
 *           3. ��������ķ��䷽ʽ����̬/��̬����ȫ�ͷ��ڴ�
 * ���������
 *   - pxTCB: ָ��Ҫɾ����������ƿ�(TCB)��ָ��
 *     ע���������ָ����Ч��TCB�ṹ�����ú�ָ�뽫������
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺�������INCLUDE_vTaskDeleteΪ1ʱ���룬��vTaskDelete���ڲ�ʵ��
 *   - ����FreeRTOS����ѡ���ͬ���ڴ���䷽��
 *   - ��̬������ڴ治���ڴ˺������ͷţ���Ҫ�ɷ����߹���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Qiguo_Cui          ����
 *******************************************************************************/  
static void prvDeleteTCB( TCB_t *pxTCB )
{
    /* �˿��ض���������������TriCore�ȶ˿���Ҫ�����TCB������
       �˺�������ͷ��ڴ�ǰ���ã�Ҳ���ܱ������˿ڻ���ʾ�������ھ�̬�ڴ����� */
    portCLEAN_UP_TCB( pxTCB );

    /* ���ʹ��NewLib������Ϊ�����룬��Ҫ�������������_reent�ṹ */
    #if ( configUSE_NEWLIB_REENTRANT == 1 )
    {
        /* ����NewLib�Ŀ�����ṹ����Դ����ֹ�ڴ�й© */
        _reclaim_reent( &( pxTCB->xNewLib_reent ) );
    }
    #endif /* configUSE_NEWLIB_REENTRANT */

    /* ����̬��������˫�ط�������Ե�������޾�̬���䡢��MPU��װ�� */
    #if( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 0 ) && ( portUSING_MPU_WRAPPERS == 0 ) )
    {
        /* ������ȫ��̬���䣺ͬʱ�ͷ�����ջ��TCB�ڴ� */
        vPortFree( pxTCB->pxStack );  /* �ͷ�����ջ�ռ� */
        vPortFree( pxTCB );           /* �ͷ�������ƿ��ڴ� */
    }
    /* ����̬�Ͷ�̬���乲������ */
    #elif( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE == 1 )
    {
        /* ����TCB�еķ����־ucStaticallyAllocated�ж��ڴ���䷽ʽ */
        if( pxTCB->ucStaticallyAllocated == tskDYNAMICALLY_ALLOCATED_STACK_AND_TCB )
        {
            /* ջ��TCB���Ƕ�̬���䣺��Ҫ�ͷ����� */
            vPortFree( pxTCB->pxStack );  /* �ͷŶ�̬���������ջ */
            vPortFree( pxTCB );           /* �ͷŶ�̬�����TCB */
        }
        else if( pxTCB->ucStaticallyAllocated == tskSTATICALLY_ALLOCATED_STACK_ONLY )
        {
            /* ��ջ�Ǿ�̬���䣺ֻ�ͷŶ�̬�����TCB��ջ�ڴ���ϵͳ���� */
            vPortFree( pxTCB );           /* ���ͷ�TCB�ڴ� */
        }
        else
        {
            /* ջ��TCB���Ǿ�̬���䣺����Ҫ�ͷ��κ��ڴ棬
               ʹ��configASSERT��֤�����־����ȷ�� */
            configASSERT( pxTCB->ucStaticallyAllocated == tskSTATICALLY_ALLOCATED_STACK_AND_TCB );
            /* �����ʱ�ǣ����ڲ��Ը��Ƿ��� */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
}

#endif /* INCLUDE_vTaskDelete */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvResetNextTaskUnblockTime
 * ����������������һ������������ʱ����ڲ�����
 *           �����ӳ������б��״̬������һ������������ʱ�䣬�����Ż�����������
 * ���������void - ���������
 * �����������
 * ����ֵ����
 * ����˵����
 *   - �˺�����FreeRTOS���������ڲ������������⹫��
 *   - ����ά��xNextTaskUnblockTime�������ñ�����¼������Ҫ�������������ʱ��
 *   - ���ӳ������б����仯ʱ���ô˺�����ȷ��xNextTaskUnblockTime��������
 *   - �Ż����������ܣ����ٲ���Ҫ���б�����ͼ��
 *   - �����ӳ������б�Ϊ�պͷǿ��������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
static void prvResetNextTaskUnblockTime( void )
{
    /* ָ��������ƿ��ָ�� */
    TCB_t *pxTCB;

    /* ����ӳ������б��Ƿ�Ϊ�� */
    if( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )
    {
        /* �µĵ�ǰ�ӳ��б�Ϊ�ա���xNextTaskUnblockTime����Ϊ������ֵ��
           �������ӳ��б�������Ŀ֮ǰ��if(xTickCount >= xNextTaskUnblockTime)����
           ��������ͨ�� */
        xNextTaskUnblockTime = portMAX_DELAY;
    }
    else
    {
        /* �µĵ�ǰ�ӳ��б�Ϊ�գ���ȡ�ӳ��б�ͷ����Ŀ��ֵ
           �����ӳ��б�ͷ������Ӧ�ô�����״̬�Ƴ���ʱ�� */
        ( pxTCB ) = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList );
        xNextTaskUnblockTime = listGET_LIST_ITEM_VALUE( &( ( pxTCB )->xStateListItem ) );
    }
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xTaskGetCurrentTaskHandle
 * ������������ȡ��ǰ�������е������������
 *           �˺������ڻ�ȡ��ǰִ�������������ƿ�ָ�룬��������ʶ�����������
 * ���������void - ���������
 * �����������
 * ����ֵ��
 *   - TaskHandle_t: ���ص�ǰ�������е������������
 *                   ָ��ǰ�����������ƿ飨TCB����ָ��
 * ����˵����
 *   - �˺�����INCLUDE_xTaskGetCurrentTaskHandleΪ1��configUSE_MUTEXESΪ1ʱ����
 *   - ����Ҫ�ٽ�����������Ϊ�������ж��е��ã��ҵ�ǰTCB���κε���ִ���̶߳�����ͬ��
 *   - �ṩ��һ�ּ򵥵ķ�ʽ�������ȡ����ľ����ǰ��������ľ��
 *   - ������������ʶ�𡢵��Ժ������ͨ�ŷǳ�����
 *   - ����ִ�м򵥿��٣�����������Ӱ��ϵͳ����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
#if ( ( INCLUDE_xTaskGetCurrentTaskHandle == 1 ) || ( configUSE_MUTEXES == 1 ) )

TaskHandle_t xTaskGetCurrentTaskHandle( void )
{
    /* �洢�������ؽ�� */
    TaskHandle_t xReturn;

    /* ����Ҫ�ٽ�������Ϊ�ⲻ�Ǵ��жϵ��õģ�
       ���ҵ�ǰTCB�����κε�����ִ���̶߳�����ͬ�� */
    xReturn = pxCurrentTCB;

    /* ���ص�ǰ������ */
    return xReturn;
}

#endif /* ( ( INCLUDE_xTaskGetCurrentTaskHandle == 1 ) || ( configUSE_MUTEXES == 1 ) ) */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xTaskGetSchedulerState
 * ������������ȡ��ǰ������״̬�ĺ���
 *           �˺������ڲ�ѯFreeRTOS�������ĵ�ǰ����״̬������δ�����������к͹�������״̬
 * ���������void - ���������
 * �����������
 * ����ֵ��
 *   - BaseType_t: ���ص�ǰ��������״̬
 *                 taskSCHEDULER_NOT_STARTED ��ʾ������δ����
 *                 taskSCHEDULER_RUNNING ��ʾ��������������
 *                 taskSCHEDULER_SUSPENDED ��ʾ�������ѹ���
 * ����˵����
 *   - �˺�����INCLUDE_xTaskGetSchedulerStateΪ1��configUSE_TIMERSΪ1ʱ����
 *   - �ṩ��һ�ְ�ȫ�ķ�ʽ��ѯ������״̬������Ӧ�ó������״̬��ȡ��Ӧ����
 *   - ������״̬���ڶ�ʱ������������ͬ���ǳ���Ҫ
 *   - ����ִ�м򵥿��٣�����������Ӱ��ϵͳ����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
#if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )

BaseType_t xTaskGetSchedulerState( void )
{
    /* �洢�������ؽ�� */
    BaseType_t xReturn;

    /* ���������Ƿ�δ���� */
    if( xSchedulerRunning == pdFALSE )
    {
        /* ���ص�����δ����״̬ */
        xReturn = taskSCHEDULER_NOT_STARTED;
    }
    else
    {
        /* ���������Ƿ�δ���� */
        if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
        {
            /* ���ص�����������״̬ */
            xReturn = taskSCHEDULER_RUNNING;
        }
        else
        {
            /* ���ص���������״̬ */
            xReturn = taskSCHEDULER_SUSPENDED;
        }
    }

    /* ���ص�����״̬ */
    return xReturn;
}

#endif /* ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) ) */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�vTaskPriorityInherit
 * �����������������ȼ��̳е��ڲ����������ڴ������ź�����ȡʱ�����ȼ�����
 *           �������ȼ������Ի�ȡ�ѱ������ȼ�������еĻ����ź���ʱ��
 *           �˺������𽫵����ȼ���������ȼ�������������ȼ�������ͬ��
 *           �Է�ֹ���ȼ���ת����
 * ���������
 *   - pxMutexHolder: �����ź��������ߵ���������ָ����Ҫ�������ȼ����������
 * �����������
 * ����ֵ����
 * ����˵����
 *   - �˺�������configUSE_MUTEXESΪ1ʱ���룬�ǻ����ź������ܵ�һ����
 *   - �������ȼ��̳л��ƣ���ֹ���ȼ���ת����
 *   - �������ź��������ߵ����ȼ����ڳ��Ի�ȡ�����ź���������ʱ����ʱ�������������ȼ�
 *   - ��������ľ����б�״̬��ȷ���������ܹ���ȷ����
 *   - �ṩ���ٹ��ܣ����ڵ��Ժ����ܷ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
#if ( configUSE_MUTEXES == 1 )

void vTaskPriorityInherit( TaskHandle_t const pxMutexHolder )
{
    /* �������ź��������߾��ת��Ϊ������ƿ�ָ�� */
    TCB_t * const pxTCB = ( TCB_t * ) pxMutexHolder;

    /* ��������ź����ڶ�������ʱ���жϷ��أ��򻥳��ź������������ڿ���ΪNULL */
    if( pxMutexHolder != NULL )
    {
        /* ��������ź��������ߵ����ȼ����ڳ��Ի�ȡ�����ź�������������ȼ���
           ��������ʱ�̳г��Ի�ȡ�����ź�������������ȼ� */
        if( pxTCB->uxPriority < pxCurrentTCB->uxPriority )
        {
            /* ���������ź���������״̬����Ӧ�������ȼ�
               �����¼��б���ֵδ���������κ���;ʱ�������� */
            if( ( listGET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ) ) & taskEVENT_LIST_ITEM_VALUE_IN_USE ) == 0UL )
            {
                /* �����¼��б���ֵΪ���������ȼ��ļ���ֵ */
                listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) pxCurrentTCB->uxPriority ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
            }
            else
            {
                /* ���븲�ǲ��Ա�ǣ��¼��б���ֵ����ʹ�õ������ */
                mtCOVERAGE_TEST_MARKER();
            }

            /* ������޸ĵ������ھ���״̬������Ҫ�����ƶ������б��� */
            if( listIS_CONTAINED_WITHIN( &( pxReadyTasksLists[ pxTCB->uxPriority ] ), &( pxTCB->xStateListItem ) ) != pdFALSE )
            {
                /* �ӵ�ǰ�����б����Ƴ����� */
                if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
                {
                    /* ��������ȼ������б�Ϊ�գ����þ������ȼ�λͼ */
                    taskRESET_READY_PRIORITY( pxTCB->uxPriority );
                }
                else
                {
                    /* ���븲�ǲ��Ա�ǣ������ھ����б��е������ */
                    mtCOVERAGE_TEST_MARKER();
                }

                /* ���ƶ������б�֮ǰ�̳����ȼ� */
                pxTCB->uxPriority = pxCurrentTCB->uxPriority;
                /* ��������ӵ������ȼ��ľ����б� */
                prvAddTaskToReadyList( pxTCB );
            }
            else
            {
                /* ���̳����ȼ��������������б� */
                pxTCB->uxPriority = pxCurrentTCB->uxPriority;
            }

            /* ��¼���ȼ��̳еĸ�����Ϣ */
            traceTASK_PRIORITY_INHERIT( pxTCB, pxCurrentTCB->uxPriority );
        }
        else
        {
            /* ���븲�ǲ��Ա�ǣ����������ȼ������ڵ�ǰ���������� */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    else
    {
        /* ���븲�ǲ��Ա�ǣ������ź���������Ϊ�յ������ */
        mtCOVERAGE_TEST_MARKER();
    }
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xTaskPriorityDisinherit
 * �����������������ȼ�ȡ���̳е��ڲ����������ڴ������ź����ͷ�ʱ�����ȼ��ָ�
 *           �������ͷŻ����ź���ʱ����������������ȼ��̳л��Ʊ����������ȼ���
 *           �˺������������ȼ��ָ�Ϊ�������ȼ�����������صľ����б����
 * ���������
 *   - pxMutexHolder: �����ź��������ߵ���������ָ����Ҫȡ�����ȼ��̳е��������
 * �����������
 * ����ֵ��
 *   - BaseType_t: �����Ƿ���Ҫ�������л��ı�־
 *                 pdTRUE��ʾ��Ҫ�������л���pdFALSE��ʾ����Ҫ�������л�
 * ����˵����
 *   - �˺�������configUSE_MUTEXESΪ1ʱ���룬�ǻ����ź������ܵ�һ����
 *   - �������ȼ��̳л��Ƶ�ȡ������ֹ���ȼ���ת����
 *   - ȷ���������ͷŻ����ź�����ָ���ԭʼ���ȼ�
 *   - ��������ľ����б�״̬��ȷ���������ܹ���ȷ����
 *   - ֻ�����л����ź������ͷź��ȡ�����ȼ��̳�
 *   - �ṩ���ٹ��ܣ����ڵ��Ժ����ܷ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
#if ( configUSE_MUTEXES == 1 )

BaseType_t xTaskPriorityDisinherit( TaskHandle_t const pxMutexHolder )
{
    /* �������ź��������߾��ת��Ϊ������ƿ�ָ�� */
    TCB_t * const pxTCB = ( TCB_t * ) pxMutexHolder;
    /* �洢�������ؽ����Ĭ��Ϊ����Ҫ�������л� */
    BaseType_t xReturn = pdFALSE;

    /* ��黥���ź����������Ƿ���Ч */
    if( pxMutexHolder != NULL )
    {
        /* ����ֻ���ڳ��л����ź���ʱ�����м̳е����ȼ�
           ��������ź�����������У����ܴ��ж��и�����
           ��������ź����ɳ��������������������������״̬���� */
        configASSERT( pxTCB == pxCurrentTCB );

        /* ����ȷ������ȷʵ���л����ź��� */
        configASSERT( pxTCB->uxMutexesHeld );
        /* ����������еĻ����ź������� */
        ( pxTCB->uxMutexesHeld )--;

        /* �����ź����������Ƿ�̳�����һ����������ȼ��� */
        if( pxTCB->uxPriority != pxTCB->uxBasePriority )
        {
            /* ֻ����û�����������ź�������ʱ��ȡ���̳� */
            if( pxTCB->uxMutexesHeld == ( UBaseType_t ) 0 )
            {
                /* ����ֻ���ڳ��л����ź���ʱ�����м̳е����ȼ�
                   ��������ź�����������У����ܴ��ж��и�����
                   ��������ź����ɳ��������������������������״̬����
                   �Ӿ����б����Ƴ��������� */
                if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
                {
                    /* ���þ������ȼ�λͼ�еĶ�Ӧλ */
                    taskRESET_READY_PRIORITY( pxTCB->uxPriority );
                }
                else
                {
                    /* ���븲�ǲ��Ա�ǣ������ھ����б��е������ */
                    mtCOVERAGE_TEST_MARKER();
                }

                /* �ڽ�������ӵ��¾����б�֮ǰȡ�����ȼ��̳� */
                traceTASK_PRIORITY_DISINHERIT( pxTCB, pxTCB->uxBasePriority );
                /* ���������ȼ��ָ�Ϊ�������ȼ� */
                pxTCB->uxPriority = pxTCB->uxBasePriority;

                /* �����¼��б���ֵ������������������У��������������κ�����Ŀ�ģ�
                   �����������������в��ܷ��ػ����ź��� */
                listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) pxTCB->uxPriority ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
                /* ��������ӵ������б� */
                prvAddTaskToReadyList( pxTCB );

                /* ����true��ʾ��Ҫ�������л�
                   ��ʵ����ֻ�ڼ����������Ҫ�������ж�������ź�������
                   �����ź����Բ�ͬ�ڻ�ȡ˳���˳�򷵻�
                   ����ڷ��ص�һ�������ź���ʱû�з����������л�����ʹ�������ڵȴ�����
                   ��ô�����һ�������ź�������ʱ�������Ƿ�������ȴ�����Ӧ�����������л� */
                xReturn = pdTRUE;
            }
            else
            {
                /* ���븲�ǲ��Ա�ǣ��������������ź������е������ */
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            /* ���븲�ǲ��Ա�ǣ����ȼ��ѵ��ڻ������ȼ�������� */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    else
    {
        /* ���븲�ǲ��Ա�ǣ������ź���������Ϊ�յ������ */
        mtCOVERAGE_TEST_MARKER();
    }

    /* �����Ƿ���Ҫ�������л��ı�־ */
    return xReturn;
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�vTaskEnterCritical
 * ���������������ٽ���������汾���������ڱ����ؼ�����������жϺ������������
 *           �˺��������жϲ����ӵ�ǰ������ٽ���Ƕ�׼�����ȷ���ؼ�����ε�ԭ����ִ��
 * ���������void - ���������
 * �����������
 * ����ֵ����
 * ����˵����
 *   - �˺�������portCRITICAL_NESTING_IN_TCBΪ1ʱ���룬��ʾ�ٽ���Ƕ�׼����洢��TCB��
 *   - ���жϰ�ȫ�汾���������жϷ�������е���
 *   - ʹ��portDISABLE_INTERRUPTS()�����ж�
 *   - ���ӵ�ǰ������ٽ���Ƕ�׼�����֧��Ƕ���ٽ���
 *   - ����Ƿ����ж��������е��ã�����Ǵ��жϵ��ûᴥ������
 *   - ֻ���ڵ��������к�Ž���Ƕ�׼�������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
#if ( portCRITICAL_NESTING_IN_TCB == 1 )

void vTaskEnterCritical( void )
{
    /* �����жϣ���ʼ�ٽ��� */
    portDISABLE_INTERRUPTS();

    /* ���������Ƿ��������� */
    if( xSchedulerRunning != pdFALSE )
    {
        /* ���ӵ�ǰ������ٽ���Ƕ�׼��� */
        ( pxCurrentTCB->uxCriticalNesting )++;

        /* �ⲻ���жϰ�ȫ�汾�Ľ����ٽ������������������ж������ĵ������ᴥ������
           ֻ����"FromISR"��β��API�����������ж���ʹ��
           ֻ�е��ٽ�Ƕ�׼���Ϊ1ʱ�Ŷ��ԣ��Է�ֹ������Ժ���Ҳʹ���ٽ���ʱ�ĵݹ���� */
        if( pxCurrentTCB->uxCriticalNesting == 1 )
        {
            /* ����Ƿ����ж��е��ã�������򴥷����� */
            portASSERT_IF_IN_ISR();
        }
    }
    else
    {
        /* ���븲�ǲ��Ա�ǣ�������δ���е������ */
        mtCOVERAGE_TEST_MARKER();
    }
}

#endif /* portCRITICAL_NESTING_IN_TCB */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�vTaskExitCritical
 * �����������˳��ٽ���������汾���������ڽ����ؼ�����εı���
 *           �˺������ٵ�ǰ������ٽ���Ƕ�׼�������Ƕ�׼���Ϊ��ʱ����ʹ���ж�
 * ���������void - ���������
 * �����������
 * ����ֵ����
 * ����˵����
 *   - �˺�������portCRITICAL_NESTING_IN_TCBΪ1ʱ���룬��ʾ�ٽ���Ƕ�׼����洢��TCB��
 *   - ���жϰ�ȫ�汾���������жϷ�������е���
 *   - ���ٵ�ǰ������ٽ���Ƕ�׼�����֧��Ƕ���ٽ���
 *   - ��Ƕ�׼���Ϊ��ʱʹ��portENABLE_INTERRUPTS()����ʹ���ж�
 *   - ֻ���ڵ��������к�Ž���Ƕ�׼�������
 *   - �ṩ��ȫ��Ƕ���ٽ����˳����ƣ�ȷ���ж�ֻ������Ƕ���ٽ������˳�������ʹ��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
#if ( portCRITICAL_NESTING_IN_TCB == 1 )

void vTaskExitCritical( void )
{
    /* ���������Ƿ��������� */
    if( xSchedulerRunning != pdFALSE )
    {
        /* ����ٽ���Ƕ�׼����Ƿ����0 */
        if( pxCurrentTCB->uxCriticalNesting > 0U )
        {
            /* ���ٵ�ǰ������ٽ���Ƕ�׼��� */
            ( pxCurrentTCB->uxCriticalNesting )--;

            /* ���Ƕ�׼�������0������ʹ���ж� */
            if( pxCurrentTCB->uxCriticalNesting == 0U )
            {
                portENABLE_INTERRUPTS();
            }
            else
            {
                /* ���븲�ǲ��Ա�ǣ�Ƕ�׼����Դ���0������� */
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            /* ���븲�ǲ��Ա�ǣ�Ƕ�׼����Ѿ�Ϊ0������� */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    else
    {
        /* ���븲�ǲ��Ա�ǣ�������δ���е������ */
        mtCOVERAGE_TEST_MARKER();
    }
}

#endif /* portCRITICAL_NESTING_IN_TCB */
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )

	static char *prvWriteNameToBuffer( char *pcBuffer, const char *pcTaskName )
	{
	size_t x;

		/* Start by copying the entire string. */
		strcpy( pcBuffer, pcTaskName );

		/* Pad the end of the string with spaces to ensure columns line up when
		printed out. */
		for( x = strlen( pcBuffer ); x < ( size_t ) ( configMAX_TASK_NAME_LEN - 1 ); x++ )
		{
			pcBuffer[ x ] = ' ';
		}

		/* Terminate. */
		pcBuffer[ x ] = 0x00;

		/* Return the new end of string. */
		return &( pcBuffer[ x ] );
	}

#endif /* ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�vTaskList
 * ������������ȡϵͳ�����������״̬��Ϣ����ʽ��Ϊ�ɶ��ַ������ڲ�����
 *           �˺�������ϵͳ�������������ϸ��Ϣ�б������������ơ�״̬�����ȼ���
 *           ջ��ˮλ�ߺ������ţ����������ʽ��Ϊ�����ʽд���ṩ�Ļ�����
 * ���������
 *   - pcWriteBuffer: ָ���ַ���������ָ�룬���ڴ洢��ʽ����������б���Ϣ�ַ���
 * ���������
 *   - pcWriteBuffer: д���ʽ����������б���Ϣ�ַ���
 * ����ֵ����
 * ����˵����
 *   - �˺�������configUSE_TRACE_FACILITY��configUSE_STATS_FORMATTING_FUNCTIONS����ʱ����
 *   - ʹ�ö�̬�ڴ�����ȡ����״̬�����ڴ棬��Ҫȷ��ϵͳ֧�ֶ�̬����
 *   - ��������ɶ��ı����ʾÿ�������״̬�����ȼ���ջʹ�������������
 *   - ʹ��sprintf������ʽ����������ܻ����Ӵ����С��ջʹ����
 *   - ��������ϵͳֱ�ӵ���uxTaskGetSystemState()��ȡԭʼͳ������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )

void vTaskList( char * pcWriteBuffer )
{
    /* ָ������״̬�����ָ�� */
    TaskStatus_t *pxTaskStatusArray;
    /* �����С��ѭ������ */
    volatile UBaseType_t uxArraySize, x;
    /* ����״̬�ַ���ʾ */
    char cStatus;

    /*
     * ��ע�⣺
     *
     * �˺�����Ϊ������ṩ�������ʾӦ�ó���ʹ��������Ҫ������Ϊ���ȳ����һ���֡�
     *
     * vTaskList()����uxTaskGetSystemState()��Ȼ��uxTaskGetSystemState()�����һ����
     * ��ʽ��Ϊ����ɶ��ı����ʾ�������ơ�״̬��ջʹ�������
     *
     * vTaskList()������sprintf() C�⺯�������ܻ����Ӵ����С��ʹ�ô�����ջ��
     * ���ڲ�ͬƽ̨���ṩ��ͬ�Ľ���������FreeRTOS/Demo��Ŀ¼�У��ṩ��һ������ġ�С�͵ġ�
     * �������ġ��������޵�sprintf()ʵ�֣�λ����Ϊprintf-stdarg.c���ļ���
     * ��ע��printf-stdarg.c���ṩ������snprintf()ʵ�֣�����
     *
     * ��������ϵͳֱ�ӵ���uxTaskGetSystemState()����ȡԭʼͳ�����ݣ������Ǽ��ͨ������vTaskList()��
     */

    /* ȷ��д�뻺�����������ַ������Կ��ַ���ͷ�� */
    *pcWriteBuffer = 0x00;

    /* ��ȡ���������Ŀ��գ��Է��˺���ִ���ڼ��������������仯 */
    uxArraySize = uxCurrentNumberOfTasks;

    /* Ϊÿ�������������������ע�⣡���configSUPPORT_DYNAMIC_ALLOCATION����Ϊ0����pvPortMalloc()������NULL */
    pxTaskStatusArray = pvPortMalloc( uxCurrentNumberOfTasks * sizeof( TaskStatus_t ) );

    /* ����ڴ�����Ƿ�ɹ� */
    if( pxTaskStatusArray != NULL )
    {
        /* ���ɣ������ƣ����ݣ�����Ҫ������ʱ�� */
        uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, NULL );

        /* �Ӷ��������ݴ�������ɶ��ı�� */
        for( x = 0; x < uxArraySize; x++ )
        {
            /* ��������ǰ״̬����״̬�ַ� */
            switch( pxTaskStatusArray[ x ].eCurrentState )
            {
                case eReady:        /* ����״̬ */
                    cStatus = tskREADY_CHAR;
                    break;

                case eBlocked:        /* ����״̬ */
                    cStatus = tskBLOCKED_CHAR;
                    break;

                case eSuspended:    /* ����״̬ */
                    cStatus = tskSUSPENDED_CHAR;
                    break;

                case eDeleted:        /* ɾ��״̬ */
                    cStatus = tskDELETED_CHAR;
                    break;

                default:            /* ��Ӧ�õ���������������Է�ֹ��̬������ */
                    cStatus = 0x00;
                    break;
            }

            /* ����������д���ַ������ÿո�����Ա�������Ա����ʽ��ӡ */
            pcWriteBuffer = prvWriteNameToBuffer( pcWriteBuffer, pxTaskStatusArray[ x ].pcTaskName );

            /* д���ַ��������ಿ�֣�״̬�����ȼ���ջ��ˮλ�ߡ������ţ� */
            sprintf( pcWriteBuffer, "\t%c\t%u\t%u\t%u\r\n", cStatus, ( unsigned int ) pxTaskStatusArray[ x ].uxCurrentPriority, ( unsigned int ) pxTaskStatusArray[ x ].usStackHighWaterMark, ( unsigned int ) pxTaskStatusArray[ x ].xTaskNumber );
            /* �ƶ�������ָ�뵽�ַ���ĩβ */
            pcWriteBuffer += strlen( pcWriteBuffer );
        }

        /* �ٴ��ͷ����顣ע�⣡���configSUPPORT_DYNAMIC_ALLOCATIONΪ0����vPortFree()��������Ϊ�� */
        vPortFree( pxTaskStatusArray );
    }
    else
    {
        /* ���븲�ǲ��Ա�ǣ��ڴ����ʧ�ܵ������ */
        mtCOVERAGE_TEST_MARKER();
    }
}

#endif /* ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) ) */
/*----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�vTaskGetRunTimeStats
 * ������������ȡ��������ʱͳ����Ϣ����ʽ��Ϊ�ɶ��ַ������ڲ�����
 *           �˺�������ϵͳ���������������ʱͳ����Ϣ����������ʱ�����ֵ�Ͱٷֱ�
 *           ���������ʽ��Ϊ�����ʽд���ṩ�Ļ�����
 * ���������
 *   - pcWriteBuffer: ָ���ַ���������ָ�룬���ڴ洢��ʽ�����ͳ����Ϣ�ַ���
 * ���������
 *   - pcWriteBuffer: д���ʽ�����ͳ����Ϣ�ַ���
 * ����ֵ����
 * ����˵����
 *   - �˺�������configGENERATE_RUN_TIME_STATS��configUSE_STATS_FORMATTING_FUNCTIONS����ʱ����
 *   - ������configUSE_TRACE_FACILITY���ܣ���������Ϊ1
 *   - ʹ�ö�̬�ڴ�����ȡ����״̬���飬��Ҫȷ��ϵͳ֧�ֶ�̬����
 *   - ��������ɶ��ı����ʾÿ������������״̬�»��ѵ�ʱ��
 *   - ʹ��sprintf������ʽ����������ܻ����Ӵ����С��ջʹ����
 *   - ��������ϵͳֱ�ӵ���uxTaskGetSystemState()��ȡԭʼͳ������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
#if ( ( configGENERATE_RUN_TIME_STATS == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )

void vTaskGetRunTimeStats( char *pcWriteBuffer )
{
    /* ָ������״̬�����ָ�� */
    TaskStatus_t *pxTaskStatusArray;
    /* �����С��ѭ������ */
    volatile UBaseType_t uxArraySize, x;
    /* ������ʱ��Ͱٷֱ�ͳ�� */
    uint32_t ulTotalTime, ulStatsAsPercentage;

    /* ����Ƿ������˸��ٹ��� */
    #if( configUSE_TRACE_FACILITY != 1 )
    {
        /* ��������configUSE_TRACE_FACILITYΪ1����ʹ��vTaskGetRunTimeStats() */
        #error configUSE_TRACE_FACILITY must also be set to 1 in FreeRTOSConfig.h to use vTaskGetRunTimeStats().
    }
    #endif

    /*
     * ��ע�⣺
     *
     * �˺�����Ϊ������ṩ�������ʾӦ�ó���ʹ��������Ҫ������Ϊ���ȳ����һ���֡�
     *
     * vTaskGetRunTimeStats()����uxTaskGetSystemState()��Ȼ��uxTaskGetSystemState()�����һ����
     * ��ʽ��Ϊ����ɶ��ı����ʾÿ������������״̬�»��ѵ�ʱ�䣨����ֵ�Ͱٷֱȣ���
     *
     * vTaskGetRunTimeStats()������sprintf() C�⺯�������ܻ����Ӵ����С��ʹ�ô�����ջ��
     * ���ڲ�ͬƽ̨���ṩ��ͬ�Ľ���������FreeRTOS/Demo��Ŀ¼�У��ṩ��һ������ġ�С�͵ġ�
     * �������ġ��������޵�sprintf()ʵ�֣�λ����Ϊprintf-stdarg.c���ļ���
     * ��ע��printf-stdarg.c���ṩ������snprintf()ʵ�֣�����
     *
     * ��������ϵͳֱ�ӵ���uxTaskGetSystemState()����ȡԭʼͳ�����ݣ������Ǽ��ͨ������vTaskGetRunTimeStats()��
     */

    /* ȷ��д�뻺�����������ַ������Կ��ַ���ͷ�� */
    *pcWriteBuffer = 0x00;

    /* ��ȡ���������Ŀ��գ��Է��˺���ִ���ڼ��������������仯 */
    uxArraySize = uxCurrentNumberOfTasks;

    /* Ϊÿ�������������������ע�⣡���configSUPPORT_DYNAMIC_ALLOCATION����Ϊ0����pvPortMalloc()������NULL */
    pxTaskStatusArray = pvPortMalloc( uxCurrentNumberOfTasks * sizeof( TaskStatus_t ) );

    /* ����ڴ�����Ƿ�ɹ� */
    if( pxTaskStatusArray != NULL )
    {
        /* ���ɣ������ƣ����� */
        uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, &ulTotalTime );

        /* ���ڰٷֱȼ��� */
        ulTotalTime /= 100UL;

        /* ���������� */
        if( ulTotalTime > 0 )
        {
            /* �Ӷ��������ݴ�������ɶ��ı�� */
            for( x = 0; x < uxArraySize; x++ )
            {
                /* ����ʹ����������ʱ��İٷֱ��Ƕ��٣�
                   �⽫ʼ������ȡ������ӽ���������
                   ulTotalTime�Ѿ�������100 */
                ulStatsAsPercentage = pxTaskStatusArray[ x ].ulRunTimeCounter / ulTotalTime;

                /* ����������д���ַ������ÿո�����Ա�������Ա����ʽ��ӡ */
                pcWriteBuffer = prvWriteNameToBuffer( pcWriteBuffer, pxTaskStatusArray[ x ].pcTaskName );

                /* ���ٷֱ��Ƿ����0 */
                if( ulStatsAsPercentage > 0UL )
                {
                    /* ����Ƿ���Ҫ���޷������ʹ�ӡ˵���� */
                    #ifdef portLU_PRINTF_SPECIFIER_REQUIRED
                    {
                        /* ʹ��%lu��ʽ˵����д������ʱ��Ͱٷֱ� */
                        sprintf( pcWriteBuffer, "\t%lu\t\t%lu%%\r\n", pxTaskStatusArray[ x ].ulRunTimeCounter, ulStatsAsPercentage );
                    }
                    #else
                    {
                        /* sizeof(int) == sizeof(long) ���Կ���ʹ�ý�С��printf()�� */
                        sprintf( pcWriteBuffer, "\t%u\t\t%u%%\r\n", ( unsigned int ) pxTaskStatusArray[ x ].ulRunTimeCounter, ( unsigned int ) ulStatsAsPercentage );
                    }
                    #endif
                }
                else
                {
                    /* �������İٷֱ�Ϊ�㣬���������ĵ�������ʱ������1% */
                    #ifdef portLU_PRINTF_SPECIFIER_REQUIRED
                    {
                        sprintf( pcWriteBuffer, "\t%lu\t\t<1%%\r\n", pxTaskStatusArray[ x ].ulRunTimeCounter );
                    }
                    #else
                    {
                        /* sizeof(int) == sizeof(long) ���Կ���ʹ�ý�С��printf()�� */
                        sprintf( pcWriteBuffer, "\t%u\t\t<1%%\r\n", ( unsigned int ) pxTaskStatusArray[ x ].ulRunTimeCounter );
                    }
                    #endif
                }

                /* �ƶ�������ָ�뵽�ַ���ĩβ */
                pcWriteBuffer += strlen( pcWriteBuffer );
            }
        }
        else
        {
            /* ���븲�ǲ��Ա�ǣ���ʱ��Ϊ0������� */
            mtCOVERAGE_TEST_MARKER();
        }

        /* �ٴ��ͷ����顣ע�⣡���configSUPPORT_DYNAMIC_ALLOCATIONΪ0����vPortFree()��������Ϊ�� */
        vPortFree( pxTaskStatusArray );
    }
    else
    {
        /* ���븲�ǲ��Ա�ǣ��ڴ����ʧ�ܵ������ */
        mtCOVERAGE_TEST_MARKER();
    }
}

#endif /* ( ( configGENERATE_RUN_TIME_STATS == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) ) */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�uxTaskResetEventItemValue
 * �������������������¼��б���ֵ������ԭֵ���ڲ�����
 *           �˺������ڻ�ȡ�����õ�ǰ�����¼��б����ֵ��ʹ��ָ�ΪĬ�����ȼ����ֵ
 * ���������void - ���������
 * �����������
 * ����ֵ��
 *   - TickType_t: ��������ǰ�����¼��б����ֵ
 * ����˵����
 *   - �˺�����Ҫ�����¼����������ͬ��������
 *   - �������¼��б����ֵ����Ϊ�����������ȼ���Ĭ��ֵ
 *   - ȷ���¼��б�������������ڶ��к��ź�������
 *   - ʹ���б���������ȡ������ֵ��ȷ�������ĸ�Ч��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
TickType_t uxTaskResetEventItemValue( void )
{
    /* �洢���ص��¼��б���ԭֵ */
    TickType_t uxReturn;

    /* ��ȡ��ǰ�����¼��б���ĵ�ǰֵ */
    uxReturn = listGET_LIST_ITEM_VALUE( &( pxCurrentTCB->xEventListItem ) );

    /* ���¼��б�������Ϊ������ֵ - �Ա�����������к��ź���һ��ʹ��
       ����ֵ���㹫ʽ��configMAX_PRIORITIES - ��ǰ�������ȼ�
       ����ȷ�������ȼ�������¼��б���ֵ��С�����б�������ǰ�� */
    listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xEventListItem ), ( ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) pxCurrentTCB->uxPriority ) ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */

    /* ��������ǰ���¼��б���ֵ */
    return uxReturn;
}
/*-----------------------------------------------------------*/

#if ( configUSE_MUTEXES == 1 )
/*******************************************************************************
 * �������ƣ�pvTaskIncrementMutexHeldCount
 * ��������������������еĻ����ź����������ڲ�����
 *           �˺����������ӵ�ǰ������еĻ����ź���������������������ƿ�ָ��
 * ���������void - ���������
 * �����������
 * ����ֵ��
 *   - void*: ���ص�ǰ������ƿ�ָ�루TCBָ�룩
 * ����˵����
 *   - �˺�������configUSE_MUTEXESΪ1ʱ���룬�ǻ����ź������ܵ�һ����
 *   - ���ڸ���������еĻ����ź���������֧�����ȼ��̳л���
 *   - ������񴴽�ǰ���ã�pxCurrentTCB����ΪNULL����Ҫ���п�ָ����
 *   - ����������ƿ�ָ�룬���ڵ����߽��к�������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
void *pvTaskIncrementMutexHeldCount( void )
{
    /* ���xSemaphoreCreateMutex()���κ����񴴽�֮ǰ���ã���pxCurrentTCB��ΪNULL */
    if( pxCurrentTCB != NULL )
    {
        /* ���ӵ�ǰ������еĻ����ź������� */
        ( pxCurrentTCB->uxMutexesHeld )++;
    }

    /* ���ص�ǰ������ƿ�ָ�� */
    return pxCurrentTCB;
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�ulTaskNotifyTake
 * ������������ȡ����ֵ֪ͨ�ĺ��ĺ�����ʵ�����Ƽ����ź�����"take"����
 *           �˺�����������ȴ�ֵ֪ͨ��Ϊ���㣬Ȼ���ȡ����ѡ�����ֵ֪ͨ
 * ���������
 *   - xClearCountOnExit: �˳�ʱ��������ķ�ʽ
 *                        pdTRUE��ʾ��ȡ������ֵ֪ͨ�����ƶ������ź�����
 *                        pdFALSE��ʾ��ȡ���1�����Ƽ����ź�����
 *   - xTicksToWait: ���ȴ�ʱ�䣨��ϵͳ����Ϊ��λ��
 *                   ������portMAX_DELAY��ʾ���޵ȴ���0��ʾ���ȴ���������
 * �����������
 * ����ֵ��
 *   - uint32_t: ���ػ�ȡ����ֵ֪ͨ
 *               ����ɹ���ȡ֪ͨ�����ط���ֵ
 *               ����ȴ���ʱ��δ��ȡ��֪ͨ������0
 * ����˵����
 *   - �˺�������configUSE_TASK_NOTIFICATIONSΪ1ʱ���룬������֪ͨ���ܵ�һ����
 *   - �ṩ���Ƽ����ź����Ĺ��ܣ����ȴ�ͳ�ź�������Ч��ռ����Դ����
 *   - ֧���������ģʽ����ȫ���㣨�������ź���ģʽ�����1�������ź���ģʽ��
 *   - ʹ���ٽ�������״̬������ȷ���ڶ����񻷾��µ��̰߳�ȫ��
 *   - �ṩ���ٹ��ܣ����ڵ��Ժ����ܷ���
 *   - ֻ����ֵ֪ͨΪ0ʱ�������������ִ��Ч��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
#if( configUSE_TASK_NOTIFICATIONS == 1 )

uint32_t ulTaskNotifyTake( BaseType_t xClearCountOnExit, TickType_t xTicksToWait )
{
    /* �洢�������ص�ֵ֪ͨ */
    uint32_t ulReturn;

    /* �����ٽ���������״̬������ԭ���� */
    taskENTER_CRITICAL();
    {
        /* ֻ����֪ͨ������δ����ʱ������ */
        if( pxCurrentTCB->ulNotifiedValue == 0UL )
        {
            /* ����������Ϊ�ȴ�֪ͨ */
            pxCurrentTCB->ucNotifyState = taskWAITING_NOTIFICATION;

            /* ���ȴ�ʱ���Ƿ����0 */
            if( xTicksToWait > ( TickType_t ) 0 )
            {
                /* ����ǰ������ӵ��ӳ��б��������������� */
                prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
                /* ��¼����֪ͨ��ȡ�����ĸ�����Ϣ */
                traceTASK_NOTIFY_TAKE_BLOCK();

                /* ���ж˿ڶ���дΪ�������ٽ�section�н���yield
                   ����Щ������yield��������ȴ��ٽ�section�˳���
                   ���ⲻ��Ӧ�ó������Ӧ���������� */
                portYIELD_WITHIN_API();
            }
            else
            {
                /* ���븲�ǲ��Ա�ǣ��ȴ�ʱ��Ϊ0������� */
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            /* ���븲�ǲ��Ա�ǣ�ֵ֪ͨ�ѷ��������� */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    /* �˳��ٽ��� */
    taskEXIT_CRITICAL();

    /* �ٴν����ٽ����������ȡ��� */
    taskENTER_CRITICAL();
    {
        /* ��¼����֪ͨ��ȡ�ĸ�����Ϣ */
        traceTASK_NOTIFY_TAKE();
        /* ��ȡ��ǰ�����ֵ֪ͨ */
        ulReturn = pxCurrentTCB->ulNotifiedValue;

        /* ���ֵ֪ͨ���㣬�������ģʽ����ֵ֪ͨ */
        if( ulReturn != 0UL )
        {
            if( xClearCountOnExit != pdFALSE )
            {
                /* �������ź���ģʽ����ȡ������ֵ֪ͨ */
                pxCurrentTCB->ulNotifiedValue = 0UL;
            }
            else
            {
                /* �����ź���ģʽ����ȡ��ֵ֪ͨ��1 */
                pxCurrentTCB->ulNotifiedValue = ulReturn - 1;
            }
        }
        else
        {
            /* ���븲�ǲ��Ա�ǣ�ֵ֪ͨΪ0������� */
            mtCOVERAGE_TEST_MARKER();
        }

        /* ������֪ͨ״̬����Ϊ"δ�ȴ�֪ͨ" */
        pxCurrentTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION;
    }
    /* �˳��ٽ��� */
    taskEXIT_CRITICAL();

    /* ���ػ�ȡ����ֵ֪ͨ */
    return ulReturn;
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xTaskNotifyWait
 * �����������ȴ�����֪ͨ�ĺ��ĺ������������������ȴ�֪ͨ�����ʱ
 *           �˺����ṩ��һ��������������ͬ�����ƣ��ȴ�ͳ���ź������¼������Ч
 * ���������
 *   - ulBitsToClearOnEntry: ����ȴ�ʱҪ��ֵ֪ͨ�������λ����
 *   - ulBitsToClearOnExit: �˳��ȴ�ʱҪ��ֵ֪ͨ�������λ����
 *   - pulNotificationValue: ָ��ֵ֪ͨ��ָ�룬���ڷ��ؽ��յ���ֵ֪ͨ
 *   - xTicksToWait: ���ȴ�ʱ�䣨��ϵͳ����Ϊ��λ����������portMAX_DELAY��ʾ���޵ȴ�
 * ���������
 *   - pulNotificationValue: �����NULL�����ؽ��յ���ֵ֪ͨ
 * ����ֵ��
 *   - BaseType_t: ���صȴ������Ľ��
 *                 pdTRUE��ʾ�ɹ����յ�֪ͨ
 *                 pdFALSE��ʾ�ȴ���ʱ��δ���յ�֪ͨ
 * ����˵����
 *   - �˺�������configUSE_TASK_NOTIFICATIONSΪ1ʱ���룬������֪ͨ���ܵ�һ����
 *   - �ṩ����λ������ƣ������ڽ�����˳��ȴ�ʱ���ֵ֪ͨ���ض�λ
 *   - ֧�ֳ�ʱ���ƣ�����ָ�����ȴ�ʱ������޵ȴ�
 *   - ʹ���ٽ�������״̬������ȷ���ڶ����񻷾��µ��̰߳�ȫ��
 *   - �ṩ���ٹ��ܣ����ڵ��Ժ����ܷ���
 *   - �ȴ�ͳ���ź������¼������Ч��ռ����Դ����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
#if( configUSE_TASK_NOTIFICATIONS == 1 )

BaseType_t xTaskNotifyWait( uint32_t ulBitsToClearOnEntry, uint32_t ulBitsToClearOnExit, uint32_t *pulNotificationValue, TickType_t xTicksToWait )
{
    /* �洢�������ؽ�� */
    BaseType_t xReturn;

    /* �����ٽ���������״̬������ԭ���� */
    taskENTER_CRITICAL();
    {
        /* ֻ����û��֪ͨ����ʱ������ */
        if( pxCurrentTCB->ucNotifyState != taskNOTIFICATION_RECEIVED )
        {
            /* �������ֵ֪ͨ�е�λ����Ϊ��Щλ���ܻᱻ֪ͨ������ж�����
               ��������ڽ�ֵ���������ض�λ */
            pxCurrentTCB->ulNotifiedValue &= ~ulBitsToClearOnEntry;

            /* ����������Ϊ�ȴ�֪ͨ */
            pxCurrentTCB->ucNotifyState = taskWAITING_NOTIFICATION;

            /* ���ȴ�ʱ���Ƿ����0 */
            if( xTicksToWait > ( TickType_t ) 0 )
            {
                /* ����ǰ������ӵ��ӳ��б��������������� */
                prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
                /* ��¼����֪ͨ�ȴ������ĸ�����Ϣ */
                traceTASK_NOTIFY_WAIT_BLOCK();

                /* ���ж˿ڶ���дΪ�������ٽ�section�н���yield
                   ����Щ������yield��������ȴ��ٽ�section�˳���
                   ���ⲻ��Ӧ�ó������Ӧ���������� */
                portYIELD_WITHIN_API();
            }
            else
            {
                /* ���븲�ǲ��Ա�ǣ��ȴ�ʱ��Ϊ0������� */
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            /* ���븲�ǲ��Ա�ǣ�����֪ͨ���������� */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    /* �˳��ٽ��� */
    taskEXIT_CRITICAL();

    /* �ٴν����ٽ���������ȴ���� */
    taskENTER_CRITICAL();
    {
        /* ��¼����֪ͨ�ȴ��ĸ�����Ϣ */
        traceTASK_NOTIFY_WAIT();

        /* ����ṩ��ֵָ֪ͨ�룬�����ǰֵ֪ͨ�������Ѹ��Ļ�δ���ģ� */
        if( pulNotificationValue != NULL )
        {
            *pulNotificationValue = pxCurrentTCB->ulNotifiedValue;
        }

        /* ���ucNotifyValue����ΪtaskWAITING_NOTIFICATION��������Ҫô��δ��������״̬
           ����Ϊ�Ѿ���֪ͨ���𣩣�Ҫô������֪ͨ���������������������ʱ��������� */
        if( pxCurrentTCB->ucNotifyState == taskWAITING_NOTIFICATION )
        {
            /* δ�յ�֪ͨ����ʱ�� */
            xReturn = pdFALSE;
        }
        else
        {
            /* �Ѿ���֪ͨ����������ڵȴ�ʱ�յ���֪ͨ */
            pxCurrentTCB->ulNotifiedValue &= ~ulBitsToClearOnExit;
            xReturn = pdTRUE;
        }

        /* ������֪ͨ״̬����Ϊ"δ�ȴ�֪ͨ" */
        pxCurrentTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION;
    }
    /* �˳��ٽ��� */
    taskEXIT_CRITICAL();

    /* ���صȴ������Ľ�� */
    return xReturn;
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xTaskGenericNotify
 * ��������������������������ָ��������ͨ��֪ͨ�ĺ��ĺ���
 *           �˺���������֪ͨ���Ƶ�ͨ������汾��֧�ֶ���֪ͨ���������Ĳ�������
 *           �����������������и��������ֵ֪ͨ��״̬�������ܽ�����������״̬
 * ���������
 *   - xTaskToNotify: Ҫ֪ͨ����������ָ����Ҫ����֪ͨ���������
 *   - ulValue: ֵ֪ͨ�����ݲ�ͬ��֪ͨ�������в�ͬ�ĺ���
 *   - eAction: ֪ͨ����ö�٣�ָ����θ��������ֵ֪ͨ
 *   - pulPreviousNotificationValue: ָ����ǰֵ֪ͨ��ָ�룬���ڷ��ظ���ǰ��ֵ֪ͨ
 * ���������
 *   - pulPreviousNotificationValue: �����NULL�����ظ���ǰ������ֵ֪ͨ
 * ����ֵ��
 *   - BaseType_t: ����֪ͨ�����Ľ��
 *                 pdPASS��ʾ֪ͨ�����ɹ����
 *                 pdFAIL��ʾĳЩ����ʧ�ܣ���eSetValueWithoutOverwriteʱ����֪ͨ��
 * ����˵����
 *   - �˺�������configUSE_TASK_NOTIFICATIONSΪ1ʱ���룬������֪ͨ���ܵ�һ����
 *   - רΪ������������ƣ�ʹ���ٽ�������ȷ���̰߳�ȫ
 *   - ֧�ֶ���֪ͨ����������λ������������������ֵ��������������ֵ���޶���
 *   - ��ѡ���Է�����ǰ��ֵ֪ͨ������Ӧ�ó�����
 *   - ����������ڵȴ�֪ͨ������������״̬��������ӵ������б�
 *   - ����Ƿ���Ҫ�����л������ڱ�Ҫʱ���������л�
 *   - ֧���޵δ����ģʽ��ȷ��������������ʱ��ȷ������һ������������ʱ��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
#if( configUSE_TASK_NOTIFICATIONS == 1 )

BaseType_t xTaskGenericNotify( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, uint32_t *pulPreviousNotificationValue )
{
    /* ָ��������ƿ��ָ�� */
    TCB_t * pxTCB;
    /* �洢�������ؽ����Ĭ��Ϊ�ɹ� */
    BaseType_t xReturn = pdPASS;
    /* �洢ԭʼ��֪ͨ״̬ */
    uint8_t ucOriginalNotifyState;

    /* ���Լ��ȷ����������Ч */
    configASSERT( xTaskToNotify );
    /* ��������ת��Ϊ������ƿ�ָ�� */
    pxTCB = ( TCB_t * ) xTaskToNotify;

    /* �����ٽ���������֪ͨ������ԭ���� */
    taskENTER_CRITICAL();
    {
        /* ����ṩ����ǰֵָ֪ͨ�룬�򱣴浱ǰֵ֪ͨ */
        if( pulPreviousNotificationValue != NULL )
        {
            *pulPreviousNotificationValue = pxTCB->ulNotifiedValue;
        }

        /* ����ԭʼ��֪ͨ״̬ */
        ucOriginalNotifyState = pxTCB->ucNotifyState;
        /* ������֪ͨ״̬����Ϊ"�ѽ���֪ͨ" */
        pxTCB->ucNotifyState = taskNOTIFICATION_RECEIVED;

        /* ����֪ͨ�������͸��������ֵ֪ͨ */
        switch( eAction )
        {
            case eSetBits : /* ����λ��ʹ�ð�λ���������ֵ֪ͨ */
                pxTCB->ulNotifiedValue |= ulValue;
                break;

            case eIncrement : /* ����������ֵ֪ͨ�����Ƽ����ź����� */
                ( pxTCB->ulNotifiedValue )++;
                break;

            case eSetValueWithOverwrite : /* ����������ֵ��ֱ�Ӹ���ֵ֪ͨ */
                pxTCB->ulNotifiedValue = ulValue;
                break;

            case eSetValueWithoutOverwrite : /* ������������ֵ����������δ����"�ѽ���֪ͨ"״̬ʱ����ֵ */
                if( ucOriginalNotifyState != taskNOTIFICATION_RECEIVED )
                {
                    pxTCB->ulNotifiedValue = ulValue;
                }
                else
                {
                    /* �޷���ֵд����������֪ͨδ���� */
                    xReturn = pdFAIL;
                }
                break;

            case eNoAction: /* �޶�����ֻ����֪ͨ״̬�����޸�ֵ֪ͨ */
                /* ����֪ͨ����������ֵ֪ͨ */
                break;
        }

        /* ��¼����֪ͨ�ĸ�����Ϣ */
        traceTASK_NOTIFY();

        /* �������������״̬ר�ŵȴ�֪ͨ����������������� */
        if( ucOriginalNotifyState == taskWAITING_NOTIFICATION )
        {
            /* �ӵ�ǰ״̬�б������б����Ƴ����� */
            ( void ) uxListRemove( &( pxTCB->xStateListItem ) );
            /* ��������ӵ������б� */
            prvAddTaskToReadyList( pxTCB );

            /* ����Ӧ�����¼��б��� */
            configASSERT( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) == NULL );

            /* ����Ƿ��������޵δ����ģʽ */
            #if( configUSE_TICKLESS_IDLE != 0 )
            {
                /* ���������ȴ�֪ͨ������������ôxNextTaskUnblockTime��������Ϊ��������ĳ�ʱʱ��
                   ���������ʱ�����ԭ����������xNextTaskUnblockTimeͨ�����ֲ��䣬
                   ��Ϊ���δ��������xNextTaskUnblockTimeʱ�������Զ�����Ϊ��ֵ
                   ���ǣ����ʹ���޵δ����ģʽ�����ܸ���Ҫ�����ھ��������ʱ�����˯��ģʽ
                   �������������xNextTaskUnblockTime��ȷ�����ھ��������ʱ����� */
                prvResetNextTaskUnblockTime();
            }
            #endif

            /* ��鱻֪ͨ��������ȼ��Ƿ���ڵ�ǰִ������ */
            if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
            {
                /* ��֪ͨ����������ȼ����ڵ�ǰִ�е����������Ҫ yield */
                taskYIELD_IF_USING_PREEMPTION();
            }
            else
            {
                /* ���븲�ǲ��Ա�ǣ����ȼ������ڵ�ǰ���������� */
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            /* ���븲�ǲ��Ա�ǣ������ڵȴ�֪ͨ״̬������� */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    /* �˳��ٽ��� */
    taskEXIT_CRITICAL();

    /* ����֪ͨ�����Ľ�� */
    return xReturn;
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xTaskGenericNotifyFromISR
 * �������������жϷ������(ISR)����������ͨ��֪ͨ���ڲ�����
 *           �˺���������֪ͨ���Ƶ�ͨ��ISR�汾��֧�ֶ���֪ͨ���������Ĳ�������
 *           �������ж��������и��������ֵ֪ͨ��״̬�������ܽ�����������״̬
 * ���������
 *   - xTaskToNotify: Ҫ֪ͨ����������ָ����Ҫ����֪ͨ���������
 *   - ulValue: ֵ֪ͨ�����ݲ�ͬ��֪ͨ�������в�ͬ�ĺ���
 *   - eAction: ֪ͨ����ö�٣�ָ����θ��������ֵ֪ͨ
 *   - pulPreviousNotificationValue: ָ����ǰֵ֪ͨ��ָ�룬���ڷ��ظ���ǰ��ֵ֪ͨ
 *   - pxHigherPriorityTaskWoken: ָ��������ȼ������ѱ�־��ָ��
 * ���������
 *   - pulPreviousNotificationValue: �����NULL�����ظ���ǰ������ֵ֪ͨ
 *   - pxHigherPriorityTaskWoken: ���֪ͨ�������¸������ȼ����������������ΪpdTRUE
 * ����ֵ��
 *   - BaseType_t: ����֪ͨ�����Ľ��
 *                 pdPASS��ʾ֪ͨ�����ɹ����
 *                 pdFAIL��ʾĳЩ����ʧ�ܣ���eSetValueWithoutOverwriteʱ����֪ͨ��
 * ����˵����
 *   - �˺�������configUSE_TASK_NOTIFICATIONSΪ1ʱ���룬������֪ͨ���ܵ�һ����
 *   - רΪ�жϷ��������ƣ�ʹ���жϰ�ȫ��API�Ͳ���
 *   - ֧�ֶ���֪ͨ����������λ������������������ֵ��������������ֵ���޶���
 *   - ��ѡ���Է�����ǰ��ֵ֪ͨ������Ӧ�ó�����
 *   - ����������ڵȴ�֪ͨ������������״̬
 *   - ����Ƿ���Ҫ�����л�����ͨ������֪ͨ������
 *   - ʹ���ж����뱣���ؼ�������ȷ�����ж��������е��̰߳�ȫ
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
#if( configUSE_TASK_NOTIFICATIONS == 1 )

BaseType_t xTaskGenericNotifyFromISR( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, uint32_t *pulPreviousNotificationValue, BaseType_t *pxHigherPriorityTaskWoken )
{
    /* ָ��������ƿ��ָ�� */
    TCB_t * pxTCB;
    /* �洢ԭʼ��֪ͨ״̬ */
    uint8_t ucOriginalNotifyState;
    /* �洢�������ؽ����Ĭ��Ϊ�ɹ� */
    BaseType_t xReturn = pdPASS;
    /* �洢�ж�״̬�����ڻָ��ж����� */
    UBaseType_t uxSavedInterruptStatus;

    /* ���Լ��ȷ����������Ч */
    configASSERT( xTaskToNotify );

    /* ֧���ж�Ƕ�׵�RTOS�˿������ϵͳ���ã������API���ã��ж����ȼ��ĸ���
       �������ϵͳ�������ȼ����жϱ����������ã���ʹRTOS�ں˴����ٽ�section
       �����ܵ����κ�FreeRTOS API�����������FreeRTOSConfig.h�ж�����configASSERT()
       ��ô������жϵ���FreeRTOS API������portASSERT_IF_INTERRUPT_PRIORITY_INVALID()
       �����¶���ʧ�ܣ����ж��ѱ�����������õ����ϵͳ�������ȼ���
       ֻ����FromISR��β��FreeRTOS�������Դ��ѱ��������ȼ����ڻ��߼��ϣ�
       �������ϵͳ�����ж����ȼ����жϵ��á�FreeRTOSά��һ���������жϰ�ȫAPI
       ��ȷ���ж���ھ����ܿ��ٺͼ򵥡�������Ϣ��������Cortex-M�ض��ģ�
       �����������ṩ��http://www.freertos.org/RTOS-Cortex-M3-M4.html */
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    /* ��������ת��Ϊ������ƿ�ָ�� */
    pxTCB = ( TCB_t * ) xTaskToNotify;

    /* ���浱ǰ�ж�״̬�������ж����� */
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        /* ����ṩ����ǰֵָ֪ͨ�룬�򱣴浱ǰֵ֪ͨ */
        if( pulPreviousNotificationValue != NULL )
        {
            *pulPreviousNotificationValue = pxTCB->ulNotifiedValue;
        }

        /* ����ԭʼ��֪ͨ״̬ */
        ucOriginalNotifyState = pxTCB->ucNotifyState;
        /* ������֪ͨ״̬����Ϊ"�ѽ���֪ͨ" */
        pxTCB->ucNotifyState = taskNOTIFICATION_RECEIVED;

        /* ����֪ͨ�������͸��������ֵ֪ͨ */
        switch( eAction )
        {
            case eSetBits : /* ����λ��ʹ�ð�λ���������ֵ֪ͨ */
                pxTCB->ulNotifiedValue |= ulValue;
                break;

            case eIncrement : /* ����������ֵ֪ͨ�����Ƽ����ź����� */
                ( pxTCB->ulNotifiedValue )++;
                break;

            case eSetValueWithOverwrite : /* ����������ֵ��ֱ�Ӹ���ֵ֪ͨ */
                pxTCB->ulNotifiedValue = ulValue;
                break;

            case eSetValueWithoutOverwrite : /* ������������ֵ����������δ����"�ѽ���֪ͨ"״̬ʱ����ֵ */
                if( ucOriginalNotifyState != taskNOTIFICATION_RECEIVED )
                {
                    pxTCB->ulNotifiedValue = ulValue;
                }
                else
                {
                    /* �޷���ֵд����������֪ͨδ���� */
                    xReturn = pdFAIL;
                }
                break;

            case eNoAction : /* �޶�����ֻ����֪ͨ״̬�����޸�ֵ֪ͨ */
                /* ����֪ͨ����������ֵ֪ͨ */
                break;
        }

        /* ��¼����֪ͨ�ĸ�����Ϣ */
        traceTASK_NOTIFY_FROM_ISR();

        /* �������������״̬ר�ŵȴ�֪ͨ����������������� */
        if( ucOriginalNotifyState == taskWAITING_NOTIFICATION )
        {
            /* ����Ӧ�����¼��б��� */
            configASSERT( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) == NULL );

            /* ���������Ƿ񱻹��� */
            if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
            {
                /* �ӵ�ǰ״̬�б������б����Ƴ����� */
                ( void ) uxListRemove( &( pxTCB->xStateListItem ) );
                /* ��������ӵ������б� */
                prvAddTaskToReadyList( pxTCB );
            }
            else
            {
                /* �޷������ӳٺ;����б���˽����������ֱ���������ָ� */
                vListInsertEnd( &( xPendingReadyList ), &( pxTCB->xEventListItem ) );
            }

            /* ��鱻֪ͨ��������ȼ��Ƿ���ڵ�ǰִ������ */
            if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
            {
                /* ��֪ͨ����������ȼ����ڵ�ǰִ�е����������Ҫ yield */
                if( pxHigherPriorityTaskWoken != NULL )
                {
                    /* ���ø������ȼ������ѱ�־ */
                    *pxHigherPriorityTaskWoken = pdTRUE;
                }
                else
                {
                    /* ����� yield �������Է��û�δ��ISR��ȫ��FreeRTOS������ʹ��
                       "xHigherPriorityTaskWoken"���� */
                    xYieldPending = pdTRUE;
                }
            }
            else
            {
                /* ���븲�ǲ��Ա�ǣ����ȼ������ڵ�ǰ���������� */
                mtCOVERAGE_TEST_MARKER();
            }
        }
    }
    /* ����ж����벢�ָ�֮ǰ���ж�״̬ */
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

    /* ����֪ͨ�����Ľ�� */
    return xReturn;
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�vTaskNotifyGiveFromISR
 * �������������жϷ������(ISR)����������֪ͨ���ڲ�����
 *           �˺���������֪ͨ���Ƶ�ISR�汾���������ж������������������ֵ֪ͨ
 *           �����ܽ�����������״̬�������ڼ����ź�����"give"����
 * ���������
 *   - xTaskToNotify: Ҫ֪ͨ����������ָ����Ҫ����֪ͨ���������
 *   - pxHigherPriorityTaskWoken: ָ��������ȼ������ѱ�־��ָ��
 * ���������
 *   - pxHigherPriorityTaskWoken: ���֪ͨ�������¸������ȼ����������������ΪpdTRUE
 * ����ֵ����
 * ����˵����
 *   - �˺�������configUSE_TASK_NOTIFICATIONSΪ1ʱ���룬������֪ͨ���ܵ�һ����
 *   - רΪ�жϷ��������ƣ�ʹ���жϰ�ȫ��API�Ͳ���
 *   - ���������ֵ֪ͨ�������ڼ����ź����ĵ�������
 *   - ����������ڵȴ�֪ͨ������������״̬
 *   - ����Ƿ���Ҫ�����л�����ͨ������֪ͨ������
 *   - ʹ���ж����뱣���ؼ�������ȷ�����ж��������е��̰߳�ȫ
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
#if( configUSE_TASK_NOTIFICATIONS == 1 )

void vTaskNotifyGiveFromISR( TaskHandle_t xTaskToNotify, BaseType_t *pxHigherPriorityTaskWoken )
{
    /* ָ��������ƿ��ָ�� */
    TCB_t * pxTCB;
    /* �洢ԭʼ��֪ͨ״̬ */
    uint8_t ucOriginalNotifyState;
    /* �洢�ж�״̬�����ڻָ��ж����� */
    UBaseType_t uxSavedInterruptStatus;

    /* ���Լ��ȷ����������Ч */
    configASSERT( xTaskToNotify );

    /* ֧���ж�Ƕ�׵�RTOS�˿������ϵͳ���ã������API���ã��ж����ȼ��ĸ���
       �������ϵͳ�������ȼ����жϱ����������ã���ʹRTOS�ں˴����ٽ�section
       �����ܵ����κ�FreeRTOS API�����������FreeRTOSConfig.h�ж�����configASSERT()
       ��ô������жϵ���FreeRTOS API������portASSERT_IF_INTERRUPT_PRIORITY_INVALID()
       �����¶���ʧ�ܣ����ж��ѱ�����������õ����ϵͳ�������ȼ���
       ֻ����FromISR��β��FreeRTOS�������Դ��ѱ��������ȼ����ڻ��߼��ϣ�
       �������ϵͳ�����ж����ȼ����жϵ��á�FreeRTOSά��һ���������жϰ�ȫAPI
       ��ȷ���ж���ھ����ܿ��ٺͼ򵥡�������Ϣ��������Cortex-M�ض��ģ�
       �����������ṩ��http://www.freertos.org/RTOS-Cortex-M3-M4.html */
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    /* ��������ת��Ϊ������ƿ�ָ�� */
    pxTCB = ( TCB_t * ) xTaskToNotify;

    /* ���浱ǰ�ж�״̬�������ж����� */
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        /* ����ԭʼ��֪ͨ״̬ */
        ucOriginalNotifyState = pxTCB->ucNotifyState;
        /* ������֪ͨ״̬����Ϊ"�ѽ���֪ͨ" */
        pxTCB->ucNotifyState = taskNOTIFICATION_RECEIVED;

        /* '����'֪ͨ�൱�����Ӽ����ź����еļ��� */
        ( pxTCB->ulNotifiedValue )++;

        /* ��¼����֪ͨ����ĸ�����Ϣ */
        traceTASK_NOTIFY_GIVE_FROM_ISR();

        /* �������������״̬ר�ŵȴ�֪ͨ����������������� */
        if( ucOriginalNotifyState == taskWAITING_NOTIFICATION )
        {
            /* ����Ӧ�����¼��б��� */
            configASSERT( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) == NULL );

            /* ���������Ƿ񱻹��� */
            if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
            {
                /* �ӵ�ǰ״̬�б������б����Ƴ����� */
                ( void ) uxListRemove( &( pxTCB->xStateListItem ) );
                /* ��������ӵ������б� */
                prvAddTaskToReadyList( pxTCB );
            }
            else
            {
                /* �޷������ӳٺ;����б���˽����������ֱ���������ָ� */
                vListInsertEnd( &( xPendingReadyList ), &( pxTCB->xEventListItem ) );
            }

            /* ��鱻֪ͨ��������ȼ��Ƿ���ڵ�ǰִ������ */
            if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
            {
                /* ��֪ͨ����������ȼ����ڵ�ǰִ�е����������Ҫ yield */
                if( pxHigherPriorityTaskWoken != NULL )
                {
                    /* ���ø������ȼ������ѱ�־ */
                    *pxHigherPriorityTaskWoken = pdTRUE;
                }
                else
                {
                    /* ����� yield �������Է��û�δ��ISR��ȫ��FreeRTOS������ʹ��
                       "xHigherPriorityTaskWoken"���� */
                    xYieldPending = pdTRUE;
                }
            }
            else
            {
                /* ���븲�ǲ��Ա�ǣ����ȼ������ڵ�ǰ���������� */
                mtCOVERAGE_TEST_MARKER();
            }
        }
    }
    /* ����ж����벢�ָ�֮ǰ���ж�״̬ */
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );
}

#endif /* configUSE_TASK_NOTIFICATIONS */

/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xTaskNotifyStateClear
 * �����������������֪ͨ״̬�ĺ��ĺ���
 *           �˺������ڽ������֪ͨ״̬��"�ѽ���֪ͨ"����Ϊ"δ�ȴ�֪ͨ"
 *           ��Ҫ��������֪ͨ�����У�����Ҫ���������֪ͨ״̬ʱ����
 * ���������
 *   - xTask: Ҫ���֪ͨ״̬��������
 *            �������NULL�����ʾ�����ǰ���������֪ͨ״̬
 * �����������
 * ����ֵ��
 *   - BaseType_t: ����״̬��������Ľ��
 *                 pdPASS��ʾ�ɹ�����������֪ͨ״̬
 *                 pdFAIL��ʾ���ʧ�ܣ�����ǰ������"�ѽ���֪ͨ"״̬��
 * ����˵����
 *   - �˺�������configUSE_TASK_NOTIFICATIONSΪ1ʱ���룬������֪ͨ���ܵ�һ����
 *   - ʹ���ٽ�������״̬���������ȷ���ڶ����񻷾��µ��̰߳�ȫ��
 *   - ͨ����������ȡ������ƿ飬֧��������������ǰ�����֪ͨ״̬
 *   - ��Ҫ��������֪ͨ�����У���������֪ͨ����Ҫ��������״̬ʱ����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
#if( configUSE_TASK_NOTIFICATIONS == 1 )

BaseType_t xTaskNotifyStateClear( TaskHandle_t xTask )
{
    /* ָ��������ƿ��ָ�� */
    TCB_t *pxTCB;
    /* �洢�������ؽ�� */
    BaseType_t xReturn;

    /* ����˴�����null�����ʾҪ���֪ͨ״̬���ǵ��������� */
    pxTCB = prvGetTCBFromHandle( xTask );

    /* �����ٽ���������״̬���������ԭ���� */
    taskENTER_CRITICAL();
    {
        /* �������ǰ�Ƿ���"�ѽ���֪ͨ"״̬ */
        if( pxTCB->ucNotifyState == taskNOTIFICATION_RECEIVED )
        {
            /* ������֪ͨ״̬����Ϊ"δ�ȴ�֪ͨ" */
            pxTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION;
            /* ���÷���ֵΪ�ɹ� */
            xReturn = pdPASS;
        }
        else
        {
            /* ���÷���ֵΪʧ�� */
            xReturn = pdFAIL;
        }
    }
    /* �˳��ٽ��� */
    taskEXIT_CRITICAL();

    /* ����״̬��������Ľ�� */
    return xReturn;
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/


/*******************************************************************************
 * �������ƣ�prvAddCurrentTaskToDelayedList
 * ��������������ǰ������ӵ��ӳ��б������б����ڲ�����
 *           ���ݵȴ�ʱ���������Ļ���ʱ�䣬����������ӵ���Ӧ���ӳ��б������б������б�
 *           ͬʱ�������������������������������������ӵ��ӳ��б�
 * ���������
 *   - xTicksToWait: ������Ҫ�ȴ���ʱ�ӽ�����
 *   - xCanBlockIndefinitely: ָʾ�����Ƿ���������������ı�־
 *                            pdTRUE��ʾ����������������pdFALSE��ʾ����
 * �����������
 * ����ֵ����
 * ����˵����
 *   - �˺���������������ĺ��Ĳ��֣�����������������״̬
 *   - ����ϵͳ���ļ�������������ȷ��������Ļ���ʱ��
 *   - ֧�������ӳ���ֹ���ܣ��������INCLUDE_xTaskAbortDelay��
 *   - ���ݵȴ�ʱ���Ƿ��������������ӵ���ͬ���ӳ��б�
 *   - ������һ������������ʱ�䣬�Ż�����������
 *   - ʹ�������������䲻ͬ�Ĺ������ã�����������ӳ���ֹ�ȣ�
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/03     V1.00          Your Name          ����
 *******************************************************************************/
static void prvAddCurrentTaskToDelayedList( TickType_t xTicksToWait, const BaseType_t xCanBlockIndefinitely )
{
    /* �洢����Ӧ�ñ����ѵ�ʱ�� */
    TickType_t xTimeToWake;
    /* �洢��ǰ�Ľ��ļ���ֵ�������ڼ��������ֵ�����仯 */
    const TickType_t xConstTickCount = xTickCount;

    /* ����Ƿ������������ӳ���ֹ���� */
    #if( INCLUDE_xTaskAbortDelay == 1 )
    {
        /* ���������ӳ��б����ȷ��ucDelayAborted��־����ΪpdFALSE
           ���������������뿪����״̬ʱ��⵽���Ƿ�����ΪpdTRUE */
        pxCurrentTCB->ucDelayAborted = pdFALSE;
    }
    #endif

    /* �ڽ�������ӵ������б�֮ǰ���Ƚ���Ӿ����б����Ƴ�
       ��Ϊͬһ���б��������������б� */
    if( uxListRemove( &( pxCurrentTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
    {
        /* ��ǰ��������ھ����б��У���˲���Ҫ��飬����ֱ�ӵ��ö˿����ú� */
        portRESET_READY_PRIORITY( pxCurrentTCB->uxPriority, uxTopReadyPriority );
    }
    else
    {
        /* ���븲�ǲ��Ա�ǣ������ھ����б��е������ */
        mtCOVERAGE_TEST_MARKER();
    }

    /* ����Ƿ���������������� */
    #if ( INCLUDE_vTaskSuspend == 1 )
    {
        /* ����Ƿ����������������������������� */
        if( ( xTicksToWait == portMAX_DELAY ) && ( xCanBlockIndefinitely != pdFALSE ) )
        {
            /* ��������ӵ����������б�������ӳ������б�
               ȷ�������ᱻ��ʱ�¼����ѣ��������������� */
            vListInsertEnd( &xSuspendedTaskList, &( pxCurrentTCB->xStateListItem ) );
        }
        else
        {
            /* ��������¼�δ����������Ӧ�ñ����ѵ�ʱ��
               ����ܻ����������û��ϵ���ں˻���ȷ������ */
            xTimeToWake = xConstTickCount + xTicksToWait;

            /* �б��������ʱ��˳����� */
            listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xStateListItem ), xTimeToWake );

            /* ��黽��ʱ���Ƿ�С�ڵ�ǰʱ�䣨�������� */
            if( xTimeToWake < xConstTickCount )
            {
                /* ����ʱ���������������Ŀ��������б��� */
                vListInsert( pxOverflowDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );
            }
            else
            {
                /* ����ʱ��δ��������ʹ�õ�ǰ�����б� */
                vListInsert( pxDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );

                /* �����������״̬�����񱻷��������������б��ͷ��
                   ��ôxNextTaskUnblockTimeҲ��Ҫ���� */
                if( xTimeToWake < xNextTaskUnblockTime )
                {
                    xNextTaskUnblockTime = xTimeToWake;
                }
                else
                {
                    /* ���븲�ǲ��Ա�ǣ�����Ҫ����xNextTaskUnblockTime������� */
                    mtCOVERAGE_TEST_MARKER();
                }
            }
        }
    }
    #else /* INCLUDE_vTaskSuspend */
    {
        /* ��������¼�δ����������Ӧ�ñ����ѵ�ʱ��
           ����ܻ����������û��ϵ���ں˻���ȷ������ */
        xTimeToWake = xConstTickCount + xTicksToWait;

        /* �б��������ʱ��˳����� */
        listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xStateListItem ), xTimeToWake );

        /* ��黽��ʱ���Ƿ�С�ڵ�ǰʱ�䣨�������� */
        if( xTimeToWake < xConstTickCount )
        {
            /* ����ʱ���������������Ŀ��������б��� */
            vListInsert( pxOverflowDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );
        }
        else
        {
            /* ����ʱ��δ��������ʹ�õ�ǰ�����б� */
            vListInsert( pxDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );

            /* �����������״̬�����񱻷��������������б��ͷ��
               ��ôxNextTaskUnblockTimeҲ��Ҫ���� */
            if( xTimeToWake < xNextTaskUnblockTime )
            {
                xNextTaskUnblockTime = xTimeToWake;
            }
            else
            {
                /* ���븲�ǲ��Ա�ǣ�����Ҫ����xNextTaskUnblockTime������� */
                mtCOVERAGE_TEST_MARKER();
            }
        }

        /* ��INCLUDE_vTaskSuspend��Ϊ1ʱ������������� */
        ( void ) xCanBlockIndefinitely;
    }
    #endif /* INCLUDE_vTaskSuspend */
}


#ifdef FREERTOS_MODULE_TEST
	#include "tasks_test_access_functions.h"
#endif


TCB_t *test_tskTCB1 = NULL;
TCB_t *test_tsKTCB2 = NULL; 
TCB_t *test_tsKTCB3 = NULL;


void debugs_test( TaskHandle_t * pxCreatedTask_Debug1 ,TaskHandle_t * pxCreatedTask_Debug2 ,TaskHandle_t * pxCreatedTask_Debug3)
{
	test_tskTCB1 = (tskTCB*)pxCreatedTask_Debug1;
	test_tsKTCB2 = (tskTCB*)pxCreatedTask_Debug2;
  test_tsKTCB3 = (tskTCB*)pxCreatedTask_Debug3;
}



/*******************************************************************************
 * ��Ȩ���� (C)2025, CQG
 *
 * �ļ����ƣ� LM_event_groups.c
 * �ļ���ʶ�� 
 * ����ժҪ�� �¼���ģ�鶨��
 * ����˵���� ��
 * ��ǰ�汾�� FreeRTOS V9.0.0
 * ��    �ߣ� Qiguo_Cui                   
 * ������ڣ� 2025��09��13��
 *
 *******************************************************************************/
 
/* Includes ------------------------------------------------------------------*/
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "event_groups.h"

/* Lint e961 and e750 are suppressed as a MISRA exception justified because the
MPU ports require MPU_WRAPPERS_INCLUDED_FROM_API_FILE to be defined for the
header files above, but not in this file, in order to generate the correct
privileged Vs unprivileged linkage and placement. */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE /*lint !e961 !e750. */

/* Private macros ------------------------------------------------------------*/
/* The following bit fields convey control information in a task's event list
item value.  It is important they don't clash with the
taskEVENT_LIST_ITEM_VALUE_IN_USE definition. */
#if configUSE_16_BIT_TICKS == 1
	#define eventCLEAR_EVENTS_ON_EXIT_BIT	0x0100U
	#define eventUNBLOCKED_DUE_TO_BIT_SET	0x0200U
	#define eventWAIT_FOR_ALL_BITS			0x0400U
	#define eventEVENT_BITS_CONTROL_BYTES	0xff00U
#else
	#define eventCLEAR_EVENTS_ON_EXIT_BIT	0x01000000UL
	#define eventUNBLOCKED_DUE_TO_BIT_SET	0x02000000UL
	#define eventWAIT_FOR_ALL_BITS			0x04000000UL
	#define eventEVENT_BITS_CONTROL_BYTES	0xff000000UL
#endif

/* Private types -------------------------------------------------------------*/
typedef struct xEventGroupDefinition
{
	EventBits_t uxEventBits;
	List_t xTasksWaitingForBits;		/*< List of tasks waiting for a bit to be set. */

	#if( configUSE_TRACE_FACILITY == 1 )
		UBaseType_t uxEventGroupNumber;
	#endif

	#if( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
		uint8_t ucStaticallyAllocated; /*< Set to pdTRUE if the event group is statically allocated to ensure no attempt is made to free the memory. */
	#endif
} EventGroup_t;

/* Private functions ---------------------------------------------------------*/
/*
 * Test the bits set in uxCurrentEventBits to see if the wait condition is met.
 * The wait condition is defined by xWaitForAllBits.  If xWaitForAllBits is
 * pdTRUE then the wait condition is met if all the bits set in uxBitsToWaitFor
 * are also set in uxCurrentEventBits.  If xWaitForAllBits is pdFALSE then the
 * wait condition is met if any of the bits set in uxBitsToWait for are also set
 * in uxCurrentEventBits.
 */
static BaseType_t prvTestWaitCondition( const EventBits_t uxCurrentEventBits, const EventBits_t uxBitsToWaitFor, const BaseType_t xWaitForAllBits ) PRIVILEGED_FUNCTION;

/*******************************************************************************
 * �������ƣ�xEventGroupCreateStatic
 * ������������̬�����¼������ʹ��Ԥ������ڴ滺����
 *           �¼�����FreeRTOS�����������ͬ�����¼�֪ͨ�Ļ��ƣ�֧�ֶ��¼�λ����
 * ���������
 *   - pxEventGroupBuffer: ָ��Ԥ����ľ�̬�¼����ڴ滺������ָ��
 * �����������
 * �� �� ֵ��
 *   - EventGroupHandle_t: �ɹ�����ʱ�����¼�������ʧ��ʱ����NULL
 * ����˵����
 *   - �˺����������þ�̬����ʱ���루configSUPPORT_STATIC_ALLOCATION == 1��
 *   - �¼����ڴ����û�Ԥ�ȷ��䣬���漰��̬�ڴ����
 *   - ��ʼ���¼�λΪ0����ʾ�����¼�λ��δ����
 *   - ��ʼ���ȴ��¼�λ�������б�
 *   - ֧�ָ����¼��鴴���ɹ���ʧ��
 *   - �������ڴ����޻���Ҫ��ȷ�ڴ������Ƕ��ʽϵͳ
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
#if( configSUPPORT_STATIC_ALLOCATION == 1 )

EventGroupHandle_t xEventGroupCreateStatic( StaticEventGroup_t *pxEventGroupBuffer )
{
    EventGroup_t *pxEventBits;  /* ָ���¼�������ָ�� */

    /* �����ṩStaticEventGroup_t����ʹ�ö��Լ��ָ����Ч�� */
    configASSERT( pxEventGroupBuffer );

    /* �û��ṩ�˾�̬������¼����ڴ� - ʹ���� */
    pxEventBits = ( EventGroup_t * ) pxEventGroupBuffer; /*lint !e740 EventGroup_t��StaticEventGroup_t��֤������ͬ�Ĵ�С�Ͷ���Ҫ�� - ��configASSERT()��� */

    /* ���ָ���Ƿ���Ч */
    if( pxEventBits != NULL )
    {
        /* ��ʼ���¼�λΪ0����ʾ�����¼�λ��δ���� */
        pxEventBits->uxEventBits = 0;
        
        /* ��ʼ���ȴ��¼�λ�������б� */
        vListInitialise( &( pxEventBits->xTasksWaitingForBits ) );

        /* ���ͬʱ֧�ֶ�̬���䣬����¼���ķ��䷽ʽ */
        #if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
        {
            /* ��̬�Ͷ�̬���䶼����ʹ�ã���˱�Ǵ��¼����Ǿ�̬�����ģ�
               �Ա��ں���ɾ���¼���ʱ��ȷ���� */
            pxEventBits->ucStaticallyAllocated = pdTRUE;
        }
        #endif /* configSUPPORT_DYNAMIC_ALLOCATION */

        /* �����¼��鴴���ɹ� */
        traceEVENT_GROUP_CREATE( pxEventBits );
    }
    else
    {
        /* �����¼��鴴��ʧ�� */
        traceEVENT_GROUP_CREATE_FAILED();
    }

    /* �����¼����� */
    return ( EventGroupHandle_t ) pxEventBits;
}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xEventGroupCreate
 * ������������̬�����¼������ʹ��ϵͳ��̬�ڴ����
 *           �¼�����FreeRTOS�����������ͬ�����¼�֪ͨ�Ļ��ƣ�֧�ֶ��¼�λ����
 * �����������
 * �����������
 * �� �� ֵ��
 *   - EventGroupHandle_t: �ɹ�����ʱ�����¼�������ʧ��ʱ����NULL
 * ����˵����
 *   - �˺����������ö�̬����ʱ���루configSUPPORT_DYNAMIC_ALLOCATION == 1��
 *   - �¼����ڴ���ϵͳ��̬���䣬ʹ��pvPortMalloc����
 *   - ��ʼ���¼�λΪ0����ʾ�����¼�λ��δ����
 *   - ��ʼ���ȴ��¼�λ�������б�
 *   - ֧�ָ����¼��鴴���ɹ���ʧ��
 *   - ������֧�ֶ�̬�ڴ�����ϵͳ���ṩ�������ڴ����
 *   - �������¼������ʹ��vEventGroupDelete����ɾ�����ͷ��ڴ�
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

EventGroupHandle_t xEventGroupCreate( void )
{
    EventGroup_t *pxEventBits;  /* ָ���¼�������ָ�� */

    /* �����¼����ڴ棬ʹ��ϵͳ�Ķ�̬�ڴ���亯�� */
    pxEventBits = ( EventGroup_t * ) pvPortMalloc( sizeof( EventGroup_t ) );

    /* ����ڴ��Ƿ����ɹ� */
    if( pxEventBits != NULL )
    {
        /* ��ʼ���¼�λΪ0����ʾ�����¼�λ��δ���� */
        pxEventBits->uxEventBits = 0;
        
        /* ��ʼ���ȴ��¼�λ�������б� */
        vListInitialise( &( pxEventBits->xTasksWaitingForBits ) );

        /* ���ͬʱ֧�־�̬���䣬����¼���ķ��䷽ʽ */
        #if( configSUPPORT_STATIC_ALLOCATION == 1 )
        {
            /* ��̬�Ͷ�̬���䶼����ʹ�ã���˱�Ǵ��¼����Ƕ�̬����ģ�
               �Ա��ں���ɾ���¼���ʱ��ȷ�����ڴ��ͷ� */
            pxEventBits->ucStaticallyAllocated = pdFALSE;
        }
        #endif /* configSUPPORT_STATIC_ALLOCATION */

        /* �����¼��鴴���ɹ� */
        traceEVENT_GROUP_CREATE( pxEventBits );
    }
    else
    {
        /* �����¼��鴴��ʧ�� */
        traceEVENT_GROUP_CREATE_FAILED();
    }

    /* �����¼����� */
    return ( EventGroupHandle_t ) pxEventBits;
}

#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xEventGroupSync
 * �����������¼���ͬ������������ʵ�ֶ�������� rendezvous����ϵ㣩ͬ��
 *           ����ָ�����¼�λ��Ȼ��ȴ�����ָ�����¼�λ�����ã������������������ã�
 *           ����һ��ǿ���ͬ��ԭ���������������ض���ȴ��˴�
 * ���������
 *   - xEventGroup: �¼�������ָ��Ҫ�������¼���
 *   - uxBitsToSet: Ҫ���õ��¼�λ����������ϣ�����õ��¼�λ����
 *   - uxBitsToWaitFor: Ҫ�ȴ����¼�λ������������Ҫ�ȴ����¼�λ����
 *   - xTicksToWait: ���ȴ�ʱ�䣨��ʱ�ӽ���Ϊ��λ����������portMAX_DELAY��ʾ���޵ȴ�
 * �����������
 * �� �� ֵ��
 *   - EventBits_t: �����¼����ֵ��������ȴ�λ֮ǰ��
 *                 �����ʱ���أ����ص��ǳ�ʱʱ�̵��¼���ֵ
 *                 ��������еȴ�λ�����ö����أ����ص�������λ���ԭʼ�¼���ֵ
 * ����˵����
 *   - �˺���������ָ�����¼�λ��Ȼ��ȴ�����ָ�����¼�λ������
 *   - ������еȴ�λ�Ѿ����ã������ո����õ�λ����������������
 *   - ����ȴ�λδȫ�����ã����񽫽�������״̬��ֱ�����еȴ�λ�����û�ʱ
 *   - �ڳɹ�ͬ�������еȴ�λ�����ã�����Щ�ȴ�λ�ᱻ�Զ����
 *   - ����һ���������ã����ܻ����������������л�
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
EventBits_t xEventGroupSync( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet, const EventBits_t uxBitsToWaitFor, TickType_t xTicksToWait )
{
    EventBits_t uxOriginalBitValue, uxReturn;              /* ԭʼ�¼�λֵ�ͷ���ֵ */
    EventGroup_t *pxEventBits = ( EventGroup_t * ) xEventGroup;  /* �����ת��Ϊ�¼���ṹָ�� */
    BaseType_t xAlreadyYielded;                            /* ����Ƿ��Ѿ������������л� */
    BaseType_t xTimeoutOccurred = pdFALSE;                 /* ����Ƿ�����ʱ */

    /* ���Լ�飺ȷ���ȴ�λ�����������ֽڣ��ҵȴ�λ��Ϊ0 */
    configASSERT( ( uxBitsToWaitFor & eventEVENT_BITS_CONTROL_BYTES ) == 0 );
    configASSERT( uxBitsToWaitFor != 0 );
    
    /* �������������״̬��ѯ��ʹ�ö�ʱ������������״̬ */
    #if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
    {
        /* ���Լ�飺���������������ָ���˷���ȴ�ʱ�䣬�򱨴� */
        configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
    }
    #endif

    /* �����������񣬷�ֹ�ڲ����¼���ʱ������������ */
    vTaskSuspendAll();
    {
        /* ��ȡ��ǰ�¼�λ��ԭʼֵ */
        uxOriginalBitValue = pxEventBits->uxEventBits;

        /* ����ָ�����¼�λ */
        ( void ) xEventGroupSetBits( xEventGroup, uxBitsToSet );

        /* ����Ƿ����еȴ�λ���Ѿ����ã������ո����õ�λ�� */
        if( ( ( uxOriginalBitValue | uxBitsToSet ) & uxBitsToWaitFor ) == uxBitsToWaitFor )
        {
            /* ���л��λ�����Ѿ����� - ����Ҫ���� */
            uxReturn = ( uxOriginalBitValue | uxBitsToSet );

            /* ��ϲ�����������ȴ�λ���������ǻ���е�Ψһ���񣬷��������Ѿ������ */
            pxEventBits->uxEventBits &= ~uxBitsToWaitFor;

            /* ���õȴ�ʱ��Ϊ0����ʾ����Ҫ���� */
            xTicksToWait = 0;
        }
        else
        {
            /* �����ָ���ĵȴ�ʱ�䣬�������������״̬ */
            if( xTicksToWait != ( TickType_t ) 0 )
            {
                /* �����¼���ͬ�������¼� */
                traceEVENT_GROUP_SYNC_BLOCK( xEventGroup, uxBitsToSet, uxBitsToWaitFor );

                /* �������������ڵȴ���λ�洢��������¼��б����У�
                   �����ں�֪����ʱ�ҵ�ƥ���Ȼ���������״̬ */
                vTaskPlaceOnUnorderedEventList( &( pxEventBits->xTasksWaitingForBits ), 
                                                ( uxBitsToWaitFor | eventCLEAR_EVENTS_ON_EXIT_BIT | eventWAIT_FOR_ALL_BITS ), 
                                                xTicksToWait );

                /* �����ֵ�ǹ�ʱ�ģ���ΪuxReturn�������������������ã�
                   ����������д˸�ֵ��һЩ���������������ɹ���uxReturn����ʱδ���õľ��� */
                uxReturn = 0;
            }
            else
            {
                /* ���λû�����ã���û��ָ������ʱ�� - ֻ�践�ص�ǰ�¼�λֵ */
                uxReturn = pxEventBits->uxEventBits;
            }
        }
    }
    /* �ָ��������񣬲���ȡ�Ƿ��Ѿ������������л� */
    xAlreadyYielded = xTaskResumeAll();

    /* ���ָ���˵ȴ�ʱ�䣬�������ܵ�������� */
    if( xTicksToWait != ( TickType_t ) 0 )
    {
        /* �����û�в��������л�����API�ڲ����������ò� */
        if( xAlreadyYielded == pdFALSE )
        {
            portYIELD_WITHIN_API();
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
        }

        /* ���������Եȴ��������λ������ - ��ʱҪô�����λ�ѱ����ã�Ҫô����ʱ���ѹ��ڡ�
           ��������λ�ѱ����ã����ǽ����洢��������¼��б����У�����Ӧ�ü������������ */
        uxReturn = uxTaskResetEventItemValue();

        /* ��������Ƿ���ʱ������λ���ö�������� */
        if( ( uxReturn & eventUNBLOCKED_DUE_TO_BIT_SET ) == ( EventBits_t ) 0 )
        {
            /* ����ʱ�����ص�ǰ�¼�λֵ */
            taskENTER_CRITICAL();
            {
                uxReturn = pxEventBits->uxEventBits;

                /* ��Ȼ������Ϊ��ʱ�������������������������п�����һ�������Ѿ�������λ��
                   ����������������ô����Ҫ���˳�ǰ�����Щλ */
                if( ( uxReturn & uxBitsToWaitFor ) == uxBitsToWaitFor )
                {
                    pxEventBits->uxEventBits &= ~uxBitsToWaitFor;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
                }
            }
            taskEXIT_CRITICAL();

            /* ��Ƿ����˳�ʱ */
            xTimeoutOccurred = pdTRUE;
        }
        else
        {
            /* ������Ϊλ�����ö�������� */
        }

        /* ����λ���ܱ����ã���Ϊ������������������Ӧ�÷�����Щ����λ */
        uxReturn &= ~eventEVENT_BITS_CONTROL_BYTES;
    }

    /* �����¼���ͬ�������¼� */
    traceEVENT_GROUP_SYNC_END( xEventGroup, uxBitsToSet, uxBitsToWaitFor, xTimeoutOccurred );

    /* �����¼�λֵ */
    return uxReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xEventGroupWaitBits
 * �����������ȴ��¼����е��ض�λ�����ã�����ѡ�����λ�͵ȴ�����������λ����һλ��
 *           ����һ��ǿ����¼��ȴ���������������ȴ�һ�������¼�λ���ض����
 * ���������
 *   - xEventGroup: �¼�������ָ��Ҫ�������¼���
 *   - uxBitsToWaitFor: Ҫ�ȴ����¼�λ���룬ָ������ϣ���ȴ����¼�λ
 *   - xClearOnExit: �˳�ʱ�Ƿ�����ȴ����¼�λ
 *                   pdTRUE: �˳�ʱ���uxBitsToWaitForָ����λ
 *                   pdFALSE: �˳�ʱ�����λ
 *   - xWaitForAllBits: �ȴ�������ָ������Ҫ����λ�����û�����һλ�ñ�����
 *                      pdTRUE: �ȴ�����ָ��λ������
 *                      pdFALSE: �ȴ���һָ��λ������
 *   - xTicksToWait: ���ȴ�ʱ�䣨��ʱ�ӽ���Ϊ��λ��
 *                  ������0��ʾ���ȴ���������
 *                  ������portMAX_DELAY��ʾ���޵ȴ�
 *                  �����Ǿ�����ֵ��ʾ���޵ĵȴ�ʱ��
 * �����������
 * �� �� ֵ��
 *   - EventBits_t: �����¼����ֵ�������λ֮ǰ�����������Ļ���
 *                 �����ʱ���أ����ص��ǳ�ʱʱ�̵��¼���ֵ
 *                 �����ȴ�������������أ����ص�����������ʱ�̵��¼���ֵ
 * ����˵����
 *   - �˺�������ָ�����¼�λ�Ƿ��Ѿ�����ȴ�����
 *   - ��������Ѿ����㣬����ѡ���������ز����λ�����xClearOnExitΪpdTRUE��
 *   - ���������������ָ���˵ȴ�ʱ�䣬���񽫽�������״̬��ֱ�����������ʱ
 *   - ����һ���������ã����ܻ����������������л�
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
EventBits_t xEventGroupWaitBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToWaitFor, const BaseType_t xClearOnExit, const BaseType_t xWaitForAllBits, TickType_t xTicksToWait )
{
    EventGroup_t *pxEventBits = ( EventGroup_t * ) xEventGroup;  /* �����ת��Ϊ�¼���ṹָ�� */
    EventBits_t uxReturn, uxControlBits = 0;                     /* ����ֵ�Ϳ���λ */
    BaseType_t xWaitConditionMet, xAlreadyYielded;               /* �ȴ������Ƿ�������Ƿ��Ѿ����������л� */
    BaseType_t xTimeoutOccurred = pdFALSE;                       /* ����Ƿ�����ʱ */

    /* ����û�û�г��Եȴ��ں˱���ʹ�õ�λ����������������һ��λ */
    configASSERT( xEventGroup );
    configASSERT( ( uxBitsToWaitFor & eventEVENT_BITS_CONTROL_BYTES ) == 0 );
    configASSERT( uxBitsToWaitFor != 0 );
    
    /* �������������״̬��ѯ��ʹ�ö�ʱ������������״̬ */
    #if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
    {
        /* ���Լ�飺���������������ָ���˷���ȴ�ʱ�䣬�򱨴� */
        configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
    }
    #endif

    /* �����������񣬷�ֹ�ڲ����¼���ʱ������������ */
    vTaskSuspendAll();
    {
        /* ��ȡ��ǰ�¼�λ��ֵ */
        const EventBits_t uxCurrentEventBits = pxEventBits->uxEventBits;

        /* ���ȴ������Ƿ��Ѿ����� */
        xWaitConditionMet = prvTestWaitCondition( uxCurrentEventBits, uxBitsToWaitFor, xWaitForAllBits );

        /* ���ݵȴ������Ƿ�������в�ͬ�Ĵ��� */
        if( xWaitConditionMet != pdFALSE )
        {
            /* �ȴ������Ѿ����㣬��˲���Ҫ���� */
            uxReturn = uxCurrentEventBits;
            xTicksToWait = ( TickType_t ) 0;

            /* �����������ȴ�λ */
            if( xClearOnExit != pdFALSE )
            {
                pxEventBits->uxEventBits &= ~uxBitsToWaitFor;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
            }
        }
        else if( xTicksToWait == ( TickType_t ) 0 )
        {
            /* �ȴ�������δ���㣬��û��ָ������ʱ�䣬���ֻ���ص�ǰֵ */
            uxReturn = uxCurrentEventBits;
        }
        else
        {
            /* ���������Եȴ��������λ�����á�uxControlBits���ڼ�ס��xEventGroupWaitBits()���õ�ָ����Ϊ -
               �����¼�λ�����������ʱ */
            if( xClearOnExit != pdFALSE )
            {
                uxControlBits |= eventCLEAR_EVENTS_ON_EXIT_BIT;  /* �����˳�ʱ���λ�Ŀ���λ */
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
            }

            if( xWaitForAllBits != pdFALSE )
            {
                uxControlBits |= eventWAIT_FOR_ALL_BITS;  /* ���õȴ�����λ�Ŀ���λ */
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
            }

            /* �������������ڵȴ���λ�洢��������¼��б����У�
               �����ں�֪����ʱ�ҵ�ƥ���Ȼ���������״̬ */
            vTaskPlaceOnUnorderedEventList( &( pxEventBits->xTasksWaitingForBits ), 
                                            ( uxBitsToWaitFor | uxControlBits ), 
                                            xTicksToWait );

            /* ���ǹ�ʱ�ģ���Ϊ���������������������ã�
               ���������������һЩ���������������ɹ��ڱ�������ʱδ���õľ��� */
            uxReturn = 0;

            /* �����¼���ȴ�λ�����¼� */
            traceEVENT_GROUP_WAIT_BITS_BLOCK( xEventGroup, uxBitsToWaitFor );
        }
    }
    /* �ָ��������񣬲���ȡ�Ƿ��Ѿ������������л� */
    xAlreadyYielded = xTaskResumeAll();

    /* ���ָ���˵ȴ�ʱ�䣬�������ܵ�������� */
    if( xTicksToWait != ( TickType_t ) 0 )
    {
        /* �����û�в��������л�����API�ڲ����������ò� */
        if( xAlreadyYielded == pdFALSE )
        {
            portYIELD_WITHIN_API();
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
        }

        /* ���������Եȴ��������λ������ - ��ʱҪô�����λ�ѱ����ã�Ҫô����ʱ���ѹ��ڡ�
           ��������λ�ѱ����ã����ǽ����洢��������¼��б����У�����Ӧ�ü������������ */
        uxReturn = uxTaskResetEventItemValue();

        /* ��������Ƿ���ʱ������λ���ö�������� */
        if( ( uxReturn & eventUNBLOCKED_DUE_TO_BIT_SET ) == ( EventBits_t ) 0 )
        {
            /* �����ٽ��������¼������ */
            taskENTER_CRITICAL();
            {
                /* ����ʱ�����ص�ǰ�¼�λֵ */
                uxReturn = pxEventBits->uxEventBits;

                /* �п����¼�λ�ڴ������뿪����״̬���ٴ�����֮�䱻���� */
                if( prvTestWaitCondition( uxReturn, uxBitsToWaitFor, xWaitForAllBits ) != pdFALSE )
                {
                    /* ��ʹ��ʱ����������������㣬���������λ�������λ */
                    if( xClearOnExit != pdFALSE )
                    {
                        pxEventBits->uxEventBits &= ~uxBitsToWaitFor;
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
            taskEXIT_CRITICAL();

            /* ��ֹ���ٺ�δʹ��ʱ���������� */
            xTimeoutOccurred = pdFALSE;
        }
        else
        {
            /* ������Ϊλ�����ö�������� */
        }

        /* ��������������˿��������˿���λ����Ҫ�������λ */
        uxReturn &= ~eventEVENT_BITS_CONTROL_BYTES;
    }
    
    /* �����¼���ȴ�λ�����¼� */
    traceEVENT_GROUP_WAIT_BITS_END( xEventGroup, uxBitsToWaitFor, xTimeoutOccurred );

    /* �����¼�λֵ */
    return uxReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xEventGroupClearBits
 * ��������������¼�����ָ����λ�����������ǰ���¼���ֵ
 *           �˺�������ԭ���Ե�����¼����е��ض�λ��ͨ�������ֶ������¼���־
 * ���������
 *   - xEventGroup: �¼�������ָ��Ҫ�������¼���
 *   - uxBitsToClear: Ҫ������¼�λ���룬ָ����Ҫ������¼�λ
 * �����������
 * �� �� ֵ��
 *   - EventBits_t: �������λ֮ǰ���¼���ֵ
 * ����˵����
 *   - �˺������ٽ�����ִ�У�ȷ��������ԭ����
 *   - ֻ������û�������¼�λ����������ں�ʹ�õĿ���λ
 *   - �ṩ���ٹ��ܣ���¼���λ����
 *   - �������ǰ��ֵ�����ڵ������˽��¼���֮ǰ��״̬
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
EventBits_t xEventGroupClearBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear )
{
    EventGroup_t *pxEventBits = ( EventGroup_t * ) xEventGroup;  /* �����ת��Ϊ�¼���ṹָ�� */
    EventBits_t uxReturn;                                        /* ����ֵ���洢���ǰ���¼���ֵ */

    /* ����û�û�г�������ں˱���ʹ�õ�λ */
    configASSERT( xEventGroup );
    configASSERT( ( uxBitsToClear & eventEVENT_BITS_CONTROL_BYTES ) == 0 );

    /* �����ٽ�����ȷ�����������ԭ���� */
    taskENTER_CRITICAL();
    {
        /* �����¼������λ���� */
        traceEVENT_GROUP_CLEAR_BITS( xEventGroup, uxBitsToClear );

        /* ����ֵ�����λ֮ǰ���¼���ֵ */
        uxReturn = pxEventBits->uxEventBits;

        /* ���ָ����λ��ʹ��λ������� */
        pxEventBits->uxEventBits &= ~uxBitsToClear;
    }
    /* �˳��ٽ��� */
    taskEXIT_CRITICAL();

    /* �������λ֮ǰ���¼���ֵ */
    return uxReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xEventGroupClearBitsFromISR
 * �������������жϷ������(ISR)������¼����е��ض�λ��ͨ���ӳٻص�����ʵ��
 *           �˺�����xEventGroupClearBits���жϰ�ȫ�汾��ͨ����ʱ���ػ�����ִ��ʵ���������
 * ���������
 *   - xEventGroup: �¼�������ָ��Ҫ�������¼���
 *   - uxBitsToClear: Ҫ������¼�λ���룬ָ����Ҫ������¼�λ
 * �����������
 * �� �� ֵ��
 *   - BaseType_t: ����ɹ�����������͵���ʱ��������У��򷵻�pdPASS
 *                 �����ʱ����������������޷����������򷵻�pdFAIL
 * ����˵����
 *   - �˺����������ø��ٹ��ܡ���ʱ��pend�������úͶ�ʱ������ʱ����
 *   - ͨ��xTimerPendFunctionCallFromISR����������ӳٵ���ʱ���ػ�������ִ��
 *   - ʵ�����������vEventGroupClearBitsCallback����ִ��
 *   - �������жϷ����������Ҫ����¼�λ�ĳ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 1 ) )

BaseType_t xEventGroupClearBitsFromISR( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear )
{
    BaseType_t xReturn;  /* ����ֵ����ʾ�����Ƿ�ɹ� */

    /* �����¼�����ж����λ���� */
    traceEVENT_GROUP_CLEAR_BITS_FROM_ISR( xEventGroup, uxBitsToClear );
    
    /* ͨ����ʱ��pend�������ô�ISR���ӳ�ִ���������
       ��vEventGroupClearBitsCallback�������¼�������Ҫ�����λ��Ϊ�������� */
    xReturn = xTimerPendFunctionCallFromISR( vEventGroupClearBitsCallback,  /* �ص����� */
                                             ( void * ) xEventGroup,        /* �¼�������Ϊ���� */
                                             ( uint32_t ) uxBitsToClear,    /* Ҫ�����λ��Ϊ���� */
                                             NULL );                         /* ����Ҫ����ֵ��ָ�� */

    /* ���ز������ */
    return xReturn;
}

#endif
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xEventGroupGetBitsFromISR
 * �������������жϷ������(ISR)�а�ȫ�ػ�ȡ�¼���ĵ�ǰλֵ
 *           �˺�����xEventGroupGetBits���жϰ�ȫ�汾��ͨ���ж����뱣��ȷ��ԭ���Զ�ȡ
 * ���������
 *   - xEventGroup: �¼�������ָ��Ҫ��ȡ���¼���
 * �����������
 * �� �� ֵ��
 *   - EventBits_t: ��ǰ�¼����λֵ
 * ����˵����
 *   - �˺�����������жϷ������(ISR)�е���
 *   - ͨ���ж����뱣��ȷ����ȡ������ԭ����
 *   - �����������ʺ����ж���������ʹ��
 *   - ���ص��¼�λֵ���ܰ����ں˿���λ����������Ҫ�ʵ�����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
EventBits_t xEventGroupGetBitsFromISR( EventGroupHandle_t xEventGroup )
{
    UBaseType_t uxSavedInterruptStatus;                         /* �����ж�״̬�����ڻָ��ж����� */
    EventGroup_t *pxEventBits = ( EventGroup_t * ) xEventGroup; /* �����ת��Ϊ�¼���ṹָ�� */
    EventBits_t uxReturn;                                       /* ����ֵ���洢�¼���ĵ�ǰλֵ */

    /* �����ж����룬���浱ǰ�ж�״̬����ֹ�жϸ��Ŷ�ȡ���� */
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        /* ��ȫ�ض�ȡ�¼���ĵ�ǰλֵ */
        uxReturn = pxEventBits->uxEventBits;
    }
    /* ����ж����룬�ָ�֮ǰ���ж�״̬ */
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

    /* �����¼���ĵ�ǰλֵ */
    return uxReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xEventGroupSetBits
 * ���������������¼����е��ض�λ��������Ƿ�����������Щλ�����ö�����ȴ�����
 *           �˺������¼���ĺ��Ĺ��ܣ����������¼�λ�����ѵȴ���Щλ������
 * ���������
 *   - xEventGroup: �¼�������ָ��Ҫ�������¼���
 *   - uxBitsToSet: Ҫ���õ��¼�λ���룬ָ����Ҫ���õ��¼�λ
 * �����������
 * �� �� ֵ��
 *   - EventBits_t: ���ò������Ӧλ����¼���ֵ
 * ����˵����
 *   - �˺���������ָ�����¼�λ�������ȴ���Щλ������
 *   - ��������ȴ����������񣬻���������λ�����Ƿ�����¼�λ
 *   - �ỽ����������ȴ�����������
 *   - ���ٽ�����ִ�к��Ĳ�����ȷ��������ԭ����
 *   - �ṩ���ٹ��ܣ���¼����λ����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
EventBits_t xEventGroupSetBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet )
{
    ListItem_t *pxListItem, *pxNext;                          /* ������ָ�����һ��������ָ�� */
    ListItem_t const *pxListEnd;                              /* �����������ָ�� */
    List_t *pxList;                                           /* ָ��ȴ�λ�����б���ָ�� */
    EventBits_t uxBitsToClear = 0, uxBitsWaitedFor, uxControlBits; /* Ҫ�����λ������ȴ���λ������λ */
    EventGroup_t *pxEventBits = ( EventGroup_t * ) xEventGroup; /* �����ת��Ϊ�¼���ṹָ�� */
    BaseType_t xMatchFound = pdFALSE;                         /* ����Ƿ��ҵ�ƥ������� */

    /* ����û�û�г��������ں˱���ʹ�õ�λ */
    configASSERT( xEventGroup );
    configASSERT( ( uxBitsToSet & eventEVENT_BITS_CONTROL_BYTES ) == 0 );

    /* ��ȡ�ȴ�λ�����б����б�������� */
    pxList = &( pxEventBits->xTasksWaitingForBits );
    pxListEnd = listGET_END_MARKER( pxList ); /*lint !e826 !e740 ʹ�������б��ṹ��Ϊ�б������Խ�ʡRAM�����Ǿ����������Ч�� */
    
    /* �����������񣬷�ֹ�ڲ����¼���ʱ������������ */
    vTaskSuspendAll();
    {
        /* �����¼�������λ���� */
        traceEVENT_GROUP_SET_BITS( xEventGroup, uxBitsToSet );

        /* ��ȡ�ȴ��б��ĵ�һ���� */
        pxListItem = listGET_HEAD_ENTRY( pxList );

        /* ����ָ��λ */
        pxEventBits->uxEventBits |= uxBitsToSet;

        /* ����µ�λֵ�Ƿ�Ӧ�ý���κ���������� */
        while( pxListItem != pxListEnd )
        {
            /* ��ȡ��һ���б���ڵ�ǰ����ܱ��Ƴ�ǰ���棩 */
            pxNext = listGET_NEXT( pxListItem );
            /* ��ȡ����ȴ���λֵ����������λ�� */
            uxBitsWaitedFor = listGET_LIST_ITEM_VALUE( pxListItem );
            xMatchFound = pdFALSE;

            /* �ӿ���λ�з��������ȴ���λ */
            uxControlBits = uxBitsWaitedFor & eventEVENT_BITS_CONTROL_BYTES;
            uxBitsWaitedFor &= ~eventEVENT_BITS_CONTROL_BYTES;

            /* ���ȴ������Ƿ����� */
            if( ( uxControlBits & eventWAIT_FOR_ALL_BITS ) == ( EventBits_t ) 0 )
            {
                /* ֻ��Ҫ����λ�����ã���һλ��λ�� */
                if( ( uxBitsWaitedFor & pxEventBits->uxEventBits ) != ( EventBits_t ) 0 )
                {
                    xMatchFound = pdTRUE;  /* �ҵ�ƥ�� */
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
                }
            }
            else if( ( uxBitsWaitedFor & pxEventBits->uxEventBits ) == uxBitsWaitedFor )
            {
                /* ������Ҫ��λ�������� */
                xMatchFound = pdTRUE;  /* �ҵ�ƥ�� */
            }
            else
            {
                /* ��Ҫ����λ�������ã�����������λ�������� */
            }

            /* ����ҵ�ƥ�䣬���������Ѻ�λ��� */
            if( xMatchFound != pdFALSE )
            {
                /* λƥ�䡣�Ƿ�Ӧ�����˳�ʱ���λ�� */
                if( ( uxControlBits & eventCLEAR_EVENTS_ON_EXIT_BIT ) != ( EventBits_t ) 0 )
                {
                    uxBitsToClear |= uxBitsWaitedFor;  /* ��¼��Ҫ�����λ */
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
                }

                /* �ڽ�������¼��б����Ƴ�֮ǰ����ʵ�ʵ��¼���־ֵ�洢��������¼��б����С�
                   ����eventUNBLOCKED_DUE_TO_BIT_SETλ��������֪�����������������λƥ�������������ģ�
                   ��������Ϊ��ʱ */
                ( void ) xTaskRemoveFromUnorderedEventList( pxListItem, pxEventBits->uxEventBits | eventUNBLOCKED_DUE_TO_BIT_SET );
            }

            /* �ƶ�����һ���б��ע�����ﲻʹ��pxListItem->pxNext����Ϊ�б�������Ѵ��¼��б����Ƴ�
               �����뵽����/�����ȡ�б��� */
            pxListItem = pxNext;
        }

        /* ����κ��ڿ�������������eventCLEAR_EVENTS_ON_EXIT_BITλ��ƥ��ʱ��Ҫ�����λ */
        pxEventBits->uxEventBits &= ~uxBitsToClear;
    }
    /* �ָ������������ */
    ( void ) xTaskResumeAll();

    /* ���ص�ǰ�¼����ֵ���������Ӧλ�� */
    return pxEventBits->uxEventBits;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�vEventGroupDelete
 * ����������ɾ���¼�����󣬻������еȴ����¼�������񣬲��ͷ������Դ
 *           �˺��������¼���������������������������Ѻ��ڴ��ͷ�
 * ���������
 *   - xEventGroup: �¼�������ָ��Ҫɾ�����¼���
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺����ỽ�����еȴ����¼�������񣬲�����0��Ϊ�¼�λֵ����Ϊ�¼������ڱ�ɾ����
 *   - �����¼���ķ��䷽ʽ����̬��̬�������Ƿ��ͷ��ڴ�
 *   - �ڹ�����������������ִ�к��Ĳ�����ȷ��������һ����
 *   - �ṩ���ٹ��ܣ���¼�¼���ɾ������
 *   - ɾ�����¼�����������Ч����Ӧ����ʹ��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
void vEventGroupDelete( EventGroupHandle_t xEventGroup )
{
    EventGroup_t *pxEventBits = ( EventGroup_t * ) xEventGroup;  /* �����ת��Ϊ�¼���ṹָ�� */
    const List_t *pxTasksWaitingForBits = &( pxEventBits->xTasksWaitingForBits );  /* ��ȡ�ȴ�λ�����б� */

    /* �����������񣬷�ֹ��ɾ���¼���ʱ������������ */
    vTaskSuspendAll();
    {
        /* �����¼���ɾ������ */
        traceEVENT_GROUP_DELETE( xEventGroup );

        /* ѭ���������еȴ����¼�������� */
        while( listCURRENT_LIST_LENGTH( pxTasksWaitingForBits ) > ( UBaseType_t ) 0 )
        {
            /* ������������������0��Ϊ�¼�λֵ����Ϊ�¼��б����ڱ�ɾ����
               ��˲������κ�λ������ */
            configASSERT( pxTasksWaitingForBits->xListEnd.pxNext != ( ListItem_t * ) &( pxTasksWaitingForBits->xListEnd ) );
            ( void ) xTaskRemoveFromUnorderedEventList( pxTasksWaitingForBits->xListEnd.pxNext, eventUNBLOCKED_DUE_TO_BIT_SET );
        }

        /* �����ڴ�������ô����¼����ڴ��ͷ� */
        #if( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 0 ) )
        {
            /* �¼���ֻ���Ƕ�̬����� - �ٴ��ͷ��� */
            vPortFree( pxEventBits );
        }
        #elif( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
        {
            /* �¼�������Ǿ�̬��̬����ģ�����ڳ����ͷ��ڴ�ǰ���м�� */
            if( pxEventBits->ucStaticallyAllocated == ( uint8_t ) pdFALSE )
            {
                vPortFree( pxEventBits );  /* ��̬������¼��飬�ͷ��ڴ� */
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�ǣ���̬���䣬����Ҫ�ͷţ� */
            }
        }
        #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
    }
    /* �ָ������������ */
    ( void ) xTaskResumeAll();
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�vEventGroupSetBitsCallback
 * �����������¼�������λ�ص����������ڴ��ж��ӳٴ�����ִ������λ����
 *           �˺������ڲ�ʹ�ã��������жϷ�������ӳ��ύ������λ����
 * ���������
 *   - pvEventGroup: �¼�������ָ�룬��Ҫת��ΪEventGroupHandle_t����ʹ��
 *   - ulBitsToSet: Ҫ���õ��¼�λ���룬ָ����Ҫ���õ��¼�λ
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺�����FreeRTOS�ڲ�ʹ�õĻص���������Ӧ��Ӧ�ó���ֱ�ӵ���
 *   - ͨ��xTimerPendFunctionCallFromISR���жϷ�������ӳٵ���
 *   - ʵ�ʵ���xEventGroupSetBits����ִ������λ����
 *   - �����������������а�ȫ��ִ���ж���������¼������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/

/* �����ڲ�ʹ�� - ִ�д��жϹ����"����λ"���� */
void vEventGroupSetBitsCallback( void *pvEventGroup, const uint32_t ulBitsToSet )
{
    /* ����xEventGroupSetBits����ʵ��ִ������λ����
       ������ת��Ϊ�ʵ������� */
    ( void ) xEventGroupSetBits( pvEventGroup, ( EventBits_t ) ulBitsToSet );
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�vEventGroupClearBitsCallback
 * �����������¼������λ�ص����������ڴ��ж��ӳٴ�����ִ�����λ����
 *           �˺������ڲ�ʹ�ã��������жϷ�������ӳ��ύ�����λ����
 * ���������
 *   - pvEventGroup: �¼�������ָ�룬��Ҫת��ΪEventGroupHandle_t����ʹ��
 *   - ulBitsToClear: Ҫ������¼�λ���룬ָ����Ҫ������¼�λ
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺�����FreeRTOS�ڲ�ʹ�õĻص���������Ӧ��Ӧ�ó���ֱ�ӵ���
 *   - ͨ��xTimerPendFunctionCallFromISR���жϷ�������ӳٵ���
 *   - ʵ�ʵ���xEventGroupClearBits����ִ�����λ����
 *   - �����������������а�ȫ��ִ���ж���������¼������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/

/* �����ڲ�ʹ�� - ִ�д��жϹ����"���λ"���� */
void vEventGroupClearBitsCallback( void *pvEventGroup, const uint32_t ulBitsToClear )
{
    /* ����xEventGroupClearBits����ʵ��ִ�����λ����
       ������ת��Ϊ�ʵ������� */
    ( void ) xEventGroupClearBits( pvEventGroup, ( EventBits_t ) ulBitsToClear );
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvTestWaitCondition
 * ���������������¼��鵱ǰλֵ�Ƿ�����ȴ�����
 *           �˺�����FreeRTOS�¼���ģ����ڲ����������������ж��¼�λ�Ƿ���������ĵȴ�����
 * ���������
 *   - uxCurrentEventBits: �¼���ĵ�ǰλֵ�����������¼�λ�ĵ�ǰ״̬
 *   - uxBitsToWaitFor: Ҫ�ȴ����¼�λ���룬ָ������ϣ���ȴ����¼�λ
 *   - xWaitForAllBits: �ȴ��������ͣ�ָ������Ҫ����λ�����û�����һλ�ñ�����
 *                     pdTRUE: ��Ҫ����ָ��λ������
 *                     pdFALSE: ��Ҫ��һָ��λ������
 * �����������
 * �� �� ֵ��
 *   - BaseType_t: �������ȴ�����������pdTRUE�����򷵻�pdFALSE
 * ����˵����
 *   - �˺����Ǿ�̬����������FreeRTOS�ں��ڲ�ʹ��
 *   - ����xWaitForAllBits���������������λ������һλ
 *   - ʹ��λ�������и�Ч���������
 *   - �������Ը����ʱ�ǣ����ڴ��븲���ʷ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
static BaseType_t prvTestWaitCondition( const EventBits_t uxCurrentEventBits, const EventBits_t uxBitsToWaitFor, const BaseType_t xWaitForAllBits )
{
    BaseType_t xWaitConditionMet = pdFALSE;  /* �ȴ������Ƿ�����ı�־����ʼ��Ϊ������ */

    /* ���ݵȴ��������ͽ��в�ͬ�ļ�� */
    if( xWaitForAllBits == pdFALSE )
    {
        /* ����ֻ��Ҫ�ȴ�uxBitsToWaitFor�е�һ��λ�����á�
           �Ƿ��Ѿ�������һ��λ�������ˣ� */
        if( ( uxCurrentEventBits & uxBitsToWaitFor ) != ( EventBits_t ) 0 )
        {
            xWaitConditionMet = pdTRUE;  /* �������㣺������һ���ȴ�λ������ */
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
        }
    }
    else
    {
        /* ������Ҫ�ȴ�uxBitsToWaitFor�е�����λ�������á�
           �����Ƿ��Ѿ������ˣ� */
        if( ( uxCurrentEventBits & uxBitsToWaitFor ) == uxBitsToWaitFor )
        {
            xWaitConditionMet = pdTRUE;  /* �������㣺���еȴ�λ�������� */
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
        }
    }

    /* ���صȴ������Ƿ�����Ľ�� */
    return xWaitConditionMet;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xEventGroupSetBitsFromISR
 * �������������жϷ������(ISR)�������¼����е��ض�λ��ͨ���ӳٻص�����ʵ��
 *           �˺�����xEventGroupSetBits���жϰ�ȫ�汾��ͨ����ʱ���ػ�����ִ��ʵ�����ò���
 * ���������
 *   - xEventGroup: �¼�������ָ��Ҫ�������¼���
 *   - uxBitsToSet: Ҫ���õ��¼�λ���룬ָ����Ҫ���õ��¼�λ
 *   - pxHigherPriorityTaskWoken: ָ��BaseType_t��ָ�룬����ָʾ�Ƿ��и������ȼ����񱻻���
 * ���������
 *   - pxHigherPriorityTaskWoken: ����ӳٴ������¸������ȼ����������������ΪpdTRUE
 * �� �� ֵ��
 *   - BaseType_t: ����ɹ������������͵���ʱ��������У��򷵻�pdPASS
 *                 �����ʱ����������������޷����������򷵻�pdFAIL
 * ����˵����
 *   - �˺����������ø��ٹ��ܡ���ʱ��pend�������úͶ�ʱ������ʱ����
 *   - ͨ��xTimerPendFunctionCallFromISR�����ò����ӳٵ���ʱ���ػ�������ִ��
 *   - ʵ�����ò�����vEventGroupSetBitsCallback����ִ��
 *   - �������жϷ����������Ҫ�����¼�λ�ĳ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 1 ) )

BaseType_t xEventGroupSetBitsFromISR( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet, BaseType_t *pxHigherPriorityTaskWoken )
{
    BaseType_t xReturn;  /* ����ֵ����ʾ�����Ƿ�ɹ� */

    /* �����¼�����ж�����λ���� */
    traceEVENT_GROUP_SET_BITS_FROM_ISR( xEventGroup, uxBitsToSet );
    
    /* ͨ����ʱ��pend�������ô�ISR���ӳ�ִ�����ò���
       ��vEventGroupSetBitsCallback�������¼�������Ҫ���õ�λ��Ϊ�������� */
    xReturn = xTimerPendFunctionCallFromISR( vEventGroupSetBitsCallback,  /* �ص����� */
                                             ( void * ) xEventGroup,      /* �¼�������Ϊ���� */
                                             ( uint32_t ) uxBitsToSet,    /* Ҫ���õ�λ��Ϊ���� */
                                             pxHigherPriorityTaskWoken ); /* �������ȼ������ѱ�־ */

    /* ���ز������ */
    return xReturn;
}

#endif
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�uxEventGroupGetNumber
 * ������������ȡ�¼����Ψһ��ţ����ڵ��Ժ͸���Ŀ��
 *           �˺��������¼����Ψһ��ʶ��ţ������ڵ��Ժ͸��ٹ�����ʶ���ض��¼���
 * ���������
 *   - xEventGroup: �¼�������ָ�룬ָ��Ҫ��ѯ���¼���
 * �����������
 * �� �� ֵ��
 *   - UBaseType_t: �¼����Ψһ��ţ��������NULLָ���򷵻�0
 * ����˵����
 *   - �˺����������ø��ٹ���ʱ���루configUSE_TRACE_FACILITY == 1��
 *   - ��Ҫ���ڵ��Ժ͸���Ŀ�ģ�����ʶ������ֲ�ͬ���¼���
 *   - �������NULLָ�룬�����ᰲȫ�ط���0�����Ǳ���
 *   - �¼��������¼��鴴��ʱ���䣬ͨ���ǵ�����Ψһֵ
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
#if (configUSE_TRACE_FACILITY == 1)

UBaseType_t uxEventGroupGetNumber( void* xEventGroup )
{
    UBaseType_t xReturn;                         /* ����ֵ���洢�¼����� */
    EventGroup_t *pxEventBits = ( EventGroup_t * ) xEventGroup;  /* �����ת��Ϊ�¼���ṹָ�� */

    /* ��鴫����¼������Ƿ�ΪNULL */
    if( xEventGroup == NULL )
    {
        xReturn = 0;  /* �������NULL������0 */
    }
    else
    {
        /* ���¼���ṹ�л�ȡ�¼����� */
        xReturn = pxEventBits->uxEventGroupNumber;
    }

    /* �����¼����� */
    return xReturn;
}

#endif


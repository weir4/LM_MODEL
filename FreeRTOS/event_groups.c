/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： LM_event_groups.c
 * 文件标识： 
 * 内容摘要： 事件组模块定义
 * 其它说明： 无
 * 当前版本： FreeRTOS V9.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年09月13日
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
 * 函数名称：xEventGroupCreateStatic
 * 功能描述：静态创建事件组对象，使用预分配的内存缓冲区
 *           事件组是FreeRTOS中用于任务间同步和事件通知的机制，支持多事件位操作
 * 输入参数：
 *   - pxEventGroupBuffer: 指向预分配的静态事件组内存缓冲区的指针
 * 输出参数：无
 * 返 回 值：
 *   - EventGroupHandle_t: 成功创建时返回事件组句柄，失败时返回NULL
 * 其它说明：
 *   - 此函数仅在启用静态分配时编译（configSUPPORT_STATIC_ALLOCATION == 1）
 *   - 事件组内存由用户预先分配，不涉及动态内存分配
 *   - 初始化事件位为0，表示所有事件位均未设置
 *   - 初始化等待事件位的任务列表
 *   - 支持跟踪事件组创建成功或失败
 *   - 适用于内存受限或需要精确内存管理的嵌入式系统
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
#if( configSUPPORT_STATIC_ALLOCATION == 1 )

EventGroupHandle_t xEventGroupCreateStatic( StaticEventGroup_t *pxEventGroupBuffer )
{
    EventGroup_t *pxEventBits;  /* 指向事件组对象的指针 */

    /* 必须提供StaticEventGroup_t对象，使用断言检查指针有效性 */
    configASSERT( pxEventGroupBuffer );

    /* 用户提供了静态分配的事件组内存 - 使用它 */
    pxEventBits = ( EventGroup_t * ) pxEventGroupBuffer; /*lint !e740 EventGroup_t和StaticEventGroup_t保证具有相同的大小和对齐要求 - 由configASSERT()检查 */

    /* 检查指针是否有效 */
    if( pxEventBits != NULL )
    {
        /* 初始化事件位为0，表示所有事件位均未设置 */
        pxEventBits->uxEventBits = 0;
        
        /* 初始化等待事件位的任务列表 */
        vListInitialise( &( pxEventBits->xTasksWaitingForBits ) );

        /* 如果同时支持动态分配，标记事件组的分配方式 */
        #if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
        {
            /* 静态和动态分配都可以使用，因此标记此事件组是静态创建的，
               以便在后续删除事件组时正确处理 */
            pxEventBits->ucStaticallyAllocated = pdTRUE;
        }
        #endif /* configSUPPORT_DYNAMIC_ALLOCATION */

        /* 跟踪事件组创建成功 */
        traceEVENT_GROUP_CREATE( pxEventBits );
    }
    else
    {
        /* 跟踪事件组创建失败 */
        traceEVENT_GROUP_CREATE_FAILED();
    }

    /* 返回事件组句柄 */
    return ( EventGroupHandle_t ) pxEventBits;
}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xEventGroupCreate
 * 功能描述：动态创建事件组对象，使用系统动态内存分配
 *           事件组是FreeRTOS中用于任务间同步和事件通知的机制，支持多事件位操作
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：
 *   - EventGroupHandle_t: 成功创建时返回事件组句柄，失败时返回NULL
 * 其它说明：
 *   - 此函数仅在启用动态分配时编译（configSUPPORT_DYNAMIC_ALLOCATION == 1）
 *   - 事件组内存由系统动态分配，使用pvPortMalloc函数
 *   - 初始化事件位为0，表示所有事件位均未设置
 *   - 初始化等待事件位的任务列表
 *   - 支持跟踪事件组创建成功或失败
 *   - 适用于支持动态内存分配的系统，提供更灵活的内存管理
 *   - 创建的事件组可以使用vEventGroupDelete函数删除并释放内存
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

EventGroupHandle_t xEventGroupCreate( void )
{
    EventGroup_t *pxEventBits;  /* 指向事件组对象的指针 */

    /* 分配事件组内存，使用系统的动态内存分配函数 */
    pxEventBits = ( EventGroup_t * ) pvPortMalloc( sizeof( EventGroup_t ) );

    /* 检查内存是否分配成功 */
    if( pxEventBits != NULL )
    {
        /* 初始化事件位为0，表示所有事件位均未设置 */
        pxEventBits->uxEventBits = 0;
        
        /* 初始化等待事件位的任务列表 */
        vListInitialise( &( pxEventBits->xTasksWaitingForBits ) );

        /* 如果同时支持静态分配，标记事件组的分配方式 */
        #if( configSUPPORT_STATIC_ALLOCATION == 1 )
        {
            /* 静态和动态分配都可以使用，因此标记此事件组是动态分配的，
               以便在后续删除事件组时正确处理内存释放 */
            pxEventBits->ucStaticallyAllocated = pdFALSE;
        }
        #endif /* configSUPPORT_STATIC_ALLOCATION */

        /* 跟踪事件组创建成功 */
        traceEVENT_GROUP_CREATE( pxEventBits );
    }
    else
    {
        /* 跟踪事件组创建失败 */
        traceEVENT_GROUP_CREATE_FAILED();
    }

    /* 返回事件组句柄 */
    return ( EventGroupHandle_t ) pxEventBits;
}

#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xEventGroupSync
 * 功能描述：事件组同步函数，用于实现多个任务间的 rendezvous（汇合点）同步
 *           设置指定的事件位，然后等待所有指定的事件位被设置（可能由其他任务设置）
 *           这是一个强大的同步原语，允许多个任务在特定点等待彼此
 * 输入参数：
 *   - xEventGroup: 事件组句柄，指定要操作的事件组
 *   - uxBitsToSet: 要设置的事件位，调用任务希望设置的事件位掩码
 *   - uxBitsToWaitFor: 要等待的事件位，调用任务需要等待的事件位掩码
 *   - xTicksToWait: 最大等待时间（以时钟节拍为单位），可以是portMAX_DELAY表示无限等待
 * 输出参数：无
 * 返 回 值：
 *   - EventBits_t: 返回事件组的值（在清除等待位之前）
 *                 如果因超时返回，返回的是超时时刻的事件组值
 *                 如果因所有等待位被设置而返回，返回的是设置位后的原始事件组值
 * 其它说明：
 *   - 此函数会设置指定的事件位，然后等待所有指定的事件位被设置
 *   - 如果所有等待位已经设置（包括刚刚设置的位），函数立即返回
 *   - 如果等待位未全部设置，任务将进入阻塞状态，直到所有等待位被设置或超时
 *   - 在成功同步后（所有等待位被设置），这些等待位会被自动清除
 *   - 这是一个阻塞调用，可能会引起任务上下文切换
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
EventBits_t xEventGroupSync( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet, const EventBits_t uxBitsToWaitFor, TickType_t xTicksToWait )
{
    EventBits_t uxOriginalBitValue, uxReturn;              /* 原始事件位值和返回值 */
    EventGroup_t *pxEventBits = ( EventGroup_t * ) xEventGroup;  /* 将句柄转换为事件组结构指针 */
    BaseType_t xAlreadyYielded;                            /* 标记是否已经产生过任务切换 */
    BaseType_t xTimeoutOccurred = pdFALSE;                 /* 标记是否发生超时 */

    /* 断言检查：确保等待位不包含控制字节，且等待位不为0 */
    configASSERT( ( uxBitsToWaitFor & eventEVENT_BITS_CONTROL_BYTES ) == 0 );
    configASSERT( uxBitsToWaitFor != 0 );
    
    /* 如果包含调度器状态查询或使用定时器，检查调度器状态 */
    #if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
    {
        /* 断言检查：如果调度器挂起且指定了非零等待时间，则报错 */
        configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
    }
    #endif

    /* 挂起所有任务，防止在操作事件组时被其他任务打断 */
    vTaskSuspendAll();
    {
        /* 获取当前事件位的原始值 */
        uxOriginalBitValue = pxEventBits->uxEventBits;

        /* 设置指定的事件位 */
        ( void ) xEventGroupSetBits( xEventGroup, uxBitsToSet );

        /* 检查是否所有等待位都已经设置（包括刚刚设置的位） */
        if( ( ( uxOriginalBitValue | uxBitsToSet ) & uxBitsToWaitFor ) == uxBitsToWaitFor )
        {
            /* 所有汇合位现在已经设置 - 不需要阻塞 */
            uxReturn = ( uxOriginalBitValue | uxBitsToSet );

            /* 汇合操作总是清除等待位。除非这是汇合中的唯一任务，否则它们已经被清除 */
            pxEventBits->uxEventBits &= ~uxBitsToWaitFor;

            /* 设置等待时间为0，表示不需要阻塞 */
            xTicksToWait = 0;
        }
        else
        {
            /* 如果有指定的等待时间，将任务放入阻塞状态 */
            if( xTicksToWait != ( TickType_t ) 0 )
            {
                /* 跟踪事件组同步阻塞事件 */
                traceEVENT_GROUP_SYNC_BLOCK( xEventGroup, uxBitsToSet, uxBitsToWaitFor );

                /* 将调用任务正在等待的位存储到任务的事件列表项中，
                   这样内核知道何时找到匹配项。然后进入阻塞状态 */
                vTaskPlaceOnUnorderedEventList( &( pxEventBits->xTasksWaitingForBits ), 
                                                ( uxBitsToWaitFor | eventCLEAR_EVENTS_ON_EXIT_BIT | eventWAIT_FOR_ALL_BITS ), 
                                                xTicksToWait );

                /* 这个赋值是过时的，因为uxReturn将在任务解除阻塞后设置，
                   但如果不进行此赋值，一些编译器会错误地生成关于uxReturn返回时未设置的警告 */
                uxReturn = 0;
            }
            else
            {
                /* 汇合位没有设置，但没有指定阻塞时间 - 只需返回当前事件位值 */
                uxReturn = pxEventBits->uxEventBits;
            }
        }
    }
    /* 恢复所有任务，并获取是否已经产生过任务切换 */
    xAlreadyYielded = xTaskResumeAll();

    /* 如果指定了等待时间，处理可能的阻塞情况 */
    if( xTicksToWait != ( TickType_t ) 0 )
    {
        /* 如果还没有产生任务切换，在API内部进行任务让步 */
        if( xAlreadyYielded == pdFALSE )
        {
            portYIELD_WITHIN_API();
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
        }

        /* 任务阻塞以等待其所需的位被设置 - 此时要么所需的位已被设置，要么阻塞时间已过期。
           如果所需的位已被设置，它们将被存储在任务的事件列表项中，现在应该检索并清除它们 */
        uxReturn = uxTaskResetEventItemValue();

        /* 检查任务是否因超时而非因位设置而解除阻塞 */
        if( ( uxReturn & eventUNBLOCKED_DUE_TO_BIT_SET ) == ( EventBits_t ) 0 )
        {
            /* 任务超时，返回当前事件位值 */
            taskENTER_CRITICAL();
            {
                uxReturn = pxEventBits->uxEventBits;

                /* 虽然任务因为超时而到达这里，但在它解除阻塞后，有可能另一个任务已经设置了位。
                   如果是这种情况，那么它需要在退出前清除这些位 */
                if( ( uxReturn & uxBitsToWaitFor ) == uxBitsToWaitFor )
                {
                    pxEventBits->uxEventBits &= ~uxBitsToWaitFor;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
                }
            }
            taskEXIT_CRITICAL();

            /* 标记发生了超时 */
            xTimeoutOccurred = pdTRUE;
        }
        else
        {
            /* 任务因为位被设置而解除阻塞 */
        }

        /* 控制位可能被设置，因为任务曾经阻塞过，不应该返回这些控制位 */
        uxReturn &= ~eventEVENT_BITS_CONTROL_BYTES;
    }

    /* 跟踪事件组同步结束事件 */
    traceEVENT_GROUP_SYNC_END( xEventGroup, uxBitsToSet, uxBitsToWaitFor, xTimeoutOccurred );

    /* 返回事件位值 */
    return uxReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xEventGroupWaitBits
 * 功能描述：等待事件组中的特定位被设置，可以选择清除位和等待条件（所有位或任一位）
 *           这是一个强大的事件等待函数，允许任务等待一个或多个事件位的特定组合
 * 输入参数：
 *   - xEventGroup: 事件组句柄，指定要操作的事件组
 *   - uxBitsToWaitFor: 要等待的事件位掩码，指定任务希望等待的事件位
 *   - xClearOnExit: 退出时是否清除等待的事件位
 *                   pdTRUE: 退出时清除uxBitsToWaitFor指定的位
 *                   pdFALSE: 退出时不清除位
 *   - xWaitForAllBits: 等待条件，指定是需要所有位被设置还是任一位置被设置
 *                      pdTRUE: 等待所有指定位被设置
 *                      pdFALSE: 等待任一指定位被设置
 *   - xTicksToWait: 最大等待时间（以时钟节拍为单位）
 *                  可以是0表示不等待立即返回
 *                  可以是portMAX_DELAY表示无限等待
 *                  可以是具体数值表示有限的等待时间
 * 输出参数：无
 * 返 回 值：
 *   - EventBits_t: 返回事件组的值（在清除位之前，如果有清除的话）
 *                 如果因超时返回，返回的是超时时刻的事件组值
 *                 如果因等待条件满足而返回，返回的是条件满足时刻的事件组值
 * 其它说明：
 *   - 此函数会检查指定的事件位是否已经满足等待条件
 *   - 如果条件已经满足，可以选择立即返回并清除位（如果xClearOnExit为pdTRUE）
 *   - 如果条件不满足且指定了等待时间，任务将进入阻塞状态，直到条件满足或超时
 *   - 这是一个阻塞调用，可能会引起任务上下文切换
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
EventBits_t xEventGroupWaitBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToWaitFor, const BaseType_t xClearOnExit, const BaseType_t xWaitForAllBits, TickType_t xTicksToWait )
{
    EventGroup_t *pxEventBits = ( EventGroup_t * ) xEventGroup;  /* 将句柄转换为事件组结构指针 */
    EventBits_t uxReturn, uxControlBits = 0;                     /* 返回值和控制位 */
    BaseType_t xWaitConditionMet, xAlreadyYielded;               /* 等待条件是否满足和是否已经产生任务切换 */
    BaseType_t xTimeoutOccurred = pdFALSE;                       /* 标记是否发生超时 */

    /* 检查用户没有尝试等待内核本身使用的位，并且至少请求了一个位 */
    configASSERT( xEventGroup );
    configASSERT( ( uxBitsToWaitFor & eventEVENT_BITS_CONTROL_BYTES ) == 0 );
    configASSERT( uxBitsToWaitFor != 0 );
    
    /* 如果包含调度器状态查询或使用定时器，检查调度器状态 */
    #if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
    {
        /* 断言检查：如果调度器挂起且指定了非零等待时间，则报错 */
        configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
    }
    #endif

    /* 挂起所有任务，防止在操作事件组时被其他任务打断 */
    vTaskSuspendAll();
    {
        /* 获取当前事件位的值 */
        const EventBits_t uxCurrentEventBits = pxEventBits->uxEventBits;

        /* 检查等待条件是否已经满足 */
        xWaitConditionMet = prvTestWaitCondition( uxCurrentEventBits, uxBitsToWaitFor, xWaitForAllBits );

        /* 根据等待条件是否满足进行不同的处理 */
        if( xWaitConditionMet != pdFALSE )
        {
            /* 等待条件已经满足，因此不需要阻塞 */
            uxReturn = uxCurrentEventBits;
            xTicksToWait = ( TickType_t ) 0;

            /* 如果请求，清除等待位 */
            if( xClearOnExit != pdFALSE )
            {
                pxEventBits->uxEventBits &= ~uxBitsToWaitFor;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
            }
        }
        else if( xTicksToWait == ( TickType_t ) 0 )
        {
            /* 等待条件尚未满足，但没有指定阻塞时间，因此只返回当前值 */
            uxReturn = uxCurrentEventBits;
        }
        else
        {
            /* 任务将阻塞以等待其所需的位被设置。uxControlBits用于记住此xEventGroupWaitBits()调用的指定行为 -
               用于事件位解除任务阻塞时 */
            if( xClearOnExit != pdFALSE )
            {
                uxControlBits |= eventCLEAR_EVENTS_ON_EXIT_BIT;  /* 设置退出时清除位的控制位 */
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
            }

            if( xWaitForAllBits != pdFALSE )
            {
                uxControlBits |= eventWAIT_FOR_ALL_BITS;  /* 设置等待所有位的控制位 */
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
            }

            /* 将调用任务正在等待的位存储到任务的事件列表项中，
               这样内核知道何时找到匹配项。然后进入阻塞状态 */
            vTaskPlaceOnUnorderedEventList( &( pxEventBits->xTasksWaitingForBits ), 
                                            ( uxBitsToWaitFor | uxControlBits ), 
                                            xTicksToWait );

            /* 这是过时的，因为它将在任务解除阻塞后设置，
               但如果不这样做，一些编译器会错误地生成关于变量返回时未设置的警告 */
            uxReturn = 0;

            /* 跟踪事件组等待位阻塞事件 */
            traceEVENT_GROUP_WAIT_BITS_BLOCK( xEventGroup, uxBitsToWaitFor );
        }
    }
    /* 恢复所有任务，并获取是否已经产生过任务切换 */
    xAlreadyYielded = xTaskResumeAll();

    /* 如果指定了等待时间，处理可能的阻塞情况 */
    if( xTicksToWait != ( TickType_t ) 0 )
    {
        /* 如果还没有产生任务切换，在API内部进行任务让步 */
        if( xAlreadyYielded == pdFALSE )
        {
            portYIELD_WITHIN_API();
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
        }

        /* 任务阻塞以等待其所需的位被设置 - 此时要么所需的位已被设置，要么阻塞时间已过期。
           如果所需的位已被设置，它们将被存储在任务的事件列表项中，现在应该检索并清除它们 */
        uxReturn = uxTaskResetEventItemValue();

        /* 检查任务是否因超时而非因位设置而解除阻塞 */
        if( ( uxReturn & eventUNBLOCKED_DUE_TO_BIT_SET ) == ( EventBits_t ) 0 )
        {
            /* 进入临界区保护事件组访问 */
            taskENTER_CRITICAL();
            {
                /* 任务超时，返回当前事件位值 */
                uxReturn = pxEventBits->uxEventBits;

                /* 有可能事件位在此任务离开阻塞状态和再次运行之间被更新 */
                if( prvTestWaitCondition( uxReturn, uxBitsToWaitFor, xWaitForAllBits ) != pdFALSE )
                {
                    /* 即使超时，如果条件现在满足，且请求清除位，则清除位 */
                    if( xClearOnExit != pdFALSE )
                    {
                        pxEventBits->uxEventBits &= ~uxBitsToWaitFor;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
                    }
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
                }
            }
            taskEXIT_CRITICAL();

            /* 防止跟踪宏未使用时编译器警告 */
            xTimeoutOccurred = pdFALSE;
        }
        else
        {
            /* 任务因为位被设置而解除阻塞 */
        }

        /* 任务阻塞过，因此可能设置了控制位，需要清除控制位 */
        uxReturn &= ~eventEVENT_BITS_CONTROL_BYTES;
    }
    
    /* 跟踪事件组等待位结束事件 */
    traceEVENT_GROUP_WAIT_BITS_END( xEventGroup, uxBitsToWaitFor, xTimeoutOccurred );

    /* 返回事件位值 */
    return uxReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xEventGroupClearBits
 * 功能描述：清除事件组中指定的位，并返回清除前的事件组值
 *           此函数用于原子性地清除事件组中的特定位，通常用于手动重置事件标志
 * 输入参数：
 *   - xEventGroup: 事件组句柄，指定要操作的事件组
 *   - uxBitsToClear: 要清除的事件位掩码，指定需要清除的事件位
 * 输出参数：无
 * 返 回 值：
 *   - EventBits_t: 返回清除位之前的事件组值
 * 其它说明：
 *   - 此函数在临界区内执行，确保操作的原子性
 *   - 只能清除用户定义的事件位，不能清除内核使用的控制位
 *   - 提供跟踪功能，记录清除位操作
 *   - 返回清除前的值，便于调用者了解事件组之前的状态
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
EventBits_t xEventGroupClearBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear )
{
    EventGroup_t *pxEventBits = ( EventGroup_t * ) xEventGroup;  /* 将句柄转换为事件组结构指针 */
    EventBits_t uxReturn;                                        /* 返回值，存储清除前的事件组值 */

    /* 检查用户没有尝试清除内核本身使用的位 */
    configASSERT( xEventGroup );
    configASSERT( ( uxBitsToClear & eventEVENT_BITS_CONTROL_BYTES ) == 0 );

    /* 进入临界区，确保清除操作的原子性 */
    taskENTER_CRITICAL();
    {
        /* 跟踪事件组清除位操作 */
        traceEVENT_GROUP_CLEAR_BITS( xEventGroup, uxBitsToClear );

        /* 返回值是清除位之前的事件组值 */
        uxReturn = pxEventBits->uxEventBits;

        /* 清除指定的位，使用位清除操作 */
        pxEventBits->uxEventBits &= ~uxBitsToClear;
    }
    /* 退出临界区 */
    taskEXIT_CRITICAL();

    /* 返回清除位之前的事件组值 */
    return uxReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xEventGroupClearBitsFromISR
 * 功能描述：从中断服务程序(ISR)中清除事件组中的特定位，通过延迟回调机制实现
 *           此函数是xEventGroupClearBits的中断安全版本，通过定时器守护任务执行实际清除操作
 * 输入参数：
 *   - xEventGroup: 事件组句柄，指定要操作的事件组
 *   - uxBitsToClear: 要清除的事件位掩码，指定需要清除的事件位
 * 输出参数：无
 * 返 回 值：
 *   - BaseType_t: 如果成功将清除请求发送到定时器命令队列，则返回pdPASS
 *                 如果定时器命令队列已满，无法发送请求，则返回pdFAIL
 * 其它说明：
 *   - 此函数仅在启用跟踪功能、定时器pend函数调用和定时器功能时编译
 *   - 通过xTimerPendFunctionCallFromISR将清除操作延迟到定时器守护任务中执行
 *   - 实际清除操作由vEventGroupClearBitsCallback函数执行
 *   - 适用于中断服务程序中需要清除事件位的场景
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 1 ) )

BaseType_t xEventGroupClearBitsFromISR( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear )
{
    BaseType_t xReturn;  /* 返回值，表示操作是否成功 */

    /* 跟踪事件组从中断清除位操作 */
    traceEVENT_GROUP_CLEAR_BITS_FROM_ISR( xEventGroup, uxBitsToClear );
    
    /* 通过定时器pend函数调用从ISR中延迟执行清除操作
       将vEventGroupClearBitsCallback函数、事件组句柄和要清除的位作为参数传递 */
    xReturn = xTimerPendFunctionCallFromISR( vEventGroupClearBitsCallback,  /* 回调函数 */
                                             ( void * ) xEventGroup,        /* 事件组句柄作为参数 */
                                             ( uint32_t ) uxBitsToClear,    /* 要清除的位作为参数 */
                                             NULL );                         /* 不需要返回值的指针 */

    /* 返回操作结果 */
    return xReturn;
}

#endif
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xEventGroupGetBitsFromISR
 * 功能描述：从中断服务程序(ISR)中安全地获取事件组的当前位值
 *           此函数是xEventGroupGetBits的中断安全版本，通过中断掩码保护确保原子性读取
 * 输入参数：
 *   - xEventGroup: 事件组句柄，指定要读取的事件组
 * 输出参数：无
 * 返 回 值：
 *   - EventBits_t: 当前事件组的位值
 * 其它说明：
 *   - 此函数设计用于中断服务程序(ISR)中调用
 *   - 通过中断掩码保护确保读取操作的原子性
 *   - 不会阻塞，适合在中断上下文中使用
 *   - 返回的事件位值可能包含内核控制位，调用者需要适当处理
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
EventBits_t xEventGroupGetBitsFromISR( EventGroupHandle_t xEventGroup )
{
    UBaseType_t uxSavedInterruptStatus;                         /* 保存中断状态，用于恢复中断掩码 */
    EventGroup_t *pxEventBits = ( EventGroup_t * ) xEventGroup; /* 将句柄转换为事件组结构指针 */
    EventBits_t uxReturn;                                       /* 返回值，存储事件组的当前位值 */

    /* 设置中断掩码，保存当前中断状态，防止中断干扰读取操作 */
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        /* 安全地读取事件组的当前位值 */
        uxReturn = pxEventBits->uxEventBits;
    }
    /* 清除中断掩码，恢复之前的中断状态 */
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

    /* 返回事件组的当前位值 */
    return uxReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xEventGroupSetBits
 * 功能描述：设置事件组中的特定位，并检查是否有任务因这些位的设置而满足等待条件
 *           此函数是事件组的核心功能，负责设置事件位并唤醒等待这些位的任务
 * 输入参数：
 *   - xEventGroup: 事件组句柄，指定要操作的事件组
 *   - uxBitsToSet: 要设置的事件位掩码，指定需要设置的事件位
 * 输出参数：无
 * 返 回 值：
 *   - EventBits_t: 设置并清除相应位后的事件组值
 * 其它说明：
 *   - 此函数会设置指定的事件位，并检查等待这些位的任务
 *   - 对于满足等待条件的任务，会根据其控制位决定是否清除事件位
 *   - 会唤醒所有满足等待条件的任务
 *   - 在临界区内执行核心操作，确保操作的原子性
 *   - 提供跟踪功能，记录设置位操作
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
EventBits_t xEventGroupSetBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet )
{
    ListItem_t *pxListItem, *pxNext;                          /* 链表项指针和下一个链表项指针 */
    ListItem_t const *pxListEnd;                              /* 链表结束标记指针 */
    List_t *pxList;                                           /* 指向等待位任务列表的指针 */
    EventBits_t uxBitsToClear = 0, uxBitsWaitedFor, uxControlBits; /* 要清除的位、任务等待的位、控制位 */
    EventGroup_t *pxEventBits = ( EventGroup_t * ) xEventGroup; /* 将句柄转换为事件组结构指针 */
    BaseType_t xMatchFound = pdFALSE;                         /* 标记是否找到匹配的任务 */

    /* 检查用户没有尝试设置内核本身使用的位 */
    configASSERT( xEventGroup );
    configASSERT( ( uxBitsToSet & eventEVENT_BITS_CONTROL_BYTES ) == 0 );

    /* 获取等待位任务列表和列表结束标记 */
    pxList = &( pxEventBits->xTasksWaitingForBits );
    pxListEnd = listGET_END_MARKER( pxList ); /*lint !e826 !e740 使用迷你列表结构作为列表结束以节省RAM。这是经过检查且有效的 */
    
    /* 挂起所有任务，防止在操作事件组时被其他任务打断 */
    vTaskSuspendAll();
    {
        /* 跟踪事件组设置位操作 */
        traceEVENT_GROUP_SET_BITS( xEventGroup, uxBitsToSet );

        /* 获取等待列表的第一个项 */
        pxListItem = listGET_HEAD_ENTRY( pxList );

        /* 设置指定位 */
        pxEventBits->uxEventBits |= uxBitsToSet;

        /* 检查新的位值是否应该解除任何任务的阻塞 */
        while( pxListItem != pxListEnd )
        {
            /* 获取下一个列表项（在当前项可能被移除前保存） */
            pxNext = listGET_NEXT( pxListItem );
            /* 获取任务等待的位值（包含控制位） */
            uxBitsWaitedFor = listGET_LIST_ITEM_VALUE( pxListItem );
            xMatchFound = pdFALSE;

            /* 从控制位中分离出任务等待的位 */
            uxControlBits = uxBitsWaitedFor & eventEVENT_BITS_CONTROL_BYTES;
            uxBitsWaitedFor &= ~eventEVENT_BITS_CONTROL_BYTES;

            /* 检查等待条件是否满足 */
            if( ( uxControlBits & eventWAIT_FOR_ALL_BITS ) == ( EventBits_t ) 0 )
            {
                /* 只需要单个位被设置（任一位置位） */
                if( ( uxBitsWaitedFor & pxEventBits->uxEventBits ) != ( EventBits_t ) 0 )
                {
                    xMatchFound = pdTRUE;  /* 找到匹配 */
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
                }
            }
            else if( ( uxBitsWaitedFor & pxEventBits->uxEventBits ) == uxBitsWaitedFor )
            {
                /* 所有需要的位都被设置 */
                xMatchFound = pdTRUE;  /* 找到匹配 */
            }
            else
            {
                /* 需要所有位都被设置，但不是所有位都被设置 */
            }

            /* 如果找到匹配，处理任务唤醒和位清除 */
            if( xMatchFound != pdFALSE )
            {
                /* 位匹配。是否应该在退出时清除位？ */
                if( ( uxControlBits & eventCLEAR_EVENTS_ON_EXIT_BIT ) != ( EventBits_t ) 0 )
                {
                    uxBitsToClear |= uxBitsWaitedFor;  /* 记录需要清除的位 */
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
                }

                /* 在将任务从事件列表中移除之前，将实际的事件标志值存储到任务的事件列表项中。
                   设置eventUNBLOCKED_DUE_TO_BIT_SET位，让任务知道它是由于其所需的位匹配而被解除阻塞的，
                   而不是因为超时 */
                ( void ) xTaskRemoveFromUnorderedEventList( pxListItem, pxEventBits->uxEventBits | eventUNBLOCKED_DUE_TO_BIT_SET );
            }

            /* 移动到下一个列表项。注意这里不使用pxListItem->pxNext，因为列表项可能已从事件列表中移除
               并插入到就绪/挂起读取列表中 */
            pxListItem = pxNext;
        }

        /* 清除任何在控制字中设置了eventCLEAR_EVENTS_ON_EXIT_BIT位且匹配时需要清除的位 */
        pxEventBits->uxEventBits &= ~uxBitsToClear;
    }
    /* 恢复所有任务调度 */
    ( void ) xTaskResumeAll();

    /* 返回当前事件组的值（已清除相应位） */
    return pxEventBits->uxEventBits;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vEventGroupDelete
 * 功能描述：删除事件组对象，唤醒所有等待该事件组的任务，并释放相关资源
 *           此函数负责事件组的完整清理工作，包括任务唤醒和内存释放
 * 输入参数：
 *   - xEventGroup: 事件组句柄，指定要删除的事件组
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数会唤醒所有等待该事件组的任务，并返回0作为事件位值（因为事件组正在被删除）
 *   - 根据事件组的分配方式（静态或动态）决定是否释放内存
 *   - 在挂起所有任务的情况下执行核心操作，确保操作的一致性
 *   - 提供跟踪功能，记录事件组删除操作
 *   - 删除后事件组句柄不再有效，不应继续使用
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void vEventGroupDelete( EventGroupHandle_t xEventGroup )
{
    EventGroup_t *pxEventBits = ( EventGroup_t * ) xEventGroup;  /* 将句柄转换为事件组结构指针 */
    const List_t *pxTasksWaitingForBits = &( pxEventBits->xTasksWaitingForBits );  /* 获取等待位任务列表 */

    /* 挂起所有任务，防止在删除事件组时被其他任务打断 */
    vTaskSuspendAll();
    {
        /* 跟踪事件组删除操作 */
        traceEVENT_GROUP_DELETE( xEventGroup );

        /* 循环处理所有等待该事件组的任务 */
        while( listCURRENT_LIST_LENGTH( pxTasksWaitingForBits ) > ( UBaseType_t ) 0 )
        {
            /* 解除任务的阻塞，返回0作为事件位值，因为事件列表正在被删除，
               因此不能有任何位被设置 */
            configASSERT( pxTasksWaitingForBits->xListEnd.pxNext != ( ListItem_t * ) &( pxTasksWaitingForBits->xListEnd ) );
            ( void ) xTaskRemoveFromUnorderedEventList( pxTasksWaitingForBits->xListEnd.pxNext, eventUNBLOCKED_DUE_TO_BIT_SET );
        }

        /* 根据内存分配配置处理事件组内存释放 */
        #if( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 0 ) )
        {
            /* 事件组只能是动态分配的 - 再次释放它 */
            vPortFree( pxEventBits );
        }
        #elif( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
        {
            /* 事件组可能是静态或动态分配的，因此在尝试释放内存前进行检查 */
            if( pxEventBits->ucStaticallyAllocated == ( uint8_t ) pdFALSE )
            {
                vPortFree( pxEventBits );  /* 动态分配的事件组，释放内存 */
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记（静态分配，不需要释放） */
            }
        }
        #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
    }
    /* 恢复所有任务调度 */
    ( void ) xTaskResumeAll();
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vEventGroupSetBitsCallback
 * 功能描述：事件组设置位回调函数，用于从中断延迟处理中执行设置位操作
 *           此函数是内部使用，处理从中断服务程序延迟提交的设置位命令
 * 输入参数：
 *   - pvEventGroup: 事件组句柄的指针，需要转换为EventGroupHandle_t类型使用
 *   - ulBitsToSet: 要设置的事件位掩码，指定需要设置的事件位
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数是FreeRTOS内部使用的回调函数，不应由应用程序直接调用
 *   - 通过xTimerPendFunctionCallFromISR从中断服务程序延迟调用
 *   - 实际调用xEventGroupSetBits函数执行设置位操作
 *   - 用于在任务上下文中安全地执行中断中请求的事件组操作
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/

/* 仅供内部使用 - 执行从中断挂起的"设置位"命令 */
void vEventGroupSetBitsCallback( void *pvEventGroup, const uint32_t ulBitsToSet )
{
    /* 调用xEventGroupSetBits函数实际执行设置位操作
       将参数转换为适当的类型 */
    ( void ) xEventGroupSetBits( pvEventGroup, ( EventBits_t ) ulBitsToSet );
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vEventGroupClearBitsCallback
 * 功能描述：事件组清除位回调函数，用于从中断延迟处理中执行清除位操作
 *           此函数是内部使用，处理从中断服务程序延迟提交的清除位命令
 * 输入参数：
 *   - pvEventGroup: 事件组句柄的指针，需要转换为EventGroupHandle_t类型使用
 *   - ulBitsToClear: 要清除的事件位掩码，指定需要清除的事件位
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数是FreeRTOS内部使用的回调函数，不应由应用程序直接调用
 *   - 通过xTimerPendFunctionCallFromISR从中断服务程序延迟调用
 *   - 实际调用xEventGroupClearBits函数执行清除位操作
 *   - 用于在任务上下文中安全地执行中断中请求的事件组操作
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/

/* 仅供内部使用 - 执行从中断挂起的"清除位"命令 */
void vEventGroupClearBitsCallback( void *pvEventGroup, const uint32_t ulBitsToClear )
{
    /* 调用xEventGroupClearBits函数实际执行清除位操作
       将参数转换为适当的类型 */
    ( void ) xEventGroupClearBits( pvEventGroup, ( EventBits_t ) ulBitsToClear );
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvTestWaitCondition
 * 功能描述：测试事件组当前位值是否满足等待条件
 *           此函数是FreeRTOS事件组模块的内部辅助函数，用于判断事件位是否满足任务的等待条件
 * 输入参数：
 *   - uxCurrentEventBits: 事件组的当前位值，包含所有事件位的当前状态
 *   - uxBitsToWaitFor: 要等待的事件位掩码，指定任务希望等待的事件位
 *   - xWaitForAllBits: 等待条件类型，指定是需要所有位被设置还是任一位置被设置
 *                     pdTRUE: 需要所有指定位被设置
 *                     pdFALSE: 需要任一指定位被设置
 * 输出参数：无
 * 返 回 值：
 *   - BaseType_t: 如果满足等待条件，返回pdTRUE；否则返回pdFALSE
 * 其它说明：
 *   - 此函数是静态函数，仅在FreeRTOS内核内部使用
 *   - 根据xWaitForAllBits参数决定检查所有位还是任一位
 *   - 使用位操作进行高效的条件检查
 *   - 包含测试覆盖率标记，用于代码覆盖率分析
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
static BaseType_t prvTestWaitCondition( const EventBits_t uxCurrentEventBits, const EventBits_t uxBitsToWaitFor, const BaseType_t xWaitForAllBits )
{
    BaseType_t xWaitConditionMet = pdFALSE;  /* 等待条件是否满足的标志，初始化为不满足 */

    /* 根据等待条件类型进行不同的检查 */
    if( xWaitForAllBits == pdFALSE )
    {
        /* 任务只需要等待uxBitsToWaitFor中的一个位被设置。
           是否已经有至少一个位被设置了？ */
        if( ( uxCurrentEventBits & uxBitsToWaitFor ) != ( EventBits_t ) 0 )
        {
            xWaitConditionMet = pdTRUE;  /* 条件满足：至少有一个等待位被设置 */
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
        }
    }
    else
    {
        /* 任务需要等待uxBitsToWaitFor中的所有位都被设置。
           它们是否都已经设置了？ */
        if( ( uxCurrentEventBits & uxBitsToWaitFor ) == uxBitsToWaitFor )
        {
            xWaitConditionMet = pdTRUE;  /* 条件满足：所有等待位都被设置 */
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
        }
    }

    /* 返回等待条件是否满足的结果 */
    return xWaitConditionMet;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xEventGroupSetBitsFromISR
 * 功能描述：从中断服务程序(ISR)中设置事件组中的特定位，通过延迟回调机制实现
 *           此函数是xEventGroupSetBits的中断安全版本，通过定时器守护任务执行实际设置操作
 * 输入参数：
 *   - xEventGroup: 事件组句柄，指定要操作的事件组
 *   - uxBitsToSet: 要设置的事件位掩码，指定需要设置的事件位
 *   - pxHigherPriorityTaskWoken: 指向BaseType_t的指针，用于指示是否有更高优先级任务被唤醒
 * 输出参数：
 *   - pxHigherPriorityTaskWoken: 如果延迟处理导致更高优先级任务就绪，则设置为pdTRUE
 * 返 回 值：
 *   - BaseType_t: 如果成功将设置请求发送到定时器命令队列，则返回pdPASS
 *                 如果定时器命令队列已满，无法发送请求，则返回pdFAIL
 * 其它说明：
 *   - 此函数仅在启用跟踪功能、定时器pend函数调用和定时器功能时编译
 *   - 通过xTimerPendFunctionCallFromISR将设置操作延迟到定时器守护任务中执行
 *   - 实际设置操作由vEventGroupSetBitsCallback函数执行
 *   - 适用于中断服务程序中需要设置事件位的场景
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 1 ) )

BaseType_t xEventGroupSetBitsFromISR( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet, BaseType_t *pxHigherPriorityTaskWoken )
{
    BaseType_t xReturn;  /* 返回值，表示操作是否成功 */

    /* 跟踪事件组从中断设置位操作 */
    traceEVENT_GROUP_SET_BITS_FROM_ISR( xEventGroup, uxBitsToSet );
    
    /* 通过定时器pend函数调用从ISR中延迟执行设置操作
       将vEventGroupSetBitsCallback函数、事件组句柄和要设置的位作为参数传递 */
    xReturn = xTimerPendFunctionCallFromISR( vEventGroupSetBitsCallback,  /* 回调函数 */
                                             ( void * ) xEventGroup,      /* 事件组句柄作为参数 */
                                             ( uint32_t ) uxBitsToSet,    /* 要设置的位作为参数 */
                                             pxHigherPriorityTaskWoken ); /* 更高优先级任务唤醒标志 */

    /* 返回操作结果 */
    return xReturn;
}

#endif
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：uxEventGroupGetNumber
 * 功能描述：获取事件组的唯一编号，用于调试和跟踪目的
 *           此函数返回事件组的唯一标识编号，便于在调试和跟踪工具中识别特定事件组
 * 输入参数：
 *   - xEventGroup: 事件组句柄的指针，指定要查询的事件组
 * 输出参数：无
 * 返 回 值：
 *   - UBaseType_t: 事件组的唯一编号，如果传入NULL指针则返回0
 * 其它说明：
 *   - 此函数仅在启用跟踪功能时编译（configUSE_TRACE_FACILITY == 1）
 *   - 主要用于调试和跟踪目的，帮助识别和区分不同的事件组
 *   - 如果传入NULL指针，函数会安全地返回0而不是崩溃
 *   - 事件组编号在事件组创建时分配，通常是递增的唯一值
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
#if (configUSE_TRACE_FACILITY == 1)

UBaseType_t uxEventGroupGetNumber( void* xEventGroup )
{
    UBaseType_t xReturn;                         /* 返回值，存储事件组编号 */
    EventGroup_t *pxEventBits = ( EventGroup_t * ) xEventGroup;  /* 将句柄转换为事件组结构指针 */

    /* 检查传入的事件组句柄是否为NULL */
    if( xEventGroup == NULL )
    {
        xReturn = 0;  /* 如果传入NULL，返回0 */
    }
    else
    {
        /* 从事件组结构中获取事件组编号 */
        xReturn = pxEventBits->uxEventGroupNumber;
    }

    /* 返回事件组编号 */
    return xReturn;
}

#endif


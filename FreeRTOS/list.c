/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： LM_list.c
 * 文件标识： 
 * 内容摘要： 链表模块定义
 * 其它说明： 无
 * 当前版本： FreeRTOS V9.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年09月13日
 *
 *******************************************************************************/


/* Includes ------------------------------------------------------------------*/
#include <stdlib.h>
#include "FreeRTOS.h"
#include "list.h"

/*******************************************************************************
 * 函数名称：vListInitialise
 * 功能描述：初始化FreeRTOS链表结构，设置链表结束标记和初始状态
 *           此函数用于准备一个新链表，使其处于空状态并设置正确的结束标记
 * 输入参数：
 *   - pxList: 指向要初始化的链表结构的指针
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 链表结构包含一个用于标记列表末尾的列表项
 *   - 初始化时将列表结束项作为唯一的列表条目插入
 *   - 设置列表结束项的值为最大可能值，确保它始终位于列表末尾
 *   - 列表结束项的前后指针指向自身，用于标识空列表状态
 *   - 支持列表数据完整性检查（如果启用）
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void vListInitialise( List_t * const pxList )  
{
    /* 链表结构包含一个列表项，用于标记列表的末尾。
       要初始化列表，将列表结束项作为唯一的列表条目插入 */
    pxList->pxIndex = ( ListItem_t * ) &( pxList->xListEnd );            /*lint !e826 !e740 使用迷你列表结构作为列表结束以节省RAM。这是经过检查且有效的 */

    /* 列表结束项的值是列表中的最高可能值，确保它始终保持在列表的末尾 */
    pxList->xListEnd.xItemValue = portMAX_DELAY;

    /* 列表结束项的前后指针指向自身，这样我们就知道列表何时为空 */
    pxList->xListEnd.pxNext = ( ListItem_t * ) &( pxList->xListEnd );    /*lint !e826 !e740 使用迷你列表结构作为列表结束以节省RAM。这是经过检查且有效的 */
    pxList->xListEnd.pxPrevious = ( ListItem_t * ) &( pxList->xListEnd );/*lint !e826 !e740 使用迷你列表结构作为列表结束以节省RAM。这是经过检查且有效的 */

    /* 初始化链表中的项目数量为0 */
    pxList->uxNumberOfItems = ( UBaseType_t ) 0U;

    /* 如果configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES设置为1，向列表中写入已知值用于完整性检查 */
    listSET_LIST_INTEGRITY_CHECK_1_VALUE( pxList );
    listSET_LIST_INTEGRITY_CHECK_2_VALUE( pxList );
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vListInitialiseItem
 * 功能描述：初始化FreeRTOS链表项，将其设置为不在任何链表中的状态
 *           此函数用于准备一个新的链表项，使其处于未插入任何链表的状态
 * 输入参数：
 *   - pxItem: 指向要初始化的链表项(ListItem_t)的指针
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 将链表项的容器指针设置为NULL，表示该项不在任何链表中
 *   - 支持链表项数据完整性检查（如果启用）
 *   - 此函数通常在使用链表项之前调用，确保链表项处于正确的初始状态
 *   - 初始化后的链表项可以插入到已初始化的链表中
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void vListInitialiseItem( ListItem_t * const pxItem )
{
    /* 确保链表项没有被记录为在任何链表中，将容器指针设置为NULL */
    pxItem->pvContainer = NULL;

    /* 如果configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES设置为1，向链表项写入已知值用于完整性检查 */
    listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem );
    listSET_SECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem );
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vListInsertEnd
 * 功能描述：将新的链表项插入到FreeRTOS链表的末尾位置
 *           此函数不按值排序链表，而是使新项成为通过listGET_OWNER_OF_NEXT_ENTRY()
 *           调用时最后被移除的项，适用于实现FIFO（先进先出）行为
 * 输入参数：
 *   - pxList: 指向目标链表的指针，新项将被插入到此链表中
 *   - pxNewListItem: 指向要插入的新链表项的指针
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数在启用configASSERT()时进行链表完整性检查
 *   - 插入操作保持链表的环形结构特性
 *   - 更新链表项容器指针以指向所属链表
 *   - 增加链表中的项目计数
 *   - 适用于不需要按值排序的链表插入场景
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void vListInsertEnd( List_t * const pxList, ListItem_t * const pxNewListItem )
{
    /* 获取链表当前的索引项指针 */
    ListItem_t * const pxIndex = pxList->pxIndex;

    /* 仅在定义configASSERT()时有效，这些测试可以捕获内存中被覆盖的链表数据结构。
       它们不会捕获由于FreeRTOS的错误配置或使用而引起的数据错误 */
    listTEST_LIST_INTEGRITY( pxList );           /* 测试链表完整性 */
    listTEST_LIST_ITEM_INTEGRITY( pxNewListItem ); /* 测试链表项完整性 */

    /* 将新链表项插入pxList，但不是对列表进行排序，
       而是使新链表项成为通过调用listGET_OWNER_OF_NEXT_ENTRY()时最后被移除的项 */
    pxNewListItem->pxNext = pxIndex;             /* 新项的下一项指向当前索引项 */
    pxNewListItem->pxPrevious = pxIndex->pxPrevious; /* 新项的前一项指向当前索引项的前一项 */

    /* 仅用于决策覆盖率测试 */
    mtCOVERAGE_TEST_DELAY();

    /* 更新相邻项的指针以包含新项 */
    pxIndex->pxPrevious->pxNext = pxNewListItem; /* 原前一项的下一项指向新项 */
    pxIndex->pxPrevious = pxNewListItem;         /* 当前索引项的前一项指向新项 */

    /* 记住此项所在的链表，更新容器指针 */
    pxNewListItem->pvContainer = ( void * ) pxList;

    /* 增加链表中的项目计数 */
    ( pxList->uxNumberOfItems )++;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vListInsert
 * 功能描述：将新的链表项按值有序插入到FreeRTOS链表中
 *           此函数按照链表项的xItemValue值进行升序排序插入，确保链表始终保持有序
 * 输入参数：
 *   - pxList: 指向目标链表的指针，新项将被按序插入到此链表中
 *   - pxNewListItem: 指向要插入的新链表项的指针，包含用于排序的xItemValue值
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数在启用configASSERT()时进行链表完整性检查
 *   - 插入操作保持链表的环形结构特性和有序性
 *   - 对于具有相同值的链表项，新项会插入到现有相同值项的后面
 *   - 特殊处理portMAX_DELAY值，避免迭代循环无法结束
 *   - 更新链表项容器指针以指向所属链表
 *   - 增加链表中的项目计数
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void vListInsert( List_t * const pxList, ListItem_t * const pxNewListItem )
{
    ListItem_t *pxIterator;                                /* 链表迭代指针，用于遍历找到插入位置 */
    const TickType_t xValueOfInsertion = pxNewListItem->xItemValue;  /* 获取新链表项的值用于排序 */

    /* 仅在定义configASSERT()时有效，这些测试可以捕获内存中被覆盖的链表数据结构。
       它们不会捕获由于FreeRTOS的错误配置或使用而引起的数据错误 */
    listTEST_LIST_INTEGRITY( pxList );                     /* 测试链表完整性 */
    listTEST_LIST_ITEM_INTEGRITY( pxNewListItem );         /* 测试链表项完整性 */

    /* 将新链表项插入列表，按xItemValue值排序。
       如果列表已包含具有相同项值的链表项，则新链表项应放置在其后。这确保存储在就绪列表中的TCB（所有这些TCB都具有相同的xItemValue值）能够共享CPU。
       但是，如果xItemValue与后端标记值相同，下面的迭代循环将不会结束。因此首先检查该值，并在必要时稍微修改算法 */
    if( xValueOfInsertion == portMAX_DELAY )
    {
        /* 如果插入值等于最大值，直接插入到列表末尾（后端标记的前面） */
        pxIterator = pxList->xListEnd.pxPrevious;
    }
    else
    {
        /* *** 注意 ***********************************************************
           如果您发现应用程序在此处崩溃，可能的原因如下所列。此外，请参阅http://www.freertos.org/FAQHelp.html获取更多提示，
           并确保定义了configASSERT()！http://www.freertos.org/a00110.html#configASSERT

                1) 栈溢出 - 请参阅http://www.freertos.org/Stacks-and-stack-overflow-checking.html
                2) 不正确的中断优先级分配，特别是在Cortex-M部件上，数值高的优先级表示实际中断优先级低，这可能看起来违反直觉。
                   请参阅http://www.freertos.org/RTOS-Cortex-M3-M4.html和http://www.freertos.org/a00110.html上的configMAX_SYSCALL_INTERRUPT_PRIORITY定义
                3) 在临界区内或调度器挂起时调用API函数，或者从中断中调用不以"FromISR"结尾的API函数
                4) 在队列或信号量初始化之前或调度器启动之前使用它们（中断是否在调用vTaskStartScheduler()之前触发？）
        **********************************************************************/

        /* 从列表结束标记开始遍历，找到第一个值大于插入值的位置 */
        for( pxIterator = ( ListItem_t * ) &( pxList->xListEnd );  /* 从列表结束标记开始 */
             pxIterator->pxNext->xItemValue <= xValueOfInsertion;  /* 当下一个项的值小于等于插入值时继续 */
             pxIterator = pxIterator->pxNext )                     /* 移动到下一个项 */
            /*lint !e826 !e740 使用迷你列表结构作为列表结束以节省RAM。这是经过检查且有效的 */
        {
            /* 此处无需执行任何操作，只是迭代到所需的插入位置 */
        }
    }

    /* 将新项插入到找到的位置 */
    pxNewListItem->pxNext = pxIterator->pxNext;            /* 新项的下一项指向迭代项的下一项 */
    pxNewListItem->pxNext->pxPrevious = pxNewListItem;     /* 原下一项的前一项指向新项 */
    pxNewListItem->pxPrevious = pxIterator;                /* 新项的前一项指向迭代项 */
    pxIterator->pxNext = pxNewListItem;                    /* 迭代项的下一项指向新项 */

    /* 记住此项所在的链表。这允许以后快速移除该项 */
    pxNewListItem->pvContainer = ( void * ) pxList;        /* 设置新项的容器指针指向所属链表 */

    /* 增加链表中的项目计数 */
    ( pxList->uxNumberOfItems )++;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：uxListRemove
 * 功能描述：从FreeRTOS链表中移除指定的链表项，并更新链表结构
 *           此函数负责从链表中安全地移除指定项，维护链表的完整性和一致性
 * 输入参数：
 *   - pxItemToRemove: 指向要移除的链表项的指针
 * 输出参数：无
 * 返 回 值：
 *   - UBaseType_t: 移除操作后链表中剩余的项目数量
 * 其它说明：
 *   - 链表项知道它所在的链表，通过pvContainer指针获取所属链表信息
 *   - 更新相邻项的指针以绕过被移除的项，保持链表连续性
 *   - 确保链表索引指向有效的项（如果当前索引指向被移除的项）
 *   - 重置被移除项的容器指针为NULL，表示它不再属于任何链表
 *   - 减少链表中的项目计数
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
UBaseType_t uxListRemove( ListItem_t * const pxItemToRemove )
{
    /* 链表项知道它所在的链表。从链表项获取链表指针 */
    List_t * const pxList = ( List_t * ) pxItemToRemove->pvContainer;

    /* 更新相邻项的指针，绕过要移除的项 */
    pxItemToRemove->pxNext->pxPrevious = pxItemToRemove->pxPrevious;
    pxItemToRemove->pxPrevious->pxNext = pxItemToRemove->pxNext;

    /* 仅用于决策覆盖率测试 */
    mtCOVERAGE_TEST_DELAY();

    /* 确保索引指向有效的项。如果当前索引指向要移除的项，将其指向前一项 */
    if( pxList->pxIndex == pxItemToRemove )
    {
        pxList->pxIndex = pxItemToRemove->pxPrevious;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
    }

    /* 将移除项的容器指针设置为NULL，表示它不再属于任何链表 */
    pxItemToRemove->pvContainer = NULL;
    
    /* 减少链表中的项目计数 */
    ( pxList->uxNumberOfItems )--;

    /* 返回移除后链表中的项目数量 */
    return pxList->uxNumberOfItems;
}
/*-----------------------------------------------------------*/


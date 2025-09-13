/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： LM_heap_4.c
 * 文件标识： 
 * 内容摘要： 栈分配模块定义
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

#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

/* Private constants ---------------------------------------------------------*/
#if( configSUPPORT_DYNAMIC_ALLOCATION == 0 )
    #error This file must not be used if configSUPPORT_DYNAMIC_ALLOCATION is 0
#endif

/* Private macros ------------------------------------------------------------*/
/* Block sizes must not get too small. */
#define heapMINIMUM_BLOCK_SIZE    ( ( size_t ) ( xHeapStructSize << 1 ) )

/* Assumes 8bit bytes! */
#define heapBITS_PER_BYTE        ( ( size_t ) 8 )

/* Private types -------------------------------------------------------------*/
/* Define the linked list structure. This is used to link free blocks in order
of their memory address. */
typedef struct A_BLOCK_LINK
{
    struct A_BLOCK_LINK *pxNextFreeBlock;    /*<< The next free block in the list. */
    size_t xBlockSize;                       /*<< The size of the free block. */
} BlockLink_t;

/* Private variables ---------------------------------------------------------*/
/* Allocate the memory for the heap. */
#if( configAPPLICATION_ALLOCATED_HEAP == 1 )
    /* The application writer has already defined the array used for the RTOS
    heap - probably so it can be placed in a special segment or address. */
    extern uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
#else
    static uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
#endif /* configAPPLICATION_ALLOCATED_HEAP */

/* The size of the structure placed at the beginning of each allocated memory
block must by correctly byte aligned. */
static const size_t xHeapStructSize = ( sizeof( BlockLink_t ) + ( ( size_t ) ( portBYTE_ALIGNMENT - 1 ) ) ) & ~( ( size_t ) portBYTE_ALIGNMENT_MASK );

/* Create a couple of list links to mark the start and end of the list. */
static BlockLink_t xStart, *pxEnd = NULL;

/* Keeps track of the number of free bytes remaining, but says nothing about
fragmentation. */
static size_t xFreeBytesRemaining = 0U;
static size_t xMinimumEverFreeBytesRemaining = 0U;

/* Gets set to the top bit of an size_t type. When this bit in the xBlockSize
member of an BlockLink_t structure is set then the block belongs to the
application. When the bit is free the block is still part of the free heap
space. */
static size_t xBlockAllocatedBit = 0;

/* Private functions ---------------------------------------------------------*/
/*
 * Inserts a block of memory that is being freed into the correct position in
 * the list of free memory blocks. The block being freed will be merged with
 * the block in front it and/or the block behind it if the memory blocks are
 * adjacent to each other.
 */
static void prvInsertBlockIntoFreeList( BlockLink_t *pxBlockToInsert );

/*
 * Called automatically to setup the required heap structures the first time
 * pvPortMalloc() is called.
 */
static void prvHeapInit( void );
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：pvPortMalloc
 * 功能描述：FreeRTOS内存分配函数，从堆中分配指定大小的内存块
 *           此函数实现了FreeRTOS的动态内存分配机制，支持多种堆管理策略
 * 输入参数：
 *   - xWantedSize: 请求分配的内存大小（以字节为单位）
 * 输出参数：无
 * 返 回 值：
 *   - void*: 成功分配时返回指向分配内存的指针，失败时返回NULL
 * 其它说明：
 *   - 此函数是FreeRTOS的核心内存分配函数，线程安全
 *   - 在分配过程中会挂起所有任务，防止竞态条件
 *   - 支持字节对齐要求，确保分配的内存满足对齐条件
 *   - 包含内存分配失败钩子函数支持，可用于调试和错误处理
 *   - 使用首次适应算法遍历空闲块链表寻找合适的内存块
 *   - 支持内存块分割，提高内存利用率
 *   - 跟踪内存分配情况，记录最小剩余内存量
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void *pvPortMalloc( size_t xWantedSize )
{
    BlockLink_t *pxBlock, *pxPreviousBlock, *pxNewBlockLink;  /* 块链表指针 */
    void *pvReturn = NULL;                                    /* 返回值，初始化为NULL */

    /* 挂起所有任务，防止在内存分配过程中被其他任务打断 */
    vTaskSuspendAll();
    {
        /* 如果是第一次调用malloc，则需要初始化堆，建立空闲块列表 */
        if( pxEnd == NULL )
        {
            prvHeapInit();  /* 初始化堆 */
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
        }

        /* 检查请求的块大小是否不会导致块大小成员的最高位被设置。
           块大小成员的最高位用于确定块的所有者 - 应用程序或内核，因此它必须空闲 */
        if( ( xWantedSize & xBlockAllocatedBit ) == 0 )
        {
            /* 增加所需大小，使其除了包含请求的字节数外，还能包含一个BlockLink_t结构 */
            if( xWantedSize > 0 )
            {
                xWantedSize += xHeapStructSize;  /* 增加堆结构大小 */
                
                /* 确保块始终对齐到所需的字节数 */
                if( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) != 0x00 )
                {
                    /* 需要字节对齐 */
                    xWantedSize += ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) );
                    configASSERT( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) == 0 );  /* 断言检查对齐 */
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

            /* 检查请求大小是否有效且有足够空闲内存 */
            if( ( xWantedSize > 0 ) && ( xWantedSize <= xFreeBytesRemaining ) )
            {
                /* 从起始（最低地址）块开始遍历列表，直到找到足够大小的块 */
                pxPreviousBlock = &xStart;
                pxBlock = xStart.pxNextFreeBlock;
                
                /* 遍历空闲块链表，寻找足够大的块 */
                while( ( pxBlock->xBlockSize < xWantedSize ) && ( pxBlock->pxNextFreeBlock != NULL ) )
                {
                    pxPreviousBlock = pxBlock;
                    pxBlock = pxBlock->pxNextFreeBlock;
                }

                /* 如果到达结束标记，则没有找到足够大小的块 */
                if( pxBlock != pxEnd )
                {
                    /* 返回指向的内存空间 - 跳过其起始处的BlockLink_t结构 */
                    pvReturn = ( void * ) ( ( ( uint8_t * ) pxPreviousBlock->pxNextFreeBlock ) + xHeapStructSize );

                    /* 此块正在被返回使用，因此必须从空闲块列表中移除 */
                    pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

                    /* 如果块比需要的大，可以将其分割成两个 */
                    if( ( pxBlock->xBlockSize - xWantedSize ) > heapMINIMUM_BLOCK_SIZE )
                    {
                        /* 此块将被分割成两个。创建一个新块跟随在请求的字节数之后。
                           void转换用于防止编译器产生字节对齐警告 */
                        pxNewBlockLink = ( void * ) ( ( ( uint8_t * ) pxBlock ) + xWantedSize );
                        configASSERT( ( ( ( size_t ) pxNewBlockLink ) & portBYTE_ALIGNMENT_MASK ) == 0 );  /* 断言检查对齐 */

                        /* 计算从单个块分割出的两个块的大小 */
                        pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;
                        pxBlock->xBlockSize = xWantedSize;

                        /* 将新块插入空闲块列表 */
                        prvInsertBlockIntoFreeList( pxNewBlockLink );
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
                    }

                    /* 更新剩余空闲字节数 */
                    xFreeBytesRemaining -= pxBlock->xBlockSize;

                    /* 更新历史最小空闲字节数 */
                    if( xFreeBytesRemaining < xMinimumEverFreeBytesRemaining )
                    {
                        xMinimumEverFreeBytesRemaining = xFreeBytesRemaining;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
                    }

                    /* 块正在被返回 - 它已被分配并由应用程序拥有，没有"下一个"块 */
                    pxBlock->xBlockSize |= xBlockAllocatedBit;  /* 设置分配位 */
                    pxBlock->pxNextFreeBlock = NULL;            /* 下一个空闲块指针设为NULL */
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
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
        }

        /* 跟踪内存分配操作 */
        traceMALLOC( pvReturn, xWantedSize );
    }
    /* 恢复所有任务调度 */
    ( void ) xTaskResumeAll();

    /* 如果启用内存分配失败钩子函数，检查分配是否失败 */
    #if( configUSE_MALLOC_FAILED_HOOK == 1 )
    {
        if( pvReturn == NULL )
        {
            /* 调用内存分配失败钩子函数 */
            extern void vApplicationMallocFailedHook( void );
            vApplicationMallocFailedHook();
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
        }
    }
    #endif

    /* 断言检查返回的指针是否满足字节对齐要求 */
    configASSERT( ( ( ( size_t ) pvReturn ) & ( size_t ) portBYTE_ALIGNMENT_MASK ) == 0 );
    
    /* 返回分配的内存指针或NULL */
    return pvReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vPortFree
 * 功能描述：释放之前由pvPortMalloc分配的内存块，将其返回给堆管理系统
 *           此函数是FreeRTOS的内存释放函数，负责将不再使用的内存块重新加入空闲列表
 * 输入参数：
 *   - pv: 指向要释放的内存块的指针，必须是之前由pvPortMalloc分配的指针
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数是FreeRTOS的核心内存释放函数，线程安全
 *   - 在释放过程中会挂起所有任务，防止竞态条件
 *   - 支持内存块合并，减少内存碎片
 *   - 包含断言检查，确保释放操作的合法性
 *   - 更新内存使用统计信息，跟踪内存释放操作
 *   - 如果传入NULL指针，函数会安全地返回而不执行任何操作
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void vPortFree( void *pv )
{
    uint8_t *puc = ( uint8_t * ) pv;  /* 将void指针转换为字节指针，便于地址计算 */
    BlockLink_t *pxLink;               /* 指向内存块管理结构的指针 */

    /* 检查传入的指针是否为NULL，如果是NULL则直接返回 */
    if( pv != NULL )
    {
        /* 要释放的内存块前面紧邻着一个BlockLink_t结构 */
        puc -= xHeapStructSize;  /* 向后移动指针以指向管理结构 */

        /* 此类型转换是为了避免编译器警告 */
        pxLink = ( void * ) puc;

        /* 检查块是否确实已分配 */
        configASSERT( ( pxLink->xBlockSize & xBlockAllocatedBit ) != 0 );  /* 断言检查分配位 */
        configASSERT( pxLink->pxNextFreeBlock == NULL );                    /* 断言检查下一个空闲块指针 */

        /* 再次验证块是否已分配（在断言禁用时提供运行时检查） */
        if( ( pxLink->xBlockSize & xBlockAllocatedBit ) != 0 )
        {
            /* 验证下一个空闲块指针是否为NULL（确保不是重复释放） */
            if( pxLink->pxNextFreeBlock == NULL )
            {
                /* 块正在返回给堆 - 它不再被分配 */
                pxLink->xBlockSize &= ~xBlockAllocatedBit;  /* 清除分配位 */

                /* 挂起所有任务，防止在内存释放过程中被其他任务打断 */
                vTaskSuspendAll();
                {
                    /* 增加剩余空闲字节数 */
                    xFreeBytesRemaining += pxLink->xBlockSize;
                    
                    /* 跟踪内存释放操作 */
                    traceFREE( pv, pxLink->xBlockSize );
                    
                    /* 将此块添加到空闲块列表中（可能会合并相邻空闲块） */
                    prvInsertBlockIntoFreeList( ( ( BlockLink_t * ) pxLink ) );
                }
                /* 恢复所有任务调度 */
                ( void ) xTaskResumeAll();
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
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xPortGetFreeHeapSize
 * 功能描述：获取当前堆中的空闲内存大小（以字节为单位）
 *           此函数用于查询堆内存的当前使用情况，帮助监控内存使用状态
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：
 *   - size_t: 当前堆中的空闲内存大小（字节数）
 * 其它说明：
 *   - 此函数提供实时的堆内存使用情况信息
 *   - 可用于检测内存泄漏或评估内存分配策略
 *   - 返回值会随着内存分配和释放操作动态变化
 *   - 函数执行非常快速，不会阻塞或挂起任务
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
size_t xPortGetFreeHeapSize( void )
{
    /* 返回当前剩余的空闲字节数 */
    return xFreeBytesRemaining;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：xPortGetMinimumEverFreeHeapSize
 * 功能描述：获取堆运行过程中曾经达到的最小空闲内存大小（以字节为单位）
 *           此函数用于查询堆内存的历史使用情况，帮助评估内存使用峰值
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：
 *   - size_t: 堆运行过程中曾经达到的最小空闲内存大小（字节数）
 * 其它说明：
 *   - 此函数提供堆内存使用的历史最低点信息
 *   - 可用于评估系统的内存需求和分配策略的有效性
 *   - 返回值只会减少不会增加，反映内存使用的峰值情况
 *   - 函数执行非常快速，不会阻塞或挂起任务
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
size_t xPortGetMinimumEverFreeHeapSize( void )
{
    /* 返回历史最小剩余空闲字节数 */
    return xMinimumEverFreeBytesRemaining;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：vPortInitialiseBlocks
 * 功能描述：初始化堆块（空函数，仅用于保持链接器安静）
 *           此函数是为了向后兼容而保留的空函数，实际初始化在prvHeapInit中完成
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数实际上不执行任何操作，仅为了兼容旧版本或特定配置而存在
 *   - 真正的堆初始化在prvHeapInit函数中完成
 *   - 在某些移植版本或配置中可能需要此函数来满足链接器要求
 *   - 函数名中的"Initialise"是英式拼写，注意与美式拼写"Initialize"区分
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
void vPortInitialiseBlocks( void )
{
    /* 这只是为了让链接器保持安静，不执行任何实际操作 */
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvHeapInit
 * 功能描述：FreeRTOS堆内存初始化函数，初始化堆管理结构和空闲块链表
 *           此函数在第一次调用pvPortMalloc时自动执行，负责设置堆内存管理的基础结构
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数是静态函数，仅在FreeRTOS内存管理内部使用
 *   - 负责对齐堆起始地址，确保内存访问满足处理器对齐要求
 *   - 初始化空闲块链表，创建初始的单个大空闲块
 *   - 设置堆结束标记，界定堆内存的边界
 *   - 计算并初始化内存管理统计信息
 *   - 确定分配位的位置，用于标记块的所有权状态
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
static void prvHeapInit( void )
{
    BlockLink_t *pxFirstFreeBlock;          /* 指向第一个空闲块的指针 */
    uint8_t *pucAlignedHeap;                /* 对齐后的堆起始地址 */
    size_t uxAddress;                       /* 地址计算临时变量 */
    size_t xTotalHeapSize = configTOTAL_HEAP_SIZE;  /* 总堆大小，从配置获取 */

    /* 确保堆起始于正确对齐的边界 */
    uxAddress = ( size_t ) ucHeap;

    /* 检查堆起始地址是否已经对齐，如果没有则进行对齐调整 */
    if( ( uxAddress & portBYTE_ALIGNMENT_MASK ) != 0 )
    {
        /* 调整地址以实现对齐 */
        uxAddress += ( portBYTE_ALIGNMENT - 1 );
        uxAddress &= ~( ( size_t ) portBYTE_ALIGNMENT_MASK );
        
        /* 调整总堆大小，扣除对齐调整所占用的空间 */
        xTotalHeapSize -= uxAddress - ( size_t ) ucHeap;
    }

    /* 将对齐后的地址转换为字节指针 */
    pucAlignedHeap = ( uint8_t * ) uxAddress;

    /* xStart用于持有指向空闲块列表中第一项的指针。
       void转换用于防止编译器警告 */
    xStart.pxNextFreeBlock = ( void * ) pucAlignedHeap;
    xStart.xBlockSize = ( size_t ) 0;

    /* pxEnd用于标记空闲块列表的结束，并插入到堆空间的末尾 */
    uxAddress = ( ( size_t ) pucAlignedHeap ) + xTotalHeapSize;  /* 计算堆结束地址 */
    uxAddress -= xHeapStructSize;                                /* 减去堆结构大小 */
    uxAddress &= ~( ( size_t ) portBYTE_ALIGNMENT_MASK );        /* 对齐地址 */
    pxEnd = ( void * ) uxAddress;                                /* 设置结束标记指针 */
    pxEnd->xBlockSize = 0;                                       /* 结束块大小为0 */
    pxEnd->pxNextFreeBlock = NULL;                               /* 结束块没有下一个块 */

    /* 开始时有一个单独的空闲块，其大小占据了整个堆空间，减去pxEnd所占的空间 */
    pxFirstFreeBlock = ( void * ) pucAlignedHeap;                           /* 第一个空闲块起始地址 */
    pxFirstFreeBlock->xBlockSize = uxAddress - ( size_t ) pxFirstFreeBlock; /* 计算第一个空闲块的大小 */
    pxFirstFreeBlock->pxNextFreeBlock = pxEnd;                              /* 第一个空闲块指向结束块 */

    /* 只存在一个块 - 它覆盖了整个可用的堆空间 */
    xMinimumEverFreeBytesRemaining = pxFirstFreeBlock->xBlockSize;  /* 初始化历史最小空闲字节数 */
    xFreeBytesRemaining = pxFirstFreeBlock->xBlockSize;             /* 初始化当前空闲字节数 */

    /* 计算size_t变量中最高位的位置，用于分配位标记 */
    xBlockAllocatedBit = ( ( size_t ) 1 ) << ( ( sizeof( size_t ) * heapBITS_PER_BYTE ) - 1 );
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * 函数名称：prvInsertBlockIntoFreeList
 * 功能描述：将空闲块插入空闲块链表，并在可能的情况下合并相邻的空闲块
 *           此函数是FreeRTOS内存管理的核心内部函数，负责维护空闲块链表的完整性
 * 输入参数：
 *   - pxBlockToInsert: 指向要插入的空闲块的指针
 * 输出参数：无
 * 返 回 值：无
 * 其它说明：
 *   - 此函数是静态函数，仅在FreeRTOS内存管理内部使用
 *   - 按地址顺序将空闲块插入链表，保持链表有序性
 *   - 检查并合并相邻的空闲块，减少内存碎片
 *   - 处理三种可能的合并情况：与前块合并、与后块合并、同时与前后块合并
 *   - 使用地址顺序遍历确保合并操作的正确性
 * 
 * 修改日期      版本号          修改人            修改内容
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          创建
 *******************************************************************************/
static void prvInsertBlockIntoFreeList( BlockLink_t *pxBlockToInsert )
{
    BlockLink_t *pxIterator;  /* 链表遍历迭代器 */
    uint8_t *puc;             /* 用于地址计算的字节指针 */

    /* 遍历链表，直到找到地址高于要插入块的块 */
    for( pxIterator = &xStart; pxIterator->pxNextFreeBlock < pxBlockToInsert; pxIterator = pxIterator->pxNextFreeBlock )
    {
        /* 此处无需执行任何操作，只是迭代到正确位置 */
    }

    /* 检查要插入的块和它要插入后的块是否构成连续的内存块？ */
    puc = ( uint8_t * ) pxIterator;
    if( ( puc + pxIterator->xBlockSize ) == ( uint8_t * ) pxBlockToInsert )
    {
        /* 与前一个块连续，合并两个块 */
        pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
        pxBlockToInsert = pxIterator;  /* 更新要插入的块指针为合并后的块 */
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
    }

    /* 检查要插入的块和它要插入前的块是否构成连续的内存块？ */
    puc = ( uint8_t * ) pxBlockToInsert;
    if( ( puc + pxBlockToInsert->xBlockSize ) == ( uint8_t * ) pxIterator->pxNextFreeBlock )
    {
        /* 与后一个块连续，检查后一个块是否是结束块 */
        if( pxIterator->pxNextFreeBlock != pxEnd )
        {
            /* 将两个块合并成一个大块 */
            pxBlockToInsert->xBlockSize += pxIterator->pxNextFreeBlock->xBlockSize;
            pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock->pxNextFreeBlock;
        }
        else
        {
            /* 后一个块是结束块，直接指向结束块 */
            pxBlockToInsert->pxNextFreeBlock = pxEnd;
        }
    }
    else
    {
        /* 不与后一个块连续，正常设置下一个块指针 */
        pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
    }

    /* 如果要插入的块填补了间隙，因此与前块和后块都合并了，
       那么它的pxNextFreeBlock指针已经被设置，不应在这里再次设置，
       否则会使其指向自身 */
    if( pxIterator != pxBlockToInsert )
    {
        /* 更新前一个块的下一个指针，指向要插入的块 */
        pxIterator->pxNextFreeBlock = pxBlockToInsert;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  /* 测试覆盖率标记 */
    }
}


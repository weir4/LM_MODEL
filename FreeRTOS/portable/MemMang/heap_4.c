/*******************************************************************************
 * ��Ȩ���� (C)2025, CQG
 *
 * �ļ����ƣ� LM_heap_4.c
 * �ļ���ʶ�� 
 * ����ժҪ�� ջ����ģ�鶨��
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
 * �������ƣ�pvPortMalloc
 * ����������FreeRTOS�ڴ���亯�����Ӷ��з���ָ����С���ڴ��
 *           �˺���ʵ����FreeRTOS�Ķ�̬�ڴ������ƣ�֧�ֶ��ֶѹ������
 * ���������
 *   - xWantedSize: ���������ڴ��С�����ֽ�Ϊ��λ��
 * �����������
 * �� �� ֵ��
 *   - void*: �ɹ�����ʱ����ָ������ڴ��ָ�룬ʧ��ʱ����NULL
 * ����˵����
 *   - �˺�����FreeRTOS�ĺ����ڴ���亯�����̰߳�ȫ
 *   - �ڷ�������л�����������񣬷�ֹ��̬����
 *   - ֧���ֽڶ���Ҫ��ȷ��������ڴ������������
 *   - �����ڴ����ʧ�ܹ��Ӻ���֧�֣������ڵ��Ժʹ�����
 *   - ʹ���״���Ӧ�㷨�������п�����Ѱ�Һ��ʵ��ڴ��
 *   - ֧���ڴ��ָ����ڴ�������
 *   - �����ڴ�����������¼��Сʣ���ڴ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
void *pvPortMalloc( size_t xWantedSize )
{
    BlockLink_t *pxBlock, *pxPreviousBlock, *pxNewBlockLink;  /* ������ָ�� */
    void *pvReturn = NULL;                                    /* ����ֵ����ʼ��ΪNULL */

    /* �����������񣬷�ֹ���ڴ��������б����������� */
    vTaskSuspendAll();
    {
        /* ����ǵ�һ�ε���malloc������Ҫ��ʼ���ѣ��������п��б� */
        if( pxEnd == NULL )
        {
            prvHeapInit();  /* ��ʼ���� */
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
        }

        /* �������Ŀ��С�Ƿ񲻻ᵼ�¿��С��Ա�����λ�����á�
           ���С��Ա�����λ����ȷ����������� - Ӧ�ó�����ںˣ������������� */
        if( ( xWantedSize & xBlockAllocatedBit ) == 0 )
        {
            /* ���������С��ʹ����˰���������ֽ����⣬���ܰ���һ��BlockLink_t�ṹ */
            if( xWantedSize > 0 )
            {
                xWantedSize += xHeapStructSize;  /* ���Ӷѽṹ��С */
                
                /* ȷ����ʼ�ն��뵽������ֽ��� */
                if( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) != 0x00 )
                {
                    /* ��Ҫ�ֽڶ��� */
                    xWantedSize += ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) );
                    configASSERT( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) == 0 );  /* ���Լ����� */
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

            /* ��������С�Ƿ���Ч�����㹻�����ڴ� */
            if( ( xWantedSize > 0 ) && ( xWantedSize <= xFreeBytesRemaining ) )
            {
                /* ����ʼ����͵�ַ���鿪ʼ�����б�ֱ���ҵ��㹻��С�Ŀ� */
                pxPreviousBlock = &xStart;
                pxBlock = xStart.pxNextFreeBlock;
                
                /* �������п�����Ѱ���㹻��Ŀ� */
                while( ( pxBlock->xBlockSize < xWantedSize ) && ( pxBlock->pxNextFreeBlock != NULL ) )
                {
                    pxPreviousBlock = pxBlock;
                    pxBlock = pxBlock->pxNextFreeBlock;
                }

                /* ������������ǣ���û���ҵ��㹻��С�Ŀ� */
                if( pxBlock != pxEnd )
                {
                    /* ����ָ����ڴ�ռ� - ��������ʼ����BlockLink_t�ṹ */
                    pvReturn = ( void * ) ( ( ( uint8_t * ) pxPreviousBlock->pxNextFreeBlock ) + xHeapStructSize );

                    /* �˿����ڱ�����ʹ�ã���˱���ӿ��п��б����Ƴ� */
                    pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

                    /* ��������Ҫ�Ĵ󣬿��Խ���ָ������ */
                    if( ( pxBlock->xBlockSize - xWantedSize ) > heapMINIMUM_BLOCK_SIZE )
                    {
                        /* �˿齫���ָ������������һ���¿������������ֽ���֮��
                           voidת�����ڷ�ֹ�����������ֽڶ��뾯�� */
                        pxNewBlockLink = ( void * ) ( ( ( uint8_t * ) pxBlock ) + xWantedSize );
                        configASSERT( ( ( ( size_t ) pxNewBlockLink ) & portBYTE_ALIGNMENT_MASK ) == 0 );  /* ���Լ����� */

                        /* ����ӵ�����ָ����������Ĵ�С */
                        pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;
                        pxBlock->xBlockSize = xWantedSize;

                        /* ���¿������п��б� */
                        prvInsertBlockIntoFreeList( pxNewBlockLink );
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
                    }

                    /* ����ʣ������ֽ��� */
                    xFreeBytesRemaining -= pxBlock->xBlockSize;

                    /* ������ʷ��С�����ֽ��� */
                    if( xFreeBytesRemaining < xMinimumEverFreeBytesRemaining )
                    {
                        xMinimumEverFreeBytesRemaining = xFreeBytesRemaining;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
                    }

                    /* �����ڱ����� - ���ѱ����䲢��Ӧ�ó���ӵ�У�û��"��һ��"�� */
                    pxBlock->xBlockSize |= xBlockAllocatedBit;  /* ���÷���λ */
                    pxBlock->pxNextFreeBlock = NULL;            /* ��һ�����п�ָ����ΪNULL */
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
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
        }

        /* �����ڴ������� */
        traceMALLOC( pvReturn, xWantedSize );
    }
    /* �ָ������������ */
    ( void ) xTaskResumeAll();

    /* ��������ڴ����ʧ�ܹ��Ӻ������������Ƿ�ʧ�� */
    #if( configUSE_MALLOC_FAILED_HOOK == 1 )
    {
        if( pvReturn == NULL )
        {
            /* �����ڴ����ʧ�ܹ��Ӻ��� */
            extern void vApplicationMallocFailedHook( void );
            vApplicationMallocFailedHook();
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
        }
    }
    #endif

    /* ���Լ�鷵�ص�ָ���Ƿ������ֽڶ���Ҫ�� */
    configASSERT( ( ( ( size_t ) pvReturn ) & ( size_t ) portBYTE_ALIGNMENT_MASK ) == 0 );
    
    /* ���ط�����ڴ�ָ���NULL */
    return pvReturn;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�vPortFree
 * �����������ͷ�֮ǰ��pvPortMalloc������ڴ�飬���䷵�ظ��ѹ���ϵͳ
 *           �˺�����FreeRTOS���ڴ��ͷź��������𽫲���ʹ�õ��ڴ�����¼�������б�
 * ���������
 *   - pv: ָ��Ҫ�ͷŵ��ڴ���ָ�룬������֮ǰ��pvPortMalloc�����ָ��
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺�����FreeRTOS�ĺ����ڴ��ͷź������̰߳�ȫ
 *   - ���ͷŹ����л�����������񣬷�ֹ��̬����
 *   - ֧���ڴ��ϲ��������ڴ���Ƭ
 *   - �������Լ�飬ȷ���ͷŲ����ĺϷ���
 *   - �����ڴ�ʹ��ͳ����Ϣ�������ڴ��ͷŲ���
 *   - �������NULLָ�룬�����ᰲȫ�ط��ض���ִ���κβ���
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
void vPortFree( void *pv )
{
    uint8_t *puc = ( uint8_t * ) pv;  /* ��voidָ��ת��Ϊ�ֽ�ָ�룬���ڵ�ַ���� */
    BlockLink_t *pxLink;               /* ָ���ڴ�����ṹ��ָ�� */

    /* ��鴫���ָ���Ƿ�ΪNULL�������NULL��ֱ�ӷ��� */
    if( pv != NULL )
    {
        /* Ҫ�ͷŵ��ڴ��ǰ�������һ��BlockLink_t�ṹ */
        puc -= xHeapStructSize;  /* ����ƶ�ָ����ָ�����ṹ */

        /* ������ת����Ϊ�˱������������ */
        pxLink = ( void * ) puc;

        /* �����Ƿ�ȷʵ�ѷ��� */
        configASSERT( ( pxLink->xBlockSize & xBlockAllocatedBit ) != 0 );  /* ���Լ�����λ */
        configASSERT( pxLink->pxNextFreeBlock == NULL );                    /* ���Լ����һ�����п�ָ�� */

        /* �ٴ���֤���Ƿ��ѷ��䣨�ڶ��Խ���ʱ�ṩ����ʱ��飩 */
        if( ( pxLink->xBlockSize & xBlockAllocatedBit ) != 0 )
        {
            /* ��֤��һ�����п�ָ���Ƿ�ΪNULL��ȷ�������ظ��ͷţ� */
            if( pxLink->pxNextFreeBlock == NULL )
            {
                /* �����ڷ��ظ��� - �����ٱ����� */
                pxLink->xBlockSize &= ~xBlockAllocatedBit;  /* �������λ */

                /* �����������񣬷�ֹ���ڴ��ͷŹ����б����������� */
                vTaskSuspendAll();
                {
                    /* ����ʣ������ֽ��� */
                    xFreeBytesRemaining += pxLink->xBlockSize;
                    
                    /* �����ڴ��ͷŲ��� */
                    traceFREE( pv, pxLink->xBlockSize );
                    
                    /* ���˿���ӵ����п��б��У����ܻ�ϲ����ڿ��п飩 */
                    prvInsertBlockIntoFreeList( ( ( BlockLink_t * ) pxLink ) );
                }
                /* �ָ������������ */
                ( void ) xTaskResumeAll();
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
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xPortGetFreeHeapSize
 * ������������ȡ��ǰ���еĿ����ڴ��С�����ֽ�Ϊ��λ��
 *           �˺������ڲ�ѯ���ڴ�ĵ�ǰʹ���������������ڴ�ʹ��״̬
 * �����������
 * �����������
 * �� �� ֵ��
 *   - size_t: ��ǰ���еĿ����ڴ��С���ֽ�����
 * ����˵����
 *   - �˺����ṩʵʱ�Ķ��ڴ�ʹ�������Ϣ
 *   - �����ڼ���ڴ�й©�������ڴ�������
 *   - ����ֵ�������ڴ������ͷŲ�����̬�仯
 *   - ����ִ�зǳ����٣������������������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
size_t xPortGetFreeHeapSize( void )
{
    /* ���ص�ǰʣ��Ŀ����ֽ��� */
    return xFreeBytesRemaining;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�xPortGetMinimumEverFreeHeapSize
 * ������������ȡ�����й����������ﵽ����С�����ڴ��С�����ֽ�Ϊ��λ��
 *           �˺������ڲ�ѯ���ڴ����ʷʹ����������������ڴ�ʹ�÷�ֵ
 * �����������
 * �����������
 * �� �� ֵ��
 *   - size_t: �����й����������ﵽ����С�����ڴ��С���ֽ�����
 * ����˵����
 *   - �˺����ṩ���ڴ�ʹ�õ���ʷ��͵���Ϣ
 *   - ����������ϵͳ���ڴ�����ͷ�����Ե���Ч��
 *   - ����ֵֻ����ٲ������ӣ���ӳ�ڴ�ʹ�õķ�ֵ���
 *   - ����ִ�зǳ����٣������������������
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
size_t xPortGetMinimumEverFreeHeapSize( void )
{
    /* ������ʷ��Сʣ������ֽ��� */
    return xMinimumEverFreeBytesRemaining;
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�vPortInitialiseBlocks
 * ������������ʼ���ѿ飨�պ����������ڱ���������������
 *           �˺�����Ϊ�������ݶ������Ŀպ�����ʵ�ʳ�ʼ����prvHeapInit�����
 * �����������
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺���ʵ���ϲ�ִ���κβ�������Ϊ�˼��ݾɰ汾���ض����ö�����
 *   - �����Ķѳ�ʼ����prvHeapInit���������
 *   - ��ĳЩ��ֲ�汾�������п�����Ҫ�˺���������������Ҫ��
 *   - �������е�"Initialise"��Ӣʽƴд��ע������ʽƴд"Initialize"����
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
void vPortInitialiseBlocks( void )
{
    /* ��ֻ��Ϊ�������������ְ�������ִ���κ�ʵ�ʲ��� */
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvHeapInit
 * ����������FreeRTOS���ڴ��ʼ����������ʼ���ѹ���ṹ�Ϳ��п�����
 *           �˺����ڵ�һ�ε���pvPortMallocʱ�Զ�ִ�У��������ö��ڴ����Ļ����ṹ
 * �����������
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺����Ǿ�̬����������FreeRTOS�ڴ�����ڲ�ʹ��
 *   - ����������ʼ��ַ��ȷ���ڴ�������㴦��������Ҫ��
 *   - ��ʼ�����п�����������ʼ�ĵ�������п�
 *   - ���öѽ�����ǣ��綨���ڴ�ı߽�
 *   - ���㲢��ʼ���ڴ����ͳ����Ϣ
 *   - ȷ������λ��λ�ã����ڱ�ǿ������Ȩ״̬
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
static void prvHeapInit( void )
{
    BlockLink_t *pxFirstFreeBlock;          /* ָ���һ�����п��ָ�� */
    uint8_t *pucAlignedHeap;                /* �����Ķ���ʼ��ַ */
    size_t uxAddress;                       /* ��ַ������ʱ���� */
    size_t xTotalHeapSize = configTOTAL_HEAP_SIZE;  /* �ܶѴ�С�������û�ȡ */

    /* ȷ������ʼ����ȷ����ı߽� */
    uxAddress = ( size_t ) ucHeap;

    /* ������ʼ��ַ�Ƿ��Ѿ����룬���û������ж������ */
    if( ( uxAddress & portBYTE_ALIGNMENT_MASK ) != 0 )
    {
        /* ������ַ��ʵ�ֶ��� */
        uxAddress += ( portBYTE_ALIGNMENT - 1 );
        uxAddress &= ~( ( size_t ) portBYTE_ALIGNMENT_MASK );
        
        /* �����ܶѴ�С���۳����������ռ�õĿռ� */
        xTotalHeapSize -= uxAddress - ( size_t ) ucHeap;
    }

    /* �������ĵ�ַת��Ϊ�ֽ�ָ�� */
    pucAlignedHeap = ( uint8_t * ) uxAddress;

    /* xStart���ڳ���ָ����п��б��е�һ���ָ�롣
       voidת�����ڷ�ֹ���������� */
    xStart.pxNextFreeBlock = ( void * ) pucAlignedHeap;
    xStart.xBlockSize = ( size_t ) 0;

    /* pxEnd���ڱ�ǿ��п��б�Ľ����������뵽�ѿռ��ĩβ */
    uxAddress = ( ( size_t ) pucAlignedHeap ) + xTotalHeapSize;  /* ����ѽ�����ַ */
    uxAddress -= xHeapStructSize;                                /* ��ȥ�ѽṹ��С */
    uxAddress &= ~( ( size_t ) portBYTE_ALIGNMENT_MASK );        /* �����ַ */
    pxEnd = ( void * ) uxAddress;                                /* ���ý������ָ�� */
    pxEnd->xBlockSize = 0;                                       /* �������СΪ0 */
    pxEnd->pxNextFreeBlock = NULL;                               /* ������û����һ���� */

    /* ��ʼʱ��һ�������Ŀ��п飬���Сռ���������ѿռ䣬��ȥpxEnd��ռ�Ŀռ� */
    pxFirstFreeBlock = ( void * ) pucAlignedHeap;                           /* ��һ�����п���ʼ��ַ */
    pxFirstFreeBlock->xBlockSize = uxAddress - ( size_t ) pxFirstFreeBlock; /* �����һ�����п�Ĵ�С */
    pxFirstFreeBlock->pxNextFreeBlock = pxEnd;                              /* ��һ�����п�ָ������� */

    /* ֻ����һ���� - ���������������õĶѿռ� */
    xMinimumEverFreeBytesRemaining = pxFirstFreeBlock->xBlockSize;  /* ��ʼ����ʷ��С�����ֽ��� */
    xFreeBytesRemaining = pxFirstFreeBlock->xBlockSize;             /* ��ʼ����ǰ�����ֽ��� */

    /* ����size_t���������λ��λ�ã����ڷ���λ��� */
    xBlockAllocatedBit = ( ( size_t ) 1 ) << ( ( sizeof( size_t ) * heapBITS_PER_BYTE ) - 1 );
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * �������ƣ�prvInsertBlockIntoFreeList
 * ���������������п������п��������ڿ��ܵ�����ºϲ����ڵĿ��п�
 *           �˺�����FreeRTOS�ڴ����ĺ����ڲ�����������ά�����п������������
 * ���������
 *   - pxBlockToInsert: ָ��Ҫ����Ŀ��п��ָ��
 * �����������
 * �� �� ֵ����
 * ����˵����
 *   - �˺����Ǿ�̬����������FreeRTOS�ڴ�����ڲ�ʹ��
 *   - ����ַ˳�򽫿��п����������������������
 *   - ��鲢�ϲ����ڵĿ��п飬�����ڴ���Ƭ
 *   - �������ֿ��ܵĺϲ��������ǰ��ϲ�������ϲ���ͬʱ��ǰ���ϲ�
 *   - ʹ�õ�ַ˳�����ȷ���ϲ���������ȷ��
 * 
 * �޸�����      �汾��          �޸���            �޸�����
 * -----------------------------------------------------------------------------
 * 2025/09/01     V1.00          Qiguo_Cui          ����
 *******************************************************************************/
static void prvInsertBlockIntoFreeList( BlockLink_t *pxBlockToInsert )
{
    BlockLink_t *pxIterator;  /* ������������� */
    uint8_t *puc;             /* ���ڵ�ַ������ֽ�ָ�� */

    /* ��������ֱ���ҵ���ַ����Ҫ�����Ŀ� */
    for( pxIterator = &xStart; pxIterator->pxNextFreeBlock < pxBlockToInsert; pxIterator = pxIterator->pxNextFreeBlock )
    {
        /* �˴�����ִ���κβ�����ֻ�ǵ�������ȷλ�� */
    }

    /* ���Ҫ����Ŀ����Ҫ�����Ŀ��Ƿ񹹳��������ڴ�飿 */
    puc = ( uint8_t * ) pxIterator;
    if( ( puc + pxIterator->xBlockSize ) == ( uint8_t * ) pxBlockToInsert )
    {
        /* ��ǰһ�����������ϲ������� */
        pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
        pxBlockToInsert = pxIterator;  /* ����Ҫ����Ŀ�ָ��Ϊ�ϲ���Ŀ� */
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
    }

    /* ���Ҫ����Ŀ����Ҫ����ǰ�Ŀ��Ƿ񹹳��������ڴ�飿 */
    puc = ( uint8_t * ) pxBlockToInsert;
    if( ( puc + pxBlockToInsert->xBlockSize ) == ( uint8_t * ) pxIterator->pxNextFreeBlock )
    {
        /* ���һ��������������һ�����Ƿ��ǽ����� */
        if( pxIterator->pxNextFreeBlock != pxEnd )
        {
            /* ��������ϲ���һ����� */
            pxBlockToInsert->xBlockSize += pxIterator->pxNextFreeBlock->xBlockSize;
            pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock->pxNextFreeBlock;
        }
        else
        {
            /* ��һ�����ǽ����飬ֱ��ָ������� */
            pxBlockToInsert->pxNextFreeBlock = pxEnd;
        }
    }
    else
    {
        /* �����һ��������������������һ����ָ�� */
        pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
    }

    /* ���Ҫ����Ŀ���˼�϶�������ǰ��ͺ�鶼�ϲ��ˣ�
       ��ô����pxNextFreeBlockָ���Ѿ������ã���Ӧ�������ٴ����ã�
       �����ʹ��ָ������ */
    if( pxIterator != pxBlockToInsert )
    {
        /* ����ǰһ�������һ��ָ�룬ָ��Ҫ����Ŀ� */
        pxIterator->pxNextFreeBlock = pxBlockToInsert;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  /* ���Ը����ʱ�� */
    }
}


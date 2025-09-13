/*******************************************************************************
 * 版权所有 (C)2025, CQG
 *
 * 文件名称： LM_list.h
 * 文件标识： 
 * 内容摘要： 链表模块声明
 * 其它说明： 无
 * 当前版本： FreeRTOS V9.0.0
 * 作    者： Qiguo_Cui                   
 * 完成日期： 2025年09月13日
 *
 *******************************************************************************/



/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef LIST_H
#define LIST_H

/* Includes ------------------------------------------------------------------*/
#ifndef INC_FREERTOS_H
	#error FreeRTOS.h must be included before list.h
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef configLIST_VOLATILE
	#define configLIST_VOLATILE
#endif /* configSUPPORT_CROSS_MODULE_OPTIMISATION */	
	
	
#if( configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES == 0 )
	/* Define the macros to do nothing. */
	#define listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE
	#define listSECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE
	#define listFIRST_LIST_INTEGRITY_CHECK_VALUE
	#define listSECOND_LIST_INTEGRITY_CHECK_VALUE
	#define listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem )
	#define listSET_SECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem )
	#define listSET_LIST_INTEGRITY_CHECK_1_VALUE( pxList )
	#define listSET_LIST_INTEGRITY_CHECK_2_VALUE( pxList )
	#define listTEST_LIST_ITEM_INTEGRITY( pxItem )
	#define listTEST_LIST_INTEGRITY( pxList )
#else
	/* Define macros that add new members into the list structures. */
	#define listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE				TickType_t xListItemIntegrityValue1;
	#define listSECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE				TickType_t xListItemIntegrityValue2;
	#define listFIRST_LIST_INTEGRITY_CHECK_VALUE					TickType_t xListIntegrityValue1;
	#define listSECOND_LIST_INTEGRITY_CHECK_VALUE					TickType_t xListIntegrityValue2;

	/* Define macros that set the new structure members to known values. */
	#define listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem )		( pxItem )->xListItemIntegrityValue1 = pdINTEGRITY_CHECK_VALUE
	#define listSET_SECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem )	( pxItem )->xListItemIntegrityValue2 = pdINTEGRITY_CHECK_VALUE
	#define listSET_LIST_INTEGRITY_CHECK_1_VALUE( pxList )		( pxList )->xListIntegrityValue1 = pdINTEGRITY_CHECK_VALUE
	#define listSET_LIST_INTEGRITY_CHECK_2_VALUE( pxList )		( pxList )->xListIntegrityValue2 = pdINTEGRITY_CHECK_VALUE

	/* Define macros that will assert if one of the structure members does not
	contain its expected value. */
	#define listTEST_LIST_ITEM_INTEGRITY( pxItem )		configASSERT( ( ( pxItem )->xListItemIntegrityValue1 == pdINTEGRITY_CHECK_VALUE ) && ( ( pxItem )->xListItemIntegrityValue2 == pdINTEGRITY_CHECK_VALUE ) )
	#define listTEST_LIST_INTEGRITY( pxList )			configASSERT( ( ( pxList )->xListIntegrityValue1 == pdINTEGRITY_CHECK_VALUE ) && ( ( pxList )->xListIntegrityValue2 == pdINTEGRITY_CHECK_VALUE ) )
#endif /* configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES */
	

/* Exported types ------------------------------------------------------------*/
/**
 * 列表项结构体定义
 * 包含列表项的值、前后指针、所有者指针和容器指针
 */
struct xLIST_ITEM
{
	listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE			/*< 如果configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES设置为1，则设置为已知值 */
	configLIST_VOLATILE TickType_t xItemValue;			/*< 列表项的值，通常用于降序排序列表 */
	struct xLIST_ITEM * configLIST_VOLATILE pxNext;		/*< 指向列表中下一个列表项的指针 */
	struct xLIST_ITEM * configLIST_VOLATILE pxPrevious;	/*< 指向列表中前一个列表项的指针 */
	void * pvOwner;										/*< 指向包含列表项的对象（通常是TCB）的指针 */
	void * configLIST_VOLATILE pvContainer;				/*< 指向此列表项所在列表的指针 */
	listSECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE			/*< 如果configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES设置为1，则设置为已知值 */
};
typedef struct xLIST_ITEM ListItem_t;					/* 列表项类型定义 */

/**
 * 迷你列表项结构体定义
 * 包含列表项的值和前后指针
 */
struct xMINI_LIST_ITEM
{
	listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE			/*< 如果configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES设置为1，则设置为已知值 */
	configLIST_VOLATILE TickType_t xItemValue;
	struct xLIST_ITEM * configLIST_VOLATILE pxNext;
	struct xLIST_ITEM * configLIST_VOLATILE pxPrevious;
};
typedef struct xMINI_LIST_ITEM MiniListItem_t;			/* 迷你列表项类型定义 */

/**
 * 列表结构体定义
 * 包含列表的项目数、索引指针和列表结束标记
 */
typedef struct xLIST
{
	listFIRST_LIST_INTEGRITY_CHECK_VALUE				/*< 如果configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES设置为1，则设置为已知值 */
	configLIST_VOLATILE UBaseType_t uxNumberOfItems;	/*< 列表中的项目数量 */
	ListItem_t * configLIST_VOLATILE pxIndex;			/*< 用于遍历列表的索引指针 */
	MiniListItem_t xListEnd;							/*< 列表结束标记项 */
	listSECOND_LIST_INTEGRITY_CHECK_VALUE				/*< 如果configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES设置为1，则设置为已知值 */
} List_t;												/* 列表类型定义 */

/* Exported constants --------------------------------------------------------*/
/* 注：列表模块没有导出的常量定义 */

/* Exported macro ------------------------------------------------------------*/
/**
 * 设置列表项的所有者
 * @param pxListItem 要设置所有者的列表项
 * @param pxOwner 所有者对象指针
 */
#define listSET_LIST_ITEM_OWNER( pxListItem, pxOwner )		( ( pxListItem )->pvOwner = ( void * ) ( pxOwner ) )

/**
 * 获取列表项的所有者
 * @param pxListItem 要获取所有者的列表项
 * @return 列表项的所有者指针
 */
#define listGET_LIST_ITEM_OWNER( pxListItem )	( ( pxListItem )->pvOwner )

/**
 * 设置列表项的值
 * @param pxListItem 要设置值的列表项
 * @param xValue 要设置的值
 */
#define listSET_LIST_ITEM_VALUE( pxListItem, xValue )	( ( pxListItem )->xItemValue = ( xValue ) )

/**
 * 获取列表项的值
 * @param pxListItem 要获取值的列表项
 * @return 列表项的值
 */
#define listGET_LIST_ITEM_VALUE( pxListItem )	( ( pxListItem )->xItemValue )

/**
 * 获取列表头部项的值
 * @param pxList 要获取头部项值的列表
 * @return 列表头部项的值
 */
#define listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxList )	( ( ( pxList )->xListEnd ).pxNext->xItemValue )

/**
 * 获取列表的头部项
 * @param pxList 要获取头部项的列表
 * @return 列表的头部项指针
 */
#define listGET_HEAD_ENTRY( pxList )	( ( ( pxList )->xListEnd ).pxNext )

/**
 * 获取列表项的下一个项
 * @param pxListItem 当前列表项
 * @return 下一个列表项指针
 */
#define listGET_NEXT( pxListItem )	( ( pxListItem )->pxNext )

/**
 * 获取列表的结束标记
 * @param pxList 要获取结束标记的列表
 * @return 列表结束标记指针
 */
#define listGET_END_MARKER( pxList )	( ( ListItem_t const * ) ( &( ( pxList )->xListEnd ) ) )

/**
 * 检查列表是否为空
 * @param pxList 要检查的列表
 * @return 如果列表为空返回true，否则返回false
 */
#define listLIST_IS_EMPTY( pxList )	( ( BaseType_t ) ( ( pxList )->uxNumberOfItems == ( UBaseType_t ) 0 ) )

/**
 * 获取列表的当前长度
 * @param pxList 要获取长度的列表
 * @return 列表中的项目数量
 */
#define listCURRENT_LIST_LENGTH( pxList )	( ( pxList )->uxNumberOfItems )

/**
 * 获取下一个列表项的所有者
 * @param pxTCB 用于存储下一个列表项所有者的变量
 * @param pxList 要遍历的列表
 */
#define listGET_OWNER_OF_NEXT_ENTRY( pxTCB, pxList )										\
{																							\
List_t * const pxConstList = ( pxList );													\
	( pxConstList )->pxIndex = ( pxConstList )->pxIndex->pxNext;							\
	if( ( void * ) ( pxConstList )->pxIndex == ( void * ) &( ( pxConstList )->xListEnd ) )	\
	{																						\
		( pxConstList )->pxIndex = ( pxConstList )->pxIndex->pxNext;						\
	}																						\
	( pxTCB ) = ( pxConstList )->pxIndex->pvOwner;											\
}

/**
 * 获取列表头部项的所有者
 * @param pxList 要获取头部项所有者的列表
 * @return 列表头部项的所有者指针
 */
#define listGET_OWNER_OF_HEAD_ENTRY( pxList )  ( (&( ( pxList )->xListEnd ))->pxNext->pvOwner )

/**
 * 检查列表项是否在指定列表中
 * @param pxList 要检查的列表
 * @param pxListItem 要检查的列表项
 * @return 如果列表项在列表中返回pdTRUE，否则返回pdFALSE
 */
#define listIS_CONTAINED_WITHIN( pxList, pxListItem ) ( ( BaseType_t ) ( ( pxListItem )->pvContainer == ( void * ) ( pxList ) ) )

/**
 * 获取列表项所在的容器列表
 * @param pxListItem 要获取容器的列表项
 * @return 列表项所在的列表指针
 */
#define listLIST_ITEM_CONTAINER( pxListItem ) ( ( pxListItem )->pvContainer )

/**
 * 检查列表是否已初始化
 * @param pxList 要检查的列表
 * @return 如果列表已初始化返回true，否则返回false
 */
#define listLIST_IS_INITIALISED( pxList ) ( ( pxList )->xListEnd.xItemValue == portMAX_DELAY )

/* Exported functions --------------------------------------------------------*/
/* 注：列表模块没有直接导出的函数，所有函数都是特权函数 */

/* Private types -------------------------------------------------------------*/
/* 注：私有类型定义已包含在导出类型中 */

/* Private variables ---------------------------------------------------------*/
/* 注：列表模块没有私有变量定义 */

/* Private constants ---------------------------------------------------------*/
/* 注：列表模块没有私有常量定义 */

/* Private macros ------------------------------------------------------------*/
/* 注：私有宏定义已包含在导出宏中 */

/* Private functions ---------------------------------------------------------*/
/**
 * 初始化列表
 * 必须在列表使用前调用，初始化列表结构的所有成员并将xListEnd项插入列表作为列表末尾的标记
 * @param pxList 要初始化的列表指针
 */
void vListInitialise( List_t * const pxList ) PRIVILEGED_FUNCTION;

/**
 * 初始化列表项
 * 必须在列表项使用前调用，将列表容器设置为null，使项不会认为它已经包含在列表中
 * @param pxItem 要初始化的列表项指针
 */
void vListInitialiseItem( ListItem_t * const pxItem ) PRIVILEGED_FUNCTION;

/**
 * 将列表项插入列表
 * 项将根据其项值（降序项值顺序）插入列表中的位置
 * @param pxList 要插入项的列表
 * @param pxNewListItem 要插入列表的项
 */
void vListInsert( List_t * const pxList, ListItem_t * const pxNewListItem ) PRIVILEGED_FUNCTION;

/**
 * 将列表项插入列表末尾
 * 项将插入到列表末尾位置
 * @param pxList 要插入项的列表
 * @param pxNewListItem 要插入列表的项
 */
void vListInsertEnd( List_t * const pxList, ListItem_t * const pxNewListItem ) PRIVILEGED_FUNCTION;

/**
 * 从列表中移除项
 * 列表项有一个指向它所在列表的指针，因此只需要将列表项传递给函数
 * @param pxItemToRemove 要移除的项
 * @return 移除项后列表中剩余的项目数量
 */
UBaseType_t uxListRemove( ListItem_t * const pxItemToRemove ) PRIVILEGED_FUNCTION;

#ifdef __cplusplus
}
#endif

#endif /* LIST_H */

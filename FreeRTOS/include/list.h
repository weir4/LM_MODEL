/*******************************************************************************
 * ��Ȩ���� (C)2025, CQG
 *
 * �ļ����ƣ� LM_list.h
 * �ļ���ʶ�� 
 * ����ժҪ�� ����ģ������
 * ����˵���� ��
 * ��ǰ�汾�� FreeRTOS V9.0.0
 * ��    �ߣ� Qiguo_Cui                   
 * ������ڣ� 2025��09��13��
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
 * �б���ṹ�嶨��
 * �����б����ֵ��ǰ��ָ�롢������ָ�������ָ��
 */
struct xLIST_ITEM
{
	listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE			/*< ���configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES����Ϊ1��������Ϊ��ֵ֪ */
	configLIST_VOLATILE TickType_t xItemValue;			/*< �б����ֵ��ͨ�����ڽ��������б� */
	struct xLIST_ITEM * configLIST_VOLATILE pxNext;		/*< ָ���б�����һ���б����ָ�� */
	struct xLIST_ITEM * configLIST_VOLATILE pxPrevious;	/*< ָ���б���ǰһ���б����ָ�� */
	void * pvOwner;										/*< ָ������б���Ķ���ͨ����TCB����ָ�� */
	void * configLIST_VOLATILE pvContainer;				/*< ָ����б��������б��ָ�� */
	listSECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE			/*< ���configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES����Ϊ1��������Ϊ��ֵ֪ */
};
typedef struct xLIST_ITEM ListItem_t;					/* �б������Ͷ��� */

/**
 * �����б���ṹ�嶨��
 * �����б����ֵ��ǰ��ָ��
 */
struct xMINI_LIST_ITEM
{
	listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE			/*< ���configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES����Ϊ1��������Ϊ��ֵ֪ */
	configLIST_VOLATILE TickType_t xItemValue;
	struct xLIST_ITEM * configLIST_VOLATILE pxNext;
	struct xLIST_ITEM * configLIST_VOLATILE pxPrevious;
};
typedef struct xMINI_LIST_ITEM MiniListItem_t;			/* �����б������Ͷ��� */

/**
 * �б�ṹ�嶨��
 * �����б����Ŀ��������ָ����б�������
 */
typedef struct xLIST
{
	listFIRST_LIST_INTEGRITY_CHECK_VALUE				/*< ���configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES����Ϊ1��������Ϊ��ֵ֪ */
	configLIST_VOLATILE UBaseType_t uxNumberOfItems;	/*< �б��е���Ŀ���� */
	ListItem_t * configLIST_VOLATILE pxIndex;			/*< ���ڱ����б������ָ�� */
	MiniListItem_t xListEnd;							/*< �б��������� */
	listSECOND_LIST_INTEGRITY_CHECK_VALUE				/*< ���configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES����Ϊ1��������Ϊ��ֵ֪ */
} List_t;												/* �б����Ͷ��� */

/* Exported constants --------------------------------------------------------*/
/* ע���б�ģ��û�е����ĳ������� */

/* Exported macro ------------------------------------------------------------*/
/**
 * �����б����������
 * @param pxListItem Ҫ���������ߵ��б���
 * @param pxOwner �����߶���ָ��
 */
#define listSET_LIST_ITEM_OWNER( pxListItem, pxOwner )		( ( pxListItem )->pvOwner = ( void * ) ( pxOwner ) )

/**
 * ��ȡ�б����������
 * @param pxListItem Ҫ��ȡ�����ߵ��б���
 * @return �б����������ָ��
 */
#define listGET_LIST_ITEM_OWNER( pxListItem )	( ( pxListItem )->pvOwner )

/**
 * �����б����ֵ
 * @param pxListItem Ҫ����ֵ���б���
 * @param xValue Ҫ���õ�ֵ
 */
#define listSET_LIST_ITEM_VALUE( pxListItem, xValue )	( ( pxListItem )->xItemValue = ( xValue ) )

/**
 * ��ȡ�б����ֵ
 * @param pxListItem Ҫ��ȡֵ���б���
 * @return �б����ֵ
 */
#define listGET_LIST_ITEM_VALUE( pxListItem )	( ( pxListItem )->xItemValue )

/**
 * ��ȡ�б�ͷ�����ֵ
 * @param pxList Ҫ��ȡͷ����ֵ���б�
 * @return �б�ͷ�����ֵ
 */
#define listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxList )	( ( ( pxList )->xListEnd ).pxNext->xItemValue )

/**
 * ��ȡ�б��ͷ����
 * @param pxList Ҫ��ȡͷ������б�
 * @return �б��ͷ����ָ��
 */
#define listGET_HEAD_ENTRY( pxList )	( ( ( pxList )->xListEnd ).pxNext )

/**
 * ��ȡ�б������һ����
 * @param pxListItem ��ǰ�б���
 * @return ��һ���б���ָ��
 */
#define listGET_NEXT( pxListItem )	( ( pxListItem )->pxNext )

/**
 * ��ȡ�б�Ľ������
 * @param pxList Ҫ��ȡ������ǵ��б�
 * @return �б�������ָ��
 */
#define listGET_END_MARKER( pxList )	( ( ListItem_t const * ) ( &( ( pxList )->xListEnd ) ) )

/**
 * ����б��Ƿ�Ϊ��
 * @param pxList Ҫ�����б�
 * @return ����б�Ϊ�շ���true�����򷵻�false
 */
#define listLIST_IS_EMPTY( pxList )	( ( BaseType_t ) ( ( pxList )->uxNumberOfItems == ( UBaseType_t ) 0 ) )

/**
 * ��ȡ�б�ĵ�ǰ����
 * @param pxList Ҫ��ȡ���ȵ��б�
 * @return �б��е���Ŀ����
 */
#define listCURRENT_LIST_LENGTH( pxList )	( ( pxList )->uxNumberOfItems )

/**
 * ��ȡ��һ���б����������
 * @param pxTCB ���ڴ洢��һ���б��������ߵı���
 * @param pxList Ҫ�������б�
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
 * ��ȡ�б�ͷ�����������
 * @param pxList Ҫ��ȡͷ���������ߵ��б�
 * @return �б�ͷ�����������ָ��
 */
#define listGET_OWNER_OF_HEAD_ENTRY( pxList )  ( (&( ( pxList )->xListEnd ))->pxNext->pvOwner )

/**
 * ����б����Ƿ���ָ���б���
 * @param pxList Ҫ�����б�
 * @param pxListItem Ҫ�����б���
 * @return ����б������б��з���pdTRUE�����򷵻�pdFALSE
 */
#define listIS_CONTAINED_WITHIN( pxList, pxListItem ) ( ( BaseType_t ) ( ( pxListItem )->pvContainer == ( void * ) ( pxList ) ) )

/**
 * ��ȡ�б������ڵ������б�
 * @param pxListItem Ҫ��ȡ�������б���
 * @return �б������ڵ��б�ָ��
 */
#define listLIST_ITEM_CONTAINER( pxListItem ) ( ( pxListItem )->pvContainer )

/**
 * ����б��Ƿ��ѳ�ʼ��
 * @param pxList Ҫ�����б�
 * @return ����б��ѳ�ʼ������true�����򷵻�false
 */
#define listLIST_IS_INITIALISED( pxList ) ( ( pxList )->xListEnd.xItemValue == portMAX_DELAY )

/* Exported functions --------------------------------------------------------*/
/* ע���б�ģ��û��ֱ�ӵ����ĺ��������к���������Ȩ���� */

/* Private types -------------------------------------------------------------*/
/* ע��˽�����Ͷ����Ѱ����ڵ��������� */

/* Private variables ---------------------------------------------------------*/
/* ע���б�ģ��û��˽�б������� */

/* Private constants ---------------------------------------------------------*/
/* ע���б�ģ��û��˽�г������� */

/* Private macros ------------------------------------------------------------*/
/* ע��˽�к궨���Ѱ����ڵ������� */

/* Private functions ---------------------------------------------------------*/
/**
 * ��ʼ���б�
 * �������б�ʹ��ǰ���ã���ʼ���б�ṹ�����г�Ա����xListEnd������б���Ϊ�б�ĩβ�ı��
 * @param pxList Ҫ��ʼ�����б�ָ��
 */
void vListInitialise( List_t * const pxList ) PRIVILEGED_FUNCTION;

/**
 * ��ʼ���б���
 * �������б���ʹ��ǰ���ã����б���������Ϊnull��ʹ�����Ϊ���Ѿ��������б���
 * @param pxItem Ҫ��ʼ�����б���ָ��
 */
void vListInitialiseItem( ListItem_t * const pxItem ) PRIVILEGED_FUNCTION;

/**
 * ���б�������б�
 * ���������ֵ��������ֵ˳�򣩲����б��е�λ��
 * @param pxList Ҫ��������б�
 * @param pxNewListItem Ҫ�����б����
 */
void vListInsert( List_t * const pxList, ListItem_t * const pxNewListItem ) PRIVILEGED_FUNCTION;

/**
 * ���б�������б�ĩβ
 * ����뵽�б�ĩβλ��
 * @param pxList Ҫ��������б�
 * @param pxNewListItem Ҫ�����б����
 */
void vListInsertEnd( List_t * const pxList, ListItem_t * const pxNewListItem ) PRIVILEGED_FUNCTION;

/**
 * ���б����Ƴ���
 * �б�����һ��ָ���������б��ָ�룬���ֻ��Ҫ���б���ݸ�����
 * @param pxItemToRemove Ҫ�Ƴ�����
 * @return �Ƴ�����б���ʣ�����Ŀ����
 */
UBaseType_t uxListRemove( ListItem_t * const pxItemToRemove ) PRIVILEGED_FUNCTION;

#ifdef __cplusplus
}
#endif

#endif /* LIST_H */

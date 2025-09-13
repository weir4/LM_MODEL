/*******************************************************************************
 * ��Ȩ���� (C)2025, CQG
 *
 * �ļ����ƣ� LM_port.c
 * �ļ���ʶ�� 
 * ����ժҪ�� M3-M4��ֲģ������
 * ����˵���� ��
 * ��ǰ�汾�� FreeRTOS V9.0.0
 * ��    �ߣ� Qiguo_Cui
 * ������ڣ� 2025��09��01��
 *
 *******************************************************************************/
/*-----------------------------------------------------------
 * ��ʵ����Դ�FPU��Cortex-M4�ںˣ������ڴ�����������л��͵͹���֧��
 *
 *
 *----------------------------------------------------------*/

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"

/* Private define ------------------------------------------------------------
 *
 * 1��Ӳ��������飺ȷ��Ŀ��ƽ̨֧��Ӳ�����㵥Ԫ(VFP)         M4�ں˶���
 * 2���ж����ȼ���֤����ֹϵͳ�����ж����ȼ����ô���
 * 3��SysTickʱ�����ã��ṩϵͳ���Ķ�ʱ����ʱ��Դѡ��
 * 4��NVIC�Ĵ������壺ֱ��ӳ��Cortex-M�ں˵�ϵͳ���ƼĴ���
 * 5��оƬʶ�����⴦��Cortex-M7 r0p1�汾�ļ�����
 * 6�����ȼ����ã�����PendSV��SysTick�жϵ����ȼ�
 * 7��VFP֧�֣����帡�������Ŀ�����ؼĴ�����λ
 * 8����ջ��ʼ�������������ʼ�����ĵĹؼ�ֵ
 * 9���޿��н���֧�֣��ṩ�͹���ģʽ����ĸ��ֳ����Ͳ�������
 *
 *-----------------------------------------------------------------------------*/
 
/*1�� ����Ƿ�����Ŀ��FPU���ͣ��������VFP�򱨴�   
      �˶˿ڽ�����Ŀ����������Ӳ������֧��ʱ����ʹ��*/
#ifndef __TARGET_FPU_VFP
	#error This port can only be used when the project options are configured to enable hardware floating point support.
#endif

/* 2��ȷ��ϵͳ�����ж����ȼ���Ϊ0 */
#if configMAX_SYSCALL_INTERRUPT_PRIORITY == 0 
	#error configMAX_SYSCALL_INTERRUPT_PRIORITY must not be set to 0.  See http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html
#endif

/* 3�����û�ж���ϵͳʱ��Ƶ�ʣ���ʹ��CPUʱ��Ƶ�� */
#ifndef configSYSTICK_CLOCK_HZ
	#define configSYSTICK_CLOCK_HZ configCPU_CLOCK_HZ
	/* ȷ��SysTick�����ں���ͬ��Ƶ��ʱ�ӹ��� */
	#define portNVIC_SYSTICK_CLK_BIT	( 1UL << 2UL )   /* SysTick���ƼĴ���ʱ��Դѡ��λ(ʹ���ں�ʱ��) */
#else
	/* ���SysTickʱ�����ں�ʱ�Ӳ�ͬ�����޸�ʱ�ӷ�ʽ */
	#define portNVIC_SYSTICK_CLK_BIT	( 0 )            /* ʹ���ⲿ�ο�ʱ�� */
#endif

/* ��Keil������__weak���Կ����޷���Ԥ�ڹ�����������Ӧ�ó����д����Ҫ�ṩ�Լ���
vPortSetupTimerInterrupt()ʵ�֣����뽫configOVERRIDE_DEFAULT_TICK_CONFIGURATION��������Ϊ1��
ȷ��������configOVERRIDE_DEFAULT_TICK_CONFIGURATION�� */
#ifndef configOVERRIDE_DEFAULT_TICK_CONFIGURATION
	#define configOVERRIDE_DEFAULT_TICK_CONFIGURATION 0   /* Ĭ��ʹ�ö˿��ṩ�Ķ�ʱ������ */
#endif

/* 4�������ں�����ĳ����������ǼĴ�����ַ... */
#define portNVIC_SYSTICK_CTRL_REG			( * ( ( volatile uint32_t * ) 0xe000e010 ) )          /* SysTick���ƺ�״̬�Ĵ��� */
#define portNVIC_SYSTICK_LOAD_REG			( * ( ( volatile uint32_t * ) 0xe000e014 ) )          /* SysTick����ֵ�Ĵ��� */
#define portNVIC_SYSTICK_CURRENT_VALUE_REG	( * ( ( volatile uint32_t * ) 0xe000e018 ) )    /* SysTick��ǰֵ�Ĵ��� */
#define portNVIC_SYSPRI2_REG				( * ( ( volatile uint32_t * ) 0xe000ed20 ) )            /* ϵͳ���ȼ��Ĵ���2 */  
  
/* ...Ȼ���ǼĴ����е�λ���塣 */
#define portNVIC_SYSTICK_INT_BIT			( 1UL << 1UL )            /* SysTick�ж�ʹ��λ */
#define portNVIC_SYSTICK_ENABLE_BIT			( 1UL << 0UL )          /* SysTick������ʹ��λ */
#define portNVIC_SYSTICK_COUNT_FLAG_BIT		( 1UL << 16UL )       /* SysTick������־λ(������0ʱ��λ) */
#define portNVIC_PENDSVCLEAR_BIT 			( 1UL << 27UL )           /* PendSV�������λ */
#define portNVIC_PEND_SYSTICK_CLEAR_BIT		( 1UL << 25UL )       /* SysTick�������λ */

/* 5�����ڼ��Cortex-M7 r0p1�ں˵ĳ�����Ӧʹ��ARM_CM7 r0p1�˿� */
#define portCPUID							( * ( ( volatile uint32_t * ) 0xE000ed00 ) )     /* CPUID�Ĵ��� */
#define portCORTEX_M7_r0p1_ID				( 0x410FC271UL )                           /* Cortex-M7 r0p1оƬ��ʶ */
#define portCORTEX_M7_r0p0_ID				( 0x410FC270UL )                           /* Cortex-M7 r0p0оƬ��ʶ */

/* 6�����ȼ����ã�����PendSV��SysTick�жϵ����ȼ� */
#define portNVIC_PENDSV_PRI					( ( ( uint32_t ) configKERNEL_INTERRUPT_PRIORITY ) << 16UL )   /* PendSV�ж����ȼ����� */
#define portNVIC_SYSTICK_PRI				( ( ( uint32_t ) configKERNEL_INTERRUPT_PRIORITY ) << 24UL )   /* SysTick�ж����ȼ����� */

/* ����ж����ȼ���Ч������ĳ��� */
#define portFIRST_USER_INTERRUPT_NUMBER		( 16 )                                /* ��һ���û����õ��жϱ�� */
#define portNVIC_IP_REGISTERS_OFFSET_16 	( 0xE000E3F0 )                        /* �ж����ȼ��Ĵ���ƫ����(�ӵ�16���жϿ�ʼ) */
#define portAIRCR_REG						( * ( ( volatile uint32_t * ) 0xE000ED0C ) )    /* Ӧ�ó����жϺ͸�λ���ƼĴ��� */ 
#define portMAX_8_BIT_VALUE					( ( uint8_t ) 0xff )                        /* 8λ���ֵ */
#define portTOP_BIT_OF_BYTE					( ( uint8_t ) 0x80 )                        /* �ֽڵ����λ */
#define portMAX_PRIGROUP_BITS				( ( uint8_t ) 7 )                           /* ������ȼ���λ�� */        
#define portPRIORITY_GROUP_MASK				( 0x07UL << 8UL )                         /* ���ȼ�������� */
#define portPRIGROUP_SHIFT					( 8UL )                                     /* ���ȼ����ƫ���� */


/* ����ICSR�Ĵ����г�VECTACTIVEλ�������λ */
#define portVECTACTIVE_MASK					( 0xFFUL )                                  /* ��ǰ��жϺ����� */


/* 7������VFP����ĳ��� */
#define portFPCCR					( ( volatile uint32_t * ) 0xe000ef34 )                /* ���������Ŀ��ƼĴ�����ַ */
#define portASPEN_AND_LSPEN_BITS	( 0x3UL << 30UL )                             /* �Զ�״̬����Ͷ���״̬����ʹ��λ */


/* 8�����ó�ʼ��ջ����ĳ��� */
#define portINITIAL_XPSR			( 0x01000000 )                                    /* ��ʼ����״̬�Ĵ���ֵ(Thumb״̬) */
#define portINITIAL_EXEC_RETURN		( 0xfffffffd )                                /* ��ʼ�쳣����ֵ(���ص��߳�ģʽ��ʹ��PSP) */


/* SysTick��һ��24λ������ */
#define portMAX_24_BIT_NUMBER		( 0xffffffUL )                                  /* 24λ���ֵ */

/* һ���������ӣ����ڹ������޿��н��ļ����ڼ�SysTick������ֹͣʱ���ܷ�����SysTick�������� */
#define portMISSED_COUNTS_FACTOR	( 45UL )                                      /* ����ļ����������� */

/* Ϊ���ϸ����Cortex-M�淶��������ʼ��ַӦ���λ0����Ϊ�������˳�ISRʱ���ص�PC�� */
#define portSTART_ADDRESS_MASK		( ( StackType_t ) 0xfffffffeUL )              /* �����ַ����(ȷ����ַ����) */


/* Private variables ---------------------------------------------------------*/

/* �ٽ���Ƕ�׼���������ʼ��Ϊ0xaaaaaaaa���ڵ��ԣ�ͨ��Ϊ��ʶ���ģʽֵ����0xaaaaaaaa�ڵ���ʱ����ʶ���ڴ�״̬�� */
static UBaseType_t uxCriticalNesting = 0xaaaaaaaa;                      


/*
 * ���һ���������������SysTick����������
 * �����޿��н���(tickless idle)ģʽ�ļ��㡣
 */
#if configUSE_TICKLESS_IDLE == 1
	static uint32_t ulTimerCountsForOneTick = 0;           /* һ���������ڶ�Ӧ�Ķ�ʱ������ֵ */
#endif /* configUSE_TICKLESS_IDLE */

/*
 * �����Ƶ���������������SysTick��ʱ��24λ�ֱ��ʵ����ơ� 
 * �����޿��н���ģʽ��ȷ������˯��ʱ�䡣
 */
#if configUSE_TICKLESS_IDLE == 1
	static uint32_t xMaximumPossibleSuppressedTicks = 0;   /* �������ƵĽ����� */
#endif /* configUSE_TICKLESS_IDLE */

/*
 * ����SysTickֹͣʱ���ݵ�CPU�������������ڵ͹��Ĺ��ܣ���
 * �����޿��н���ģʽ��У����ʱ��ֹͣ��ɵ�ʱ����
 */
#if configUSE_TICKLESS_IDLE == 1
	static uint32_t ulStoppedTimerCompensation = 0;       /* ֹͣ��ʱ������ֵ */
#endif /* configUSE_TICKLESS_IDLE */

/*
 * ��portASSERT_IF_INTERRUPT_PRIORITY_INVALID()��ʹ�ã�ȷ��FreeRTOS API����
 * ������ѱ��������configMAX_SYSCALL_INTERRUPT_PRIORITY���ȼ����ж��е��á�
 * ���ڵ���ģʽ��(configASSERT_DEFINED == 1)��Ч��
 */
#if ( configASSERT_DEFINED == 1 )
	 static uint8_t ucMaxSysCallPriority = 0;   /* ���ϵͳ�������ȼ�ֵ */
	 static uint32_t ulMaxPRIGROUPValue = 0;    /* ������ȼ���ֵ */
	                                            /* ָ���ж����ȼ��Ĵ����ĳ���ָ�룬�ӵ�16���жϿ�ʼ */
	 static const volatile uint8_t * const pcInterruptPriorityRegisters = ( uint8_t * ) portNVIC_IP_REGISTERS_OFFSET_16;
#endif /* configASSERT_DEFINED */

/* Private function prototypes -----------------------------------------------*/

/*
 * ���ö�ʱ�������ɽ����жϡ����ļ��е�ʵ��Ϊ������(__weak)��
 * ����Ӧ�ó����д�߸����������ɽ����жϵĶ�ʱ����
 * �����������
 * �����������
 */
void vPortSetupTimerInterrupt( void );


/*
 * �쳣�������
 */
void xPortPendSVHandler( void );
void xPortSysTickHandler( void );
void vPortSVCHandler( void );

/*
 * ������һ��������Ϊһ�������������Ա���Ե������ԡ�
 * �����������
 * �����������
 */
static void prvStartFirstTask( void );

/*
 * ��portasm.s�ж���ĺ�������������VFP(�������㵥Ԫ)��
 * �����������
 * �����������
 */
static void prvEnableVFP( void );

/*
 * ���ڲ�����ͼ����ʵ�ֺ������ص�������������Ӧ���أ�Ӧ����ѭ��������ɾ������
 * �����������
 * �����������
 */
static void prvTaskExitError( void );


/* functions -------------------------------------------------------------------*/

/*******************************************************************************
 �������ƣ�    StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters )
 ����������    ��ʼ�������ջ֡ - ģ���������л��ж�ʱӲ���Զ�����ļĴ���״̬
 ���������    pxTopOfStack: ָ�������ջ������ָ�루��ջ�Ӹߵ�ַ��͵�ַ������
               pxCode: ����������ڵ�ַ������Ҫִ�еĴ��룩
               pvParameters: ���ݸ��������Ĳ���ָ��
 ���������    ��
 �� �� ֵ��    ָ���ʼ�����ջ֡�ײ���ָ�루���������Ŀ�ʼ��λ�ã�
 ����˵����    �˺���ר������ARM Cortex-M�ܹ��������쳣����ʱӲ���Զ�����Ĵ�����˳�򹹽���ջ֡
 
 �ߵ�ַ
+-------------------+  �� ��ʼ pxTopOfStack
|   ����Ԥ���ռ�     |  (pxTopOfStack--)
+-------------------+
|      xPSR         |  (��ʼ����״̬�Ĵ���)
+-------------------+
|        PC         |  (������ڵ�ַ)
+-------------------+
|        LR         |  (ָ�� prvTaskExitError)
+-------------------+
|      R12          |  (δ��ʼ��)
+-------------------+
|       R3          |  (δ��ʼ��)
+-------------------+
|       R2          |  (δ��ʼ��)
+-------------------+
|       R1          |  (δ��ʼ��)
+-------------------+
|   R0 (����ָ��)    |  (pvParameters)
+-------------------+
|   EXC_RETURN      |  (�쳣����ֵ)
+-------------------+
|       R11         |  (δ��ʼ��)
+-------------------+
|       R10         |  (δ��ʼ��)
+-------------------+
|        R9         |  (δ��ʼ��)
+-------------------+
|        R8         |  (δ��ʼ��)
+-------------------+
|        R7         |  (δ��ʼ��)
+-------------------+
|        R6         |  (δ��ʼ��)
+-------------------+
|        R5         |  (δ��ʼ��)
+-------------------+
|        R4         |  (δ��ʼ��)
+-------------------+  �� ���ص� pxTopOfStack (��ջ֡�ײ�)
�͵�ַ
               
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2025/09/01     V1.0          Qiguo_Cui          ����
 *******************************************************************************/
StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters )
{
  /* ģ���������л��ж�ʱ�����Ķ�ջ֡�ṹ */

  
  pxTopOfStack--;                    /* ����ջָ��ƫ�ƣ�����MCU���жϽ���/�˳�ʱʹ�ö�ջ�ķ�ʽ����ȷ������Ҫ�� */
  
	*pxTopOfStack = portINITIAL_XPSR;  /* ���ó�ʼ����״̬�Ĵ���ֵ (xPSR) - ����Thumb״̬λ */
  pxTopOfStack--;
	
  *pxTopOfStack = ( ( StackType_t ) pxCode ) & portSTART_ADDRESS_MASK;  /* ���ó�������� (PC) - ָ��������ڵ�ַ��ȷ������ */
  pxTopOfStack--;
	
  *pxTopOfStack = ( StackType_t ) prvTaskExitError;  /* �������ӼĴ��� (LR) - ָ�������˳��������� */
  pxTopOfStack -= 5;  /* �������ּĴ�����ʼ���Խ�ʡ����ռ� - ΪR12��R3��R2��R1Ԥ���ռ� */
	
  *pxTopOfStack = ( StackType_t ) pvParameters;  /* ����R0�Ĵ��� - �洢�������ָ�� */
  pxTopOfStack--;
	                                         /* ʹ��һ�ֱ��淽����Ҫ��ÿ������ά���Լ����쳣����ֵ */
  *pxTopOfStack = portINITIAL_EXEC_RETURN; /* �����쳣����ֵ - ���������״�ִ��ʱ�Ĵ�����ģʽ�Ͷ�ջָ��ѡ�� */

  
  pxTopOfStack -= 8;       /* Ϊʣ��Ĵ���Ԥ���ռ� - R11, R10, R9, R8, R7, R6, R5 and R4 */
  
  return pxTopOfStack;    /* ���س�ʼ����Ķ�ջָ��λ�ã�ָ���ջ֡�ײ��� */
}



/*******************************************************************************
�������ƣ�prvTaskExitError
���������������˳��������������ڲ����������Ƿ����صĴ��������
          ����������ͼ����ʱ����ô˺������������Բ������жϣ�������ѭ����
�����������
�����������
�� �� ֵ����
����˵����1.�����������������أ�Ӧͨ������vTaskDelete(NULL)����ֹ����
          2.�˺�����ǿ�ƴ�������(���configASSERT�Ѷ���)�����������ж�
          3.������ڰ��������߲������񷵻ش���

�޸�����      �汾��          �޸���            �޸�����
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          ����
*******************************************************************************/

static void prvTaskExitError( void )
{
    /* ʵ������ĺ��������˳����Է���������ߣ���Ϊû�пɷ��صĵط���
       ���������Ҫ�˳�����Ӧ�õ���vTaskDelete(NULL)��

       ���������configASSERT()�����˹�ǿ�ƴ���assert()��
       Ȼ���ڴ�ֹͣ���Ա�Ӧ�ó����д�߿��Բ������ */
    configASSERT( uxCriticalNesting == ~0UL );  /* �������ԣ�����ٽ���Ƕ�׼����Ƿ�Ϊ���ֵ */
    portDISABLE_INTERRUPTS();                    /* ���������жϣ���ֹ��һ��ִ�� */
    for( ;; );                                   /* ����ѭ����ֹͣϵͳ���� */
}



/*******************************************************************************
�������ƣ�vPortSVCHandler
����������FreeRTOS SVC�жϷ������̣����������������л������������������״�����������ʱ��
          ͨ��SVC�쳣ʵ�ִ���Ȩģʽ�������û�ģʽ���л������ָ����������ġ�
����������ޣ�Ӳ���Զ�������
�����������
�� �� ֵ����
����˵����1.ʹ�ý���ջָ��(PSP)�������������Ļָ�
          2.ͨ��ldmiaָ��������ջ�ָ��Ĵ���
          3.���basepri�Ĵ��������������ж�

�޸�����      �汾��          �޸���            �޸�����
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          ����
*******************************************************************************/

__asm void vPortSVCHandler( void )
{
    PRESERVE8          /* ָ��8�ֽ�ջ���� */

    /* ��ȡ��ǰ������ƿ�(TCB)��ַ */
    ldr    r3, =pxCurrentTCB   /* ����pxCurrentTCBȫ�ֱ�����ַ��r3 */
    ldr    r1, [r3]            /* ��ȡpxCurrentTCBָ��ֵ����TCB��ַ����r1 */
    ldr    r0, [r1]            /* ��TCB�׳�Ա��ȡ����ջ��ָ�뵽r0 */

    /* �Ӷ�ջ�ָ��ں˼Ĵ��� */
    ldmia r0!, {r4-r11, r14}   /* ��r0ָ��Ķ�ջ���ε���r4-r11��r14��ͬʱr0���� */

    msr psp, r0                /* �����º�Ķ�ջָ��ֵ�������ջָ��(PSP) */
    isb                        /* ָ��ͬ�����ϣ�ȷ��pspд����� */

    mov r0, #0                 /* ��0����r0�Ĵ��� */
    msr    basepri, r0         /* ���basepri�Ĵ��������������жϣ� */

    bx r14                     /* ���ص�������루ͨ��lr�Ĵ����� */
}

/*******************************************************************************
�������ƣ�prvStartFirstTask
��������������FreeRTOS�ĵ�һ�����񣬳�ʼ������ջָ��(MSP)��ȫ�������жϣ�
          ��ͨ��SVC���ô�����һ�������л������ǵ����������Ĺؼ���ʼ��������
�����������
�����������
�� �� ֵ����
����˵����1.��NVIC��������ƫ�ƼĴ�����ȡ��ʼ��ջֵ
          2.��������ջָ��(MSP)Ϊ��λֵ
          3.���������ж�(IRQ��FIQ)
          4.ʹ��SVC 0ָ�����һ�����������

�޸�����      �汾��          �޸���            �޸�����
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          ����
*******************************************************************************/

__asm void prvStartFirstTask( void )
{
    PRESERVE8          /* ָ��8�ֽ�ջ���� */

    /* ʹ��NVICƫ�ƼĴ�����λ����ջ��ʼλ�� */
    ldr r0, =0xE000ED08   /* ����VTOR�Ĵ�����ַ(������ƫ�ƼĴ���)��r0 */
    ldr r0, [r0]          /* ��ȡ���������ַ(�洢MSP��ʼֵ��λ��) */
    ldr r0, [r0]          /* ��ȡ����ջָ��(MSP)�ĳ�ʼֵ */

    /* ��MSP����Ϊ��ջ��ʼλ�� */
    msr msp, r0           /* ����ȡ�ĳ�ʼֵ���õ�����ջָ��Ĵ��� */

    /* ȫ�������ж� */
    cpsie i               /* ����IRQ�ж�(���PRIMASK) */
    cpsie f               /* ����FIQ�ж�(���FAULTMASK) */
    dsb                   /* ����ͬ�����ϣ�ȷ�������ڴ������� */
    isb                   /* ָ��ͬ�����ϣ�ȷ������ָ��ִ����� */

    /* ����SVCָ��������һ������ */
    svc 0                 /* ����SVC�쳣������0��ʾ������һ������ */
    nop                   /* �ղ��������ڶ����ռλ */
    nop                   /* �ղ��������ڶ����ռλ */
}


/*******************************************************************************
�������ƣ�prvEnableVFP
��������������ARM Cortex-M�������ĸ��㵥Ԫ(FPU)��ͨ������CPACR�Ĵ�����CP10��CP11λ��
          ��������ִ�и�������ָ��˺���ͨ����ϵͳ��ʼ��ʱ����һ�Ρ�
�����������
�����������
�� �� ֵ����
����˵����1.�޸�Э���������ʿ��ƼĴ���(CPACR)ʹ�ܸ��㵥Ԫ
          2.������Cortex-M4/M7�ȴ���FPU�Ĵ�����
          3.ʹ��ldr.wָ��ȷ��32λ��ַ����

�޸�����      �汾��          �޸���            �޸�����
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          ����
*******************************************************************************/

__asm void prvEnableVFP( void )
{
    PRESERVE8          /* ָ��8�ֽ�ջ���� */

    /* FPUʹ��λλ��CPACR�Ĵ����� */
    ldr.w r0, =0xE000ED88   /* ʹ�ÿ�ָ�����CPACR�Ĵ�����ַ��r0 */
    ldr    r1, [r0]         /* ��ȡCPACR�Ĵ����ĵ�ǰֵ��r1 */

    /* ����CP10��CP11Э��������Ȼ�󱣴�ؼĴ��� */
    orr    r1, r1, #( 0xf << 20 )  /* ����CP10��CP11�ֶ�Ϊȫ����Ȩ��(0b11) */
    str r1, [r0]            /* ���޸ĺ��ֵд��CPACR�Ĵ��� */
    bx    r14               /* ���ص�������(ʹ�����ӼĴ���r14) */
    nop                     /* �ղ��������ڶ����ռλ */
}
/*******************************************************************************
�������ƣ�xPortStartScheduler
��������������FreeRTOS������������ϵͳ��ʼ�����ã������ж����ȼ����á�ϵͳ���Ķ�ʱ����ʼ����
          ���㵥Ԫ���ã���������һ����������FreeRTOS�����ĺ��ĺ�����
�����������
�����������
�� �� ֵ��BaseType_t���ͣ�������Ӧ����0(����������²��᷵��)
����˵����1.����������ö��Լ�飬ȷ��ϵͳ������ȷ
          2.����PendSV��SysTick�ж�Ϊ������ȼ�
          3.��ʼ��ϵͳ���Ķ�ʱ��
          4.���ø��㵥Ԫ�����ö��Զ�ջ����
          5.����ͨ��prvStartFirstTask������һ������

�޸�����      �汾��          �޸���            �޸�����
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          ����
*******************************************************************************/

BaseType_t xPortStartScheduler( void )
{
    /* configMAX_SYSCALL_INTERRUPT_PRIORITY��������Ϊ0��
    �μ�http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html */
    configASSERT( configMAX_SYSCALL_INTERRUPT_PRIORITY );

    /* �˶˿ڿ����ڳ�r0p1�汾֮�������Cortex-M7�ںˡ�
    r0p1����Ӧʹ��/source/portable/GCC/ARM_CM7/r0p1Ŀ¼�еĶ˿ڡ� */
    configASSERT( portCPUID != portCORTEX_M7_r0p1_ID );
    configASSERT( portCPUID != portCORTEX_M7_r0p0_ID );

    #if( configASSERT_DEFINED == 1 )
    {
        volatile uint32_t ulOriginalPriority;
        volatile uint8_t * const pucFirstUserPriorityRegister = ( uint8_t * ) ( portNVIC_IP_REGISTERS_OFFSET_16 + portFIRST_USER_INTERRUPT_NUMBER );
        volatile uint8_t ucMaxPriorityValue;

        /* ȷ�����Դ��е���FreeRTOS API��ȫ�жϺ���(��"FromISR"��β�ĺ���)��������ȼ���
        FreeRTOSά���������̺߳�ISR API��������ȷ���ж���ھ����ܿ��ٺͼ򵥡�

        ���漴�����޸ĵ��ж����ȼ�ֵ�� */
        ulOriginalPriority = *pucFirstUserPriorityRegister;

        /* ȷ�����õ����ȼ�λ�������������п��ܵ�λд��ֵ�� */
        *pucFirstUserPriorityRegister = portMAX_8_BIT_VALUE;

        /* ����ֵ�Բ鿴�ж���λ�������� */
        ucMaxPriorityValue = *pucFirstUserPriorityRegister;

        /* �ں��ж����ȼ�Ӧ����Ϊ������ȼ��� */
        configASSERT( ucMaxPriorityValue == ( configKERNEL_INTERRUPT_PRIORITY & ucMaxPriorityValue ) );

        /* �����ϵͳ�������ȼ�ʹ����ͬ�����롣 */
        ucMaxSysCallPriority = configMAX_SYSCALL_INTERRUPT_PRIORITY & ucMaxPriorityValue;

        /* ���ݶ��ص�λ���������ɽ��ܵ����ȼ���ֵ�� */
        ulMaxPRIGROUPValue = portMAX_PRIGROUP_BITS;
        while( ( ucMaxPriorityValue & portTOP_BIT_OF_BYTE ) == portTOP_BIT_OF_BYTE )
        {
            ulMaxPRIGROUPValue--;
            ucMaxPriorityValue <<= ( uint8_t ) 0x01;
        }

        /* �����ȼ���ֵ�ƻ�����AIRCR�Ĵ����е�λ�á� */
        ulMaxPRIGROUPValue <<= portPRIGROUP_SHIFT;
        ulMaxPRIGROUPValue &= portPRIORITY_GROUP_MASK;

        /* ���޸Ĺ����ж����ȼ��Ĵ����ָ�Ϊ��ԭʼֵ�� */
        *pucFirstUserPriorityRegister = ulOriginalPriority;
    }
    #endif /* conifgASSERT_DEFINED */

    /* ��PendSV��SysTick����Ϊ������ȼ��жϡ� */
    portNVIC_SYSPRI2_REG |= portNVIC_PENDSV_PRI;
    portNVIC_SYSPRI2_REG |= portNVIC_SYSTICK_PRI;

    /* �������ɵδ�ʱ���жϵĶ�ʱ�����˴��ж��ѱ����á� */
    vPortSetupTimerInterrupt();

    /* ��ʼ���ؼ�Ƕ�׼�����׼����һ������ */
    uxCriticalNesting = 0;

    /* ȷ��VFP������ - ��Ӧ�����Ѿ����õġ� */
    prvEnableVFP();

    /* ʼ�����ö��Ա��档 */
    *( portFPCCR ) |= portASPEN_AND_LSPEN_BITS;

    /* ������һ������ */
    prvStartFirstTask();

    /* ��Ӧִ�е���� */
    return 0;
}


/*******************************************************************************
�������ƣ�vPortEndScheduler
����������ֹͣFreeRTOS���������ڴ����Ƕ��ʽ�˿���δʵ�ִ˹��ܣ���Ϊһ��������������
          ͨ��û�пɷ��صĻ������˺�����Ҫ���ڵ��Ի����������ֹͣ���������С�
�����������
�����������
�� �� ֵ����
����˵����1.�ڴ�����˿�ʵ���У��˺���ʵ����δ����ʵ��
          2.ͨ�����Լ��ȷ����������������µ���
          3.���������֧�ֵ��Ի�����Ӧ�ó���

�޸�����      �汾��          �޸���            �޸�����
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          ����
*******************************************************************************/

void vPortEndScheduler( void )
{
    /* ���޿ɷ��ػ����Ķ˿���δʵ�ִ˹��ܡ�
    �˹�ǿ�ƴ������ԡ� */
    configASSERT( uxCriticalNesting == 1000UL );  /* ���ؼ�Ƕ�׼����Ƿ�Ϊ1000����������򴥷����Դ��� */
}


/*******************************************************************************
�������ƣ�vPortEnterCritical
���������������ٽ����������жϲ������ٽ���Ƕ�׼�����ȷ���������ִ��ʱ���ᱻ�жϴ�ϣ�
          ���ڱ���������Դ��ԭ�Ӳ��������жϰ�ȫ�汾���������жϷ��������е��á�
�����������
�����������
�� �� ֵ����
����˵����1.�˺��������ȫ���ж�
          2.ά���ٽ���Ƕ�׼�����uxCriticalNesting
          3.ͨ�����Լ���ֹ���ж��������д������
          4.ֻ����"FromISR"��β��API�����������ж���ʹ��

�޸�����      �汾��          �޸���            �޸�����
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          ����
*******************************************************************************/

void vPortEnterCritical( void )
{
    portDISABLE_INTERRUPTS();  /* ���������жϣ���ֹ�ٽ������뱻��� */
    uxCriticalNesting++;       /* �����ٽ���Ƕ�׼���������¼Ƕ����� */

    /* �ⲻ�ǽ����ٽ����������жϰ�ȫ�汾�����������ж��������е��ã��򴥷�����()��
    ֻ����"FromISR"��β��API�����������ж���ʹ�á������ٽ�Ƕ�׼���Ϊ1ʱ���ж��ԣ�
    �Է�ֹ������Ժ���Ҳʹ���ٽ���ʱ�ĵݹ���á� */
    if( uxCriticalNesting == 1 )  /* ֻ�ڵ�һ�ν����ٽ���ʱ��飬����ݹ�������� */
    {
        /* ��鵱ǰ�Ƿ����ж�������(portVECTACTIVE_MASK�����ʾ�����ж���) */
        configASSERT( ( portNVIC_INT_CTRL_REG & portVECTACTIVE_MASK ) == 0 );
    }
}

/*******************************************************************************
�������ƣ�vPortExitCritical
�����������˳��ٽ��������������ٽ���Ƕ�׼�������Ƕ�׼���Ϊ��ʱ���������жϡ�
          �˺���������vPortEnterCritical���ʹ�ã����ڽ����ܱ����Ĵ���Ρ�
�����������
�����������
�� �� ֵ����
����˵����1.ʹ�ö���ȷ���������δ��Ե��˳�����
          2.ֻ������Ƕ���ٽ������˳�������������ж�
          3.��ѭ"����ȳ�"(LIFO)��Ƕ��ԭ��

�޸�����      �汾��          �޸���            �޸�����
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          ����
*******************************************************************************/
void vPortExitCritical( void )
{
	configASSERT( uxCriticalNesting );  /* ���Լ�飺ȷ���ٽ���Ƕ�׼�����Ϊ�㣬��ֹδ��Ե��˳����� */
	uxCriticalNesting--;                /* �����ٽ���Ƕ�׼���������ʾ�˳�һ���ٽ��� */
	if( uxCriticalNesting == 0 )        /* ����Ƿ����˳�����Ƕ�׵��ٽ��� */
	{
		portENABLE_INTERRUPTS();        /* ���������жϣ��ָ�ϵͳ�������ж���Ӧ */
	}
}


/*******************************************************************************
�������ƣ�xPortPendSVHandler
����������PendSV(�ɹ����ϵͳ����)�жϴ�����򣬸���FreeRTOS�������������л���
          ���浱ǰ���������ģ����������л�������Ȼ��ָ���һ������������ġ�
����������ޣ���Ӳ���Զ�������
�����������
�� �� ֵ����
����˵����1.ʹ�ý���ջָ��(PSP)�������������ı���ͻָ�
          2.֧�ָ��㵥Ԫ(FPU)�����ĵĸ�Ч����ͻָ�
          3.ͨ��basepri�Ĵ���ʵ���ٽ�������
          4.����XMC4000ϵ���ض� errata �Ĺ���around

�޸�����      �汾��          �޸���            �޸�����
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          ����
*******************************************************************************/
__asm void xPortPendSVHandler( void )
{
    /* �����ⲿ�����ͺ��� */
    extern uxCriticalNesting;        /* �ٽ���Ƕ�׼����� */
    extern pxCurrentTCB;             /* ��ǰ������ƿ�ָ�� */
    extern vTaskSwitchContext;       /* �����������л����� */

    PRESERVE8                        /* ָ��8�ֽ�ջ���� */

    mrs r0, psp                      /* ������ջָ��(PSP)��ֵ��ȡ��r0�Ĵ��� */
    isb                              /* ָ��ͬ�����ϣ�ȷ��PSP��ȡ��� */

    /* ��ȡ��ǰ������ƿ�(TCB)��λ�� */
    ldr    r3, =pxCurrentTCB         /* ����pxCurrentTCB�ĵ�ַ��r3 */
    ldr    r2, [r3]                  /* ��ȡ��ǰTCB��ָ�뵽r2 */

    /* ��������Ƿ�ʹ��FPU�����ģ�����ǣ����͸�VFP�Ĵ����� */
    tst r14, #0x10                   /* ����EXC_RETURNֵ�ĵ�4λ(0x10) */
    it eq                            /* ������(ʹ��FPU)��ִ����һ��ָ�� */
    vstmdbeq r0!, {s16-s31}          /* �Եݼ���ʽ��FPU�Ĵ���s16-s31���浽�����ջ */

    /* ������ļĴ����� */
    stmdb r0!, {r4-r11, r14}         /* �Եݼ���ʽ���Ĵ���r4-r11��r14���浽�����ջ */

    /* ���µ�ջ�����浽TCB�ĵ�һ����Ա�С� */
    str r0, [r2]                     /* �����º�Ķ�ջָ�뱣�浽TCB�ĵ�һ���ֶ� */

    stmdb sp!, {r3}                  /* ��r3(pxCurrentTCB��ַ)���浽����ջ */
    mov r0, #configMAX_SYSCALL_INTERRUPT_PRIORITY  /* �������ϵͳ�����ж����ȼ� */
    msr basepri, r0                  /* ����basepri�Ĵ��������ε��ڴ����ȼ����ж� */
    dsb                              /* ����ͬ������ */
    isb                              /* ָ��ͬ������ */
    bl vTaskSwitchContext            /* ���������������л����� */
    mov r0, #0                       /* ��0����r0 */
    msr basepri, r0                  /* ���basepri�Ĵ���(���������ж�) */
    ldmia sp!, {r3}                  /* ������ջ�ָ�r3(pxCurrentTCB��ַ) */

    /* pxCurrentTCB�еĵ�һ���������ջ���� */
    ldr r1, [r3]                     /* ��ȡ�µĵ�ǰTCBָ�� */
    ldr r0, [r1]                     /* ��TCB��ȡ�������ջ��ָ�� */

    /* �������ļĴ����� */
    ldmia r0!, {r4-r11, r14}         /* ���������ջ�ָ��Ĵ���r4-r11��r14 */

    /* ��������Ƿ�ʹ��FPU�����ģ�����ǣ�������VFP�Ĵ����� */
    tst r14, #0x10                   /* �ٴβ���EXC_RETURNֵ�ĵ�4λ */
    it eq                            /* ������(ʹ��FPU)��ִ����һ��ָ�� */
    vldmiaeq r0!, {s16-s31}          /* �������ջ�ָ�FPU�Ĵ���s16-s31 */

    msr psp, r0                      /* �����º�Ķ�ջָ�����û�PSP */
    isb                              /* ָ��ͬ������ */

    #ifdef WORKAROUND_PMU_CM001 /* XMC4000�ض� errata ���� */
        #if WORKAROUND_PMU_CM001 == 1
            push { r14 }             /* ��r14ѹ���ջ */
            pop { pc }               /* ������PC��ʵ�ַ��� */
            nop                      /* �ղ��� */
        #endif
    #endif

    bx r14                           /* ʹ��EXC_RETURNֵ���ص�����ģʽ */
}


/*******************************************************************************
�������ƣ�xPortSysTickHandler
����������SysTickϵͳ���Ķ�ʱ���жϷ����������FreeRTOS���������Ĵ���
          ����ϵͳ���ļ�������������Ҫʱ���������������л���
����������ޣ���Ӳ���Զ�������
�����������
�� �� ֵ����
����˵����1.SysTick����������ж����ȼ���ִ��ʱ�����ж���ȡ������
          2.ʹ��vPortRaiseBASEPRI()���portSET_INTERRUPT_MASK_FROM_ISR()�����Ч��
          3.ͨ������PendSV�ж��������������л�
          4.��ѭFreeRTOS�жϷ����������ʵ��

�޸�����      �汾��          �޸���            �޸�����
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          ����
*******************************************************************************/
void xPortSysTickHandler( void )
{
    /* SysTick����������ж����ȼ�����˵����ж�ִ��ʱ�����жϱ�����ȡ�����Ρ�
    ��˲���Ҫ����Ȼ��ָ��ж�����ֵ����Ϊ����ֵ�Ѿ�����֪�� - ���ʹ���Կ��
    vPortRaiseBASEPRI()�������portSET_INTERRUPT_MASK_FROM_ISR()�� */
    vPortRaiseBASEPRI();  /* ��ʱ�����ж����ȼ��������ٽ������� */
    {
        /* ����RTOS���ļ������� */
        if( xTaskIncrementTick() != pdFALSE )  /* ����ϵͳ���Ĳ�����Ƿ���Ҫ�������л� */
        {
            /* ��Ҫ�������л����������л���PendSV�ж���ִ�С�
            ����PendSV�жϡ� */
            portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;  /* ����PendSV����λ�������������л� */
        }
    }
    vPortClearBASEPRIFromISR();  /* ����ж����ȼ����룬�˳��ٽ��� */
}

/*******************************************************************************
�������ƣ�vPortSuppressTicksAndSleep
����������FreeRTOS�޿��н���(tickless idle)ģʽʵ�ֺ�����������ϵͳ����ʱ����͹���״̬��
          ͨ����̬����SysTick��ʱ�����ӳ�˯��ʱ�䣬���ٲ���Ҫ�Ķ�ʱ���жϣ��Ӷ����͹��ġ�
���������xExpectedIdleTime - Ԥ�ڵĿ���ʱ�䣨��ϵͳ������Ϊ��λ��
�����������
�� �� ֵ����
����˵����1.�˺���Ϊ������(__weak)�������û��Զ���ʵ��
          2.����configUSE_TICKLESS_IDLEΪ1ʱ����
          3.ͨ����ȷ����SysTick����ֵʵ�ֳ�ʱ��˯��
          4.֧��˯��ǰ��˯�ߺ���û��Զ��崦��

�޸�����      �汾��          �޸���            �޸�����
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          ����
*******************************************************************************/
#if configUSE_TICKLESS_IDLE == 1

    __weak void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime )
    {
        uint32_t ulReloadValue, ulCompleteTickPeriods, ulCompletedSysTickDecrements, ulSysTickCTRL;
        TickType_t xModifiableIdleTime;

        /* ȷ��SysTick����ֵ����ʹ����������� */
        if( xExpectedIdleTime > xMaximumPossibleSuppressedTicks )
        {
            xExpectedIdleTime = xMaximumPossibleSuppressedTicks;  /* �����������ƵĽ����� */
        }

        /* ��ʱֹͣSysTick��SysTickֹͣ��ʱ��ᾡ���ܱ����ǽ�ȥ����ʹ���޿��н���ģʽ
        �����ɱ���ص����ں�ά����ʱ��������ʱ��֮�����΢СƯ�ơ� */
        portNVIC_SYSTICK_CTRL_REG &= ~portNVIC_SYSTICK_ENABLE_BIT;  /* ����SysTick��ʱ�� */

        /* ����ȴ�xExpectedIdleTime������������������¼���ֵ��
        ʹ��-1����Ϊ�˴��뽫������һ���������ڵ�һ����ʱ����ִ�С� */
        ulReloadValue = portNVIC_SYSTICK_CURRENT_VALUE_REG + ( ulTimerCountsForOneTick * ( xExpectedIdleTime - 1UL ) );  /* ��������ֵ */
        if( ulReloadValue > ulStoppedTimerCompensation )  /* �������ֵ����ֹͣ��ʱ������ֵ */
        {
            ulReloadValue -= ulStoppedTimerCompensation;  /* ��ȥ����ֵ */
        }

        /* �����ٽ���������ʹ��taskENTER_CRITICAL()��������Ϊ�÷���������Ӧ�˳�˯��ģʽ���жϡ� */
        __disable_irq();  /* �����ж� */
        __dsb( portSY_FULL_READ_WRITE );  /* ����ͬ������ */
        __isb( portSY_FULL_READ_WRITE );  /* ָ��ͬ������ */

        /* ����������л�������������ڵȴ����ȳ���ָ���������͹��Ľ��롣 */
        if( eTaskConfirmSleepModeStatus() == eAbortSleep )  /* ����Ƿ�Ӧ��ֹ˯�� */
        {
            /* �Ӽ����Ĵ�����ʣ�������ֵ��������������ɴ˽������ڡ� */
            portNVIC_SYSTICK_LOAD_REG = portNVIC_SYSTICK_CURRENT_VALUE_REG;  /* ��������ֵΪ��ǰֵ */

            /* ��������SysTick�� */
            portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;  /* ����SysTick��ʱ�� */

            /* �����ؼĴ�������Ϊ�����������������ֵ�� */
            portNVIC_SYSTICK_LOAD_REG = ulTimerCountsForOneTick - 1UL;  /* �����������ĵ�����ֵ */

            /* ���������ж� - �μ�����__disable_irq()���õ�ע�͡� */
            __enable_irq();  /* �����ж� */
        }
        else
        {
            /* �����µ�����ֵ�� */
            portNVIC_SYSTICK_LOAD_REG = ulReloadValue;  /* ���ü���õ�������ֵ */

            /* ���SysTick������־��������ֵ����Ϊ�㡣 */
            portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;  /* ���õ�ǰ����ֵ */

            /* ��������SysTick�� */
            portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;  /* ����SysTick��ʱ�� */

            /* ˯��ֱ������ĳЩ���顣configPRE_SLEEP_PROCESSING()���Խ����������Ϊ0��
            ��ʾ��ʵ�ְ����Լ��ĵȴ��жϻ�ȴ��¼�ָ���˲�Ӧ�ٴ�ִ��wfi��
            ���ǣ����뱣��ԭʼ��Ԥ�ڿ���ʱ��������䣬�����Ҫ��ȡ������ */
            xModifiableIdleTime = xExpectedIdleTime;  /* �������޸ĵĸ��� */
            configPRE_SLEEP_PROCESSING( xModifiableIdleTime );  /* ����˯��ǰ�����Ӻ��� */
            if( xModifiableIdleTime > 0 )  /* �������Ҫ˯�� */
            {
                __dsb( portSY_FULL_READ_WRITE );  /* ����ͬ������ */
                __wfi();  /* �ȴ��ж�ָ�����͹���״̬ */
                __isb( portSY_FULL_READ_WRITE );  /* ָ��ͬ������ */
            }
            configPOST_SLEEP_PROCESSING( xExpectedIdleTime );  /* ����˯�ߺ����Ӻ��� */

            /* �ٴ�ֹͣSysTick��SysTickֹͣ��ʱ��ᾡ���ܱ����ǽ�ȥ����ʹ���޿��н���ģʽ
            �����ɱ���ص����ں�ά����ʱ��������ʱ��֮�����΢СƯ�ơ� */
            ulSysTickCTRL = portNVIC_SYSTICK_CTRL_REG;  /* ������ƼĴ���ֵ */
            portNVIC_SYSTICK_CTRL_REG = ( ulSysTickCTRL & ~portNVIC_SYSTICK_ENABLE_BIT );  /* ����SysTick��ʱ�� */

            /* ���������ж� - �μ�����__disable_irq()���õ�ע�͡� */
            __enable_irq();  /* �����ж� */

            if( ( ulSysTickCTRL & portNVIC_SYSTICK_COUNT_FLAG_BIT ) != 0 )  /* ��������־�Ƿ���λ */
            {
                uint32_t ulCalculatedLoadValue;

                /* �����ж��Ѿ�ִ�У�����SysTick������ʹ��ulReloadValue���¼��ء�
                ʹ�ô˽�������ʣ����κ�ֵ����portNVIC_SYSTICK_LOAD_REG�� */
                ulCalculatedLoadValue = ( ulTimerCountsForOneTick - 1UL ) - ( ulReloadValue - portNVIC_SYSTICK_CURRENT_VALUE_REG );  /* ����ʣ�������ֵ */

                /* ������ʹ��΢Сֵ����������˯�ߺ��Ӻ���ִ����ĳЩ��ʱ���������������ֵ�� */
                if( ( ulCalculatedLoadValue < ulStoppedTimerCompensation ) || ( ulCalculatedLoadValue > ulTimerCountsForOneTick ) )  /* ������ֵ�Ƿ�����Ч��Χ�� */
                {
                    ulCalculatedLoadValue = ( ulTimerCountsForOneTick - 1UL );  /* ʹ��Ĭ��ֵ */
                }

                portNVIC_SYSTICK_LOAD_REG = ulCalculatedLoadValue;  /* ���ü���õ�������ֵ */

                /* �����жϴ�������Ѿ����ں��й����˽��Ĵ������ڹ���Ľ��Ľ���
                �˺����˳�������������˽���ά���Ľ���ֵ����ǰ�����ȴ�ʱ���һ�� */
                ulCompleteTickPeriods = xExpectedIdleTime - 1UL;  /* ���������Ľ��������� */
            }
            else
            {
                /* �����ж�����������¼�������˯�ߡ�
                ����˯�߳���ʱ�䣬�������뵽�����Ľ������ڣ����ǿ��ǲ��ֽ��ĵ�ulReloadֵ���� */
                ulCompletedSysTickDecrements = ( xExpectedIdleTime * ulTimerCountsForOneTick ) - portNVIC_SYSTICK_CURRENT_VALUE_REG;  /* ��������ɵ�SysTick�ݼ����� */

                /* �������ȴ�ʱ�����˶��������Ľ������ڣ� */
                ulCompleteTickPeriods = ulCompletedSysTickDecrements / ulTimerCountsForOneTick;  /* ���������Ľ��������� */

                /* ����ֵ����Ϊʣ��ĵ��������ڵķ������֡� */
                portNVIC_SYSTICK_LOAD_REG = ( ( ulCompleteTickPeriods + 1UL ) * ulTimerCountsForOneTick ) - ulCompletedSysTickDecrements;  /* ���㲢��������ֵ */
            }

            /* ��������SysTick��ʹ���portNVIC_SYSTICK_LOAD_REG�ٴ����У�
            Ȼ��portNVIC_SYSTICK_LOAD_REG���û����׼ֵ��
            ʹ���ٽ���ȷ�������ؼĴ����ӽ��������£������ж�ֻ��ִ��һ�Ρ� */
            portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;  /* ���õ�ǰ����ֵ */
            portENTER_CRITICAL();  /* �����ٽ��� */
            {
                portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;  /* ����SysTick��ʱ�� */
                vTaskStepTick( ulCompleteTickPeriods );  /* ����ϵͳ���ļ����� */
                portNVIC_SYSTICK_LOAD_REG = ulTimerCountsForOneTick - 1UL;  /* �ָ�����������ֵ */
            }
            portEXIT_CRITICAL();  /* �˳��ٽ��� */
        }
    }

#endif /* #if configUSE_TICKLESS_IDLE */

/*******************************************************************************
�������ƣ�vPortSetupTimerInterrupt
��������������SysTick��ʱ���жϣ�����ϵͳ����Ƶ�ʺ���ز�����
          �������ü��㶨ʱ������ֵ������ʼ��SysTick���ƼĴ�����
�����������
�����������
�� �� ֵ����
����˵����1.����configOVERRIDE_DEFAULT_TICK_CONFIGURATIONΪ0ʱ����
          2.֧���޿��н���ģʽ(configUSE_TICKLESS_IDLE)����ؼ���
          3.����SysTick��ʱ����ָ����Ƶ�ʲ����ж�

�޸�����      �汾��          �޸���            �޸�����
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          ����
*******************************************************************************/
#if configOVERRIDE_DEFAULT_TICK_CONFIGURATION == 0

    void vPortSetupTimerInterrupt( void )
    {
        /* �������ý����ж�����ĳ����� */
        #if configUSE_TICKLESS_IDLE == 1  /* ��������޿��н���ģʽ */
        {
            ulTimerCountsForOneTick = ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ );  /* ����һ����������Ķ�ʱ������ */
            xMaximumPossibleSuppressedTicks = portMAX_24_BIT_NUMBER / ulTimerCountsForOneTick;  /* �����������ƵĽ����� */
            ulStoppedTimerCompensation = portMISSED_COUNTS_FACTOR / ( configCPU_CLOCK_HZ / configSYSTICK_CLOCK_HZ );  /* ����ֹͣ��ʱ������ֵ */
        }
        #endif /* configUSE_TICKLESS_IDLE */

        /* ����SysTick������������жϡ� */
        portNVIC_SYSTICK_LOAD_REG = ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ ) - 1UL;  /* ����SysTick����ֵ */
        portNVIC_SYSTICK_CTRL_REG = ( portNVIC_SYSTICK_CLK_BIT | portNVIC_SYSTICK_INT_BIT | portNVIC_SYSTICK_ENABLE_BIT );  /* ���ò�����SysTick */
    }

#endif /* configOVERRIDE_DEFAULT_TICK_CONFIGURATION */

/*******************************************************************************
�������ƣ�vPortGetIPSR
������������ȡ��ǰIPSR(�жϳ���״̬�Ĵ���)��ֵ������ȷ����ǰ����ִ�е��жϻ��쳣��š�
          �ú���ͨ�����ָ��ֱ�Ӷ�ȡIPSR�Ĵ�����ֵ�����ء�
�����������
�����������
�� �� ֵ��uint32_t���ͣ����ص�ǰIPSR�Ĵ�����ֵ���������ڴ�����жϻ��쳣���
����˵����1.ʹ��MRSָ��ֱ�Ӷ�ȡIPSR�Ĵ���
          2.��ѭARM AAPCS����Լ��������ֵͨ��r0�Ĵ�������
          3.ʹ��PRESERVE8��֤8�ֽ�ջ����

�޸�����      �汾��          �޸���            �޸�����
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          ����
*******************************************************************************/
__asm uint32_t vPortGetIPSR( void )
{
    PRESERVE8          /* ָ��8�ֽ�ջ���룬����ARM EABI��׼ */

    mrs r0, ipsr       /* ���жϳ���״̬�Ĵ���(IPSR)��ֵ�ƶ���r0�Ĵ��� */
    bx r14             /* ʹ�����ӼĴ���r14(��lr)���ص������ߣ�����ֵ��r0�� */
}

/*******************************************************************************
�������ƣ�vPortValidateInterruptPriority
������������֤�ж����ȼ������Ƿ���ȷ��ȷ��ʹ��FreeRTOS API���жϾ��к��ʵ����ȼ���
          ��鵱ǰ�жϵ����ȼ��Ƿ���ڻ����configMAX_SYSCALL_INTERRUPT_PRIORITY��
          ����֤���ȼ����������Ƿ���ȷ��
�����������
�����������
�� �� ֵ����
����˵����1.����configASSERT_DEFINEDΪ1ʱ����(����ģʽ��)
          2.ʹ�ö��Լ���ж����ȼ����õ���ȷ��
          3.ȷ��FreeRTOS API���жϰ�ȫ������ȷ����

�޸�����      �汾��          �޸���            �޸�����
-----------------------------------------------------------------------------
2025/09/01     V1.00          Qiguo_Cui          ����
*******************************************************************************/
#if( configASSERT_DEFINED == 1 )

    void vPortValidateInterruptPriority( void )
    {
        uint32_t ulCurrentInterrupt;
        uint8_t ucCurrentPriority;

        /* ��ȡ��ǰ����ִ�е��жϱ�š� */
        ulCurrentInterrupt = vPortGetIPSR();  /* ����vPortGetIPSR������ȡ��ǰ�жϺ� */

        /* �жϱ���Ƿ�Ϊ�û�������жϣ� */
        if( ulCurrentInterrupt >= portFIRST_USER_INTERRUPT_NUMBER )  /* ����Ƿ�Ϊ�û��ж�(���ں��쳣) */
        {
            /* �����жϵ����ȼ��� */
            ucCurrentPriority = pcInterruptPriorityRegisters[ ulCurrentInterrupt ];  /* ���ж����ȼ��Ĵ��������ȡ��ǰ���ȼ� */

            /* ���Ϊ�ѱ��������configMAX_SYSCALL_INTERRUPT_PRIORITY���ȼ����жϵķ�������(ISR)
            ������ISR��ȫ��FreeRTOS API���������¶��Խ�ʧ�ܡ�ISR��ȫ��FreeRTOS API��������*��*
            ���ѱ��������ȼ����ڻ����configMAX_SYSCALL_INTERRUPT_PRIORITY���ж��е��á�

            ��ֵ�͵��ж����ȼ���ű�ʾ�߼��ϵĸ��ж����ȼ�������жϵ����ȼ���������Ϊ���ڻ���ֵ��
            *����*configMAX_SYSCALL_INTERRUPT_PRIORITY��ֵ��

            ʹ��FreeRTOS API���жϲ��ܱ�����Ĭ�����ȼ��㣬��Ϊ������߿��ܵ����ȼ���
            ��֤����configMAX_SYSCALL_INTERRUPT_PRIORITY�����Ҳ��֤��Ч��

            FreeRTOSά���������̺߳�ISR API��������ȷ���ж���ھ����ܿ��ٺͼ򵥡�

            ���������ṩ��ϸ��Ϣ��
            http://www.freertos.org/RTOS-Cortex-M3-M4.html
            http://www.freertos.org/FAQHelp.html */
            configASSERT( ucCurrentPriority >= ucMaxSysCallPriority );  /* ���Լ�鵱ǰ�ж����ȼ��Ƿ����Ҫ�� */
        }

        /* ���ȼ����飺�жϿ�����(NVIC)��������ÿ���ж�����λ��λ���Ϊ�����жϵ���ռ���ȼ�λ
        �Ͷ����жϵ������ȼ���λ��Ϊ�����������λ���붨��Ϊ��ռ���ȼ�λ����������������
        (���ĳЩλ��ʾ�����ȼ�)�����¶��Խ�ʧ�ܡ�

        ���Ӧ�ó����ʹ��CMSIS������ж����ã������ͨ�����������ȳ���֮ǰ����
        NVIC_SetPriorityGrouping(0);������Cortex-M�豸��ʵ����ȷ���á�����ע�⣬
        ĳЩ��Ӧ���ض��������ٶ��������ȼ������ã�����������£�ʹ����ֵ�����²���Ԥ�����Ϊ�� */
        configASSERT( ( portAIRCR_REG & portPRIORITY_GROUP_MASK ) <= ulMaxPRIGROUPValue );  /* ���Լ�����ȼ��������� */
    }

#endif /* configASSERT_DEFINED */




#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "usart_demo.h"
#include "uart_dma_demo.h"
#include "led.h"
#include "system\flash\flash_demo.h"
#include "FreeRTOS.h"
#include "task.h"
/************************************************
 ALIENTEK ������STM32F429������ FreeRTOSʵ��2-1
 FreeRTOS��ֲʵ��-HAL��汾
 ����֧�֣�www.openedv.com
 �Ա����̣�http://eboard.taobao.com 
 ��ע΢�Ź���ƽ̨΢�źţ�"����ԭ��"����ѻ�ȡSTM32���ϡ�
 �������������ӿƼ����޹�˾  
 ���ߣ�����ԭ�� @ALIENTEK
************************************************/

//�������ȼ�
#define START_TASK_PRIO		1
//�����ջ��С	
#define START_STK_SIZE 		128  
//������
TaskHandle_t StartTask_Handler;
//������
void start_task(void *pvParameters);

//�������ȼ�
#define LED0_TASK_PRIO		2
//�����ջ��С	
#define LED0_STK_SIZE 		50  
//������
TaskHandle_t LED0Task_Handler;
//������
void led0_task(void *pvParameters);

//�������ȼ�
#define LED1_TASK_PRIO		3
//�����ջ��С	
#define LED1_STK_SIZE 		50  
//������
TaskHandle_t LED1Task_Handler;
//������
void led1_task(void *pvParameters);

//�������ȼ�
#define FLOAT_TASK_PRIO		3
//�����ջ��С
#define FLOAT_STK_SIZE 		64
//������
TaskHandle_t FLOATTask_Handler;
//������
void float_task(void *pvParameters);

//UART演示任务优先级
#define UART_DEMO_TASK_PRIO		4
//UART演示任务堆栈大小
#define UART_DEMO_STK_SIZE 		128
//UART演示任务句柄
TaskHandle_t UARTDemoTask_Handler;
//UART演示任务函数
void uart_demo_task(void *pvParameters);

//UART+DMA演示任务优先级
#define UART_DMA_DEMO_TASK_PRIO		5
//UART+DMA演示任务堆栈大小
#define UART_DMA_DEMO_STK_SIZE 		128
//UART+DMA演示任务句柄
TaskHandle_t UARTDmaDemoTask_Handler;
//UART+DMA演示任务函数
void uart_dma_task(void *pvParameters);

//Flash演示任务优先级
#define FLASH_DEMO_TASK_PRIO		6
//Flash演示任务堆栈大小
#define FLASH_DEMO_STK_SIZE 		128
//Flash演示任务句柄
TaskHandle_t FlashDemoTask_Handler;
//Flash演示任务函数
void flash_demo_task(void *pvParameters);

//���ڴ�����ʧ�ܹ��Ӻ���
void vApplicationMallocFailedHook(void)
{
    printf("内存分配失败！\r\n");
    printf("剩余内存: %u 字节\r\n", xPortGetFreeHeapSize());
    printf("最小剩余内存: %u 字节\r\n", xPortGetMinimumEverFreeHeapSize());
    while(1);
}

int main(void)
{
    HAL_Init();                     //��ʼ��HAL��   
    Stm32_Clock_Init(360,25,2,8);   //����ʱ��,180Mhz
	delay_init(180);                //��ʼ����ʱ����
    LED_Init();                     //��ʼ��LED 
    uart_init(115200);              //��ʼ������
    //������ʼ����
    xTaskCreate((TaskFunction_t )start_task,            //������
                (const char*    )"start_task",          //��������
                (uint16_t       )START_STK_SIZE,        //�����ջ��С
                (void*          )NULL,                  //���ݸ��������Ĳ���
                (UBaseType_t    )START_TASK_PRIO,       //�������ȼ�
                (TaskHandle_t*  )&StartTask_Handler);   //������              
    vTaskStartScheduler();          //�����������
}

//��ʼ����������
void start_task(void *pvParameters)
{
    taskENTER_CRITICAL();           //�����ٽ���
    //����LED0����
    xTaskCreate((TaskFunction_t )led0_task,     	
                (const char*    )"led0_task",   	
                (uint16_t       )LED0_STK_SIZE, 
                (void*          )NULL,				
                (UBaseType_t    )LED0_TASK_PRIO,	
                (TaskHandle_t*  )&LED0Task_Handler);   
    //����LED1����
    xTaskCreate((TaskFunction_t )led1_task,     
                (const char*    )"led1_task",   
                (uint16_t       )LED1_STK_SIZE, 
                (void*          )NULL,	
                (UBaseType_t    )LED1_TASK_PRIO,	
                (TaskHandle_t*  )&LED1Task_Handler);        
    //�����������
    xTaskCreate((TaskFunction_t )float_task,     
                (const char*    )"float_task",   
                (uint16_t       )FLOAT_STK_SIZE, 
                (void*          )NULL,    
                (UBaseType_t    )FLOAT_TASK_PRIO,    
                (TaskHandle_t*  )&FLOATTask_Handler);  
                
    //创建UART演示任务
    xTaskCreate((TaskFunction_t )uart_demo_task,     
                (const char*    )"uart_demo_task",   
                (uint16_t       )UART_DEMO_STK_SIZE, 
                (void*          )NULL,    
                (UBaseType_t    )UART_DEMO_TASK_PRIO,    
                (TaskHandle_t*  )&UARTDemoTask_Handler);  

    //创建UART+DMA演示任务
    xTaskCreate((TaskFunction_t )uart_dma_task,     
                (const char*    )"uart_dma_task",   
                (uint16_t       )UART_DMA_DEMO_STK_SIZE, 
                (void*          )NULL,    
                (UBaseType_t    )UART_DMA_DEMO_TASK_PRIO,    
                (TaskHandle_t*  )&UARTDmaDemoTask_Handler);  

    //创建Flash演示任务
    xTaskCreate((TaskFunction_t )flash_demo_task,     
                (const char*    )"flash_demo_task",   
                (uint16_t       )FLASH_DEMO_STK_SIZE, 
                (void*          )NULL,    
                (UBaseType_t    )FLASH_DEMO_TASK_PRIO,    
                (TaskHandle_t*  )&FlashDemoTask_Handler);  
                
    vTaskDelete(NULL); //删除当前任务(启动任务)
    taskEXIT_CRITICAL();            //退出临界区
}

//LED0������ 
void led0_task(void *pvParameters)
{
    while(1)
    {
        LED0=~LED0;
        vTaskDelay(500);
    }
}   

//LED1������
void led1_task(void *pvParameters)
{
    while(1)
    {
        LED1=0;
        vTaskDelay(200);
        LED1=1;
        vTaskDelay(800);
    }
}

//�����������
void float_task(void *pvParameters)
{
	static float float_num=0.00;
	while(1)
	{
		float_num+=0.01f;
		printf("float_num��ֵΪ: %.4f\r\n",float_num);
        vTaskDelay(1000);
	}
}

//UART演示任务函数
void uart_demo_task(void *pvParameters)
{
    // 调用UART演示函数
    uart_demo();
}

//UART+DMA演示任务函数
void uart_dma_task(void *pvParameters)
{
    // 调用UART+DMA演示函数
    uart_dma_demo();
}

//Flash演示任务函数
void flash_demo_task(void *pvParameters)
{
    // 调用Flash演示函数
    Flash_Demo();
}




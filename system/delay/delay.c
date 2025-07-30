#include "delay.h"
#include "sys.h"
////////////////////////////////////////////////////////////////////////////////// 	 
//���ʹ��OS,����������ͷ�ļ�����.
#if SYSTEM_SUPPORT_OS
#include "FreeRTOS.h"					//FreeRTOSʹ��	 
#include "task.h"
#endif
//////////////////////////////////////////////////////////////////////////////////  
//������ֻ��ѧϰʹ�ã�δ���������ɣ��������������κ���;
//ALIENTEK STM32F429������
//ʹ��SysTick����ͨ����ģʽ���ӳٽ��й���(FreeRTOSר��)
//����delay_us,delay_ms
//����ԭ��@ALIENTEK
//������̳:www.openedv.com
//��������:2016/9/13
//�汾��V1.1
//��Ȩ���У�����ؾ���
//Copyright(C) �������������ӿƼ����޹�˾ 2014-2024
//All rights reserved
//********************************************************************************
//�޸�˵��
////////////////////////////////////////////////////////////////////////////////// 

static u32 fac_us=0;							//us��ʱ������

#if SYSTEM_SUPPORT_OS		
    static u16 fac_ms=0;				        //ms��ʱ������,��os��,����ÿ�����ĵ�ms��
#endif

 
extern void xPortSysTickHandler(void);
//systick�жϷ�����,ʹ��OSʱ�õ�
void SysTick_Handler(void)
{  
    if(xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED)//ϵͳ�Ѿ�����
    {
        xPortSysTickHandler();	
    }
    HAL_IncTick();
}
			   
//��ʼ���ӳٺ���
//��ʹ��ucos��ʱ��,�˺������ʼ��ucos��ʱ�ӽ���
//SYSTICK��ʱ�ӹ̶�ΪAHBʱ��
//SYSCLK:ϵͳʱ��Ƶ��
void delay_init(u8 SYSCLK)
{
	u32 reload;
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);//SysTickƵ��ΪHCLK
	fac_us=SYSCLK;						    //�����Ƿ�ʹ��OS,fac_us����Ҫʹ��
	reload=SYSCLK;					        //ÿ���ӵļ������� ��λΪK	   
	reload*=1000000/configTICK_RATE_HZ;		//����configTICK_RATE_HZ�趨���ʱ��
											//reloadΪ24λ�Ĵ���,���ֵ:16777216,��180M��,Լ��0.745s����	
	fac_ms=1000/configTICK_RATE_HZ;			//����OS������ʱ�����ٵ�λ		
    SysTick->CTRL|=SysTick_CTRL_TICKINT_Msk;//����SYSTICK�ж�
	SysTick->LOAD=reload - 1; 					//ÿ1/configTICK_RATE_HZ��һ��	
	SysTick->CTRL|=SysTick_CTRL_ENABLE_Msk; //����SYSTICK
}								    

//��ʱnus
//nus:Ҫ��ʱ��us��.	
//nus:0~190887435(���ֵ��2^32/fac_us@fac_us=22.5)	    								   
void delay_us(u32 nus)
{		
	if(nus < 10)
	{
		// ���ڷǳ���Ķ�ʱʹ�ñ�����
		for(volatile u32 i=0; i<nus*fac_us/8; i++);
	}
	else
	{
		u32 ticks;
		u32 told,tnow,tcnt=0;
		u32 reload=SysTick->LOAD;			//LOAD��ֵ	    	 
		ticks=nus*fac_us*10/8; 						//��Ҫ�Ľ����� 
		told=SysTick->VAL;        				//�ս���ʱ�ļ�����ֵ
		while(1)
		{
			tnow=SysTick->VAL;	
			if(tnow!=told)
			{	    
				if(tnow<told)tcnt+=told-tnow;	//����ע��һ��SYSTICK��һ���ݼ��ļ������Ϳ�����.
				else tcnt+=reload-tnow+told;	    
				told=tnow;
				if(tcnt>=ticks)break;		//ʱ�䳬��/����Ҫ�ӳٵ�ʱ��,���˳�.
			}	  
		};				    
	}
}  
	
//��ʱnms,�������������
//nms:Ҫ��ʱ��ms��
//nms:0~65535
void delay_ms(u32 nms)
{	
	if(xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED)//ϵͳ�Ѿ�����
	{		
		if(nms>=fac_ms)						//��ʱ��ʱ�����OS������ʱ������ 
		{ 
   			vTaskDelay(nms/fac_ms);	 		//FreeRTOS��ʱ
		}
		nms%=fac_ms;						//OS�Ѿ��޷��ṩ��ôС����ʱ��,������ͨ��ʽ��ʱ    
	}
	delay_us((u32)(nms*1000));				//��ͨ��ʽ��ʱ
}

//��ʱnms,���������������
//nms:Ҫ��ʱ��ms��
void delay_xms(u32 nms)
{
	u32 i;
	for(i=0;i<nms;i++) delay_us(1000);
}


			 




































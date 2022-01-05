#include "main.h"

#define COLUMN(x) ((x) * (((sFONT *)BSP_LCD_GetFont())->Width))    //see font.h, for defining LINE(X)

#define Step_Number 48
#define default_Period 64	///From my student number 400231297 -> 97 - 33 = 64 sec/cycle
#define f_Counter 10000				///Required Counter frequency
#define SYSCLK 180000000			///System Clock frequency
 
#define full_Stepping 1
#define half_Stepping 0
#define Clockwise  1
#define CounterClockwise 0
#define ON 1
#define OFF 0

uint8_t pin_State_0[4]={1,0,1,0};
uint8_t pin_State_1[4]={1,0,0,0};
uint8_t pin_State_2[4]={1,0,0,1};
uint8_t pin_State_3[4]={0,0,0,1};
uint8_t pin_State_4[4]={0,1,0,1};
uint8_t pin_State_5[4]={0,1,0,0};
uint8_t pin_State_6[4]={0,1,1,0};
uint8_t pin_State_7[4]={0,0,1,0};
uint8_t pin_State = 0;

/*	Initial settings to start with	*/
uint8_t speed_Setting_Mode = OFF;
uint8_t current_Stepping = full_Stepping;
uint8_t current_Rotation = Clockwise;
uint16_t rotation_Period = default_Period;	
uint8_t *current_Pin_State = pin_State_0;

uint16_t prescaler;
uint16_t period;

TIM_HandleTypeDef		Tim3_Handle;
TIM_OC_InitTypeDef	Tim3_OCInitStructure;

void LCD_DisplayString(uint16_t LineNumber, uint16_t ColumnNumber, uint8_t *ptr);
void LCD_DisplayInt(uint16_t LineNumber, uint16_t ColumnNumber, int Number);
void LCD_DisplayFloat(uint16_t LineNumber, uint16_t ColumnNumber, float Number, int DigitAfterDecimalPoint);

static void SystemClock_Config(void);
static void Error_Handler(void);

void TIM3_OC_Config(void);
void TIM3_Config(uint8_t stepping_Mode);
void TIM3_Re_Config(void);

void GPIO_Pins_Config(void);
void ExtBtn1_Config(void);
void ExtBtn2_Config(void);
void ExtBtn3_Config(void);

void switch_Stepping(void);
void switch_Direction(void);
void rotate_Clockwise(void);
void rotate_CounterClockwise(void);

void step_Full_CW(void);
void step_Half_CW(void);
void step_Full_CCW(void);
void step_Half_CCW(void);


int main(void){
	

		HAL_Init();
	
		 /* Configure the system clock to 180 MHz */
		SystemClock_Config();
		
		HAL_InitTick(0x0000); // set systick's priority to the highest.
	
		BSP_LCD_Init();
		BSP_LCD_LayerDefaultInit(0, LCD_FRAME_BUFFER);   //LCD_FRAME_BUFFER, defined as 0xD0000000 in _discovery_lcd.h
															// the LayerIndex may be 0 and 1. if is 2, then the LCD is dark.
		BSP_LCD_SelectLayer(0);
		BSP_LCD_Clear(LCD_COLOR_WHITE);  //need this line, otherwise, the screen is dark	
		BSP_LCD_DisplayOn();
	 
		BSP_LCD_SetFont(&Font20);  //the default font,  LCD_DEFAULT_FONT, which is defined in _lcd.h, is Font24
	
	
	
		HAL_TIM_OC_Init(&Tim3_Handle);
		
	
		BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_EXTI);
		GPIO_Pins_Config();
		ExtBtn1_Config();
		ExtBtn2_Config();
		ExtBtn3_Config();
		
		BSP_LED_Init(LED3);
		BSP_LED_Init(LED4);
		BSP_LED_Toggle(LED3);
		BSP_LED_Toggle(LED4);


		/*	Start with full stepping	*/
		TIM3_Config(full_Stepping);
		TIM3_OC_Config();

		LCD_DisplayString(5, 3, "Frequency:");
		LCD_DisplayString(8, 3, "Priod:");
		
		HAL_GPIO_WritePin(GPIOE,GPIO_PIN_2,current_Pin_State[0]);
		HAL_GPIO_WritePin(GPIOE,GPIO_PIN_3,current_Pin_State[1]);
		HAL_GPIO_WritePin(GPIOE,GPIO_PIN_4,current_Pin_State[2]);
		HAL_GPIO_WritePin(GPIOE,GPIO_PIN_5,current_Pin_State[3]);
		
		while(1) {	
			

			
			LCD_DisplayFloat(6, 5, 1.0 / (rotation_Period), 4);
			LCD_DisplayInt(9, 5, rotation_Period);

			
		} // end of while loop
	
}  //end of main


/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow : 
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 180000000
  *            HCLK(Hz)                       = 180000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 4
  *            APB2 Prescaler                 = 2
  *            HSE Frequency(Hz)              = 8000000
  *            PLL_M                          = 8
  *            PLL_N                          = 360
  *            PLL_P                          = 2
  *            PLL_Q                          = 7
  *            VDD(V)                         = 3.3
  *            Main regulator output voltage  = Scale1 mode
  *            Flash Latency(WS)              = 5
  * @param  None
  * @retval None
  */
static void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;

  /* Enable Power Control clock */
  __HAL_RCC_PWR_CLK_ENABLE();
  
  /* The voltage scaling allows optimizing the power consumption when the device is 
     clocked below the maximum system frequency, to update the voltage scaling value 
     regarding system frequency refer to product datasheet.  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  
  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 360;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /* Activate the Over-Drive mode */
  HAL_PWREx_EnableOverDrive();
 
  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;  
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;  
  if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}





void LCD_DisplayString(uint16_t LineNumber, uint16_t ColumnNumber, uint8_t *ptr)
{  
  //here the LineNumber and the ColumnNumber are NOT  pixel numbers!!!
		while (*ptr!=NULL)
    {
				BSP_LCD_DisplayChar(COLUMN(ColumnNumber),LINE(LineNumber), *ptr); //new version of this function need Xpos first. so COLUMN() is the first para.
				ColumnNumber++;
			 //to avoid wrapping on the same line and replacing chars 
				if ((ColumnNumber+1)*(((sFONT *)BSP_LCD_GetFont())->Width)>=BSP_LCD_GetXSize() ){
					ColumnNumber=0;
					LineNumber++;
				}
					
				ptr++;
		}
}

void LCD_DisplayInt(uint16_t LineNumber, uint16_t ColumnNumber, int Number)
{  
  //here the LineNumber and the ColumnNumber are NOT  pixel numbers!!!
		char lcd_buffer[15];
		sprintf(lcd_buffer,"%d",Number);
	
		LCD_DisplayString(LineNumber, ColumnNumber, (uint8_t *) lcd_buffer);
}

void LCD_DisplayFloat(uint16_t LineNumber, uint16_t ColumnNumber, float Number, int DigitAfterDecimalPoint)
{  
  //here the LineNumber and the ColumnNumber are NOT  pixel numbers!!!
		char lcd_buffer[15];
		
		sprintf(lcd_buffer,"%.*f",DigitAfterDecimalPoint, Number);  //6 digits after decimal point, this is also the default setting for Keil uVision 4.74 environment.
	
		LCD_DisplayString(LineNumber, ColumnNumber, (uint8_t *) lcd_buffer);
}



void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{

		if (GPIO_Pin == KEY_BUTTON_PIN) //GPIO_PIN_0
		{
			BSP_LED_Toggle(LED3);
			BSP_LED_Toggle(LED4);
		}
		
		
		
		if (GPIO_Pin == GPIO_PIN_1)
		{
			if (speed_Setting_Mode)
			{
				rotation_Period -= 4;		///Decrease period in order to increase frequancy, thus increase speed
				
				BSP_LCD_ClearStringLine(6);
				BSP_LCD_ClearStringLine(9);
			}
			else 
			{
				switch_Stepping();
				BSP_LED_Toggle(LED4);
			}
			
		}  //end of PIN_1
		
		

		if (GPIO_Pin == GPIO_PIN_2)
		{			
			if (speed_Setting_Mode)
			{
				rotation_Period += 4;		///Increase period in order to decrease frequancy, thus decrease speed
				
				BSP_LCD_ClearStringLine(6);
				BSP_LCD_ClearStringLine(9);
			
			}
			else 
			{
				switch_Direction();
				BSP_LED_Toggle(LED3);
			}
				
		} //end of if PIN_2	
		
		
		
		if(GPIO_Pin == GPIO_PIN_3)
		{
			BSP_LED_Toggle(LED3);
			BSP_LED_Toggle(LED4);
			
			if (speed_Setting_Mode)
			{
				speed_Setting_Mode = 0;
			}
			else speed_Setting_Mode = 1;
				
		} //end of if PIN_3
		
		TIM3_Re_Config();
}


void switch_Stepping()
{
	if (current_Stepping == full_Stepping)
	{
		current_Stepping = half_Stepping;
		TIM3_Config(half_Stepping);
	}
	else 
	{
		current_Stepping = full_Stepping;
		TIM3_Config(full_Stepping);
	}
}

void switch_Direction()
{
	if (current_Rotation == Clockwise)
	{
		current_Rotation = CounterClockwise;
		TIM3_Re_Config();
	}
	else 
	{
		current_Rotation = Clockwise;
		TIM3_Re_Config();
	}
	
}

void GPIO_Pins_Config()
{
	GPIO_InitTypeDef   GPIO_InitStructure;
	__HAL_RCC_GPIOE_CLK_ENABLE();
	
	GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure.Pull =GPIO_PULLDOWN;
	GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
	
  GPIO_InitStructure.Pin = GPIO_PIN_2;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStructure);

  GPIO_InitStructure.Pin = GPIO_PIN_3;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStructure);

  GPIO_InitStructure.Pin = GPIO_PIN_4;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStructure);

  GPIO_InitStructure.Pin = GPIO_PIN_5;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStructure);
	
}


void TIM3_Config(uint8_t stepping_Mode)		///Takes in the stepping mode decision
{
  period = (rotation_Period * f_Counter) / Step_Number;		///Counter period for each step
	//LCD_DisplayFloat(4,4,period,4);
  if (stepping_Mode == half_Stepping)
  {
    period /= 2;		///Half period for half-stepping
  }
  prescaler = ((SYSCLK/2) / f_Counter) - 1;		///Prescaler value

  Tim3_Handle.Instance = TIM3;
  Tim3_Handle.Init.Period = period - 1;
  Tim3_Handle.Init.Prescaler = prescaler;
  Tim3_Handle.Init.ClockDivision = 0;
  Tim3_Handle.Init.CounterMode = TIM_COUNTERMODE_UP;
	
  if(HAL_TIM_OC_Init(&Tim3_Handle) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }

}

void TIM3_Re_Config()
{
	if (current_Stepping == full_Stepping)
		{
			TIM3_Config(full_Stepping);
		}
		else TIM3_Config(half_Stepping);
}

void TIM3_OC_Config(void)
{
		Tim3_OCInitStructure.OCMode = TIM_OCMODE_TIMING;
		Tim3_OCInitStructure.Pulse = period - 1;     
		Tim3_OCInitStructure.OCPolarity = TIM_OCPOLARITY_HIGH;
		
		HAL_TIM_OC_Init(&Tim3_Handle); 
	
		HAL_TIM_OC_ConfigChannel(&Tim3_Handle, &Tim3_OCInitStructure, TIM_CHANNEL_1); 
	
	 	HAL_TIM_OC_Start_IT(&Tim3_Handle, TIM_CHANNEL_1); 
		
}

void ExtBtn1_Config(void)  
{
  GPIO_InitTypeDef   GPIO_InitStructure;

  /* Enable GPIOC clock */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  
  /* Configure PC1 pin as input floating */
  GPIO_InitStructure.Mode =  GPIO_MODE_IT_FALLING;
  GPIO_InitStructure.Pull =GPIO_PULLUP;
  GPIO_InitStructure.Pin = GPIO_PIN_1;
	//GPIO_InitStructure.Speed=GPIO_SPEED_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStructure);

	//__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_1);   //is defined the same as the __HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_1); ---check the hal_gpio.h
	__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_1);// after moving the chunk of code in the GPIO_EXTI callback from _it.c (before these chunks are in _it.c)
																					//the program "freezed" when start, suspect there is a interupt pending bit there. Clearing it solve the problem.
  /* Enable and set EXTI Line0 Interrupt to the lowest priority */
  HAL_NVIC_SetPriority(EXTI1_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);
}

void ExtBtn2_Config(void)  
{
  GPIO_InitTypeDef   GPIO_InitStructure;

  /* Enable GPIOD clock */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  
  /* Configure PD2 pin as input floating */
  GPIO_InitStructure.Mode =  GPIO_MODE_IT_FALLING;
  GPIO_InitStructure.Pull =GPIO_PULLUP;
  GPIO_InitStructure.Pin = GPIO_PIN_2;
	//GPIO_InitStructure.Speed=GPIO_SPEED_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStructure);

	//__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_1);   //is defined the same as the __HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_1); ---check the hal_gpio.h
	__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_2);// after moving the chunk of code in the GPIO_EXTI callback from _it.c (before these chunks are in _it.c)
																					//the program "freezed" when start, suspect there is a interupt pending bit there. Clearing it solve the problem.
  /* Enable and set EXTI Line0 Interrupt to the lowest priority */
  HAL_NVIC_SetPriority(EXTI2_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);
}

void ExtBtn3_Config(void)  
{
  GPIO_InitTypeDef   GPIO_InitStructure;

  /* Enable GPIOC clock */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  
  /* Configure PC3 pin as input floating */
  GPIO_InitStructure.Mode =  GPIO_MODE_IT_FALLING;
  GPIO_InitStructure.Pull =GPIO_PULLUP;
  GPIO_InitStructure.Pin = GPIO_PIN_3;
	//GPIO_InitStructure.Speed=GPIO_SPEED_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStructure);

	//__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_1);   //is defined the same as the __HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_1); ---check the hal_gpio.h
	__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_3);// after moving the chunk of code in the GPIO_EXTI callback from _it.c (before these chunks are in _it.c)
																					//the program "freezed" when start, suspect there is a interupt pending bit there. Clearing it solve the problem.
  /* Enable and set EXTI Line0 Interrupt to the lowest priority */
  HAL_NVIC_SetPriority(EXTI3_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);
}

void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef * htim) 
{	
	if ((current_Stepping == full_Stepping) && (current_Rotation == Clockwise))
	{
		step_Full_CW();
	}
	else if ((current_Stepping == half_Stepping) && (current_Rotation == Clockwise))
	{
		step_Half_CW();
	}
	else if ((current_Stepping == full_Stepping) && (current_Rotation == CounterClockwise))
	{
		step_Full_CCW();
	}
	else if ((current_Stepping == half_Stepping) && (current_Rotation == CounterClockwise))
	{
		step_Half_CCW();
	}
	
	HAL_GPIO_WritePin(GPIOE,GPIO_PIN_2,current_Pin_State[0]);
	HAL_GPIO_WritePin(GPIOE,GPIO_PIN_3,current_Pin_State[1]);
	HAL_GPIO_WritePin(GPIOE,GPIO_PIN_4,current_Pin_State[2]);
	HAL_GPIO_WritePin(GPIOE,GPIO_PIN_5,current_Pin_State[3]);
	
	BSP_LED_Toggle(LED4);
	__HAL_TIM_SET_COUNTER(htim, 0x0000);
}
 
void step_Full_CW()
{
	switch (pin_State)
	{
		case 0:
			pin_State = 2;
			current_Pin_State = pin_State_2;
			break;
		case 2:
			pin_State = 4;
			current_Pin_State = pin_State_4;
			break;
		case 4:
			pin_State = 6;
			current_Pin_State = pin_State_6;
			break;
		case 6:
			pin_State = 0;
			current_Pin_State = pin_State_0;
			break;
	}
}

void step_Half_CW()
{
	switch (pin_State)
	{
		case 0:
			pin_State = 1;
			current_Pin_State = pin_State_1;
			break;
		case 1:
			pin_State = 2;
			current_Pin_State = pin_State_2;
			break;
		case 2:
			pin_State = 3;
			current_Pin_State = pin_State_3;
			break;
		case 3:
			pin_State = 4;
			current_Pin_State = pin_State_4;
			break;
		case 4:
			pin_State = 5;
			current_Pin_State = pin_State_5;
			break;
		case 5:
			pin_State = 6;
			current_Pin_State = pin_State_6;
			break;
		case 6:
			pin_State = 7;
			current_Pin_State = pin_State_7;
			break;
		case 7:
			pin_State = 0;
			current_Pin_State = pin_State_0;
			break;
	}
}

void step_Full_CCW()
{
	switch (pin_State)
	{
		case 0:
			pin_State = 6;
			current_Pin_State = pin_State_6;
			break;
		case 6:
			pin_State = 4;
			current_Pin_State = pin_State_4;
			break;
		case 4:
			pin_State = 2;
			current_Pin_State = pin_State_2;
			break;
		case 2:
			pin_State = 0;
			current_Pin_State = pin_State_0;
			break;
	}
}

void step_Half_CCW()
{
	switch (pin_State)
	{
		case 0:
			pin_State = 7;
			current_Pin_State = pin_State_7;
			break;
		case 7:
			pin_State = 6;
			current_Pin_State = pin_State_6;
			break;
		case 6:
			pin_State = 5;
			current_Pin_State = pin_State_5;
			break;
		case 5:
			pin_State = 4;
			current_Pin_State = pin_State_4;
			break;
		case 4:
			pin_State = 3;
			current_Pin_State = pin_State_3;
			break;
		case 3:
			pin_State = 2;
			current_Pin_State = pin_State_2;
			break;
		case 2:
			pin_State = 1;
			current_Pin_State = pin_State_1;
			break;
		case 1:
			pin_State = 0;
			current_Pin_State = pin_State_0;
			break;
	}
}



static void Error_Handler(void)
{
  /* Toggle LED4 Fast */
  while(1)
  {
		BSP_LED_Toggle(LED4);
		HAL_Delay(50);
  }
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/****** 


1.The Fist Extern button (named extBtn1)  connected to PC1, ExtBtn1_PC1_Config  //
		2014: canot use pin PB1, for 429i-DISCO ,pb1 is used by LCD. if use this pin, always interrupt by itself
					can not use pin PA1, since it is used by gyro. if use this pin, never interrupt.
					pd1----WILL ACT AS PC13, To trigger the RTC timestamp event
					....ONLY PC1 CAN BE USED TO FIRE EXTI1 !!!!
2. the Second external button (extBtn2)
		2014: 
		PA2: NOT OK. (USED BY LCD??)
		PB2: ????.
		PC2: ok, BUT sometimes (every 5 times around), press pc2 will trigger exti1, which is configured to use PC1. (is it because of using internal pull up pin config?)
		      however, press PC1 does not affect exti 2. sometimes press PC2 will also affect time stamp (PC13)
		PD2: OK,     
		PE2:  OK  (PE3, PE4 PE5 , seems has no other AF function, according to the table in manual for discovery board)
		PF2: NOT OK. (although PF2 is used by SDRAM, it affects LCD. press it, LCD will flick and displayed chars change to garbage)
		PG2: OK
		

*/

#include "main.h"


#define COLUMN(x) ((x) * (((sFONT *)BSP_LCD_GetFont())->Width))    //see font.h, for defining LINE(X)

///Checkpoints for the buttons
__IO uint32_t sysTick_1 = 0;
__IO uint32_t sysTick_2 = 0;

///Structs in ADC
ADC_HandleTypeDef    Adc3_Handle;
ADC_ChannelConfTypeDef sConfig;

__IO uint16_t ADC3ConvertedValue=0;

///Structs in PWM
TIM_HandleTypeDef    Tim3_Handle;
TIM_OC_InitTypeDef Tim3_OCInitStructure;

__IO uint16_t pulseWidth = 0;

///Initial setpoint, higher than room temperature
volatile double  setPoint=27.5;  

///Temperature variables
double measuredTemp;
double tempDiff;


void  LEDs_Config(void);
void  LEDs_On(void);
void  LEDs_Off(void);
void  LEDs_Toggle(void);


void LCD_DisplayString(uint16_t LineNumber, uint16_t ColumnNumber, uint8_t *ptr);
void LCD_DisplayInt(uint16_t LineNumber, uint16_t ColumnNumber, int Number);
void LCD_DisplayFloat(uint16_t LineNumber, uint16_t ColumnNumber, float Number, int DigitAfterDecimalPoint);



static void SystemClock_Config(void);
static void Error_Handler(void);

void ExtBtn1_Config(void);
void ExtBtn2_Config(void);


void ADC_Config(void);
void PWM_Config(void);


int main(void){
	
		/* STM32F4xx HAL library initialization:
       - Configure the Flash prefetch, instruction and Data caches
       - Configure the Systick to generate an interrupt each 1 msec
       - Set NVIC Group Priority to 4
       - Global MSP (MCU Support Package) initialization
     */
		HAL_Init();
		
	
		 /* Configure the system clock to 180 MHz */
		SystemClock_Config();
		
		HAL_InitTick(0x0000); // set systick's priority to the highest.
	
		//Configure LED3 and LED4 ======================================
		LEDs_Config();
	
		PWM_Config();
	
		//configure the USER button as exti mode. 
		BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_EXTI);   // BSP_functions in stm32f429i_discovery.c
	
		///Button Configs
		ExtBtn1_Config();
		ExtBtn2_Config();
	
	
		BSP_LCD_Init();
		//BSP_LCD_LayerDefaultInit(uint16_t LayerIndex, uint32_t FB_Address);
		BSP_LCD_LayerDefaultInit(0, LCD_FRAME_BUFFER);   //LCD_FRAME_BUFFER, defined as 0xD0000000 in _discovery_lcd.h
															// the LayerIndex may be 0 and 1. if is 2, then the LCD is dark.
		//BSP_LCD_SelectLayer(uint32_t LayerIndex);
		BSP_LCD_SelectLayer(0);
		//BSP_LCD_SetLayerVisible(0, ENABLE); //do not need this line.
		BSP_LCD_Clear(LCD_COLOR_WHITE);  //need this line, otherwise, the screen is dark	
		BSP_LCD_DisplayOn();
	 
		BSP_LCD_SetFont(&Font20);  //the default font,  LCD_DEFAULT_FONT, which is defined in _lcd.h, is Font24
	
	
		LCD_DisplayString(3, 2, (uint8_t *) " 3TA4 Lab4 ");
	
		LCD_DisplayString(5, 1, (uint8_t *) "Current ");
		LCD_DisplayString(6, 1, (uint8_t *)"setPoint");
	

		ADC_Config();
		
	while(1) {
		
		///Convert ADC output value into temperature
		BSP_LED_Toggle(LED3);
		ADC3ConvertedValue=HAL_ADC_GetValue(&Adc3_Handle);
		measuredTemp = ADC3ConvertedValue*0.02441406;
		
		///Display on screen
		LCD_DisplayFloat(5, 11, measuredTemp, 2);
		LCD_DisplayFloat(6, 11, setPoint, 2);
		HAL_Delay(500);
		
		
		LEDs_Toggle();///Indicator for processing
		
	
		///Turn up the setPoint on Hold button 1 longer than 0.5s
		if (!HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1)){
			sysTick_1 = HAL_GetTick();
			while (!HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1)){
				if (HAL_GetTick() - sysTick_1 > 500){
					setPoint += 0.002;
					LCD_DisplayFloat(6, 11, setPoint, 2);
				}
			}
		}
		
		///Turn down the setPoint on Hold bitton 2 longer than 0.5s
		if (!HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_2)){
			sysTick_2 = HAL_GetTick();
			while (!HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_2)){
				if (HAL_GetTick() - sysTick_2 > 500){
					setPoint -= 0.002;
					LCD_DisplayFloat(6, 11, setPoint, 2);
				}
			}
		}		
		
		
		///Get gap value
		tempDiff = measuredTemp - setPoint;
		
		///Compare and turn on the fan
		if (tempDiff > 5)
		{
			pulseWidth = 1000;	///Highest speed for temp gap greater than 5 degrees
		}
		
		else if (tempDiff > 0)
		{
			pulseWidth = 500 + 500 * tempDiff / 5;	///Adjust speed autometically for 0 < temp < 5C
		}
		
		else pulseWidth = 0;
		
		BSP_LCD_ClearStringLine(12);
		LCD_DisplayString(11,3,"PulseWidth");
		LCD_DisplayInt(12, 7, pulseWidth);
		PWM_Config();	///Update pulseWidth for fan power
	
		
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

void LEDs_Config(void)
{
 /* Initialize Leds mounted on STM32F429-Discovery board */
	BSP_LED_Init(LED3);   //BSP_LED_....() are in stm32f4291_discovery.c
  BSP_LED_Init(LED4);
}

void LEDs_On(void)
{
/* Turn on LED3, LED4 */
 BSP_LED_On(LED3);
  BSP_LED_On(LED4);
}

void LEDs_Off(void)
{
/* Turn on LED3, LED4 */
  BSP_LED_Off(LED3);
  BSP_LED_Off(LED4);
}
void LEDs_Toggle(void)
{
/* Turn on LED3, LED4 */
  BSP_LED_Toggle(LED3);
  BSP_LED_Toggle(LED4);
	HAL_Delay(1000);
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
	
  if(GPIO_Pin == KEY_BUTTON_PIN)  //GPIO_PIN_0
  {
		LEDs_Toggle();	///Test for working
  }
	
	
	if(GPIO_Pin == GPIO_PIN_1)
  {
		BSP_LED_Toggle(LED3);	///Test for working

	}  //end of PIN_1

	if(GPIO_Pin == GPIO_PIN_2)
  {
		BSP_LED_Toggle(LED4);	///Test for working
			
	} //end of if PIN_2	
	
	
}



void ADC_Config()
{
	/*##-1- Configure the ADC peripheral #######################################*/
	Adc3_Handle.Instance          = ADC3;  
	
	
  Adc3_Handle.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV2;
  Adc3_Handle.Init.Resolution = ADC_RESOLUTION_12B;
  Adc3_Handle.Init.ScanConvMode = DISABLE;
  Adc3_Handle.Init.ContinuousConvMode = ENABLE;
  Adc3_Handle.Init.DiscontinuousConvMode = DISABLE;
  Adc3_Handle.Init.NbrOfDiscConversion = 0;
  Adc3_Handle.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  Adc3_Handle.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T1_CC1;
  Adc3_Handle.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  Adc3_Handle.Init.NbrOfConversion = 1;
  Adc3_Handle.Init.DMAContinuousRequests = DISABLE;
  Adc3_Handle.Init.EOCSelection = DISABLE;
	
	if(HAL_ADC_Init(&Adc3_Handle) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler(); 
  }
	
	/*##-2- Configure ADC regular channel ######################################*/
	sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  sConfig.Offset = 0;
	
	if(HAL_ADC_ConfigChannel(&Adc3_Handle, &sConfig) != HAL_OK)
  {
    /* Channel Configuration Error */
    Error_Handler(); 
	}
	
	/*##-3- Start the conversion process and enable interrupt ##################*/
	if(HAL_ADC_Start_DMA(&Adc3_Handle, (uint32_t*)&ADC3ConvertedValue, 1) != HAL_OK)
  {
    /* Start Conversation Error */
    Error_Handler(); 
  }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* AdcHandle)
{
  /* Turn LED3 on: Transfer process is correct */
  BSP_LED_On(LED3);
	HAL_Delay(500);
	BSP_LED_On(LED4);
}



void PWM_Config()
{
	
	Tim3_Handle.Instance = TIM3; ///Using TIM3
	
  Tim3_Handle.Init.Period = 1000; 	///Set period
  Tim3_Handle.Init.Prescaler = 1799;	///Set prescaler
  Tim3_Handle.Init.ClockDivision = 0;
  Tim3_Handle.Init.CounterMode = TIM_COUNTERMODE_UP;
	Tim3_Handle.Init.RepetitionCounter = 0;
	Tim3_Handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	
	Tim3_OCInitStructure.OCMode = TIM_OCMODE_PWM1; 
	Tim3_OCInitStructure.OCFastMode = TIM_OCFAST_DISABLE;
	Tim3_OCInitStructure.OCPolarity = TIM_OCPOLARITY_HIGH;
	Tim3_OCInitStructure.Pulse = pulseWidth;	///PulseWidth for powering the fan
	
	if(HAL_TIM_PWM_Init(&Tim3_Handle) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }
	
	if(HAL_TIM_PWM_ConfigChannel(&Tim3_Handle, &Tim3_OCInitStructure, TIM_CHANNEL_3) != HAL_OK)
  {
    /* Configuration Error */
    Error_Handler();
	}
	
	if(HAL_TIM_PWM_Start(&Tim3_Handle, TIM_CHANNEL_3) != HAL_OK)
  {
    /* Starting Error */
    Error_Handler();
  }  
	
	
	
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

  /* Enable GPIOB clock */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  
  /* Configure PA0 pin as input floating */
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



void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef * htim) //see  stm32fxx_hal_tim.c for different callback function names. 
{																																//for timer4 
	//	if ((*htim).Instance==TIM4)
			 //BSP_LED_Toggle(LED4);
		//clear the timer counter!  in stm32f4xx_hal_tim.c, the counter is not cleared after  OC interrupt
		__HAL_TIM_SET_COUNTER(htim, 0x0000);   //this maro is defined in stm32f4xx_hal_tim.h
	
}
 
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef * htim){  //this is for TIM3_pwm
	
	//__HAL_TIM_SET_COUNTER(htim, 0x0000);   //not necessary  
}



static void Error_Handler(void)
{
  /* Turn LED4 on */
  BSP_LED_On(LED4);
  while(1)
  {
		BSP_LED_Toggle(LED4);
		HAL_Delay(200);
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
/**
  * @}
  */

/**
  * @}
  */




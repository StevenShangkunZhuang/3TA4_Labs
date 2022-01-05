/****** 



1.The Fist Extern button (named extBtn1)  connected to PC1 (canot use PB1, for 429i-DISCO ,pb1 is used by LCD), ExtBtn1_Config()  //
		2014: canot use pin PB1, for 429i-DISCO ,pb1 is used by LCD. if use this pin, always interrupt by itself
					can not use pin PA1, used by gyro. if use this pin, never interrupt
					pd1----WILL ACT AS PC13, To trigger the RTC timestamp event
					....ONLY PC1 CAN BE USED TO FIRE EXTI1 !!!!
2. the Second external button (extBtn2) may connect to pin PD2.  ExtBtn2_Config() --The pin PB2 on the board have trouble.
    when connect extern button to the pin PB2, the voltage at this pin is not 3V as it is supposed to be, it is 0.3V, why?
		so changed to use pin PD2.
	
	 
		PA2: NOT OK. (USED BY LCD??)
		PB2: ??????
		PC2: ok, BUT sometimes (every 5 times around), press pc2 will trigger exti1, which is configured to use PC1. (is it because of using internal pull up pin config?)
		      however, press PC1 does not affect exti 2. sometimes press PC2 will also affect time stamp (PC13)
		PD2: OK,     
		PE2:  OK  (PE3, PE4 PE5 , seems has no other AF function, according to the table in manual for discovery board)
		PF2: NOT OK. (although PF2 is used by SDRAM, it affects LCD. press it, LCD will flick and displayed chars change to garbage)
		PG2: OK
		
	3. RTC and RTC_Alarm have been configured.
 
*/


/* Includes ------------------------------------------------------------------*/
#include "main.h"


/** @addtogroup STM32F4xx_HAL_Examples
  * @{
  */

/** @addtogroup GPIO_EXTI
  * @{
  */ 

/* Private typedef -----------------------------------------------------------*/

/* Typedef of all states */
typedef enum
{
	currentTime,									///Main page, update time at 1 Hz
	currentDate,									///User pressed and held, display date
	twoHistory,										///Ext_Btn_1 pressed, display last two times at USER press
	settingMode,									///Ext_Btn_2 pressed, enter setting mode
} enumState;

/* Setting mode states typedef */
typedef enum
{
	yearSetting,												///Setting year value
	monthSetting,												///Setting month value
	daySetting,													///Setting day value
	weekdaySetting,											///Setting weekday value
	hrSetting,													///Setting hour value
	minSetting,													///uSetting minute vale
	secSetting,													///Setting second value
	endSetting,													///End of row, wrap around to year
} enumSetting;

/* struct of all flag status */
typedef struct
{
	uint8_t userPressed;								///Flag when USER is pressed
	uint8_t userHeld;										///Flag when USER is held for > 800 ms
	uint8_t ext1Pressed;								///Flag when external button 1 is pressed
	uint8_t ext2Pressed;								///Flag when external button 2 is pressed
} buttonFlagsStruct;

/* Internal time struct */
typedef struct
{
	uint8_t hour;												///Get  hour value
	uint8_t minute;											///Get minute value
	uint8_t second;											///Get second value
}time_t;


/* Private define ------------------------------------------------------------*/
#define COLUMN(x) ((x) * (((sFONT *)BSP_LCD_GetFont())->Width))    //see font.h, for defining LINE(X)


/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* General variables */
enumState					state;										///To choose states
enumSetting				setting;									///To start setting date and time
buttonFlagsStruct	buttonFlags;							///To switch flags

/* Labels to improve Usability when setting day and date */
char yearVal[]		=	{"Year"};
char monthVal[]		=	{"Month"};
char dayVal[]			=	{"Day"};
char wkdVal[]			= {"Weekday"};
char hourVal[]		=	{"Hour"};
char minuteVal[]	=	{"Minute"};
char secondVal[]	=	{"Second"};



uint8_t RTC_INT_TRIGGERED_FLAG = 1;			///RTC Interrupt Flag set 1

I2C_HandleTypeDef  I2c3_Handle;

RTC_HandleTypeDef RTCHandle;
RTC_DateTypeDef 	RTC_DateStructure;
RTC_TimeTypeDef 	RTC_TimeStructure;



HAL_StatusTypeDef Hal_status;  //HAL_ERROR, HAL_TIMEOUT, HAL_OK, of HAL_BUSY 


//memory location to write to in the device
__IO uint16_t memLocation = 0x000A; 


char lcd_buffer[100];										///Store temperary data



void RTC_Config(void);
void RTC_AlarmAConfig(void);



void ExtBtn1_Config(void);  						///Config first External button
void ExtBtn2_Config(void);							///Config second External button


 

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void Error_Handler(void);

__IO uint32_t sysTick_At_User;     	    ///Fire sysTick when the User button is pressed

void RTC_Config                 (void);
void RTC_AlarmAConfig           (void);
void displayTime                (void);
void displayDate                (void);
void displayHistory           (void);
void getPastTimes               (time_t *, time_t *);
void saveCurrentTime            (void);
void increaseTimeSettingState  (enumSetting *);
void increaseTimeSetting       (enumSetting *);
void updateTimeSetScreen        (enumSetting *);
void clearBtnFlags         			(void);

/* State navigation functions */
void gotoTimeState      (void);
void gotoDateState      (void);
void gotoHistoryState (void);
void goTimeSettingState          (void);

/* BCD Decimal Conversion functions */
static int bcdToDecimal (uint8_t);
static int decimalToBcd (unsigned int);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
  //the following variables are for testging I2C_EEPROM
	
	///uint8_t data1 =0x67,  data2=0x68;
	///uint8_t readData=0x00;
	///char AA[34]= "this_is_for_testing_eeprom_LCD_Lmt";
	///uint8_t * bufferdata=(uint8_t *)AA;	
	///int i;
	///uint8_t readMatch=1;
	///uint32_t EE_status;

	
	/* Set all-time state to display time state */
  state = currentTime;

  /* Clear all Button lags */
  clearBtnFlags();
	
	
	/* STM32F4xx HAL library initialization:
       - Configure the Flash prefetch, instruction and Data caches
       - Configure the Systick to generate an interrupt each 1 msec
       - Set NVIC Group Priority to 4
       - Global MSP (MCU Support Package) initialization
     */
  HAL_Init();
	
	 /* Configure the system clock to 180 MHz */
  SystemClock_Config();
	
	
	//Init Systic interrupt so can use HAL_Delay() function 
	HAL_InitTick(0x0000); // set systick's priority to the highest.
                        
  
	
	//Configure LED3 and LED4 ======================================
	BSP_LED_Init(LED3);
  BSP_LED_Init(LED4);


	//configure the USER button as exti mode. 
	BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_EXTI);   // BSP_functions in stm32f429i_discovery.c
	


	//Use this line, so do not need extiline0_config()
	//configure external push-buttons and interrupts
	ExtBtn1_Config();
	ExtBtn2_Config();
	
	
	
//-----------Init LCD 
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
	
	
		LCD_DisplayString(0, 2, (uint8_t *)"MT3TA4 LAB 3");
		///LCD_DisplayString(6, 2, (uint8_t *)"Testing EEPROM....");
		
		///HAL_Delay(2000);   //display for 1 second
		
		//BSP_LCD_Clear(LCD_COLOR_WHITE);
		///BSP_LCD_ClearStringLine(5);
		///BSP_LCD_ClearStringLine(6);
		///BSP_LCD_ClearStringLine(7);
		
		
		
	//	LCD_DisplayInt(7, 6, 123456789);
	//	LCD_DisplayFloat(8, 6, 12.3456789, 4);


	
	//configure real-time clock
	RTC_Config();
		
	RTC_AlarmAConfig();
	
	
	
//Init I2C for EEPROM		
	I2C_Init(&I2c3_Handle);


//*********************Testing I2C EEPROM------------------
/*

	EE_status=I2C_ByteWrite(&I2c3_Handle,EEPROM_ADDRESS, memLocation , data1);
	if (EE_status==HAL_OK)
			LCD_DisplayString(0, 0, (uint8_t *)"w data1 OK");
	else
			LCD_DisplayString(0, 0, (uint8_t *)"w data1 failed");

	EE_status=I2C_ByteWrite(&I2c3_Handle,EEPROM_ADDRESS, memLocation+1 , data2);
	if (EE_status==HAL_OK)
			LCD_DisplayString(1, 0, (uint8_t *)"w data2 OK");
	else
			LCD_DisplayString(1, 0, (uint8_t *)"w data2 failed");
	
	
	
	
	readData=I2C_ByteRead(&I2c3_Handle,EEPROM_ADDRESS, memLocation);
	if (data1 == readData) {
			LCD_DisplayString(3, 0, (uint8_t *)"r data1 success");
	}else{
			LCD_DisplayString(3, 0, (uint8_t *)"r data1 mismatch");
	}	
	LCD_DisplayInt(3, 14, readData);
	
	readData=I2C_ByteRead(&I2c3_Handle,EEPROM_ADDRESS, memLocation+1);
	if (data2 == readData) {
			LCD_DisplayString(4, 0, (uint8_t *)"r data2 success");
	}else{
			LCD_DisplayString(4, 0, (uint8_t *)"r data2 mismatch");
	}	
	LCD_DisplayInt(4, 14, readData);
	

	EE_status=I2C_BufferWrite(&I2c3_Handle,EEPROM_ADDRESS, memLocation, bufferdata, 34);
	if (EE_status==HAL_OK)
		LCD_DisplayString(6, 0, (uint8_t *)"w buffer OK");
	else
		LCD_DisplayString(6, 0, (uint8_t *)"W buffer failed");

	for (i=0;i<=33;i++) { 
			readData=I2C_ByteRead(&I2c3_Handle,EEPROM_ADDRESS, memLocation+i);
			HAL_Delay(5);   // just for display effect. for EEPROM read, do not need dalay		
		//BUT :  if here delay longer time, the floowing display will have trouble,???
	
			BSP_LCD_DisplayChar(COLUMN(i%16),LINE(8+ 2*(int)(i/16)), (char) readData);	
			BSP_LCD_DisplayChar(COLUMN(i%16),LINE(9+ 2*(int)(i/16)),  bufferdata[i]);
			if (bufferdata[i]!=readData)
					readMatch=0;
	}

	if (readMatch==0)
		LCD_DisplayString(15, 0, (uint8_t *)"r buffer mismatch");
	else 
		LCD_DisplayString(15, 0, (uint8_t *)"r buffer success");

*/
//******************************testing I2C EEPROM*****************************/



  /* Infinite loop */
int i = 1;
while (1)
  {				
		LCD_DisplayString(0, 2, (uint8_t *)"MT3TA4 LAB 3");
		if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0)) {																		///Read when user is pressed
      sysTick_At_User = HAL_GetTick();																						///Record pressing start time
      while(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0)) {  															///Still pressing
        if ((HAL_GetTick() - sysTick_At_User)>800) {															///Determint pressing/holding
					gotoDateState();																												///If held, show date
					buttonFlags.userPressed = 0;																						///Switch flags
          buttonFlags.userHeld = 1;
					
        }
      }
    }
		
		switch (state)
    {
      case currentTime:																																						
        /* If RTC interrupt has been fired */
        if (RTC_INT_TRIGGERED_FLAG)
        {
					/* If RTC working, display current time */
          if (HAL_RTC_GetTime(&RTCHandle, &RTC_TimeStructure, RTC_FORMAT_BCD) == HAL_OK)
          {
            BSP_LED_Toggle(LED3);
            BSP_LED_Toggle(LED4);
            displayTime();
          }
          RTC_INT_TRIGGERED_FLAG = 0;
					HAL_RTC_GetDate(&RTCHandle, &RTC_DateStructure, RTC_FORMAT_BCD);
          
        }
        /* If user pressed, save current time to eeprom */
        if (buttonFlags.userPressed)
        {
          buttonFlags.userPressed = 0;
          saveCurrentTime();
        }
        /* If user held, display date */
        if (buttonFlags.userHeld)
        {
					buttonFlags.userHeld = 0;
          gotoDateState();
        }
        /* If ext1 pressed, show previous two times user was pressed */
        else if (buttonFlags.ext1Pressed)
        {
					buttonFlags.ext1Pressed = 0;
          gotoHistoryState();
        }
        /* If ext2 pressed, go to set date and time state */
        else if (buttonFlags.ext2Pressed)
        {
					buttonFlags.ext2Pressed = 0;
          goTimeSettingState();
        }
        break;

      case currentDate:
        /* If RTC Interrupt is fired, update the screen */
        if (RTC_INT_TRIGGERED_FLAG)
        {
          RTC_INT_TRIGGERED_FLAG = 0;
					HAL_RTC_GetDate(&RTCHandle, &RTC_DateStructure, RTC_FORMAT_BCD);
          displayDate();
					


        }
        /* If user is pressed or held, go back to display time state */
        if (buttonFlags.userPressed | buttonFlags.userHeld)
        {
          gotoTimeState();
        }
        break;

      case twoHistory:
        /* If ext1 button pressed, go back to display time state */
        if (buttonFlags.ext1Pressed)
        {
          buttonFlags.ext1Pressed = 0;
          gotoTimeState();
        }
        break;

      case settingMode:

        /* If exdt1 pressed, change to next setting value */
        if (buttonFlags.ext1Pressed)
        {
					BSP_LCD_ClearStringLine(5);
					BSP_LCD_ClearStringLine(6);
					BSP_LCD_ClearStringLine(7);
          buttonFlags.ext1Pressed = 0;
          increaseTimeSettingState(&setting);
          updateTimeSetScreen(&setting);
        }

        /* If user pressed, increase data in chosen value */
        if (buttonFlags.userPressed)
        {
          buttonFlags.userPressed = 0;
          increaseTimeSetting(&setting);
          updateTimeSetScreen(&setting);
        }

        /* If ext2 button pressed, go back to display time state */
        if (buttonFlags.ext2Pressed)
        {
          buttonFlags.ext2Pressed = 0;
          gotoTimeState();
        }

    }

  } //end of while

}  

/* Display current day and date to screen */
void displayDate(void)
{
	int num = RTC_DateStructure.WeekDay;
	char weekdays[][20] = {"MON", "TUE", "WED", "THUR", "FRI", "SAT", "SUN"};
	//BSP_LCD_Clear(LCD_COLOR_WHITE);
	//LCD_DisplayInt(6,6, RTC_DateStructure.WeekDay);
	sprintf(lcd_buffer, "%s", weekdays[num-1]);
  LCD_DisplayString(1,2,(uint8_t*)lcd_buffer);
  sprintf(lcd_buffer, "20%02d/%02d/%02d", bcdToDecimal(RTC_DateStructure.Year), bcdToDecimal(RTC_DateStructure.Month), bcdToDecimal(RTC_DateStructure.Date));
  LCD_DisplayString(1,7,(uint8_t*)lcd_buffer);
}

/* Display current time to screen */
void displayTime(void)
{
  BSP_LCD_Clear(LCD_COLOR_WHITE);
  sprintf(lcd_buffer, "Time %02d:%02d:%02d", bcdToDecimal(RTC_TimeStructure.Hours), bcdToDecimal(RTC_TimeStructure.Minutes), bcdToDecimal(RTC_TimeStructure.Seconds));
  LCD_DisplayString(2,2,(uint8_t*)lcd_buffer);
}

/* Display past two press times to screen */
void displayHistory(void)
{
  //BSP_LCD_Clear(LCD_COLOR_WHITE);
  time_t time1;
  time_t time2;
  getPastTimes(&time1, &time2);
  sprintf(lcd_buffer, "P1 %02d:%02d:%02d", time1.hour, time1.minute, time1.second);
  LCD_DisplayString(3,2,(uint8_t*)lcd_buffer);
	sprintf(lcd_buffer, "P2 %02d:%02d:%02d", time2.hour, time2.minute, time2.second);
	LCD_DisplayString(4,2,(uint8_t*)lcd_buffer);
}

/* Display date and time settings to screen */
void displayTimeSetting(char * label, uint8_t val)
{
  //BSP_LCD_Clear(LCD_COLOR_WHITE);
  sprintf(lcd_buffer, "%s %d", label, bcdToDecimal(val));
  LCD_DisplayString(5,2,(uint8_t*)lcd_buffer);
}

/* Go to display time state */
void gotoTimeState(void)
{
  clearBtnFlags();																						///Clear all set flags
  state = currentTime;																				///Lead to state
  displayTime();																							///Display corresponding information
}

/* Go to display date state */
void gotoDateState(void)
{
  clearBtnFlags();
  state = currentDate;
  displayDate();
}

/* Go to display past two times state */
void gotoHistoryState(void)
{
  clearBtnFlags();
  state = twoHistory;
  displayHistory();
}

/* Go to set date and time state */
void goTimeSettingState(void)
{
  clearBtnFlags();
  state = settingMode;
  updateTimeSetScreen(&setting);
}

/* Update current setting value to date and time settings display function */
void updateTimeSetScreen (enumSetting * setting)
{
  switch ((*setting))
  {
    case yearSetting:
      displayTimeSetting(yearVal, (uint8_t) RTC_DateStructure.Year);
      break;
    case monthSetting:
      displayTimeSetting(monthVal, (uint8_t) RTC_DateStructure.Month);
      break;
    case daySetting:
      displayTimeSetting(dayVal, (uint8_t) RTC_DateStructure.Date);
      break;
		case weekdaySetting:
      displayTimeSetting(wkdVal, (uint8_t) RTC_DateStructure.WeekDay);
      break;
    case hrSetting:
      displayTimeSetting(hourVal, (uint8_t) RTC_TimeStructure.Hours);
      break;
    case minSetting:
      displayTimeSetting(minuteVal, (uint8_t) RTC_TimeStructure.Minutes);
      break;
    case secSetting:
      displayTimeSetting(secondVal, (uint8_t) RTC_TimeStructure.Seconds);
			break;
		case endSetting:																																		///After setting for second, go back to setting for year
			while(1);
  }
}


/* Clear all button flags, waiting for interrupts*/
void clearBtnFlags(void)
{
    buttonFlags.ext1Pressed	= 0;
    buttonFlags.ext2Pressed	= 0;
    buttonFlags.userPressed	= 0;
    buttonFlags.userHeld		= 0;
}

/* Convert decimal to decimal hex / 16 * 10 = dec tens */
static int bcdToDecimal(uint8_t hex)
{
  int dec = ((hex & 0xF0) >> 4) * 10 + (hex & 0x0F);
  return dec;
}

/* Convert decimal to decimal, dec / 10 * 16 = hex tens*/
static int decimalToBcd(unsigned int num) 
{
  unsigned int ones = 0;
  unsigned int tens = 0;
  unsigned int temp = 0;

  ones = num%10;
  temp = num/10; 
  tens = temp<<4;
  return (tens + ones);
}


/* Get past times from EEPROM */
void getPastTimes (time_t * time1, time_t * time2)
{
  /* Get time data */
  time1->second = I2C_ByteRead(&I2c3_Handle,EEPROM_ADDRESS, memLocation);
  time1->minute = I2C_ByteRead(&I2c3_Handle,EEPROM_ADDRESS, memLocation+1);
  time1->hour   = I2C_ByteRead(&I2c3_Handle,EEPROM_ADDRESS, memLocation+2);

  time2->second = I2C_ByteRead(&I2c3_Handle,EEPROM_ADDRESS, memLocation+3);
  time2->minute = I2C_ByteRead(&I2c3_Handle,EEPROM_ADDRESS, memLocation+4);
  time2->hour   = I2C_ByteRead(&I2c3_Handle,EEPROM_ADDRESS, memLocation+5);
}


/* Stores current time to EEPROM */
void saveCurrentTime(void)
{
  /* Get most recent time data */
  uint8_t storedSeconds = I2C_ByteRead(&I2c3_Handle,EEPROM_ADDRESS, memLocation);
  uint8_t storedMinutes = I2C_ByteRead(&I2c3_Handle,EEPROM_ADDRESS, memLocation+1);
  uint8_t storedHours   = I2C_ByteRead(&I2c3_Handle,EEPROM_ADDRESS, memLocation+2);

  /* Get current time data */
  uint8_t seconds = bcdToDecimal((uint8_t) (RTC_TimeStructure.Seconds));
  uint8_t minutes = bcdToDecimal((uint8_t) (RTC_TimeStructure.Minutes));
  uint8_t hours   = bcdToDecimal((uint8_t) (RTC_TimeStructure.Hours));


  uint16_t EE_status = 0;
  /* Store all */
  EE_status |= I2C_ByteWrite(&I2c3_Handle,EEPROM_ADDRESS, memLocation,     seconds);
  EE_status |= I2C_ByteWrite(&I2c3_Handle,EEPROM_ADDRESS, memLocation + 1, minutes);
  EE_status |= I2C_ByteWrite(&I2c3_Handle,EEPROM_ADDRESS, memLocation + 2, hours);
  EE_status |= I2C_ByteWrite(&I2c3_Handle,EEPROM_ADDRESS, memLocation + 3, storedSeconds);
  EE_status |= I2C_ByteWrite(&I2c3_Handle,EEPROM_ADDRESS, memLocation + 4, storedMinutes);
  EE_status |= I2C_ByteWrite(&I2c3_Handle,EEPROM_ADDRESS, memLocation + 5, storedHours);
  if(EE_status != HAL_OK)
  {
    I2C_Error(&I2c3_Handle);
  }
}


/* Increase date and time setting state */
void increaseTimeSettingState (enumSetting * setting)
{
  
	(*setting)++;
	if ((*setting) == endSetting){									///Reach the end, wrap around
		(*setting) = yearSetting;
	}
	

}

/* Increase Year */
void increaseYear(void)													///Take value from RTC, translate to dec, addition, translate back, restore
{
  uint16_t num = bcdToDecimal((uint16_t) (RTC_DateStructure.Year)) + 1;
  if (num >= 100 )																///Goes up to 99 and back to 0
  {
		BSP_LCD_ClearStringLine(5);										///Wipe the lines to avoid remainder left out
		BSP_LCD_ClearStringLine(6);
		BSP_LCD_ClearStringLine(7);
    num = 0;
  }
  RTC_DateStructure.Year = decimalToBcd(num);
}

/* Increase Month */
void increaseMonth(void)
{
  uint16_t num = bcdToDecimal((uint16_t) (RTC_DateStructure.Month)) + 1;
  if (num >= 13 )
  {
		BSP_LCD_ClearStringLine(5);
		BSP_LCD_ClearStringLine(6);
		BSP_LCD_ClearStringLine(7);
    num = 1;
  }
  RTC_DateStructure.Month = decimalToBcd(num);
}

/* Increase Day */
void increaseDay(void)
{
  uint16_t num = bcdToDecimal((uint16_t) (RTC_DateStructure.Date)) + 1;
  if (num >= 32 )
  {
		BSP_LCD_ClearStringLine(5);
		BSP_LCD_ClearStringLine(6);
		BSP_LCD_ClearStringLine(7);
    num = 1;
  }
  RTC_DateStructure.Date = decimalToBcd(num);
}
/* Increase Weekday */
void increaseWeekday(void)
{
  uint16_t num = bcdToDecimal((uint16_t) (RTC_DateStructure.WeekDay)) + 1;
  if (num >= 8 )
  {
		BSP_LCD_ClearStringLine(5);
		BSP_LCD_ClearStringLine(6);
		BSP_LCD_ClearStringLine(7);
    num = 1;
  }
  RTC_DateStructure.WeekDay = decimalToBcd(num);
}

/* Increase Hour */
void increaseHour(void)																		///Hour goes up to 11 and back to 0, without 12
{
  uint8_t num = bcdToDecimal((uint16_t) (RTC_TimeStructure.Hours)) + 1;
  if (num >= 12 )
  {
		BSP_LCD_ClearStringLine(5);
		BSP_LCD_ClearStringLine(6);
		BSP_LCD_ClearStringLine(7);
    num = 0;
  }
  RTC_TimeStructure.Hours = decimalToBcd(num);
}

/* Increase Minute */
void increaseMinute(void)
{
  uint16_t num = bcdToDecimal((uint16_t) (RTC_TimeStructure.Minutes)) + 1;
  if (num >= 60 )
  {
		BSP_LCD_ClearStringLine(5);
		BSP_LCD_ClearStringLine(6);
		BSP_LCD_ClearStringLine(7);
    num = 0;
  }
  RTC_TimeStructure.Minutes = decimalToBcd(num);
}

/* Increase second */
void increaseSecond(void)
{
  uint16_t num = bcdToDecimal((uint16_t) (RTC_TimeStructure.Seconds)) + 1;
  if (num >= 60 )
  {
		BSP_LCD_ClearStringLine(5);
		BSP_LCD_ClearStringLine(6);
		BSP_LCD_ClearStringLine(7);
    num = 0;
  }
  RTC_TimeStructure.Seconds = decimalToBcd(num);
}

/* Sets RTC Date and Time */
void increaseTimeSetting (enumSetting * setting)
{
  switch ((*setting))
  {
    case yearSetting:
      increaseYear();
      break;
    case monthSetting:
      increaseMonth();
      break;
    case daySetting:
      increaseDay();
      break;
		case weekdaySetting:
      increaseWeekday();
      break;
    case hrSetting:
      increaseHour();
      break;
    case minSetting:
      increaseMinute();
      break;
    case secSetting:
      increaseSecond();
			break;
		case endSetting:
			while(1);

  }
  HAL_RTC_SetTime(&RTCHandle,&RTC_TimeStructure,RTC_FORMAT_BCD);															///Set time
  HAL_RTC_SetDate(&RTCHandle,&RTC_DateStructure,RTC_FORMAT_BCD);															///Set date
  HAL_RTC_GetTime(&RTCHandle, &RTC_TimeStructure, RTC_FORMAT_BCD);														///get time
  HAL_RTC_GetDate(&RTCHandle, &RTC_DateStructure, RTC_FORMAT_BCD);														///get date
  updateTimeSetScreen(setting);																																///Update setting to screen

}





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
 
  /* user PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 
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


/**
  * @brief  Configures EXTI Line0 (connected to PA0 pin) in interrupt mode
  * @param  None
  * @retval None
  */




/**
 * Use this function to configure the GPIO to handle input from
 * external pushbuttons and configure them so that you will handle
 * them through external interrupts.
 */
void ExtBtn1_Config(void)  
{
  GPIO_InitTypeDef   GPIO_InitStructure;

  /* Enable GPIOB clock */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  
  /* Configure PA0 pin as input floating */
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

void ExtBtn2_Config(void){  //**********PD2.***********
 
	// Same as ext1, change C to D, 1 to 2
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


void RTC_Config(void) {
	RTC_TimeTypeDef RTC_TimeStructure;
	RTC_DateTypeDef RTC_DateStructure;
	
	/****************
	EXTI_InitTypeDef EXTI_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	RTC_AlarmTypeDef RTC_AlarmStructure;
	****************/
	
	//1: Enable the RTC domain access (enable wirte access to the RTC )
			//1.1: Enable the Power Controller (PWR) APB1 interface clock:
        __HAL_RCC_PWR_CLK_ENABLE(); 
			//1.2:  Enable access to RTC domain 
				HAL_PWR_EnableBkUpAccess();
			//1.3: Select the RTC clock source
				__HAL_RCC_RTC_CONFIG(RCC_RTCCLKSOURCE_LSI);  //RCC_RTCCLKSOURCE_LSI is defined in hal_rcc.h
	       // according to P9 of AN3371 Application Note, LSI's accuracy is not suitable for RTC application!!!! 
					//can not use LSE!!!---LSE is not available, at leaset not available for stm32f407 board.
				//****"Without parts at X3, C16, C27, and removing SB15 and SB16, the LSE is not going to tick or come ready"*****.
			//1.4: Enable RTC Clock
			__HAL_RCC_RTC_ENABLE();   //enable RTC
			
	
			//1.5  Enable LSI
			__HAL_RCC_LSI_ENABLE();   //need to enable the LSI !!!
																//defined in _rcc.c
			while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSIRDY)==RESET) {}    //defind in rcc.c
	
			// for the above steps, please see the CubeHal UM1725, p616, section "Backup Domain Access" 	
				
				
				
	//2.  Configure the RTC Prescaler (Asynchronous and Synchronous) and RTC hour 
        
				RTCHandle.Instance = RTC;
				RTCHandle.Init.HourFormat = RTC_HOURFORMAT_24;
				//RTC time base frequency =LSE/((AsynchPreDiv+1)*(SynchPreDiv+1))=1Hz
				//see the AN3371 Application Note: if LSE=32.768K, PreDiv_A=127, Prediv_S=255
				//    if LSI=32K, PreDiv_A=127, Prediv_S=249
				//also in the note: LSI accuracy is not suitable for calendar application!!!!!! 
				RTCHandle.Init.AsynchPrediv = 127; //if using LSE: Asyn=127, Asyn=255: 
				RTCHandle.Init.SynchPrediv = 249;  //if using LSI(32Khz): Asyn=127, Asyn=249: 
				// but the UM1725 says: to set the Asyn Prescaler a higher value can mimimize power comsumption
				
				RTCHandle.Init.OutPut = RTC_OUTPUT_DISABLE;
				RTCHandle.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
				RTCHandle.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
				
				//HAL_RTC_Init(); 
				if(HAL_RTC_Init(&RTCHandle) != HAL_OK)
				{
						LCD_DisplayString(1, 0, (uint8_t *)"RTC Init Error!");
				}
	
	//3. init the time and date
				RTC_DateStructure.Year = 21;
				RTC_DateStructure.Month = RTC_MONTH_NOVEMBER;
				RTC_DateStructure.Date = 3; //if use RTC_FORMAT_BCD, NEED TO SET IT AS 0x18 for the 18th.
				RTC_DateStructure.WeekDay = RTC_WEEKDAY_WEDNESDAY; //???  if the real weekday is not correct for the given date, still set as 
																												//what is specified here.
				
				if(HAL_RTC_SetDate(&RTCHandle,&RTC_DateStructure,RTC_FORMAT_BIN) != HAL_OK)   //BIN format is better 
															//before, must set in BCD format and read in BIN format!!
				{
					LCD_DisplayString(2, 0, (uint8_t *)"Date Init Error!");
				} 
  
  
				RTC_TimeStructure.Hours = 8;  
				RTC_TimeStructure.Minutes = 40; //if use RTC_FORMAT_BCD, NEED TO SET IT AS 0x19
				RTC_TimeStructure.Seconds = 29;
				RTC_TimeStructure.TimeFormat = RTC_HOURFORMAT12_AM;
				RTC_TimeStructure.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
				RTC_TimeStructure.StoreOperation = RTC_STOREOPERATION_RESET;//?????/
				
				if(HAL_RTC_SetTime(&RTCHandle,&RTC_TimeStructure,RTC_FORMAT_BIN) != HAL_OK)   //BIN format is better
																																					//before, must set in BCD format and read in BIN format!!
				{
					LCD_DisplayString(3, 0, (uint8_t *)"TIME Init Error!");
				}
			//Writes a data in a RTC Backup data Register0   --why need this line?
			//HAL_RTCEx_BKUPWrite(&RTCHandle,RTC_BKP_DR0,0x32F2);   

	/*
			//The RTC Resynchronization mode is write protected, use the
			//__HAL_RTC_WRITEPROTECTION_DISABLE() befor calling this function.
			__HAL_RTC_WRITEPROTECTION_DISABLE(&RTCHandle);
			//wait for RTC APB registers synchronisation
			HAL_RTC_WaitForSynchro(&RTCHandle);
			__HAL_RTC_WRITEPROTECTION_ENABLE(&RTCHandle);				
	 */
				
				
			__HAL_RTC_TAMPER1_DISABLE(&RTCHandle);
			__HAL_RTC_TAMPER2_DISABLE(&RTCHandle);	
				//Optionally, a tamper event can cause a timestamp to be recorded. ---P802 of RM0090
				//Timestamp on tamper event
				//With TAMPTS set to ‘1 , any tamper event causes a timestamp to occur. In this case, either
				//the TSF bit or the TSOVF bit are set in RTC_ISR, in the same manner as if a normal
				//timestamp event occurs. The affected tamper flag register (TAMP1F, TAMP2F) is set at the
				//same time that TSF or TSOVF is set. ---P802, about Tamper detection
				//-------that is why need to disable this two tamper interrupts. Before disable these two, when program start, there is always a timestamp interrupt.
				//----also, these two disable function can not be put in the TSConfig().---put there will make  the program freezed when start. the possible reason is
				//-----one the RTC is configured, changing the control register again need to lock and unlock RTC and disable write protection.---See Alarm disable/Enable 
				//---function.
				
			HAL_RTC_WaitForSynchro(&RTCHandle);	
			//To read the calendar through the shadow registers after Calendar initialization,
			//		calendar update or after wake-up from low power modes the software must first clear
			//the RSF flag. The software must then wait until it is set again before reading the
			//calendar, which means that the calendar registers have been correctly copied into the
			//RTC_TR and RTC_DR shadow registers.The HAL_RTC_WaitForSynchro() function
			//implements the above software sequence (RSF clear and RSF check).	
}


void RTC_AlarmAConfig(void)
{
	RTC_AlarmTypeDef RTC_Alarm_Structure;

	RTC_Alarm_Structure.Alarm = RTC_ALARM_A;
  RTC_Alarm_Structure.AlarmMask = RTC_ALARMMASK_ALL;
				// See reference manual. especially 
				//p11-12 of AN3371 Application Note.
				// this mask mean alarm occurs every second.
				//if MaskAll, the other 3 fieds of the AlarmStructure do not need to be initiated
				//the other three fieds are: RTC_AlarmTime(for when to occur), 
				//RTC_AlarmDateWeekDaySel (to use RTC_AlarmDateWeekDaySel_Date or RTC_AlarmDateWeekDaySel_WeekDay
				//RTC_AlarmDateWeekDay (0-31, or RTC_Weekday_Monday, RTC_Weekday_Tuesday...., depends on the value of AlarmDateWeekDaySel)	
	//RTC_Alarm_Structure.AlarmDateWeekDay = RTC_WEEKDAY_MONDAY;
  //RTC_Alarm_Structure.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
  //RTC_Alarm_Structure.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
		   //RTC_ALARMSUBSECONDMASK_ALL --> All Alarm SS fields are masked. 
        //There is no comparison on sub seconds for Alarm 
			
  //RTC_Alarm_Structure.AlarmTime.Hours = 0x02;
  //RTC_Alarm_Structure.AlarmTime.Minutes = 0x20;
  //RTC_Alarm_Structure.AlarmTime.Seconds = 0x30;
  //RTC_Alarm_Structure.AlarmTime.SubSeconds = 0x56;
  
  if(HAL_RTC_SetAlarm_IT(&RTCHandle,&RTC_Alarm_Structure,RTC_FORMAT_BCD) != HAL_OK)
  {
			LCD_DisplayString(4, 0, (uint8_t *)"Alarm setup Error!");
  }
  
	//Enable the RTC Alarm interrupt
//	__HAL_RTC_ALARM_ENABLE_IT(&RTCHandle,RTC_IT_ALRA);   //already in function HAL_RTC_SetAlarm_IT()
	
	//Enable the RTC ALARMA peripheral.
//	__HAL_RTC_ALARMA_ENABLE(&RTCHandle);  //already in function HAL_RTC_SetAlarm_IT()
	
	__HAL_RTC_ALARM_CLEAR_FLAG(&RTCHandle, RTC_FLAG_ALRAF); //need it? !!!!, without it, sometimes(SOMETIMES, when first time to use the alarm interrupt)
																			//the interrupt handler will not work!!! 		
	

	
		//need to set/enable the NVIC for RTC_Alarm_IRQn!!!!
	HAL_NVIC_EnableIRQ(RTC_Alarm_IRQn);   
	HAL_NVIC_SetPriority(RTC_Alarm_IRQn, 0x00, 0);  //not important
	
				
	
}


HAL_StatusTypeDef  RTC_AlarmA_IT_Disable(RTC_HandleTypeDef *hrtc) 
{ 
 	// Process Locked  
	__HAL_LOCK(hrtc);
  
  hrtc->State = HAL_RTC_STATE_BUSY;
  
  // Disable the write protection for RTC registers 
  __HAL_RTC_WRITEPROTECTION_DISABLE(hrtc);
  
  // __HAL_RTC_ALARMA_DISABLE(hrtc);
    
   // In case of interrupt mode is used, the interrupt source must disabled 
   __HAL_RTC_ALARM_DISABLE_IT(hrtc, RTC_IT_ALRA);


 // Enable the write protection for RTC registers 
  __HAL_RTC_WRITEPROTECTION_ENABLE(hrtc);
  
  hrtc->State = HAL_RTC_STATE_READY; 
  
  // Process Unlocked 
  __HAL_UNLOCK(hrtc);  
}


HAL_StatusTypeDef  RTC_AlarmA_IT_Enable(RTC_HandleTypeDef *hrtc) 
{	
	// Process Locked  
	__HAL_LOCK(hrtc);	
  hrtc->State = HAL_RTC_STATE_BUSY;
  
  // Disable the write protection for RTC registers 
  __HAL_RTC_WRITEPROTECTION_DISABLE(hrtc);
  
  // __HAL_RTC_ALARMA_ENABLE(hrtc);
    
   // In case of interrupt mode is used, the interrupt source must disabled 
   __HAL_RTC_ALARM_ENABLE_IT(hrtc, RTC_IT_ALRA);


 // Enable the write protection for RTC registers 
  __HAL_RTC_WRITEPROTECTION_ENABLE(hrtc);
  
  hrtc->State = HAL_RTC_STATE_READY; 
  
  // Process Unlocked 
  __HAL_UNLOCK(hrtc);  

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



/**
  * @brief EXTI line detection callbacks
  * @param GPIO_Pin: Specifies the pins connected EXTI line
  * @retval None
  */


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	
  if(GPIO_Pin == KEY_BUTTON_PIN)  //GPIO_PIN_0
  {
		buttonFlags.userPressed = 1;
		BSP_LED_Toggle(LED3);
		BSP_LED_Toggle(LED4);
  }
	
	
	if(GPIO_Pin == GPIO_PIN_1)
  {
    BSP_LED_Toggle(LED3);
		buttonFlags.ext1Pressed = 1;
			
	}  //end of PIN_1

	if(GPIO_Pin == GPIO_PIN_2)
  {
		BSP_LED_Toggle(LED4);
		buttonFlags.ext2Pressed = 1;
			
	} //end of if PIN_2	
	
	
}


void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)
{
	RTC_INT_TRIGGERED_FLAG = 1;
	BSP_LED_Toggle(LED3);
	BSP_LED_Toggle(LED4);
	
}





/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
static void Error_Handler(void)
{
  /* Turn LED4 on */
  BSP_LED_On(LED4);
	LCD_DisplayString(5,0,(uint8_t *)"Error Handler Awake!");
  while(1)
  {
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

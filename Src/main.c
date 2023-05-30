/**
  ******************************************************************************
  * @file    AccLogger/Src/main.c 
  * @author  S Darmi
  * @brief   Main program body. Acceleration data acquisition 
  ******************************************************************************
  * @attention
  *
  * 
  * All rights reserved.

  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f4_discovery.h"
#include "stm32f4_discovery_accelerometer.h"
#include "string.h"
#include "time.h"
#include "stdio.h"
#include "l3gd20.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define KEY_PRESSED     0x01
#define KEY_NOT_PRESSED 0x00

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
FATFS USBDISKFatFs;           /* File system object for USB disk logical drive */
FIL MyFile;                   /* File object */
char USBDISKPath[4];          /* USB Host logical drive path */
USBH_HandleTypeDef hUSB_Host; /* USB Host handle */

typedef enum {
  APPLICATION_IDLE = 0,  
  APPLICATION_START,    
  APPLICATION_RUNNING,
}MSC_ApplicationTypeDef;

MSC_ApplicationTypeDef Appli_state = APPLICATION_IDLE;

/* Private function prototypes -----------------------------------------------*/ 
static void SystemClock_Config(void);
static void Error_Handler(void);
static void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id);
static void MSC_Application(void);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
  /* STM32F4xx HAL library initialization:
       - Configure the Flash prefetch, instruction and Data caches
       - Configure the Systick to generate an interrupt each 1 msec
       - Set NVIC Group Priority to 4
       - Global MSP (MCU Support Package) initialization
     */

	HAL_Init();
  
	BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_EXTI);
	
  /* Configure LED4 and LED5 */
  BSP_LED_Init(LED3);
  BSP_LED_Init(LED4);
	BSP_LED_Init(LED5);
	BSP_LED_Init(LED6);
  
	/* Configure Accelerometer */
  BSP_ACCELERO_Init();
	
  /* Configure the system clock to 168 MHz */
  SystemClock_Config();
    
  /*##-1- Link the USB Host disk I/O driver ##################################*/
  if(FATFS_LinkDriver(&USBH_Driver, USBDISKPath) == 0)
  {
    /*##-2- Init Host Library ################################################*/
    USBH_Init(&hUSB_Host, USBH_UserProcess, 0);
    
    /*##-3- Add Supported Class ##############################################*/
    USBH_RegisterClass(&hUSB_Host, USBH_MSC_CLASS);
    
    /*##-4- Start Host Process ###############################################*/
    USBH_Start(&hUSB_Host);
    
    /*##-5- Run Application (Blocking mode) ##################################*/
    while (1)
    {
      /* USB Host Background task */
      USBH_Process(&hUSB_Host);

			/* Mass Storage Application State Machine */
      switch(Appli_state)
      {
      case APPLICATION_START:
        MSC_Application();
        Appli_state = APPLICATION_IDLE;
        break;
        
      case APPLICATION_IDLE:
      default:
        break;      
      }
    }
  }
  
  /* TrueStudio compilation error correction */
  while (1)
  {
  }
}

/**
  * @brief  Main routine for Mass Storage Class
  * @param  None
  * @retval None
  */
static void MSC_Application(void)
{
  FRESULT res;                                          /* FatFs function common result code */
  uint32_t byteswritten;					                      /* File write counts */
	float x, y, z;																				/* Accelerometer and gyroscope 3-axis variables */
	int16_t Buffer[3];																		/* Accelerometer buffer variables */
	char text[50];																				/* Text buffer for acceleration data */
	char file_name[50];																		/* .csv file name */
	char csv_header[50] = "x_acc;y_acc;z_acc;\n";					/* Header for .csv file */
	
	int file_number = 1;
	
	sprintf(file_name, "Acceleration%d.csv", file_number);
	int i;
	
  /* Initialize Accelerometer MEMS */
  if(BSP_ACCELERO_Init() != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler(); 
  }
    
	/* SysTick end of count event each 10ms */
	SystemCoreClock = HAL_RCC_GetHCLKFreq();
	SysTick_Config(SystemCoreClock / 100000);
    
    
	/* Waiting USER Button is pressed */
	while (BSP_PB_GetState(BUTTON_KEY) != KEY_PRESSED)
	{
		/* Toggle LED6 */
		BSP_LED_Toggle(LED6);
		HAL_Delay(10000);
	}
	
	/* Waiting USER Button is Released */
	while (BSP_PB_GetState(BUTTON_KEY) != KEY_NOT_PRESSED)
	{}
	
	/* Turn off LED3 */	
	BSP_LED_Off(LED3);

  /* Register the file system object to the FatFs module */
  if(f_mount(&USBDISKFatFs, (TCHAR const*)USBDISKPath, 0) != FR_OK)
  {
    /* FatFs Initialization Error */
    Error_Handler();
  }
  else
  {
		/* Check whether the numbered file already existed */
		for(i=1; i <= 20; ++i)
		{
			if(f_open(&MyFile, file_name, FA_READ) == FR_OK)
			{
				f_close(&MyFile);
				
				/* Continue with the next numbered file */
				file_number++;
				sprintf(file_name, "Acceleration%d.csv", file_number);
			}
			/* Break loop if the numbered file does not exist */
			if(f_open(&MyFile, file_name, FA_READ) != FR_OK)
			{
				break;
			}
			f_close(&MyFile);
		}
				
      /* Create and open a new .csv file object with write access */
      if(f_open(&MyFile, file_name, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
      {
        /* 'Acceleration.csv' file open or write error */
        Error_Handler();
      }
      else
      {
				/* Turn on LED6 */	
				BSP_LED_On(LED6);
				
				/* Create header for .csv file */
				f_write(&MyFile, csv_header, strlen(csv_header), (void *)&byteswritten);
				
				/* Waiting USER Button is pressed */
				while (BSP_PB_GetState(BUTTON_KEY) != KEY_PRESSED)
				{
					/* Read Acceleration*/
					BSP_ACCELERO_GetXYZ(Buffer);
					
					/* Assign X, Y and Z values of acceleration */
					x = Buffer[0];
					y = Buffer[1];
					z = Buffer[2];
					
					/* Put values on text */
					sprintf(text, "%6.1f;%6.1f;%6.1f;", x, y, z);
					
					/* \n at the end */
					strcat(text, "\n");
					
					/* Write data onto .csv file */
					res = f_write(&MyFile, text, strlen(text), (void *)&byteswritten);
					
					HAL_Delay(70);
				}
				
				/* Waiting USER Button is Released */
        while (BSP_PB_GetState(BUTTON_KEY) != KEY_NOT_PRESSED)
        {}

        if((byteswritten == 0) || (res != FR_OK))
        {
          /* File Write or EOF Error */
          Error_Handler();
        }
        else
        {
          /* Close the .csv file */
          f_close(&MyFile);

          /* Success of the file write: no error occurrence */
          BSP_LED_On(LED4);
					BSP_LED_Off(LED6);
				}
			}
		}
		/* Unlink the USB disk I/O driver */
		FATFS_UnLinkDriver(USBDISKPath);
}

/**
  * @brief  User Process
  * @param  phost: Host handle
  * @param  id: Host Library user message ID
  * @retval None
  */
static void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id)
{  
  switch(id)
  { 
  case HOST_USER_SELECT_CONFIGURATION:
    break;
    
  case HOST_USER_DISCONNECTION:
    Appli_state = APPLICATION_IDLE;
    BSP_LED_Off(LED4); 
    BSP_LED_Off(LED5);  
    f_mount(NULL, (TCHAR const*)"", 0);          
    break;
    
  case HOST_USER_CLASS_ACTIVE:
    Appli_state = APPLICATION_START;
    break;
    
  default:
    break; 
  }
}

/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow : 
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 168000000
  *            HCLK(Hz)                       = 168000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 4
  *            APB2 Prescaler                 = 2
  *            HSE Frequency(Hz)              = 8000000
  *            PLL_M                          = 8
  *            PLL_N                          = 336
  *            PLL_P                          = 2
  *            PLL_Q                          = 7
  *            VDD(V)                         = 3.3
  *            Main regulator output voltage  = Scale1 mode
  *            Flash Latency(WS)              = 5
  * @param  None
  * @retval None
  */
static void SystemClock_Config  (void)
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
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  HAL_RCC_OscConfig (&RCC_OscInitStruct);
  
  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;  
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;  
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

  /* STM32F405x/407x/415x/417x Revision Z devices: prefetch is supported  */
  if (HAL_GetREVID() == 0x1001)
  {
    /* Enable the Flash prefetch */
    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
  }
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
static void Error_Handler(void)
{
  /* Turn LED5 on */
  BSP_LED_On(LED5);
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

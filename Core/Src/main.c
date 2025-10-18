/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "lwrb.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/**
 * \brief           Calculate length of statically allocated array
 */
#define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))

/**
 * \brief           Ring buffer instance for TX data
 */
lwrb_t usart_tx_rb;

/**
 * \brief           Ring buffer data array for TX DMA
 */
uint8_t usart_tx_rb_data[128];

/**
 * \brief           Length of currently active TX DMA transfer
 */
volatile size_t usart_tx_dma_current_len;

/**
 * \brief           USART RX buffer for DMA to transfer every received byte
 * \note            Contains raw data that are about to be processed by different events
 */
uint8_t usart_rx_dma_buffer[64];

volatile uint8_t print_buffers = 0; /* Flag to print buffers */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void usart_rx_check(void);
void usart_process_data(const void *data, size_t len);
void usart_send_string(const char *str);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  lwrb_init(&usart_tx_rb, usart_tx_rb_data, sizeof(usart_tx_rb_data));
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  HAL_UARTEx_ReceiveToIdle_DMA(&huart1, usart_rx_dma_buffer, sizeof(usart_rx_dma_buffer)); // Start RX DMA transfer

  usart_send_string("USART DMA example: DMA HT & TC + USART IDLE LINE IRQ\r\n");
  usart_send_string("Start sending data to STM32\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    /* Reshow ring buffer */
    if (print_buffers)
    {
      print_buffers = 0;
      printf("TX Ring Buffer Data: \r\n");
      for (int i = 0; i < sizeof(usart_tx_rb_data); i++)
      {
        printf("%02X ", usart_tx_rb_data[i]);
      }
      printf("\r\n");

      printf("RX DMA Buffer Data: \r\n");
      for (int i = 0; i < sizeof(usart_rx_dma_buffer); i++)
      {
        printf("%02X ", usart_rx_dma_buffer[i]);
      }
      printf("\r\n");
    }
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/**
 * \brief           Check for new data received with DMA
 *
 * User must select context to call this function from:
 * - Only interrupts (DMA HT, DMA TC, UART IDLE) with same preemption priority level
 * - Only thread context (outside interrupts)
 *
 * If called from both context-es, exclusive access protection must be implemented
 * This mode is not advised as it usually means architecture design problems
 *
 * When IDLE interrupt is not present, application must rely only on thread context,
 * by manually calling function as quickly as possible, to make sure
 * data are read from raw buffer and processed.
 *
 * Not doing reads fast enough may cause DMA to overflow unread received bytes,
 * hence application will lost useful data.
 *
 * Solutions to this are:
 * - Improve architecture design to achieve faster reads
 * - Increase raw buffer size and allow DMA to write more data before this function is called
 */
void usart_rx_check(void)
{
  static size_t old_pos;
  size_t pos;

  /* Calculate current position in buffer and check for new data available */
  pos = ARRAY_LEN(usart_rx_dma_buffer) - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);
  if (pos != old_pos)
  { /* Check change in received data */
    if (pos > old_pos)
    { /* Current position is over previous one */
      /*
       * Processing is done in "linear" mode.
       *
       * Application processing is fast with single data block,
       * length is simply calculated by subtracting pointers
       *
       * [   0   ]
       * [   1   ] <- old_pos |------------------------------------|
       * [   2   ]            |                                    |
       * [   3   ]            | Single block (len = pos - old_pos) |
       * [   4   ]            |                                    |
       * [   5   ]            |------------------------------------|
       * [   6   ] <- pos
       * [   7   ]
       * [ N - 1 ]
       */
      usart_process_data(&usart_rx_dma_buffer[old_pos], pos - old_pos);
    }
    else
    {
      /*
       * Processing is done in "overflow" mode..
       *
       * Application must process data twice,
       * since there are 2 linear memory blocks to handle
       *
       * [   0   ]            |---------------------------------|
       * [   1   ]            | Second block (len = pos)        |
       * [   2   ]            |---------------------------------|
       * [   3   ] <- pos
       * [   4   ] <- old_pos |---------------------------------|
       * [   5   ]            |                                 |
       * [   6   ]            | First block (len = N - old_pos) |
       * [   7   ]            |                                 |
       * [ N - 1 ]            |---------------------------------|
       */
      usart_process_data(&usart_rx_dma_buffer[old_pos], ARRAY_LEN(usart_rx_dma_buffer) - old_pos);
      if (pos > 0)
      {
        usart_process_data(&usart_rx_dma_buffer[0], pos);
      }
    }
    old_pos = pos; /* Save current position as old for next transfers */
  }
}

/**
 * \brief           Check if DMA is active and if not try to send data
 * \return          `1` if transfer just started, `0` if on-going or no data to transmit
 */
uint8_t
usart_start_tx_dma_transfer(void)
{
  uint32_t primask;
  uint8_t started = 0;

  /*
   * First check if transfer is currently in-active,
   * by examining the value of usart_tx_dma_current_len variable.
   *
   * This variable is set before DMA transfer is started and cleared in DMA TX complete interrupt.
   *
   * It is not necessary to disable the interrupts before checking the variable:
   *
   * When usart_tx_dma_current_len == 0
   *    - This function is called by either application or TX DMA interrupt
   *    - When called from interrupt, it was just reset before the call,
   *         indicating transfer just completed and ready for more
   *    - When called from an application, transfer was previously already in-active
   *         and immediate call from interrupt cannot happen at this moment
   *
   * When usart_tx_dma_current_len != 0
   *    - This function is called only by an application.
   *    - It will never be called from interrupt with usart_tx_dma_current_len != 0 condition
   *
   * Disabling interrupts before checking for next transfer is advised
   * only if multiple operating system threads can access to this function w/o
   * exclusive access protection (mutex) configured,
   * or if application calls this function from multiple interrupts.
   *
   * This example assumes worst use case scenario,
   * hence interrupts are disabled prior every check
   */
  primask = __get_PRIMASK();
  __disable_irq();
  if (usart_tx_dma_current_len == 0 && (usart_tx_dma_current_len = lwrb_get_linear_block_read_length(&usart_tx_rb)) > 0)
  {
    /* Disable channel if enabled and clear all flags*/
    HAL_DMA_Abort(&hdma_usart1_tx);

    /* Prepare DMA data and length and start transfer*/
    HAL_UART_Transmit_DMA(&huart1, (uint8_t *)lwrb_get_linear_block_read_address(&usart_tx_rb), usart_tx_dma_current_len);
    started = 1;
  }
  __set_PRIMASK(primask);
  return started;
}

/**
 * \brief           Process received data over UART
 * \note            Either process them directly or copy to other bigger buffer
 * \param[in]       data: Data to process
 * \param[in]       len: Length in units of bytes
 */
void usart_process_data(const void *data, size_t len)
{
  lwrb_write(&usart_tx_rb, data, len); /* Write data to TX buffer for loopback */
  usart_start_tx_dma_transfer();       /* Then try to start transfer */
}

/**
 * \brief           Send string to USART
 * \param[in]       str: String to send
 */
void usart_send_string(const char *str)
{
  lwrb_write(&usart_tx_rb, str, strlen(str)); /* Write data to TX buffer for loopback */
  usart_start_tx_dma_transfer();              /* Then try to start transfer */
}

/* Interrupt handlers here */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  if (huart->Instance == USART1)
  {
    HAL_UART_RxEventTypeTypeDef rxEventType = HAL_UARTEx_GetRxEventType(huart);

    if (rxEventType == HAL_UART_RXEVENT_HT) /* Check half-transfer complete interrupt */
    {
      usart_rx_check();
    }
    else if (rxEventType == HAL_UART_RXEVENT_TC) /* Check transfer-complete interrupt */
    {
      usart_rx_check();
    }
    else if (rxEventType == HAL_UART_RXEVENT_IDLE) /* Check for IDLE line interrupt */
    {
      usart_rx_check();

      /* Print buffers */
      print_buffers = 1;
    }

    /* Restart DMA transfer */
    HAL_UARTEx_ReceiveToIdle_DMA(huart, usart_rx_dma_buffer, sizeof(usart_rx_dma_buffer));
  }
}

/* Check transfer-complete interrupt */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    lwrb_skip(&usart_tx_rb, usart_tx_dma_current_len); /* Skip buffer, it has been successfully sent out */
    usart_tx_dma_current_len = 0;                      /* Reset data length */
    usart_start_tx_dma_transfer();                     /* Start new transfer */
  }
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : UART komut tablosu (dispatch table) mimarisi
  *
  * Yeni komut eklemek için:
  * 1. Handler fonksiyonunu yaz  (örn: void Cmd_LedBlink(void))
  * 2. command_table[] dizisine bir satır ekle
  * Başka hiçbir yere dokunmana gerek yok.
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
    MODE_UART,
    MODE_BUTTON,
    MODE_ADC
} SystemMode_t;

#define CMDFLAG_ANY  (-1)

typedef struct {
    const char   *name;          /* UART'tan beklenen string, ör. "LEDON" */
    int           allowed_mode;  /* hangi modda aktif, ya da CMDFLAG_ANY   */
    void        (*handler)(void);/* çağrılacak fonksiyon                   */
} Command_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile uint8_t button_pressed_flag = 0;
uint32_t         last_button_time    = 0;

uint8_t  rx_byte       = 0;
char     command[32]   = {0};
uint8_t  cmd_index     = 0;
volatile uint8_t command_ready = 0;

uint16_t adcValue = 0;
char     msg[64]  = {0};

SystemMode_t currentMode = MODE_UART;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
/* Mod görev fonksiyonları */
static void Uart_Mode_Task(void);
static void Button_Mode_Task(void);
static void Adc_Mode_Task(void);

/* Komut handler'ları — her biri tabloya bağlanır */
static void Cmd_SetModeUart(void);
static void Cmd_SetModeButton(void);
static void Cmd_SetModeAdc(void);
static void Cmd_LedOn(void);
static void Cmd_LedOff(void);

/* Dispatch motoru */
static void Process_Command(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* ── KOMUT TABLOSU ────────────────────────────────────────────────────────── */
static const Command_t command_table[] = {
    /* isim     mod izni        handler         */
    { "UART",    CMDFLAG_ANY,    Cmd_SetModeUart   },
    { "BUTTON",  CMDFLAG_ANY,    Cmd_SetModeButton },
    { "ADC",     CMDFLAG_ANY,    Cmd_SetModeAdc    },
    { "LEDON",   MODE_UART,      Cmd_LedOn         },
    { "LEDOFF",  MODE_UART,      Cmd_LedOff        },
};

#define COMMAND_TABLE_SIZE  (sizeof(command_table) / sizeof(command_table[0]))
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

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */
  HAL_UART_Transmit(&huart1, (uint8_t *)"System ready\r\n", 14, 100);
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* Önce: yeni UART komutu geldiyse işle */
    if (command_ready)
    {
        Process_Command();
        command_ready = 0;
        memset(command, 0, sizeof(command));
    }

    /* Sonra: aktif moda ait görevi çalıştır */
    switch (currentMode)
    {
        case MODE_UART:   Uart_Mode_Task();   break;
        case MODE_BUTTON: Button_Mode_Task(); break;
        case MODE_ADC:    Adc_Mode_Task();    break;
    }
    /* USER CODE END WHILE */

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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/* ── Dispatch motoru ──────────────────────────────────────────────────────── */
static void Process_Command(void)
{
    for (uint8_t i = 0; i < COMMAND_TABLE_SIZE; i++)
    {
        if (strcmp(command, command_table[i].name) == 0)
        {
            /* Mod kısıtlaması var mı? */
            if (command_table[i].allowed_mode != CMDFLAG_ANY &&
                command_table[i].allowed_mode != (int)currentMode)
            {
                HAL_UART_Transmit(&huart1,
                    (uint8_t *)"Command not available in this mode\r\n", 36, 100);
                return;
            }

            /* Handler'ı çalıştır */
            command_table[i].handler();
            return;
        }
    }

    /* Hiç eşleşme bulunamadı */
    HAL_UART_Transmit(&huart1, (uint8_t *)"Unknown command\r\n", 17, 100);
}

/* ── Komut handler'ları ───────────────────────────────────────────────────── */
static void Cmd_SetModeUart(void)
{
    currentMode = MODE_UART;
    HAL_UART_Transmit(&huart1, (uint8_t *)"UART MODE\r\n", 11, 100);
}

static void Cmd_SetModeButton(void)
{
    currentMode = MODE_BUTTON;
    HAL_UART_Transmit(&huart1, (uint8_t *)"BUTTON MODE\r\n", 13, 100);
}

static void Cmd_SetModeAdc(void)
{
    currentMode = MODE_ADC;
    HAL_UART_Transmit(&huart1, (uint8_t *)"ADC MODE\r\n", 10, 100);
}

static void Cmd_LedOn(void)
{
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); /* aktif-low LED */
    HAL_UART_Transmit(&huart1, (uint8_t *)"LED ON\r\n", 8, 100);
}

static void Cmd_LedOff(void)
{
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    HAL_UART_Transmit(&huart1, (uint8_t *)"LED OFF\r\n", 9, 100);
}

/* ── Mod görev fonksiyonları ──────────────────────────────────────────────── */
static void Uart_Mode_Task(void)
{
    /* Mod periyodik işleri buraya yazılabilir */
}

static void Button_Mode_Task(void)
{
    if (button_pressed_flag)
    {
        button_pressed_flag = 0;
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    }
}

static void Adc_Mode_Task(void)
{
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 100);
    adcValue = HAL_ADC_GetValue(&hadc1);

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13,
                      (adcValue < 2048) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    sprintf(msg, "ADC: %u\r\n", adcValue);
    HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), 100);
}

/* ── Interrupt callback'leri ──────────────────────────────────────────────── */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_0)
    {
        if ((HAL_GetTick() - last_button_time) > 200)
        {
            button_pressed_flag = 1;
            last_button_time    = HAL_GetTick();
        }
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) return;

    if (rx_byte != '\n' && rx_byte != '\r')
    {
        if (cmd_index < sizeof(command) - 1)   /* taşma koruması */
        {
            command[cmd_index++] = rx_byte;
        }
    }
    else
    {
        command[cmd_index] = '\0';
        if (cmd_index > 0)                     /* boş satırı yoksay */
        {
            command_ready = 1;
        }
        cmd_index = 0;
    }

    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  * where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

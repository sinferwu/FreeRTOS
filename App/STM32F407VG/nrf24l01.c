/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
/* Library includes. */
#include "stm32f4xx.h"
#include "stm32f4xx_hal_conf.h"
#include "stm32f4xx_hal.h"
#include "nrf24l01.h"
#include "stm324xg_eval.h"

#include "serial.h"

#define TX_SEND

const uint8_t nRF_TX_ADDRESS[nRF_TX_ADR_WIDTH] = {0x59, 0x12, 0x67, 0x67, 0x25};
const uint8_t nRF_RX_ADDRESS[nRF_RX_ADR_WIDTH] = {0x59, 0x12, 0x67, 0x67, 0x25};
static nRF_Tx_DataType nRF_Tx_Buf, nRF_Rx_Buf;
EventGroupHandle_t xEventGruop;

uint8_t nRF_Check(void)
{
	uint8_t buf[5] = {0xA5, 0xA5, 0xA5, 0xA5, 0xA5};
	uint8_t i;

	nRF_SPI_IO_WriteData(nRF_WRITE_REG+nRF_TX_ADDR, buf, 5);
	nRF_SPI_IO_ReadData(nRF_TX_ADDR, buf, 5);

	for (i = 0; i < 5; ++i)
		if(buf[i] != 0xA5)
			return HAL_ERROR;
	return HAL_OK;
}

void nRF_RX_Mode(void)
{
	/*!< Select the nRF: Chip Select low */
	nRF_SPI_CS_LOW();

	nRF_SPI_IO_WriteData(nRF_WRITE_REG+nRF_TX_ADDR, (const uint8_t *)nRF_TX_ADDRESS, nRF_TX_ADR_WIDTH);
	nRF_SPI_IO_WriteData(nRF_WRITE_REG+nRF_RX_ADDR_P0, (const uint8_t *)nRF_RX_ADDRESS, nRF_RX_ADR_WIDTH);

	nRF_SPI_IO_WriteReg(nRF_WRITE_REG+nRF_EN_AA, 0x01);
	nRF_SPI_IO_WriteReg(nRF_WRITE_REG+nRF_EN_RXADDR, 0x01);
	nRF_SPI_IO_WriteReg(nRF_WRITE_REG+nRF_RF_CH, 40);
	nRF_SPI_IO_WriteReg(nRF_WRITE_REG+nRF_RX_PW_P0, nRF_RX_PLOAD_WIDTH);
	nRF_SPI_IO_WriteReg(nRF_WRITE_REG+nRF_RF_SETUP, 0x0F);
	nRF_SPI_IO_WriteReg(nRF_WRITE_REG+nRF_CONFIG, 0x0F);
	
	/*!< Deselect the nRF: Chip Select high */
	nRF_SPI_CS_HIGH();
}


void nRF_TX_Mode(void)
{
	/*!< Select the nRF: Chip Select low */
	nRF_CSN_LOW();

	nRF_SPI_IO_WriteData(nRF_WRITE_REG+nRF_TX_ADDR, (const uint8_t *)nRF_TX_ADDRESS, nRF_TX_ADR_WIDTH);
	nRF_SPI_IO_WriteData(nRF_WRITE_REG+nRF_RX_ADDR_P0, (const uint8_t *)nRF_RX_ADDRESS, nRF_RX_ADR_WIDTH);

	nRF_SPI_IO_WriteReg(nRF_WRITE_REG+nRF_EN_AA, 0x01);
	nRF_SPI_IO_WriteReg(nRF_WRITE_REG+nRF_EN_RXADDR, 0x01);
	nRF_SPI_IO_WriteReg(nRF_WRITE_REG+nRF_SETUP_RETR, 0x0A);
	nRF_SPI_IO_WriteReg(nRF_WRITE_REG+nRF_RF_CH, 40);
	nRF_SPI_IO_WriteReg(nRF_WRITE_REG+nRF_RF_SETUP, 0x0F);
	nRF_SPI_IO_WriteReg(nRF_WRITE_REG+nRF_RX_PW_P0, nRF_RX_PLOAD_WIDTH);
	nRF_SPI_IO_WriteReg(nRF_WRITE_REG+nRF_CONFIG, 0x0E);
	
	/*!< Deselect the nRF: Chip Select high */
	nRF_CSN_HIGH();
	
}

/**
  * @brief ���������жϣ������жϷ�������
  * @param  None
  * retval   None
  */
uint8_t nRF_Start_Tx(void)
{
	BaseType_t uxBits;
	const TickType_t xTicksToWait = 3;		// Time Out 3ms
	
	// Entry TX Mode to Send Data
	nRF_TX_Mode();
	
	/*!< Select the nRF: Chip Select low */
	nRF_CSN_LOW();
	
	nRF_SPI_IO_WriteReg(nRF_FLUSH_TX, 0x00);

	nRF_SPI_IO_WriteData(nRF_WR_TX_PLOAD, (uint8_t *)&nRF_Tx_Buf, nRF_TX_PLOAD_WIDTH);
	
	/*!< Deselect the nRF: Start Send */
	nRF_CSN_HIGH();

	uxBits = xEventGroupWaitBits(xEventGruop, nRF_State_TX_OK|nRF_State_TX_MAX, pdTRUE, pdFALSE, xTicksToWait);
	if (uxBits & nRF_State_TX_OK)
	{
		nRF_RX_Mode();
		return nRF_TX_OK;
	}
	else if ( uxBits & nRF_State_TX_MAX)
	{
		nRF_CSN_LOW();
		nRF_SPI_IO_WriteReg(nRF_FLUSH_TX, 0x00);
		nRF_CSN_HIGH();
		return nRF_MAX_TX;
	}
	else
	{
		nRF_CSN_LOW();
		nRF_SPI_IO_WriteReg(nRF_FLUSH_TX, 0x00);
		nRF_CSN_HIGH();
		return nRF_TIMEOUT;
	}
}

uint8_t nRF_Start_Rx(void)
{
	BaseType_t uxBits;
	const TickType_t xTickToWait = 3;		// Time Out 3ms

	uxBits = xEventGroupWaitBits(xEventGruop, nRF_State_RX_OK, pdTRUE, pdFALSE, xTickToWait);

	if (uxBits & nRF_State_RX_OK)
	{
		nRF_TX_Mode();
		return nRF_RX_OK;
	}
	else
	{
		return nRF_TIMEOUT;
	}
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	BaseType_t xResult, xHigherPriorityTaskWoken;
	uint8_t RegValue;

	RegValue = nRF_SPI_IO_ReadReg(nRF_STATUS);
	nRF_SPI_IO_WriteReg(nRF_WRITE_REG+nRF_STATUS, RegValue);
	
	xHigherPriorityTaskWoken = pdFALSE;
	
	if (RegValue & nRF_TX_OK)
	{
		xResult = xEventGroupSetBitsFromISR(xEventGruop, nRF_State_TX_OK, &xHigherPriorityTaskWoken);
	}
	else if(RegValue & nRF_MAX_TX)
	{
		xResult = xEventGroupSetBitsFromISR(xEventGruop, nRF_State_TX_MAX, &xHigherPriorityTaskWoken);
	}
	if (RegValue & nRF_RX_OK)
	{
		nRF_SPI_IO_ReadData(nRF_RD_RX_PLOAD, (uint8_t *)&nRF_Rx_Buf, nRF_RX_PLOAD_WIDTH);
		nRF_CSN_LOW();
		nRF_SPI_IO_WriteReg(nRF_FLUSH_RX, 0x00);
		nRF_CSN_HIGH();
		xResult = xEventGroupSetBitsFromISR(xEventGruop, nRF_State_RX_OK, &xHigherPriorityTaskWoken);
	}
	
	if (xResult != pdFAIL)
	{
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}


/* The task that is created three times. */
static portTASK_FUNCTION_PROTO( vnRFTask, pvParameters );

/*-----------------------------------------------------------*/

void vStartnRFTasks( UBaseType_t uxPriority )
{
	xTaskCreate( vnRFTask, "nRF", configMINIMAL_STACK_SIZE, NULL, uxPriority, ( TaskHandle_t * ) NULL );
}
/*-----------------------------------------------------------*/

static portTASK_FUNCTION( vnRFTask, pvParameters )
{
TickType_t xRate, xLastTime;
uint8_t	retValue = 'A';

	/* The parameters are not used. */
	( void ) pvParameters;

	nRF_SPI_IO_Init();
	xRate = 300;
	/* We will turn the LED on and off again in the delay period, so each
	delay is only half the total period. */
	xRate /= ( TickType_t ) 2;

	/* �����¼���ͬ���ж�������*/
	xEventGruop = xEventGroupCreate();

	/* We need to initialise xLastFlashTime prior to the first call to 
	vTaskDelayUntil(). */
	xLastTime = xTaskGetTickCount();

	for(;;)
	{
		if (xEventGruop == NULL)
		{
			xEventGruop = xEventGroupCreate();
			continue;
		}

		nRF_Tx_Buf.datatype = 'T';
		memcpy(&nRF_Tx_Buf.data, "x Send  \n", 9);
		nRF_Tx_Buf.data[7] = retValue;
		retValue++;
		if (retValue > 'Z')
			retValue = 'A';
		if (nRF_Start_Tx() == nRF_TX_OK)
		{
			BSP_LED_Toggle(LED2);
			if (nRF_Start_Rx() == nRF_RX_OK)
			{
				BSP_LED_Toggle(LED3);
				UART_PutString((uint8_t *)&nRF_Rx_Buf, 10);
			}
		}
		
		vTaskDelayUntil( &xLastTime, xRate );
	}
} /*lint !e715 !e818 !e830 Function definition must be standard for task creation. */

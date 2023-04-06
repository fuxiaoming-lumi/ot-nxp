/*
 * Copyright 2021-2022 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "wifi_bt_config.h"
#include "pin_mux.h"
#include "fsl_gpio.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#if defined(WIFI_IW612_BOARD_RD_USD) || defined(WIFI_IW612_BOARD_MURATA_2EL_USD) || \
    defined(WIFI_IW611_BOARD_MURATA_2DL_USD) || defined(WIFI_AW611_BOARD_UBX_JODY_W5_USD)
#define CONTROLLER_RESET_GPIO GPIO3
#define CONTROLLER_RESET_PIN  9U
#endif
/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/

void BOARD_WIFI_BT_Enable(bool enable)
{
    if (enable)
    {
        /* Enable module */
#ifdef WIFI_BT_USE_M2_INTERFACE
        /* Set SDIO_RST to 1 */
        GPIO_PortSet(BOARD_INITPINSM2_SDIO_RST_GPIO, BOARD_INITPINSM2_SDIO_RST_GPIO_PIN_MASK);
        vTaskDelay(pdMS_TO_TICKS(100));

        /* Set WL_RST to 1 */
        GPIO_PortSet(BOARD_INITPINSM2_WL_RST_GPIO, BOARD_INITPINSM2_WL_RST_GPIO_PIN_MASK);
        vTaskDelay(pdMS_TO_TICKS(100));
#elif defined(WIFI_BT_USE_USD_INTERFACE)
        /* Enable power supply for SD */
        GPIO_PinWrite(CONTROLLER_RESET_GPIO, CONTROLLER_RESET_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
#endif /* WIFI_BT_USE_M2_INTERFACE */
    }
    else
    {
        /* Disable module */
#ifdef WIFI_BT_USE_M2_INTERFACE
        /* Set WL_RST to 0 */
        GPIO_PortClear(BOARD_INITPINSM2_WL_RST_GPIO, BOARD_INITPINSM2_WL_RST_GPIO_PIN_MASK);
        /* Set SDIO_RST to 0 */
        GPIO_PortClear(BOARD_INITPINSM2_SDIO_RST_GPIO, BOARD_INITPINSM2_SDIO_RST_GPIO_PIN_MASK);
#elif defined(WIFI_BT_USE_USD_INTERFACE)
        /* Disable power supply for SD */
        GPIO_PinWrite(CONTROLLER_RESET_GPIO, CONTROLLER_RESET_PIN, 0);
#endif /* WIFI_BT_USE_M2_INTERFACE */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void BOARD_WIFI_BT_Config(void *card, sdio_int_t cardInt)
{
    BOARD_SDIO_Config(card, NULL, BOARD_SDMMC_SDIO_HOST_IRQ_PRIORITY, cardInt);

#ifdef WIFI_BT_USE_M2_INTERFACE
    ((sdio_card_t *)card)->usrParam.pwr = NULL;
    BOARD_InitPinsM2();
#endif
    BOARD_WIFI_BT_Enable(false);
}

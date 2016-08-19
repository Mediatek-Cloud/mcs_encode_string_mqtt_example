/* Copyright Statement:
 *
 * (C) 2005-2016  MediaTek Inc. All rights reserved.
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. ("MediaTek") and/or its licensors.
 * Without the prior written permission of MediaTek and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 * You may only use, reproduce, modify, or distribute (as applicable) MediaTek Software
 * if you have agreed to and been bound by the applicable license agreement with
 * MediaTek ("License Agreement") and been granted explicit permission to do so within
 * the License Agreement ("Permitted User").  If you are not a Permitted User,
 * please cease any access or use of MediaTek Software immediately.
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT MEDIATEK SOFTWARE RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES
 * ARE PROVIDED TO RECEIVER ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "sys_init.h"
#include "lwip_network.h"
#include "wifi_api.h"
#if defined(MTK_MINICLI_ENABLE)
#include "cli_def.h"
#endif
#include "bsp_gpio_ept_config.h"
#include "app_common.h"
#include "nvdm.h"
#include "wifi_api.h"
#include "lwip/ip4_addr.h"
#include "lwip/inet.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "ethernetif.h"
#include "mcs.h"

#define deviceId "Input your deviceId"
#define deviceKey "Input your deviceKey"
#define Ssid "Input your wifi Ssid"
#define Password "Input your wifi password"
#define topic "mcs/{Input your deviceId}/{Input your deviceKey}/+"
#define host "com"
#define server "mqtt.mcs.mediatek.com"
#define port "1883"
#define clientId "mt7687"

/* md5 module */
#include "hal_md5.h"
#define MAX_DATA_SIZE 1024

/* mcs setting */
#define ENCODE_MD5_CHANNEL "encodeByMD5"
#define DECODE_MD5_CHANNEL "decodeByMD5"

static SemaphoreHandle_t ip_ready;

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */

wifi_config_t wifi_config = {0};
lwip_tcpip_config_t tcpip_config = {{0}, {0}, {0}, {0}, {0}, {0}};

static int32_t _wifi_event_handler(wifi_event_t event,
        uint8_t *payload,
        uint32_t length)
{
    struct netif *sta_if;
    LOG_I(common, "wifi event: %d", event);

    switch(event)
    {
    case WIFI_EVENT_IOT_PORT_SECURE:
        sta_if = netif_find_by_type(NETIF_TYPE_STA);
        netif_set_link_up(sta_if);
        LOG_I(common, "wifi connected");
        break;
    case WIFI_EVENT_IOT_DISCONNECTED:
        sta_if = netif_find_by_type(NETIF_TYPE_STA);
        netif_set_link_down(sta_if);
        LOG_I(common, "wifi disconnected");
        break;
    }

    return 1;
}

void mcs_mqtt_callback(char *rcv_buf) {

    char *arr[7];
    char *del = ",";
    mcs_split(arr, rcv_buf, del);
    // Dln7lL0G,zLfxhiabFnCEZZJc,1459307476444,encodeByMD5,test
    if (0 == strncmp (arr[3], ENCODE_MD5_CHANNEL, strlen(ENCODE_MD5_CHANNEL))) {
        /* encode BY MD5 */
        uint8_t digest[HAL_MD5_DIGEST_SIZE] = {0};
        printf("User give: %s \n", arr[4]);
        hal_md5_context_t context = {0};
        hal_md5_init(&context);
        hal_md5_append(&context, arr[4], strlen(arr[4]));
        hal_md5_end(&context, digest);

        uint8_t i;
        char str_buffer [50] = {0};
        strcpy(str_buffer, "");
        for (i = 0; i < sizeof(digest); i++) {
          if (i % 16 == 0) {
              printf("\r\n");
          }
          char buffer [2];
          sprintf (buffer, "%02x", digest[i]);
          strcat(str_buffer, buffer);
        }

        /* send to MCS */
        char data_buf [MAX_DATA_SIZE] = {0};
        strcat(data_buf, DECODE_MD5_CHANNEL);
        strcat(data_buf, ",,");
        strcat(data_buf, str_buffer);
        mcs_upload_datapoint(data_buf);
    }
}

static void _ip_ready_callback(struct netif *netif)
{
    if (!ip4_addr_isany_val(netif->ip_addr)) {
        char ip_addr[17] = {0};
        if (NULL != inet_ntoa(netif->ip_addr)) {
            strcpy(ip_addr, inet_ntoa(netif->ip_addr));
            LOG_I(common, "************************");
            LOG_I(common, "DHCP got IP:%s", ip_addr);
            LOG_I(common, "************************");
        } else {
            LOG_E(common, "DHCP got Failed");
        }
    }
    xSemaphoreGive(ip_ready);
    LOG_I(common, "ip ready");
}

void wifi_initial_task() {
  struct netif *sta_if;
  wifi_init(&wifi_config, NULL);
  lwip_tcpip_init(&tcpip_config, WIFI_MODE_STA_ONLY);

  ip_ready = xSemaphoreCreateBinary();

  sta_if = netif_find_by_type(NETIF_TYPE_STA);
  netif_set_status_callback(sta_if, _ip_ready_callback);
  dhcp_start(sta_if);

  xSemaphoreTake(ip_ready, portMAX_DELAY);
  mcs_mqtt_init(mcs_mqtt_callback);
  vTaskDelete(NULL);
}


int main(void)
{
    /* Do system initialization, eg: hardware, nvdm, logging and random seed. */
    system_init();

    /* bsp_ept_gpio_setting_init() under driver/board/mt76x7_hdk/ept will initialize the GPIO settings
     * generated by easy pinmux tool (ept). ept_*.c and ept*.h are the ept files and will be used by
     * bsp_ept_gpio_setting_init() for GPIO pinumux setup.
     */
    bsp_ept_gpio_setting_init();

    int nvdm_deviceKey_len = sizeof(deviceKey);
    int nvdm_deviceId_len = sizeof(deviceId);
    int nvdm_host_len = sizeof(host);
    int nvdm_clientId_len = sizeof(clientId);
    int nvdm_server_len = sizeof(server);
    int nvdm_port_len = sizeof(port);
    int nvdm_topic_len = sizeof(topic);

    nvdm_write_data_item("common", "deviceId", NVDM_DATA_ITEM_TYPE_STRING, (uint8_t *)deviceId, nvdm_deviceId_len);
    nvdm_write_data_item("common", "deviceKey", NVDM_DATA_ITEM_TYPE_STRING, (uint8_t *)deviceKey, nvdm_deviceKey_len);
    nvdm_write_data_item("common", "topic", NVDM_DATA_ITEM_TYPE_STRING, (uint8_t *)topic, nvdm_topic_len);
    nvdm_write_data_item("common", "host", NVDM_DATA_ITEM_TYPE_STRING, (uint8_t *)host, nvdm_host_len);
    nvdm_write_data_item("common", "clientId", NVDM_DATA_ITEM_TYPE_STRING, (uint8_t *)clientId, nvdm_clientId_len);
    nvdm_write_data_item("common", "server", NVDM_DATA_ITEM_TYPE_STRING, (uint8_t *)server, nvdm_server_len);
    nvdm_write_data_item("common", "port", NVDM_DATA_ITEM_TYPE_STRING, (uint8_t *)port, nvdm_port_len);

    wifi_connection_register_event_handler(WIFI_EVENT_IOT_INIT_COMPLETE , _wifi_event_handler);
    wifi_connection_register_event_handler(WIFI_EVENT_IOT_CONNECTED, _wifi_event_handler);
    wifi_connection_register_event_handler(WIFI_EVENT_IOT_PORT_SECURE, _wifi_event_handler);
    wifi_connection_register_event_handler(WIFI_EVENT_IOT_DISCONNECTED, _wifi_event_handler);

    wifi_config.opmode = WIFI_MODE_STA_ONLY;

    strcpy((char *)wifi_config.sta_config.ssid, Ssid);
    wifi_config.sta_config.ssid_length = strlen(Ssid);
    strcpy((char *)wifi_config.sta_config.password, Password);
    wifi_config.sta_config.password_length = strlen(Password);

    xTaskCreate(wifi_initial_task, "User app", 1024, NULL, 1, NULL);

    /* Initialize cli task to enable user input cli command from uart port.*/
#if defined(MTK_MINICLI_ENABLE)
    cli_def_create();
    cli_task_create();
#endif

    vTaskStartScheduler();

    /* If all is well, the scheduler will now be running, and the following line
    will never be reached.  If the following line does execute, then there was
    insufficient FreeRTOS heap memory available for the idle and/or timer tasks
    to be created.  See the memory management section on the FreeRTOS web site
    for more details. */
    for ( ;; );
}
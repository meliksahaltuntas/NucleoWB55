/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    p2p_server_app.c
  * @author  MCD Application Team
  * @brief   Peer to peer Server Application
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2019-2021 STMicroelectronics.
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
#include "app_common.h"
#include "dbg_trace.h"
#include "ble.h"
#include "p2p_server_app.h"
#include "stm32_seq.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
 typedef struct{
    uint8_t             Device_Led_Selection;
    uint8_t             Led1;
 }P2P_LedCharValue_t;

 typedef struct{
    uint8_t             Device_Button_Selection;
    uint8_t             ButtonStatus;
 }P2P_ButtonCharValue_t;

typedef struct
{
  uint8_t               Notification_Status; /* used to check if P2P Server is enabled to Notify */
  P2P_LedCharValue_t    LedControl;
  P2P_ButtonCharValue_t ButtonControl;
  uint16_t              ConnectionHandle;
} P2P_Server_App_Context_t;
/* USER CODE END PTD */

/* Private defines ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macros -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
// 30-byte veri transferi için değişkenler
static uint8_t ServerData[30];
static uint32_t DataGenerationCount = 0;
static uint8_t DataTransferComplete = 0;

// P2P Server App Context - EKSIK OLAN TANIMLAMA
static P2P_Server_App_Context_t P2P_Server_App_Context;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static void P2PS_Send_Notification(void);
static void P2PS_APP_LED_BUTTON_context_Init(void);
void P2PS_Send_30ByteData(void); // GLOBAL FUNCTION - static değil
/* USER CODE END PFP */

/* Functions Definition ------------------------------------------------------*/
void P2PS_STM_App_Notification(P2PS_STM_App_Notification_evt_t *pNotification)
{
/* USER CODE BEGIN P2PS_STM_App_Notification_1 */

/* USER CODE END P2PS_STM_App_Notification_1 */
  switch(pNotification->P2P_Evt_Opcode)
  {
/* USER CODE BEGIN P2PS_STM_App_Notification_P2P_Evt_Opcode */
#if(BLE_CFG_OTA_REBOOT_CHAR != 0)
    case P2PS_STM_BOOT_REQUEST_EVT:
      APP_DBG_MSG("-- P2P APPLICATION SERVER : BOOT REQUESTED\n");
      APP_DBG_MSG(" \n\r");

      *(uint32_t*)SRAM1_BASE = *(uint32_t*)pNotification->DataTransfered.pPayload;
      NVIC_SystemReset();
      break;
#endif
/* USER CODE END P2PS_STM_App_Notification_P2P_Evt_Opcode */

    case P2PS_STM_WRITE_EVT:
    /* USER CODE BEGIN P2PS_STM_WRITE_EVT */

    // ÖNCELİK: Client'tan gelen DATA CONFIRMATION kontrol et
    if(pNotification->DataTransfered.Length == 2 &&
       pNotification->DataTransfered.pPayload[0] == 0xDE &&
       pNotification->DataTransfered.pPayload[1] == 0xAD) {

        APP_DBG_MSG("=== SERVER: CLIENT CONFIRMED DATA RECEIVED ===\n");
        APP_DBG_MSG("Client successfully received 30-byte data\n");
        APP_DBG_MSG("Clearing data and preparing disconnect...\n");

        // Server veriyi temizle
        memset(ServerData, 0, 30);
        APP_DBG_MSG("Server data cleared, generation count: %lu\n", DataGenerationCount);

        // P2P context'ten connection handle al
        if (P2P_Server_App_Context.ConnectionHandle != 0) {
            APP_DBG_MSG("Disconnecting client with handle: 0x%04X\n", P2P_Server_App_Context.ConnectionHandle);

            // 1 saniye sonra client disconnect et
            HAL_Delay(1000);
            aci_gap_terminate(P2P_Server_App_Context.ConnectionHandle, 0x13);

            APP_DBG_MSG("Client disconnection initiated\n");
        } else {
            APP_DBG_MSG("WARNING: No valid connection handle found\n");
        }

        return; // Fonksiyondan çık
    }

    // ESKİ P2P LED CONTROL LOGIC
    if(pNotification->DataTransfered.pPayload[0] == 0x00){
        if(pNotification->DataTransfered.pPayload[1] == 0x01)
        {
            BSP_LED_On(LED_BLUE);
            APP_DBG_MSG("-- P2P APPLICATION SERVER  : LED1 ON\n");
            P2P_Server_App_Context.LedControl.Led1=0x01;
        }
        if(pNotification->DataTransfered.pPayload[1] == 0x00)
        {
            BSP_LED_Off(LED_BLUE);
            APP_DBG_MSG("-- P2P APPLICATION SERVER  : LED1 OFF\n");
            P2P_Server_App_Context.LedControl.Led1=0x00;
        }
    }

    #if(P2P_SERVER1 != 0)
    if(pNotification->DataTransfered.pPayload[0] == 0x01){
        if(pNotification->DataTransfered.pPayload[1] == 0x01)
        {
            BSP_LED_On(LED_BLUE);
            APP_DBG_MSG("-- P2P APPLICATION SERVER 1 : LED1 ON\n");
            P2P_Server_App_Context.LedControl.Led1=0x01;
        }
        if(pNotification->DataTransfered.pPayload[1] == 0x00)
        {
            BSP_LED_Off(LED_BLUE);
            APP_DBG_MSG("-- P2P APPLICATION SERVER 1 : LED1 OFF\n");
            P2P_Server_App_Context.LedControl.Led1=0x00;
        }
    }
    #endif

    #if(P2P_SERVER2 != 0)
    if(pNotification->DataTransfered.pPayload[0] == 0x02){
        if(pNotification->DataTransfered.pPayload[1] == 0x01)
        {
            BSP_LED_On(LED_BLUE);
            APP_DBG_MSG("-- P2P APPLICATION SERVER 2 : LED1 ON\n");
            P2P_Server_App_Context.LedControl.Led1=0x01;
        }
        if(pNotification->DataTransfered.pPayload[1] == 0x00)
        {
            BSP_LED_Off(LED_BLUE);
            APP_DBG_MSG("-- P2P APPLICATION SERVER 2 : LED1 OFF\n");
            P2P_Server_App_Context.LedControl.Led1=0x00;
        }
    }
    #endif

    #if(P2P_SERVER3 != 0)
    if(pNotification->DataTransfered.pPayload[0] == 0x03){
        if(pNotification->DataTransfered.pPayload[1] == 0x01)
        {
            BSP_LED_On(LED_BLUE);
            APP_DBG_MSG("-- P2P APPLICATION SERVER 3 : LED1 ON\n");
            P2P_Server_App_Context.LedControl.Led1=0x01;
        }
        if(pNotification->DataTransfered.pPayload[1] == 0x00)
        {
            BSP_LED_Off(LED_BLUE);
            APP_DBG_MSG("-- P2P APPLICATION SERVER 3 : LED1 OFF\n");
            P2P_Server_App_Context.LedControl.Led1=0x00;
        }
    }
    #endif

    #if(P2P_SERVER4 != 0)
    if(pNotification->DataTransfered.pPayload[0] == 0x04){
        if(pNotification->DataTransfered.pPayload[1] == 0x01)
        {
            BSP_LED_On(LED_BLUE);
            APP_DBG_MSG("-- P2P APPLICATION SERVER 4 : LED1 ON\n");
            P2P_Server_App_Context.LedControl.Led1=0x01;
        }
        if(pNotification->DataTransfered.pPayload[1] == 0x00)
        {
            BSP_LED_Off(LED_BLUE);
            APP_DBG_MSG("-- P2P APPLICATION SERVER 4 : LED1 OFF\n");
            P2P_Server_App_Context.LedControl.Led1=0x00;
        }
    }
    #endif

    #if(P2P_SERVER5 != 0)
    if(pNotification->DataTransfered.pPayload[0] == 0x05){
        if(pNotification->DataTransfered.pPayload[1] == 0x01)
        {
            BSP_LED_On(LED_BLUE);
            APP_DBG_MSG("-- P2P APPLICATION SERVER 5 : LED1 ON\n");
            P2P_Server_App_Context.LedControl.Led1=0x01;
        }
        if(pNotification->DataTransfered.pPayload[1] == 0x00)
        {
            BSP_LED_Off(LED_BLUE);
            APP_DBG_MSG("-- P2P APPLICATION SERVER 5 : LED1 OFF\n");
            P2P_Server_App_Context.LedControl.Led1=0x00;
        }
    }
    #endif

    #if(P2P_SERVER6 != 0)
    if(pNotification->DataTransfered.pPayload[0] == 0x06){
        if(pNotification->DataTransfered.pPayload[1] == 0x01)
        {
            BSP_LED_On(LED_BLUE);
            APP_DBG_MSG("-- P2P APPLICATION SERVER 6 : LED1 ON\n");
            P2P_Server_App_Context.LedControl.Led1=0x01;
        }
        if(pNotification->DataTransfered.pPayload[1] == 0x00)
        {
            BSP_LED_Off(LED_BLUE);
            APP_DBG_MSG("-- P2P APPLICATION SERVER 6 : LED1 OFF\n");
            P2P_Server_App_Context.LedControl.Led1=0x00;
        }
    }
    #endif

    /* USER CODE END P2PS_STM_WRITE_EVT */
    break;

    case P2PS_STM__NOTIFY_ENABLED_EVT:
/* USER CODE BEGIN P2PS_STM__NOTIFY_ENABLED_EVT */
      P2P_Server_App_Context.Notification_Status = 1;
      APP_DBG_MSG("-- P2P APPLICATION SERVER : NOTIFICATION ENABLED\n");

      // Notification aktif olunca veri gönder
      APP_DBG_MSG("P2P notifications enabled, starting data transmission...\n");
      HAL_Delay(1000);
      P2PS_Send_30ByteData();
/* USER CODE END P2PS_STM__NOTIFY_ENABLED_EVT */
      break;

    case P2PS_STM_NOTIFY_DISABLED_EVT:
/* USER CODE BEGIN P2PS_STM_NOTIFY_DISABLED_EVT */
      P2P_Server_App_Context.Notification_Status = 0;
      APP_DBG_MSG("-- P2P APPLICATION SERVER : NOTIFICATION DISABLED\n");
/* USER CODE END P2PS_STM_NOTIFY_DISABLED_EVT */
      break;

    default:
/* USER CODE BEGIN P2PS_STM_App_Notification_default */
      
/* USER CODE END P2PS_STM_App_Notification_default */
      break;
  }
/* USER CODE BEGIN P2PS_STM_App_Notification_2 */

/* USER CODE END P2PS_STM_App_Notification_2 */
  return;
}

void P2PS_APP_Notification(P2PS_APP_ConnHandle_Not_evt_t *pNotification)
{
/* USER CODE BEGIN P2PS_APP_Notification_1 */

/* USER CODE END P2PS_APP_Notification_1 */
  switch(pNotification->P2P_Evt_Opcode)
  {
/* USER CODE BEGIN P2PS_APP_Notification_P2P_Evt_Opcode */

/* USER CODE END P2PS_APP_Notification_P2P_Evt_Opcode */
  case PEER_CONN_HANDLE_EVT :
  /* USER CODE BEGIN PEER_CONN_HANDLE_EVT */
  P2P_Server_App_Context.ConnectionHandle = pNotification->ConnectionHandle;
  APP_DBG_MSG("P2P Server: Connection handle saved: 0x%04X\n", pNotification->ConnectionHandle);
  /* USER CODE END PEER_CONN_HANDLE_EVT */
  break;
  case PEER_DISCON_HANDLE_EVT :
  /* USER CODE BEGIN PEER_DISCON_HANDLE_EVT */
  P2PS_APP_LED_BUTTON_context_Init();
  P2P_Server_App_Context.ConnectionHandle = 0; // Reset connection handle
  APP_DBG_MSG("P2P Server: Connection handle reset\n");
  /* USER CODE END PEER_DISCON_HANDLE_EVT */
  break;
    default:
/* USER CODE BEGIN P2PS_APP_Notification_default */

/* USER CODE END P2PS_APP_Notification_default */
      break;
  }
/* USER CODE BEGIN P2PS_APP_Notification_2 */

/* USER CODE END P2PS_APP_Notification_2 */
  return;
}

void P2PS_APP_Init(void)
{
/* USER CODE BEGIN P2PS_APP_Init */
  UTIL_SEQ_RegTask( 1<< CFG_TASK_SW1_BUTTON_PUSHED_ID, UTIL_SEQ_RFU, P2PS_Send_Notification );

  /**
   * Initialize LedButton Service
   */
  P2P_Server_App_Context.Notification_Status=0; 
  P2PS_APP_LED_BUTTON_context_Init();
/* USER CODE END P2PS_APP_Init */
  return;
}

/* USER CODE BEGIN FD */
void P2PS_APP_LED_BUTTON_context_Init(void){
  
  BSP_LED_Off(LED_BLUE);
  APP_DBG_MSG("LED BLUE OFF\n");
  
  #if(P2P_SERVER1 != 0)
  P2P_Server_App_Context.LedControl.Device_Led_Selection=0x01; /* Device1 */
  P2P_Server_App_Context.LedControl.Led1=0x00; /* led OFF */
  P2P_Server_App_Context.ButtonControl.Device_Button_Selection=0x01;/* Device1 */
  P2P_Server_App_Context.ButtonControl.ButtonStatus=0x00;
#endif
#if(P2P_SERVER2 != 0)
  P2P_Server_App_Context.LedControl.Device_Led_Selection=0x02; /* Device2 */
  P2P_Server_App_Context.LedControl.Led1=0x00; /* led OFF */
  P2P_Server_App_Context.ButtonControl.Device_Button_Selection=0x02;/* Device2 */
  P2P_Server_App_Context.ButtonControl.ButtonStatus=0x00;
#endif  
#if(P2P_SERVER3 != 0)
  P2P_Server_App_Context.LedControl.Device_Led_Selection=0x03; /* Device3 */
  P2P_Server_App_Context.LedControl.Led1=0x00; /* led OFF */
  P2P_Server_App_Context.ButtonControl.Device_Button_Selection=0x03; /* Device3 */
  P2P_Server_App_Context.ButtonControl.ButtonStatus=0x00;
#endif
#if(P2P_SERVER4 != 0)
  P2P_Server_App_Context.LedControl.Device_Led_Selection=0x04; /* Device4 */
  P2P_Server_App_Context.LedControl.Led1=0x00; /* led OFF */
  P2P_Server_App_Context.ButtonControl.Device_Button_Selection=0x04; /* Device4 */
  P2P_Server_App_Context.ButtonControl.ButtonStatus=0x00;
#endif  
 #if(P2P_SERVER5 != 0)
  P2P_Server_App_Context.LedControl.Device_Led_Selection=0x05; /* Device5 */
  P2P_Server_App_Context.LedControl.Led1=0x00; /* led OFF */
  P2P_Server_App_Context.ButtonControl.Device_Button_Selection=0x05; /* Device5 */
  P2P_Server_App_Context.ButtonControl.ButtonStatus=0x00;
#endif
#if(P2P_SERVER6 != 0)
  P2P_Server_App_Context.LedControl.Device_Led_Selection=0x06; /* device6 */
  P2P_Server_App_Context.LedControl.Led1=0x00; /* led OFF */
  P2P_Server_App_Context.ButtonControl.Device_Button_Selection=0x06; /* Device6 */
  P2P_Server_App_Context.ButtonControl.ButtonStatus=0x00;
#endif  
}

void P2PS_APP_SW1_Button_Action(void)
{
  UTIL_SEQ_SetTask( 1<<CFG_TASK_SW1_BUTTON_PUSHED_ID, CFG_SCH_PRIO_0);

  return;
}
/* USER CODE END FD */

/*************************************************************
 *
 * LOCAL FUNCTIONS
 *
 *************************************************************/
/* USER CODE BEGIN FD_LOCAL_FUNCTIONS*/

// 30-byte veri üretme ve P2P ile gönderme fonksiyonu
// 30-byte veri üretme ve P2P ile gönderme fonksiyonu
void P2PS_Send_30ByteData(void) {
    APP_DBG_MSG("=== SERVER: GENERATING 30-BYTE DATA ===\n");

    // Random veri üret
    srand(HAL_GetTick());
    for (int i = 0; i < 30; i++) {
        ServerData[i] = rand() % 256;
    }
    DataGenerationCount++;

    // Veriyi terminalde göster
    APP_DBG_MSG("Generated Data (30 bytes): ");
    for (int i = 0; i < 30; i++) {
        APP_DBG_MSG("%02X ", ServerData[i]);
        if ((i + 1) % 10 == 0) APP_DBG_MSG("\n                          ");
    }
    APP_DBG_MSG("\nGeneration count: %lu\n", DataGenerationCount);

    // P2P notification ile gönder (2 paket: 15+15 byte)
    if (P2P_Server_App_Context.Notification_Status) {
        APP_DBG_MSG("Sending data via P2P notifications (2 packets)...\n");

        // PAKET 1: İlk 15 byte
        uint8_t packet1[20];
        packet1[0] = 0xAA; // Start marker
        packet1[1] = 0x01; // Packet 1 ID
        packet1[2] = 0x1E; // Total data length (30 bytes)
        packet1[3] = 0x0F; // This packet length (15 bytes)
        memcpy(&packet1[4], &ServerData[0], 15); // Data bytes 0-14
        packet1[19] = 0xBB; // End marker

        // DÜZELTME: P2P sadece 2-byte alabilir, farklı yaklaşım gerekiyor
        // 30-byte veriyi 15 parça halinde gönderelim (her paket 2-byte)

        // Veri gönderimi için farklı yaklaşım
        for(int i = 0; i < 15; i++) {
            uint8_t data_packet[2];
            data_packet[0] = 0x01; // Paket grubu 1
            data_packet[1] = ServerData[i];
            P2PS_STM_App_Update_Char(P2P_NOTIFY_CHAR_UUID, data_packet);
            HAL_Delay(50); // Kısa bekleme
        }

        HAL_Delay(200);

        // İkinci 15 byte
        for(int i = 15; i < 30; i++) {
            uint8_t data_packet[2];
            data_packet[0] = 0x02; // Paket grubu 2
            data_packet[1] = ServerData[i];
            P2PS_STM_App_Update_Char(P2P_NOTIFY_CHAR_UUID, data_packet);
            HAL_Delay(50);
        }

        APP_DBG_MSG("30-byte data sent successfully (30 packets of 2-bytes each)\n");
        DataTransferComplete = 0;

    } else {
        APP_DBG_MSG("ERROR: P2P notifications not enabled\n");
    }
}
// Eski P2PS_Send_Notification fonksiyonunu güncelle
void P2PS_Send_Notification(void)
{
    // Eski button logic'i basitleştir
    if(P2P_Server_App_Context.ButtonControl.ButtonStatus == 0x00){
        P2P_Server_App_Context.ButtonControl.ButtonStatus=0x01;
    } else {
        P2P_Server_App_Context.ButtonControl.ButtonStatus=0x00;
    }

    // Manual veri gönderimi (button basıldığında)
    APP_DBG_MSG("=== SERVER: MANUAL DATA SEND (BUTTON PRESSED) ===\n");
    P2PS_Send_30ByteData();
}

/* USER CODE END FD_LOCAL_FUNCTIONS*/

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    p2p_client_app.c
  * @author  MCD Application Team
  * @brief   peer to peer Client Application
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
#include "p2p_client_app.h"

#include "stm32_seq.h"
#include "app_ble.h"

/* USER CODE BEGIN Includes */
// Custom service için sabitler
#define CUSTOM_DATA_SERVICE_UUID    0x1234
#define DATA_STORAGE_CHAR_UUID      0x1235
#define DATA_COMMAND_CHAR_UUID      0x1236
#define DATA_STATUS_CHAR_UUID       0x1237

#define DATA_STORAGE_SIZE           30
#define COMMAND_SIZE               2
#define CMD_CLEAR_DATA_BYTE1       0xDE
#define CMD_CLEAR_DATA_BYTE2       0xAD

// Client için global değişkenler
static uint8_t ReceivedCustomData[DATA_STORAGE_SIZE];  // Alınan veri
static uint32_t CustomReadCount = 0;                  // Kaç kez okundu
static uint32_t CustomWriteCount = 0;                 // Kaç komut gönderildi

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/

typedef enum
{
  P2P_START_TIMER_EVT,
  P2P_STOP_TIMER_EVT,
  P2P_NOTIFICATION_INFO_RECEIVED_EVT,
} P2P_Client_Opcode_Notification_evt_t;

typedef struct
{
  uint8_t * pPayload;
  uint8_t     Length;
}P2P_Client_Data_t;

typedef struct
{
  P2P_Client_Opcode_Notification_evt_t  P2P_Client_Evt_Opcode;
  P2P_Client_Data_t DataTransfered;
}P2P_Client_App_Notification_evt_t;

typedef struct
{
  /**
   * state of the P2P Client
   * state machine
   */
  APP_BLE_ConnStatus_t state;

  /**
   * connection handle
   */
  uint16_t connHandle;

  /**
   * handle of the P2P service
   */
  uint16_t P2PServiceHandle;

  /**
   * end handle of the P2P service
   */
  uint16_t P2PServiceEndHandle;

  /**
   * handle of the Tx characteristic - Write To Server
   *
   */
  uint16_t P2PWriteToServerCharHdle;

  /**
   * handle of the client configuration
   * descriptor of Tx characteristic
   */
  uint16_t P2PWriteToServerDescHandle;

  /**
   * handle of the Rx characteristic - Notification From Server
   *
   */
  uint16_t P2PNotificationCharHdle;

  /**
   * handle of the client configuration
   * descriptor of Rx characteristic
   */
  uint16_t P2PNotificationDescHandle;

}P2P_ClientContext_t;

/* USER CODE BEGIN PTD */
typedef struct{
  uint8_t                                     Device_Led_Selection;
  uint8_t                                     Led1;
}P2P_LedCharValue_t;

typedef struct{
  uint8_t                                     Device_Button_Selection;
  uint8_t                                     Button1;
}P2P_ButtonCharValue_t;

typedef struct
{

  uint8_t       Notification_Status; /* used to check if P2P Server is enabled to Notify */

  P2P_LedCharValue_t         LedControl;
  P2P_ButtonCharValue_t      ButtonStatus;

  uint16_t ConnectionHandle; 


} P2P_Client_App_Context_t;

/* USER CODE END PTD */

/* Private defines ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macros -------------------------------------------------------------*/
#define UNPACK_2_BYTE_PARAMETER(ptr)  \
        (uint16_t)((uint16_t)(*((uint8_t *)ptr))) |   \
        (uint16_t)((((uint16_t)(*((uint8_t *)ptr + 1))) << 8))
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/**
 * START of Section BLE_APP_CONTEXT
 */

static P2P_ClientContext_t aP2PClientContext[BLE_CFG_CLT_MAX_NBR_CB];

/**
 * END of Section BLE_APP_CONTEXT
 */
/* USER CODE BEGIN PV */
static P2P_Client_App_Context_t P2P_Client_App_Context;
static char target_device_name[32];
static uint8_t DisconnectTimerId;     // Timer ID'si
static uint8_t AutoDisconnectEnabled; // Otomatik disconnect aktif mi?
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void Gatt_Notification(P2P_Client_App_Notification_evt_t *pNotification);
static SVCCTL_EvtAckStatus_t Event_Handler(void *Event);
/* USER CODE BEGIN PFP */
static tBleStatus Write_Char(uint16_t UUID, uint8_t Service_Instance, uint8_t *pPayload);
static void Button_Trigger_Received( void );
static void Update_Service( void );
static void AutoDisconnect_Timer_Callback(void);
// Fonksiyon tanımları
static void PrintReceivedData(uint8_t *data, uint16_t length, const char* source);
static void ReadCustomData(void);
static void SendClearCommand(void);
static void TestCustomService(void);
/* USER CODE END PFP */

/* Functions Definition ------------------------------------------------------*/
/**
 * @brief  Service initialization
 * @param  None
 * @retval None
 */
void P2PC_APP_Init(void)
{
  uint8_t index =0;
/* USER CODE BEGIN P2PC_APP_Init_1 */
  UTIL_SEQ_RegTask( 1<< CFG_TASK_SEARCH_SERVICE_ID, UTIL_SEQ_RFU, Update_Service );
  UTIL_SEQ_RegTask( 1<< CFG_TASK_SW1_BUTTON_PUSHED_ID, UTIL_SEQ_RFU, Button_Trigger_Received );
  HW_TS_Create(CFG_TIM_PROC_ID_ISR, &DisconnectTimerId, hw_ts_SingleShot, AutoDisconnect_Timer_Callback);
  AutoDisconnectEnabled = 0; // Başlangıçta kapalı

  /**
   * Initialize LedButton Service
   */
  P2P_Client_App_Context.Notification_Status=0;
  P2P_Client_App_Context.ConnectionHandle =  0x00;

  P2P_Client_App_Context.LedControl.Device_Led_Selection=0x00;/* device Led */
  P2P_Client_App_Context.LedControl.Led1=0x00; /* led OFF */
  P2P_Client_App_Context.ButtonStatus.Device_Button_Selection=0x01;/* Device1 */
  P2P_Client_App_Context.ButtonStatus.Button1=0x00;
/* USER CODE END P2PC_APP_Init_1 */
  for(index = 0; index < BLE_CFG_CLT_MAX_NBR_CB; index++)
  {
    aP2PClientContext[index].state= APP_BLE_IDLE;
  }

  /**
   *  Register the event handler to the BLE controller
   */
  SVCCTL_RegisterCltHandler(Event_Handler);

#if(CFG_DEBUG_APP_TRACE != 0)
  APP_DBG_MSG("-- P2P CLIENT INITIALIZED \n");
#endif

/* USER CODE BEGIN P2PC_APP_Init_2 */

/* USER CODE END P2PC_APP_Init_2 */
  return;
}

void P2PC_APP_Notification(P2PC_APP_ConnHandle_Not_evt_t *pNotification)
{
/* USER CODE BEGIN P2PC_APP_Notification_1 */

/* USER CODE END P2PC_APP_Notification_1 */
  switch(pNotification->P2P_Evt_Opcode)
  {
/* USER CODE BEGIN P2P_Evt_Opcode */

/* USER CODE END P2P_Evt_Opcode */

  case PEER_CONN_HANDLE_EVT :
/* USER CODE BEGIN PEER_CONN_HANDLE_EVT */
	  P2P_Client_App_Context.ConnectionHandle = pNotification->ConnectionHandle;
	     APP_DBG_MSG("\n===========================================\n");
	     APP_DBG_MSG("    CLIENT SUCCESSFULLY CONNECTED!\n");
	     APP_DBG_MSG("    Connected to server: %s\n", target_device_name);
	     APP_DBG_MSG("===========================================\n\n");
	     AutoDisconnectEnabled = 1;
	      HW_TS_Start(DisconnectTimerId, (uint32_t)(2 * 1000 * 1000 / CFG_TS_TICK_VAL)); // 2 saniye
	      APP_DBG_MSG("-- 2 SANİYE SONRA OTOMATİK DISCONNECT --\n");
	      break;
/* USER CODE END PEER_CONN_HANDLE_EVT */
      break;

  case PEER_DISCON_HANDLE_EVT :
  {
      APP_DBG_MSG("=== CLIENT SERVER'DAN KOPTU ===\n");

      uint8_t index = 0;
      P2P_Client_App_Context.ConnectionHandle = 0x00;
      while((index < BLE_CFG_CLT_MAX_NBR_CB) && (aP2PClientContext[index].state != APP_BLE_IDLE)) {
          aP2PClientContext[index].state = APP_BLE_IDLE;
          index++;
      }

      BSP_LED_Off(LED_BLUE);

      // 2 saniye bekle sonra yeniden tara
      HAL_Delay(2000);
      APP_DBG_MSG("=== YENİDEN TARAMA BAŞLATILIYOR ===\n");
      UTIL_SEQ_SetTask(1<<CFG_TASK_START_SCAN_ID, CFG_SCH_PRIO_0);
  }
  break;
/* USER CODE BEGIN P2PC_APP_Notification_2 */

/* USER CODE END P2PC_APP_Notification_2 */
  return;
}
}
/* USER CODE BEGIN FD */
void P2PC_APP_SW1_Button_Action(void)
{

  UTIL_SEQ_SetTask(1<<CFG_TASK_SW1_BUTTON_PUSHED_ID, CFG_SCH_PRIO_0);

}
/* USER CODE END FD */

/*************************************************************
 *
 * LOCAL FUNCTIONS
 *
 *************************************************************/

/**
 * @brief  Event handler
 * @param  Event: Address of the buffer holding the Event
 * @retval Ack: Return whether the Event has been managed or not
 */
static SVCCTL_EvtAckStatus_t Event_Handler(void *Event)
{
  SVCCTL_EvtAckStatus_t return_value;
  hci_event_pckt *event_pckt;
  evt_blecore_aci *blecore_evt;

  P2P_Client_App_Notification_evt_t Notification;

  return_value = SVCCTL_EvtNotAck;
  event_pckt = (hci_event_pckt *)(((hci_uart_pckt*)Event)->data);

  switch(event_pckt->evt)
  {
    case HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE:
    {
      blecore_evt = (evt_blecore_aci*)event_pckt->data;
      switch(blecore_evt->ecode)
      {
      /* USER CODE BEGIN NEW_CASES */

                      /*
                       * GATT Read Response - Server'dan veri geldi
                       *
                       * Bu case ne zaman tetiklenir?
                       * - ReadCustomData() çağırdığımızda
                       * - Server veriyi gönderince
                       */
                      case ACI_ATT_READ_RESP_VSEVT_CODE: {
                          aci_att_read_resp_event_rp0 *pr = (void*)blecore_evt->data;

                          APP_DBG_MSG("=== GATT READ RESPONSE ===\n");
                          APP_DBG_MSG("Bağlantı Handle: 0x%04X\n", pr->Connection_Handle);
                          APP_DBG_MSG("Gelen veri uzunluğu: %d byte\n", pr->Event_Data_Length);

                          // Veriyi yazdır
                          if (pr->Event_Data_Length > 0) {
                              PrintReceivedData(pr->Attribute_Value, pr->Event_Data_Length, "READ RESPONSE");
                          }
                          break;
                      }

                      /*
                       * GATT Notification - Server bildirim gönderdi
                       *
                       * Bu case ne zaman tetiklenir?
                       * - Server veri değiştiğinde
                       * - Notification açıksa otomatik gelir
                       */
                      case ACI_GATT_NOTIFICATION_VSEVT_CODE: {
                          aci_gatt_notification_event_rp0 *pr = (void*)blecore_evt->data;
                          uint8_t index;

                          // Debug yazdır
                          APP_DBG_MSG("=== NOTIFICATION GELDI ===\n");
                          APP_DBG_MSG("Handle: 0x%04X\n", pr->Attribute_Handle);
                          APP_DBG_MSG("Veri uzunluğu: %d byte\n", pr->Attribute_Value_Length);

                          // Client context bul
                          index = 0;
                          while((index < BLE_CFG_CLT_MAX_NBR_CB) &&
                                (aP2PClientContext[index].connHandle != pr->Connection_Handle))
                              index++;

                          if(index < BLE_CFG_CLT_MAX_NBR_CB) {

                              // P2P Service notification mı kontrol et
                              if ((pr->Attribute_Handle == aP2PClientContext[index].P2PNotificationCharHdle) &&
                                  (pr->Attribute_Value_Length == 2)) {

                                  // P2P notification işle
                                  P2P_Client_App_Notification_evt_t Notification;
                                  Notification.P2P_Client_Evt_Opcode = P2P_NOTIFICATION_INFO_RECEIVED_EVT;
                                  Notification.DataTransfered.Length = pr->Attribute_Value_Length;
                                  Notification.DataTransfered.pPayload = &pr->Attribute_Value[0];
                                  Gatt_Notification(&Notification);

                              } else {
                                  // Custom service notification (diğer handle'lar için)
                                  if (pr->Attribute_Value_Length > 0) {
                                      PrintReceivedData(&pr->Attribute_Value[0], pr->Attribute_Value_Length, "NOTIFICATION");
                                  }
                              }
                          }
                          break;
                      }

                      /* USER CODE END NEW_CASES */
        case ACI_ATT_READ_BY_GROUP_TYPE_RESP_VSEVT_CODE:
        {
          aci_att_read_by_group_type_resp_event_rp0 *pr = (void*)blecore_evt->data;
          uint8_t numServ, i, idx;
          uint16_t uuid, handle;

          uint8_t index;
          handle = pr->Connection_Handle;
          index = 0;
          while((index < BLE_CFG_CLT_MAX_NBR_CB) &&
                  (aP2PClientContext[index].state != APP_BLE_IDLE))
          {
            APP_BLE_ConnStatus_t status;

            status = APP_BLE_Get_Client_Connection_Status(aP2PClientContext[index].connHandle);

            if((aP2PClientContext[index].state == APP_BLE_CONNECTED_CLIENT)&&
                    (status == APP_BLE_IDLE))
            {
              /* Handle deconnected */

              aP2PClientContext[index].state = APP_BLE_IDLE;
              aP2PClientContext[index].connHandle = 0xFFFF;
              break;
            }
            index++;
          }

          if(index < BLE_CFG_CLT_MAX_NBR_CB)
          {
            aP2PClientContext[index].connHandle= handle;

            numServ = (pr->Data_Length) / pr->Attribute_Data_Length;

            /* the event data will be
             * 2bytes start handle
             * 2bytes end handle
             * 2 or 16 bytes data
             * we are interested only if the UUID is 16 bit.
             * So check if the data length is 6
             */
#if (UUID_128BIT_FORMAT==1)
          if (pr->Attribute_Data_Length == 20)
          {
            idx = 16;
#else
          if (pr->Attribute_Data_Length == 6)
          {
            idx = 4;
#endif
              for (i=0; i<numServ; i++)
              {
                uuid = UNPACK_2_BYTE_PARAMETER(&pr->Attribute_Data_List[idx]);
                if(uuid == P2P_SERVICE_UUID)
                {
#if(CFG_DEBUG_APP_TRACE != 0)
                  APP_DBG_MSG("-- GATT : P2P_SERVICE_UUID FOUND - connection handle 0x%x \n", aP2PClientContext[index].connHandle);
#endif
#if (UUID_128BIT_FORMAT==1)
                aP2PClientContext[index].P2PServiceHandle = UNPACK_2_BYTE_PARAMETER(&pr->Attribute_Data_List[idx-16]);
                aP2PClientContext[index].P2PServiceEndHandle = UNPACK_2_BYTE_PARAMETER (&pr->Attribute_Data_List[idx-14]);
#else
                aP2PClientContext[index].P2PServiceHandle = UNPACK_2_BYTE_PARAMETER(&pr->Attribute_Data_List[idx-4]);
                aP2PClientContext[index].P2PServiceEndHandle = UNPACK_2_BYTE_PARAMETER (&pr->Attribute_Data_List[idx-2]);
#endif
                  aP2PClientContext[index].state = APP_BLE_DISCOVER_CHARACS ;
                }
                idx += 6;
              }
            }
          }
        }
        break;

        case ACI_ATT_READ_BY_TYPE_RESP_VSEVT_CODE:
        {

          aci_att_read_by_type_resp_event_rp0 *pr = (void*)blecore_evt->data;
          uint8_t idx;
          uint16_t uuid, handle;

          /* the event data will be
           * 2 bytes start handle
           * 1 byte char properties
           * 2 bytes handle
           * 2 or 16 bytes data
           */

          uint8_t index;

          index = 0;
          while((index < BLE_CFG_CLT_MAX_NBR_CB) &&
                  (aP2PClientContext[index].connHandle != pr->Connection_Handle))
            index++;

          if(index < BLE_CFG_CLT_MAX_NBR_CB)
          {

            /* we are interested in only 16 bit UUIDs */
#if (UUID_128BIT_FORMAT==1)
            idx = 17;
            if (pr->Handle_Value_Pair_Length == 21)
#else
              idx = 5;
            if (pr->Handle_Value_Pair_Length == 7)
#endif
            {
              pr->Data_Length -= 1;
              while(pr->Data_Length > 0)
              {
                uuid = UNPACK_2_BYTE_PARAMETER(&pr->Handle_Value_Pair_Data[idx]);
                /* store the characteristic handle not the attribute handle */
#if (UUID_128BIT_FORMAT==1)
                handle = UNPACK_2_BYTE_PARAMETER(&pr->Handle_Value_Pair_Data[idx-14]);
#else
                handle = UNPACK_2_BYTE_PARAMETER(&pr->Handle_Value_Pair_Data[idx-2]);
#endif
                if(uuid == P2P_WRITE_CHAR_UUID)
                {
#if(CFG_DEBUG_APP_TRACE != 0)
                  APP_DBG_MSG("-- GATT : WRITE_UUID FOUND - connection handle 0x%x\n", aP2PClientContext[index].connHandle);
#endif
                  aP2PClientContext[index].state = APP_BLE_DISCOVER_WRITE_DESC;
                  aP2PClientContext[index].P2PWriteToServerCharHdle = handle;
                }

                else if(uuid == P2P_NOTIFY_CHAR_UUID)
                {
#if(CFG_DEBUG_APP_TRACE != 0)
                  APP_DBG_MSG("-- GATT : NOTIFICATION_CHAR_UUID FOUND  - connection handle 0x%x\n", aP2PClientContext[index].connHandle);
#endif
                  aP2PClientContext[index].state = APP_BLE_DISCOVER_NOTIFICATION_CHAR_DESC;
                  aP2PClientContext[index].P2PNotificationCharHdle = handle;
                }
#if (UUID_128BIT_FORMAT==1)
                pr->Data_Length -= 21;
                idx += 21;
#else
                pr->Data_Length -= 7;
                idx += 7;
#endif
              }
            }
          }
        }
        break;

        case ACI_ATT_FIND_INFO_RESP_VSEVT_CODE:
        {
          aci_att_find_info_resp_event_rp0 *pr = (void*)blecore_evt->data;

          uint8_t numDesc, idx, i;
          uint16_t uuid, handle;

          /*
           * event data will be of the format
           * 2 bytes handle
           * 2 bytes UUID
           */

          uint8_t index;

          index = 0;
          while((index < BLE_CFG_CLT_MAX_NBR_CB) &&
                  (aP2PClientContext[index].connHandle != pr->Connection_Handle))

            index++;

          if(index < BLE_CFG_CLT_MAX_NBR_CB)
          {

            numDesc = (pr->Event_Data_Length) / 4;
            /* we are interested only in 16 bit UUIDs */
            idx = 0;
            if (pr->Format == UUID_TYPE_16)
            {
              for (i=0; i<numDesc; i++)
              {
                handle = UNPACK_2_BYTE_PARAMETER(&pr->Handle_UUID_Pair[idx]);
                uuid = UNPACK_2_BYTE_PARAMETER(&pr->Handle_UUID_Pair[idx+2]);

                if(uuid == CLIENT_CHAR_CONFIG_DESCRIPTOR_UUID)
                {
#if(CFG_DEBUG_APP_TRACE != 0)
                  APP_DBG_MSG("-- GATT : CLIENT_CHAR_CONFIG_DESCRIPTOR_UUID- connection handle 0x%x\n", aP2PClientContext[index].connHandle);
#endif
                  if( aP2PClientContext[index].state == APP_BLE_DISCOVER_NOTIFICATION_CHAR_DESC)
                  {

                    aP2PClientContext[index].P2PNotificationDescHandle = handle;
                    aP2PClientContext[index].state = APP_BLE_ENABLE_NOTIFICATION_DESC;

                  }
                }
                idx += 4;
              }
            }
          }
        }
        break; /*ACI_ATT_FIND_INFO_RESP_VSEVT_CODE*/


        case ACI_GATT_PROC_COMPLETE_VSEVT_CODE:
        {
          aci_gatt_proc_complete_event_rp0 *pr = (void*)blecore_evt->data;
#if(CFG_DEBUG_APP_TRACE != 0)
          APP_DBG_MSG("-- GATT : ACI_GATT_PROC_COMPLETE_VSEVT_CODE \n");
          APP_DBG_MSG("\n");
#endif

          uint8_t index;

          index = 0;
          while((index < BLE_CFG_CLT_MAX_NBR_CB) &&
                  (aP2PClientContext[index].connHandle != pr->Connection_Handle))
            index++;

          if(index < BLE_CFG_CLT_MAX_NBR_CB)
          {

            UTIL_SEQ_SetTask( 1<<CFG_TASK_SEARCH_SERVICE_ID, CFG_SCH_PRIO_0);

          }
        }
        break; /*ACI_GATT_PROC_COMPLETE_VSEVT_CODE*/
        default:
          break;
      }
    }

    break; /* HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE */

    default:
      break;
  }

  return(return_value);
}/* end BLE_CTRL_Event_Acknowledged_Status_t */

void Gatt_Notification(P2P_Client_App_Notification_evt_t *pNotification)
{
/* USER CODE BEGIN Gatt_Notification_1*/

/* USER CODE END Gatt_Notification_1 */
  switch(pNotification->P2P_Client_Evt_Opcode)
  {
/* USER CODE BEGIN P2P_Client_Evt_Opcode */

/* USER CODE END P2P_Client_Evt_Opcode */

    case P2P_NOTIFICATION_INFO_RECEIVED_EVT:
/* USER CODE BEGIN P2P_NOTIFICATION_INFO_RECEIVED_EVT */
    {
      P2P_Client_App_Context.LedControl.Device_Led_Selection=pNotification->DataTransfered.pPayload[0];
      switch(P2P_Client_App_Context.LedControl.Device_Led_Selection) {

        case 0x01 : {

          P2P_Client_App_Context.LedControl.Led1=pNotification->DataTransfered.pPayload[1];

          if(P2P_Client_App_Context.LedControl.Led1==0x00){
            BSP_LED_Off(LED_BLUE);
            APP_DBG_MSG(" -- P2P APPLICATION CLIENT : NOTIFICATION RECEIVED - LED OFF \n\r");
            APP_DBG_MSG(" \n\r");
          } else {
            APP_DBG_MSG(" -- P2P APPLICATION CLIENT : NOTIFICATION RECEIVED - LED ON\n\r");
            APP_DBG_MSG(" \n\r");
            BSP_LED_On(LED_BLUE);
          }

          break;
        }
        default : break;
      }

    }
/* USER CODE END P2P_NOTIFICATION_INFO_RECEIVED_EVT */
      break;

    default:
/* USER CODE BEGIN P2P_Client_Evt_Opcode_Default */

/* USER CODE END P2P_Client_Evt_Opcode_Default */
      break;
  }
/* USER CODE BEGIN Gatt_Notification_2*/

/* USER CODE END Gatt_Notification_2 */
  return;
}

uint8_t P2P_Client_APP_Get_State( void ) {
  return aP2PClientContext[0].state;
}
/* USER CODE BEGIN LF */
/**
 * @brief  Feature Characteristic update
 * @param  pFeatureValue: The address of the new value to be written
 * @retval None
 */

/* Alınan veriyi yazdıran fonksiyon
*
* Bu fonksiyon ne yapar?
* - Client'ın aldığı veriyi detaylı şekilde yazdırır
* - Hex formatında gösterir
* - Checksum hesaplar
* - Server ile karşılaştırma için
*/
static void PrintReceivedData(uint8_t *data, uint16_t length, const char* source) {
   APP_DBG_MSG("\n=== CLIENT VERİ ALDI ===\n");
   APP_DBG_MSG("Kaynak: %s\n", source);
   APP_DBG_MSG("Uzunluk: %d byte\n", length);

   if (length == DATA_STORAGE_SIZE) {  // 30 byte custom data ise
       APP_DBG_MSG("--- CLIENT ALDIĞI VERİ (30 byte) ---\n");

       // Veriyi kopyala (sonra kullanmak için)
       memcpy(ReceivedCustomData, data, DATA_STORAGE_SIZE);

       // Hex formatında yazdır
       for (int i = 0; i < length; i++) {
           if (i % 8 == 0) {
               APP_DBG_MSG("\n[%02d-%02d]: ", i,
                          (i+7 < length) ? i+7 : length-1);
           }
           APP_DBG_MSG("%02X ", data[i]);
       }

       APP_DBG_MSG("\n\n");

       // Checksum hesapla (Server ile karşılaştırmak için)
       uint16_t checksum = 0;
       for (int i = 0; i < length; i++) {
           checksum += data[i];
       }

       APP_DBG_MSG("Client Checksum: 0x%04X (%d decimal)\n", checksum, checksum);
       APP_DBG_MSG("Alma Zamanı: %lu ms\n", HAL_GetTick());

       // ASCII karakter kontrolü (eğer okunabilir karakterler varsa)
       APP_DBG_MSG("ASCII Görünümü: ");
       for (int i = 0; i < length; i++) {
           if (data[i] >= 32 && data[i] <= 126) {  // Printable ASCII aralığı
               APP_DBG_MSG("%c", data[i]);
           } else {
               APP_DBG_MSG(".");  // Okunmayan karakterler için nokta
           }
       }
       APP_DBG_MSG("\n");

       CustomReadCount++;  // İstatistik
       APP_DBG_MSG("Toplam okuma sayısı: %lu\n", CustomReadCount);

   } else {
       // Kısa veri için basit yazdırma
       APP_DBG_MSG("Veri: ");
       for (int i = 0; i < length; i++) {
           APP_DBG_MSG("%02X ", data[i]);
       }
       APP_DBG_MSG("\n");
   }

   APP_DBG_MSG("==========================\n\n");
}

/*
* Custom veri okuma fonksiyonu
*
* Bu fonksiyon ne yapar?
* - Server'daki 30 byte veriyi okur
* - Handle bilgisini kullanır (debug'dan öğrendiğimiz)
* - GATT read komutu gönderir
*
* ÖNEMLI: Handle değerlerini debug mesajlarından öğrenmelisiniz!
*/
static void ReadCustomData(void) {
   if (P2P_Client_App_Context.ConnectionHandle != 0x00) {
       APP_DBG_MSG("\n>>> CLIENT CUSTOM VERİYİ OKUYOR <<<\n");

       /*
        * ÖNEMLI: Bu handle değeri sabit değil!
        * Debug mesajlarından öğrenin ve güncelleyin
        * Server başladığında "Storage Handle: 0x****" mesajını bulun
        */
       uint16_t custom_data_handle = 0x000E;  // ← BU DEĞER DEĞİŞEBİLİR

       /*
        * GATT read komutu gönder
        *
        * aci_gatt_read_char_value nedir?
        * - GATT karakteristiğini okuyan fonksiyon
        * - Server'a "bu handle'daki veriyi gönder" der
        * - Cevap event olarak gelir
        */
       tBleStatus ret = aci_gatt_read_char_value(
           P2P_Client_App_Context.ConnectionHandle,  // Bağlantı handle'ı
           custom_data_handle                        // Okumak istediğimiz karakteristik
       );

       if (ret == BLE_STATUS_SUCCESS) {
           APP_DBG_MSG("✓ Custom data read komutu gönderildi, Handle: 0x%04X\n", custom_data_handle);
           APP_DBG_MSG("  Cevap event olarak gelecek...\n");
       } else {
           APP_DBG_MSG("✗ Custom data read hatası: 0x%x\n", ret);
           APP_DBG_MSG("  Handle değeri yanlış olabilir: 0x%04X\n", custom_data_handle);
       }
   } else {
       APP_DBG_MSG("✗ Client bağlı değil, veri okunamıyor\n");
   }
}

/*
* Clear komutu gönderme fonksiyonu
*
* Bu fonksiyon ne yapar?
* - Server'a [0xDE, 0xAD] komutunu gönderir
* - "Veriyi okudum, yenisini üretebilirsin" anlamında
* - Server yeni veri üretir
*/
static void SendClearCommand(void) {
   if (P2P_Client_App_Context.ConnectionHandle != 0x00) {
       APP_DBG_MSG("\n>>> CLIENT CLEAR KOMUTU GÖNDERİYOR <<<\n");

       /*
        * ÖNEMLI: Bu handle değeri de debug'dan öğrenilmeli!
        * Server başladığında "Command Handle: 0x****" mesajını bulun
        */
       uint16_t custom_command_handle = 0x0010;  // ← BU DEĞER DEĞİŞEBİLİR

       // Clear komutunu hazırla
       uint8_t clear_command[COMMAND_SIZE] = {
           CMD_CLEAR_DATA_BYTE1,  // 0xDE
           CMD_CLEAR_DATA_BYTE2   // 0xAD
       };

       /*
        * GATT write komutu gönder
        *
        * aci_gatt_write_char_value nedir?
        * - GATT karakteristiğine veri yazan fonksiyon
        * - Server'a "bu handle'a bu veriyi yaz" der
        * - Server event olarak alır
        */
       tBleStatus ret = aci_gatt_write_char_value(
           P2P_Client_App_Context.ConnectionHandle,  // Bağlantı handle'ı
           custom_command_handle,                    // Yazmak istediğimiz karakteristik
           COMMAND_SIZE,                             // 2 byte
           clear_command                             // Komut verisi
       );

       if (ret == BLE_STATUS_SUCCESS) {
           APP_DBG_MSG("✓ Clear komutu gönderildi: [0x%02X, 0x%02X]\n",
                      clear_command[0], clear_command[1]);
           APP_DBG_MSG("  Handle: 0x%04X\n", custom_command_handle);
           APP_DBG_MSG("  Server yeni veri üretecek...\n");

           CustomWriteCount++;  // İstatistik
           APP_DBG_MSG("Toplam yazma sayısı: %lu\n", CustomWriteCount);

       } else {
           APP_DBG_MSG("✗ Clear komutu hatası: 0x%x\n", ret);
           APP_DBG_MSG("  Handle değeri yanlış olabilir: 0x%04X\n", custom_command_handle);
       }
   } else {
       APP_DBG_MSG("✗ Client bağlı değil, komut gönderilemez\n");
   }
}

/*
* Komple test fonksiyonu
*
* Bu fonksiyon ne yapar?
* 1. Önce veriyi okur
* 2. 2 saniye bekler
* 3. Clear komutu gönderir
* 4. 2 saniye bekler
* 5. Yeni veriyi okur
*/
static void TestCustomService(void) {
   APP_DBG_MSG("\n========== CUSTOM SERVICE TEST BAŞLIYOR ==========\n");

   // 1. Mevcut veriyi oku
   APP_DBG_MSG("ADIM 1: Mevcut veri okunuyor...\n");
   ReadCustomData();

   // 2. Kısa bekleme (Server'ın cevap vermesi için)
   HAL_Delay(2000);  // 2 saniye bekle

   // 3. Clear komutu gönder
   APP_DBG_MSG("ADIM 2: Clear komutu gönderiliyor...\n");
   SendClearCommand();

   // 4. Server'ın yeni veri üretmesi için bekle
   HAL_Delay(2000);  // 2 saniye bekle

   // 5. Yeni veriyi oku
   APP_DBG_MSG("ADIM 3: Yeni veri okunuyor...\n");
   ReadCustomData();

   APP_DBG_MSG("========== CUSTOM SERVICE TEST BİTTİ ==========\n\n");
}
static void AutoDisconnect_Timer_Callback(void) {
    // 2 saniye sonra çağrılacak fonksiyon
    APP_DBG_MSG("-- 2 SANIYE GECTI - BAGLANTI KESILIYOR --\n");

    if (P2P_Client_App_Context.ConnectionHandle != 0x00) {
        // Bağlantıyı kes
        aci_gap_terminate(P2P_Client_App_Context.ConnectionHandle,
                         HCI_REMOTE_USER_TERMINATED_CONNECTION_ERR_CODE);

        AutoDisconnectEnabled = 0; // Otomatik disconnect'i durdur
        APP_DBG_MSG("-- DISCONNECT KOMUTU GÖNDERİLDİ --\n");
        UTIL_SEQ_SetTask(1 << CFG_TASK_START_SCAN_ID, CFG_SCH_PRIO_0);
    }
}

// Clienttan servera veri göndermek için kullanılır
tBleStatus Write_Char(uint16_t UUID, uint8_t Service_Instance, uint8_t *pPayload)
{
  tBleStatus ret = BLE_STATUS_INVALID_PARAMS;
  uint8_t index;

  index = 0;
  while((index < BLE_CFG_CLT_MAX_NBR_CB) &&
          (aP2PClientContext[index].state != APP_BLE_IDLE))
  {
    switch(UUID)
    {
      case P2P_WRITE_CHAR_UUID: /* SERVER RX -- so CLIENT TX */
        ret = aci_gatt_write_without_resp(aP2PClientContext[index].connHandle,
                                         aP2PClientContext[index].P2PWriteToServerCharHdle,
                                         2, /* charValueLen */
                                         (uint8_t *)  pPayload);
        break;
      default:
        break;
    }
    index++;
  }

  return ret;
}/* end Write_Char() */

void Button_Trigger_Received(void)
{
  APP_DBG_MSG("-- P2P APPLICATION CLIENT  : BUTTON PUSHED - WRITE TO SERVER \n ");
  APP_DBG_MSG(" \n\r");
  if(P2P_Client_App_Context.ButtonStatus.Button1 == 0x00)
  {
    P2P_Client_App_Context.ButtonStatus.Button1 = 0x01;
  }else {
    P2P_Client_App_Context.ButtonStatus.Button1 = 0x00;
  }

  Write_Char( P2P_WRITE_CHAR_UUID, 0, (uint8_t *)&P2P_Client_App_Context.ButtonStatus);
  /* USER CODE BEGIN Button_Custom */

    /*
     * Custom Service Test
     *
     * Buton her basıldığında custom service test edilir
     * Test sırası:
     * - Tek sayılarda (1,3,5...): Sadece veri oku
     * - Çift sayılarda (2,4,6...): Clear komutu gönder ve yeni veriyi oku
     */
    static uint8_t button_counter = 0;
    button_counter++;

    APP_DBG_MSG("\n--- BUTON BASILDI (%d. kez) ---\n", button_counter);

    if (button_counter % 3 == 1) {
        // Her 3 basımda 1: Sadece veri oku
        APP_DBG_MSG("Test modu: Sadece veri okuma\n");
        ReadCustomData();

    } else if (button_counter % 3 == 2) {
        // Her 3 basımda 1: Clear komutu gönder
        APP_DBG_MSG("Test modu: Clear komutu\n");
        SendClearCommand();

    } else {
        // Her 3 basımda 1: Komple test
        APP_DBG_MSG("Test modu: Komple test\n");
        TestCustomService();
    }

    /* USER CODE END Button_Custom */

  return;
}

void Update_Service()
{
  uint16_t enable = 0x0001;
  uint16_t disable = 0x0000;
  uint8_t index;

  index = 0;
  while((index < BLE_CFG_CLT_MAX_NBR_CB) &&
          (aP2PClientContext[index].state != APP_BLE_IDLE))
  {
    switch(aP2PClientContext[index].state)
    {
      case APP_BLE_DISCOVER_SERVICES:
        APP_DBG_MSG("P2P_DISCOVER_SERVICES\n");
        break;
      case APP_BLE_DISCOVER_CHARACS:
        APP_DBG_MSG("* GATT : Discover P2P Characteristics\n");
        aci_gatt_disc_all_char_of_service(aP2PClientContext[index].connHandle,
                                          aP2PClientContext[index].P2PServiceHandle,
                                          aP2PClientContext[index].P2PServiceEndHandle);
        break;
      case APP_BLE_DISCOVER_WRITE_DESC: /* Not Used - No descriptor */
        APP_DBG_MSG("* GATT : Discover Descriptor of TX - Write  Characteristic\n");
        aci_gatt_disc_all_char_desc(aP2PClientContext[index].connHandle,
                                    aP2PClientContext[index].P2PWriteToServerCharHdle,
                                    aP2PClientContext[index].P2PWriteToServerCharHdle+2);
        break;
      case APP_BLE_DISCOVER_NOTIFICATION_CHAR_DESC:
        APP_DBG_MSG("* GATT : Discover Descriptor of Rx - Notification  Characteristic\n");
        aci_gatt_disc_all_char_desc(aP2PClientContext[index].connHandle,
                                    aP2PClientContext[index].P2PNotificationCharHdle,
                                    aP2PClientContext[index].P2PNotificationCharHdle+2);
        break;
      case APP_BLE_ENABLE_NOTIFICATION_DESC:
        APP_DBG_MSG("* GATT : Enable Server Notification\n");
        aci_gatt_write_char_desc(aP2PClientContext[index].connHandle,
                                 aP2PClientContext[index].P2PNotificationDescHandle,
                                 2,
                                 (uint8_t *)&enable);
        aP2PClientContext[index].state = APP_BLE_CONNECTED_CLIENT;
        BSP_LED_Off(LED_RED);
        break;
      case APP_BLE_DISABLE_NOTIFICATION_DESC :
        APP_DBG_MSG("* GATT : Disable Server Notification\n");
        aci_gatt_write_char_desc(aP2PClientContext[index].connHandle,
                                 aP2PClientContext[index].P2PNotificationDescHandle,
                                 2,
                                 (uint8_t *)&disable);
        aP2PClientContext[index].state = APP_BLE_CONNECTED_CLIENT;
        break;
      default:
        break;
    }
    index++;
  }
  return;
}
/* USER CODE END LF */

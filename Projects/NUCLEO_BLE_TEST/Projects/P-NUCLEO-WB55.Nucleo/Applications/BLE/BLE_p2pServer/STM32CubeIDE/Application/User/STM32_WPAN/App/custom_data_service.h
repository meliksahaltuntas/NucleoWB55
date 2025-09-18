/*
 * =====================================================================
 * CUSTOM DATA SERVICE HEADER - OPTIMIZED VERSION
 * =====================================================================
 *
 * Bu versiyon ne değişti?
 * - UUID tanımları STM32WB BLE stack'e uygun hale getirildi
 * - Struct minimal versiyona göre optimize edildi
 * - Kullanılmayan field'lar kaldırıldı
 * - Resource efficient tasarım
 */

#ifndef CUSTOM_DATA_SERVICE_H
#define CUSTOM_DATA_SERVICE_H

// =====================================================================
// INCLUDE'LAR
// =====================================================================
#include "ble.h"
#include "ble_types.h"
#include "hci_tl.h"
#include "ble_events.h"

// =====================================================================
// UUID TANIMLARI - STM32WB UYUMLU FORMAT
// =====================================================================

/*
 * UUID Format Değişikliği:
 *
 * ESKI (Yanlış):  #define CUSTOM_DATA_SERVICE_UUID    0x1234
 * YENİ (Doğru):   static const uint8_t CUSTOM_DATA_SERVICE_UUID[] = {0x34, 0x12};
 *
 * Neden değişti?
 * - STM32WB BLE stack UUID'leri byte array olarak bekler
 * - Little-endian format kullanılır
 * - Bu format 0x98 hatasını önler
 */

static const uint8_t CUSTOM_DATA_SERVICE_UUID[] = {0x34, 0x12};  // 0x1234
static const uint8_t DATA_STORAGE_CHAR_UUID[] = {0x35, 0x12};    // 0x1235
static const uint8_t DATA_COMMAND_CHAR_UUID[] = {0x36, 0x12};    // 0x1236
static const uint8_t DATA_STATUS_CHAR_UUID[] = {0x37, 0x12};     // 0x1237

// =====================================================================
// PROTOKOL SABİTLERİ
// =====================================================================
#define DATA_STORAGE_SIZE       30      // Ana veri deposu (30 byte)
#define COMMAND_SIZE            2       // Komut boyutu (2 byte)
#define STATUS_SIZE             1       // Durum boyutu (1 byte)

// Komut protokolü
#define CMD_CLEAR_DATA_BYTE1    0xDE    // "DELETE"
#define CMD_CLEAR_DATA_BYTE2    0xAD    // "reAD done"

// =====================================================================
// DURUM KODLARI
// =====================================================================
typedef enum {
    DATA_STATUS_INITIALIZING = 0x01,    // Başlatılıyor
    DATA_STATUS_READY = 0x02,           // Hazır
    DATA_STATUS_READING = 0x03,         // Okunuyor
    DATA_STATUS_PROCESSING = 0x04,      // İşleniyor
    DATA_STATUS_GENERATING = 0x05,      // Üretiliyor
    DATA_STATUS_ERROR = 0xFF            // Hata
} DataStatus_t;

// =====================================================================
// VERİ YAPILARI - MİNİMAL VERSİYON
// =====================================================================

/*
 * Struct Optimizasyonu:
 *
 * Kaldırılan field'lar (minimal version için):
 * - CommandCharHandle (henüz eklenmedi)
 * - StatusCharHandle (henüz eklenmedi)
 * - ReadCount (kullanılmıyor)
 *
 * Kalan field'lar (çalışan özellikler):
 * - ServiceHandle, StorageCharHandle
 * - StorageData, CurrentStatus
 * - GenerateCount, ClearCount
 */
typedef struct {
    // GATT Handle'ları (sadece aktif olanlar)
    uint16_t ServiceHandle;         // Ana service handle
    uint16_t StorageCharHandle;     // Storage characteristic handle
    // CommandCharHandle - minimal versiyonda yok
    // StatusCharHandle - minimal versiyonda yok

    // Veri depolama
    uint8_t StorageData[DATA_STORAGE_SIZE];  // 30 byte veri
    DataStatus_t CurrentStatus;              // Mevcut durum

    // İstatistikler (sadece kullanılanlar)
    uint32_t GenerateCount;         // Kaç kez veri üretildi
    uint32_t ClearCount;            // Kaç kez temizlendi
    // ReadCount kaldırıldı (kullanılmıyor)
} CustomDataService_Context_t;

// =====================================================================
// FONKSİYON TANIMLARI - MİNİMAL VERSİYON
// =====================================================================

/*
 * Fonksiyon Listesi:
 *
 * Aktif fonksiyonlar (çalışıyor):
 * - Init, GenerateData, ClearData, UpdateStatus
 * - PrintData, PrintStatus (debug)
 *
 * Minimal event handler:
 * - Event_Handler (basitleştirilmiş)
 */

// Ana servis fonksiyonları
tBleStatus CustomDataService_Init(void);
tBleStatus CustomDataService_Event_Handler(void *Event);

// Veri yönetimi
void CustomDataService_GenerateData(void);
void CustomDataService_ClearData(void);
tBleStatus CustomDataService_UpdateStatus(DataStatus_t status);

// Debug fonksiyonları
void CustomDataService_PrintData(const char* prefix);
void CustomDataService_PrintStatus(void);

/*
 * GELECEK EKLEMELERİ İÇİN NOTLAR:
 *
 * Service başarıyla çalıştığında eklenecekler:
 * 1. Command characteristic → struct'a CommandCharHandle ekle
 * 2. Status characteristic → struct'a StatusCharHandle ekle
 * 3. Full event handler → command processing ekle
 * 4. ReadCount istatistiği → gerekirse geri ekle
 *
 * Bu aşamalı yaklaşım 0x98 hatasını önler.
 */

#endif /* CUSTOM_DATA_SERVICE_H */

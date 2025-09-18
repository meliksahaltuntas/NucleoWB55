/*
 * =====================================================================
 * CUSTOM DATA SERVICE - RESOURCE OPTIMIZED VERSION
 * =====================================================================
 *
 * Bu versiyon ne değişti?
 * - Daha az GATT resource kullanır (0x98 hatası için)
 * - Sadece gerekli karakteristikler eklenir
 * - Bellekte daha az yer kaplar
 * - Test için basitleştirildi
 */

// =====================================================================
// INCLUDE'LAR
// =====================================================================
#include "custom_data_service.h"
#include "app_common.h"
#include "dbg_trace.h"
#include <string.h>
#include <stdlib.h>

// =====================================================================
// GLOBAL DEĞİŞKENLER
// =====================================================================
static CustomDataService_Context_t ServiceContext;

// =====================================================================
// RESOURCE OPTIMIZED SERVICE INIT
// =====================================================================
tBleStatus CustomDataService_Init(void) {
    tBleStatus ret = BLE_STATUS_SUCCESS;

    APP_DBG_MSG("\n===== CUSTOM DATA SERVICE (OPTIMIZED) =====\n");
    APP_DBG_MSG("Resource optimized version başlatılıyor...\n");

    // Context temizle
    memset(&ServiceContext, 0, sizeof(ServiceContext));
    ServiceContext.CurrentStatus = DATA_STATUS_INITIALIZING;

    /*
     * ANA SERVICE - MINIMAL RESOURCE
     *
     * max_attribute_records = 1 (sadece 1 characteristic için)
     * Bu şekilde daha az bellek kullanırız
     */
    ret = aci_gatt_add_service(UUID_TYPE_16,
                              (Service_UUID_t*)CUSTOM_DATA_SERVICE_UUID,
                              PRIMARY_SERVICE,
                              1,  // Sadece 1 characteristic (storage)
                              &ServiceContext.ServiceHandle);

    if (ret != BLE_STATUS_SUCCESS) {
        APP_DBG_MSG("HATA: Service oluşturulamadı: 0x%x\n", ret);
        APP_DBG_MSG("Bellek yetersiz olabilir (0x98 = INSUFFICIENT_RESOURCES)\n");
        return ret;
    }

    APP_DBG_MSG("✓ Service oluşturuldu, Handle: 0x%04X\n", ServiceContext.ServiceHandle);

    /*
     * SADECE STORAGE CHARACTERISTIC
     *
     * İlk test için sadece veri deposu karakteristiğini ekleyelim
     * Command ve Status karakteristikleri daha sonra eklenebilir
     */
    ret = aci_gatt_add_char(ServiceContext.ServiceHandle,
                           UUID_TYPE_16,
                           (Char_UUID_t*)DATA_STORAGE_CHAR_UUID,
                           DATA_STORAGE_SIZE,                    // 30 byte
                           CHAR_PROP_READ,                       // Sadece READ (NOTIFY kaldırıldı)
                           ATTR_PERMISSION_NONE,
                           GATT_DONT_NOTIFY_EVENTS,
                           16,
                           CHAR_VALUE_LEN_CONSTANT,
                           &ServiceContext.StorageCharHandle);

    if (ret != BLE_STATUS_SUCCESS) {
        APP_DBG_MSG("HATA: Storage char eklenemedi: 0x%x\n", ret);
        return ret;
    }

    APP_DBG_MSG("✓ Storage char eklendi, Handle: 0x%04X\n", ServiceContext.StorageCharHandle);

    // İlk veriyi üret
    CustomDataService_GenerateData();
    CustomDataService_UpdateStatus(DATA_STATUS_READY);

    APP_DBG_MSG("===== CUSTOM SERVICE HAZIR (MINIMAL) =====\n\n");
    return BLE_STATUS_SUCCESS;
}

// =====================================================================
// EVENT HANDLER - SİMPLİFİED
// =====================================================================
tBleStatus CustomDataService_Event_Handler(void *Event) {
    // Şimdilik sadece log, karakteristik ekleme sorunu çözülünce genişletiriz
    APP_DBG_MSG(">>> Custom Service Event (minimal handler)\n");
    return BLE_STATUS_SUCCESS;
}

// =====================================================================
// VERİ YÖNETİMİ - BASIC VERSION
// =====================================================================
void CustomDataService_GenerateData(void) {
    APP_DBG_MSG("\n=== RASTGELE VERİ ÜRETİLİYOR (30 byte) ===\n");

    // Status güncelle (sadece local context'te)
    ServiceContext.CurrentStatus = DATA_STATUS_GENERATING;

    // Rastgele veri üret
    srand(HAL_GetTick());
    for (int i = 0; i < DATA_STORAGE_SIZE; i++) {
        ServiceContext.StorageData[i] = rand() % 256;
    }

    // İstatistik
    ServiceContext.GenerateCount++;

    // GATT'a veriyi yaz
    tBleStatus ret = aci_gatt_update_char_value(
        ServiceContext.ServiceHandle,
        ServiceContext.StorageCharHandle,
        0,
        DATA_STORAGE_SIZE,
        ServiceContext.StorageData
    );

    if (ret == BLE_STATUS_SUCCESS) {
        APP_DBG_MSG("✓ Veri GATT'a yazıldı (Handle: 0x%04X)\n", ServiceContext.StorageCharHandle);
    } else {
        APP_DBG_MSG("✗ GATT yazma hatası: 0x%x\n", ret);
    }

    // Debug için veriyi yazdır
    CustomDataService_PrintData("SERVER ÜRETTI");

    APP_DBG_MSG("Üretim sayısı: %lu\n", ServiceContext.GenerateCount);
    APP_DBG_MSG("===============================\n\n");

    // Status hazır
    ServiceContext.CurrentStatus = DATA_STATUS_READY;
}

void CustomDataService_ClearData(void) {
    APP_DBG_MSG("--- VERİ TEMİZLENİYOR ---\n");

    memset(ServiceContext.StorageData, 0, DATA_STORAGE_SIZE);
    ServiceContext.ClearCount++;

    APP_DBG_MSG("✓ 30 byte sıfırlandı\n");
    APP_DBG_MSG("Temizleme sayısı: %lu\n", ServiceContext.ClearCount);
    APP_DBG_MSG("-------------------------\n");
}

tBleStatus CustomDataService_UpdateStatus(DataStatus_t new_status) {
    ServiceContext.CurrentStatus = new_status;

    // Status names
    const char* status_names[] = {
        "UNKNOWN",          // 0
        "INITIALIZING",     // 1
        "READY",            // 2
        "READING",          // 3
        "PROCESSING",       // 4
        "GENERATING"        // 5
    };

    const char* status_name;
    if (new_status == DATA_STATUS_ERROR) {
        status_name = "ERROR";
    } else if (new_status <= 5) {
        status_name = status_names[new_status];
    } else {
        status_name = "UNKNOWN";
    }

    APP_DBG_MSG("→ Server status: %s (%d)\n", status_name, new_status);

    // Status karakteristiği olmadığı için GATT güncelleme yok
    return BLE_STATUS_SUCCESS;
}

// =====================================================================
// DEBUG FONKSİYONLARI
// =====================================================================
void CustomDataService_PrintData(const char* prefix) {
    APP_DBG_MSG("\n--- %s VERİ (30 byte) ---\n", prefix);

    // Hex formatında yazdır
    for (int i = 0; i < DATA_STORAGE_SIZE; i++) {
        if (i % 8 == 0) {
            APP_DBG_MSG("\n[%02d-%02d]: ", i,
                       (i+7 < DATA_STORAGE_SIZE) ? i+7 : DATA_STORAGE_SIZE-1);
        }
        APP_DBG_MSG("%02X ", ServiceContext.StorageData[i]);
    }

    APP_DBG_MSG("\n\n");

    // Checksum hesapla
    uint16_t checksum = 0;
    for (int i = 0; i < DATA_STORAGE_SIZE; i++) {
        checksum += ServiceContext.StorageData[i];
    }

    APP_DBG_MSG("Checksum: 0x%04X (%d)\n", checksum, checksum);
    APP_DBG_MSG("Timestamp: %lu ms\n", HAL_GetTick());
    APP_DBG_MSG("------------------------\n");
}

void CustomDataService_PrintStatus(void) {
    APP_DBG_MSG("\n=== CUSTOM SERVICE STATUS (MINIMAL) ===\n");
    APP_DBG_MSG("Status: %d\n", ServiceContext.CurrentStatus);
    APP_DBG_MSG("Üretim: %lu\n", ServiceContext.GenerateCount);
    APP_DBG_MSG("Temizleme: %lu\n", ServiceContext.ClearCount);
    APP_DBG_MSG("Service Handle: 0x%04X\n", ServiceContext.ServiceHandle);
    APP_DBG_MSG("Storage Handle: 0x%04X\n", ServiceContext.StorageCharHandle);
    APP_DBG_MSG("Command Handle: NONE (minimal version)\n");
    APP_DBG_MSG("Status Handle: NONE (minimal version)\n");
    APP_DBG_MSG("=====================================\n\n");
}

/*
 * GELECEK GENIŞLETMELER İÇİN PLACEHOLDER'LAR
 *
 * Service başarıyla çalıştığında bu fonksiyonları ekleyebiliriz:
 * - Command characteristic ekleme
 * - Status characteristic ekleme
 * - Full event handler
 * - Command processing
 */

// TODO: Command characteristic ekle (service çalışınca)
/*
static tBleStatus AddCommandCharacteristic(void) {
    // Command char eklemek için
    return BLE_STATUS_SUCCESS;
}
*/

// TODO: Status characteristic ekle (service çalışınca)
/*
static tBleStatus AddStatusCharacteristic(void) {
    // Status char eklemek için
    return BLE_STATUS_SUCCESS;
}
*/

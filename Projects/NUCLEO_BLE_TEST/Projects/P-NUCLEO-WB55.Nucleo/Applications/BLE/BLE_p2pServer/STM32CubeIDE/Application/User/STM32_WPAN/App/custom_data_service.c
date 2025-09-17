/*
 * =====================================================================
 * CUSTOM DATA SERVICE IMPLEMENTATION DOSYASI
 * =====================================================================
 *
 * Bu dosya nedir?
 * - Header'da tanımlanan fonksiyonların gerçek kodları
 * - Asıl işi yapan fonksiyonlar burada
 * - "Aşçının tariflerden yemek pişirmesi" gibi
 *
 * Dosya yapısı:
 * 1. Include'lar (diğer dosyaları dahil etme)
 * 2. Global değişkenler (tüm fonksiyonların erişebildiği)
 * 3. Fonksiyon implementasyonları
 */

// =====================================================================
// DİĞER DOSYALARI DAHIL ETME
// =====================================================================
#include "custom_data_service.h"    // Kendi header'ımız
#include "app_common.h"             // STM32WB ortak tanımları
#include "dbg_trace.h"              // Debug mesajları için
#include <string.h>                 // memset, memcpy gibi fonksiyonlar
#include <stdlib.h>                 // rand, srand gibi fonksiyonlar

// =====================================================================
// GLOBAL DEĞİŞKENLER
// =====================================================================

/*
 * Global değişken nedir?
 * - Tüm fonksiyonların erişebildiği değişken
 * - Program boyunca yaşar
 * - static = sadece bu dosyadan erişilebilir (güvenlik)
 *
 * Neden global?
 * - Servis bilgileri her fonksiyonda gerekli
 * - BLE handle'ları kaybolmamalı
 */
static CustomDataService_Context_t ServiceContext;  // Ana servis bilgileri

// =====================================================================
// SERVİS BAŞLATMA FONKSİYONU
// =====================================================================

/*
 * Bu fonksiyon ne yapar?
 * - BLE Custom Service'i oluşturur
 * - 3 karakteristik ekler (veri deposu, komut, durum)
 * - İlk veriyi üretir
 * - Program başında bir kez çalışır
 */
tBleStatus CustomDataService_Init(void) {
    tBleStatus ret = BLE_STATUS_SUCCESS;  // İşlem sonucu değişkeni

    APP_DBG_MSG("\n===== CUSTOM DATA SERVICE BAŞLATILIYOR =====\n");
    APP_DBG_MSG("30 byte veri saklayan service oluşturuluyor...\n");

    /*
     * ADIM 1: Context'i temizle
     *
     * memset nedir?
     * - Bellek alanını belirli bir değerle doldurur
     * - memset(&ServiceContext, 0, sizeof(...)) = tüm alanı sıfırla
     * - Başlangıç değerleri temiz olsun diye
     */
    memset(&ServiceContext, 0, sizeof(ServiceContext));
    ServiceContext.CurrentStatus = DATA_STATUS_INITIALIZING;

    /*
     * ADIM 2: Ana Service'i oluştur
     *
     * aci_gatt_add_service parametreleri:
     * - UUID_TYPE_16: 16-bit UUID kullanıyoruz
     * - CUSTOM_DATA_SERVICE_UUID: Servisimizin kimliği (0x1234)
     * - PRIMARY_SERVICE: Ana servis (alt servis değil)
     * - 4: Kaç karakteristik olacak (veri + komut + durum = 3+1)
     * - &ServiceContext.ServiceHandle: Sonuç buraya yazılacak
     */
    ret = aci_gatt_add_service(UUID_TYPE_16,
                              (Service_UUID_t*)CUSTOM_DATA_SERVICE_UUID,
                              PRIMARY_SERVICE,
                              4,  // 3 karakteristik + 1 servis bilgisi
                              &ServiceContext.ServiceHandle);

    // İşlem başarılı mı kontrol et
    if (ret != BLE_STATUS_SUCCESS) {
        APP_DBG_MSG("HATA: Service oluşturulamadı, kod: 0x%x\n", ret);
        return ret;  // Hata varsa çık
    }

    APP_DBG_MSG("✓ Ana service oluşturuldu, Handle: 0x%04X\n", ServiceContext.ServiceHandle);

    /*
     * ADIM 3: Storage Characteristic (30 byte veri deposu)
     *
     * Bu karakteristik:
     * - 30 byte veri saklar
     * - Client tarafından okunabilir (READ)
     * - Veri değişince bildirim gönderebilir (NOTIFY)
     */
    ret = aci_gatt_add_char(ServiceContext.ServiceHandle,        // Hangi service'e
                           UUID_TYPE_16,                         // UUID tipi
                           (Char_UUID_t*)DATA_STORAGE_CHAR_UUID, // UUID değeri
                           DATA_STORAGE_SIZE,                    // 30 byte
                           CHAR_PROP_READ | CHAR_PROP_NOTIFY,    // Okunabilir + bildirim
                           ATTR_PERMISSION_NONE,                 // Özel izin yok
                           GATT_DONT_NOTIFY_EVENTS,              // Event otomatik gelmesin
                           16,                                   // Encryption key boyutu
                           CHAR_VALUE_LEN_CONSTANT,              // Sabit uzunluk
                           &ServiceContext.StorageCharHandle);   // Handle buraya

    if (ret != BLE_STATUS_SUCCESS) {
        APP_DBG_MSG("HATA: Storage karakteristiği eklenemedi: 0x%x\n", ret);
        return ret;
    }

    APP_DBG_MSG("✓ Storage karakteristiği eklendi, Handle: 0x%04X\n", ServiceContext.StorageCharHandle);

    /*
     * ADIM 4: Command Characteristic (komut kanalı)
     *
     * Bu karakteristik:
     * - 2 byte komut alır
     * - Client tarafından yazılabilir (WRITE)
     * - Yazıldığında event tetiklenir
     */
    ret = aci_gatt_add_char(ServiceContext.ServiceHandle,
                           UUID_TYPE_16,
                           (Char_UUID_t*)DATA_COMMAND_CHAR_UUID,
                           COMMAND_SIZE,                         // 2 byte
                           CHAR_PROP_WRITE,                      // Sadece yazılabilir
                           ATTR_PERMISSION_NONE,
                           GATT_NOTIFY_WRITE_REQ_AND_WAIT_FOR_APPL_RESP, // Yazma event'i gönder
                           16,
                           CHAR_VALUE_LEN_CONSTANT,
                           &ServiceContext.CommandCharHandle);

    if (ret != BLE_STATUS_SUCCESS) {
        APP_DBG_MSG("HATA: Command karakteristiği eklenemedi: 0x%x\n", ret);
        return ret;
    }

    APP_DBG_MSG("✓ Command karakteristiği eklendi, Handle: 0x%04X\n", ServiceContext.CommandCharHandle);

    /*
     * ADIM 5: Status Characteristic (durum bildirimi)
     *
     * Bu karakteristik:
     * - 1 byte durum bilgisi
     * - Client okuyabilir + notification alabilir
     */
    ret = aci_gatt_add_char(ServiceContext.ServiceHandle,
                           UUID_TYPE_16,
                           (Char_UUID_t*)DATA_STATUS_CHAR_UUID,
                           STATUS_SIZE,                          // 1 byte
                           CHAR_PROP_READ | CHAR_PROP_NOTIFY,    // Okunabilir + bildirim
                           ATTR_PERMISSION_NONE,
                           GATT_DONT_NOTIFY_EVENTS,
                           16,
                           CHAR_VALUE_LEN_CONSTANT,
                           &ServiceContext.StatusCharHandle);

    if (ret != BLE_STATUS_SUCCESS) {
        APP_DBG_MSG("HATA: Status karakteristiği eklenemedi: 0x%x\n", ret);
        return ret;
    }

    APP_DBG_MSG("✓ Status karakteristiği eklendi, Handle: 0x%04X\n", ServiceContext.StatusCharHandle);

    /*
     * ADIM 6: İlk veriyi oluştur ve durumu güncelle
     */
    CustomDataService_GenerateData();                    // İlk veri
    CustomDataService_UpdateStatus(DATA_STATUS_READY);   // Hazır durumuna geç

    APP_DBG_MSG("===== CUSTOM DATA SERVICE HAZIR =====\n\n");
    return BLE_STATUS_SUCCESS;
}

// =====================================================================
// EVENT HANDLER FONKSİYONU
// =====================================================================

/*
 * Event Handler nedir?
 * - BLE sisteminden gelen olayları işleyen fonksiyon
 * - Örnek olaylar: Client yazdı, okudu, bağlandı
 * - Event = Olay, Handler = İşleyici
 *
 * Bu fonksiyon ne zaman çalışır?
 * - Client bir şey yazdığında
 * - Client okuduğunda
 * - Bağlantı durumu değiştiğinde
 */
tBleStatus CustomDataService_Event_Handler(void *Event) {
    APP_DBG_MSG(">>> Event geldi, işleniyor...\n");

    /*
     * Event'i parse etme (ayrıştırma)
     *
     * Event karmaşık bir yapı, içinden bilgiyi çıkarmalıyız
     * Sanki mektubun zarfını açıp içindekileri okumak gibi
     */
    aci_gatt_attribute_modified_event_rp0 *attribute_modified;

    // Event paketini ayrıştır
    hci_uart_pckt *event_pckt = (hci_uart_pckt*)Event;
    hci_event_pckt *event = (hci_event_pckt*)event_pckt->data;

    /*
     * Event tipini kontrol et
     *
     * HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE = STM32WB özel event'leri
     * Bizim ilgilendiğimiz event'ler bu kategoride
     */
    if(event->evt == HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE) {
        evt_blecore_aci *blecore_evt = (evt_blecore_aci*)event->data;

        /*
         * Attribute modified event mı kontrol et
         *
         * ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE = "Bir karakteristiğe yazıldı"
         * Bizim command karakteristiğimize yazıldı mı bakacağız
         */
        if(blecore_evt->ecode == ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE) {
            attribute_modified = (aci_gatt_attribute_modified_event_rp0*)blecore_evt->data;

            APP_DBG_MSG("→ Bir karakteristiğe yazıldı!\n");
            APP_DBG_MSG("  Handle: 0x%04X\n", attribute_modified->Attr_Handle);
            APP_DBG_MSG("  Uzunluk: %d byte\n", attribute_modified->Attr_Data_Length);

            /*
             * Command karakteristiğine yazıldı mı kontrol et
             *
             * ÖNEMLI: Handle+1 kontrolü
             * GATT sisteminde karakteristiğin handle'ı X ise
             * asıl veri X+1 handle'ında saklanır
             */
            if(attribute_modified->Attr_Handle == (ServiceContext.CommandCharHandle + 1)) {
                APP_DBG_MSG("✓ Command karakteristiğine yazıldı!\n");

                // Veri uzunluğu doğru mu kontrol et
                if (attribute_modified->Attr_Data_Length == COMMAND_SIZE) {

                    // Veriyi yazdır
                    APP_DBG_MSG("  Gelen veri: [0x%02X, 0x%02X]\n",
                               attribute_modified->Attr_Data[0],
                               attribute_modified->Attr_Data[1]);

                    /*
                     * Clear komutu mu kontrol et
                     *
                     * Beklediğimiz komut: [0xDE, 0xAD]
                     * DE = "DELETE", AD = "reAD done"
                     */
                    if (attribute_modified->Attr_Data[0] == CMD_CLEAR_DATA_BYTE1 &&
                        attribute_modified->Attr_Data[1] == CMD_CLEAR_DATA_BYTE2) {

                        APP_DBG_MSG("✓ CLEAR DATA komutu doğrulandı!\n");
                        APP_DBG_MSG("  Anlamı: Client veriyi okudu, yenisini üretebiliriz\n");

                        // Durumu güncelle
                        CustomDataService_UpdateStatus(DATA_STATUS_PROCESSING);

                        // Eski veriyi temizle
                        CustomDataService_ClearData();

                        // Yeni veri üret
                        CustomDataService_GenerateData();

                        // Hazır durumuna dön
                        CustomDataService_UpdateStatus(DATA_STATUS_READY);

                        ServiceContext.ClearCount++;  // İstatistik

                    } else {
                        APP_DBG_MSG("✗ Bilinmeyen komut!\n");
                        APP_DBG_MSG("  Beklenen: [0x%02X, 0x%02X]\n",
                                   CMD_CLEAR_DATA_BYTE1, CMD_CLEAR_DATA_BYTE2);
                        CustomDataService_UpdateStatus(DATA_STATUS_ERROR);
                    }

                } else {
                    APP_DBG_MSG("✗ Yanlış veri uzunluğu: %d (beklenen: %d)\n",
                               attribute_modified->Attr_Data_Length, COMMAND_SIZE);
                    CustomDataService_UpdateStatus(DATA_STATUS_ERROR);
                }

                return BLE_STATUS_SUCCESS;
            }
        }
    }

    return BLE_STATUS_SUCCESS;
}

// =====================================================================
// VERİ YÖNETİMİ FONKSİYONLARI
// =====================================================================

/*
 * Rastgele veri üretme fonksiyonu
 *
 * Bu fonksiyon ne yapar?
 * - 30 byte rastgele sayı üretir
 * - Bu veriyi storage array'ine kaydeder
 * - GATT karakteristiğini günceller (Client görebilsin)
 * - Debug için veriyi ekrana yazdırır
 */
void CustomDataService_GenerateData(void) {
    APP_DBG_MSG("\n=== YENİ RASTGELE VERİ ÜRETİLİYOR ===\n");

    // Durumu güncelle
    CustomDataService_UpdateStatus(DATA_STATUS_GENERATING);

    /*
     * Rastgele sayı üretme
     *
     * srand(HAL_GetTick()) = rastgele sayı çekirdeğini ayarla
     * HAL_GetTick() = sistem başladığından beri geçen milisaniye
     * rand() % 256 = 0-255 arası rastgele sayı
     */
    srand(HAL_GetTick());  // Seed'i ayarla

    for (int i = 0; i < DATA_STORAGE_SIZE; i++) {
        ServiceContext.StorageData[i] = rand() % 256;  // 0-255 arası
    }

    // İstatistiği güncelle
    ServiceContext.GenerateCount++;

    /*
     * GATT karakteristiğini güncelle
     *
     * aci_gatt_update_char_value nedir?
     * - GATT karakteristiğinin değerini günceller
     * - Client bu veriyi okuyabilir
     * - Notification aktifse otomatik gönderilir
     */
    tBleStatus ret = aci_gatt_update_char_value(
        ServiceContext.ServiceHandle,           // Hangi service
        ServiceContext.StorageCharHandle,       // Hangi karakteristik
        0,                                      // Offset (başlangıç noktası)
        DATA_STORAGE_SIZE,                      // Kaç byte
        ServiceContext.StorageData              // Veri pointer'ı
    );

    if (ret == BLE_STATUS_SUCCESS) {
        APP_DBG_MSG("✓ Veri GATT sistemine yazıldı\n");
    } else {
        APP_DBG_MSG("✗ GATT güncelleme hatası: 0x%x\n", ret);
    }

    // Veriyi ekrana yazdır (debug için)
    CustomDataService_PrintData("SERVER ÜRETTİĞİ");

    APP_DBG_MSG("Veri üretim sayısı: %lu\n", ServiceContext.GenerateCount);
    APP_DBG_MSG("===============================\n\n");
}

/*
 * Veri temizleme fonksiyonu
 *
 * Bu fonksiyon ne yapar?
 * - 30 byte'lık veriyi sıfırlar
 * - İstatistiği günceller
 * - Debug mesajı yazdırır
 */
void CustomDataService_ClearData(void) {
    APP_DBG_MSG("--- VERİ TEMİZLENİYOR ---\n");

    /*
     * memset fonksiyonu
     *
     * memset(array, değer, boyut) = array'i belirtilen değerle doldur
     * memset(..., 0, ...) = sıfırla
     */
    memset(ServiceContext.StorageData, 0, DATA_STORAGE_SIZE);

    // İstatistik güncelle
    ServiceContext.ClearCount++;

    APP_DBG_MSG("✓ 30 byte veri sıfırlandı\n");
    APP_DBG_MSG("Toplam temizleme sayısı: %lu\n", ServiceContext.ClearCount);
    APP_DBG_MSG("-------------------------\n");
}

/*
 * Durum güncelleme fonksiyonu
 *
 * Bu fonksiyon ne yapar?
 * - Servisin mevcut durumunu günceller
 * - GATT status karakteristiğini günceller
 * - Debug mesajı yazdırır
 */
tBleStatus CustomDataService_UpdateStatus(DataStatus_t new_status) {
    ServiceContext.CurrentStatus = new_status;

    // GATT status karakteristiğini güncelle
    tBleStatus ret = aci_gatt_update_char_value(
        ServiceContext.ServiceHandle,
        ServiceContext.StatusCharHandle,
        0,
        STATUS_SIZE,                        // 1 byte
        (uint8_t*)&new_status               // Status değerini gönder
    );

    // Durum ismini bul ve yazdır
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

    APP_DBG_MSG("→ Server durumu: %s (%d)\n", status_name, new_status);

    return ret;
}

// =====================================================================
// DEBUG FONKSİYONLARI
// =====================================================================

/*
 * Veri yazdırma fonksiyonu
 *
 * Bu fonksiyon ne yapar?
 * - 30 byte veriyi hex formatında ekrana yazdırır
 * - Checksum (kontrol toplamı) hesaplar
 * - Zaman damgası ekler
 * - Debug amaçlı, veri doğruluğunu kontrol için
 */
void CustomDataService_PrintData(const char* prefix) {
    APP_DBG_MSG("\n--- %s VERİ (30 byte) ---\n", prefix);

    /*
     * Veriyi hex formatında yazdır
     *
     * %02X = 2 haneli hex, başına 0 ekle
     * Örnek: 15 → "0F", 255 → "FF"
     */
    for (int i = 0; i < DATA_STORAGE_SIZE; i++) {
        // Her 8 byte'da yeni satır
        if (i % 8 == 0) {
            APP_DBG_MSG("\n[%02d-%02d]: ", i,
                       (i+7 < DATA_STORAGE_SIZE) ? i+7 : DATA_STORAGE_SIZE-1);
        }
        APP_DBG_MSG("%02X ", ServiceContext.StorageData[i]);
    }

    APP_DBG_MSG("\n\n");

    /*
     * Checksum hesaplama
     *
     * Checksum nedir?
     * - Verilerin doğruluğunu kontrol etme yöntemi
     * - Tüm byte'ları topla
     * - Server ve Client aynı checksum'ı bulmalı
     */
    uint16_t checksum = 0;
    for (int i = 0; i < DATA_STORAGE_SIZE; i++) {
        checksum += ServiceContext.StorageData[i];
    }

    APP_DBG_MSG("Checksum: 0x%04X (%d decimal)\n", checksum, checksum);
    APP_DBG_MSG("Timestamp: %lu ms\n", HAL_GetTick());
    APP_DBG_MSG("------------------------\n");
}

/*
 * Durum yazdırma fonksiyonu
 *
 * Bu fonksiyon ne yapar?
 * - Servisin mevcut durumunu yazdırır
 * - İstatistikleri gösterir
 * - Debug bilgileri verir
 */
void CustomDataService_PrintStatus(void) {
    APP_DBG_MSG("\n=== CUSTOM DATA SERVICE DURUMU ===\n");
    APP_DBG_MSG("Mevcut Durum: %d\n", ServiceContext.CurrentStatus);
    APP_DBG_MSG("Okuma Sayısı: %lu\n", ServiceContext.ReadCount);
    APP_DBG_MSG("Temizleme Sayısı: %lu\n", ServiceContext.ClearCount);
    APP_DBG_MSG("Üretim Sayısı: %lu\n", ServiceContext.GenerateCount);
    APP_DBG_MSG("Service Handle: 0x%04X\n", ServiceContext.ServiceHandle);
    APP_DBG_MSG("Storage Handle: 0x%04X\n", ServiceContext.StorageCharHandle);
    APP_DBG_MSG("Command Handle: 0x%04X\n", ServiceContext.CommandCharHandle);
    APP_DBG_MSG("Status Handle: 0x%04X\n", ServiceContext.StatusCharHandle);
    APP_DBG_MSG("================================\n\n");
}

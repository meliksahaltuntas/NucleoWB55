/*
 * =====================================================================
 * CUSTOM DATA SERVICE HEADER DOSYASI
 * =====================================================================
 *
 * Bu dosya nedir?
 * - Header dosyası = Diğer dosyaların görebileceği tanımlar
 * - Fonksiyon isimleri (ne yapacakları)
 * - Sabit değerler (UUID'ler, boyutlar)
 * - Veri yapıları (struct'lar)
 *
 * Neden gerekli?
 * - C dilinde fonksiyonu kullanmadan önce tanımlanması gerekir
 * - Bu dosya "sözlük" gibi, hangi fonksiyonlar var söyler
 */

#ifndef CUSTOM_DATA_SERVICE_H  // Bu satır "include guard" - dosyanın iki kez okunmasını önler
#define CUSTOM_DATA_SERVICE_H  // Eğer bu tanım yoksa dosyayı oku, varsa atlama

// =====================================================================
// DİĞER DOSYALARI DAHIL ETME (Include)
// =====================================================================

/*
 * Include nedir?
 * - Başka dosyalardaki kodları bu dosyaya dahil etme
 * - #include "ble.h" = ble.h dosyasındaki tüm tanımları getir
 * - Sanki copy-paste yapmak gibi
 */
#include "ble.h"        // BLE temel fonksiyonları için
#include "ble_types.h"  // BLE veri tipleri için
#include "hci_tl.h"
#include "ble_events.h"

// =====================================================================
// UUID (Benzersiz Kimlik Numaraları) TANIMLARI
// =====================================================================

/*
 * UUID nedir?
 * - Universally Unique Identifier = Evrensel Benzersiz Tanımlayıcı
 * - Her BLE servisi ve karakteristiğinin kendine özel kimlik numarası
 * - Sanki her kişinin TC kimlik numarası olması gibi
 *
 * Neden 16-bit?
 * - 16-bit UUID = 2 byte, hafızada az yer kaplar
 * - Hızlı karşılaştırma yapılır
 * - Test amaçlı projeler için yeterli
 *
 * UUID değerleri nereden geliyor?
 * - 0x1234, 0x1235... sıralı seçtik
 * - Gerçek projede rastgele olmalı
 */

#define CUSTOM_DATA_SERVICE_UUID    0x1234
#define DATA_STORAGE_CHAR_UUID      0x1235
#define DATA_COMMAND_CHAR_UUID      0x1236
#define DATA_STATUS_CHAR_UUID       0x1237

// =====================================================================
// PROTOKOL SABİTLERİ
// =====================================================================

/*
 * Protokol nedir?
 * - İki cihaz arasındaki iletişim kuralları
 * - Hangi byte ne anlama geliyor
 * - Hangi komut ne yapmak için
 *
 * #define nedir?
 * - Sabit değer tanımlama
 * - #define ISIM DEGER şeklinde
 * - Kod yazarken ISIM yazdığınızda DEGER yerine geçer
 * - Örnek: DATA_STORAGE_SIZE yazdığında 30 anlaşılır
 */
#define DATA_STORAGE_SIZE       30      // Ana veri deposu boyutu (30 byte)
#define COMMAND_SIZE            2       // Komut boyutu (2 byte)
#define STATUS_SIZE             1       // Durum bilgisi boyutu (1 byte)

// Komut protokolü - Client'ın Server'a göndereceği komutlar
#define CMD_CLEAR_DATA_BYTE1    0xDE    // İlk byte: "DELETE" anlamında
#define CMD_CLEAR_DATA_BYTE2    0xAD    // İkinci byte: "reAD done" anlamında
// Yani [0xDE, 0xAD] = "Veriyi okudum, silebilirsin"

// =====================================================================
// DURUM KODLARI (Enum)
// =====================================================================

/*
 * Enum nedir?
 * - Sabit değerlere isim verme yöntemi
 * - Sayılar yerine anlamlı isimler kullanırız
 * - Örnek: 2 yazmak yerine DATA_STATUS_READY yazmak
 *
 * typedef enum nedir?
 * - enum'a yeni bir isim veriyoruz (DataStatus_t)
 * - Artık DataStatus_t tipinde değişken tanımlayabiliriz
 */
typedef enum {
    DATA_STATUS_INITIALIZING = 0x01,    // Başlatılıyor (1)
    DATA_STATUS_READY = 0x02,           // Veri hazır (2)
    DATA_STATUS_READING = 0x03,         // Client okuyor (3)
    DATA_STATUS_PROCESSING = 0x04,      // Komut işleniyor (4)
    DATA_STATUS_GENERATING = 0x05,      // Yeni veri üretiliyor (5)
    DATA_STATUS_ERROR = 0xFF            // Hata durumu (255)
} DataStatus_t;

// =====================================================================
// VERİ YAPILARI (Struct)
// =====================================================================

/*
 * Struct nedir?
 * - İlgili verileri bir arada tutan paket
 * - Örnek: Kişi struct'ı = isim + yaş + adres
 * - Bizim struct'ımız = servis bilgileri
 *
 * Neden struct kullanıyoruz?
 * - İlgili veriler dağınık olmasın
 * - Kolay yönetim
 * - Kod okunabilirliği
 */
typedef struct {
    // GATT Handle'ları - BLE sisteminin verdiği kimlik numaraları
    uint16_t ServiceHandle;         // Ana servisin kimliği
    uint16_t StorageCharHandle;     // Veri deposu karakteristiğinin kimliği
    uint16_t CommandCharHandle;     // Komut karakteristiğinin kimliği
    uint16_t StatusCharHandle;      // Durum karakteristiğinin kimliği

    // Veri depolama alanları
    uint8_t StorageData[DATA_STORAGE_SIZE];  // 30 byte'lık veri deposu
    DataStatus_t CurrentStatus;              // Şu anki durum

    // İstatistikler (debug için)
    uint32_t ReadCount;             // Kaç kez okundu
    uint32_t ClearCount;            // Kaç kez temizlendi
    uint32_t GenerateCount;         // Kaç kez veri üretildi
} CustomDataService_Context_t;

// =====================================================================
// FONKSİYON TANIMLARI (Function Prototypes)
// =====================================================================

/*
 * Function prototype nedir?
 * - Fonksiyonun "imzası" - nasıl çağrılacağı
 * - Gerçek kod başka dosyada, burada sadece tanım
 * - Diğer dosyalar bu fonksiyonları görebilir
 *
 * tBleStatus nedir?
 * - BLE işlemlerinin sonucunu gösteren tip
 * - BLE_STATUS_SUCCESS = başarılı
 * - Diğer değerler = çeşitli hata kodları
 */

// Ana servis fonksiyonları
tBleStatus CustomDataService_Init(void);                    // Servisi başlat
tBleStatus CustomDataService_Event_Handler(void *Event);    // Olayları işle

// Veri yönetim fonksiyonları
void CustomDataService_GenerateData(void);                 // Yeni veri üret
void CustomDataService_ClearData(void);                    // Veriyi temizle
tBleStatus CustomDataService_UpdateStatus(DataStatus_t status); // Durumu güncelle

// Debug fonksiyonları
void CustomDataService_PrintData(const char* prefix);      // Veriyi yazdır
void CustomDataService_PrintStatus(void);                  // Durumu yazdır

#endif /* CUSTOM_DATA_SERVICE_H */

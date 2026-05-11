#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <Firebase_ESP_Client.h>

// --- YENİ EKLENEN KÜTÜPHANE ---
#include <Dusk2Dawn.h> 

// Token oluşturma yardımcıları
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// --- AYARLAR ---
#define WIFI_SSID "****"
#define WIFI_PASSWORD "*****"
#define API_KEY "*******"
#define DATABASE_URL "********" 

// --- NESNELER ---
Adafruit_INA219 ina219;
FirebaseData fbdo;       // Anlık veri için
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;

//  LOKASYON NESNESİ (İstanbul Koordinatları) ---
Dusk2Dawn istanbul(41.061177, 28.811160, 3); 

// --- ZAMANLAYICILAR ---
unsigned long sendDataPrevMillis = 0;
// Anlık veri zamanlayıcısı (5 sn)
unsigned long historyPrevMillis = 0;
// Geçmiş veri zamanlayıcısı (2 dk)
const unsigned long historyInterval = 120000;
// 2 Dakika = 120000 ms

// --- ORTALAMA İÇİN DEĞİŞKENLER ---
float sumPower = 0.0;
// 2 dakika boyunca okunan güçleri burada toplayacağız
int readCount = 0;
// Kaç kere okuma yaptığımızı sayacağız

// --- YENİ EKLENEN: UYKU KONTROL FONKSİYONU ---
void checkSleepTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Zaman alınamadı, uyku kontrolü atlanıyor.");
    return;
  }

  int year  = timeinfo.tm_year + 1900;
  int month = timeinfo.tm_mon + 1;
  int day   = timeinfo.tm_mday;

  // Gün doğumu ve batımını gece yarısından itibaren dakika olarak alıyoruz
  int sunrise = istanbul.sunrise(year, month, day, false);
  int sunset  = istanbul.sunset(year, month, day, false);
  int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;

  // Eğer şu anki zaman gün batımından sonra veya gün doğumundan önceyse
  if (currentMinutes >= sunset || currentMinutes < sunrise) {
    Serial.println("🌙 Gece vakti algılandı! Derin uykuya (Deep Sleep) geçiliyor...");

    int minutesUntilSunrise;
    if (currentMinutes >= sunset) {
      // Gece yarısından önceyiz. Yarına kadar olan süre + yarının gün doğuşu
      minutesUntilSunrise = (1440 - currentMinutes) + sunrise;
    } else {
      // Gece yarısını geçtik, gün doğuşuna kalan süre
      minutesUntilSunrise = sunrise - currentMinutes;
    }

    // Güvenlik payı: Sistem güneş doğduktan 15 dakika sonra uyansın ki yeterli voltaj oluşsun
    minutesUntilSunrise += 15;

    Serial.printf("ESP32 %d dakika boyunca uyuyacak...\n", minutesUntilSunrise);

    // Dakikayı mikrosaniyeye çevir (64-bit hesaplama batarya taşmalarını önler)
    uint64_t sleepTimeInMicroseconds = (uint64_t)minutesUntilSunrise * 60ULL * 1000000ULL;

    esp_sleep_enable_timer_wakeup(sleepTimeInMicroseconds);
    esp_deep_sleep_start();
  }
}

void setup() {
  Serial.begin(115200);
// 1. Sensör Başlatma
  if (!ina219.begin()) {
    Serial.println("HATA: INA219 çipi bulunamadı!");
    while (1) { delay(10);
}
  }
  ina219.setCalibration_32V_2A();
  Serial.println("INA219 Sensörü Başarıyla Bağlandı ✅");

  // 2. Wi-Fi Bağlantısı
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Wi-Fi Baglaniyor");
while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nWi-Fi Baglandi!");
  
// NTP ile zaman senkronizasyonu
configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
Serial.print("NTP Zamani Bekleniyor");
while (time(nullptr) < 100000) {
  Serial.print(".");
delay(500);
}
Serial.println("\nZaman Senkronize Edildi!");

  // --- YENİ EKLENEN: ZAMAN SENKRONİZASYONUNDAN SONRA UYKU KONTROLÜ ---
  checkSleepTime(); 

  // 3. Firebase Yapılandırması (Eğer gece değilse buraya geçecek)
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase Bağlantısı Hazır ☁️");
    signupOK = true;
} else {
    Serial.printf("Firebase Hatası: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  // Firebase hazırsa işlem yap
  if (Firebase.ready() && signupOK) {
    
    // --- GÖREV 1: ANLIK VERİ OKUMA VE GÖNDERME (5 Saniyede bir) ---
    if (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0) {
      sendDataPrevMillis = millis();
// Sensörden Verileri Oku
      float busVoltage = ina219.getBusVoltage_V();
      float current_mA = ina219.getCurrent_mA();
float power_mW   = ina219.getPower_mW();

      float current_A = current_mA / 1000.0;
      float power_W   = power_mW / 1000.0;
// Filtreleme
      if (current_A < 0) current_A = 0;
if (power_W < 0) power_W = 0;

      // Seri Port (Kontrol)
      Serial.print("Anlık Güç: "); Serial.println(power_W);
// --- ORTALAMA İÇİN HAVUZA AT ---
      sumPower += power_W;
      readCount++;
// Firebase'e ANLIK Yaz (panel_1 düğümünü günceller)
      FirebaseJson json;
      json.set("voltaj", busVoltage);
      json.set("akim", current_A);
      json.set("guc", power_W);
json.set("ts", millis()); // Sistemin çalışma süresi

      if (Firebase.RTDB.updateNode(&fbdo, "panel_1", &json)) {
        // Başarılı anlık gönderim
      } else {
        Serial.println("Anlık Veri Hatası: " + fbdo.errorReason());
}
    }

    // --- GÖREV 2: GEÇMİŞ VERİ KAYDI (2 Dakikada bir) ---
    if (millis() - historyPrevMillis > historyInterval) {
      historyPrevMillis = millis();
if (readCount > 0) {
        // 1. Ortalamayı Hesapla
        float avgPower = sumPower / readCount;
Serial.println(">>> 2 Dakika Doldu! Ortalama Hesaplanıyor...");
        Serial.print("Okuma Sayısı: "); Serial.println(readCount);
        Serial.print("Ortalama Güç: "); Serial.println(avgPower);
// 2. Enerjiyi Hesapla (Wh cinsinden)
        // Enerji (Wh) = Güç (W) * Saat (h)
        // 5 dakika = 5/60 saat
        float energy_Wh = avgPower * (2.0 / 60.0);
// 3. Firebase'e GEÇMİŞ OLARAK EKLE (push)
        FirebaseJson historyJson;
        historyJson.set("guc", avgPower);
        historyJson.set("enerji", energy_Wh);
historyJson.set("zaman/.sv", "timestamp");
        
        // Zaman damgası olarak Firebase Sunucu Zamanını kullanıyoruz
        // Bu sayede tarih/saat ESP32 kapansa bile doğru olur.
historyJson.set("zaman", "timestamp_placeholder"); 
        
        // .sv (Server Value) timestamp kullanımı için özel ayar
        FirebaseJson jsonTimestamp;
jsonTimestamp.set(".sv", "timestamp");
        historyJson.set("timestamp", jsonTimestamp);

        // 'GecmisVeriler' altına yeni bir ID ile ekle (pushJSON)
        if (Firebase.RTDB.pushJSON(&fbdo, "GecmisVeriler", &historyJson)) {
           Serial.println(">>> Geçmiş Veri Kaydedildi! ✅");
} else {
           Serial.println(">>> Geçmiş Kayıt Hatası: " + fbdo.errorReason());
}

        // 4. Sayaçları Sıfırla (Bir sonraki 2 dk için)
        sumPower = 0;
readCount = 0;

      } else {
        Serial.println("Veri okunamadı, geçmiş kayıt atlandı.");
}
    }
  }
}

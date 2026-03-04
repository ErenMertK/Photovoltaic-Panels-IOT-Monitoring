#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <Firebase_ESP_Client.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// --- AYARLAR ---
#define WIFI_SSID "Kura"
#define WIFI_PASSWORD "05234ek!"
#define API_KEY "AIzaSyCbO8Cj5nX05T3nYH0qI7crZyq5qZe1L3w"
#define DATABASE_URL "https://bitirme-projesi-f5c5d-default-rtdb.europe-west1.firebasedatabase.app/" 

// --- UYKU MODU AYARLARI ---
#define VOLTAGE_THRESHOLD 4.0
#define SLEEP_DURATION 30
#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP (SLEEP_DURATION * 60)

// --- NESNELER ---
Adafruit_INA219 ina219;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool signupOK = false;

// --- ZAMANLAYICILAR ---
unsigned long sendDataPrevMillis = 0;
unsigned long historyPrevMillis = 0;
const unsigned long historyInterval = 120000;

// --- ORTALAMA İÇİN DEĞİŞKENLER ---
float sumPower = 0.0;
int readCount = 0;

// --- RTC MEMORY ---
RTC_DATA_ATTR float rtc_sumPower = 0.0;
RTC_DATA_ATTR int rtc_readCount = 0;
RTC_DATA_ATTR unsigned long rtc_historyTimer = 0;

void goToSleep() {
  Serial.println("\n⚠️ VOLTAJ DÜŞÜK! Uyku Moduna Geçiliyor...");
  
  rtc_sumPower = sumPower;
  rtc_readCount = readCount;
  rtc_historyTimer = millis() - historyPrevMillis;
  
  if (Firebase.ready()) {
    FirebaseJson sleepJson;
    sleepJson.set("durum", "uyku_modu");
    sleepJson.set("voltaj_dusuk", true);
    Firebase.RTDB.updateNode(&fbdo, "panel_1/sistem", &sleepJson);
    delay(100);
  }
  
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  
  Serial.println("💤 Uyku Moduna Giriliyor...");
  Serial.flush();
  
  esp_deep_sleep_start();
}

bool checkVoltage() {
  float busVoltage = ina219.getBusVoltage_V();
  
  Serial.print("Kontrol Voltajı: ");
  Serial.print(busVoltage);
  Serial.println(" V");
  
  if (busVoltage < VOLTAGE_THRESHOLD) {
    Serial.println("⚠️ Voltaj eşik değerin altında!");
    return false;
  }
  
  Serial.println("✅ Voltaj yeterli");
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("\n🔄 Timer'dan Uyandı");
  } else {
    Serial.println("\n🚀 İlk Başlatma");
    rtc_sumPower = 0;
    rtc_readCount = 0;
    rtc_historyTimer = 0;
  }
  
  // Sensör Başlatma
  if (!ina219.begin()) {
    Serial.println("HATA: INA219 bulunamadı!");
    while (1) { delay(10); }
  }
  ina219.setCalibration_32V_2A();
  Serial.println("INA219 Bağlandı ✅");

  // Voltaj Kontrolü
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    if (!checkVoltage()) {
      goToSleep();
      return;
    }
    sumPower = rtc_sumPower;
    readCount = rtc_readCount;
    historyPrevMillis = millis() - rtc_historyTimer;
  }

  // Wi-Fi Bağlantısı
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Wi-Fi Baglaniyor");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nWi-Fi Baglandi!");

  // NTP Senkronizasyonu
  configTime(3 * 3600, 0, "pool.ntp.org");
  Serial.print("NTP Bekleniyor");
  while (time(nullptr) < 100000) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nZaman Senkronize!");

  // Firebase Yapılandırması
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase Hazır ☁️");
    signupOK = true;
  } else {
    Serial.printf("Firebase Hatası: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  // Firebase Hazır Bekle
  Serial.print("Firebase kontrol");
  int fbWaitCount = 0;
  while (!Firebase.ready() && fbWaitCount < 20) {
    Serial.print(".");
    delay(500);
    fbWaitCount++;
  }
  Serial.println();
  
  // Sistem Durumunu Güncelle
  if (Firebase.ready()) {
    FirebaseJson initJson;
    initJson.set("durum", "aktif");
    initJson.set("voltaj_dusuk", false);
    
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
      initJson.set("uyanma_nedeni", "timer");
      Serial.println("✅ Uyandı - sistem aktif");
    } else {
      initJson.set("uyanma_nedeni", "restart");
      Serial.println("✅ Yeni başlatma - sistem aktif");
    }
    
    if (Firebase.RTDB.updateNode(&fbdo, "panel_1/sistem", &initJson)) {
      Serial.println(">>> Sistem durumu gönderildi");
    }
  }
}

void loop() {
  if (Firebase.ready() && signupOK) {
    
    // --- ANLIK VERİ (5 Saniye) ---
    if (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0) {
      sendDataPrevMillis = millis();

      float busVoltage = ina219.getBusVoltage_V();
      float current_mA = ina219.getCurrent_mA();
      float power_mW   = ina219.getPower_mW();

      float current_A = current_mA / 1000.0;
      float power_W   = power_mW / 1000.0;

      if (current_A < 0) current_A = 0;
      if (power_W < 0) power_W = 0;

      // ⚡ VOLTAJ KONTROLÜ
      if (busVoltage < VOLTAGE_THRESHOLD) {
        goToSleep();
        return;
      }

      Serial.print("Güç: "); Serial.print(power_W);
      Serial.print(" W | Voltaj: "); Serial.print(busVoltage); Serial.println(" V");

      sumPower += power_W;
      readCount++;

      // Firebase'e Gönder
      FirebaseJson json;
      json.set("voltaj", busVoltage);
      json.set("akim", current_A);
      json.set("guc", power_W);
      json.set("ts", millis());

      Firebase.RTDB.updateNode(&fbdo, "panel_1", &json);

      // Sistem Durumu Güncelle
      FirebaseJson statusJson;
      statusJson.set("durum", "aktif");
      statusJson.set("voltaj_dusuk", false);
      Firebase.RTDB.updateNode(&fbdo, "panel_1/sistem", &statusJson);
    }

    // --- GEÇMİŞ VERİ (2 Dakika) ---
    if (millis() - historyPrevMillis > historyInterval) {
      historyPrevMillis = millis();

      if (readCount > 0) {
        float avgPower = sumPower / readCount;
        
        Serial.println(">>> 2 Dakika Doldu!");
        Serial.print("Ortalama Güç: "); Serial.println(avgPower);

        float energy_Wh = avgPower * (2.0 / 60.0);

        FirebaseJson historyJson;
        historyJson.set("guc", avgPower);
        historyJson.set("enerji", energy_Wh);
        
        FirebaseJson jsonTimestamp;
        jsonTimestamp.set(".sv", "timestamp");
        historyJson.set("timestamp", jsonTimestamp);

        if (Firebase.RTDB.pushJSON(&fbdo, "GecmisVeriler", &historyJson)) {
           Serial.println(">>> Geçmiş Kaydedildi! ✅");
        }

        sumPower = 0;
        readCount = 0;
      }
    }
  }
}
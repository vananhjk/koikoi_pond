#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <time.h>

const char* serverUrl = "https://koikoi-pond.onrender.com/api/sensors/upload/";

#define ONE_WIRE_BUS 4
#define PH_PIN 34
#define TDS_PIN 35
#define TURBIDITY_PIN 32
#define LCD_SDA 21
#define LCD_SCL 22
#define BOOT_PIN 0

// Múi giờ Việt Nam (UTC+7)
const long GMT_OFFSET_SEC = 7 * 3600;
const int DAYLIGHT_OFFSET_SEC = 0;

// Chống gửi cảnh báo lặp liên tục mỗi vài giây (60 giây)
const unsigned long ALERT_COOLDOWN_MS = 60000;
unsigned long lastAlertSendMs = 0;

// Ghi nhớ mốc giờ định kỳ (6, 12, 21) đã gửi trong ngày
int lastScheduledDay = -1;
int lastScheduledHour = -1;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Preferences prefs;

char deviceId[32] = "KOI_POND_01";
bool shouldSaveConfig = false;

void saveConfigCallback() {
  shouldSaveConfig = true;
}

void loadDeviceId() {
  prefs.begin("koi_iot", true);
  String savedId = prefs.getString("device_id", "KOI_POND_01");
  prefs.end();

  savedId.trim();
  if (savedId.length() == 0) {
    savedId = "KOI_POND_01";
  }
  savedId.toCharArray(deviceId, sizeof(deviceId));
}

void saveDeviceId(const char* newId) {
  String cleanId(newId);
  cleanId.trim();
  if (cleanId.length() == 0) {
    cleanId = "KOI_POND_01";
  }
  prefs.begin("koi_iot", false);
  prefs.putString("device_id", cleanId);
  prefs.end();
  cleanId.toCharArray(deviceId, sizeof(deviceId));
}

void checkBootButtonToResetWifi() {
  if (digitalRead(BOOT_PIN) == LOW) {
    unsigned long pressStart = millis();
    while (digitalRead(BOOT_PIN) == LOW) {
      if (millis() - pressStart >= 1000) {
        Serial.println("Phat hien nhan nut BOOT -> Xoa Wi-Fi cu de doi mang moi...");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Reset WiFi...");
        lcd.setCursor(0, 1);
        lcd.print("Open Setup AP");

        WiFiManager wmTemp;
        wmTemp.resetSettings();
        WiFi.disconnect(true, true);
        delay(1000);
        ESP.restart();
      }
      delay(20);
    }
  }
}

float readAverageVoltage(int pin, int samples = 50) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delayMicroseconds(400);
  }
  float avgRaw = (float)sum / samples;
  return avgRaw * (3.3f / 4095.0f);
}

float voltageToTdsPpm(float tdsVoltage, float tempC) {
  if (tdsVoltage < 0.04f) {
    return 0.0f;
  }

  float safeTemp = (tempC <= 0.0f) ? 25.0f : tempC;
  float tempCoeff = 1.0f + 0.02f * (safeTemp - 25.0f);
  if (tempCoeff < 0.5f) tempCoeff = 0.5f;

  float compVoltage = tdsVoltage / tempCoeff;
  float tdsPpm = (133.42f * pow(compVoltage, 3) -
                   255.86f * pow(compVoltage, 2) +
                   857.39f * compVoltage) * 0.5f;

  if (tdsPpm < 0.0f) tdsPpm = 0.0f;
  if (tdsPpm > 1500.0f) tdsPpm = 0.0f;
  return tdsPpm;
}

float voltageToTurbidityNtu(float turbVoltage) {
  if (turbVoltage < 0.20f) {
    return 0.0f;
  }

  float v5 = turbVoltage * (5.0f / 3.3f);
  float ntu = 0.0f;

  if (v5 >= 4.2f || v5 <= 2.5f) {
    ntu = 0.0f;
  } else {
    ntu = -1120.4f * sq(v5) + 5742.3f * v5 - 4352.9f;
  }

  if (ntu < 0.0f) ntu = 0.0f;
  if (ntu > 3000.0f) ntu = 3000.0f;
  return ntu;
}

// Kiểm tra vượt ngưỡng trên cả 4 thông số
bool isAnyOutOfRange(float tempC, float ph, float tds, float turb) {
  bool tempBad = (tempC < 20.0f || tempC > 28.0f);
  bool phBad   = (ph < 6.8f || ph > 8.0f);
  bool tdsBad  = (tds < 100.0f || tds > 400.0f);
  bool turbBad = (turb >= 25.0f);
  return (tempBad || phBad || tdsBad || turbBad);
}

bool sendSensorPayload(float tempC, float ph, float tds, float turb) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[%s] Mat ket noi Wi-Fi, dang thu ket noi lai...\n", deviceId);
    WiFi.reconnect();
    return false;
  }

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> doc;
  doc["device_id"] = deviceId;
  doc["temperature"] = tempC;
  doc["ph"] = ph;
  doc["tds"] = tds;
  doc["turbidity"] = turb;

  String requestBody;
  serializeJson(doc, requestBody);

  int httpResponseCode = http.POST(requestBody);
  bool ok = (httpResponseCode > 0 && httpResponseCode < 400);
  if (ok) {
    Serial.printf("[%s] Gui du lieu thanh cong! (HTTP %d)\n", deviceId, httpResponseCode);
  } else {
    Serial.printf("[%s] Loi gui du lieu: %d\n", deviceId, httpResponseCode);
  }
  http.end();
  return ok;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(BOOT_PIN, INPUT_PULLUP);
  loadDeviceId();
  Serial.printf("Device ID hien tai: %s\n", deviceId);

  Wire.begin(LCD_SDA, LCD_SCL);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Koi Pond IoT");
  lcd.setCursor(0, 1);
  lcd.print(deviceId);

  WiFiManager wm;
  wm.setSaveConfigCallback(saveConfigCallback);

  WiFiManagerParameter custom_device_id(
      "device_id",
      "Device ID (vd: KOI_POND_02)",
      deviceId,
      sizeof(deviceId) - 1
  );
  wm.addParameter(&custom_device_id);

  if (!wm.autoConnect("KoiPond_Setup")) {
    Serial.println("Ket noi that bai, khoi dong lai thiet bi...");
    delay(3000);
    ESP.restart();
  }

  if (shouldSaveConfig) {
    saveDeviceId(custom_device_id.getValue());
    Serial.printf("Da luu Device ID moi: %s\n", deviceId);
  }

  // Đồng bộ thời gian thực qua NTP (UTC+7)
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");

  Serial.println("Da ket noi Wi-Fi thanh cong!");
  Serial.printf("Gui du lieu cho thiet bi: %s\n", deviceId);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected!");
  lcd.setCursor(0, 1);
  lcd.print(deviceId);
  delay(1500);

  sensors.begin();
  pinMode(PH_PIN, INPUT);
  pinMode(TDS_PIN, INPUT);
  pinMode(TURBIDITY_PIN, INPUT);
}

void loop() {
  checkBootButtonToResetWifi();

  sensors.requestTemperatures();
  float waterTemp = sensors.getTempCByIndex(0);
  if (waterTemp == DEVICE_DISCONNECTED_C) {
    waterTemp = 0.0f;
  }

  float phVoltage = readAverageVoltage(PH_PIN);
  float tdsVoltage = readAverageVoltage(TDS_PIN);
  float turbidityVoltage = readAverageVoltage(TURBIDITY_PIN);

  float phValue = 7.0f + ((phVoltage - 2.5f) * 3.5f);
  if (phValue < 0.0f) phValue = 0.0f;
  if (phValue > 14.0f) phValue = 14.0f;

  float tdsPpm = voltageToTdsPpm(tdsVoltage, waterTemp);
  float turbidityNtu = voltageToTurbidityNtu(turbidityVoltage);

  Serial.printf("[%s] Temp: %.2f C | pH: %.2f | TDS: %.1f ppm | Turb: %.1f NTU\n",
                deviceId, waterTemp, phValue, tdsPpm, turbidityNtu);

  // Hiển thị liên tục trên LCD 16x2
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(waterTemp, 1);
  lcd.print("C pH:");
  lcd.print(phValue, 1);

  lcd.setCursor(0, 1);
  lcd.print("S:");
  lcd.print((int)round(tdsPpm));
  lcd.print("ppm U:");
  lcd.print((int)round(turbidityNtu));
  lcd.print("NTU");

  bool outOfRange = isAnyOutOfRange(waterTemp, phValue, tdsPpm, turbidityNtu);

  if (outOfRange) {
    // Vượt ngưỡng bất kỳ 1 trong 4 thông số -> gửi ngay (có chống spam mỗi 60s)
    if (millis() - lastAlertSendMs >= ALERT_COOLDOWN_MS) {
      if (sendSensorPayload(waterTemp, phValue, tdsPpm, turbidityNtu)) {
        lastAlertSendMs = millis();
      }
    }
  } else {
    // Nước ổn định -> chỉ gửi vào 3 mốc: 6h sáng, 12h trưa, 21h tối
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 200)) {
      int hour = timeinfo.tm_hour;
      int yday = timeinfo.tm_yday;
      bool isTargetHour = (hour == 6 || hour == 12 || hour == 21);

      if (isTargetHour && !(lastScheduledDay == yday && lastScheduledHour == hour)) {
        if (sendSensorPayload(waterTemp, phValue, tdsPpm, turbidityNtu)) {
          lastScheduledDay = yday;
          lastScheduledHour = hour;
        }
      }
    }
  }

  delay(2000);
}
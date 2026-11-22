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

// Địa chỉ URL server trên Render
const char* serverUrl = "https://koikoi-pond.onrender.com/api/sensors/upload/";

// Cấu hình chân GPIO theo yêu cầu
#define ONE_WIRE_BUS 4    // DS18B20 data
#define PH_PIN 34         // Cảm biến pH (ADC1)
#define TDS_PIN 35        // Cảm biến TDS (ADC1)
#define TURBIDITY_PIN 32  // Cảm biến độ đục SEN0189 (ADC1)
#define LCD_SDA 21        // I2C SDA
#define LCD_SCL 22        // I2C SCL

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Preferences prefs;

// Biến lưu Device ID (mặc định nếu chưa cấu hình)
char deviceId[32] = "KOI_POND_01";
bool shouldSaveConfig = false;

unsigned long lastTime = 0;
unsigned long timerDelay = 10000;

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

// Đọc trung bình nhiều mẫu ADC để giảm nhiễu
float readAverageVoltage(int pin, int samples = 15) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delayMicroseconds(300);
  }
  float avgRaw = (float)sum / samples;
  return avgRaw * (3.3 / 4095.0);
}

// Quy đổi điện áp TDS (V) + nhiệt độ (°C) -> TDS (ppm) có bù nhiệt
float voltageToTdsPpm(float tdsVoltage, float tempC) {
  float tempCoeff = 1.0 + 0.02 * (tempC - 25.0);
  if (tempCoeff < 0.5) tempCoeff = 0.5;
  float compVoltage = tdsVoltage / tempCoeff;

  float tdsPpm = (133.42 * pow(compVoltage, 3) -
                  255.86 * pow(compVoltage, 2) +
                  857.39 * compVoltage) * 0.5;

  if (tdsPpm < 0.0) tdsPpm = 0.0;
  return tdsPpm;
}

// Quy đổi điện áp Turbidity (V) -> Độ đục (NTU)
// Nếu SEN0189 qua cầu phân áp về 3.3V thì nhân 5.0 / 3.3 để khôi phục dải 0-5V
float voltageToTurbidityNtu(float turbVoltage) {
  float v5 = turbVoltage * (5.0 / 3.3);
  float ntu = 0.0;

  if (v5 >= 4.2) {
    ntu = 0.0;
  } else if (v5 <= 2.5) {
    ntu = 3000.0;
  } else {
    ntu = -1120.4 * sq(v5) + 5742.3 * v5 - 4352.9;
  }

  if (ntu < 0.0) ntu = 0.0;
  return ntu;
}

void setup() {
  Serial.begin(115200);
  delay(200);

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
  // 1. Nhiệt độ nước (°C)
  sensors.requestTemperatures();
  float waterTemp = sensors.getTempCByIndex(0);
  if (waterTemp == DEVICE_DISCONNECTED_C) {
    waterTemp = 0.0;
  }

  // 2. Đọc điện áp & quy đổi sang giá trị thực trong nước
  float phVoltage = readAverageVoltage(PH_PIN);
  float tdsVoltage = readAverageVoltage(TDS_PIN);
  float turbidityVoltage = readAverageVoltage(TURBIDITY_PIN);

  float phValue = 7.0 + ((phVoltage - 2.5) * 3.5);
  if (phValue < 0.0) phValue = 0.0;
  if (phValue > 14.0) phValue = 14.0;

  float tdsPpm = voltageToTdsPpm(tdsVoltage, waterTemp);
  float turbidityNtu = voltageToTurbidityNtu(turbidityVoltage);

  // 3. In đủ 4 giá trị thực ra Serial Monitor
  Serial.printf("[%s] Temp: %.2f C | pH: %.2f | TDS: %.1f ppm | Turb: %.1f NTU\n",
                deviceId, waterTemp, phValue, tdsPpm, turbidityNtu);

  // 4. Hiển thị gọn đủ 4 giá trị thực lên LCD 16x2
  // Dòng 0: T:25.0C pH:7.1
  // Dòng 1: S:210ppm U:12NTU
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

  // 5. Gửi 4 giá trị thực lên server Render qua HTTP POST
  if ((millis() - lastTime) > timerDelay) {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverUrl);
      http.addHeader("Content-Type", "application/json");

      StaticJsonDocument<256> doc;
      doc["device_id"] = deviceId;
      doc["temperature"] = waterTemp;
      doc["ph"] = phValue;
      doc["tds"] = tdsPpm;
      doc["turbidity"] = turbidityNtu;

      String requestBody;
      serializeJson(doc, requestBody);

      int httpResponseCode = http.POST(requestBody);
      if (httpResponseCode > 0) {
        Serial.printf("[%s] Gui du lieu thanh cong! (HTTP %d)\n", deviceId, httpResponseCode);
      } else {
        Serial.printf("[%s] Loi gui du lieu: %d\n", deviceId, httpResponseCode);
      }
      http.end();
    } else {
      Serial.printf("[%s] Mat ket noi Wi-Fi, dang thu ket noi lai...\n", deviceId);
      WiFi.reconnect();
    }
    lastTime = millis();
  }

  delay(2000);
}
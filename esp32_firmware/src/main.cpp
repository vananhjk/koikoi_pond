#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
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

unsigned long lastTime = 0;
unsigned long timerDelay = 10000;

void setup() {
  Serial.begin(115200);

  // Khởi tạo I2C theo đúng SDA = GPIO21, SCL = GPIO22
  Wire.begin(LCD_SDA, LCD_SCL);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Koi Pond IoT");
  lcd.setCursor(0, 1);
  lcd.print("WiFi Config Mode");

  // Khởi tạo WiFiManager để tự động cấu hình mạng
  WiFiManager wm;
  // Nếu kết nối thất bại, phát ra mạng Wi-Fi "KoiPond_Setup"
  if (!wm.autoConnect("KoiPond_Setup")) {
    Serial.println("Ket noi that bai, khoi dong lai thiet bi...");
    delay(3000);
    ESP.restart();
  }

  Serial.println("Da ket noi Wi-Fi thanh cong!");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected!");
  delay(1500);

  sensors.begin();
  pinMode(PH_PIN, INPUT);
  pinMode(TDS_PIN, INPUT);
  pinMode(TURBIDITY_PIN, INPUT);
}

void loop() {
  sensors.requestTemperatures();
  float waterTemp = sensors.getTempCByIndex(0);
  if (waterTemp == DEVICE_DISCONNECTED_C) {
    waterTemp = 0.0;
  }

  int phRaw = analogRead(PH_PIN);
  int tdsRaw = analogRead(TDS_PIN);
  int turbidityRaw = analogRead(TURBIDITY_PIN);

  float phVoltage = phRaw * (3.3 / 4095.0);
  float tdsVoltage = tdsRaw * (3.3 / 4095.0);
  float turbidityVoltage = turbidityRaw * (3.3 / 4095.0);

  float phValue = 7.0 + ((phVoltage - 2.5) * 3.5);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(waterTemp, 1);
  lcd.print("C pH:");
  lcd.print(phValue, 1);

  lcd.setCursor(0, 1);
  lcd.print("Turb:");
  lcd.print(turbidityVoltage, 1);
  lcd.print("V");

  if ((millis() - lastTime) > timerDelay) {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverUrl);
      http.addHeader("Content-Type", "application/json");

      StaticJsonDocument<200> doc;
      doc["device_id"] = "KOI_POND_01";
      doc["temperature"] = waterTemp;
      doc["turbidity"] = turbidityVoltage;
      doc["tds"] = tdsVoltage;
      doc["ph"] = phValue;

      String requestBody;
      serializeJson(doc, requestBody);

      int httpResponseCode = http.POST(requestBody);
      if (httpResponseCode > 0) {
        Serial.println("Gui du lieu len Render thanh cong!");
      } else {
        Serial.printf("Loi gui du lieu: %d\n", httpResponseCode);
      }
      http.end();
    }
    lastTime = millis();
  }

  delay(2000);
}
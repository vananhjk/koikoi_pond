#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Địa chỉ URL server trên Render của bạn
const char* serverUrl = "https://koikoi-pond.onrender.com/api/sensors/upload/";

#define ONE_WIRE_BUS 4    
#define TURBIDITY_PIN 34  
#define TDS_PIN 35        
#define PH_PIN 32         

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
LiquidCrystal_I2C lcd(0x27, 16, 2);

unsigned long lastTime = 0;
unsigned long timerDelay = 10000; 

void setup() {
  Serial.begin(115200);

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Koi Pond IoT");
  lcd.setCursor(0, 1);
  lcd.print("WiFi Config Mode");

  // Khởi tạo WiFiManager để tự động cấu hình mạng
  WiFiManager wm;
  // Nếu kết nối thất bại, nó sẽ phát ra mạng Wi-Fi tên "KoiPond_Setup" (mật khẩu trống)
  if (!wm.autoConnect("KoiPond_Setup")) {
    Serial.println("Kết nối thất bại, khởi động lại thiết bị...");
    delay(3000);
    ESP.restart();
  }

  Serial.println("Đã kết nối Wi-Fi thành công!");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected!");
  delay(1500);

  sensors.begin();
  pinMode(TURBIDITY_PIN, INPUT);
  pinMode(TDS_PIN, INPUT);
  pinMode(PH_PIN, INPUT);
}

void loop() {
  sensors.requestTemperatures();
  float waterTemp = sensors.getTempCByIndex(0);
  if (waterTemp == DEVICE_DISCONNECTED_C) waterTemp = 0.0; 

  int turbidityRaw = analogRead(TURBIDITY_PIN);
  int tdsRaw = analogRead(TDS_PIN);
  int phRaw = analogRead(PH_PIN);

  float turbidityVoltage = turbidityRaw * (3.3 / 4095.0);
  float tdsVoltage = tdsRaw * (3.3 / 4095.0);
  float phValue = 7.0 + ((phRaw * (3.3 / 4095.0) - 2.5) * 3.5); 

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:"); lcd.print(waterTemp, 1); lcd.print("C pH:"); lcd.print(phValue, 1);
  lcd.setCursor(0, 1);
  lcd.print("Turb:"); lcd.print(turbidityVoltage, 1); lcd.print("V");

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
        Serial.println("Gửi dữ liệu lên Render thành công!");
      } else {
        Serial.printf("Lỗi gửi dữ liệu: %d\n", httpResponseCode);
      }
      http.end();
    }
    lastTime = millis();
  }
  delay(2000);
}
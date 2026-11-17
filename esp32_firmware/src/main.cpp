#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// --- CẤU HÌNH WIFI VÀ SERVER ---
const char* ssid = "Cong Tuan";
const char* password = "tuan110393";
// Sử dụng đúng IP máy tính của bạn (192.168.1.50) và đường dẫn upload API
const char* serverUrl = "http://192.168.1.50:8000/api/sensors/upload/";
// --- KHAI BÁO CHÂN KẾT NỐI ---
#define ONE_WIRE_BUS 4    // DS18B20 Data pin
#define TURBIDITY_PIN 34  // Cảm biến độ đục (Analog ADC1)
#define TDS_PIN 35        // Cảm biến TDS (Analog ADC1)
#define PH_PIN 32         // Cảm biến pH (Analog ADC1 - Bổ sung cảm biến thứ 4)

// --- KHỞI TẠO ĐỐI TƯỢNG ---
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
LiquidCrystal_I2C lcd(0x27, 16, 2); // Địa chỉ I2C LCD (thường là 0x27 hoặc 0x3F)

unsigned long lastTime = 0;
unsigned long timerDelay = 10000; // Gửi dữ liệu lên server mỗi 10 giây

void setup() {
  Serial.begin(115200);

  // Khởi tạo LCD
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Koi Pond IoT");
  lcd.setCursor(0, 1);
  lcd.print("Connecting WiFi");

  // Kết nối WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected!");
  delay(1500);

  // Khởi động cảm biến
  sensors.begin();
  pinMode(TURBIDITY_PIN, INPUT);
  pinMode(TDS_PIN, INPUT);
  pinMode(PH_PIN, INPUT);
}

void loop() {
  // 1. Đọc dữ liệu từ cảm biến nhiệt độ DS18B20
  sensors.requestTemperatures();
  float waterTemp = sensors.getTempCByIndex(0);
  if (waterTemp == DEVICE_DISCONNECTED_C) {
    waterTemp = 0.0; 
  }

  // 2. Đọc giá trị ADC từ các cảm biến khác
  int turbidityRaw = analogRead(TURBIDITY_PIN);
  int tdsRaw = analogRead(TDS_PIN);
  int phRaw = analogRead(PH_PIN);

  // Quy đổi điện áp hoặc tính toán thông số cơ bản (có thể hiệu chuẩn sau)
  float turbidityVoltage = turbidityRaw * (3.3 / 4095.0);
  float tdsVoltage = tdsRaw * (3.3 / 4095.0);
  
  // Công thức giả lập chuyển đổi điện áp sang giá trị pH (sẽ hiệu chuẩn thực tế theo mạch đo pH của bạn)
  float phValue = 7.0 + ((phRaw * (3.3 / 4095.0) - 2.5) * 3.5); 

  // 3. Hiển thị thông tin lên LCD 16x2 luân phiên hoặc rút gọn
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:"); lcd.print(waterTemp, 1); lcd.print("C pH:"); lcd.print(phValue, 1);
  
  lcd.setCursor(0, 1);
  lcd.print("Turb:"); lcd.print(turbidityVoltage, 1); lcd.print("V");

  // 4. Gửi dữ liệu lên Django Backend theo chu kỳ định sẵn (10 giây)
  if ((millis() - lastTime) > timerDelay) {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverUrl);
      http.addHeader("Content-Type", "application/json");

      // Đóng gói đủ 4 cảm biến chính vào JSON gửi lên Django
      StaticJsonDocument<200> doc;
      doc["temperature"] = waterTemp;
      doc["turbidity"] = turbidityVoltage; // Hoặc giá trị NTU sau khi quy đổi
      doc["tds"] = tdsVoltage;             // Hoặc giá trị ppm sau khi quy đổi
      doc["ph"] = phValue;                 // Bổ sung đầy đủ cảm biến pH

      String requestBody;
      serializeJson(doc, requestBody);

      int httpResponseCode = http.POST(requestBody);
      if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        Serial.println("Server response: " + response);
      } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      http.end();
    } else {
      Serial.println("WiFi Disconnected");
    }
    lastTime = millis();
  }

  delay(2000);
}
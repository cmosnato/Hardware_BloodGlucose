#include <TFT_eSPI.h>  // ใช้ไลบรารี TFT_eSPI
#include <SPI.h>
TFT_eSPI tft = TFT_eSPI();  // ประกาศตัวแปรของจอ

#include <math.h>
#include <Arduino.h>

// sensorVoltage
#include <Wire.h>
#include <Adafruit_INA219.h>
Adafruit_INA219 ina219;

#include <WiFiManager.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <HTTPClient.h>

#include <Bounce2.h>
Bounce debouncer = Bounce();         // สร้างอ็อบเจ็ค Bounce
bool isButtonPressed = false;        // ตัวแปรเก็บสถานะการกดปุ่ม
unsigned long buttonPressStartTime;  // เก็บเวลาที่เริ่มกดปุ่ม

const int Idle = 0;
const int Detect = 1;
const int Cal = 2;
const int Result = 3;
int state;
int analog;

static unsigned long previousMillis = 0;
static unsigned long previousMillis1 = 0;
static unsigned long previousMillis_result = 0;
static unsigned long previousMillis_Bat = 0;

unsigned long sum = 0;
int count = 0;
int status = 1;
int flag1, flag2;

int bat_percentage = 100;
float bat_volte;


// Provide the token generation process info.
#include <addons/TokenHelper.h>

// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

// // Insert Authorized Email and Corresponding Password
// #define USER_EMAIL "bloodglucose2024@gmail.com"
// #define USER_PASSWORD "adminbloodglucose

/* 2. Define the API Key */
#define API_KEY "AIzaSyBBNbmbzWOdiipMo3Yj1EyakYMdUz1s9VM"

/* 3. Define the RTDB URL */
#define DATABASE_URL "https://blood-glucose2-default-rtdb.firebaseio.com/"  //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app

/* 4. Define the user Email and password that alreadey registerd or added in your project */
#define USER_EMAIL "cmoscmark@gmail.com"
#define USER_PASSWORD "cmos1234"

// Define Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;


// Variables to save database paths
String databasePath;

int analogValue;
float voltValue, currentValue;
String analogReadPath;
String voltValuePath;
String currentValuePath;
// String macStr = "";

// Global variables to store the new username and password
String newUsername = "";
String newPassword = "";
const int resetButtonPin = 13;  // กำหนดขาของปุ่มสำหรับรีเซ็ตค่า Wi-Fi
const char* apName = "Blood glucose";

WiFiManager wifiManager;

void saveConfigCallback() {
  ESP.restart();
}

void resetWiFi() {
  Serial.println("Resetting Wi-Fi settings...");
  wifiManager.resetSettings();
  ESP.restart();
}


void setup() {
  Serial.begin(115200);
  Serial.println("Starting setup...");
  state = 0;
  flag1 = 0;
  Wire.begin(21, 5);
  tft.init();
  ina219.begin();
  pinMode(resetButtonPin, INPUT_PULLUP);
  debouncer.attach(resetButtonPin);
  debouncer.interval(5);  // เซ็ต debounce interval

  // กำหนด HTML ที่ต้องการแสดงในหน้า Config Portal
  String html = "<h1>ESP32 WiFi Setup</h1>";
  html += "<p>Connect to this WiFi: <strong>" + String(apName) + "</strong></p>";
  html += "<p>Then go to 192.168.4.1</p>";
  html += "<h2>Change Username and Password</h2>";
  html += "<form action=\"/change\" method=\"post\">";
  html += "<label for=\"newUsername\">New Username:</label>";
  html += "<input type=\"text\" id=\"newUsername\" name=\"newUsername\" required><br><br>";
  html += "<label for=\"newPassword\">New Password:</label>";
  html += "<input type=\"password\" id=\"newPassword\" name=\"newPassword\" required><br><br>";
  html += "<input type=\"submit\" value=\"Save\">";
  html += "</form>";

  // ✅ ใส่ HTML ที่กำหนดเองไว้ที่นี่ก่อนเริ่ม WiFiManager
  wifiManager.setCustomHeadElement(html.c_str());

  wifiManager.setAPCallback([](WiFiManager* wm) {
    tft.fillScreen(TFT_WHITE);
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(3);
    tft.drawString("Wifi Name", 40, 20);
    tft.drawString("Blood glucose", 5, 50);
    tft.drawString("AP Mode", 50, 110);
    tft.drawString("192.168.4.1", 20, 140);
  });

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // Start WiFiManager to connect to WiFi or set up a new Access Point
  if (!wifiManager.autoConnect(apName)) {
    Serial.println("Failed to connect and hit timeout");
  }


  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the user sign in credentials */
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback;  // see addons/TokenHelper.h
  // Comment or pass false value when WiFi reconnection will control by your code or third party library e.g. WiFiManager
  Firebase.reconnectNetwork(false);

  // Since v4.4.x, BearSSL engine was used, the SSL buffer need to be set.
  // Large data transmission may require larger RX buffer, otherwise connection issue or data read time out can be occurred.
  fbdo.setBSSLBufferSize(2048, 512);  // จาก 4096, 1024
  fbdo.setResponseSize(1024);         // จาก 2048

  Firebase.begin(&config, &auth);

  Firebase.setDoubleDigits(5);

  config.timeout.serverResponse = 10 * 1000;

  // Update database path
  databasePath = "/Blood/C3AFA9F0";
  // Update database path for sensor readings
  analogReadPath = databasePath + "/analog_value";
  voltValuePath = databasePath + "/voltage";
  currentValuePath = databasePath + "/currents";
}

void loop() {
  if (state == Idle) {
    SetButton();
    analogSetAttenuation(ADC_11db);
    analog = analogRead(33);
    unsigned long currentMillis_Bat = millis();
    unsigned long currentMillis = millis();
    updateBatteryStatus(currentMillis_Bat);
    if (currentMillis - previousMillis >= 5000) {
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(2);     // ลดขนาดตัวอักษร
      tft.setCursor(160, 0);  // เดิม 160 → ปรับไปขวาให้ตรงกลางมากขึ้น

      if (bat_percentage < 10) {
        tft.print(F("  "));
        tft.print(bat_percentage);
        tft.println(F("%"));
      } else if (bat_percentage < 100) {
        tft.print(F(" "));
        tft.print(bat_percentage);
        tft.println(F("%"));
      } else {
        tft.print(bat_percentage);
        tft.println(F("%"));
      }

      // ข้อความอื่น ๆ ปรับ Y ขึ้นเล็กน้อยให้สมดุลกับขนาดตัวอักษร
      tft.drawString("Mac Address", 0, 50);
      tft.drawString("C3AFA9F0", 0, 80);
      tft.drawString("Glucose Meter", 0, 120);
      tft.drawString("Ready", 90, 150);  // เดิม 70, ปรับชิดขวาเล็กน้อย
      if (checkInternetConnection()) {
        Serial.println("Connected to the Internet");
        tft.drawString("Online", 0, 0);
      } else {
        Serial.println("Not connected to the Internet");
        tft.drawString("Offline", 0, 0);
      }
      previousMillis = currentMillis;
    }
    if (isButtonPressed && (millis() - buttonPressStartTime >= 5000)) {
      ButtonReset();
    }
    if (analog <= 3000 && analog > 10) {
      state = Detect;
    }

  } else if (state == Detect) {
    analogSetAttenuation(ADC_11db);
    analog = analogRead(33);
    Serial.println(analog);
    unsigned long currentMillis = millis();
    unsigned long currentMillis1 = millis();
    if (currentMillis1 - previousMillis1 >= 5000) {
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(2);     // ลดขนาดตัวอักษร
      tft.setCursor(160, 0);  // เดิม 160 → ปรับไปขวาให้ตรงกลางมากขึ้น

      if (bat_percentage < 10) {
        tft.print(F("  "));
        tft.print(bat_percentage);
        tft.println(F("%"));
      } else if (bat_percentage < 100) {
        tft.print(F(" "));
        tft.print(bat_percentage);
        tft.println(F("%"));
      } else {
        tft.print(bat_percentage);
        tft.println(F("%"));
      }

      // ปรับตำแหน่ง Y ของ drawString ให้ดูพอ ๆ กับตอน TextSize=3
      tft.drawString("Mac Address", 0, 50);    // เดิม 50
      tft.drawString("C3AFA9F0", 0, 80);       // เดิม 80
      tft.drawString("Glucose Meter", 0, 120);  // เดิม 120
      tft.drawString("Reading DATA", 0, 150);  // เดิม 150
      if (checkInternetConnection()) {
        Serial.println("Connected to the Internet");
        tft.drawString("Online", 0, 0);
      } else {
        Serial.println("Not connected to the Internet");
        tft.drawString("Offline", 0, 0);
      }
      previousMillis1 = currentMillis1;
    }
    if (flag1 >= 1) {
      Serial.println("analog");
      Serial.println(analog);
      sum += analog;
      count++;
    }
    if (currentMillis - previousMillis >= 2500) {
      flag1++;
      if (flag1 == 4) {
        state = Cal;
      }
      previousMillis = currentMillis;
    }
  } else if (state == Cal) {
    int average = sum / count;
    analogValue = average;
    voltValue = Voltagein_map(analogValue);
    currentValue = Current_map(analogValue);

    Firebase.RTDB.setInt(&fbdo, analogReadPath.c_str(), analogValue);
    Firebase.RTDB.setFloat(&fbdo, voltValuePath.c_str(), voltValue);
    Firebase.RTDB.setFloat(&fbdo, currentValuePath.c_str(), currentValue);

    average = 0;
    sum = 0;
    count = 0;
    flag1 = 0;
    delay(500);
    state = Result;
  } else if (state == Result) {
    unsigned long currentMillis2 = millis();
    analogSetAttenuation(ADC_11db);
    analog = analogRead(33);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);  // ลดขนาดตัวอักษร

    // ปรับตำแหน่ง X ของเปอร์เซ็นต์แบตให้อยู่ด้านขวาเหมือนเดิม
    tft.setCursor(160, 0);  // เดิมใช้ 160 ตอน size 3
    if (bat_percentage < 10) {
      tft.print(F("  "));
      tft.print(bat_percentage);
      tft.println(F("%"));
    } else if (bat_percentage < 100) {
      tft.print(F(" "));
      tft.print(bat_percentage);
      tft.println(F("%"));
    } else {
      tft.print(bat_percentage);
      tft.println(F("%"));
    }

    // ปรับ Y ของข้อความอื่นให้ต่ำลงเล็กน้อยเพื่อสมดุลกับขนาดตัวอักษรใหม่
    tft.drawString("Mac Address", 0, 50);
    tft.drawString("C3AFA9F0", 0, 80);
    tft.drawString("Glucose", 0, 120);
    tft.drawString(String(analogValue) + " ", 0, 140);
    tft.drawString(String(voltValue) + " V", 0, 160);
    tft.drawString(String(currentValue) + " mA", 0, 180);
    if (checkInternetConnection()) {
      Serial.println("Connected to the Internet");
      tft.drawString("Online", 0, 0);
    } else {
      Serial.println("Not connected to the Internet");
      tft.drawString("Offline", 0, 0);
    }
    if ((currentMillis2 - previousMillis_result >= 5000) && analog == 4095) {
      state = Idle;
      previousMillis_result = currentMillis2;
      delay(50);
    }
  }
}

bool checkInternetConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  HTTPClient http;
  http.begin("http://www.google.com");
  int httpCode = http.GET();
  bool result = (httpCode == 200);
  http.end();

  return result;
}

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

float Voltagein_map(int adcValue) {
  float Vref = 3.3;
  float V_in = ((float)adcValue / 4095.0) * Vref;
  V_in = (int)(V_in * 1000) / 1000.0;  // ตัดเศษ 3 ตำแหน่ง
  return V_in;
}

float Current_map(int adcValue) {
  float V_in = Voltagein_map(adcValue);  // ได้แรงดันจาก ADC
  float R_load = 100000.0;               // R = 100KΩ
  float I = V_in / R_load;               // กระแส I = V / R
  I = (int)(I * 1000 * 1000) / 1000.0;   // แปลงเป็น mA และตัดเศษ 3 ตำแหน่ง
  return I;
}


void SetButton() {
  debouncer.update();
  if (debouncer.fell()) {
    isButtonPressed = true;
    buttonPressStartTime = millis();
  }
  if (debouncer.rose()) {
    isButtonPressed = false;
  }
}

void ButtonReset() {
  Serial.println("Button pressed for 5 seconds. Resetting Wi-Fi settings");
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(3);
  tft.drawString("Wifi Name", 40, 20);
  tft.drawString("Blood glucose", 5, 50);
  tft.drawString("AP Mode", 50, 110);
  tft.drawString("192.168.4.1", 20, 140);
  resetWiFi();
}

void updateBatteryStatus(unsigned long currentMillis_Bat) {
  if (currentMillis_Bat - previousMillis_Bat >= 10000) {
    bat_volte = ina219.getBusVoltage_V();
    int bat_map = mapfloat(bat_volte, 2.8, 4.5, 0, 100);
    bat_percentage = constrain(bat_map, 1, 100);
    previousMillis_Bat = currentMillis_Bat;
  }
}

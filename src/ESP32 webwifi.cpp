#include <WiFi.h>
#include <SPIFFS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>

// OLED setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// กำหนด SSID และรหัสผ่านสำหรับ Access Point
const char* apSSID = "ESP32-Config";
const char* apPassword = "12345678";

// MQTT setup
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* mqtt_user = "";
const char* mqtt_password = "";
const char* mqtt_topic = "esp32/mqtt";

// สร้างเว็บเซิร์ฟเวอร์
AsyncWebServer server(80);

// HTML ฟอร์มสำหรับรับข้อมูล SSID และรหัสผ่าน
const char* htmlForm = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 WiFi Configuration</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: Arial, sans-serif;
      display: flex;
      justify-content: center;
      align-items: center;
      height: 100vh;
      margin: 0;
      background-color: #f0f0f0;
    }
    .container {
      background-color: white;
      padding: 20px;
      border-radius: 10px;
      box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
      max-width: 400px;
      width: 100%;
      text-align: center;
    }
    h2 {
      margin-bottom: 20px;
      color: #333;
    }
    input[type="text"], input[type="password"] {
      width: calc(100% - 22px);
      padding: 10px;
      margin: 10px 0;
      border: 1px solid #ccc;
      border-radius: 5px;
    }
    input[type="submit"] {
      background-color: #4CAF50;
      color: white;
      border: none;
      border-radius: 5px;
      padding: 10px 20px;
      cursor: pointer;
      margin-top: 10px;
    }
    input[type="submit"]:hover {
      background-color: #45a049;
    }
    .form-group {
      margin-bottom: 15px;
    }
  </style>
</head>
<body>
  <div class="container">
    <h2>WiFi Configuration</h2>
    <form action="/get" method="get">
      <div class="form-group">
        <label for="ssid">SSID:</label>
        <input type="text" id="ssid" name="ssid" required>
      </div>
      <div class="form-group">
        <label for="password">Password:</label>
        <input type="password" id="password" name="password" required>
      </div>
      <input type="submit" value="Save">
    </form>
    <form action="/reset" method="get">
      <input type="submit" value="Reset WiFi Configuration" style="background-color: #f44336;">
    </form>
  </div>
</body>
</html>
)rawliteral";


// MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

// Callback function for MQTT
void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Display message on OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Message:");
  display.setCursor(0, 10);
  display.print(messageTemp);
  display.display();
}

// ฟังก์ชันบันทึกข้อมูล Wi-Fi ไปยัง SPIFFS
void saveWiFiConfig(String ssid, String password) {
  File file = SPIFFS.open("/wifi_config.txt", FILE_WRITE);
  if (file) {
    file.println(ssid);
    file.println(password);
    file.close();
    Serial.println("WiFi configuration saved.");
  } else {
    Serial.println("Failed to save WiFi configuration.");
  }
}

// ฟังก์ชันโหลดข้อมูล Wi-Fi จาก SPIFFS
void loadWiFiConfig(String &ssid, String &password) {
  File file = SPIFFS.open("/wifi_config.txt", FILE_READ);
  if (file) {
    ssid = file.readStringUntil('\n');
    password = file.readStringUntil('\n');
    ssid.trim();
    password.trim();
    file.close();
    Serial.println("WiFi configuration loaded.");
  } else {
    Serial.println("Failed to load WiFi configuration.");
  }
}

// ฟังก์ชันลบข้อมูล Wi-Fi จาก SPIFFS
void resetWiFiConfig() {
  SPIFFS.remove("/wifi_config.txt");
  Serial.println("WiFi configuration reset.");
  ESP.restart(); // รีสตาร์ท ESP32
}

// ฟังก์ชันแสดงข้อความบนจอ OLED
void displayMessage(const char* message) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(message);
  display.display();
}

void connectToMqttBroker() {
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    // display.clearDisplay();
    // display.setCursor(0, 0);
    // display.println("Connecting to MQTT...");
    // display.display();

    if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("Connected to MQTT");
      // display.clearDisplay();
      // display.setCursor(0, 0);
      // display.println("Connected to MQTT");
      // display.display();
      client.subscribe(mqtt_topic);
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retry in 5 seconds");
      // display.clearDisplay();
      // display.setCursor(0, 0);
      // display.println("Failed to connect MQTT");
      // display.println("Retry in 5 seconds");
      // display.display();
      delay(5000);
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    // display.clearDisplay();
    // display.setCursor(0, 0);
    // display.println("Attempting MQTT connection...");
    // display.display();

    if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("Connected to MQTT");
      // display.clearDisplay();
      // display.setCursor(0, 0);
      // display.println("Connected to MQTT");
      // display.display();
      client.subscribe(mqtt_topic);
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retry in 5 seconds");
      // display.clearDisplay();
      // display.setCursor(0, 0);
      // display.println("Failed to connect MQTT");
      // display.println("Retry in 5 seconds");
      // display.display();
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("An error occurred while mounting SPIFFS");
    return;
  }

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();
  display.display();

// โหลดข้อมูล Wi-Fi ที่บันทึกไว้
  String ssid, password;
  loadWiFiConfig(ssid, password);

  // พยายามเชื่อมต่อ Wi-Fi หากมีข้อมูลบันทึกไว้
  if (ssid.length() > 0 && password.length() > 0) {
    WiFi.begin(ssid.c_str(), password.c_str());
    Serial.print("Connecting to WiFi");
    displayMessage("Connecting to WiFi");

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
      delay(1000);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected to WiFi");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      String ipAddress = "Connected to WiFi\nIP:" + WiFi.localIP().toString();
      displayMessage(ipAddress.c_str());
    } else {
      Serial.println("Failed to connect to WiFi, starting AP mode...");
      displayMessage("Failed to connect\nStarting AP mode...");
    }
  }

  // ถ้าไม่มีข้อมูล Wi-Fi ที่บันทึกไว้ หรือเชื่อมต่อไม่สำเร็จ
  if (WiFi.status() != WL_CONNECTED) {
    // ตั้งค่า ESP32 เป็น Access Point
    bool result = WiFi.softAP(apSSID, apPassword);
    if(result) {
      Serial.println("Access Point Started");
      displayMessage("Access Point Started");
    } else {
      Serial.println("Access Point Failed");
      displayMessage("Access Point Failed");
    }

    // ตรวจสอบที่อยู่ IP ของ Access Point
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    String apIpAddress = "AP IP address: " + IP.toString();
    displayMessage(apIpAddress.c_str());
  }

  // กำหนดรูท URL และแสดงฟอร์ม HTML
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", htmlForm);
  });

  // รับข้อมูล SSID และรหัสผ่านจากฟอร์ม
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request){
    String ssid = request->getParam("ssid")->value();
    String password = request->getParam("password")->value();
    saveWiFiConfig(ssid, password);
    request->send(200, "text/html", "WiFi configuration saved. Please restart the ESP32.");
  });

  // รีเซ็ตข้อมูล Wi-Fi
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
    resetWiFiConfig();
    request->send(200, "text/html", "WiFi configuration reset. ESP32 is restarting...");
  });

  // เริ่มต้นเว็บเซิร์ฟเวอร์
  server.begin();

  // Connect to MQTT broker
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback); // Set callback function
  connectToMqttBroker();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Publish random temperature values to MQTT
  // float temperature = random(20, 40);
  // client.publish(mqtt_topic, String(temperature).c_str());

  // Display temperature on OLED
  // display.clearDisplay();
  // display.setTextSize(1);
  // display.setTextColor(SSD1306_WHITE);
  // display.setCursor(0, 0);
  // display.println("Temp:");
  // display.setCursor(0, 10);
  // display.println(String(temperature));
  // display.display();

  delay(1000);
}


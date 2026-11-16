
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ==================== WIFI CONFIGURATION ====================

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// ==================== MQTT CONFIGURATION ====================

const char* mqtt_server = "YOUR_MQTT_BROKER_IP";
const int mqtt_port = 1883;

const char* mqtt_topic = "gasmonitor/esp32/telemetry";
const char* mqtt_command_topic = "gasmonitor/esp32/command";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ==================== PIN CONFIGURATION ====================

#define MQ5_PIN       34
#define DHT_PIN       4
#define DHT_TYPE      DHT11
#define BUZZER_PIN    25
#define BUTTON_PIN    26

DHT dht(DHT_PIN, DHT_TYPE);

// ==================== GAS DETECTION SETTINGS ====================

#define GAS_ON_THRESHOLD        300
#define GAS_RECOVERY_THRESHOLD  250

#define GAS_DETECT_COUNT        2
#define GAS_RECOVERY_COUNT      3

// ==================== TIMING SETTINGS ====================

#define GAS_SAMPLE_INTERVAL    500
#define DHT_INTERVAL           2000
#define STATUS_INTERVAL        2000

#define MUTE_DURATION          10000

#define BUTTON_DEBOUNCE        50

#define BUZZER_ON_TIME         300
#define BUZZER_OFF_TIME        500

#define WIFI_RETRY_INTERVAL    10000
#define MQTT_RETRY_INTERVAL    5000

// ==================== GLOBAL VARIABLES ====================

// Gas sensor
int gasValue = 0;

int gasHighCount = 0;
int gasLowCount = 0;

bool gasDetected = false;
bool alarmLatched = false;

// Button and mute
bool buzzerMuted = false;

unsigned long muteStartTime = 0;

bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;

unsigned long lastDebounceTime = 0;

// Buzzer
bool buzzerOutputState = false;

unsigned long lastBuzzerChange = 0;

// Timers
unsigned long lastGasSample = 0;
unsigned long lastDHTRead = 0;
unsigned long lastStatusPrint = 0;

unsigned long lastWiFiAttempt = 0;
unsigned long lastMQTTAttempt = 0;

// DHT readings
float temperature = NAN;
float humidity = NAN;


// ==================== WIFI CONNECTION ====================

void connectWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();

  if (now - lastWiFiAttempt >= WIFI_RETRY_INTERVAL) {

    lastWiFiAttempt = now;

    Serial.println();
    Serial.println("Connecting to Wi-Fi...");

    WiFi.begin(ssid, password);
  }
}


// ==================== MQTT CONNECTION ====================

void mqttCallback(char* topic, byte* payload, unsigned int length) {

  String message = "";

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  message.trim();

  if (String(topic) == mqtt_command_topic) {

    if (message == "MUTE" && gasDetected) {

      buzzerMuted = true;
      muteStartTime = millis();

      buzzerOutputState = false;
      digitalWrite(BUZZER_PIN, LOW);

      Serial.println();
      Serial.println("REMOTE MUTE COMMAND RECEIVED");
      Serial.println("Buzzer muted for 10 seconds.");
    }
  }
}

void reconnectMQTT() {

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (mqttClient.connected()) {
    return;
  }

  unsigned long now = millis();

  if (now - lastMQTTAttempt < MQTT_RETRY_INTERVAL) {
    return;
  }

  lastMQTTAttempt = now;

  Serial.println();
  Serial.print("Connecting to MQTT broker... ");

  if (mqttClient.connect("ESP32_GAS_MONITOR")) {

    Serial.println("CONNECTED!");

    Serial.print("MQTT Telemetry Topic: ");
    Serial.println(mqtt_topic);

    mqttClient.subscribe(mqtt_command_topic);

    Serial.print("Subscribed to command topic: ");
    Serial.println(mqtt_command_topic);

  }

  else {

    Serial.print("FAILED. State: ");
    Serial.println(mqttClient.state());
  }
}


// ==================== READ MQ-5 SENSOR ====================

int readGasSensor() {

  long total = 0;

  const int samples = 30;

  for (int i = 0; i < samples; i++) {

    total += analogRead(MQ5_PIN);

    delay(5);
  }

  return total / samples;
}


// ==================== GAS DETECTION LOGIC ====================

void checkGas(int value) {

  // -------- GAS DETECTION --------

  if (value >= GAS_ON_THRESHOLD) {

    gasHighCount++;
    gasLowCount = 0;

    if (gasHighCount >= GAS_DETECT_COUNT && !gasDetected) {

      gasDetected = true;
      alarmLatched = true;

      buzzerMuted = false;

      buzzerOutputState = false;
      digitalWrite(BUZZER_PIN, LOW);

      Serial.println();
      Serial.println("!!! NEW GAS DETECTION !!!");
      Serial.println("ALARM ACTIVATED!");
    }
  }

  // -------- GAS RECOVERY --------

  else {

    gasHighCount = 0;

    if (gasDetected && value <= GAS_RECOVERY_THRESHOLD) {

      gasLowCount++;

      if (gasLowCount >= GAS_RECOVERY_COUNT) {

        gasDetected = false;
        alarmLatched = false;

        buzzerMuted = false;

        gasHighCount = 0;
        gasLowCount = 0;

        buzzerOutputState = false;
        digitalWrite(BUZZER_PIN, LOW);

        Serial.println();
        Serial.println("Gas recovery confirmed.");
        Serial.println("Alarm reset.");
      }
    }

    else {

      gasLowCount = 0;
    }
  }
}


// ==================== BUTTON HANDLING ====================

void checkButton() {

  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading) {

    lastDebounceTime = millis();
  }

  if (millis() - lastDebounceTime > BUTTON_DEBOUNCE) {

    if (reading != stableButtonState) {

      stableButtonState = reading;

      if (stableButtonState == LOW && gasDetected) {

        buzzerMuted = true;

        muteStartTime = millis();

        buzzerOutputState = false;
        digitalWrite(BUZZER_PIN, LOW);

        Serial.println();
        Serial.println("Silence button pressed.");
        Serial.println("Buzzer muted for 10 seconds.");
      }
    }
  }

  lastButtonReading = reading;
}


// ==================== MUTE TIMER ====================

void checkMuteTimer() {

  if (buzzerMuted && gasDetected) {

    if (millis() - muteStartTime >= MUTE_DURATION) {

      buzzerMuted = false;

      Serial.println();
      Serial.println("Mute period expired.");

      Serial.println("Gas still detected: alarm reactivated.");
    }
  }
}


// ==================== BUZZER CONTROL ====================

void updateBuzzer() {

  if (!alarmLatched || buzzerMuted) {

    buzzerOutputState = false;

    digitalWrite(BUZZER_PIN, LOW);

    return;
  }

  unsigned long currentTime = millis();

  if (buzzerOutputState == false) {

    if (currentTime - lastBuzzerChange >= BUZZER_OFF_TIME) {

      buzzerOutputState = true;

      lastBuzzerChange = currentTime;

      digitalWrite(BUZZER_PIN, HIGH);
    }
  }

  else {

    if (currentTime - lastBuzzerChange >= BUZZER_ON_TIME) {

      buzzerOutputState = false;

      lastBuzzerChange = currentTime;

      digitalWrite(BUZZER_PIN, LOW);
    }
  }
}


// ==================== DHT11 READING ====================

void readDHTSensor() {

  float newHumidity = dht.readHumidity();

  float newTemperature = dht.readTemperature();

  if (!isnan(newHumidity) && !isnan(newTemperature)) {

    humidity = newHumidity;
    temperature = newTemperature;
  }

  else {

    Serial.println("DHT11 reading error!");
  }
}


// ==================== MQTT DATA PUBLISHING ====================

void publishMQTTData() {

  if (!mqttClient.connected()) {
    return;
  }

  String tempValue = isnan(temperature)
                     ? "null"
                     : String(temperature, 2);

  String humidityValue = isnan(humidity)
                         ? "null"
                         : String(humidity, 2);

  String gasStatus = gasDetected ? "DETECTED" : "NORMAL";

  String buzzerStatus;

  if (buzzerOutputState) {
    buzzerStatus = "ACTIVE";
  }
  else if (buzzerMuted && gasDetected) {
    buzzerStatus = "MUTED";
  }
  else {
    buzzerStatus = "OFF";
  }

  String payload = "{";

  payload += "\"gas_raw\":" + String(gasValue) + ",";
  payload += "\"temperature\":" + tempValue + ",";
  payload += "\"humidity\":" + humidityValue + ",";
  payload += "\"gas_status\":\"" + gasStatus + "\",";
  payload += "\"gas_detected\":" + String(gasDetected ? "true" : "false") + ",";
  payload += "\"alarm_latched\":" + String(alarmLatched ? "true" : "false") + ",";
  payload += "\"buzzer\":\"" + buzzerStatus + "\",";
  payload += "\"buzzer_muted\":" + String(buzzerMuted ? "true" : "false");

  payload += "}";

  bool success = mqttClient.publish(
    mqtt_topic,
    payload.c_str()
  );

  if (success) {

    Serial.println();
    Serial.println("MQTT DATA PUBLISHED:");

    Serial.println(payload);
  }

  else {

    Serial.println("MQTT publishing failed!");
  }
}


// ==================== SERIAL MONITOR ====================

void printStatus() {

  Serial.println();
  Serial.println("========== SYSTEM STATUS ==========");

  Serial.print("MQ-5 RAW ADC: ");
  Serial.println(gasValue);

  Serial.print("Temperature: ");

  if (!isnan(temperature)) {
    Serial.print(temperature);
    Serial.println(" °C");
  }
  else {
    Serial.println("Unavailable");
  }

  Serial.print("Humidity: ");

  if (!isnan(humidity)) {
    Serial.print(humidity);
    Serial.println(" %");
  }
  else {
    Serial.println("Unavailable");
  }

  Serial.print("Gas Status: ");

  if (gasDetected) {
    Serial.println("GAS DETECTED");
  }
  else {
    Serial.println("NORMAL");
  }

  Serial.print("Buzzer: ");

  if (buzzerOutputState) {
    Serial.println("ACTIVE");
  }
  else if (buzzerMuted && gasDetected) {
    Serial.println("MUTED");
  }
  else {
    Serial.println("OFF");
  }

  Serial.print("Recovery counter: ");
  Serial.print(gasLowCount);
  Serial.print("/");
  Serial.println(GAS_RECOVERY_COUNT);

  Serial.print("Wi-Fi: ");
  Serial.println(
    WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED"
  );

  Serial.print("MQTT: ");
  Serial.println(
    mqttClient.connected() ? "CONNECTED" : "DISCONNECTED"
  );

  Serial.println("===================================");
}


// ==================== SETUP ====================

void setup() {

  Serial.begin(115200);

  pinMode(MQ5_PIN, INPUT);

  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  digitalWrite(BUZZER_PIN, LOW);

  analogReadResolution(12);

  analogSetPinAttenuation(MQ5_PIN, ADC_11db);

  dht.begin();

  Serial.println();
  Serial.println("===================================");
  Serial.println(" ESP32 GAS DETECTION SYSTEM");
  Serial.println(" MQ-5 + DHT11 + BUZZER + BUTTON");
  Serial.println(" MQTT ENABLED");
  Serial.println("===================================");

  Serial.println("Initializing sensors...");

  delay(2000);

  readDHTSensor();

  // Initialize Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.println("Connecting to Wi-Fi...");

  // Configure MQTT
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);

  Serial.println("System ready.");
}


// ==================== MAIN LOOP ====================

void loop() {

  unsigned long currentTime = millis();

  // 1. Maintain Wi-Fi connection
  connectWiFi();

  // 2. Maintain MQTT connection
  reconnectMQTT();

  // 3. Process incoming MQTT traffic
  if (mqttClient.connected()) {
    mqttClient.loop();
  }

  // 4. Read MQ-5 periodically
  if (currentTime - lastGasSample >= GAS_SAMPLE_INTERVAL) {

    lastGasSample = currentTime;

    gasValue = readGasSensor();

    checkGas(gasValue);
  }

  // 5. Read DHT11 every 2 seconds
  if (currentTime - lastDHTRead >= DHT_INTERVAL) {

    lastDHTRead = currentTime;

    readDHTSensor();
  }

  // 6. Check physical silence button
  checkButton();

  // 7. Check mute timer
  checkMuteTimer();

  // 8. Update buzzer state
  updateBuzzer();

  // 9. Print status and publish MQTT data
  if (currentTime - lastStatusPrint >= STATUS_INTERVAL) {

    lastStatusPrint = currentTime;

    printStatus();

    publishMQTTData();
  }
}

#include <heltec_unofficial.h>
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

// Configurações de WiFi
#define AP_PASS           "password"
#define EEPROM_SIZE       512
IPAddress local_IP(192, 168, 40, 1);
IPAddress gateway(192, 168, 40, 1);
IPAddress subnet(255, 255, 255, 0);

String ssid, password;
WebServer server(80);

unsigned long lastBatteryUpdate = 0;
unsigned long lastIPUpdate = 0;
const unsigned long batteryInterval = 30000; // 30 segundos
const unsigned long ipInterval = 60000;     // 60 segundos

// Recepção de dados LoRa
void processReceivedData(const String& data) {
  StaticJsonDocument<256> doc;

  DeserializationError error = deserializeJson(doc, data);

  if (error) {
    Serial.printf("Erro ao parsear JSON: %s\n", error.c_str());
    display.clear();
    display.printf("Erro JSON\n");
    display.display();
    return;
  }

  // Extrair dados do JSON
  String id = doc["id"] | "unknown";
  int value = doc["value"] | 0;
  int rssi = doc["rssi"] | 0;
  float bat = doc["bat"] | 0.0;

  // Mostrar dados no display
  display.clear();
  display.printf("ID: %s\n", id.c_str());
  display.printf("Valor: %d\n", value);
  display.printf("RSSI: %d dBm\n", rssi);
  display.printf("Bateria: %.2fV\n", bat);
  display.display();

  // Logar os dados no console
  Serial.printf("Recebido - ID: %s, Valor: %d, RSSI: %d, Bateria: %.2fV\n", 
                id.c_str(), value, rssi, bat);
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Configuração WiFi</title></head><body>";
  html += "<h1>Configuração WiFi</h1>";
  html += "<p><a href='/setup'>Configurar WiFi</a></p>";
  html += "<p><a href='/status'>Verificar Estado WiFi</a></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleSetup() {
  EEPROM.begin(EEPROM_SIZE);
  char storedSSID[32];
  char storedPassword[64];

  for (int i = 0; i < 32; ++i) {
    storedSSID[i] = char(EEPROM.read(i));
  }
  for (int i = 32; i < 96; ++i) {
    storedPassword[i - 32] = char(EEPROM.read(i));
  }

  storedSSID[31] = '\0';
  storedPassword[63] = '\0';

  ssid = String(storedSSID);
  password = String(storedPassword);

  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Configurar WiFi</title></head><body>";
  html += "<h1>Configuração WiFi</h1>";
  html += "<form action='/save' method='POST'>";
  html += "SSID: <input type='text' name='ssid' value='" + ssid + "'><br>";
  html += "Password: <input type='password' name='password' value='" + password + "'><br>";
  html += "<input type='submit' value='Salvar'>";
  html += "</form></body></html>";
  server.send(200, "text/html", html);
}

void handleSave() {
  ssid = server.arg("ssid");
  password = server.arg("password");

  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < 32; ++i) {
    EEPROM.write(i, i < ssid.length() ? ssid[i] : 0);
  }
  for (int i = 32; i < 96; ++i) {
    EEPROM.write(i, i - 32 < password.length() ? password[i - 32] : 0);
  }
  EEPROM.commit();

  String message = "Configurações salvas! Tentando conectar-se à rede WiFi: " + ssid;
  server.send(200, "text/html", message);

  delay(2000);
  ESP.restart();
}

void handleStatus() {
  String state = (WiFi.status() == WL_CONNECTED) ? "Conectado" : "Desconectado";
  String json = "{\"wifiStatus\": \"" + state + "\", \"ip\": \"" + WiFi.localIP().toString() + "\"}";
  server.send(200, "application/json", json);
}

void displayMessage(const String& message) {
  display.clear();
  display.println(message);
  display.display();
}

void connectToWiFi() {
  EEPROM.begin(EEPROM_SIZE);
  char storedSSID[32];
  char storedPassword[64];

  for (int i = 0; i < 32; ++i) {
    storedSSID[i] = char(EEPROM.read(i));
  }
  for (int i = 32; i < 96; ++i) {
    storedPassword[i - 32] = char(EEPROM.read(i));
  }

  storedSSID[31] = '\0';
  storedPassword[63] = '\0';

  ssid = String(storedSSID);
  password = String(storedPassword);

  if (ssid.length() > 0) {
    WiFi.begin(storedSSID, storedPassword);
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
      delay(500);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    String message = "WiFi conectado!\nIP: " + WiFi.localIP().toString();
    displayMessage(message);
    Serial.println(message);
  } else {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    String apSSID = "ESP32-" + String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
    apSSID.toUpperCase(); // Para manter o padrão de SSID em letras maiúsculas

    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(apSSID.c_str(), AP_PASS);

    String message = "Configure WiFi\nem 192.168.40.1";
    displayMessage(message);
    Serial.println("Modo AP ativo com SSID: " + apSSID);
  }
}

void setup() {
  heltec_setup();
  Serial.begin(115200);
  display.clear();

  display.println("Iniciando WiFi...");
  display.display();

  connectToWiFi();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/setup", HTTP_GET, handleSetup);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/status", HTTP_GET, handleStatus);
  server.begin();

  display.println("Receptor LoRa iniciado!");
  int state = radio.begin();
  if (state == RADIOLIB_ERR_NONE) {
    display.println("LoRa inicializado");
  } else {
    display.printf("Erro LoRa: %i\n", state);
  }
  display.display();
  delay(2000);
}

void loop() {
  server.handleClient();

  // Atualização periódica de informações
  unsigned long currentTime = millis();

  // Atualizar informações da bateria a cada 30 segundos
  if (currentTime - lastBatteryUpdate >= batteryInterval) {
    lastBatteryUpdate = currentTime;
    display.clear();
    float vbat = heltec_vbat();
    int batteryPercent = heltec_battery_percent(vbat);
    display.printf("Bateria: %.2fV (%d%%)\n", vbat, batteryPercent);
    display.display();
    Serial.printf("Bateria: %.2fV (%d%%)\n", vbat, batteryPercent);
  }

  // Informar o IP a cada 60 segundos, se conectado ao WiFi
  if (currentTime - lastIPUpdate >= ipInterval) {
    lastIPUpdate = currentTime;
    if (WiFi.status() == WL_CONNECTED) {
      String message = "IP: " + WiFi.localIP().toString();
      display.clear();
      display.println(message);
      display.display();
      Serial.println(message);
    }
  }

  // Recepção de dados LoRa
  String receivedData;
  int state = radio.receive(receivedData);

  if (state == RADIOLIB_ERR_NONE) {
    processReceivedData(receivedData); // Processar o JSON recebido
    display.clear();
    display.printf("Recebido: %s\n", receivedData.c_str());
    int rssi = radio.getRSSI();
    display.printf("RSSI: %i dBm\n", rssi);
    Serial.printf("Recebido: %s | RSSI: %i dBm\n", receivedData.c_str(), rssi);
    heltec_led(100);
    delay(100);
    heltec_led(0);
    display.display();
  } else if (state != RADIOLIB_ERR_RX_TIMEOUT) {
    display.clear();
    display.printf("Erro RX: %i\n", state);
    display.display();
  }
}


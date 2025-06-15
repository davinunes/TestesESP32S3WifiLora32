#include <heltec_unofficial.h>
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <NewPing.h> // Biblioteca para lidar com o sensor de ultrassom

#define TRIGGER_PIN 17  // Pino para o trigger do ultrassom
#define ECHO_PIN 18    // Pino para o echo do ultrassom
#define MAX_DISTANCE 400 // Distância máxima do ultrassom em cm

#define EEPROM_SIZE 512

int counter = 0; // Contador para as mensagens
WebServer server(80); // Servidor web

String deviceID = "default_id"; // ID do dispositivo, inicializado com "default_id"
String ssid; // Variável para armazenar o SSID com base no MAC

NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE); // Instância do sensor de ultrassom

// Função para carregar o ID da EEPROM
void loadDeviceID() {
  EEPROM.begin(EEPROM_SIZE);
  deviceID = "";
  for (int i = 0; i < EEPROM_SIZE; ++i) {
    char c = EEPROM.read(i);
    if (c == 0 || c == 255) break; // Fim da string ou EEPROM vazia
    deviceID += c;
  }
  if (deviceID.isEmpty()) {
    deviceID = "default_id"; // Se não houver ID salvo, usa o padrão
  }
}

// Função para criar o ponto de acesso (AP) com SSID baseado no MAC
void setupAP() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  String apSSID = "ESP32-" + String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
  apSSID.toUpperCase(); // Para manter o padrão de SSID em letras maiúsculas
  ssid = apSSID;

  WiFi.softAPConfig(IPAddress(192, 168, 40, 1), IPAddress(192, 168, 40, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid.c_str(), "password"); // Usa o SSID baseado no MAC
  Serial.println("Ponto de acesso (AP) iniciado: " + ssid);
}

// Função para exibir a página de configuração do ID
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Configuração ID</title></head><body>";
  html += "<h1>Configuração de ID</h1>";
  html += "<form action='/save' method='POST'>";
  html += "ID: <input type='text' name='id' value='" + deviceID + "'><br>";
  html += "<input type='submit' value='Salvar'>";
  html += "</form></body></html>";
  server.send(200, "text/html", html);
}

// Função para salvar o ID
void handleSave() {
  deviceID = server.arg("id");

  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < deviceID.length(); ++i) {
    EEPROM.write(i, deviceID[i]);
  }
  EEPROM.write(deviceID.length(), 0); // Marca o final da string
  EEPROM.commit();

  String message = "ID: " + deviceID;
  server.send(200, "text/html", message);
  delay(2000);
  ESP.restart(); // Reinicia o dispositivo para aplicar a nova configuração
}

void setup() {
  heltec_setup();
  Serial.begin(115200);
  display.clear();
  display.println("Iniciando ponto de acesso...");
  display.display();

  loadDeviceID(); // Carrega o ID salvo na EEPROM

  setupAP(); // Inicia o ponto de acesso

  // Configura as rotas do servidor web
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();

  display.println("Ponto de acesso (AP) iniciado!");
  display.display();
  delay(2000);

  // Inicializa o LoRa
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
  server.handleClient(); // Lida com as requisições do servidor

  // Leitura do sensor de ultrassom
  unsigned int distance = sonar.ping_cm();
  Serial.println(distance);

  // Mensagem JSON a ser transmitida
  StaticJsonDocument<256> doc;
  doc["id"] = deviceID;
  doc["value"] = distance;
  
  String message;
  serializeJson(doc, message);
  Serial.println(message);

  // Exibe a mensagem no display
  display.clear();
  //display.println("Transmissor");
  display.println(message.c_str());
  display.display();

  // Piscar LED durante a transmissão
  heltec_led(100); // Liga o LED
  int state = radio.transmit(message.c_str());
  heltec_led(0); // Desliga o LED após transmissão

  if (state == RADIOLIB_ERR_NONE) {
    display.println("Transmissão OK!");
  } else {
    display.printf("Erro TX: %i\n", state);
  }

  display.display();
  delay(3000); // Aguarda 3 segundos para enviar o próximo
}

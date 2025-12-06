#include <WiFi.h>
#include <PubSubClient.h>
#include "Cairo-project-1_inferencing.h"

const char* ssid = "AMF-CORP";
const char* password = "@MF$4515";

const char* mqtt_server = "broker.hivemq.com";
WiFiClient espClient;
PubSubClient client(espClient);

const int PINO_TRIG = 5;
const int PINO_ECHO = 18;
const int BUZZER_PIN = 23;   

bool alertaDisparado = false;  

void setup_wifi() {
  delay(1000);
  Serial.print("Conectando no WiFi");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado!");
}

void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.print("Conectando no MQTT...");
    if (client.connect("ESP32_PORTA")) {
      Serial.println("conectado!");
    } else {
      Serial.print("falha, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

float lerDistancia() {
  digitalWrite(PINO_TRIG, LOW);
  delayMicroseconds(5);
  digitalWrite(PINO_TRIG, HIGH);
  delayMicroseconds(15);
  digitalWrite(PINO_TRIG, LOW);

  long duracao = pulseIn(PINO_ECHO, HIGH, 30000);
  if (duracao <= 0) return -1;
  return duracao * 0.0343 / 2.0;
}

void setup() {
  Serial.begin(115200);

  pinMode(PINO_TRIG, OUTPUT);
  pinMode(PINO_ECHO, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
}

void loop() {
  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();

  float distancia = lerDistancia();
  if (distancia < 0) distancia = 0;

  signal_t signal;
  static float buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

  for (int i = 0; i < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; i++) {
    buffer[i] = distancia;
    delay(111); // 9Hz
  }

  signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
  signal.get_data = [](size_t offset, size_t length, float *out) {
    memcpy(out, buffer + offset, length * sizeof(float));
    return 0;
  };

  ei_impulse_result_t result;
  run_classifier(&signal, &result, false);

  String classe = "desconhecida";
  float maior = 0;

  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    if (result.classification[i].value > maior) {
      maior = result.classification[i].value;
      classe = result.classification[i].label;
    }
  }

  client.publish("iot/porta/distancia", String(distancia).c_str());
  client.publish("iot/porta/classificacao", classe.c_str());

  Serial.println("---------------------------");
  Serial.print("Distância: ");
  Serial.println(distancia);
  Serial.print("Classificação: ");
  Serial.println(classe);

  
  if (classe == "fechada") {
    digitalWrite(BUZZER_PIN, LOW);
    alertaDisparado = false;  
  }
  else if (classe == "entreaberta") {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  }
  else if (classe == "aberta") {
    if (!alertaDisparado) {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(1000);
      digitalWrite(BUZZER_PIN, LOW);
      alertaDisparado = true;
    }
  }

  delay(500);
}

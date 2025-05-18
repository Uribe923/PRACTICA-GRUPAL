#include <WiFi.h>                   // Para conectar el ESP32 al WiFi
#include <PubSubClient.h>          // Para conectarse a un broker MQTT
#include <Adafruit_GFX.h>          // Librería base para manejar gráficos en pantallas
#include <Adafruit_SSD1306.h>      // Librería para usar pantallas OLED con chip SSD1306
#include <DHT.h>                   // Librería para leer sensores de temperatura/humedad DHT
#include <Wire.h>                  // Para la comunicación I2C (necesaria para la pantalla)


// ---- CONFIGURACIÓN WIFI ----
const char* ssid     = "MIWIFI_2G_HkMC";  // Nombre de la red WiFi
const char* password = "uAYFGAUA";        // Contraseña del WiFi

// ---- CONFIGURACIÓN MQTT ----
const char* mqtt_username   = "Uribe";                  // Usuario MQTT (si lo pide el broker)
const char* mqtt_password   = "1234";                   // Contraseña del MQTT
const char* mqtt_server     = "broker.emqx.io";         // Dirección del broker (en la nube)
const int   mqtt_port       = 1883;                     // Puerto por defecto para MQTT sin cifrar
const char* mqtt_client_id  = "Ko4x8LEAIm";              // ID único del dispositivo para el broker
const char* mqtt_topic      = "sensor/alerta";           // Tema al que se publican las alertas

// ---- OBJETOS ----
WiFiClient espClient;                  // Cliente WiFi
PubSubClient mqtt_client(espClient);  // Cliente MQTT que usa ese WiFi

// ---- CONFIGURACIÓN OLED ----
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1  // No se usa pin de reset
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);  // Se crea el objeto pantalla

// ---- CONFIGURACIÓN DHT ----
#define DHTPIN 4            // Pin donde está conectado el sensor
#define DHTTYPE DHT11       // Tipo de sensor (en este caso, el DHT11)
DHT dht(DHTPIN, DHTTYPE);   // Se crea el objeto DHT

// ---- VARIABLES ----
float temperatura = 0.0;                 // Variable donde se guarda la temperatura
bool alertaEnviada = false;             // Para no enviar alertas repetidas
unsigned long tiempoAnterior = 0;       // Para controlar el tiempo entre lecturas
const unsigned long intervaloLectura = 5000; // Tiempo entre lecturas (5 segundos)

void setup() {
  Serial.begin(115200);  // Iniciar comunicación por puerto serie para ver mensajes

  Wire.begin(21, 22);    // Inicia el bus I2C en los pines 21 (SDA) y 22 (SCL) del ESP32

  // Iniciar pantalla OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("ERROR: No se pudo iniciar OLED");
    while (true); // Se queda parado si la pantalla no va
  }

  // Mostrar mensaje de inicio
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Iniciando...");
  display.display();
  delay(1000);

  // Iniciar el sensor DHT
  dht.begin();
  Serial.println("Sensor DHT OK");

  // Intentar conectar al WiFi
  display.setCursor(0, 10);
  display.println("Conectando WiFi...");
  display.display();

  WiFi.begin(ssid, password);
  int contador = 0;
  while (WiFi.status() != WL_CONNECTED && contador < 20) {
    delay(500);
    Serial.print(".");
    contador++;
  }

  // Si conecta, muestra mensaje; si no, se queda parado
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi OK");
    display.display();
  } else {
    Serial.println("Error al conectar WiFi");
    display.println("Error WiFi");
    display.display();
    while (true);  // Se queda en bucle si falla el WiFi
  }

  mqtt_client.setServer(mqtt_server, mqtt_port);  // Se configura el broker MQTT
}

void enviarAlerta(float temp) {
  // Si no está conectado al broker, intenta conectarse
  if (!mqtt_client.connected()) {
    Serial.println("Conectando a MQTT...");
    if (mqtt_client.connect(mqtt_client_id, mqtt_username, mqtt_password)) {
      Serial.println("MQTT conectado");
    } else {
      Serial.print("Fallo MQTT. Código: ");
      Serial.println(mqtt_client.state());
      return;
    }
  }

  // Crea el mensaje de alerta
  String mensaje = "ALERTA TEMP: " + String(temp) + " C";
  mqtt_client.publish(mqtt_topic, mensaje.c_str());  // Publica el mensaje
  Serial.println("Alerta publicada: " + mensaje);

  // Lo muestra también en pantalla
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("ALERTA ENVIADA");
  display.setCursor(0, 20);
  display.println(mensaje);
  display.display();
  delay(2000);  // Pausa para que dé tiempo a leer el mensaje
}

void loop() {
  mqtt_client.loop();  // Mantiene viva la conexión MQTT
  unsigned long tiempoActual = millis();

  // Cada 5 segundos (intervaloLectura)
  if (tiempoActual - tiempoAnterior >= intervaloLectura) {
    tiempoAnterior = tiempoActual;

    float temp = dht.readTemperature();  // Lee la temperatura
    if (isnan(temp)) {
      // Si hay error en la lectura
      Serial.println("Error al leer temperatura");
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("ERROR DHT");
      display.display();
      return;
    }

    temperatura = temp;
    Serial.println("Temperatura: " + String(temperatura));

    // Mostrar temperatura en pantalla
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Temp actual:");
    display.println(String(temperatura) + " C");

    // Comprobación de umbrales
    if (temperatura > 25 || temperatura < 17) {
      display.setCursor(0, 30);
      display.println("ALERTA!");
      display.display();

      // Solo envía una vez la alerta
      if (!alertaEnviada) {
        enviarAlerta(temperatura);
        alertaEnviada = true;
      }
    } else {
      display.setCursor(0, 30);
      display.println("Temp OK");
      display.display();
      alertaEnviada = false;  // Si está en rango, se resetea la bandera
    }
  }
}
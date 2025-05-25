#include <WiFi.h>
#include <PubSubClient.h>

// WiFi
const char *ssid = "SUPERONLINE_Wi-Fi_9KNE"; // Wi-Fi adınızı girin
const char *password = "the6x2sesz7E";  // Wi-Fi şifrenizi girin

// MQTT Broker
const char *mqtt_broker = "broker.emqx.io";
const char *topic = "emqx/esp32";
const char *control_topic = "emqx/esp32/control";
const char *mqtt_username = "emqx";
const char *mqtt_password = "public";
const int mqtt_port = 1883;

// Sensör ve Aktüatör Pinleri
const int soilMoisturePin = 34; // Analog giriş pini
const int relayPin = 26;        // Röle kontrol pini

WiFiClient espClient;
PubSubClient client(espClient);

// Zamanlayıcı
unsigned long lastMsg = 0;
const long interval = 60000; // 1 dakika

void callback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);
    Serial.print("Message: ");
    String message;
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.println(message);
    Serial.println("-----------------------");

    // Kontrol komutlarını işle
    if (String(topic) == control_topic) {
        if (message == "WATER") {
            waterPlant();
        }
    }
}

void reconnect() {
    // MQTT broker'a yeniden bağlanma denemeleri
    while (!client.connected()) {
        String client_id = "esp32-client-";
        client_id += String(WiFi.macAddress());
        Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
        if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
            Serial.println("Public EMQX MQTT broker connected");
            client.publish(topic, "Hi, I'm ESP32 ^^");
            client.subscribe(control_topic);
        } else {
            Serial.print("failed with state ");
            Serial.print(client.state());
            delay(2000);
        }
    }
}

void setup() {
    // Seri haberleşmeyi başlat
    Serial.begin(115200);
    
    // Wi-Fi bağlantısını başlat
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("Connecting to WiFi..");
    }
    Serial.println("Connected to the Wi-Fi network");

    // MQTT broker ayarları
    client.setServer(mqtt_broker, mqtt_port);
    client.setCallback(callback);

    // Sensör ve aktüatör pinlerini ayarla
    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, LOW); // Başlangıçta pompayı kapalı tut

    // İlk bağlantıyı sağla
    reconnect();
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    unsigned long now = millis();
    if (now - lastMsg > interval) {
        lastMsg = now;
        readSoilMoisture();
    }
}

int readSoilMoisture() {
    int soilMoistureValue = analogRead(soilMoisturePin);
    Serial.print("Soil Moisture: ");
    Serial.println(soilMoistureValue);

    // Nem değerini MQTT ile yayınla
    String moistureStr = String(soilMoistureValue);
    client.publish("emqx/esp32/soilMoisture", moistureStr.c_str());

    // Belirli bir nem seviyesinin altındaysa sulama yap
    int dryThreshold = 3000; // Bu değeri sensörünüze göre ayarlayın
    if (soilMoistureValue < dryThreshold) {
        waterPlant();
    }

    return soilMoistureValue;
}

void waterPlant() {
    digitalWrite(relayPin, HIGH); // Pompayı aç
    Serial.println("Watering started");
    delay(10000); // 10 saniye sulama
    digitalWrite(relayPin, LOW);  // Pompayı kapat
    Serial.println("Watering stopped");

    // MQTT ile sulama durumunu bildir
    client.publish("emqx/esp32/watering", "Watering completed");
}

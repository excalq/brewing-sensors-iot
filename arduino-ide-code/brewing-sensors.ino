// ESP32 Devboard with Wifi (WPA2), and MQTT to AK's AWS Server
#include <PubSubClient.h>
#include <WiFi.h>
#include <ssl_client.h>
#include <DHT.h>

///////////////// CHANGE THIS FOR YOUR SETUP ///////
const char* ssid = "MY_SSID";    // CHANGE ME!
const char* password = "WIFI_PASSWORD"; // CHANGE ME!

const char* mqtt_server = "MY.MQTTSERVER.XYZ"; // CHANGE ME!
const char* mqtt_user = "MYUSER"; // CHANGE ME!
const char* mqtt_pass = "MYPASS"; // CHANGE ME!
const int   mqtt_port = 1883;     // MQTT default port

// CHANGE TOPIC TO YOUR SETUP (TODO: Separte prefix, change type from const char*)
const char mqtt_topic_prefix[] = "ak-garage/#";
const char* mqtt_topic_dht_temp = "ak-garage/dht/temp";
const char* mqtt_topic_dht_humidity = "ak-garage/dht/humidity";
const char* mqtt_topic_kps = "ak-garage/kps/pressure";
const char* mqtt_topic_kps_raw = "ak-garage/kps/pressure_raw";
////////////////////////////////////////////////////

////////// DHT (Humidity + Temp) Setup

// Digital pin connected to the DHT sensor
#define DHTPIN 23 // Pin 2 can work but DHT must be disconnected during program upload.
#define DHTTYPE DHT11 // 3-pin DHT11 vs. DHT21 or DHT22 
// Sensor NOTES:
// Connect pin 1 (on the left) of the sensor to +5V
// NOTE: If using a board with 3.3V logic like an Arduino Due connect pin 1
// to 3.3V instead of 5V!
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 3 (on the right) of the sensor to GROUND
// Connect a 10KΩ resistor from pin 2 (data) to pin 1 (power) of the sensor

DHT dht(DHTPIN, DHTTYPE); // Initialize DHT sensor.

////////// KPS (Pressure Transducer) Setup

// Register whether the pressure sensor has given input
// Until that has happened, assume its not connective or active
bool kps_activated = false;
// KPS ADC pin for pressure sensor
#define PRESSURE_ADC_PIN 36

////////// General Init
WiFiClient wifiClient;
PubSubClient client(wifiClient);

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  String payload_printable = String((char *)payload);
  Serial.print("MQTT Server replied: {");
  Serial.print(topic);
  Serial.print("}: \"");
  Serial.print(payload_printable);
  Serial.println("\"");
}

void mqtt_reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "AK-Garage-ESP32-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      // ... and resubscribe
      client.subscribe(mqtt_topic_prefix);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// Ensure both sensors are working, flag error if not
void initialize() {
  if (isnan(dht.readTemperature())) {
      Serial.println(F("Failed to read from DHT sensor!"));
  }

  // Raw signal will be 0 if not present, or 4095 if only pullup resistor is present
  if (analogRead(PRESSURE_ADC_PIN) > 0 && analogRead(PRESSURE_ADC_PIN) < 4095) {
      kps_activated = true;
  } else {
      Serial.println(F("Failed to read from Keg Pressure sensor!"));
  }

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqtt_callback);
  
  if (!client.connect("esp32-ak-garage", mqtt_user, mqtt_pass)) {
    Serial.println("Unable to connect to MQTT");
  }
}

void poll_temp_and_humidity() {
  float dht_temp_calibration = -1.5; // °C based on calibration vs IR thermometer at 16.5°C
  float c_to_f = (9.0/5.0);
 
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature() + dht_temp_calibration;
  // Read temperature as Fahrenheit (isFahrenheit = true), calibrate to °F
  float f = dht.readTemperature(true) + (dht_temp_calibration * c_to_f);
  Serial.print("++++++++");
  Serial.print(dht.readTemperature(true));
  Serial.print(" Calibration for °F: ");
  Serial.println(dht_temp_calibration * c_to_f);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);

  // Publish to MQTT
  char temp_as_json[42];
  char humd_as_json[42];
  const char* temp_template = "{\"C\": %2.2f, \"F\": %2.2f}";
  const char* humd_template = "%2.2f%";
  sprintf(temp_as_json, temp_template, t, f);
  sprintf(humd_as_json, humd_template, h);
  
  client.publish(mqtt_topic_dht_temp, temp_as_json);
  
  if (client.publish(mqtt_topic_dht_humidity, humd_as_json)) {
    Serial.print(F("Successfully published: "));
    Serial.println(temp_as_json);
  } else {
    Serial.println(F("MQTT is unreachable!"));
  }

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.print(F("°C "));
  Serial.print(f);
  Serial.print(F("°F  Heat index: "));
  Serial.print(hic);
  Serial.print(F("°C "));
  Serial.print(hif);
  Serial.println(F("°F"));
}

void poll_transducer_pressure() {
  int reading = 0;
  int read_qty = 20;
  float pressure;

  if (!kps_activated) {
    return;
  }

  // take 20 samples at 20ms intervals (400ms)
  for (int i = 0; i < read_qty; i++) {
    reading += analogRead(PRESSURE_ADC_PIN);
    delay(20);
  }
  
  // simple linear model from sensor reading to PSI
  // Abdul: rough guess of 0psi (416) and slope (30.7 / psi) 
  // Arthur: baseline is ~425 (0psi) (with 100k ohm to GND)

  // Arthur: calibrated 2019.09.08 in Kepler Garage on CO2 tank w/ 150psi tranducer
  // Baseline at 0psi was ~425 (on ADC13/GPIO23 on NodeMCU ESP32 Dev Board, with 100kΩ resistor to GND)
  // Using the following values, calculated slope was 23.1:
  // {0: 425, 10: 625, 15: 750, 20: 865, 30: 1115}
  // https://docs.google.com/spreadsheets/d/1o72WBAO7lgtv2Z4gCs53Q8aE0iwgIj8KWk1Tl2XpHkI/edit?usp=sharing
  
  int baseline = 0; // With no input connected
  //int td_zero = 425; // Daves Tranducer at 0psi
  int td_zero = 482; // Arthurs Tranducer #1 at 0psi (#2 is 493)
  int td_thirty = 1100; // Tranducer
  float slope = 23.1; // Tranducer
  pressure = ((reading / read_qty) - (baseline + td_zero)) / slope;

  // MQTT Publish: pressure in PSI, Raw 12-bit sensor reading
  char str_buffer_pressure[10];
  char str_buffer_raw[10];
  const char* ftostr_template = "%2.2f";
  
  sprintf(str_buffer_pressure, ftostr_template, pressure);
  sprintf(str_buffer_raw, ftostr_template, (reading / read_qty));
  
  client.publish(mqtt_topic_kps, str_buffer_pressure);
  client.publish(mqtt_topic_kps_raw, str_buffer_raw);
    
  Serial.print(F("CO2 Keg Line pressure: "));
  Serial.print(pressure);
  Serial.print(F("PSI  Raw data reading was: "));
  Serial.println(reading / read_qty);
}


// --------- setup() && loop()

void setup() {
  Serial.begin(9600);
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  dht.begin();
  delay(500);
  initialize();  
}

void loop() {
  // Wait a few seconds between measurements.
  int measure_frequency = 2000; // Millsecs
  delay(measure_frequency);

  if (!client.connected()) {
    mqtt_reconnect();
  }
  client.loop();
  poll_temp_and_humidity();
  poll_transducer_pressure();
  client.subscribe(mqtt_topic_prefix);

}

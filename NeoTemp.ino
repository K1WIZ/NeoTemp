/*
 * Neopixel Ring Thermometer
 * john@wizworks.net
 * 
 * Permission for use is free to all, with the only condition that this notice is intact within
 * all copies or derivatives of this code. 
 * 
 */
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>

bool firstRun = true;
const char* devHostname = "WX-TEMP";

// Replace with the URL to the JSON data (HTTPS)
const char* jsonUrl = "https://tools.wizworks.net/observations.json";

// Replace with your NeoPixel configuration
#define NEOPIXEL_PIN 4  // Pin connected to the NeoPixel ring (GPIO4)
#define NUM_PIXELS 16    // Number of NeoPixels in the ring
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 120000;  //  in milliseconds
const unsigned long rebootInterval = 12 * 60 * 60 * 1000;  // 12 hours in milliseconds

void setup() {
  Serial.begin(115200);
  delay(10);
  strip.begin();
  strip.show();  // Initialize all pixels to 'off'

  // Set the initial color (e.g., red)
  strip.fill(strip.Color(2, 0, 4));   // purple while waiting for wifi connection
  strip.show();

  // Initialize WiFiManager
  WiFiManager wifiManager;

  // Set WiFi to station mode
  WiFi.mode(WIFI_STA);
  WiFi.hostname(devHostname);

  // Set the timeout for the configuration portal to 2 minutes (120 seconds)
  wifiManager.setConfigPortalTimeout(120);

  // Start the configuration portal and try to connect to the WiFi
  // If it fails to connect within the timeout, it will return false
  if (!wifiManager.autoConnect("WX Temperature")) {
    Serial.println("Failed to connect and hit timeout");
    // Wait a bit before restarting
    delay(3000);
    // Restart the ESP
    ESP.restart();
  }

  // If connected to WiFi, print the IP Address
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("MyIP: ");
    Serial.println(WiFi.localIP());
  }
}

void loop() {
  unsigned long currentMillis = millis();

  // Check if it's the first run or updateInterval milliseconds have passed
  if (firstRun || (currentMillis - lastUpdateTime >= updateInterval)) {
    lastUpdateTime = currentMillis;
    changeNeoPixelColor();

    // Set the firstRun flag to false after the initial run
    if (firstRun) {
      firstRun = false;
    }
  }

  // Check if it's time to force a reboot
  if (currentMillis >= rebootInterval) {
    Serial.println("Forcing a reboot...");
    ESP.restart();
  }
}

void changeNeoPixelColor() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    HTTPClient http;

    client.setInsecure(); // Ignore SSL certificate verification (not recommended in a production environment)

    http.begin(client, jsonUrl);

    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      http.end();

      // Parse the JSON data
      DynamicJsonDocument jsonDocument(1024);
      deserializeJson(jsonDocument, payload);

      // Access the nested "obs" array
      JsonArray obsArray = jsonDocument["obs"];

      if (obsArray.size() > 0) {
        // Access the first observation in the array
        JsonObject firstObservation = obsArray[0];

        // Extract the "air_temperature" from the first observation as a float in Celsius
        float airTemperatureCelsius = firstObservation["air_temperature"];
        Serial.print("Air Temperature (Celsius): ");
        Serial.println(airTemperatureCelsius, 2); // Print with 2 decimal places

        // Convert the temperature to Fahrenheit
        float airTemperatureFahrenheit = (airTemperatureCelsius * 9/5) + 32;
        Serial.print("Air Temperature (Fahrenheit): ");
        Serial.println(airTemperatureFahrenheit, 2); // Print with 2 decimal places

            // Adjust NeoPixel color based on temperature in Fahrenheit
            if (airTemperatureFahrenheit < 32.0) {
              strip.fill(strip.Color(180, 180, 180)); // White
            } else if (airTemperatureFahrenheit < 40.0) {
              strip.fill(strip.Color(128, 0, 128)); // Purple
            } else if (airTemperatureFahrenheit < 50.0) {
              strip.fill(strip.Color(0, 0, 200)); // Blue
            } else if (airTemperatureFahrenheit < 60.0) {
              strip.fill(strip.Color(0, 200, 0)); // Green
            } else if (airTemperatureFahrenheit < 70.0) {
              strip.fill(strip.Color(200, 200, 0)); // Yellow
            } else if (airTemperatureFahrenheit < 80.0) {
              strip.fill(strip.Color(200, 40, 0)); // Orange
            } else if (airTemperatureFahrenheit < 90.0) {
              strip.fill(strip.Color(200, 2, 160)); // Pink
            } else {
              strip.fill(strip.Color(200, 0, 0)); // Red
            }

            strip.show();
      } else {
        Serial.println("No observations found in the JSON data.");
      }
    } else {
      Serial.println("Failed to connect to JSON server");
    }
  } else {
    Serial.println("WiFi not connected");
  }
}

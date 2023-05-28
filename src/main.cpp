#include <Arduino.h>

#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <Wire.h>
#include <Adafruit_SGP30.h>

#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <WiFi.h>
#include <inttypes.h>
#include <stdio.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <TFT_eSPI.h>

char ssid[50];  // network SSID name
char pass[50];  // network password

char apiKey[50]; // weather api key
float latitude; // latitude for weather api
float longitude; // longitude for weather api

Adafruit_SGP30 sgp;

const int AQ_BUTTON = 0;
const int WEATHER_BUTTON = 35;

void nvs_access() {
  // Initialize NVS
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || 
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  // Open
  Serial.printf("\n");
  Serial.printf("Opening Non-Volatile Storage (NVS) handle... ");
  nvs_handle_t my_handle;
  err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    Serial.printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
  }
  else {
    Serial.printf("Done\n");
    Serial.printf("Retrieving SSID/PASSWD/apiKey\n");
    size_t ssid_len;
    size_t pass_len;
    size_t apiKey_len;
    err = nvs_get_str(my_handle, "ssid", ssid, &ssid_len);
    err |= nvs_get_str(my_handle, "pass", pass, &pass_len);
    err |= nvs_get_str(my_handle, "apiKey", apiKey, &apiKey_len);
    switch (err) {
      case ESP_OK:
        Serial.printf("Done\n");
        break;
      case ESP_ERR_NVS_NOT_FOUND:
        Serial.printf("The value is not initialized yet.\n");
        break;
      default:
        Serial.printf("Error (%s) reading.\n", esp_err_to_name(err));
        break;
    }
  }
  // Close
  nvs_close(my_handle);
}

String httpGET(const char* hostName) {
  WiFiClient client;
  HTTPClient http;

  http.begin(hostName);

  int responseCode = http.GET();

  String body = "{}";

  if(responseCode > 0) {
    //Serial.print("Got Response Code: ");
    //Serial.println(responseCode);
    body = http.getString();
  }
  else {
    Serial.print("Got error code: ");
    Serial.println(responseCode);
  }
  http.end();

  return body;
}

void getWeatherData() {
  // will use GPS to get lat and long, but for now just hardcoded
  latitude = 33.644510345086914;
  longitude = -117.82569552931159;

  char apiPath[150];
  sprintf(apiPath, "https://api.openweathermap.org/data/2.5/weather?lat=%.04f&lon=%.04f&appid=%s&units=imperial", latitude, longitude, apiKey);

  String apiResponse;
  if(WiFi.status() == WL_CONNECTED) {
    apiResponse = httpGET(apiPath);

    JSONVar data = JSON.parse(apiResponse);

    if(JSON.typeof(data) == "undefined") {
      Serial.println("Parsing input failed.");
      return;
    }

    Serial.print("Description: "); Serial.println(data["weather"][0]["main"]);
    Serial.print("Temperature: "); Serial.print(data["main"]["temp"]); Serial.println(" degrees F");
    Serial.print("Pressure: "); Serial.print(data["main"]["pressure"]); Serial.println(" hPa");
    Serial.print("Humidity: "); Serial.print(data["main"]["humidity"]); Serial.println("%");
    Serial.print("Wind Speed: "); Serial.print(data["wind"]["speed"]); Serial.println(" mph");
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(AQ_BUTTON, INPUT_PULLUP);
  pinMode(WEATHER_BUTTON, INPUT_PULLUP);
  delay(1000);
  
  // Retrieve SSID/PASSWD/apiKey from flash
  nvs_access();

  // Connect to WiFi network
  delay(1000);
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected\n");
  //Serial.println("IP address: ");
  //Serial.println(WiFi.localIP());
  //Serial.println("MAC address: ");
  //Serial.println(WiFi.macAddress());

  if (!sgp.begin()) {
    Serial.println("Sensor not found.");
    while(1);
  }

  Serial.println("Application Starting Now\n");
}

void loop() {
  if (!digitalRead(AQ_BUTTON)) {
    Serial.println("Fetching Air Quality Data...");

    if (!sgp.IAQmeasure()) {
      Serial.println("Measurement failed");
      return;
    }

    Serial.print("TVOC "); Serial.print(sgp.TVOC); Serial.print(" ppb\t");
    Serial.print("eCO2 "); Serial.print(sgp.eCO2); Serial.println(" ppm");

    if (!sgp.IAQmeasureRaw()) {
      Serial.println("Raw Measurement failed");
      return;
    }

    Serial.print("Raw H2 "); Serial.print(sgp.rawH2); Serial.print(" \t");
    Serial.print("Raw Ethanol "); Serial.print(sgp.rawEthanol); Serial.println("");

    delay(1000);

    while(!digitalRead(AQ_BUTTON)) {
      delay(250);
    }

    Serial.println("Returning to Home\n");
  }

  if (!digitalRead(WEATHER_BUTTON)) {
    Serial.println("Fetching Local Weather Data...");

    getWeatherData();

    delay(1000);

    while(!digitalRead(WEATHER_BUTTON)) {
      delay(250);
    }

    Serial.println("Returning to Home\n");
  }

  delay(100);
}
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

TFT_eSPI tft = TFT_eSPI();

char ssid[50];  // network SSID name
char pass[50];  // network password

char apiKey[50]; // weather api key

Adafruit_SGP30 sgp;

const int AQ_BUTTON = 0;        // Initialize Air Quality Button Pin
const int WEATHER_BUTTON = 35;  // Initialize Weather Button Pin
const int OFF_BUTTON = 17;      // Initialize Off Button Pin
const int BUZZER_PIN = 27;      // Initialize Buzzer Pin
const int BUZZER_BUTTON = 2;    // FOR DEMO PURPOSES ONLY Initialize Buzzer Button Pin

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

void alertUser(int eCO2) {  // alerts user until button press, then displays eCO2 level
  uint8_t count = 0;
  while(digitalRead(OFF_BUTTON)) {
    if(count%2 == 0) {
      tft.fillScreen(TFT_RED);
      tone(BUZZER_PIN, 262);
    }
    else {
      tft.fillScreen(TFT_BLACK);
      noTone(BUZZER_PIN);
    }
    delay(250);
  }
  noTone(BUZZER_PIN);

  tft.setCursor(0,0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.println("Unhealthy eCO2\nLevels Detected!");
  tft.print("Measured: ");
  tft.setTextColor(TFT_RED);
  tft.print(eCO2); tft.println(" ppm");
  tft.setTextColor(TFT_WHITE);
  tft.println("Recommend Wearing Mask");
  delay(1000);

  while(digitalRead(OFF_BUTTON));
  tft.fillScreen(TFT_BLACK);
}

int geteCO2() { // fetches eCO2 data from sensor
  if (!sgp.IAQmeasure()) {
    Serial.println("Measurement failed");
    return 0;
  }

  return sgp.eCO2;
}

void getAQData() {  // displays air quality data
  tft.setTextDatum(TC_DATUM);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Fetching\nData", tft.width()/2, tft.height()/2);

  // get data from sensor
  if (!sgp.IAQmeasure()) {
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_BLACK);
    tft.drawString("Measurement\nFailed", tft.width()/2, tft.height()/2);
    Serial.println("Measurement failed");
    delay(5000);
    tft.fillScreen(TFT_BLACK);
    return;
  }

  // print TVOC and eCO2 data to Serial
  Serial.print("TVOC "); Serial.print(sgp.TVOC); Serial.print(" ppb\t");
  Serial.print("eCO2 "); Serial.print(sgp.eCO2); Serial.println(" ppm");

  // get raw data from sensor
  if (!sgp.IAQmeasureRaw()) {
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_BLACK);
    tft.drawString("Raw\nMeasurement\nFailed", tft.width()/2, tft.height()/2);
    Serial.println("Raw Measurement failed");
    delay(5000);
    tft.fillScreen(TFT_BLACK);
    return;
  }

  // print raw data to Serial
  Serial.print("Raw H2 "); Serial.print(sgp.rawH2); Serial.print(" \t");
  Serial.print("Raw Ethanol "); Serial.print(sgp.rawEthanol); Serial.println("");

  // fill background
  tft.fillScreen(TFT_SKYBLUE);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.setTextFont(2);

  char buffer[10];

  // print TVOC data from sensor
  sprintf(buffer, "%d ppb", sgp.TVOC);
  tft.setTextSize(1);
  tft.drawString("TVOC:", 5, 5);
  tft.setTextDatum(BC_DATUM);
  tft.setTextSize(2);
  tft.drawString(String(buffer), tft.width()/2, tft.height()/4 - 5);

  // print eCO2 data from sensor
  sprintf(buffer, "%d ppm", sgp.eCO2);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.drawString("eCO2:", 5, tft.height()/4);
  tft.setTextDatum(BC_DATUM);
  tft.setTextSize(2);
  tft.drawString(String(buffer), tft.width()/2, tft.height()/2 - 7);

  // print raw H2 data from sensor
  sprintf(buffer, "%d", sgp.rawH2);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.drawString("Raw H2:", 5, tft.height()/2);
  tft.setTextDatum(BC_DATUM);
  tft.setTextSize(2);
  tft.drawString(String(buffer), tft.width()/2, 3 * (tft.height()/4) - 7);

  // print raw Ethanol data from sensor
  sprintf(buffer, "%d", sgp.rawEthanol);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.drawString("Raw Ethanol:", 5, 3 * (tft.height()/4));
  tft.setTextDatum(BC_DATUM);
  tft.setTextSize(2);
  tft.drawString(String(buffer), tft.width()/2, tft.height()-7);
}

void getWeatherData() { // displays weather data
  tft.setTextDatum(TC_DATUM);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Fetching\nData", tft.width()/2, tft.height()/2);

  // Using Irvine city ID for API call
  uint32_t cityID = 5359777;

  // fetch weather data
  char apiPath[150];
  sprintf(apiPath, "https://api.openweathermap.org/data/2.5/weather?id=%d&appid=%s&units=imperial", cityID, apiKey);

  String apiResponse;
  if(WiFi.status() == WL_CONNECTED) {
    apiResponse = httpGET(apiPath);

    JSONVar data = JSON.parse(apiResponse);

    if(JSON.typeof(data) == "undefined") {
      tft.fillScreen(TFT_RED);
      tft.setTextColor(TFT_BLACK);
      tft.drawString("Parsing\nData\nFailed", tft.width()/2, tft.height()/2);
      Serial.println("Parsing input failed.");
      delay(5000);
      tft.fillScreen(TFT_BLACK);
      return;
    }

    // print weather data to Serial
    Serial.print("Description: "); Serial.println(data["weather"][0]["main"]);
    Serial.print("Temperature: "); Serial.print(data["main"]["temp"]); Serial.println(" degrees F");
    Serial.print("Pressure: "); Serial.print(data["main"]["pressure"]); Serial.println(" hPa");
    Serial.print("Humidity: "); Serial.print(data["main"]["humidity"]); Serial.println("%");
    Serial.print("Wind Speed: "); Serial.print(data["wind"]["speed"]); Serial.println(" mph");

    String description = data["weather"][0]["main"];

    char buffer[13];

    // fill background
    tft.fillScreen(TFT_SKYBLUE);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_WHITE);
    tft.setTextFont(2);

    // print description of weather from API
    tft.setTextSize(2);
    tft.drawString(description, tft.width()/2, 10);

    // draw line
    tft.drawLine(0, 52, tft.width(), 52, TFT_WHITE);

    // print temperature in degrees fahrenheit from API
    sprintf(buffer, "%d", static_cast<int>(data["main"]["temp"]));
    tft.setTextSize(4);
    tft.drawString(String(buffer), tft.width()/2, 60);
    tft.setTextSize(1);
    tft.drawString("degrees F", tft.width()/2, 110);

    // print air pressure in hPa from API
    sprintf(buffer, "%d", static_cast<int>(data["main"]["pressure"]));
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(2);
    tft.drawString(String(buffer), 15, 130);
    tft.setTextSize(1);
    tft.drawString("hPa", tft.width()/2 + 20, 140);

    // print humidity percentage from API
    sprintf(buffer, "%d%", static_cast<int>(data["main"]["humidity"]));
    tft.setTextSize(2);
    tft.drawString(String(buffer), 15, 160);
    tft.setTextSize(1);
    tft.drawString("humidity", tft.width()/2 + 5, 170);

    // print wind speed in mph from API
    sprintf(buffer, "%d", static_cast<int>(data["wind"]["speed"]));
    tft.setTextSize(2);
    tft.drawString(String(buffer), 15, 190);
    tft.setTextSize(1);
    tft.drawString("mph wind", tft.width()/2 - 15, 200);
  }
}

void setup() {
  Serial.begin(9600);
  // set up I/O pins
  pinMode(AQ_BUTTON, INPUT_PULLUP);
  pinMode(WEATHER_BUTTON, INPUT_PULLUP);
  pinMode(OFF_BUTTON, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUZZER_BUTTON, INPUT_PULLUP);
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

  if (!sgp.begin()) {
    Serial.println("Sensor not found.");
    while(1);
  }

  // initialize screen
  tft.init();
  tft.setRotation(4);
  tft.fillScreen(TFT_BLACK);
}

void loop() {
  if (!digitalRead(AQ_BUTTON)) {  // displays air quality data on button press
    getAQData();
  }

  if (!digitalRead(WEATHER_BUTTON)) { // displays weather data on button press
    getWeatherData();
  }

  if (!digitalRead(OFF_BUTTON)) { // turns display off on button press
    tft.fillScreen(TFT_BLACK);
  }

  int eCO2 = geteCO2();

  if (eCO2 >= 600) {  // triggers alert if eCO2 levels are above threshold
    alertUser(eCO2);
  }

  if (!digitalRead(BUZZER_BUTTON)) {  // Manually triggers alert on button press (DEMO PURPOSES ONLY)
    alertUser(600);
  }

  delay(100);
}
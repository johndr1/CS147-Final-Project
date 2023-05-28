#include <Arduino.h>

#include <TinyGPS++.h>
#include <Adafruit_SGP30.h>

#include <HttpClient.h>
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


void setup() {
  Serial.begin(9600);
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
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("MAC address: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  // put your main code here, to run repeatedly:
}
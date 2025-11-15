#include "define.h"

static esp_wps_config_t wps_config = WPS_CONFIG_INIT_DEFAULT(WPS_TYPE_PBC);
WpsManager wpsManager;
void WpsManager::begin()
{
    WiFi.onEvent(WpsManager::WiFiEvent);
}

void WpsManager::StartWps()
{
    Serial.println("WPS Başlatılıyor...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_wps_enable(&wps_config);
    esp_wifi_wps_start(0);
}

void WpsManager::WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
{
    switch (event)
    {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        break;

    case ARDUINO_EVENT_WPS_ER_SUCCESS:
        Serial.println("WPS BAŞARILI");
        esp_wifi_wps_disable();
        WiFi.begin(); // stored credentials
        break;

    case ARDUINO_EVENT_WPS_ER_FAILED:
    case ARDUINO_EVENT_WPS_ER_TIMEOUT:
        Serial.println("WPS BAŞARISIZ");
        esp_wifi_wps_disable();
        break;

    default:
        break;
    }
}

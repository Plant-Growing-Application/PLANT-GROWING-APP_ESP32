#pragma once
#ifndef WPS_H
#define WPS_H

// Bazı ESP32 SDK sürümlerinde WPS_CONFIG_INIT_DEFAULT tanımı eksik olabiliyor.
// Bu durumda IntelliSense hata verir ama build geçer.
// Aşağıdaki tanım her iki durumda da uyum sağlar.
#ifndef WPS_CONFIG_INIT_DEFAULT
#define WPS_CONFIG_INIT_DEFAULT(type) {               \
    .wps_type = type,                                 \
    .crypto_funcs = &g_wifi_default_wps_crypto_funcs, \
    .factory_info = &g_wifi_default_wps_factory_info, \
}
#endif

class WpsManager
{
public:
    void begin();
    void StartWps();
    static void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);

private:
};
extern WpsManager wpsManager;  

#endif /* WPS_H */

/*
  esp3dconfig.cpp -  wifi functions class

  Copyright (c) 2014 Luc Lebosse. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "esp3dlibconfig.h"
#if defined(ESP3D_WIFISUPPORT)
#include <Preferences.h>
#include "wificonfig.h"
#include <Arduino.h>
#ifdef __cplusplus
extern "C" {
#endif
esp_err_t esp_task_wdt_reset();
#ifdef __cplusplus
}
#endif

bool Esp3DLibConfig::restart_ESP_module = false;

/*
 * delay is to avoid sometimes and may not enough neither
 */
void Esp3DLibConfig::wait(uint32_t milliseconds)
{
    esp_task_wdt_reset(); //for a wait 0;
    vTaskDelay((milliseconds>0?milliseconds:1) / portTICK_RATE_MS);  // Yield to other tasks
}

/**
 * Restart ESP
 */
void Esp3DLibConfig::restart_ESP()
{
    restart_ESP_module=true;
}

/**
 * Handle not critical actions that must be done in sync environement
 */
void Esp3DLibConfig::handle()
{
    //in case of restart requested
    if (restart_ESP_module) {
        ESP.restart();
        while (1) {};
    }
}

bool Esp3DLibConfig::reset_settings()
{
    Preferences prefs;
    prefs.begin(NAMESPACE, false);
    String sval;
    int8_t bbuf;
    int16_t ibuf;
    bool error = false;
    sval = DEFAULT_HOSTNAME;
    if (prefs.putString(HOSTNAME_ENTRY, sval) == 0) {
        error = true;
    }
    sval = DEFAULT_STA_SSID;
    if (prefs.putString(STA_SSID_ENTRY, sval) == 0) {
        error = true;
    }
    sval = DEFAULT_STA_PWD;
    if (prefs.putString(STA_PWD_ENTRY, sval) != sval.length()) {
        error = true;
    }
    sval = DEFAULT_AP_SSID;
    if (prefs.putString(AP_SSID_ENTRY, sval) == 0) {
        error = true;
    }
    sval = DEFAULT_AP_PWD;
    if (prefs.putString(AP_PWD_ENTRY, sval) != sval.length()) {
        error = true;
    }

    bbuf = DEFAULT_AP_CHANNEL;
    if (prefs.putChar(AP_CHANNEL_ENTRY, bbuf) ==0 ) {
        error = true;
    }
    bbuf = DEFAULT_STA_IP_MODE;
    if (prefs.putChar(STA_IP_MODE_ENTRY, bbuf) ==0 ) {
        error = true;
    }
    bbuf = DEFAULT_HTTP_STATE;
    if (prefs.putChar(HTTP_ENABLE_ENTRY, bbuf) ==0 ) {
        error = true;
    }
    /*bbuf = DEFAULT_TELNET_STATE;
    if (prefs.putChar(TELNET_ENABLE_ENTRY, bbuf) ==0 ) {
        error = true;
    }*/
    bbuf = DEFAULT_RADIO_MODE;
    if (prefs.putChar(ESP_RADIO_MODE, bbuf) ==0 ) {
        error = true;
    }
    ibuf = DEFAULT_WEBSERVER_PORT;
    if (prefs.putUShort(HTTP_PORT_ENTRY, ibuf) == 0) {
        error = true;
    }
    /*ibuf = DEFAULT_TELNETSERVER_PORT;
    if (prefs.putUShort(TELNET_PORT_ENTRY, ibuf) == 0) {
        error = true;
    }*/
    sval = DEFAULT_STA_IP;
    if (prefs.putInt(STA_IP_ENTRY, WiFiConfig::IP_int_from_string(sval)) == 0) {
        error = true;
    }
    sval = DEFAULT_STA_GW;
    if (prefs.putInt(STA_GW_ENTRY, WiFiConfig::IP_int_from_string(sval)) == 0) {
        error = true;
    }
    sval = DEFAULT_STA_MK;
    if (prefs.putInt(STA_MK_ENTRY, WiFiConfig::IP_int_from_string(sval)) == 0) {
        error = true;
    }
    sval = DEFAULT_AP_IP;
    if (prefs.putInt(AP_IP_ENTRY, WiFiConfig::IP_int_from_string(sval)) == 0) {
        error = true;
    }
    prefs.end();
    return error;
}

#endif // ESP3D_WIFISUPPORT

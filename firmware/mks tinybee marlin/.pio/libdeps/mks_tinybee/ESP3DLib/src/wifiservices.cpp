/*
  wifiservices.cpp -  wifi services functions class

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
#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>
#include "espcom.h"
#include <Preferences.h>
#include "wificonfig.h"
#include "wifiservices.h"
#ifdef MDNS_FEATURE
#include <ESPmDNS.h>
#endif //MDNS_FEATURE
#ifdef OTA_FEATURE
#include <ArduinoOTA.h>
#endif //OTA_FEATURE
#ifdef HTTP_FEATURE
#include "web_server.h"
#endif //HTTP_FEATURE

WiFiServices wifi_services;

bool WiFiServices::_started = false;

WiFiServices::WiFiServices()
{
}
WiFiServices::~WiFiServices()
{
    end();
}

bool WiFiServices::begin()
{
    bool no_error = true;
    //Sanity check
    if(WiFi.getMode() == WIFI_OFF) {
        return false;
    }
    String h;
    Preferences prefs;
    //Get hostname
    String defV = DEFAULT_HOSTNAME;
    prefs.begin(NAMESPACE, true);
    h = prefs.getString(HOSTNAME_ENTRY, defV);
    prefs.end();
    //Start SPIFFS
    SPIFFS.begin(true);

#ifdef OTA_FEATURE
    ArduinoOTA
    .onStart([]() {
        String type = "OTA:Start OTA updating ";
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type += "sketch";
        } else { // U_SPIFFS
            // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
            type += "filesystem";
            SPIFFS.end();
        }
        Esp3DCom::echo(type.c_str());
    })
    .onEnd([]() {
        Esp3DCom::echo("OTA:End");

    })
    .onProgress([](unsigned int progress, unsigned int total) {
        String tmp = "OTA:OTA Progress:";
        tmp += String(progress / (total / 100));
        tmp +="%%";
        Esp3DCom::echo(tmp.c_str());
    })
    .onError([](ota_error_t error) {
        String tmp = "OTA: Error(";
        tmp += String(error) + ")";
        Esp3DCom::echo(tmp.c_str());
        if (error == OTA_AUTH_ERROR) {
            Esp3DCom::echo("OTA:Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            Esp3DCom::echo("OTA:Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            Esp3DCom::echo("OTA:Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            Esp3DCom::echo("OTA:Receive Failed");
        } else if (error == OTA_END_ERROR) {
            Esp3DCom::echo("OTA:End Failed");
        }
    });
    ArduinoOTA.begin();
    Esp3DCom::echo("OTA service started");
#endif
#ifdef MDNS_FEATURE
    //no need in AP mode
    if(WiFi.getMode() == WIFI_STA) {
        //start mDns
        if (!MDNS.begin(h.c_str())) {
            Esp3DCom::echo("Cannot start mDNS");
            no_error = false;
        } else {
            String tmp = "mDNS service (" + h;
            tmp+=") started";
            Esp3DCom::echo(tmp.c_str());
        }
    }
#endif //MDNS_FEATURE
#ifdef AUTHENTICATION_FEATURE
    Esp3DCom::echo("Authentication enabled");
#endif //AUTHENTICATION_FEATURE
#ifdef HTTP_FEATURE
    web_server.begin();
#endif //HTTP_FEATURE
    _started = true;
    return no_error;
}
void WiFiServices::end()
{
    _started = false;
#ifdef HTTP_FEATURE
    web_server.end();
#endif
    //stop OTA
#ifdef OTA_FEATURE
    ArduinoOTA.end();
#endif //OTA_FEATURE
    //Stop SPIFFS
    SPIFFS.end();

#ifdef MDNS_FEATURE
    //Stop mDNS
    //MDNS.end();
#endif //MDNS_FEATURE
}

void WiFiServices::handle()
{
#ifdef OTA_FEATURE
    ArduinoOTA.handle();
#endif //OTA_FEATURE
#ifdef HTTP_FEATURE
    web_server.handle();
#endif //HTTP_FEATURE
}

#endif // ENABLE_WIFI

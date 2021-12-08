/*
  wificonfig.h -  wifi functions class

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

//Services that need to be used
#include "esp3dlibconfig.h"

#ifndef _WIFI_CONFIG_H
#define _WIFI_CONFIG_H

#include <WiFi.h>

class WiFiConfig
{
public:
    static bool isValidIP(const char * string);
    static bool isPasswordValid (const char * password);
    static bool isSSIDValid (const char * ssid);
    static bool isHostnameValid (const char * hostname);
    static uint32_t IP_int_from_string(String & s);
    static String IP_string_from_int(uint32_t ip_int);
    static const char * currentIP();
    static bool StartAP();
    static bool StartSTA();
    static void StopWiFi();
    static int32_t getSignal (int32_t RSSI);
    static void begin();
    static void end();
    static void handle();
private :
    static bool ConnectSTA2AP();
    static void WiFiEvent(WiFiEvent_t event);
};

#endif

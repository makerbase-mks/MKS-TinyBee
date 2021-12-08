/*
  command.cpp - ESP3D command class

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
#include "command.h"
#include "wificonfig.h"
#if defined(SDSUPPORT)
#include "sd_ESP32.h"
#endif
#include <Preferences.h>
#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>
#include <esp_wifi.h>
#if defined(HTTP_FEATURE)
//#include "web_server.h"
#include <WebServer.h>
#endif //HTTP_FEATURE
#include "serial2socket.h"
#include <esp_ota_ops.h>
#include MARLIN_PATH(inc/Version.h)

String COMMAND::get_param (String & cmd_params, const char * id, bool withspace)
{
    static String parameter;
    String sid = id;
    int start;
    int end = -1;
    parameter = "";
    //if no id it means it is first part of cmd
    if (strlen (id) == 0) {
        start = 0;
    }
    //else find id position
    else {
        start = cmd_params.indexOf (id);
    }
    //if no id found and not first part leave
    if (start == -1 ) {
        return parameter;
    }
    //password and SSID can have space so handle it
    //if no space expected use space as delimiter
    if (!withspace) {
        end = cmd_params.indexOf (" ", start);
    }
    //if no end found - take all
    if (end == -1) {
        end = cmd_params.length();
    }
    //extract parameter
    parameter = cmd_params.substring (start + strlen (id), end);
    //be sure no extra space
    parameter.trim();
    return parameter;
}


#ifdef AUTHENTICATION_FEATURE
bool COMMAND::isLocalPasswordValid (const char * password)
{
    char c;
    //limited size
    if ( (strlen (password) > MAX_LOCAL_PASSWORD_LENGTH) ||  (strlen (password) < MIN_LOCAL_PASSWORD_LENGTH) ) {
        return false;
    }
    //no space allowed
    for (int i = 0; i < strlen (password); i++) {
        c = password[i];
        if (c == ' ') {
            return false;
        }
    }
    return true;
}
#endif //AUTHENTICATION_FEATURE


bool COMMAND::execute_internal_command (int cmd, String cmd_params, level_authenticate_type auth_level,  ESPResponseStream  *espresponse)
{
    bool response = true;
    level_authenticate_type auth_type = auth_level;

    //manage parameters
    String parameter;
    switch (cmd) {
    case 0:
        espresponse->println ("[List of ESP3D commands]");
        espresponse->println ("[ESP] - display this help");
        espresponse->println ("[ESP100](SSID) - display/set STA SSID");
        espresponse->println ("[ESP101](Password) - set STA password");
        espresponse->println ("[ESP102](Mode) - display/set STA IP mode (DHCP/STATIC)");
        espresponse->println ("[ESP103](IP=xxxx MSK=xxxx GW=xxxx) - display/set STA IP/Mask/GW ");
        espresponse->println ("[ESP105](SSID) - display/set AP SSID");
        espresponse->println ("[ESP106](Password) - set AP password");
        espresponse->println ("[ESP107](IP) - display/set AP IP");
        espresponse->println ("[ESP108](Chanel) - display/set AP chanel");
        espresponse->println ("[ESP110](State) - display/set radio state which can be STA, AP, OFF");
        espresponse->println ("[ESP111]display current IP");
        espresponse->println ("[ESP112](Hostname) - display/set Hostname");
        espresponse->println ("[ESP120](State) - display/set HTTP state which can be ON, OFF");
        espresponse->println ("[ESP121](Port) - display/set HTTP port ");
        espresponse->println ("[ESP200] - display SD Card Status");
        espresponse->println ("[ESP400] - display ESP3D settings in JSON");
        espresponse->println ("[ESP401]P=(position) T=(type) V=(value) - Set specific setting");
        espresponse->println ("[ESP410] - display available AP list (limited to 30) in JSON");
        espresponse->println ("[ESP420] - display ESP3D current status");
        espresponse->println ("[ESP444](Cmd) - set ESP3D state (RESET/RESTART)");
#ifdef AUTHENTICATION_FEATURE
        espresponse->println ("[ESP550](Password) - set admin password");
        espresponse->println ("[ESP555](Password) - set user password");
#endif //AUTHENTICATION_FEATURE
        espresponse->println ("[ESP710]FORMAT - Format ESP Filesystem");
        espresponse->println ("[ESP800] - display FW Informations");
        break;

    //STA SSID
    //[ESP100]<SSID>[pwd=<admin password>]
    case 100: {
#ifdef AUTHENTICATION_FEATURE
        if (auth_type == LEVEL_GUEST) {
            if (espresponse) {
                espresponse->println ("Error: Wrong authentication!");
            }
            return false;
        }
#endif //AUTHENTICATION_FEATURE
        parameter = get_param (cmd_params, "", true);
        if ((parameter.length() == 0) && !espresponse) {
            return false;
        }
        //get
        if (parameter.length() == 0) {
            Preferences prefs;
            prefs.begin(NAMESPACE, true);
            String defV = DEFAULT_STA_SSID;
            espresponse->println(prefs.getString(STA_SSID_ENTRY, defV).c_str());
            prefs.end();
        } else { //set
#ifdef ENABLE_AUTHENTICATION
            if (auth_type != LEVEL_ADMIN) {
                if (espresponse) {
                    espresponse->println ("Error: Wrong authentication!");
                }
                return false;
            }
#endif
            if (!WiFiConfig::isSSIDValid (parameter.c_str() ) ) {
                if(espresponse) {
                    espresponse->println ("Error: Incorrect SSID!");
                }
                response = false;
            } else {
                Preferences prefs;
                prefs.begin(NAMESPACE, false);
                if (prefs.putString(STA_SSID_ENTRY, parameter) == 0) {
                    response = false;
                    if(espresponse) {
                        espresponse->println ("Error: Set failed!");
                    }
                } else if(espresponse) {
                    espresponse->println ("ok");
                }
                prefs.end();
            }
        }
    }
    break;
    //STA Password
    //[ESP101]<Password>
    case 101: {
#ifdef ENABLE_AUTHENTICATION
        if (auth_type != LEVEL_ADMIN) {
            if(espresponse) {
                espresponse->println ("Error: Wrong authentication!");
            }
            return false;
        }
#endif
        parameter = get_param (cmd_params, "", true);
        if (!WiFiConfig::isPasswordValid (parameter.c_str() ) ) {
            if(espresponse) {
                espresponse->println ("Error: Incorrect password!");
            }
            response = false;
            return false;
        } else {
            Preferences prefs;
            prefs.begin(NAMESPACE, false);
            if (prefs.putString(STA_PWD_ENTRY, parameter) != parameter.length()) {
                response = false;
                if(espresponse) {
                    espresponse->println ("Error: Set failed!");
                }
            } else if(espresponse) {
                espresponse->println ("ok");
            }
            prefs.end();
        }
    }
    break;
    //Get/Change STA IP mode (DHCP/STATIC)
    //[ESP102]<mode>
    case 102: {
#ifdef ENABLE_AUTHENTICATION
        if (auth_type == LEVEL_GUEST) {
            if (espresponse) {
                espresponse->println ("Error: Wrong authentication!");
            }
            return false;
        }
#endif
        parameter = get_param (cmd_params, "", true);
        if ((parameter.length() == 0) && !espresponse) {
            return false;
        }
        //get
        if (parameter.length() == 0) {
            Preferences prefs;
            prefs.begin(NAMESPACE, true);
            int8_t resp = prefs.getChar(STA_IP_MODE_ENTRY, DEFAULT_STA_IP_MODE);
            if (resp == DHCP_MODE) {
                espresponse->println("DHCP");
            } else if (resp == STATIC_MODE) {
                espresponse->println("STATIC");
            } else {
                espresponse->println("???");
            }
            prefs.end();
        } else { //set
#ifdef ENABLE_AUTHENTICATION
            if (auth_type != LEVEL_ADMIN) {
                if (espresponse) {
                    espresponse->println ("Error: Wrong authentication!");
                }
                return false;
            }
#endif
            parameter.toUpperCase();
            if (!((parameter == "STATIC") || (parameter == "DHCP"))) {
                if(espresponse) {
                    espresponse->println ("Error: only STATIC or DHCP mode supported!");
                }
                response = false;
                return false;
            } else {
                Preferences prefs;
                prefs.begin(NAMESPACE, false);
                int8_t bbuf = (parameter == "DHCP")?DHCP_MODE:STATIC_MODE;
                if (prefs.putChar(STA_IP_MODE_ENTRY, bbuf) == 0) {
                    response = false;
                    if(espresponse) {
                        espresponse->println ("Error: Set failed!");
                    }
                } else if(espresponse) {
                    espresponse->println ("ok");
                }
                prefs.end();
            }
        }
    }
    break;
    //Get/Change STA IP/Mask/GW
    //[ESP103]IP=<IP> MSK=<IP> GW=<IP>
    case 103: {
#ifdef ENABLE_AUTHENTICATION
        if (auth_type == LEVEL_GUEST) {
            espresponse->println ("Error: Wrong authentication!");
            return false;
        }
#endif
        parameter = get_param (cmd_params, "", true);
        if ((parameter.length() == 0) && !espresponse) {
            return false;
        }
        //get
        if (parameter.length() == 0) {
            Preferences prefs;
            prefs.begin(NAMESPACE, true);
            //IP
            String defV = DEFAULT_STA_IP;
            int32_t IP = prefs.getInt(STA_IP_ENTRY, WiFiConfig::IP_int_from_string(defV));
            //GW
            defV = DEFAULT_STA_GW;
            int32_t GW = prefs.getInt(STA_GW_ENTRY, WiFiConfig::IP_int_from_string(defV));
            //MK
            defV = DEFAULT_STA_MK;
            int32_t MK = prefs.getInt(STA_MK_ENTRY, WiFiConfig::IP_int_from_string(defV));
            defV = "IP:" + WiFiConfig::IP_string_from_int(IP) + ", GW:" + WiFiConfig::IP_string_from_int(GW) + ", MSK:" + WiFiConfig::IP_string_from_int(MK);
            espresponse->println(defV.c_str());
            prefs.end();
        } else { //set
#ifdef ENABLE_AUTHENTICATION
            if (auth_type != LEVEL_ADMIN) {
                if (espresponse) {
                    espresponse->println ("Error: Wrong authentication!");
                }
                return false;
            }
#endif

            String IP = get_param (cmd_params, "IP=", false);
            String GW = get_param (cmd_params, "GW=", false);
            String MSK = get_param (cmd_params, "MSK=", false);
            Serial.println(IP);
            Serial.println(GW);
            if ( !WiFiConfig::isValidIP(IP.c_str())) {
                if(espresponse) {
                    espresponse->println ("Error: Incorrect IP!");
                }
                response = false;
                return false;
            }
            if ( !WiFiConfig::isValidIP(GW.c_str())) {
                if(espresponse) {
                    espresponse->println ("Error: Incorrect Gateway!");
                }
                response = false;
                return false;
            }
            if ( !WiFiConfig::isValidIP(MSK.c_str())) {
                if(espresponse) {
                    espresponse->println ("Error: Incorrect Mask!");
                }
                response = false;
                return false;
            }
            Preferences prefs;
            prefs.begin(NAMESPACE, false);
            if ((prefs.putInt(STA_IP_ENTRY, WiFiConfig::IP_int_from_string(IP)) == 0) ||
                    (prefs.putInt(STA_GW_ENTRY, WiFiConfig::IP_int_from_string(GW)) == 0) ||
                    (prefs.putInt(STA_MK_ENTRY, WiFiConfig::IP_int_from_string(MSK)) == 0)) {
                response = false;
                if(espresponse) {
                    espresponse->println ("Error: Set failed!");
                }
            } else if(espresponse) {
                espresponse->println ("ok");
            }
            prefs.end();
        }
    }
    break;
    //Change AP SSID
    //[ESP105]<SSID>
    case 105: {
#ifdef ENABLE_AUTHENTICATION
        if (auth_type == LEVEL_GUEST) {
            if(espresponse) {
                espresponse->println ("Error: Wrong authentication!");
            }
            return false;
        }
#endif
        parameter = get_param (cmd_params, "", true);
        if ((parameter.length() == 0) && !espresponse) {
            return false;
        }
        //get
        if (parameter.length() == 0) {
            Preferences prefs;
            prefs.begin(NAMESPACE, true);
            String defV = DEFAULT_AP_SSID;
            espresponse->println(prefs.getString(AP_SSID_ENTRY, defV).c_str());
            prefs.end();
        } else { //set
#ifdef ENABLE_AUTHENTICATION
            if (auth_type != LEVEL_ADMIN) {
                if (espresponse) {
                    espresponse->println ("Error: Wrong authentication!");
                }
                return false;
            }
#endif
            if (!WiFiConfig::isSSIDValid (parameter.c_str() ) ) {
                if(espresponse) {
                    espresponse->println ("Error: Incorrect SSID!");
                }
                response = false;
            }
            Preferences prefs;
            prefs.begin(NAMESPACE, false);
            if (prefs.putString(AP_SSID_ENTRY, parameter) == 0) {
                response = false;
                if(espresponse) {
                    espresponse->println ("Error: Set failed!");
                }
            } else if(espresponse) {
                espresponse->println ("ok");
            }
            prefs.end();
        }
    }
    break;
    //Change AP Password
    //[ESP106]<Password>
    case 106: {
#ifdef ENABLE_AUTHENTICATION
        if (auth_type != LEVEL_ADMIN) {
            if(espresponse) {
                espresponse->println ("Error: Wrong authentication!");
            }
            return false;
        }
#endif
        parameter = get_param (cmd_params, "", true);
        if (!WiFiConfig::isPasswordValid (parameter.c_str() ) ) {
            if(espresponse) {
                espresponse->println ("Error: Incorrect password!");
            }
            response = false;
            return false;
        }
        Preferences prefs;
        prefs.begin(NAMESPACE, false);
        if (prefs.putString(AP_PWD_ENTRY, parameter) != parameter.length()) {
            response = false;
            if(espresponse) {
                espresponse->println ("Error: Set failed!");
            }
        } else if(espresponse) {
            espresponse->println ("ok");
        }
        prefs.end();

    }
    break;
    //Change AP IP
    //[ESP107]<IP>
    case 107: {
#ifdef ENABLE_AUTHENTICATION
        if (auth_type == LEVEL_GUEST) {
            if(espresponse) {
                espresponse->println ("Error: Wrong authentication!");
            }
            return false;
        }
#endif
        parameter = get_param (cmd_params, "", true);
        if ((parameter.length() == 0) && !espresponse) {
            return false;
        }
        //get
        if (parameter.length() == 0) {
            Preferences prefs;
            prefs.begin(NAMESPACE, true);
            //IP
            String defV = DEFAULT_AP_IP;
            int32_t IP = prefs.getInt(AP_IP_ENTRY, WiFiConfig::IP_int_from_string(defV));
            espresponse->println(WiFiConfig::IP_string_from_int(IP).c_str());
            prefs.end();
        } else { //set
#ifdef ENABLE_AUTHENTICATION
            if (auth_type != LEVEL_ADMIN) {
                if (espresponse) {
                    espresponse->println ("Error: Wrong authentication!");
                }
                return false;
            }
#endif
            if ( !WiFiConfig::isValidIP(parameter.c_str())) {
                if(espresponse) {
                    espresponse->println ("Error: Incorrect IP!");
                }
                response = false;
                return false;
            }
            Preferences prefs;
            prefs.begin(NAMESPACE, false);
            if (prefs.putInt(AP_IP_ENTRY, WiFiConfig::IP_int_from_string(parameter)) == 0) {
                response = false;
                if(espresponse) {
                    espresponse->println ("Error: Set failed!");
                }
            } else if(espresponse) {
                espresponse->println ("ok");
            }
            prefs.end();
        }
    }
    break;
    //Change AP channel
    //[ESP108]<channel>
    case 108: {
#ifdef ENABLE_AUTHENTICATION
        if (auth_type == LEVEL_GUEST) {
            if(espresponse) {
                espresponse->println ("Error: Wrong authentication!");
            }
            return false;
        }
#endif
        parameter = get_param (cmd_params, "", true);
        if ((parameter.length() == 0) && !espresponse) {
            return false;
        }
        //get
        if (parameter.length() == 0) {
            Preferences prefs;
            prefs.begin(NAMESPACE, true);
            int8_t channel = prefs.getChar(AP_CHANNEL_ENTRY, DEFAULT_AP_CHANNEL);
            espresponse->println(String(channel).c_str());
            prefs.end();
        } else { //set
#ifdef ENABLE_AUTHENTICATION
            if (auth_type != LEVEL_ADMIN) {
                if (espresponse) {
                    espresponse->println ("Error: Wrong authentication!");
                }
                return false;
            }
#endif
            int8_t bbuf = parameter.toInt();
            if ((bbuf > MAX_CHANNEL) || (bbuf < MIN_CHANNEL)) {
                if(espresponse) {
                    espresponse->println ("Error: Incorrect channel!");
                }
                response = false;
                return false;
            }
            Preferences prefs;
            prefs.begin(NAMESPACE, false);
            if (prefs.putChar(AP_CHANNEL_ENTRY, bbuf) == 0) {
                response = false;
                if(espresponse) {
                    espresponse->println ("Error: Set failed!");
                }
            } else if(espresponse) {
                espresponse->println ("ok");
            }
            prefs.end();
        }
    }
    break;
    //Set radio state at boot which can be  STA, AP, OFF
    //[ESP110]<state>
    case 110: {
#ifdef ENABLE_AUTHENTICATION
        if (auth_type == LEVEL_GUEST) {
            if(espresponse) {
                espresponse->println ("Error: Wrong authentication!");
            }
            return false;
        }
#endif
        parameter = get_param (cmd_params, "", true);
        if ((parameter.length() == 0) && !espresponse) {
            return false;
        }
        //get
        if (parameter.length() == 0) {
            Preferences prefs;
            prefs.begin(NAMESPACE, true);
            int8_t wifiMode = prefs.getChar(ESP_RADIO_MODE, DEFAULT_RADIO_MODE);
            if (wifiMode == ESP_RADIO_OFF) {
                espresponse->println("OFF");
            } else if (wifiMode == ESP_WIFI_AP) {
                espresponse->println("AP");
            } else if (wifiMode == ESP_WIFI_STA) {
                espresponse->println("STA");
            } else {
                espresponse->println("??");
            }
            prefs.end();
        } else { //set
#ifdef ENABLE_AUTHENTICATION
            if (auth_type != LEVEL_ADMIN) {
                if (espresponse) {
                    espresponse->println ("Error: Wrong authentication!");
                }
                return false;
            }
#endif
            parameter.toUpperCase();
            if (!(
                        (parameter == "STA") || (parameter == "AP") ||
                        (parameter == "OFF"))) {

                if(espresponse)espresponse->println ("Error: only "
                                                         "STA or AP or "
                                                         "OFF mode supported!");
                response = false;
                return false;
            } else {
                Preferences prefs;
                prefs.begin(NAMESPACE, false);
                int8_t bbuf = ESP_RADIO_OFF;

                if(parameter == "STA") {
                    bbuf = ESP_WIFI_STA;
                }
                if(parameter == "AP") {
                    bbuf = ESP_WIFI_AP;
                }
                if (prefs.putChar(ESP_RADIO_MODE, bbuf) == 0) {
                    response = false;
                    if(espresponse) {
                        espresponse->println ("Error: Set failed!");
                    }
                } else if(espresponse) {
                    espresponse->println ("ok");
                }
                prefs.end();
            }
        }
    }
    break;
    //Get current IP
    //[ESP111]
    case 111: {
        if (!espresponse) {
            return false;
        }
        String currentIP =  WiFiConfig::currentIP();
        espresponse->println (currentIP.c_str());
    }
    break;

    //Get/Set hostname
    //[ESP112]<Hostname>
    case 112: {
#ifdef ENABLE_AUTHENTICATION
        if (auth_type == LEVEL_GUEST) {
            if (espresponse) {
                espresponse->println ("Error: Wrong authentication!");
            }
            return false;
        }
#endif
        parameter = get_param (cmd_params, "", true);
        if ((parameter.length() == 0) && !espresponse) {
            return false;
        }
        //Get hostname
        if (parameter.length() == 0) {
            Preferences prefs;
            prefs.begin(NAMESPACE, true);
            String defV = DEFAULT_HOSTNAME;
            espresponse->println(prefs.getString(HOSTNAME_ENTRY, defV).c_str());
            prefs.end();
        } else { //set host name
#ifdef ENABLE_AUTHENTICATION
            if (auth_type != LEVEL_ADMIN) {
                if (espresponse) {
                    espresponse->println ("Error: Wrong authentication!");
                }
                return false;
            }
#endif
            if (!WiFiConfig::isHostnameValid (parameter.c_str() ) ) {
                if(espresponse) {
                    espresponse->println ("Error: Incorrect hostname!");
                }
                response = false;
            } else {
                Preferences prefs;
                prefs.begin(NAMESPACE, false);
                if (prefs.putString(HOSTNAME_ENTRY, parameter) == 0) {
                    response = false;
                    if(espresponse) {
                        espresponse->println ("Error: Set failed!");
                    }
                } else if(espresponse) {
                    espresponse->println ("ok");
                }
                prefs.end();
            }
        }
    }
    break;
    //Set HTTP state which can be ON, OFF
    //[ESP120]<state>pwd=<admin password>
    case 120: {
#ifdef ENABLE_AUTHENTICATION
        if (auth_type == LEVEL_GUEST) {
            if(espresponse) {
                espresponse->println ("Error: Wrong authentication!");
            }
            return false;
        }
#endif
        parameter = get_param (cmd_params, "", true);
        if ((parameter.length() == 0) && !espresponse) {
            return false;
        }
        //get
        if (parameter.length() == 0) {
            Preferences prefs;
            prefs.begin(NAMESPACE, true);
            int8_t Mode = prefs.getChar(HTTP_ENABLE_ENTRY, DEFAULT_HTTP_STATE);
            espresponse->println((Mode == 0)?"OFF":"ON");
            prefs.end();
        } else { //set
#ifdef ENABLE_AUTHENTICATION
            if (auth_type != LEVEL_ADMIN) {
                if (espresponse) {
                    espresponse->println ("Error: Wrong authentication!");
                }
                return false;
            }
#endif
            parameter.toUpperCase();
            if (!((parameter == "ON") || (parameter == "OFF"))) {
                if(espresponse) {
                    espresponse->println ("Error: only ON or OFF mode supported!");
                }
                response = false;
                return false;
            } else {
                Preferences prefs;
                prefs.begin(NAMESPACE, false);
                int8_t bbuf = (parameter == "ON")?1:0;
                if (prefs.putChar(HTTP_ENABLE_ENTRY, bbuf) == 0) {
                    response = false;
                    if(espresponse) {
                        espresponse->println ("Error: Set failed!");
                    }
                } else if(espresponse) {
                    espresponse->println ("ok");
                }
                prefs.end();
            }
        }
    }
    break;
    //Set HTTP port
    //[ESP121]<port>pwd=<admin password>
    case 121: {
#ifdef ENABLE_AUTHENTICATION
        if (auth_type == LEVEL_GUEST) {
            if(espresponse) {
                espresponse->println ("Error: Wrong authentication!");
            }
            return false;
        }
#endif
        parameter = get_param (cmd_params, "", true);
        if ((parameter.length() == 0) && !espresponse) {
            return false;
        }
        //get
        if (parameter.length() == 0) {
            Preferences prefs;
            prefs.begin(NAMESPACE, true);
            int port = prefs.getUShort(HTTP_PORT_ENTRY, DEFAULT_WEBSERVER_PORT);
            espresponse->println(String(port).c_str());
            prefs.end();
        } else { //set
#ifdef ENABLE_AUTHENTICATION
            if (auth_type != LEVEL_ADMIN) {
                if (espresponse) {
                    espresponse->println ("Error: Wrong authentication!");
                }
                return false;
            }
#endif
            int ibuf = parameter.toInt();
            if ((ibuf > MAX_HTTP_PORT) || (ibuf < MIN_HTTP_PORT)) {
                if(espresponse) {
                    espresponse->println ("Error: Incorrect port!");
                }
                response = false;
                return false;
            }
            Preferences prefs;
            prefs.begin(NAMESPACE, false);

            if (prefs.putUShort(HTTP_PORT_ENTRY, ibuf) == 0) {
                response = false;
                if(espresponse) {
                    espresponse->println ("Error: Set failed!");
                }
            } else if(espresponse) {
                espresponse->println ("ok");
            }
            prefs.end();
        }

    }
    break;
    //Get SD Card Status
    //[ESP200]
    case 200: {
        if (!espresponse) {
            return false;
        }
        String resp = "No SD card";
#if defined(SDSUPPORT)
        ESP_SD card;
        int8_t state = card.card_status();
        if (state == -1) {
            resp="Busy";
        } else if (state == 1) {
            resp="SD card detected";
        } else {
            resp="No SD card";
        }
#endif
        espresponse->println (resp.c_str());
    }
    break;
    //Get full ESP3D settings
    //[ESP400]
    case 400: {
        String v;
        String defV;
        Preferences prefs;
        if (!espresponse) {
            return false;
        }
#ifdef AUTHENTICATION_FEATURE
        if (auth_type == LEVEL_GUEST) {
            return false;
        }
#endif
        int8_t vi;
        espresponse->print("{\"EEPROM\":[");
        prefs.begin(NAMESPACE, true);
        //1 - Hostname
        defV = DEFAULT_HOSTNAME;
        v = prefs.getString(HOSTNAME_ENTRY, defV);
        espresponse->print ("{\"F\":\"network\",\"P\":\"");
        espresponse->print (HOSTNAME_ENTRY);
        espresponse->print ("\",\"T\":\"S\",\"V\":\"");
        espresponse->print (v.c_str());
        espresponse->print ("\",\"H\":\"Hostname\" ,\"S\":\"");
        espresponse->print (String(MAX_HOSTNAME_LENGTH).c_str());
        espresponse->print ("\", \"M\":\"");
        espresponse->print (String(MIN_HOSTNAME_LENGTH).c_str());
        espresponse->print ("\"}");
        espresponse->print (",");

        //2 - http protocol mode
        espresponse->print ("{\"F\":\"network\",\"P\":\"");
        espresponse->print (HTTP_ENABLE_ENTRY);
        espresponse->print ("\",\"T\":\"B\",\"V\":\"");
        vi = prefs.getChar(HTTP_ENABLE_ENTRY, 1);
        espresponse->print (String(vi).c_str());
        espresponse->print ("\",\"H\":\"HTTP protocol\",\"O\":[{\"Enabled\":\"1\"},{\"Disabled\":\"0\"}]}");
        espresponse->print (",");

        //3 - http port
        espresponse->print ("{\"F\":\"network\",\"P\":\"");
        espresponse->print (HTTP_PORT_ENTRY);
        espresponse->print ("\",\"T\":\"I\",\"V\":\"");
        espresponse->print (String(prefs.getUShort(HTTP_PORT_ENTRY, DEFAULT_WEBSERVER_PORT)).c_str());
        espresponse->print ("\",\"H\":\"HTTP Port\",\"S\":\"");
        espresponse->print (String(MAX_HTTP_PORT).c_str());
        espresponse->print ("\",\"M\":\"");
        espresponse->print (String(MIN_HTTP_PORT).c_str());
        espresponse->print ("\"}");
        espresponse->print (",");

        //TODO
        //4 - telnet protocol mode
        /*      espresponse->print ("{\"F\":\"network\",\"P\":\"");
              espresponse->print (TELNET_ENABLE_ENTRY);
              espresponse->print ("\",\"T\":\"B\",\"V\":\"");
              vi = prefs.getChar(TELNET_ENABLE_ENTRY, 0);
              espresponse->print (String(vi).c_str());
              espresponse->print ("\",\"H\":\"Telnet protocol\",\"O\":[{\"Enabled\":\"1\"},{\"Disabled\":\"0\"}]}");
              espresponse->print (",");*/

        //5 - telnet Port
        /*    espresponse->print ("{\"F\":\"network\",\"P\":\"");
            espresponse->print (TELNET_PORT_ENTRY);
            espresponse->print ("\",\"T\":\"I\",\"V\":\"");
            espresponse->print (String(_data_port).c_str());
            espresponse->print ("\",\"H\":\"Telnet Port\",\"S\":\"");
            espresponse->print (String(MAX_TELNET_PORT).c_str());
            espresponse->print ("\",\"M\":\"");
            espresponse->print (String(MIN_TELNET_PORT).c_str());
            espresponse->print ("\"}");
            espresponse->print (",");*/

        //6 - wifi mode
        espresponse->print ("{\"F\":\"network\",\"P\":\"");
        espresponse->print (ESP_RADIO_MODE);
        espresponse->print ("\",\"T\":\"B\",\"V\":\"");
        vi = prefs.getChar(ESP_RADIO_MODE, DEFAULT_RADIO_MODE);
        espresponse->print (String(vi).c_str());
        espresponse->print ("\",\"H\":\"Wifi mode\",\"O\":[{\"STA\":\"1\"},{\"AP\":\"2\"},{\"None\":\"0\"}]}");
        espresponse->print (",");

        //7 - STA SSID
        espresponse->print ("{\"F\":\"network\",\"P\":\"");
        espresponse->print (STA_SSID_ENTRY);
        espresponse->print ("\",\"T\":\"S\",\"V\":\"");
        defV = DEFAULT_STA_SSID;
        espresponse->print (prefs.getString(STA_SSID_ENTRY, defV).c_str());
        espresponse->print ("\",\"S\":\"");
        espresponse->print (String(MAX_SSID_LENGTH).c_str());
        espresponse->print ("\",\"H\":\"Station SSID\",\"M\":\"");
        espresponse->print (String(MIN_SSID_LENGTH).c_str());
        espresponse->print ("\"}");
        espresponse->print (",");

        //8 - STA password
        espresponse->print ("{\"F\":\"network\",\"P\":\"");
        espresponse->print (STA_PWD_ENTRY);
        espresponse->print ("\",\"T\":\"S\",\"V\":\"");
        espresponse->print (HIDDEN_PASSWORD);
        espresponse->print ("\",\"S\":\"");
        espresponse->print (String(MAX_PASSWORD_LENGTH).c_str());
        espresponse->print ("\",\"H\":\"Station Password\",\"M\":\"");
        espresponse->print (String(MIN_PASSWORD_LENGTH).c_str());
        espresponse->print ("\"}");
        espresponse->print (",");

        // 9 - STA IP mode
        espresponse->print ("{\"F\":\"network\",\"P\":\"");
        espresponse->print (STA_IP_MODE_ENTRY);
        espresponse->print ("\",\"T\":\"B\",\"V\":\"");
        espresponse->print (String(prefs.getChar(STA_IP_MODE_ENTRY, DEFAULT_STA_IP_MODE)).c_str());
        espresponse->print ("\",\"H\":\"Station IP Mode\",\"O\":[{\"DHCP\":\"0\"},{\"Static\":\"1\"}]}");
        espresponse->print (",");

        //10-STA static IP
        espresponse->print ("{\"F\":\"network\",\"P\":\"");
        espresponse->print (STA_IP_ENTRY);
        espresponse->print ("\",\"T\":\"A\",\"V\":\"");
        espresponse->print (WiFiConfig::IP_string_from_int(prefs.getInt(STA_IP_ENTRY, 0)).c_str());
        espresponse->print ("\",\"H\":\"Station Static IP\"}");
        espresponse->print (",");

        //11-STA static Gateway
        espresponse->print ("{\"F\":\"network\",\"P\":\"");
        espresponse->print (STA_GW_ENTRY);
        espresponse->print ("\",\"T\":\"A\",\"V\":\"");
        espresponse->print (WiFiConfig::IP_string_from_int(prefs.getInt(STA_GW_ENTRY, 0)).c_str());
        espresponse->print ("\",\"H\":\"Station Static Gateway\"}");
        espresponse->print (",");

        //12-STA static Mask
        espresponse->print ("{\"F\":\"network\",\"P\":\"");
        espresponse->print (STA_MK_ENTRY);
        espresponse->print ("\",\"T\":\"A\",\"V\":\"");
        espresponse->print (WiFiConfig::IP_string_from_int(prefs.getInt(STA_MK_ENTRY, 0)).c_str());
        espresponse->print ("\",\"H\":\"Station Static Mask\"}");
        espresponse->print (",");

        //13 - AP SSID
        espresponse->print ("{\"F\":\"network\",\"P\":\"");
        espresponse->print (AP_SSID_ENTRY);
        espresponse->print ("\",\"T\":\"S\",\"V\":\"");
        defV = DEFAULT_AP_SSID;
        espresponse->print (prefs.getString(AP_SSID_ENTRY, defV).c_str());
        espresponse->print ("\",\"S\":\"");
        espresponse->print (String(MAX_SSID_LENGTH).c_str());
        espresponse->print ("\",\"H\":\"AP SSID\",\"M\":\"");
        espresponse->print (String(MIN_SSID_LENGTH).c_str());
        espresponse->print ("\"}");
        espresponse->print (",");

        //14 - AP password
        espresponse->print ("{\"F\":\"network\",\"P\":\"");
        espresponse->print (AP_PWD_ENTRY);
        espresponse->print ("\",\"T\":\"S\",\"V\":\"");
        espresponse->print (HIDDEN_PASSWORD);
        espresponse->print ("\",\"S\":\"");
        espresponse->print (String(MAX_PASSWORD_LENGTH).c_str());
        espresponse->print ("\",\"H\":\"AP Password\",\"M\":\"");
        espresponse->print (String(MIN_PASSWORD_LENGTH).c_str());
        espresponse->print ("\"}");
        espresponse->print (",");

        //15 - AP static IP
        espresponse->print ("{\"F\":\"network\",\"P\":\"");
        espresponse->print (AP_IP_ENTRY);
        espresponse->print ("\",\"T\":\"A\",\"V\":\"");
        defV = DEFAULT_AP_IP;
        espresponse->print (WiFiConfig::IP_string_from_int(prefs.getInt(AP_IP_ENTRY, WiFiConfig::IP_int_from_string(defV))).c_str());
        espresponse->print ("\",\"H\":\"AP Static IP\"}");
        espresponse->print (",");

        //16 - AP Channel
        espresponse->print ("{\"F\":\"network\",\"P\":\"");
        espresponse->print (AP_CHANNEL_ENTRY);
        espresponse->print ("\",\"T\":\"B\",\"V\":\"");
        espresponse->print (String(prefs.getChar(AP_CHANNEL_ENTRY, DEFAULT_AP_CHANNEL)).c_str());
        espresponse->print ("\",\"H\":\"AP Channel\",\"O\":[");
        for (int i = MIN_CHANNEL; i <= MAX_CHANNEL ; i++) {
            espresponse->print ("{\"");
            espresponse->print (String(i).c_str());
            espresponse->print ("\":\"");
            espresponse->print (String(i).c_str());
            espresponse->print ("\"}");
            if (i < MAX_CHANNEL) {
                espresponse->print (",");
            }
        }
        espresponse->print ("]}");

        espresponse->println ("]}");
        prefs.end();
    }
    break;
    //Set EEPROM setting
    //[ESP401]P=<position> T=<type> V=<value> pwd=<user/admin password>
    case 401: {
#ifdef AUTHENTICATION_FEATURE
        if (auth_type != LEVEL_ADMIN) {
            return false;
        }
#endif
        //check validity of parameters
        String spos = get_param (cmd_params, "P=", false);
        String styp = get_param (cmd_params, "T=", false);
        String sval = get_param (cmd_params, "V=", true);
        spos.trim();
        sval.trim();
        if (spos.length() == 0) {
            response = false;
        }
        if (! (styp == "B" || styp == "S" || styp == "A" || styp == "I" || styp == "F") ) {
            response = false;
        }
        if ((sval.length() == 0) && !((spos==AP_PWD_ENTRY) || (spos==STA_PWD_ENTRY))) {
            response = false;
        }

        if (response) {
            Preferences prefs;
            prefs.begin(NAMESPACE, false);
            //Byte value
            if ((styp == "B")  ||  (styp == "F")) {
                int8_t bbuf = sval.toInt();
                if (prefs.putChar(spos.c_str(), bbuf) ==0 ) {
                    response = false;
                } else {
                    //dynamique refresh is better than restart the board
                    if (spos == ESP_RADIO_MODE) {
                        //TODO
                    }
                    if (spos == AP_CHANNEL_ENTRY) {
                        //TODO
                    }
                    if (spos == HTTP_ENABLE_ENTRY) {
                        //TODO
                    }
                    if (spos == TELNET_ENABLE_ENTRY) {
                        //TODO
                    }
                }
            }
            //Integer value
            if (styp == "I") {
                int16_t ibuf = sval.toInt();
                if (prefs.putUShort(spos.c_str(), ibuf) == 0) {
                    response = false;
                } else {
                    if (spos == HTTP_PORT_ENTRY) {
                        //TODO
                    }
                    if (spos == TELNET_PORT_ENTRY) {
                        //TODO
                        //Serial.println(ibuf);
                    }
                }

            }
            //String value
            if (styp == "S") {
                if (prefs.putString(spos.c_str(), sval) == 0) {
                    response = false;
                } else {
                    if (spos == HOSTNAME_ENTRY) {
                        //TODO
                    }
                    if (spos == STA_SSID_ENTRY) {
                        //TODO
                    }
                    if (spos == STA_PWD_ENTRY) {
                        //TODO
                    }
                    if (spos == AP_SSID_ENTRY) {
                        //TODO
                    }
                    if (spos == AP_PWD_ENTRY) {
                        //TODO
                    }
                }

            }
            //IP address
            if (styp == "A") {
                if (prefs.putInt(spos.c_str(), WiFiConfig::IP_int_from_string(sval)) == 0) {
                    response = false;
                } else {
                    if (spos == STA_IP_ENTRY) {
                        //TODO
                    }
                    if (spos == STA_GW_ENTRY) {
                        //TODO
                    }
                    if (spos == STA_MK_ENTRY) {
                        //TODO
                    }
                    if (spos == AP_IP_ENTRY) {
                        //TODO
                    }
                }
            }
            prefs.end();
        }
        if (!response) {
            if (espresponse) {
                espresponse->println ("Error: Incorrect Command");
            }
        } else {
            if (espresponse) {
                espresponse->println ("ok");
            }
        }

    }
    break;
    //Get available AP list (limited to 30)
    //output is JSON
    //[ESP410]
    case 410: {
        if (!espresponse) {
            return false;
        }
#ifdef AUTHENTICATION_FEATURE
        if (auth_type == LEVEL_GUEST) {
            return false;
        }
#endif
        espresponse->print("{\"AP_LIST\":[");
        int n = WiFi.scanNetworks();
        if (n > 0) {
            for (int i = 0; i < n; ++i) {
                if (i > 0) {
                    espresponse->print (",");
                }
                espresponse->print ("{\"SSID\":\"");
                espresponse->print (WiFi.SSID (i).c_str());
                espresponse->print ("\",\"SIGNAL\":\"");
                espresponse->print (String(WiFiConfig::getSignal (WiFi.RSSI (i) )).c_str());
                espresponse->print ("\",\"IS_PROTECTED\":\"");

                if (WiFi.encryptionType (i) == WIFI_AUTH_OPEN) {
                    espresponse->print ("0");
                } else {
                    espresponse->print ("1");
                }
                espresponse->print ("\"}");
                Esp3DLibConfig::wait(0);
            }
            WiFi.scanDelete();
        }
        espresponse->println ("]}");
        //Ugly fix for the AP mode
        if (WiFi.getMode() == WIFI_AP_STA) {
            WiFi.enableSTA (false);
        }
    }
    break;
    //Get ESP current status
    case 420: {
#ifdef AUTHENTICATION_FEATURE
        if (auth_type == LEVEL_GUEST) {
            return false;
        }
#endif
        if (!espresponse) {
            return false;
        }
        Preferences prefs;
        prefs.begin(NAMESPACE, true);
        espresponse->print ("Chip ID: ");
        espresponse->print (String ( (uint16_t) (ESP.getEfuseMac() >> 32) ).c_str());
        espresponse->print ("\n");
        espresponse->print ("CPU Frequency: ");
        espresponse->print (String (ESP.getCpuFreqMHz() ).c_str());
        espresponse->print ("Mhz");
        espresponse->print ("\n");
        espresponse->print ("CPU Temperature: ");
        espresponse->print (String (temperatureRead(), 1).c_str());
        espresponse->print ("C");
        espresponse->print ("\n");
        espresponse->print ("Free memory: ");
        espresponse->print (ESPResponseStream::formatBytes (ESP.getFreeHeap()).c_str());
        espresponse->print ("\n");
        espresponse->print ("SDK: ");
        espresponse->print (ESP.getSdkVersion());
        espresponse->print ("\n");
        espresponse->print ("Flash Size: ");
        espresponse->print (ESPResponseStream::formatBytes (ESP.getFlashChipSize()).c_str());
        espresponse->print ("\n");
        espresponse->print ("Available Size for update: ");
        size_t  flashsize = 0;
        const esp_partition_t* mainpartition = esp_ota_get_running_partition();
        if (mainpartition) {
            const esp_partition_t* partition = esp_ota_get_next_update_partition(mainpartition);
            if (partition) {
                const esp_partition_t* partition2 = esp_ota_get_next_update_partition(partition);
                if (partition2 && (partition->address!=partition2->address)) {
                    flashsize = partition2->size;
                }
            }
        }
        espresponse->print (ESPResponseStream::formatBytes (flashsize).c_str());
        espresponse->print ("\n");
        espresponse->print ("Available Size for SPIFFS: ");
        espresponse->print (ESPResponseStream::formatBytes (SPIFFS.totalBytes()).c_str());
        espresponse->print ("\n");
        espresponse->print ("Baud rate: ");
        espresponse->print (String(ESPResponseStream::baudRate()).c_str());
        espresponse->print ("\n");
        espresponse->print ("Sleep mode: ");
        if (WiFi.getSleep()) {
            espresponse->print ("Modem");
        } else {
            espresponse->print ("None");
        }
        espresponse->print ("\n");
        espresponse->print ("Web port: ");
        espresponse->print (String( prefs.getUShort(HTTP_PORT_ENTRY, DEFAULT_WEBSERVER_PORT)).c_str());
        espresponse->print ("\n");
        /*espresponse->print ("Data port: ");
        //if (_data_port!=0)espresponse->print (String(_data_port).c_str());
        //else
        espresponse->print ("Disabled");
        espresponse->print ("\n");*/
        espresponse->print ("Hostname: ");
        String defV = DEFAULT_HOSTNAME;
        espresponse->print ( prefs.getString(HOSTNAME_ENTRY, defV).c_str());
        espresponse->print ("\n");
        espresponse->print ("Active Mode: ");
        if (WiFi.getMode() == WIFI_STA) {
            espresponse->print ("STA (");
            espresponse->print ( WiFi.macAddress().c_str());
            espresponse->print (")");
            espresponse->print ("\n");
            espresponse->print ("Connected to: ");
            if (WiFi.isConnected()) { //in theory no need but ...
                espresponse->print (WiFi.SSID().c_str());
                espresponse->print ("\n");
                espresponse->print ("Signal: ");
                espresponse->print ( String(WiFiConfig::getSignal (WiFi.RSSI())).c_str());
                espresponse->print ("%");
                espresponse->print ("\n");
                uint8_t PhyMode;
                esp_wifi_get_protocol (ESP_IF_WIFI_STA, &PhyMode);
                espresponse->print ("Phy Mode: ");
                if (PhyMode == (WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N)) {
                    espresponse->print ("11n");
                } else if (PhyMode == (WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G)) {
                    espresponse->print ("11g");
                } else if (PhyMode == (WIFI_PROTOCOL_11B )) {
                    espresponse->print ("11b");
                } else {
                    espresponse->print ("???");
                }
                espresponse->print ("\n");
                espresponse->print ("Channel: ");
                espresponse->print (String (WiFi.channel()).c_str());
                espresponse->print ("\n");
                espresponse->print ("IP Mode: ");
                tcpip_adapter_dhcp_status_t dhcp_status;
                tcpip_adapter_dhcpc_get_status (TCPIP_ADAPTER_IF_STA, &dhcp_status);
                if (dhcp_status == TCPIP_ADAPTER_DHCP_STARTED) {
                    espresponse->print ("DHCP");
                } else {
                    espresponse->print ("Static");
                }
                espresponse->print ("\n");
                espresponse->print ("IP: ");
                espresponse->print (WiFi.localIP().toString().c_str());
                espresponse->print ("\n");
                espresponse->print ("Gateway: ");
                espresponse->print (WiFi.gatewayIP().toString().c_str());
                espresponse->print ("\n");
                espresponse->print ("Mask: ");
                espresponse->print (WiFi.subnetMask().toString().c_str());
                espresponse->print ("\n");
                espresponse->print ("DNS: ");
                espresponse->print (WiFi.dnsIP().toString().c_str());
                espresponse->print ("\n");
            } //this is web command so connection => no command
            espresponse->print ("Disabled Mode: ");
            espresponse->print ("AP (");
            espresponse->print (WiFi.softAPmacAddress().c_str());
            espresponse->print (")");
            espresponse->print ("\n");
        } else if (WiFi.getMode() == WIFI_AP) {
            espresponse->print ("AP (");
            espresponse->print (WiFi.softAPmacAddress().c_str());
            espresponse->print (")");
            espresponse->print ("\n");
            wifi_config_t conf;
            esp_wifi_get_config (ESP_IF_WIFI_AP, &conf);
            espresponse->print ("SSID: ");
            espresponse->print ((const char*) conf.ap.ssid);
            espresponse->print ("\n");
            espresponse->print ("Visible: ");
            espresponse->print ( (conf.ap.ssid_hidden == 0) ? "Yes" : "No");
            espresponse->print ("\n");
            espresponse->print ("Authentication: ");
            if (conf.ap.authmode == WIFI_AUTH_OPEN) {
                espresponse->print ("None");
            } else if (conf.ap.authmode == WIFI_AUTH_WEP) {
                espresponse->print ("WEP");
            } else if (conf.ap.authmode == WIFI_AUTH_WPA_PSK) {
                espresponse->print ("WPA");
            } else if (conf.ap.authmode == WIFI_AUTH_WPA2_PSK) {
                espresponse->print ("WPA2");
            } else {
                espresponse->print ("WPA/WPA2");
            }
            espresponse->print ("\n");
            espresponse->print ("Max Connections: ");
            espresponse->print (String(conf.ap.max_connection).c_str());
            espresponse->print ("\n");
            espresponse->print ("DHCP Server: ");
            tcpip_adapter_dhcp_status_t dhcp_status;
            tcpip_adapter_dhcps_get_status (TCPIP_ADAPTER_IF_AP, &dhcp_status);
            if (dhcp_status == TCPIP_ADAPTER_DHCP_STARTED) {
                espresponse->print ("Started");
            } else {
                espresponse->print ("Stopped");
            }
            espresponse->print ("\n");
            espresponse->print ("IP: ");
            espresponse->print (WiFi.softAPIP().toString().c_str());
            espresponse->print ("\n");
            tcpip_adapter_ip_info_t ip_AP;
            tcpip_adapter_get_ip_info (TCPIP_ADAPTER_IF_AP, &ip_AP);
            espresponse->print ("Gateway: ");
            espresponse->print (IPAddress (ip_AP.gw.addr).toString().c_str());
            espresponse->print ("\n");
            espresponse->print ("Mask: ");
            espresponse->print (IPAddress (ip_AP.netmask.addr).toString().c_str());
            espresponse->print ("\n");
            espresponse->print ("Connected clients: ");
            wifi_sta_list_t station;
            tcpip_adapter_sta_list_t tcpip_sta_list;
            esp_wifi_ap_get_sta_list (&station);
            tcpip_adapter_get_sta_list (&station, &tcpip_sta_list);
            espresponse->print (String(station.num).c_str());
            espresponse->print ("\n");
            for (int i = 0; i < station.num; i++) {
                espresponse->print (ESPResponseStream::mac2str(tcpip_sta_list.sta[i].mac));
                espresponse->print (" ");
                espresponse->print ( IPAddress (tcpip_sta_list.sta[i].ip.addr).toString().c_str());
                espresponse->print ("\n");
            }
            espresponse->print ("Disabled Mode: ");
            espresponse->print ("STA (");
            espresponse->print (WiFi.macAddress().c_str());
            espresponse->print (")");
            espresponse->print ("\n");
        } else if (WiFi.getMode() == WIFI_AP_STA) { //we should not be in this state but just in case ....
            espresponse->print ("Mixed");
            espresponse->print ("\n");
            espresponse->print ("STA (");
            espresponse->print (WiFi.macAddress().c_str());
            espresponse->print (")");
            espresponse->print ("\n");
            espresponse->print ("AP (");
            espresponse->print (WiFi.softAPmacAddress().c_str());
            espresponse->print (")");
            espresponse->print ("\n");

        } else { //we should not be there if no wifi ....
            espresponse->print ("Wifi Off");
            espresponse->print ("\n");
        }
        prefs.end();
        //TODO to complete
        espresponse->print ("FW version: Marlin ");
        espresponse->print (SHORT_BUILD_VERSION);
        espresponse->print (" (ESP3D:");
        espresponse->print (LIB_VERSION);
        espresponse->println (")");
    }
    break;
    //Set ESP mode
    //cmd is RESTART, RESET
    //[ESP444]<cmd>
    case 444:
        parameter = get_param(cmd_params,"", true);
#ifdef AUTHENTICATION_FEATURE
        if (auth_type != LEVEL_ADMIN) {
            response = false;
        } else
#endif
        {
            if (parameter=="RESTART") {
                Esp3DCom::echo("Restart ongoing");
                Esp3DLibConfig::restart_ESP();
            } else if (parameter=="RESET") {
                Esp3DCom::echo("Reset");
                Esp3DLibConfig::reset_settings();
            } else {
                response = false;
            }
        }
        if (!response) {
            if (espresponse) {
                espresponse->println ("Error: Incorrect Command, only RESET or RESTART");
            }
        } else {
            if (espresponse) {
                espresponse->println ("ok");
            }
        }
        break;
#ifdef AUTHENTICATION_FEATURE
    //Change / Reset admin password
    //[ESP550]<password>
    case 550: {
        if (auth_type == LEVEL_ADMIN) {
            parameter = get_param (cmd_params, "", true);
            if (parameter.length() == 0) {
                Preferences prefs;
                parameter = DEFAULT_ADMIN_PWD;
                prefs.begin(NAMESPACE, false);
                if (prefs.putString(ADMIN_PWD_ENTRY, parameter) != parameter.length()) {
                    response = false;
                    espresponse->println ("error");
                } else {
                    espresponse->println ("ok");
                }
                prefs.end();

            } else {
                if (isLocalPasswordValid (parameter.c_str() ) ) {
                    Preferences prefs;
                    prefs.begin(NAMESPACE, false);
                    if (prefs.putString(ADMIN_PWD_ENTRY, parameter) != parameter.length()) {
                        response = false;
                        espresponse->println ("error");
                    } else {
                        espresponse->println ("ok");
                    }
                    prefs.end();
                } else {
                    espresponse->println ("error");
                    response = false;
                }
            }
        } else {
            espresponse->println ("error");
            response = false;
        }
        break;
    }
    //Change / Reset user password
    //[ESP555]<password>
    case 555: {
        if (auth_type == LEVEL_ADMIN) {
            parameter = get_param (cmd_params, "", true);
            if (parameter.length() == 0) {
                Preferences prefs;
                parameter = DEFAULT_USER_PWD;
                prefs.begin(NAMESPACE, false);
                if (prefs.putString(USER_PWD_ENTRY, parameter) != parameter.length()) {
                    response = false;
                    espresponse->println ("error");
                } else {
                    espresponse->println ("ok");
                }
                prefs.end();

            } else {
                if (isLocalPasswordValid (parameter.c_str() ) ) {
                    Preferences prefs;
                    prefs.begin(NAMESPACE, false);
                    if (prefs.putString(USER_PWD_ENTRY, parameter) != parameter.length()) {
                        response = false;
                        espresponse->println ("error");
                    } else {
                        espresponse->println ("ok");
                    }
                    prefs.end();
                } else {
                    espresponse->println ("error");
                    response = false;
                }
            }
        } else {
            espresponse->println ("error");
            response = false;
        }
        break;
    }
#endif
    //[ESP700]<filename>
    case 700: { //read local file
#ifdef AUTHENTICATION_FEATURE
        if (auth_type == LEVEL_GUEST) {
            return false;
        }
#endif
        cmd_params.trim() ;
        if ( (cmd_params.length() > 0) && (cmd_params[0] != '/') ) {
            cmd_params = "/" + cmd_params;
        }
        File currentfile = SPIFFS.open (cmd_params, FILE_READ);
        if (currentfile) {//if file open success
            //until no line in file
            while (currentfile.available()) {
                String currentline = currentfile.readStringUntil('\n');
                currentline.replace("\n","");
                currentline.replace("\r","");
                if (currentline.length() > 0) {
                    int ESPpos = currentline.indexOf ("[ESP");
                    if (ESPpos > -1) {
                        //is there the second part?
                        int ESPpos2 = currentline.indexOf ("]", ESPpos);
                        if (ESPpos2 > -1) {
                            //Split in command and parameters
                            String cmd_part1 = currentline.substring (ESPpos + 4, ESPpos2);
                            String cmd_part2 = "";
                            //is there space for parameters?
                            if (ESPpos2 < currentline.length() ) {
                                cmd_part2 = currentline.substring (ESPpos2 + 1);
                            }
                            //if command is a valid number then execute command
                            if(cmd_part1.toInt()!=0) {
                                if (!execute_internal_command(cmd_part1.toInt(),cmd_part2, auth_type, espresponse)) {
                                    response = false;
                                }
                            }
                            //if not is not a valid [ESPXXX] command ignore it
                        }
                    } else {
                        if (currentline.length() > 0) {
                            currentline+="\n";
                            Serial2Socket.push(currentline.c_str());
                            //GCodeQueue::enqueue_one_now(currentline.c_str());
                        }
                        Esp3DLibConfig::wait (1);
                    }
                    Esp3DLibConfig::wait (1);
                }
            }
            currentfile.close();
            if (espresponse) {
                espresponse->println ("ok");
            }
        } else {
            if (espresponse) {
                espresponse->println ("error");
            }
            response = false;
        }
        break;
    }
    //Format SPIFFS
    //[ESP710]FORMAT pwd=<admin password>
    case 710:
#ifdef AUTHENTICATION_FEATURE
        if (auth_type != LEVEL_ADMIN) {
            return false;
        }
#endif
        parameter = get_param (cmd_params, "", true);
#ifdef AUTHENTICATION_FEATURE
        if (auth_type != LEVEL_ADMIN) {
            espresponse->println ("error");
            response = false;
            break;
        } else
#endif
        {
            if (parameter == "FORMAT") {
                if (espresponse) {
                    espresponse->print ("Formating");
                }
                SPIFFS.format();
                if (espresponse) {
                    espresponse->println ("...Done");
                }
            } else {
                if (espresponse) {
                    espresponse->println ("error");
                }
                response = false;
            }
        }
        break;
    //get fw version / fw target / hostname / authentication
    //[ESP800]
    case 800: {
        if (!espresponse) {
            return false;
        }
        Preferences prefs;
        prefs.begin(NAMESPACE, true);
        String resp;
        resp = "FW version:";
        resp += SHORT_BUILD_VERSION;
        resp += "-";
        resp += LIB_VERSION;
        resp += " # FW target:marlin-embedded  # FW HW:";
#if defined(SDSUPPORT)
        resp += "Direct SD";
#else
        resp += "No SD";
#endif
        resp += " # primary sd:/sd # secondary sd:none # authentication:";
#ifdef AUTHENTICATION_FEATURE
        resp += "yes";
#else
        resp += "no";
#endif

#if defined (HTTP_FEATURE)
        resp += " # webcommunication: Sync: ";
        resp += String(prefs.getUShort(HTTP_PORT_ENTRY, DEFAULT_WEBSERVER_PORT) + 1);
        resp += ":";
        if (WiFi.getMode() == WIFI_MODE_AP) {
            resp += WiFi.softAPIP().toString();
        } else if (WiFi.getMode() == WIFI_MODE_STA) {
            resp += WiFi.localIP().toString();
        } else if (WiFi.getMode() == WIFI_MODE_APSTA) {
            resp += WiFi.softAPIP().toString();
        } else {
            resp += "0.0.0.0";
        }
#endif
        resp += "# hostname:";
        String defV = DEFAULT_HOSTNAME;
        resp += prefs.getString(HOSTNAME_ENTRY, defV);;
        prefs.end();
        if (WiFi.getMode() == WIFI_AP) {
            resp += "(AP mode)";
        }
        if (espresponse) {
            espresponse->println (resp.c_str());
        }
    }
    break;
    default:
        if (espresponse) {
            espresponse->println ("Error: Incorrect Command");
        }
        response = false;
        break;
    }
    return response;
}

#endif //ESP3D_WIFISUPPORT

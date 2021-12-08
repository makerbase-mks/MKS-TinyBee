/*
  web_server.cpp -  web server functions class

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
#if defined(ESP3D_WIFISUPPORT) && defined(HTTP_FEATURE)
#include "web_server.h"
#include "espcom.h"
#include "wificonfig.h"
#include "command.h"
//#include MARLIN_PATH(gcode/queue.h)
//#include MARLIN_PATH(inc/Version.h)
#undef DISABLED
#undef _BV
#include "wifiservices.h"
#include "serial2socket.h"
#include "command.h"
#include <WebSocketsServer.h>
#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>
#if defined(SDSUPPORT)
#include "sd_ESP32.h"
#endif
#include <Preferences.h>
#include <WebServer.h>
#include <ESP32SSDP.h>
#include <StreamString.h>
#include <Update.h>
#include <esp_wifi_types.h>
#ifdef MDNS_FEATURE
#include <ESPmDNS.h>
#endif //MDNS_FEATURE
#ifdef SSDP_FEATURE
#include <ESP32SSDP.h>
#endif //SSDP_FEATURE
#ifdef CAPTIVE_PORTAL_FEATURE
#include <DNSServer.h>
const byte DNS_PORT = 53;
DNSServer dnsServer;
#endif //CAPTIVE_PORTAL_FEATURE
#include <esp_ota_ops.h>
//embedded response file if no files on SPIFFS
#include "nofile.h"

//Upload status
typedef enum {
    UPLOAD_STATUS_NONE = 0,
    UPLOAD_STATUS_FAILED = 1,
    UPLOAD_STATUS_CANCELLED = 2,
    UPLOAD_STATUS_SUCCESSFUL = 3,
    UPLOAD_STATUS_ONGOING  = 4
} upload_status_type;

//Default 404
const char PAGE_404 []  = "<HTML>\n<HEAD>\n<title>Redirecting...</title> \n</HEAD>\n<BODY>\n<CENTER>Unknown page : $QUERY$- you will be redirected...\n<BR><BR>\nif not redirected, <a href='http://$WEB_ADDRESS$'>click here</a>\n<BR><BR>\n<PROGRESS name='prg' id='prg'></PROGRESS>\n\n<script>\nvar i = 0; \nvar x = document.getElementById(\"prg\"); \nx.max=5; \nvar interval=setInterval(function(){\ni=i+1; \nvar x = document.getElementById(\"prg\"); \nx.value=i; \nif (i>5) \n{\nclearInterval(interval);\nwindow.location.href='/';\n}\n},1000);\n</script>\n</CENTER>\n</BODY>\n</HTML>\n\n";
const char PAGE_CAPTIVE [] = "<HTML>\n<HEAD>\n<title>Captive Portal</title> \n</HEAD>\n<BODY>\n<CENTER>Captive Portal page : $QUERY$- you will be redirected...\n<BR><BR>\nif not redirected, <a href='http://$WEB_ADDRESS$'>click here</a>\n<BR><BR>\n<PROGRESS name='prg' id='prg'></PROGRESS>\n\n<script>\nvar i = 0; \nvar x = document.getElementById(\"prg\"); \nx.max=5; \nvar interval=setInterval(function(){\ni=i+1; \nvar x = document.getElementById(\"prg\"); \nx.value=i; \nif (i>5) \n{\nclearInterval(interval);\nwindow.location.href='/';\n}\n},1000);\n</script>\n</CENTER>\n</BODY>\n</HTML>\n\n";

//error codes fo upload
#define ESP_ERROR_AUTHENTICATION 1
#define ESP_ERROR_FILE_CREATION  2
#define ESP_ERROR_FILE_WRITE 3
#define ESP_ERROR_UPLOAD 4
#define ESP_ERROR_NOT_ENOUGH_SPACE 5
#define ESP_ERROR_UPLOAD_CANCELLED 6
#define ESP_ERROR_FILE_CLOSE 7
#define ESP_ERROR_NO_SD 8

Web_Server web_server;
bool Web_Server::_setupdone = false;
uint16_t Web_Server::_port = 0;
String Web_Server::_hostname = "";
uint16_t Web_Server::_data_port = 0;
long Web_Server::_id_connection = 0;
uint8_t Web_Server::_upload_status = UPLOAD_STATUS_NONE;
WebServer * Web_Server::_webserver = NULL;
WebSocketsServer * Web_Server::_socket_server = NULL;
#ifdef AUTHENTICATION_FEATURE
auth_ip * Web_Server::_head = NULL;
uint8_t Web_Server::_nb_ip = 0;
#define MAX_AUTH_IP 10
#endif
Web_Server::Web_Server()
{

}
Web_Server::~Web_Server()
{
    end();
}

long Web_Server::get_client_ID()
{
    return  _id_connection;
}

bool Web_Server::begin()
{

    bool no_error = true;
    _setupdone = false;
    Preferences prefs;
    prefs.begin(NAMESPACE, true);
    int8_t penabled = prefs.getChar(HTTP_ENABLE_ENTRY, DEFAULT_HTTP_STATE);
    //Get http port
    _port = prefs.getUShort(HTTP_PORT_ENTRY, DEFAULT_WEBSERVER_PORT);
    //Get hostname
    String defV = DEFAULT_HOSTNAME;
    _hostname = prefs.getString(HOSTNAME_ENTRY, defV);
    prefs.end();
    if (penabled == 0) {
        return false;
    }
    //create instance
    _webserver= new WebServer(_port);
#ifdef AUTHENTICATION_FEATURE
    //here the list of headers to be recorded
    const char * headerkeys[] = {"Cookie"} ;
    size_t headerkeyssize = sizeof (headerkeys) / sizeof (char*);
    //ask server to track these headers
    _webserver->collectHeaders (headerkeys, headerkeyssize );
#endif
    _socket_server = new WebSocketsServer(_port + 1);
    _socket_server->begin();
    _socket_server->onEvent(handle_Websocket_Event);


    //Websocket output
    Serial2Socket.attachWS(_socket_server);

    //Web server handlers
    //trick to catch command line on "/" before file being processed
    _webserver->on("/",HTTP_ANY, handle_root);

    //Page not found handler
    _webserver->onNotFound (handle_not_found);

    //need to be there even no authentication to say to UI no authentication
    _webserver->on("/login", HTTP_ANY, handle_login);

    //web commands
    _webserver->on ("/command", HTTP_ANY, handle_web_command);
    _webserver->on ("/command_silent", HTTP_ANY, handle_web_command_silent);

    //SPIFFS
    _webserver->on ("/files", HTTP_ANY, handleFileList, SPIFFSFileupload);

    //web update
    _webserver->on ("/updatefw", HTTP_ANY, handleUpdate, WebUpdateUpload);

#if defined(SDSUPPORT)
    //Direct SD management
    _webserver->on("/upload", HTTP_ANY, handle_direct_SDFileList,SDFile_direct_upload);
#endif

#ifdef CAPTIVE_PORTAL_FEATURE
    if(WiFi.getMode() == WIFI_AP) {
        // if DNSServer is started with "*" for domain name, it will reply with
        // provided IP to all DNS request
        dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
        _webserver->on ("/generate_204", HTTP_ANY,  handle_root);
        _webserver->on ("/gconnectivitycheck.gstatic.com", HTTP_ANY, handle_root);
        //do not forget the / at the end
        _webserver->on ("/fwlink/", HTTP_ANY, handle_root);
    }
#endif //CAPTIVE_PORTAL_FEATURE

#ifdef SSDP_FEATURE
    //SSDP service presentation
    if(WiFi.getMode() == WIFI_STA) {
        _webserver->on ("/description.xml", HTTP_GET, handle_SSDP);
        //Add specific for SSDP
        SSDP.setSchemaURL ("description.xml");
        SSDP.setHTTPPort (_port);
        SSDP.setName (_hostname);
        SSDP.setURL ("/");
        SSDP.setDeviceType ("upnp:rootdevice");
        /*Any customization could be here
        SSDP.setModelName (ESP32_MODEL_NAME);
        SSDP.setModelURL (ESP32_MODEL_URL);
        SSDP.setModelNumber (ESP_MODEL_NUMBER);
        SSDP.setManufacturer (ESP_MANUFACTURER_NAME);
        SSDP.setManufacturerURL (ESP_MANUFACTURER_URL);
        */

        //Start SSDP
        Esp3DCom::echo("SSDP service started");
        SSDP.begin();
    }
#endif //SSDP_FEATURE
    Esp3DCom::echo("HTTP server started");
    //start webserver
    _webserver->begin();
#ifdef MDNS_FEATURE
    //add mDNS
    if(WiFi.getMode() == WIFI_STA) {
        MDNS.addService("http","tcp",_port);
    }
#endif //MDNS_FEATURE
    _setupdone = true;
    return no_error;
}

void Web_Server::end()
{
    _setupdone = false;
#ifdef SSDP_FEATURE
    SSDP.end();
#endif //SSDP_FEATURE
#ifdef MDNS_FEATURE
    //remove mDNS
    mdns_service_remove("_http", "_tcp");
#endif //MDNS_FEATURE
    if (_socket_server) {
        delete _socket_server;
        _socket_server = NULL;
    }
    if (_webserver) {
        delete _webserver;
        _webserver = NULL;
    }
#ifdef AUTHENTICATION_FEATURE
    while (_head) {
        auth_ip * current = _head;
        _head = _head->_next;
        delete current;
    }
    _nb_ip = 0;
#endif
}

//Root of Webserver/////////////////////////////////////////////////////

void Web_Server::handle_root()
{
    String path = "/index.html";
    String contentType =  getContentType(path);
    String pathWithGz = path + ".gz";
    //if have a index.html or gzip version this is default root page
    if((SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) && !_webserver->hasArg("forcefallback") && _webserver->arg("forcefallback")!="yes") {
        if(SPIFFS.exists(pathWithGz)) {
            path = pathWithGz;
        }
        File file = SPIFFS.open(path, FILE_READ);
        _webserver->streamFile(file, contentType);
        file.close();
        return;
    }
    //if no lets launch the default content
    _webserver->sendHeader("Content-Encoding", "gzip");
    _webserver->send_P(200,"text/html",PAGE_NOFILES,PAGE_NOFILES_SIZE);
}

//Handle not registred path on SPIFFS neither SD ///////////////////////
void Web_Server:: handle_not_found()
{
    if (is_authenticated() == LEVEL_GUEST) {
        _webserver->sendContent_P("HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n");
        //_webserver->client().stop();
        return;
    }
    bool page_not_found = false;
    String path = _webserver->urlDecode(_webserver->uri());
    String contentType =  getContentType(path);
    String pathWithGz = path + ".gz";

#if defined(SDSUPPORT)
    if ((path.substring(0,4) == "/SD/")) {
        //remove /SD
        path = path.substring(3);
        ESP_SD SD_card;
        if (SD_card.card_status() == 1) {
            if (SD_card.exists(pathWithGz.c_str()) || SD_card.exists(path.c_str())) {
                if (!SD_card.exists(path.c_str())) {
                    path =  pathWithGz;
                }
                if(SD_card.open(path.c_str())) {
                    uint8_t buf[1200];
                    _webserver->setContentLength(SD_card.size());
                    _webserver->sendHeader("Cache-Control","no-cache");
                    _webserver->send(200, "application/octet-stream", "");

                    WiFiClient c = _webserver->client();
                    int16_t len = SD_card.read( buf, 1200);
                    while(len > 0) {
                        c.write(buf, len);
                        len = SD_card.read( buf, 1200);
                        Esp3DLibConfig::wait(0);
                        handle();
                    }
                    SD_card.close();
                    return;
                }
            }
        }

        String content = "cannot find ";
        content+=path;
        _webserver->send(404,"text/plain",content.c_str());
        return;
    } else
#endif
        if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
            if(SPIFFS.exists(pathWithGz)) {
                path = pathWithGz;
            }
            File file = SPIFFS.open(path, FILE_READ);
            _webserver->streamFile(file, contentType);
            file.close();
            return;
        } else {
            page_not_found = true;
        }

    if (page_not_found ) {
#ifdef CAPTIVE_PORTAL_FEATURE
        if (WiFi.getMode()!=WIFI_STA ) {
            String content=PAGE_CAPTIVE;
            String stmp = WiFi.softAPIP().toString();
            //Web address = ip + port
            String KEY_IP = "$WEB_ADDRESS$";
            String KEY_QUERY = "$QUERY$";
            if (_port != 80) {
                stmp+=":";
                stmp+=String(_port);
            }
            content.replace(KEY_IP,stmp);
            content.replace(KEY_IP,stmp);
            content.replace(KEY_QUERY,_webserver->uri());
            _webserver->send(200,"text/html",content);
            return;
        }
#endif //CAPTIVE_PORTAL_FEATURE
        path = "/404.htm";
        contentType =  getContentType(path);
        pathWithGz =  path + ".gz";
        if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
            if(SPIFFS.exists(pathWithGz)) {
                path = pathWithGz;
            }
            File file = SPIFFS.open(path, FILE_READ);
            _webserver->streamFile(file, contentType);
            file.close();

        } else {
            //if not template use default page
            contentType = PAGE_404;
            String stmp;
            if (WiFi.getMode()==WIFI_STA ) {
                stmp=WiFi.localIP().toString();
            } else {
                stmp=WiFi.softAPIP().toString();
            }
            //Web address = ip + port
            String KEY_IP = "$WEB_ADDRESS$";
            String KEY_QUERY = "$QUERY$";
            if ( _port != 80) {
                stmp+=":";
                stmp+=String(_port);
            }
            contentType.replace(KEY_IP,stmp);
            contentType.replace(KEY_QUERY,_webserver->uri());
            _webserver->send(200,"text/html",contentType);
        }
    }
}
#ifdef SSDP_FEATURE
//http SSDP xml presentation
void Web_Server::handle_SSDP ()
{
    StreamString sschema ;
    if (sschema.reserve (1024) ) {
        String templ =  "<?xml version=\"1.0\"?>"
                        "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">"
                        "<specVersion>"
                        "<major>1</major>"
                        "<minor>0</minor>"
                        "</specVersion>"
                        "<URLBase>http://%s:%u/</URLBase>"
                        "<device>"
                        "<deviceType>upnp:rootdevice</deviceType>"
                        "<friendlyName>%s</friendlyName>"
                        "<presentationURL>/</presentationURL>"
                        "<serialNumber>%s</serialNumber>"
                        "<modelName>ESP32</modelName>"
                        "<modelNumber>Marlin</modelNumber>"
                        "<modelURL>http://espressif.com/en/products/hardware/esp-wroom-32/overview</modelURL>"
                        "<manufacturer>Espressif Systems</manufacturer>"
                        "<manufacturerURL>http://espressif.com</manufacturerURL>"
                        "<UDN>uuid:%s</UDN>"
                        "</device>"
                        "</root>\r\n"
                        "\r\n";
        char uuid[37];
        String sip = WiFi.localIP().toString();
        uint32_t chipId = (uint16_t) (ESP.getEfuseMac() >> 32);
        sprintf (uuid, "38323636-4558-4dda-9188-cda0e6%02x%02x%02x",
                 (uint16_t) ( (chipId >> 16) & 0xff),
                 (uint16_t) ( (chipId >>  8) & 0xff),
                 (uint16_t)   chipId        & 0xff  );
        String serialNumber = String (chipId);
        sschema.printf (templ.c_str(),
                        sip.c_str(),
                        _port,
                        _hostname.c_str(),
                        serialNumber.c_str(),
                        uuid);
        _webserver->send (200, "text/xml", (String) sschema);
    } else {
        _webserver->send (500);
    }
}

#endif //SSDP_FEATURE

//Handle web command query and send answer//////////////////////////////
void Web_Server::handle_web_command ()
{
    //to save time if already disconnected
    //if (_webserver->hasArg ("PAGEID") ) {
    //    if (_webserver->arg ("PAGEID").length() > 0 ) {
    //       if (_webserver->arg ("PAGEID").toInt() != _id_connection) {
    //       _webserver->send (200, "text/plain", "Invalid command");
    //       return;
    //       }
    //    }
    //}
    level_authenticate_type auth_level = is_authenticated();
    String cmd = "";
    if (_webserver->hasArg ("plain") || _webserver->hasArg ("commandText") ) {
        if (_webserver->hasArg ("plain") ) {
            cmd = _webserver->arg ("plain");
        } else {
            cmd = _webserver->arg ("commandText");
        }
    } else {
        _webserver->send (200, "text/plain", "Invalid command");
        return;
    }
    //if it is internal command [ESPXXX]<parameter>
    cmd.trim();
    int ESPpos = cmd.indexOf ("[ESP");
    if (ESPpos > -1) {
        //is there the second part?
        int ESPpos2 = cmd.indexOf ("]", ESPpos);
        if (ESPpos2 > -1) {
            //Split in command and parameters
            String cmd_part1 = cmd.substring (ESPpos + 4, ESPpos2);
            String cmd_part2 = "";
            //only [ESP800] is allowed login free if authentication is enabled
            if ( (auth_level == LEVEL_GUEST)  && (cmd_part1.toInt() != 800) ) {
                _webserver->send (401, "text/plain", "Authentication failed!\n");
                return;
            }
            //is there space for parameters?
            if (ESPpos2 < cmd.length() ) {
                cmd_part2 = cmd.substring (ESPpos2 + 1);
            }
            //if command is a valid number then execute command
            if (cmd_part1.toInt() >= 0) {
                ESPResponseStream espresponse(_webserver);
                //commmand is web only
                COMMAND::execute_internal_command (cmd_part1.toInt(), cmd_part2, auth_level, &espresponse);
                //flush
                espresponse.flush();
            }
            //if not is not a valid [ESPXXX] command
        }
    } else { //execute GCODE
        if (auth_level == LEVEL_GUEST) {
            _webserver->send (401, "text/plain", "Authentication failed!\n");
            return;
        }
        //Instead of send several commands one by one by web  / send full set and split here
        String scmd;
        String res = "Ok";
        uint8_t sindex = 0;
        scmd = get_Splited_Value(cmd,'\n', sindex);
        while ( scmd != "" ) {
            scmd+="\n";
            Serial2Socket.push(scmd.c_str());
            //GCodeQueue::enqueue_one_now(scmd.c_str());
            sindex++;
            scmd = get_Splited_Value(cmd,'\n', sindex);
        }
        _webserver->send (200, "text/plain", res.c_str());
    }
}
//Handle web command query and send answer//////////////////////////////
void Web_Server::handle_web_command_silent ()
{
    //to save time if already disconnected
    //if (_webserver->hasArg ("PAGEID") ) {
    //   if (_webserver->arg ("PAGEID").length() > 0 ) {
    //      if (_webserver->arg ("PAGEID").toInt() != _id_connection) {
    //      _webserver->send (200, "text/plain", "Invalid command");
    //      return;
    //      }
    //   }
    //}
    level_authenticate_type auth_level = is_authenticated();
    String cmd = "";
    if (_webserver->hasArg ("plain") || _webserver->hasArg ("commandText") ) {
        if (_webserver->hasArg ("plain") ) {
            cmd = _webserver->arg ("plain");
        } else {
            cmd = _webserver->arg ("commandText");
        }
    } else {
        _webserver->send (200, "text/plain", "Invalid command");
        return;
    }
    //if it is internal command [ESPXXX]<parameter>
    cmd.trim();
    int ESPpos = cmd.indexOf ("[ESP");
    if (ESPpos > -1) {
        //is there the second part?
        int ESPpos2 = cmd.indexOf ("]", ESPpos);
        if (ESPpos2 > -1) {
            //Split in command and parameters
            String cmd_part1 = cmd.substring (ESPpos + 4, ESPpos2);
            String cmd_part2 = "";
            //only [ESP800] is allowed login free if authentication is enabled
            if ( (auth_level == LEVEL_GUEST)  && (cmd_part1.toInt() != 800) ) {
                _webserver->send (401, "text/plain", "Authentication failed!\n");
                return;
            }
            //is there space for parameters?
            if (ESPpos2 < cmd.length() ) {
                cmd_part2 = cmd.substring (ESPpos2 + 1);
            }
            //if command is a valid number then execute command
            if (cmd_part1.toInt() >= 0) {
                //commmand is web only
                if(COMMAND::execute_internal_command (cmd_part1.toInt(), cmd_part2, auth_level, NULL)) {
                    _webserver->send (200, "text/plain", "ok");
                } else {
                    _webserver->send (200, "text/plain", "error");
                }
            }
            //if not is not a valid [ESPXXX] command
        }
    } else { //execute GCODE
        if (auth_level == LEVEL_GUEST) {
            _webserver->send (401, "text/plain", "Authentication failed!\n");
            return;
        }
        //Instead of send several commands one by one by web  / send full set and split here
        String scmd;
        uint8_t sindex = 0;
        scmd = get_Splited_Value(cmd,'\n', sindex);
        String res = "Ok";
        while (scmd != "" ) {
            scmd+="\n";
            Serial2Socket.push(scmd.c_str());
            //GCodeQueue::enqueue_one_now(scmd.c_str());
            sindex++;
            scmd = get_Splited_Value(cmd,'\n', sindex);
        }
        _webserver->send (200, "text/plain", res.c_str());
    }
}

//login status check
void Web_Server::handle_login()
{
#ifdef AUTHENTICATION_FEATURE
    String smsg;
    String sUser,sPassword;
    String auths;
    int code = 200;
    bool msg_alert_error=false;
    //disconnect can be done anytime no need to check credential
    if (_webserver->hasArg("DISCONNECT")) {
        String cookie = _webserver->header("Cookie");
        int pos = cookie.indexOf("ESPSESSIONID=");
        String sessionID;
        if (pos!= -1) {
            int pos2 = cookie.indexOf(";",pos);
            sessionID = cookie.substring(pos+strlen("ESPSESSIONID="),pos2);
        }
        ClearAuthIP(_webserver->client().remoteIP(), sessionID.c_str());
        _webserver->sendHeader("Set-Cookie","ESPSESSIONID=0");
        _webserver->sendHeader("Cache-Control","no-cache");
        String buffer2send = "{\"status\":\"Ok\",\"authentication_lvl\":\"guest\"}";
        _webserver->send(code, "application/json", buffer2send);
        //_webserver->client().stop();
        return;
    }

    level_authenticate_type auth_level = is_authenticated();
    if (auth_level == LEVEL_GUEST) {
        auths = "guest";
    } else if (auth_level == LEVEL_USER) {
        auths = "user";
    } else if (auth_level == LEVEL_ADMIN) {
        auths = "admin";
    } else {
        auths = "???";
    }

    //check is it is a submission or a query
    if (_webserver->hasArg("SUBMIT")) {
        //is there a correct list of query?
        if ( _webserver->hasArg("PASSWORD") && _webserver->hasArg("USER")) {
            //USER
            sUser = _webserver->arg("USER");
            if ( !((sUser == DEFAULT_ADMIN_LOGIN) || (sUser == DEFAULT_USER_LOGIN))) {
                msg_alert_error=true;
                smsg = "Error : Incorrect User";
                code = 401;
            }
            if (msg_alert_error == false) {
                //Password
                sPassword = _webserver->arg("PASSWORD");
                String sadminPassword;

                Preferences prefs;
                prefs.begin(NAMESPACE, true);
                String defV = DEFAULT_ADMIN_PWD;
                sadminPassword = prefs.getString(ADMIN_PWD_ENTRY, defV);
                String suserPassword;
                defV = DEFAULT_USER_PWD;
                suserPassword = prefs.getString(USER_PWD_ENTRY, defV);
                prefs.end();

                if(!(((sUser == DEFAULT_ADMIN_LOGIN) && (strcmp(sPassword.c_str(),sadminPassword.c_str()) == 0)) ||
                        ((sUser == DEFAULT_USER_LOGIN) && (strcmp(sPassword.c_str(),suserPassword.c_str()) == 0)))) {
                    msg_alert_error=true;
                    smsg = "Error: Incorrect password";
                    code = 401;
                }
            }
        } else {
            msg_alert_error=true;
            smsg = "Error: Missing data";
            code = 500;
        }
        //change password
        if (_webserver->hasArg("PASSWORD") && _webserver->hasArg("USER") && _webserver->hasArg("NEWPASSWORD") && (msg_alert_error==false) ) {
            String newpassword =  _webserver->arg("NEWPASSWORD");
            if (COMMAND::isLocalPasswordValid(newpassword.c_str())) {
                String spos;
                if(sUser == DEFAULT_ADMIN_LOGIN) {
                    spos = ADMIN_PWD_ENTRY;
                } else {
                    spos = USER_PWD_ENTRY;
                }

                Preferences prefs;
                prefs.begin(NAMESPACE, false);
                if (prefs.putString(spos.c_str(), newpassword) != newpassword.length()) {
                    msg_alert_error = true;
                    smsg = "Error: Cannot apply changes";
                    code = 500;
                }
                prefs.end();
            } else {
                msg_alert_error=true;
                smsg = "Error: Incorrect password";
                code = 500;
            }
        }
        if ((code == 200) || (code == 500)) {
            level_authenticate_type current_auth_level;
            if(sUser == DEFAULT_ADMIN_LOGIN) {
                current_auth_level = LEVEL_ADMIN;
            } else  if(sUser == DEFAULT_USER_LOGIN) {
                current_auth_level = LEVEL_USER;
            } else {
                current_auth_level = LEVEL_GUEST;
            }
            //create Session
            if ((current_auth_level != auth_level) || (auth_level== LEVEL_GUEST)) {
                auth_ip * current_auth = new auth_ip;
                current_auth->level = current_auth_level;
                current_auth->ip=_webserver->client().remoteIP();
                strcpy(current_auth->sessionID,create_session_ID());
                strcpy(current_auth->userID,sUser.c_str());
                current_auth->last_time=millis();
                if (AddAuthIP(current_auth)) {
                    String tmps ="ESPSESSIONID=";
                    tmps+=current_auth->sessionID;
                    _webserver->sendHeader("Set-Cookie",tmps);
                    _webserver->sendHeader("Cache-Control","no-cache");
                    switch(current_auth->level) {
                    case LEVEL_ADMIN:
                        auths = "admin";
                        break;
                    case LEVEL_USER:
                        auths = "user";
                        break;
                    default:
                        auths = "guest";
                        break;
                    }
                } else {
                    delete current_auth;
                    msg_alert_error=true;
                    code = 500;
                    smsg = "Error: Too many connections";
                }
            }
        }
        if (code == 200) {
            smsg = "Ok";
        }

        //build  JSON
        String buffer2send = "{\"status\":\"" + smsg + "\",\"authentication_lvl\":\"";
        buffer2send += auths;
        buffer2send += "\"}";
        _webserver->send(code, "application/json", buffer2send);
    } else {
        if (auth_level != LEVEL_GUEST) {
            String cookie = _webserver->header("Cookie");
            int pos = cookie.indexOf("ESPSESSIONID=");
            String sessionID;
            if (pos!= -1) {
                int pos2 = cookie.indexOf(";",pos);
                sessionID = cookie.substring(pos+strlen("ESPSESSIONID="),pos2);
                auth_ip * current_auth_info = GetAuth(_webserver->client().remoteIP(), sessionID.c_str());
                if (current_auth_info != NULL) {
                    sUser = current_auth_info->userID;
                }
            }
        }
        String buffer2send = "{\"status\":\"200\",\"authentication_lvl\":\"";
        buffer2send += auths;
        buffer2send += "\",\"user\":\"";
        buffer2send += sUser;
        buffer2send +="\"}";
        _webserver->send(code, "application/json", buffer2send);
    }
#else
    _webserver->sendHeader("Cache-Control","no-cache");
    _webserver->send(200, "application/json", "{\"status\":\"Ok\",\"authentication_lvl\":\"admin\"}");
#endif
}
//SPIFFS
//SPIFFS files list and file commands
void Web_Server::handleFileList ()
{
    level_authenticate_type auth_level = is_authenticated();
    if (auth_level == LEVEL_GUEST) {
        _upload_status = UPLOAD_STATUS_NONE;
        _webserver->send (401, "text/plain", "Authentication failed!\n");
        return;
    }
    String path ;
    String status = "Ok";
    if (_upload_status == UPLOAD_STATUS_FAILED) {
        status = "Upload failed";
        _upload_status = UPLOAD_STATUS_NONE;
    }
    _upload_status = UPLOAD_STATUS_NONE;
    //be sure root is correct according authentication
    if (auth_level == LEVEL_ADMIN) {
        path = "/";
    } else {
        path = "/user";
    }
    //get current path
    if (_webserver->hasArg ("path") ) {
        path += _webserver->arg ("path") ;
    }
    //to have a clean path
    path.trim();
    path.replace ("//", "/");
    if (path[path.length() - 1] != '/') {
        path += "/";
    }
    //check if query need some action
    if (_webserver->hasArg ("action") ) {
        //delete a file
        if (_webserver->arg ("action") == "delete" && _webserver->hasArg ("filename") ) {
            String filename;
            String shortname = _webserver->arg ("filename");
            shortname.replace ("/", "");
            filename = path + _webserver->arg ("filename");
            filename.replace ("//", "/");
            if (!SPIFFS.exists (filename) ) {
                status = shortname + " does not exists!";
            } else {
                if (SPIFFS.remove (filename) ) {
                    status = shortname + " deleted";
                    //what happen if no "/." and no other subfiles ?
                    String ptmp = path;
                    if ( (path != "/") && (path[path.length() - 1] = '/') ) {
                        ptmp = path.substring (0, path.length() - 1);
                    }
                    File dir = SPIFFS.open (ptmp);
                    File dircontent = dir.openNextFile();
                    if (!dircontent) {
                        //keep directory alive even empty
                        File r = SPIFFS.open (path + "/.", FILE_WRITE);
                        if (r) {
                            r.close();
                        }
                    }
                } else {
                    status = "Cannot deleted " ;
                    status += shortname ;
                }
            }
        }
        //delete a directory
        if (_webserver->arg ("action") == "deletedir" && _webserver->hasArg ("filename") ) {
            String filename;
            String shortname = _webserver->arg ("filename");
            shortname.replace ("/", "");
            filename = path + _webserver->arg ("filename");
            filename += "/";
            filename.replace ("//", "/");
            if (filename != "/") {
                bool delete_error = false;
                File dir = SPIFFS.open (path + shortname);
                {
                    File file2deleted = dir.openNextFile();
                    while (file2deleted) {
                        String fullpath = file2deleted.name();
                        if (!SPIFFS.remove (fullpath) ) {
                            delete_error = true;
                            status = "Cannot deleted " ;
                            status += fullpath;
                        }
                        file2deleted = dir.openNextFile();
                    }
                }
                if (!delete_error) {
                    status = shortname ;
                    status += " deleted";
                }
            }
        }
        //create a directory
        if (_webserver->arg ("action") == "createdir" && _webserver->hasArg ("filename") ) {
            String filename;
            filename = path + _webserver->arg ("filename") + "/.";
            String shortname = _webserver->arg ("filename");
            shortname.replace ("/", "");
            filename.replace ("//", "/");
            if (SPIFFS.exists (filename) ) {
                status = shortname + " already exists!";
            } else {
                File r = SPIFFS.open (filename, FILE_WRITE);
                if (!r) {
                    status = "Cannot create ";
                    status += shortname ;
                } else {
                    r.close();
                    status = shortname + " created";
                }
            }
        }
    }
    String jsonfile = "{";
    String ptmp = path;
    if ( (path != "/") && (path[path.length() - 1] = '/') ) {
        ptmp = path.substring (0, path.length() - 1);
    }
    File dir = SPIFFS.open (ptmp);
    jsonfile += "\"files\":[";
    bool firstentry = true;
    String subdirlist = "";
    File fileparsed = dir.openNextFile();
    while (fileparsed) {
        String filename = fileparsed.name();
        String size = "";
        bool addtolist = true;
        //remove path from name
        filename = filename.substring (path.length(), filename.length() );
        //check if file or subfile
        if (filename.indexOf ("/") > -1) {
            //Do not rely on "/." to define directory as SPIFFS upload won't create it but directly files
            //and no need to overload SPIFFS if not necessary to create "/." if no need
            //it will reduce SPIFFS available space so limit it to creation
            filename = filename.substring (0, filename.indexOf ("/") );
            String tag = "*";
            tag += filename + "*";
            if (subdirlist.indexOf (tag) > -1 || filename.length() == 0) { //already in list
                addtolist = false; //no need to add
            } else {
                size = "-1"; //it is subfile so display only directory, size will be -1 to describe it is directory
                if (subdirlist.length() == 0) {
                    subdirlist += "*";
                }
                subdirlist += filename + "*"; //add to list
            }
        } else {
            //do not add "." file
            if (! ( (filename == ".") || (filename == "") ) ) {
                size = ESPResponseStream::formatBytes (fileparsed.size() );
            } else {
                addtolist = false;
            }
        }
        if (addtolist) {
            if (!firstentry) {
                jsonfile += ",";
            } else {
                firstentry = false;
            }
            jsonfile += "{";
            jsonfile += "\"name\":\"";
            jsonfile += filename;
            jsonfile += "\",\"size\":\"";
            jsonfile += size;
            jsonfile += "\"";
            jsonfile += "}";
        }
        fileparsed = dir.openNextFile();
    }
    jsonfile += "],";
    jsonfile += "\"path\":\"" + path + "\",";
    jsonfile += "\"status\":\"" + status + "\",";
    size_t totalBytes;
    size_t usedBytes;
    totalBytes = SPIFFS.totalBytes();
    usedBytes = SPIFFS.usedBytes();
    jsonfile += "\"total\":\"" + ESPResponseStream::formatBytes (totalBytes) + "\",";
    jsonfile += "\"used\":\"" + ESPResponseStream::formatBytes (usedBytes) + "\",";
    jsonfile.concat (F ("\"occupation\":\"") );
    jsonfile += String (100 * usedBytes / totalBytes);
    jsonfile += "\"";
    jsonfile += "}";
    path = "";
    _webserver->sendHeader("Cache-Control", "no-cache");
    _webserver->send(200, "application/json", jsonfile);
    _upload_status = UPLOAD_STATUS_NONE;
}

//push error code and message to websocket
void Web_Server::pushError(int code, const char * st, uint16_t web_error, uint16_t timeout)
{
    if (_socket_server && st) {
        String s = "ERROR:" + String(code) + ":";
        s+=st;
        _socket_server->sendTXT(_id_connection, s);
        if (web_error != 0) {
            if (_webserver) {
                if (_webserver->client().available() > 0) {
                    _webserver->send (web_error, "text/xml", st);
                }
            }
        }
        uint32_t t = millis();
        while (millis() - t < timeout) {
            _socket_server->loop();
            delay(10);
        }
    }
}

//abort reception of packages
void Web_Server::cancelUpload()
{
    if (_webserver) {
        if (_webserver->client().available() > 0) {
            HTTPUpload& upload = _webserver->upload();
            upload.status = UPLOAD_FILE_ABORTED;
            errno = ECONNABORTED;
            _webserver->client().stop();
            delay(100);
        }
    }
}

//SPIFFS files uploader handle
void Web_Server::SPIFFSFileupload ()
{
    static String filename;
    static File fsUploadFile = (File)0;
    //get authentication status
    level_authenticate_type auth_level= is_authenticated();
    //Guest cannot upload - only admin
    if (auth_level == LEVEL_GUEST) {
        _upload_status = UPLOAD_STATUS_FAILED;
        Esp3DCom::echo("Upload rejected");
        pushError(ESP_ERROR_AUTHENTICATION, "Upload rejected", 401);
    } else {
        HTTPUpload& upload = _webserver->upload();
        if((_upload_status != UPLOAD_STATUS_FAILED)|| (upload.status == UPLOAD_FILE_START)) {
            //Upload start
            //**************
            if(upload.status == UPLOAD_FILE_START) {
                _upload_status= UPLOAD_STATUS_ONGOING;
                String upload_filename = upload.filename;
                if (upload_filename[0] != '/') {
                    filename = "/" + upload_filename;
                } else {
                    filename = upload.filename;
                }
                //according User or Admin the root is different as user is isolate to /user when admin has full access
                if(auth_level != LEVEL_ADMIN) {
                    upload_filename = filename;
                    filename = "/user" + upload_filename;
                }

                if (SPIFFS.exists (filename) ) {
                    SPIFFS.remove (filename);
                }
                if (fsUploadFile ) {
                    fsUploadFile.close();
                }
                String  sizeargname  = upload.filename + "S";
                if (_webserver->hasArg (sizeargname.c_str()) ) {
                    uint32_t filesize = _webserver->arg (sizeargname.c_str()).toInt();
                    uint32_t freespace = SPIFFS.totalBytes() - SPIFFS.usedBytes();
                    if (filesize > freespace) {
                        _upload_status=UPLOAD_STATUS_FAILED;
                        Esp3DCom::echo("Upload error");
                        pushError(ESP_ERROR_NOT_ENOUGH_SPACE, "Upload rejected, not enough space");
                    }

                }
                if (_upload_status != UPLOAD_STATUS_FAILED) {
                    //create file
                    fsUploadFile = SPIFFS.open(filename, FILE_WRITE);
                    //check If creation succeed
                    if (fsUploadFile) {
                        //if yes upload is started
                        _upload_status= UPLOAD_STATUS_ONGOING;
                    } else {
                        //if no set cancel flag
                        _upload_status=UPLOAD_STATUS_FAILED;
                        Esp3DCom::echo("Upload error");
                        pushError(ESP_ERROR_FILE_CREATION, "File creation failed");
                    }
                }
                //Upload write
                //**************
            } else if(upload.status == UPLOAD_FILE_WRITE) {
                vTaskDelay(1 / portTICK_RATE_MS);
                //check if file is available and no error
                if(fsUploadFile && _upload_status == UPLOAD_STATUS_ONGOING) {
                    //no error so write post date
                    if (upload.currentSize != fsUploadFile.write(upload.buf, upload.currentSize)) {
                        _upload_status=UPLOAD_STATUS_FAILED;
                        Esp3DCom::echo("Upload error");
                        pushError(ESP_ERROR_FILE_WRITE, "File write failed");
                    }
                } else {
                    //we have a problem set flag UPLOAD_STATUS_FAILED
                    _upload_status=UPLOAD_STATUS_FAILED;
                    Esp3DCom::echo("Upload error");
                    pushError(ESP_ERROR_FILE_WRITE, "File write failed");
                }
                //Upload end
                //**************
            } else if(upload.status == UPLOAD_FILE_END) {
                //check if file is still open
                if(fsUploadFile) {
                    //close it
                    fsUploadFile.close();
                    //check size
                    String  sizeargname  = upload.filename + "S";
                    fsUploadFile = SPIFFS.open (filename, FILE_READ);
                    uint32_t filesize = fsUploadFile.size();
                    fsUploadFile.close();
                    if (_webserver->hasArg (sizeargname.c_str()) ) {
                        if (_webserver->arg (sizeargname.c_str()) != String(filesize)) {
                            _upload_status = UPLOAD_STATUS_FAILED;
                        }
                    }
                    if (_upload_status == UPLOAD_STATUS_ONGOING) {
                        _upload_status = UPLOAD_STATUS_SUCCESSFUL;
                    } else {
                        Esp3DCom::echo("Upload error");
                        pushError(ESP_ERROR_UPLOAD, "File upload failed");
                    }
                } else {
                    //we have a problem set flag UPLOAD_STATUS_FAILED
                    _upload_status=UPLOAD_STATUS_FAILED;
                    pushError(ESP_ERROR_FILE_CLOSE, "File close failed");
                    Esp3DCom::echo("Upload error");

                }
                //Upload cancelled
                //**************
            } else {
                _upload_status = UPLOAD_STATUS_FAILED;
                //pushError(ESP_ERROR_UPLOAD, "File upload failed");
                return;
            }
        }
    }

    if (_upload_status == UPLOAD_STATUS_FAILED) {
        cancelUpload();
        if (SPIFFS.exists (filename) ) {
            SPIFFS.remove (filename);
        }
    }
    Esp3DLibConfig::wait(0);
}

//Web Update handler
void Web_Server::handleUpdate ()
{
    level_authenticate_type auth_level = is_authenticated();
    if (auth_level != LEVEL_ADMIN) {
        _upload_status = UPLOAD_STATUS_NONE;
        _webserver->send (403, "text/plain", "Not allowed, log in first!\n");
        return;
    }
    String jsonfile = "{\"status\":\"" ;
    jsonfile += String(_upload_status);
    jsonfile += "\"}";
    //send status
    _webserver->sendHeader("Cache-Control", "no-cache");
    _webserver->send(200, "application/json", jsonfile);
    //if success restart
    if (_upload_status == UPLOAD_STATUS_SUCCESSFUL) {
        Esp3DLibConfig::wait(1000);
        Esp3DLibConfig::restart_ESP();
    } else {
        _upload_status = UPLOAD_STATUS_NONE;
    }
}

//File upload for Web update
void Web_Server::WebUpdateUpload ()
{
    static size_t last_upload_update;
    static uint32_t maxSketchSpace = 0;
    //only admin can update FW
    if (is_authenticated() != LEVEL_ADMIN) {
        _upload_status = UPLOAD_STATUS_FAILED;
        Esp3DCom::echo("Upload rejected");
        pushError(ESP_ERROR_AUTHENTICATION, "Upload rejected", 401);
    } else {
        //get current file ID
        HTTPUpload& upload = _webserver->upload();
        if((_upload_status != UPLOAD_STATUS_FAILED)|| (upload.status == UPLOAD_FILE_START)) {
            //Upload start
            //**************
            if(upload.status == UPLOAD_FILE_START) {
                Esp3DCom::echo("Update Firmware");
                _upload_status= UPLOAD_STATUS_ONGOING;
                String  sizeargname  = upload.filename + "S";
                if (_webserver->hasArg (sizeargname.c_str()) ) {
                    maxSketchSpace = _webserver->arg (sizeargname).toInt();
                }
                //check space
                size_t flashsize = 0;
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
                if (flashsize < maxSketchSpace) {
                    pushError(ESP_ERROR_NOT_ENOUGH_SPACE, "Upload rejected, not enough space");
                    _upload_status=UPLOAD_STATUS_FAILED;
                    Esp3DCom::echo("Update cancelled");
                }
                if (_upload_status != UPLOAD_STATUS_FAILED) {
                    last_upload_update = 0;
                    if(!Update.begin()) { //start with max available size
                        _upload_status=UPLOAD_STATUS_FAILED;
                        Esp3DCom::echo("Update cancelled");
                        pushError(ESP_ERROR_NOT_ENOUGH_SPACE, "Upload rejected, not enough space");
                    } else {
                        Esp3DCom::echo("Update 0%");
                    }
                }
                //Upload write
                //**************
            } else if(upload.status == UPLOAD_FILE_WRITE) {
                //check if no error
                if (_upload_status == UPLOAD_STATUS_ONGOING) {
                    if ( ((100 * upload.totalSize) / maxSketchSpace) !=last_upload_update) {
                        if ( maxSketchSpace > 0) {
                            last_upload_update = (100 * upload.totalSize) / maxSketchSpace;
                        } else {
                            last_upload_update = upload.totalSize;
                        }
                        String s = "Update ";
                        s+= String(last_upload_update);
                        s+="%";
                        Esp3DCom::echo(s.c_str());
                    }
                    if(Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                        _upload_status=UPLOAD_STATUS_FAILED;
                        Esp3DCom::echo("Update write failed");
                        pushError(ESP_ERROR_FILE_WRITE, "File write failed");
                    }
                }
                //Upload end
                //**************
            } else if(upload.status == UPLOAD_FILE_END) {
                if(Update.end(true)) { //true to set the size to the current progress
                    //Now Reboot
                    Esp3DCom::echo("Update 100%");
                    _upload_status=UPLOAD_STATUS_SUCCESSFUL;
                } else {
                    _upload_status=UPLOAD_STATUS_FAILED;
                    Esp3DCom::echo("Update failed");
                    pushError(ESP_ERROR_UPLOAD, "Update upload failed");
                }
            } else if(upload.status == UPLOAD_FILE_ABORTED) {
                Esp3DCom::echo("Update failed");
                _upload_status=UPLOAD_STATUS_FAILED;
                return;
            }
        }
    }
    if (_upload_status == UPLOAD_STATUS_FAILED) {
        cancelUpload();
        Update.end();
    }
    Esp3DLibConfig::wait(0);
}


#if defined(SDSUPPORT)

//direct SD files list//////////////////////////////////////////////////
void Web_Server::handle_direct_SDFileList()
{
    //this is only for admin an user
    if (is_authenticated() == LEVEL_GUEST) {
        _upload_status=UPLOAD_STATUS_NONE;
        _webserver->send(401, "application/json", "{\"status\":\"Authentication failed!\"}");
        return;
    }


    String path="/";
    String sstatus="Ok";
    if ((_upload_status == UPLOAD_STATUS_FAILED) || (_upload_status == UPLOAD_STATUS_CANCELLED)) {
        sstatus = "Upload failed";
        _upload_status = UPLOAD_STATUS_NONE;
    }
    bool list_files = true;
    ESP_SD card;
    int8_t state = card.card_status(true);
    if (state != 1) {
        _webserver->sendHeader("Cache-Control","no-cache");
        _webserver->send(200, "application/json", "{\"status\":\"No SD Card\"}");
        return;
    }

    //get current path
    if(_webserver->hasArg("path")) {
        path += _webserver->arg("path") ;
    }
    //to have a clean path
    path.trim();
    path.replace("//","/");

    //check if query need some action
    if(_webserver->hasArg("action")) {
        //delete a file
        if(_webserver->arg("action") == "delete" && _webserver->hasArg("filename")) {
            String filename;
            String shortname = _webserver->arg("filename");
            filename = path + shortname;
            shortname.replace("/","");
            filename.replace("//","/");

            if(!card.exists(filename.c_str())) {
                sstatus = shortname + " does not exist!";
            } else {
                if (card.remove(filename.c_str())) {
                    sstatus = shortname + " deleted";
                } else {
                    sstatus = "Cannot deleted ";
                    sstatus+=shortname ;
                }
            }
        }
        //delete a directory
        if( _webserver->arg("action") == "deletedir" &&  _webserver->hasArg("filename")) {
            String filename;
            String shortname = _webserver->arg("filename");
            shortname.replace("/","");
            filename = path + "/" + shortname;
            filename.replace("//","/");
            if (filename != "/") {
                if(!card.dir_exists(filename.c_str())) {
                    sstatus = shortname + " does not exist!";
                } else {
                    if (!card.rmdir(filename.c_str())) {
                        sstatus ="Error deleting: ";
                        sstatus += shortname ;
                    } else {
                        sstatus = shortname ;
                        sstatus+=" deleted";
                    }
                }
            } else {
                sstatus ="Cannot delete root";
            }
        }
        //create a directory
        if( _webserver->arg("action")=="createdir" &&  _webserver->hasArg("filename")) {
            String filename;
            String shortname =  _webserver->arg("filename");
            filename = path + shortname;
            shortname.replace("/","");
            filename.replace("//","/");
            if(card.exists(filename.c_str())) {
                sstatus = shortname + " already exists!";
            } else {
                if (!card.mkdir(filename.c_str())) {
                    sstatus = "Cannot create ";
                    sstatus += shortname ;
                } else {
                    sstatus = shortname + " created";
                }
            }
        }
    }

    //check if no need build file list
    if( _webserver->hasArg("dontlist")) {
        if( _webserver->arg("dontlist") == "yes") {
            list_files = false;
        }
    }
    String jsonfile = "{" ;
    jsonfile+="\"files\":[";
    if (!card.openDir(path)) {
        String s =  "{\"status\":\" ";
        s += path;
        s+=  " does not exist on SD Card\"}";
        _webserver->sendHeader("Cache-Control","no-cache");
        _webserver->send(200, "application/json", s.c_str());
        return;
    }
    if (list_files) {
        char name[13];
        uint32_t size;
        bool isFile;
        uint i = 0;
        while (card.readDir(name,&size,&isFile)) {
            if (i>0) {
                jsonfile+=",";
            }
            jsonfile+="{\"name\":\"";
            jsonfile+=name;
            jsonfile+="\",\"shortname\":\"";
            jsonfile+=name;
            jsonfile+="\",\"size\":\"";
            if (isFile) {
                jsonfile+=ESPResponseStream::formatBytes(size);
            } else {
                jsonfile+="-1";
            }
            jsonfile+="\",\"datetime\":\"";
            //TODO datatime
            jsonfile+="\"}";
            i++;
            Esp3DLibConfig::wait(1);
        }
        jsonfile+="],\"path\":\"";
        jsonfile+=path + "\",";
    }
    static uint64_t volTotal = card.card_total_space();
    static uint64_t volUsed = card.card_used_space();
    //TODO
    //Get right values
    uint32_t  occupedspace = (volUsed/volTotal)*100;
    jsonfile+="\"total\":\"";
    if ( (occupedspace <= 1) && (volTotal!=volUsed)) {
        occupedspace=1;
    }
    jsonfile+= ESPResponseStream::formatBytes(volTotal); ;

    jsonfile+="\",\"used\":\"";
    jsonfile+= ESPResponseStream::formatBytes(volUsed); ;
    jsonfile+="\",\"occupation\":\"";
    jsonfile+=String(occupedspace);
    jsonfile+= "\",";

    jsonfile+= "\"mode\":\"direct\",";
    jsonfile+= "\"status\":\"";
    jsonfile+=sstatus + "\"";
    jsonfile+= "}";

    _webserver->sendHeader("Cache-Control","no-cache");
    _webserver->send (200, "application/json", jsonfile.c_str());
    _upload_status=UPLOAD_STATUS_NONE;
}

//SD File upload with direct access to SD///////////////////////////////
void Web_Server::SDFile_direct_upload()
{
    static ESP_SD sdfile;
    static String upload_filename;
    //this is only for admin and user
    if (is_authenticated() == LEVEL_GUEST) {
        _upload_status=UPLOAD_STATUS_FAILED;
        _webserver->send(401, "application/json", "{\"status\":\"Authentication failed!\"}");
        pushError(ESP_ERROR_AUTHENTICATION, "Upload rejected", 401);
    } else {
        //retrieve current file id
        HTTPUpload& upload = _webserver->upload();
        if((_upload_status != UPLOAD_STATUS_FAILED) || (upload.status == UPLOAD_FILE_START)) {
            //Upload start
            //**************
            if(upload.status == UPLOAD_FILE_START) {
                upload_filename = upload.filename;
                if (upload_filename[0] != '/') {
                    upload_filename = "/" + upload.filename;
                }
                upload_filename=  sdfile.makepath83(upload_filename);
                if ( sdfile.card_status(true) != 1) {
                    _upload_status=UPLOAD_STATUS_FAILED;
                    Esp3DCom::echo("Upload cancelled");
                    pushError(ESP_ERROR_UPLOAD_CANCELLED, "Upload cancelled");
                } else {
                    if (sdfile.exists (upload_filename.c_str()) ) {
                        sdfile.remove (upload_filename.c_str());
                    }

                    if (sdfile.isopen()) {
                        sdfile.close();
                    }
                    String  sizeargname  = upload.filename + "S";
                    if (_webserver->hasArg (sizeargname.c_str()) ) {
                        uint32_t filesize = _webserver->arg (sizeargname.c_str()).toInt();
                        uint64_t freespace = sdfile.card_total_space()-sdfile.card_used_space();;
                        if (filesize > freespace) {
                            _upload_status=UPLOAD_STATUS_FAILED;
                            Esp3DCom::echo("Upload cancelled");
                            pushError(ESP_ERROR_NOT_ENOUGH_SPACE, "Upload rejected, not enough space");
                        }

                    }
                    if (_upload_status != UPLOAD_STATUS_FAILED) {
                        if (!sdfile.open (upload_filename.c_str(),false)) {
                            Esp3DCom::echo("Upload failed");
                            _upload_status = UPLOAD_STATUS_FAILED;
                            pushError(ESP_ERROR_FILE_CREATION, "File creation failed");
                        } else {
                            _upload_status = UPLOAD_STATUS_ONGOING;
                        }
                    }
                }
                //Upload write
                //**************
            } else if(upload.status == UPLOAD_FILE_WRITE) {
                //we need to check SD is inside
                if ((sdfile.card_status() == 1)  && (_upload_status == UPLOAD_STATUS_ONGOING) && sdfile.isopen()) {
                    if (upload.currentSize != sdfile.write(upload.buf, upload.currentSize)) {
                        _upload_status = UPLOAD_STATUS_FAILED;
                        Esp3DCom::echo("Upload failed");
                        pushError(ESP_ERROR_FILE_WRITE, "File write failed");
                    }
                } else { //if error set flag UPLOAD_STATUS_FAILED
                    _upload_status = UPLOAD_STATUS_FAILED;
                    Esp3DCom::echo("Upload failed");
                    pushError(ESP_ERROR_FILE_WRITE, "File write failed");
                }
                //Upload end
                //**************
            } else if(upload.status == UPLOAD_FILE_END) {
                sdfile.close();
                uint32_t filesize = sdfile.size();
                String  sizeargname  = upload.filename + "S";
                if (_webserver->hasArg (sizeargname.c_str()) ) {
                    if (_webserver->arg (sizeargname.c_str()) != String(filesize)) {
                        Esp3DCom::echo("Upload failed");
                        _upload_status = UPLOAD_STATUS_FAILED;
                        pushError(ESP_ERROR_UPLOAD, "File upload mismatch");
                    }
                }
                if (_upload_status == UPLOAD_STATUS_ONGOING) {
                    _upload_status = UPLOAD_STATUS_SUCCESSFUL;
                } else {
                    _upload_status = UPLOAD_STATUS_FAILED;
                    Esp3DCom::echo("Upload failed");
                    pushError(ESP_ERROR_UPLOAD, "Upload error");
                }
            } else {//Upload cancelled
                _upload_status=UPLOAD_STATUS_FAILED;
                Esp3DCom::echo("Upload failed");
            }
        }
    }
    if (_upload_status == UPLOAD_STATUS_FAILED) {
        cancelUpload();
        sdfile.close();
        if (sdfile.exists (upload_filename.c_str()) ) {
            sdfile.remove (upload_filename.c_str());
        }
    }
    Esp3DLibConfig::wait(0);
}
#endif

void Web_Server::handle()
{
    static uint32_t timeout = millis();
#ifdef CAPTIVE_PORTAL_FEATURE
    if(WiFi.getMode() == WIFI_AP) {
        dnsServer.processNextRequest();
    }
#endif //CAPTIVE_PORTAL_FEATURE
    if (_webserver) {
        _webserver->handleClient();
    }
    if (_socket_server && _setupdone) {
        Serial2Socket.handle_flush();
        _socket_server->loop();
    }
    if ((millis() - timeout) > 10000) {
        if (_socket_server) {
            String s = "PING:";
            s+=String(_id_connection);
            _socket_server->broadcastTXT(s);
            timeout=millis();
        }
    }
}


void Web_Server::handle_Websocket_Event(uint8_t num, uint8_t type, uint8_t * payload, size_t length)
{

    switch(type) {
    case WStype_DISCONNECTED:
        //USE_SERIAL.printf("[%u] Disconnected!\n", num);
        break;
    case WStype_CONNECTED: {
        IPAddress ip = _socket_server->remoteIP(num);
        //USE_SERIAL.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        String s = "CURRENT_ID:" + String(num);
        // send message to client
        _id_connection = num;
        _socket_server->sendTXT(_id_connection, s);
        s = "ACTIVE_ID:" + String(_id_connection);
        _socket_server->broadcastTXT(s);
    }
    break;
    case WStype_TEXT:
        //USE_SERIAL.printf("[%u] get Text: %s\n", num, payload);

        // send message to client
        // webSocket.sendTXT(num, "message here");

        // send data to all connected clients
        // webSocket.broadcastTXT("message here");
        break;
    case WStype_BIN:
        //USE_SERIAL.printf("[%u] get binary length: %u\n", num, length);
        //hexdump(payload, length);

        // send message to client
        // webSocket.sendBIN(num, payload, length);
        break;
    default:
        break;
    }

}

String Web_Server::get_Splited_Value(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = {0, -1};
    int maxIndex = data.length()-1;

    for(int i=0; i<=maxIndex && found<=index; i++) {
        if(data.charAt(i)==separator || i==maxIndex) {
            found++;
            strIndex[0] = strIndex[1]+1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }

    return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}


//helper to extract content type from file extension
//Check what is the content tye according extension file
String Web_Server::getContentType (String filename)
{
    String file_name = filename;
    file_name.toLowerCase();
    if (filename.endsWith (".htm") ) {
        return "text/html";
    } else if (file_name.endsWith (".html") ) {
        return "text/html";
    } else if (file_name.endsWith (".css") ) {
        return "text/css";
    } else if (file_name.endsWith (".js") ) {
        return "application/javascript";
    } else if (file_name.endsWith (".png") ) {
        return "image/png";
    } else if (file_name.endsWith (".gif") ) {
        return "image/gif";
    } else if (file_name.endsWith (".jpeg") ) {
        return "image/jpeg";
    } else if (file_name.endsWith (".jpg") ) {
        return "image/jpeg";
    } else if (file_name.endsWith (".ico") ) {
        return "image/x-icon";
    } else if (file_name.endsWith (".xml") ) {
        return "text/xml";
    } else if (file_name.endsWith (".pdf") ) {
        return "application/x-pdf";
    } else if (file_name.endsWith (".zip") ) {
        return "application/x-zip";
    } else if (file_name.endsWith (".gz") ) {
        return "application/x-gzip";
    } else if (file_name.endsWith (".txt") ) {
        return "text/plain";
    }
    return "application/octet-stream";
}

//check authentification
level_authenticate_type Web_Server::is_authenticated()
{
#ifdef AUTHENTICATION_FEATURE
    if (_webserver->hasHeader ("Cookie") ) {
        String cookie = _webserver->header ("Cookie");
        int pos = cookie.indexOf ("ESPSESSIONID=");
        if (pos != -1) {
            int pos2 = cookie.indexOf (";", pos);
            String sessionID = cookie.substring (pos + strlen ("ESPSESSIONID="), pos2);
            IPAddress ip = _webserver->client().remoteIP();
            //check if cookie can be reset and clean table in same time
            return ResetAuthIP (ip, sessionID.c_str() );
        }
    }
    return LEVEL_GUEST;
#else
    return LEVEL_ADMIN;
#endif
}

#ifdef AUTHENTICATION_FEATURE

//add the information in the linked list if possible
bool Web_Server::AddAuthIP (auth_ip * item)
{
    if (_nb_ip > MAX_AUTH_IP) {
        return false;
    }
    item->_next = _head;
    _head = item;
    _nb_ip++;
    return true;
}

//Session ID based on IP and time using 16 char
char * Web_Server::create_session_ID()
{
    static char  sessionID[17];
//reset SESSIONID
    for (int i = 0; i < 17; i++) {
        sessionID[i] = '\0';
    }
//get time
    uint32_t now = millis();
//get remote IP
    IPAddress remoteIP = _webserver->client().remoteIP();
//generate SESSIONID
    if (0 > sprintf (sessionID, "%02X%02X%02X%02X%02X%02X%02X%02X", remoteIP[0], remoteIP[1], remoteIP[2], remoteIP[3], (uint8_t) ( (now >> 0) & 0xff), (uint8_t) ( (now >> 8) & 0xff), (uint8_t) ( (now >> 16) & 0xff), (uint8_t) ( (now >> 24) & 0xff) ) ) {
        strcpy (sessionID, "NONE");
    }
    return sessionID;
}


bool Web_Server::ClearAuthIP (IPAddress ip, const char * sessionID)
{
    auth_ip * current = _head;
    auth_ip * previous = NULL;
    bool done = false;
    while (current) {
        if ( (ip == current->ip) && (strcmp (sessionID, current->sessionID) == 0) ) {
            //remove
            done = true;
            if (current == _head) {
                _head = current->_next;
                _nb_ip--;
                delete current;
                current = _head;
            } else {
                previous->_next = current->_next;
                _nb_ip--;
                delete current;
                current = previous->_next;
            }
        } else {
            previous = current;
            current = current->_next;
        }
    }
    return done;
}

//Get info
auth_ip * Web_Server::GetAuth (IPAddress ip, const char * sessionID)
{
    auth_ip * current = _head;
    //auth_ip * previous = NULL;
    //get time
    //uint32_t now = millis();
    while (current) {
        if (ip == current->ip) {
            if (strcmp (sessionID, current->sessionID) == 0) {
                //found
                return current;
            }
        }
        //previous = current;
        current = current->_next;
    }
    return NULL;
}

//Review all IP to reset timers
level_authenticate_type Web_Server::ResetAuthIP (IPAddress ip, const char * sessionID)
{
    auth_ip * current = _head;
    auth_ip * previous = NULL;
    //get time
    //uint32_t now = millis();
    while (current) {
        if ( (millis() - current->last_time) > 360000) {
            //remove
            if (current == _head) {
                _head = current->_next;
                _nb_ip--;
                delete current;
                current = _head;
            } else {
                previous->_next = current->_next;
                _nb_ip--;
                delete current;
                current = previous->_next;
            }
        } else {
            if (ip == current->ip) {
                if (strcmp (sessionID, current->sessionID) == 0) {
                    //reset time
                    current->last_time = millis();
                    return (level_authenticate_type) current->level;
                }
            }
            previous = current;
            current = current->_next;
        }
    }
    return LEVEL_GUEST;
}
#endif


#endif // ESP3D_WIFISUPPORT && HTTP_FEATURE

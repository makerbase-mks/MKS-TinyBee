/*
  espcom.cpp - esp3d communication serial/tcp/etc.. class

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
#include <U8glib-HAL.h>

#if defined(ESP3D_WIFISUPPORT)
#include MARLIN_HAL_PATH(FlushableHardwareSerial.h)
#include MARLIN_HAL_PATH(HAL.h)
#if HAS_GRAPHICAL_LCD
#include <U8glib.h>
#endif
//for 2.0.x
#if defined __has_include
#if __has_include (MARLIN_PATH(lcd/ultralcd.h))
#include MARLIN_PATH(lcd/ultralcd.h)
#endif
//for bugfix-2.0.x
#if __has_include (MARLIN_PATH(lcd/marlinui.h))
#include MARLIN_PATH(lcd/marlinui.h)
#endif
#endif
#include "espcom.h"
#if defined(HTTP_FEATURE)
#include <WebServer.h>
#endif //HTTP_FEATURE
void Esp3DCom::echo(const char * data)
{
    SERIAL_ECHO_START();
    SERIAL_ECHOLN(data);
#if HAS_DISPLAY
    if (strlen(data)) {
        ui.set_status(data);
    } else {
        ui.reset_status();
    }
#endif //HAS_DISPLAY
}
long ESPResponseStream::baudRate()
{
    long br = flushableSerial.baudRate();
    //workaround for ESP32
    if (br == 115201) {
        br = 115200;
    }
    if (br == 230423) {
        br = 230400;
    }
    return br;
}
#if defined(HTTP_FEATURE)
ESPResponseStream::ESPResponseStream(WebServer * webserver)
{
    _header_sent=false;
    _webserver = webserver;
    _pipe = WEB_PIPE;
}
#endif //HTTP_FEATURE
ESPResponseStream::ESPResponseStream(tpipe pipe)
{
    _pipe = pipe;
}

void ESPResponseStream::println(const char *data)
{
    print(data);
    print("\n");
}

void ESPResponseStream::print(const char *data)
{
#if defined(HTTP_FEATURE)
    if (_pipe == WEB_PIPE) {
        if (!_header_sent) {
            _webserver->setContentLength(CONTENT_LENGTH_UNKNOWN);
            _webserver->sendHeader("Content-Type","text/html");
            _webserver->sendHeader("Cache-Control","no-cache");
            _webserver->send(200);
            _header_sent = true;
        }
        _buffer+=data;
        if (_buffer.length() > 1200) {
            //send data
            _webserver->sendContent(_buffer);
            //reset buffer
            _buffer = "";
        }
    }
#endif //HTTP_FEATURE
    if (_pipe == SERIAL_PIPE) {
        SERIAL_ECHOPGM_P(data);
    }
}

void ESPResponseStream::flush()
{
#if defined(HTTP_FEATURE)
    if (_pipe == WEB_PIPE) {
        if(_header_sent) {
            //send data
            if(_buffer.length() > 0) {
                _webserver->sendContent(_buffer);
            }
            //close connection
            _webserver->sendContent("");
        }
        _header_sent = false;
        _buffer = "";
    }
#endif //HTTP_FEATURE
}

//just simple helper to convert mac address to string
char * ESPResponseStream::mac2str (uint8_t mac [8])
{
    static char macstr [18];
    if (0 > sprintf (macstr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]) ) {
        strcpy (macstr, "00:00:00:00:00:00");
    }
    return macstr;
}

//helper to format size to readable string
String ESPResponseStream::formatBytes (uint64_t bytes)
{
    if (bytes < 1024) {
        return String ((uint16_t)bytes) + " B";
    } else if (bytes < (1024 * 1024) ) {
        return String ((float)(bytes / 1024.0),2) + " KB";
    } else if (bytes < (1024 * 1024 * 1024) ) {
        return String ((float)(bytes / 1024.0 / 1024.0),2) + " MB";
    } else {
        return String ((float)(bytes / 1024.0 / 1024.0 / 1024.0),2) + " GB";
    }
}

#endif //ESP3D_WIFISUPPORT

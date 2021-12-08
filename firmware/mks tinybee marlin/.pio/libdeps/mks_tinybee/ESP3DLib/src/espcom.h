/*
  espcom.h - esp3d communication serial/tcp/etc... class

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

#ifndef ESPCOM_H
#define ESPCOM_H
#include "esp3dlibconfig.h"
//Authentication levels
typedef enum {
    LEVEL_GUEST = 0,
    LEVEL_USER = 1,
    LEVEL_ADMIN = 2
} level_authenticate_type;

//communication pipes
typedef enum {
    NO_PIPE = 0,
    SERIAL_PIPE = 2,
#ifdef TCP_IP_DATA_FEATURE
    TCP_PIPE = 5,
#endif
#ifdef WS_DATA_FEATURE
    WS_PIPE = 6,
#endif
#ifdef ESP_OLED_FEATURE
    OLED_PIPE = 7,
#endif
    WEB_PIPE = 8
} tpipe;
#if defined(HTTP_FEATURE)
class WebServer;
#endif //HTTP_FEATURE
class ESPResponseStream
{
public:
    void print(const char *data);
    void println(const char *data);
    void flush();
    tpipe pipe()
    {
        return _pipe;
    };
    ESPResponseStream(tpipe pipe);
#if defined(HTTP_FEATURE)
    ESPResponseStream(WebServer * webserver);
#endif //HTTP_FEATURE
    static char * mac2str (uint8_t mac [8]);
    static String formatBytes (uint64_t bytes);
    static long baudRate();
private:
#if defined(HTTP_FEATURE)
    bool _header_sent;
    WebServer * _webserver;
#endif //HTTP_FEATURE
    tpipe _pipe;
    String _buffer;
};

class Esp3DCom
{
public:
    static void echo(const char * data);
};
#endif //ESPCOM_H

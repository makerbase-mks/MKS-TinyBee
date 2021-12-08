/*
  esp3dlib.h - esp3dlib class

  Copyright (c) 2019 Luc Lebosse. All rights reserved.

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

#ifndef ESP3DLIB_H
#define ESP3DLIB_H
//be sure correct IDE and settings are used for ESP8266 or ESP32
#if !defined(ARDUINO_ARCH_ESP32)
#error Oops!  Make sure you have 'ESP32' compatible board selected
#endif
#include "serial2socket.h"
/*! Esp3DLib main class */
class Esp3DLib
{
public:
    /**
       * Constructor.
       */
    Esp3DLib();
    /**
       * @brief Initializes the task used by \link esp3dlib \endlink.
       */
    void init();
    /**
       * Parser for commmand.
       * @param[in] cmd - the string to parse
       *
       * @return true - if it is seen as ESP command.
       * @return false - if not ESP command.
       */
    bool parse(char * cmd);
    /**
       * idletask function.
       */
    void idletask();
};

/**
 * @brief You must call esp3dlib.init() before using this variable.
 */
extern Esp3DLib esp3dlib;

#endif

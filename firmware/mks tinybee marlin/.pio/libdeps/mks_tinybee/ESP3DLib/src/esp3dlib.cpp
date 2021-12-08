/*
    This file is part of ESP3DLib library for 3D printer.

    ESP3D Firmware for 3D printer is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    ESP3D Firmware for 3D printer is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this Firmware.  If not, see <http://www.gnu.org/licenses/>.

    This firmware is using the standard arduino IDE with module to support ESP8266/ESP32:
    https://github.com/esp8266/Arduino
    https://github.com/espressif/arduino-esp32

    Latest version of the code and documentation can be found here :
    https://github.com/luc-github/ESP3D

    Main author: luc lebosse

*/
#include "esp3dlibconfig.h"

#if defined(ESP3D_WIFISUPPORT)
#include "esp3dlib.h"
#include "wificonfig.h"
#include "wifiservices.h"
#include "espcom.h"
#include "command.h"
Esp3DLib esp3dlib;

#ifndef DELAY_START_ESP3D 
#define DELAY_START_ESP3D 100
#endif //DELAY_START_ESP3D

void ESP3DLibTaskfn( void * parameter )
{
    Esp3DLibConfig::wait(DELAY_START_ESP3D);  // Yield to other tasks
    WiFiConfig::begin();
    for(;;) {
        WiFiConfig::handle();
        Esp3DLibConfig::wait(0);  // Yield to other tasks
    }
    vTaskDelete( NULL );
}


//Contructor
Esp3DLib::Esp3DLib()
{

}

//Begin which setup everything
void Esp3DLib::init()
{
    xTaskCreatePinnedToCore(
        ESP3DLibTaskfn, /* Task function. */
        "ESP3DLib Task", /* name of task. */
        8192, /* Stack size of task */
        NULL, /* parameter of the task */
        ESP3DLIB_RUNNING_PRIORITY, /* priority of the task */
        NULL, /* Task handle to keep track of created task */
        ESP3DLIB_RUNNING_CORE    /* Core to run the task */
    );
}
//Parse command
bool Esp3DLib::parse(char * cmd)
{
    String scmd = cmd;
    int ESPpos = scmd.indexOf ("[ESP");
    if (ESPpos > -1) {
        int ESPpos2 = scmd.indexOf("]",ESPpos);
        if (ESPpos2>-1) {
            //Split in command and parameters
            String cmd_part1=scmd.substring(ESPpos+4,ESPpos2);
            String cmd_part2="";
            //is there space for parameters?
            if (ESPpos2<scmd.length()) {
                cmd_part2=scmd.substring(ESPpos2+1);
            }
            //if command is a valid number then execute command
            if(cmd_part1.toInt() >=0) {
                ESPResponseStream response(SERIAL_PIPE);
                level_authenticate_type auth_type = LEVEL_ADMIN; //we do not care serial authentication
                COMMAND::execute_internal_command (cmd_part1.toInt(), cmd_part2, auth_type,  &response);
                return true;
            } else {
                return false;
            }
        } else {
            return false;
        }
    } else {
        return false;
    }

}

//Idletask when setup is done
void Esp3DLib::idletask()
{
    static bool setupdone = false;
    if (!setupdone) {
        if (wifi_services.started()) {
            if(strlen(WiFiConfig::currentIP())) {
                Esp3DCom::echo(WiFiConfig::currentIP());
            }
            setupdone = true;
        }
    }
}
#endif //ESP3D_WIFISUPPORT

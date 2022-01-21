/**
 * Marlin 3D Printer Firmware
 * Copyright (C) 2016 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (C) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _ESP_SD_H_
#define _ESP_SD_H_

extern bool sd_busy_lock;

class ESP_SD
{
public:
    ESP_SD();
    ~ESP_SD();
    int8_t card_status(bool forcemount = false);
    uint64_t card_total_space();
    uint64_t card_used_space();
    bool open(const char * path, bool readonly = true );
    void close();
    int16_t write(const uint8_t * data, uint16_t len);
    int16_t write(const uint8_t byte);
    uint16_t read(uint8_t * buf, uint16_t nbyte);
    int16_t read();
    uint32_t size();
    uint32_t available();
    bool exists(const char * path);
    bool dir_exists(const char * path);
    bool remove(const char * path);
    bool rmdir(const char * path);
    bool mkdir(const char * path);
    bool isopen();
    String makepath83(String longpath);
    String makeshortname(String longname, uint8_t index = 1);
    bool openDir(String path);
    bool readDir(char name[13], uint32_t * size, bool * isFile);
    bool * isFile;
private:
    void * _sdfile;
    uint32_t _size;
    uint32_t _pos;
    bool _readonly;
    String get_path_part(String data, int index);

};
#endif

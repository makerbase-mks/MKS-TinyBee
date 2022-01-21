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
#include "esp3dlibconfig.h"
#if defined(SDSUPPORT) && defined(ESP3D_WIFISUPPORT)
#include MARLIN_PATH(sd/cardreader.h)
#include MARLIN_PATH(sd/SdVolume.h)
#include MARLIN_PATH(sd/SdFatStructs.h)
#include MARLIN_PATH(sd/SdFile.h)
#include "sd_ESP32.h"


//Cannot move to static variable due to conflict with ESP32 SD library
SdFile workDir;
dir_t dir_info;
SdVolume sd_volume;

bool sd_busy_lock = false;

ESP_SD::ESP_SD()
{
    _size = 0;
    _pos = 0;
    _readonly=true;
    //ugly Workaround to not expose SdFile class which conflict with Native ESP32 SD class
    _sdfile = new SdFile;
}
ESP_SD::~ESP_SD()
{
    if (((SdFile *)_sdfile)->isOpen()) {
        close();
    }
    if (_sdfile) {
        delete (SdFile *) _sdfile;
    }
}

bool ESP_SD::isopen()
{
    return (((SdFile *)_sdfile)->isOpen());
}

int8_t ESP_SD::card_status(bool forcemount)
{
    if(!card.isMounted() || forcemount) {
        card.mount();
    }
    if (!IS_SD_INSERTED() || !card.isMounted()) {
        return 0;    //No sd
    }
    if ( card.isPrinting() || card.isFileOpen() ) {
        return -1;    // busy
    }
    return 1; //ok
}

bool ESP_SD::open(const char * path, bool readonly )
{
    if (path == NULL) {
        return false;
    }
    String fullpath=path;
    String pathname = fullpath.substring(0,fullpath.lastIndexOf("/"));
    String filename = makeshortname(fullpath.substring(fullpath.lastIndexOf("/")+1));
    if (pathname.length() == 0) {
        pathname="/";
    }
    if (!openDir(pathname)) {
        return false;
    }
    _pos = 0;
    _readonly = readonly;
    return ((SdFile *)_sdfile)->open(&workDir, filename.c_str(), readonly?O_READ:(O_CREAT | O_APPEND | O_WRITE | O_TRUNC));
}

uint32_t ESP_SD::size()
{
    if(((SdFile *)_sdfile)->isOpen()) {
        _size = ((SdFile *)_sdfile)->fileSize();
    }
    return _size ;
}

uint32_t ESP_SD::available()
{
    if(!((SdFile *)_sdfile)->isOpen() || !_readonly) {
        return 0;
    }
    _size = ((SdFile *)_sdfile)->fileSize();
    if (_size == 0) {
        return 0;
    }

    return _size - _pos ;
}

void ESP_SD::close()
{
    if(((SdFile *)_sdfile)->isOpen()) {
        ((SdFile *)_sdfile)->sync();
        _size = ((SdFile *)_sdfile)->fileSize();
        ((SdFile *)_sdfile)->close();
    }
}

int16_t ESP_SD::write(const uint8_t * data, uint16_t len)
{
    return ((SdFile *)_sdfile)->write(data, len);
}

int16_t  ESP_SD::write(const uint8_t byte)
{
    return ((SdFile *)_sdfile)->write(&byte, 1);
}


bool ESP_SD::exists(const char * path)
{
    bool response = open(path);
    if (response) {
        close();
    }
    return response;
}

bool ESP_SD::remove(const char * path)
{
    if (path == NULL) {
        return false;
    }
    String fullpath=path;
    String pathname = fullpath.substring(0,fullpath.lastIndexOf("/"));
    String filename = makeshortname(fullpath.substring(fullpath.lastIndexOf("/")+1));
    if (pathname.length() == 0) {
        pathname="/";
    }
    if (!openDir(pathname)) {
        return false;
    }
    SdFile file;
    return  file.remove(&workDir, filename.c_str());
}

bool ESP_SD::dir_exists(const char * path)
{
    return openDir(path);
}

bool ESP_SD::rmdir(const char * path)
{
    if (path == NULL) {
        return false;
    }
    String fullpath=path;
    if (fullpath=="/") {
        return false;
    }
    if (!openDir(fullpath)) {
        return false;
    }
    return workDir.rmRfStar();
}

bool ESP_SD::mkdir(const char * path)
{
    if (path == NULL) {
        return false;
    }
    String fullpath=path;
    String pathname = fullpath.substring(0,fullpath.lastIndexOf("/"));
    String filename = makeshortname(fullpath.substring(fullpath.lastIndexOf("/")+1));
    if (pathname.length() == 0) {
        pathname="/";
    }
    if (!openDir(pathname)) {
        return false;
    }
    SdFile file;
    return  file.mkdir(&workDir, filename.c_str());
}

int16_t ESP_SD::read()
{
    if (!_readonly) {
        return 0;
    }
    int16_t v = ((SdFile *)_sdfile)->read();
    if (v!=-1) {
        _pos++;
    }
    return v;

}

uint16_t ESP_SD::read(uint8_t * buf, uint16_t nbyte)
{
    if (!_readonly) {
        return 0;
    }
    int16_t v = ((SdFile *)_sdfile)->read(buf, nbyte);
    if (v!=-1) {
        _pos+=v;
    }
    return v;
}

String ESP_SD::get_path_part(String data, int index)
{
    int found = 0;
    int strIndex[] = {0, -1};
    int maxIndex;
    String no_res;
    String s = data;
    s.trim();
    if (s.length() == 0) {
        return no_res;
    }
    maxIndex = s.length()-1;
    if ((s[0] == '/') && (s.length() > 1)) {
        String s2 = &s[1];
        s = s2;
    }
    for(int i=0; i<=maxIndex && found<=index; i++) {
        if(s.charAt(i)=='/' || i==maxIndex) {
            found++;
            strIndex[0] = strIndex[1]+1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }

    return found>index ? s.substring(strIndex[0], strIndex[1]) : no_res;
}

String ESP_SD::makeshortname(String longname, uint8_t index)
{
    String s = longname;
    String part_name;
    String part_ext;
    //Sanity check name is uppercase and no space
    s.replace(" ","");
    s.toUpperCase();
    int pos = s.lastIndexOf(".");
    //do we have extension ?
    if (pos != -1) {
        part_name = s.substring(0,pos);
        if (part_name.lastIndexOf(".") !=-1) {
            part_name.replace(".","");
            //trick for short name but force ~1 at the end
            part_name+="       ";
        }
        part_ext = s.substring(pos+1,pos+4);
    } else {
        part_name = s;
    }
    //check is under 8 char
    if (part_name.length() > 8) {
        //if not cut and use index
        //check file exists is not part of this function
        part_name = part_name.substring(0,6);
        part_name += "~" + String(index);
    }
    //remove the possible " " for the trick
    part_name.replace(" ","");
    //create full short name
    if (part_ext.length() > 0) {
        part_name+="." + part_ext;
    }
    return part_name;
}

String ESP_SD::makepath83(String longpath)
{
    String path;
    String tmp;
    int index = 0;
    tmp = get_path_part(longpath,index);
    while (tmp.length() > 0) {
        path += "/";
        //TODO need to check short name index (~1) match actually the long name...
        path += makeshortname (tmp);
        index++;
        tmp = get_path_part(longpath,index);
    }
    return path;
}


uint64_t ESP_SD::card_total_space()
{

    return (512.00) * (sd_volume.clusterCount()) * (sd_volume.blocksPerCluster());
}
uint64_t ESP_SD::card_used_space()
{
    return (512.00) * (sd_volume.clusterCount() - sd_volume.freeClusterCount() ) * (sd_volume.blocksPerCluster());
}

bool ESP_SD::openDir(String path)
{
    static SdFile root;
    static String name;
    int index = 0;
    //SdFile *parent;
    if(root.isOpen()) {
        root.close();
    }
    if (!sd_volume.init(card.diskIODriver())) {
        return false;
    }
    if (!root.openRoot(&sd_volume)) {
        return false;
    }
    root.rewind();
    workDir = root;
    //parent = &workDir;
    name = get_path_part(path,index);
    while ((name.length() > 0) && (name!="/")) {
        SdFile newDir;
        if (!newDir.open(&root, name.c_str(), O_READ)) {
            return false;
        }
        workDir=newDir;
        //parent = &workDir;
        index++;
        if (index > MAX_DIR_DEPTH) {
            return false;
        }
        name = get_path_part(path,index);
    }
    return true;
}
//TODO may be add date and use a struct for all info
bool ESP_SD::readDir(char name[13], uint32_t * size, bool * isFile)
{
    if ((name == NULL) || (size==NULL)) {
        return false;
    }
    * size = 0;
    name[0]= 0;
    * isFile = false;

    if ((workDir.readDir(&dir_info, NULL)) > 0) {
        workDir.dirName(dir_info,name);
        * size = dir_info.fileSize;
        if (DIR_IS_FILE(&dir_info)) {
            * isFile = true;
        }
        return true;
    }
    return false;
}


//TODO
/*
bool SD_file_timestamp(const char * path, uint8_t flag, uint16_t year, uint8_t month, uint8_t day,
                 uint8_t hour, uint8_t minute, uint8_t second){
}**/

#endif// SDSUPPORT && ESP3D_WIFISUPPORT


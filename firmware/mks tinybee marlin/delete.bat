
for /d %%G in ("%USERPROFILE%\.platformio\.cache*") do rmdir /S /Q "%%~G"
for /d %%G in ("%USERPROFILE%\.platformio\packages\framework-arduinoespressif32*") do rmdir /S /Q "%%~G"
for /d %%G in ("%USERPROFILE%\.platformio\platforms\espressif32*") do rmdir /S /Q "%%~G"

pause enter to close, delete also .pio in marlin directory

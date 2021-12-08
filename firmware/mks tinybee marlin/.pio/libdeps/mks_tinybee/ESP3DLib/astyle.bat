
cd %~dp0src
astyle --recursive --style=otbs *.h *.cpp *.ino
del /S *.ori
dir
cd ..
pause

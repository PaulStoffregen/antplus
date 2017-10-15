@echo off

k:
cd K:\code\Teensy\antplus
call apath.bat

cd %APPATH%
arduino.exe --pref build.path=M:\temp\teensy --board teensy:avr:teensy36:speed=180 --preserve-temp-files --verify K:\code\Teensy\antplus\antplus.ino


pause

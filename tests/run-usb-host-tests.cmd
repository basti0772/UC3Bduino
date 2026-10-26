@echo off
rem SPDX-License-Identifier: MIT
rem Run from repository root in a Visual Studio x64 Native Tools prompt.
if not exist .scratch\usb-tests mkdir .scratch\usb-tests
cl /nologo /W4 /WX /TC /std:c11 tests\usb_protocol_test.c /Fo.scratch\usb-tests\protocol.obj /Fe.scratch\usb-tests\protocol.exe
if errorlevel 1 exit /b 1
.scratch\usb-tests\protocol.exe
if errorlevel 1 exit /b 1
rem The reduced unconfigured entry point intentionally does not call helpers.
cl /nologo /W4 /WX /wd4505 /TC /std:c11 /DTEST_UNCONFIGURED tests\usb_protocol_test.c /Fo.scratch\usb-tests\unconfigured.obj /Fe.scratch\usb-tests\unconfigured.exe
if errorlevel 1 exit /b 1
.scratch\usb-tests\unconfigured.exe
if errorlevel 1 exit /b 1
cl /nologo /W4 /WX /EHsc /I src /I arduino\hardware\uc3bduino\avr32\cores\uc3bduino tests\usb_serial_test.cpp /Fo.scratch\usb-tests\api.obj /Fe.scratch\usb-tests\api.exe
if errorlevel 1 exit /b 1
.scratch\usb-tests\api.exe
if errorlevel 1 exit /b 1
for %%F in (8000000 12000000 16000000 10000000) do (
  cl /nologo /W4 /WX /TC /std:c11 /DUC3B_CLOCK_SOURCE=2 /DUC3B_OSC0_HZ=%%F /DUC3B_OSC0_CRYSTAL=1 tests\usb_clock_test.c /Fo.scratch\usb-tests\clock%%F.obj /Fe.scratch\usb-tests\clock%%F.exe
  if errorlevel 1 exit /b 1
  .scratch\usb-tests\clock%%F.exe
  if errorlevel 1 exit /b 1
)
cl /nologo /W4 /WX /TC /std:c11 tests\usb_clock_test.c /Fo.scratch\usb-tests\clock-rc.obj /Fe.scratch\usb-tests\clock-rc.exe
if errorlevel 1 exit /b 1
.scratch\usb-tests\clock-rc.exe
exit /b %errorlevel%

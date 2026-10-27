@echo off
rem SPDX-License-Identifier: MIT
rem Run from repository root in a Visual Studio x64 Native Tools prompt.
if not exist .scratch\host-tests mkdir .scratch\host-tests
for %%N in (2 64 256) do (
  cl /nologo /W4 /WX /TC /std:c11 /DSERIAL_RX_BUFFER_SIZE=%%N tests\serial_uart_test.c /Fo.scratch\host-tests\uart%%N.obj /Fe.scratch\host-tests\uart%%N.exe
  if errorlevel 1 exit /b 1
  .scratch\host-tests\uart%%N.exe
  if errorlevel 1 exit /b 1
)
for %%N in (serial_print timing_delay) do (
  cl /nologo /W4 /WX /EHsc /I src /I arduino\hardware\uc3bduino\avr32\cores\uc3bduino /I arduino\hardware\uc3bduino\avr32\variants\uc3b0256 tests\%%N_test.cpp /Fo.scratch\host-tests\%%N.obj /Fe.scratch\host-tests\%%N.exe
  if errorlevel 1 exit /b 1
  .scratch\host-tests\%%N.exe
  if errorlevel 1 exit /b 1
)
cl /nologo /W4 /WX /TC /std:c11 tests\timing_snapshot_test.c /Fo.scratch\host-tests\snapshot.obj /Fe.scratch\host-tests\snapshot.exe
if errorlevel 1 exit /b 1
.scratch\host-tests\snapshot.exe
if errorlevel 1 exit /b 1
call tests\run-usb-host-tests.cmd
exit /b %errorlevel%

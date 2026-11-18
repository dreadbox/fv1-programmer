@echo off
REM write_fv1.bat - Write FV1 program to EEPROM (Windows)
REM
REM Usage:
REM   write_fv1.bat <input_file> [COM_port]
REM
REM Arguments:
REM   input_file - FV1 program file to write (must be exactly 4096 bytes)
REM   COM_port   - COM port (e.g., COM3) - optional, will list if not specified
REM

setlocal enabledelayedexpansion

if "%~1"=="" (
    echo Usage: %0 ^<input_file^> [COM_port]
    echo.
    echo Arguments:
    echo   input_file - FV1 program file to write ^(must be exactly 4096 bytes^)
    echo   COM_port   - COM port ^(e.g., COM3^) - optional
    echo.
    echo Example:
    echo   %0 program.fv1 COM3
    exit /b 1
)

set INPUT_FILE=%~1
set COM_PORT=%~2

REM Check if input file exists
if not exist "%INPUT_FILE%" (
    echo Error: Input file '%INPUT_FILE%' not found
    exit /b 1
)

REM If COM port not specified, list available ports
if "%COM_PORT%"=="" (
    echo Available COM ports:
    mode | findstr "COM"
    echo.
    echo Please specify COM port:
    echo   %0 %INPUT_FILE% COMX
    exit /b 1
)

echo Writing FV1 program to %COM_PORT%...
echo Input file: %INPUT_FILE%

REM Configure COM port using mode command
mode %COM_PORT%: BAUD=115200 PARITY=N DATA=8 STOP=1

REM Use PowerShell to send data (more reliable than pure batch)
powershell -Command "& { $port = New-Object System.IO.Ports.SerialPort '%COM_PORT%', 115200, 'None', 8, 'One'; $port.Open(); $port.Write('W'); $data = [System.IO.File]::ReadAllBytes('%INPUT_FILE%'); $port.Write($data, 0, $data.Length); Start-Sleep -Milliseconds 1000; $response = $port.ReadExisting(); $port.Close(); if ($response -match 'OK') { Write-Host 'Success! FV1 program written to EEPROM'; exit 0 } elseif ($response -match 'ERROR') { Write-Host 'Error: Write failed'; exit 1 } else { Write-Host 'Warning: No response from device'; exit 0 } }"

endlocal

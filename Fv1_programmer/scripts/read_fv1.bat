@echo off
REM read_fv1.bat - Read FV1 program from EEPROM (Windows)
REM
REM Usage:
REM   read_fv1.bat [output_file] [COM_port]
REM
REM Arguments:
REM   output_file - Output file to save EEPROM contents (default: current.fv1)
REM   COM_port    - COM port (e.g., COM3) - optional, will list if not specified
REM

setlocal enabledelayedexpansion

set OUTPUT_FILE=%~1
if "%OUTPUT_FILE%"=="" set OUTPUT_FILE=current.fv1

set COM_PORT=%~2

REM If COM port not specified, list available ports
if "%COM_PORT%"=="" (
    echo Available COM ports:
    mode | findstr "COM"
    echo.
    echo Please specify COM port:
    echo   %0 %OUTPUT_FILE% COMX
    exit /b 1
)

echo Reading FV1 program from %COM_PORT%...
echo Output file: %OUTPUT_FILE%

REM Configure COM port using mode command
mode %COM_PORT%: BAUD=115200 PARITY=N DATA=8 STOP=1

REM Use PowerShell to read data
powershell -Command "& { $port = New-Object System.IO.Ports.SerialPort '%COM_PORT%', 115200, 'None', 8, 'One'; $port.ReadTimeout = 5000; $port.Open(); $port.Write('R'); Start-Sleep -Milliseconds 500; $data = New-Object byte[] 4096; $offset = 0; while ($offset -lt 4096) { $read = $port.Read($data, $offset, 4096 - $offset); $offset += $read; }; $port.Close(); [System.IO.File]::WriteAllBytes('%OUTPUT_FILE%', $data); Write-Host 'Done! Saved to %OUTPUT_FILE% (4096 bytes)' }"

endlocal

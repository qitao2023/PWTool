@echo off
echo ============================================
echo   ProjectWise dmawin.dll check
echo ============================================
echo.
echo [1] dmawin.dll in PATH:
where dmawin.dll 2>nul
echo.
echo [2] dmawin.dll under Bentley folders:
dir /s /b "%ProgramFiles%\Bentley\dmawin.dll" 2>nul
dir /s /b "%ProgramFiles(x86)%\Bentley\dmawin.dll" 2>nul
echo.
echo [3] dmawin.dll under ProjectWise folders:
dir /s /b "%ProgramFiles%\ProjectWise\dmawin.dll" 2>nul
dir /s /b "%ProgramFiles(x86)%\ProjectWise\dmawin.dll" 2>nul
echo.
echo ============================================
echo  HOW TO READ THE RESULT:
echo  - If a path is listed above: PW client IS installed.
echo      If path contains "x86" -> 32-bit PW -> use:  Release\PWExampleFroTSZ-VS2017.exe
echo      If path has NO  "x86"  -> 64-bit PW -> use:  x64\Release\PWExampleFroTSZ-VS2017.exe
echo  - If nothing is listed: PW client is NOT installed on this PC.
echo.
echo  TIP: put the exe into the same folder as dmawin.dll and run it.
echo.
pause

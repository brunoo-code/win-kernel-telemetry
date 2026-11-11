@echo off
setlocal
if "%~2"=="" (
  echo Usage: build-driver.cmd WDK_C_DIRECTORY WINDOWS_SDK_DIRECTORY
  exit /b 2
)
where cl >nul 2>&1 || (echo Use the x64 Visual Studio developer command prompt. & exit /b 2)
if /i not "%VSCMD_ARG_TGT_ARCH%"=="x64" (echo Expected x64 compiler environment. & exit /b 2)
set "version=10.0.26100.0"
set "wdk=%~1"
set "sdk=%~2"
set "km=%wdk%\Lib\%version%\km\x64"
pushd "%~dp0.." || exit /b 1
if not exist "build\driver" mkdir "build\driver"
cl /nologo /c /std:c++17 /W4 /kernel /Zl /Zi /Od /GS /GR- /D_AMD64_ ^
  /I"%wdk%\Include\%version%\km" /I"%sdk%\Include\%version%\shared" ^
  /Fo"build\driver\driver.obj" /Fd"build\driver\compile.pdb" "driver\driver.cpp"
if errorlevel 1 goto failed
link /nologo /kernel /driver /subsystem:native /machine:x64 /entry:GsDriverEntry ^
  /nodefaultlib /debug /incremental:no /out:"build\driver\BnqqKernelTelemetry.sys" ^
  /pdb:"build\driver\BnqqKernelTelemetry.pdb" "build\driver\driver.obj" ^
  "%km%\ntoskrnl.lib" "%km%\wdmsec.lib" "%km%\BufferOverflowK.lib"
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1

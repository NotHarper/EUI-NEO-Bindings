@echo off
setlocal enabledelayedexpansion

rem ============================================================
rem EUI-NEO one-click release build (Windows)
rem Output: D:\Code\EUI-NEO\release
rem Usage:  build-release.bat [clean]
rem ============================================================

set SCRIPT_DIR=%~dp0
if "%SCRIPT_DIR:~-1%"=="\" set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%
set BUILD_DIR=%SCRIPT_DIR%\build-release
set RELEASE_DIR=D:\Code\EUI-NEO\release
set JAR=%BUILD_DIR%\eui-neo-java.jar
set DLL=%BUILD_DIR%\eui_neo_jni.dll
set SMOKE_DIR=%BUILD_DIR%\smoke-classes
set SMOKE_SRC=%SCRIPT_DIR%\tests\java\SmokeTest.java

rem ---- optional clean ----
if /I "%1"=="clean" (
    echo [release] Cleaning...
    if exist "%BUILD_DIR%"   rmdir /s /q "%BUILD_DIR%"
    if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%"
)

rem ---- CMake configure ----
if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [release] Configuring...
    cmake -S "%SCRIPT_DIR%" -B "%BUILD_DIR%" ^
        -DEUI_BUILD_APPS=OFF ^
        -DEUI_BUILD_JNI=ON ^
        -DEUI_DEPS_MODE=bundled
    if errorlevel 1 ( echo [release] CONFIGURE FAILED & exit /b 1 )
)

rem ---- Build native + JNI DLL ----
echo [release] Building native...
cmake --build "%BUILD_DIR%" --parallel
if errorlevel 1 ( echo [release] BUILD FAILED & exit /b 1 )

rem ---- Build Java classes + JAR ----
echo [release] Building Java + JAR...
cmake --build "%BUILD_DIR%" --target eui_neo_java_classes
if errorlevel 1 ( echo [release] JAVA BUILD FAILED & exit /b 1 )

rem ---- Smoke test ----
if exist "%SMOKE_SRC%" (
    echo [release] Running smoke test...
    if not exist "%SMOKE_DIR%" mkdir "%SMOKE_DIR%"
    javac --release 17 -encoding UTF-8 -cp "%JAR%" -d "%SMOKE_DIR%" "%SMOKE_SRC%"
    if errorlevel 1 ( echo [release] SMOKE COMPILE FAILED & exit /b 1 )
    java -cp "%JAR%;%SMOKE_DIR%" com.sudoevolve.euineo.SmokeTest
    if errorlevel 1 ( echo [release] SMOKE TEST FAILED & exit /b 1 )
) else (
    echo [release] Smoke test not found, skipping.
)

rem ---- Extract version from source ----
set VERSION=unknown
for /f "tokens=1,* delims==" %%a in ('findstr /c:"kVersion =" "%SCRIPT_DIR%\core\api\neo_c_api.cpp"') do set _V=%%b
if defined _V (
    set VERSION=%_V:"=%
    set VERSION=!VERSION:;=!
    set VERSION=!VERSION: =!
)

rem ---- Package artifacts ----
echo [release] Packaging to %RELEASE_DIR%...
if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%\include\eui"

copy /y "%JAR%" "%RELEASE_DIR%\eui-neo-java.jar" >nul
if errorlevel 1 ( echo [release] Failed to copy JAR & exit /b 1 )

if exist "%DLL%" (
    copy /y "%DLL%" "%RELEASE_DIR%\eui_neo_jni.dll" >nul
    if errorlevel 1 ( echo [release] Failed to copy DLL & exit /b 1 )
) else (
    echo [release] WARNING: DLL not found at %DLL%
)

copy /y "%SCRIPT_DIR%\include\eui\neo_c_api.h" "%RELEASE_DIR%\include\eui\neo_c_api.h" >nul
if errorlevel 1 ( echo [release] Failed to copy header & exit /b 1 )

echo %VERSION%>"%RELEASE_DIR%\VERSION"

rem ---- Verify JAR contains natives ----
echo [release] Verifying JAR...
jar tf "%RELEASE_DIR%\eui-neo-java.jar" | findstr /c:"natives/" >nul
if errorlevel 1 ( echo [release] WARNING: natives/ not found in JAR )

echo.
echo [release] SUCCESS
echo   JAR    : %RELEASE_DIR%\eui-neo-java.jar
echo   DLL    : %RELEASE_DIR%\eui_neo_jni.dll
echo   Header : %RELEASE_DIR%\include\eui\neo_c_api.h
echo   Version: %VERSION%
echo.
endlocal
exit /b 0

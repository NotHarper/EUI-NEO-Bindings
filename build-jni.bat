@echo off
setlocal enabledelayedexpansion

:: ============================================================
:: EUI-NEO JNI 一键构建脚本 (Windows)
:: 用法: build-jni.bat [clean]
::   clean  — 删除 build-jni 目录后重新配置
:: ============================================================

set SCRIPT_DIR=%~dp0
set BUILD_DIR=%SCRIPT_DIR%build-jni
set SMOKE_DIR=%BUILD_DIR%\smoke-classes
set JAR=%BUILD_DIR%\eui-neo-java.jar
set SMOKE_SRC=%SCRIPT_DIR%tests\java\SmokeTest.java

:: ---- 可选 clean ----
if /I "%1"=="clean" (
    echo [build-jni] Removing %BUILD_DIR% ...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
)

:: ---- CMake configure ----
if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [build-jni] Configuring ...
    cmake -S "%SCRIPT_DIR%" -B "%BUILD_DIR%" ^
        -DEUI_BUILD_APPS=OFF ^
        -DEUI_BUILD_JNI=ON ^
        -DEUI_DEPS_MODE=bundled
    if errorlevel 1 ( echo [build-jni] CONFIGURE FAILED & exit /b 1 )
)

:: ---- Build native + JNI DLL ----
echo [build-jni] Building native ...
cmake --build "%BUILD_DIR%" --parallel
if errorlevel 1 ( echo [build-jni] NATIVE BUILD FAILED & exit /b 1 )

:: ---- Build Java classes + JAR ----
echo [build-jni] Building Java classes + JAR ...
cmake --build "%BUILD_DIR%" --target eui_neo_java_classes
if errorlevel 1 ( echo [build-jni] JAVA BUILD FAILED & exit /b 1 )

:: ---- Verify JAR contents ----
echo [build-jni] Verifying JAR ...
jar tf "%JAR%" | findstr /C:"natives/"
if errorlevel 1 ( echo [build-jni] WARNING: natives/ not found in JAR )

:: ---- Smoke test ----
if exist "%SMOKE_SRC%" (
    echo [build-jni] Compiling smoke test ...
    if not exist "%SMOKE_DIR%" mkdir "%SMOKE_DIR%"
    javac --release 17 -encoding UTF-8 ^
        -cp "%JAR%" ^
        -d "%SMOKE_DIR%" ^
        "%SMOKE_SRC%"
    if errorlevel 1 ( echo [build-jni] SMOKE COMPILE FAILED & exit /b 1 )

    echo [build-jni] Running smoke test ...
    java -cp "%JAR%;%SMOKE_DIR%" com.sudoevolve.euineo.SmokeTest
    if errorlevel 1 ( echo [build-jni] SMOKE TEST FAILED & exit /b 1 )
) else (
    echo [build-jni] Smoke test not found at %SMOKE_SRC%, skipping.
)

echo.
echo [build-jni] SUCCESS
echo   DLL : %BUILD_DIR%\eui_neo_jni.dll
echo   JAR : %JAR%
echo.
endlocal

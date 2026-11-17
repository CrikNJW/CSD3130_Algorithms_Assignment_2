@echo off
setlocal

pushd "%~dp0"
if errorlevel 1 (
    echo Could not open the seam carver project folder.
    pause
    exit /b 1
)

set "CMAKE_EXE="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 Microsoft.VisualStudio.Component.VC.CMake.Project -find Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`) do set "CMAKE_EXE=%%I"
)
if not defined CMAKE_EXE (
    for /f "delims=" %%I in ('where.exe $PATH:cmake.exe 2^>nul') do if not defined CMAKE_EXE set "CMAKE_EXE=%%I"
)
if not defined CMAKE_EXE (
    echo CMake was not found. In Visual Studio Installer, install Desktop development with C++
    echo and make sure C++ CMake tools for Windows is selected.
    goto failed
)

echo Configuring Seam Carver...
"%CMAKE_EXE%" -S . -B build -A x64
if errorlevel 1 goto failed

echo Building Seam Carver...
"%CMAKE_EXE%" --build build --config Release --target seam-carver --parallel 1 -- /nr:false
if errorlevel 1 goto failed

echo Launching Seam Carver...
"build\Release\seam-carver.exe"
if errorlevel 1 goto failed
popd
exit /b 0

:failed
echo Seam Carver stopped with an error. See the message above.
pause
popd
exit /b 1

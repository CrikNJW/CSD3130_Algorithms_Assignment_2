@echo off
setlocal DisableDelayedExpansion

set "BUILD_DIR=%~dp0build"
if not exist "%BUILD_DIR%\" (
    echo No build folder to clean.
    exit /b 0
)

echo Removing "%BUILD_DIR%"...
rmdir /s /q "%BUILD_DIR%"
if exist "%BUILD_DIR%\" (
    echo Could not remove the entire build folder. Close any programs using it and try again. 1>&2
    exit /b 1
)

echo Build folder removed.
exit /b 0

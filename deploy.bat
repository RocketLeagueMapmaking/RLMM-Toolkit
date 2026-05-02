@echo off
setlocal

set "QTDIR=F:\Qt\6.11.0\msvc2022_64"
set "VERSION=0.0.0"

if exist dist rmdir /s /q dist
mkdir dist

copy /y x64\Release\RLMMToolkit.exe dist\ || exit /b 1
copy /y x64\Release\RLMMUpdater.exe dist\ || exit /b 1
"%QTDIR%\bin\windeployqt.exe" dist\RLMMToolkit.exe --release --no-translations || exit /b 1

makensis /DVERSION=%VERSION% installer.nsi || exit /b 1
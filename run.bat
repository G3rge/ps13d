@echo off
setlocal
set PATH=C:\msys64\ucrt64\bin;%PATH%
cd /d %~dp0
ps13d.exe %*
@echo off
call "%~1\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
pushd "%~dp0.."
if not exist artifacts\native mkdir artifacts\native
rc /nologo /fo artifacts\native\app.res native\app.rc
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /MT /O2 /utf-8 /W4 /WX /EHsc /DUNICODE /D_UNICODE native\app.cpp artifacts\native\app.res /Foartifacts\native\app.obj /Feartifacts\native\Miao.exe /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:native\app.manifest hid.lib setupapi.lib gdiplus.lib shell32.lib comctl32.lib user32.lib gdi32.lib advapi32.lib ole32.lib
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /MT /O2 /utf-8 /W4 /WX /EHsc /DUNICODE /D_UNICODE native\tests.cpp /Foartifacts\native\tests.obj /Feartifacts\native\native-tests.exe /link hid.lib setupapi.lib
if errorlevel 1 exit /b 1
artifacts\native\native-tests.exe
set result=%errorlevel%
popd
exit /b %result%

@echo off

if %1.==. goto No1

mkdir ..\..\build
mkdir ..\..\build\handmade
pushd ..\..\build\handmade
rem cl /Zi /D "_UNICODE" /D "UNICODE" ..\handmade\code\win32_handmade.cpp user32.lib
if "%2" == "-g" (
   cmake -G "Visual Studio 14 2015 Win64" ..\..\handmade
)
cmake --build . --config %1
popd

goto End1

:No1
    echo "Usage: build Release|Debug|RelWithDebInfo|MinSizeRel [-g]"
goto End1

:End1

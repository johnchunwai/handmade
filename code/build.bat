@echo off

mkdir ..\..\build
pushd ..\..\build
cl /Zi /D "_UNICODE" /D "UNICODE" ..\handmade\code\win32_handmade.cpp user32.lib
popd

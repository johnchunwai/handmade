@echo off

REM
REM To run this at startup. use this as your shortcut target:
REM %comspec% /k "w:\handmade\misc\shell.bat"
REM

call "%ProgramFiles(x86)%\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64
set path=w:\handmade\misc;%path%
REM setting home for emacs to find the .emacs file, also for git to store global config
set home=w:\home
cd w:\handmade\code

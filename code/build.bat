@echo off

REM ------------- Run vcvars64.bat to get access to cl and devenv -------------
set CC=cl
set DB=devenv
REM ---------------------------------------------------------------------------

set CFLAGS=-Zi /EHsc /std:c++latest /DEBUG:FULL

set DEF=/DGLM_ENABLE_EXPERIMENTAL /DUNITY_BUILD /DSTB_IMAGE_IMPLEMENTATION /D_MBCS
REM set GLFW_INC_DIR=/I C:/dependencies/glfw-3.2.1.bin.WIN64/include



REM -------------- TO MODIFY -------------------
set GLM_INC_DIR=/I C:/dependencies/
set VULKAN_INC_DIR=/I C:/VulkanSDK/1.1.108.0/Include
set STB_INC_DIR=/I C:/dependencies/stb-master/
set LUA_INC_DIR=/I C:/dependencies/Lua/include/
set VML_INC_DIR=/I C:/dependencies/vml/vml/
REM --------------------------------------------



set INC=%GLM_INC_DIR% %VULKAN_INC_DIR% %LUA_INC_DIR% %VML_INC_DIR% %STB_INC_DIR%
 
set BIN=Saska.exe
set DLL_NAME=Saska.dll

set SRC=win32_core.cpp
set DLL_SRC=game.cpp

REM Don't need GLFW for now: C:/dependencies/glfw-3.2.1.bin.WIN64/lib-vc2015/glfw3.lib
REM If link errors appear, maybe add these libs into the list: Shell32.lib kernel32.lib msvcmrt.lib 
set LIBS=ws2_32.lib winmm.lib user32.lib gdi32.lib msvcrt.lib C:/VulkanSDK/1.1.108.0/Lib/vulkan-1.lib C:/dependencies/Lua/lib/lua5.1.lib

If "%1" == "compile" goto compile
If "%1" == "debug" goto debug
If "%1" == "clean" goto clean
If "%1" == "run" goto run
If "%1" == "help" goto help

:compile
%CC% %CFLAGS% %DEF% %INC% /Fe%BIN% %SRC% %LIBS%

etags *.cpp *.hpp
echo Built emacs tags

goto :eof

:debug
%DB% %BIN%
goto :eof

:clean
rm *.exe *.obj *.ilk *.pdb TAGS
goto :eof

:run
%BIN%
goto :eof


:help
echo To build application, enter into command line: build.bat compile
echo To debug application, enter into command line: build.bat debug
echo To run application, enter into command line: build.bat run
echo To clean application, enter into command line: build.bat clean

:eof

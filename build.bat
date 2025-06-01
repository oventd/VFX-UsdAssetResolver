REM Clear current session log 
cls
REM Source environment (Uncomment lines starting with "set" if you current env does not have these defined.)
set HFS=C:\Program Files\Side Effects Software\Houdini 20.0.896
REM Define Resolver > Has to be one of 'fileResolver'/'pythonResolver'/'cachedResolver'/'httpResolver'
set AR_RESOLVER_NAME=fileResolver
REM Define App
set AR_DCC_NAME=HOUDINI
REM Clear existing build data and invoke cmake
rmdir /S /Q build
rmdir /S /Q dist
REM Make sure to match the correct VS version the DCC was built with
cmake . -B build -G "Visual Studio 17 2022" -A x64 -T v143
cmake --build build  --clean-first --config Release
cmake --install build
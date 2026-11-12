@echo off
setlocal
if "%~2"=="" exit /b 1
set "audit_root=%~1"
set "audit_build=%~2"
if not exist "%audit_build%" mkdir "%audit_build%"
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x86
if errorlevel 1 exit /b %errorlevel%
where cl
pushd "%audit_build%"
cl /nologo /std:c++14 /EHsc /W4 /WX /I"%audit_root%\include" "%audit_root%\docs\nav-bsp-feasibility\harness\production_probe.cpp" "%audit_root%\src\core\nav\legacy_nav_reader.cpp" "%audit_root%\src\core\nav\nav_model.cpp" "%audit_root%\src\core\nav\nav_snapshot.cpp" "%audit_root%\src\core\nav\nav_query.cpp" /Fe:production_probe.exe
set "audit_result=%errorlevel%"
popd
exit /b %audit_result%

@echo off
REM Compila y ejecuta el nucleo de generacion SIN Unreal.
REM
REM Esto es lo que demuestra que los algoritmos son propios y no
REM configuracion del motor: corren solos, en una consola.
REM
REM Uso: doble clic, o ejecutar desde cmd en esta carpeta.

setlocal
cd /d "%~dp0"

set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
set "CORE=..\Source\Jardin\Core"

if not exist "%VCVARS%" (
  echo No se encontro vcvars64.bat en:
  echo   %VCVARS%
  echo Ajusta la variable VCVARS si tu Visual Studio esta en otro sitio.
  pause
  exit /b 1
)

call "%VCVARS%" >nul 2>&1

REM vcvars64.bat cambia el directorio actual: hay que volver.
cd /d "%~dp0"

REM Los .obj caen en esta carpeta porque ya estamos dentro de ella.
cl /nologo /std:c++17 /W4 /O2 /EHsc /I"%CORE%" ^
   "%CORE%\JardinCore.cpp" "%CORE%\JardinWorld.cpp" "%CORE%\JardinWater.cpp" "%CORE%\JardinPath.cpp" "%CORE%\JardinGrass.cpp" "%CORE%\JardinScatter.cpp" ^
   HarnessCore.cpp ^
   /Fe:jardin_test.exe

if errorlevel 1 (
  echo.
  echo FALLO LA COMPILACION
  pause
  exit /b 1
)

echo.
echo ===================== TERRENO, ZONAS Y COLOCACION =====================
"%~dp0jardin_test.exe"
set RESULT=%ERRORLEVEL%

cl /nologo /std:c++17 /W4 /O2 /EHsc /I"%CORE%" ^
   "%CORE%\JardinCore.cpp" "%CORE%\JardinFractalTree.cpp" ^
   HarnessFractal.cpp ^
   /Fe:fractal_test.exe

if errorlevel 1 (
  echo.
  echo FALLO LA COMPILACION DEL ARBOL FRACTAL
  pause
  exit /b 1
)

echo.
echo ============================ ARBOL FRACTAL ============================
"%~dp0fractal_test.exe"
if errorlevel 1 set RESULT=1

echo.
pause
exit /b %RESULT%

@echo off
REM ECO-WATT API Documentation Generator (preserves docs\docs)
setlocal

REM Always run from this script's folder (which is docs\)
cd /d "%~dp0"

echo.
echo ========================================
echo   ECO-WATT API Documentation Generator
echo ========================================
echo.

REM Check if Doxygen is installed
where doxygen >nul 2>&1 || (
    echo ERROR: Doxygen is not installed or not in PATH
    echo Install: https://www.doxygen.nl/download.html  (choco install doxygen.install / scoop install doxygen)
    echo.
    pause
    exit /b 1
)

echo ✓ Doxygen found
doxygen --version
echo.

REM Clean previous top-level outputs ONLY (keep docs\docs intact)
for %%D in ("html" "latex") do (
    if exist %%~D (
        echo Cleaning %%~D...
        rmdir /s /q %%~D
    )
)

echo.
echo Generating API documentation...
echo.

REM Generate
doxygen Doxyfile || goto :fail

echo.
echo ========================================
echo   Documentation Generated Successfully!
echo ========================================
echo.
echo HTML Documentation: .\html\index.html
echo LaTeX Directory   : .\latex\
echo.

set /p "open=Open documentation in browser? (y/n): "
if /i "%open%"=="y" if exist ".\html\index.html" start "" ".\html\index.html"

set /p "pdf=Generate PDF documentation? (y/n): "
if /i "%pdf%"=="y" (
    echo.
    echo Generating PDF documentation...
    if exist ".\latex\make.bat" (
        pushd ".\latex"
        call make.bat
        if exist "refman.pdf" (
            echo ✓ PDF generated: .\latex\refman.pdf
            start "" "refman.pdf"
        ) else (
            echo ⚠ PDF generation failed - LaTeX may not be installed
        )
        popd
    ) else (
        echo ⚠ LaTeX makefile not found
    )
)

echo.
pause
exit /b 0

:fail
echo.
echo ========================================
echo   Documentation Generation Failed!
echo ========================================
echo.
echo Please check the Doxyfile and your sources.
echo Common issues: bad paths, missing files, or comment syntax errors.
echo.
pause
exit /b 1

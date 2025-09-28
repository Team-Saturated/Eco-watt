@echo off
REM ECO-WATT API Documentation Generator
REM This script generates comprehensive API documentation using Doxygen

echo.
echo ========================================
echo   ECO-WATT API Documentation Generator
echo ========================================
echo.

REM Check if Doxygen is installed
where doxygen >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Doxygen is not installed or not in PATH
    echo.
    echo Please install Doxygen from: https://www.doxygen.nl/download.html
    echo Or install via Chocolatey: choco install doxygen.install
    echo Or install via Scoop: scoop install doxygen
    echo.
    pause
    exit /b 1
)

echo ✓ Doxygen found
doxygen --version
echo.

REM Clean previous documentation
if exist docs\html (
    echo Cleaning previous HTML documentation...
    rmdir /s /q docs\html
)

if exist docs\latex (
    echo Cleaning previous LaTeX documentation...
    rmdir /s /q docs\latex
)

echo.
echo Generating API documentation...
echo.

REM Generate documentation
doxygen Doxyfile

if %errorlevel% equ 0 (
    echo.
    echo ========================================
    echo   Documentation Generated Successfully!
    echo ========================================
    echo.
    echo HTML Documentation: docs\html\index.html
    echo LaTeX Documentation: docs\latex\
    echo.
    
    REM Ask user if they want to open the documentation
    set /p "open=Open documentation in browser? (y/n): "
    if /i "%open%"=="y" (
        start docs\html\index.html
    )
    
    REM Ask user if they want to generate PDF
    set /p "pdf=Generate PDF documentation? (y/n): "
    if /i "%pdf%"=="y" (
        echo.
        echo Generating PDF documentation...
        cd docs\latex
        if exist make.bat (
            call make.bat
            if exist refman.pdf (
                echo ✓ PDF generated: docs\latex\refman.pdf
                start refman.pdf
            ) else (
                echo ⚠ PDF generation failed - LaTeX may not be installed
            )
        ) else (
            echo ⚠ LaTeX makefile not found
        )
        cd ..\..
    )
    
) else (
    echo.
    echo ========================================
    echo   Documentation Generation Failed!
    echo ========================================
    echo.
    echo Please check the Doxyfile configuration and try again.
    echo Common issues:
    echo - Missing source files
    echo - Invalid file paths
    echo - Doxygen syntax errors in comments
)

echo.
pause
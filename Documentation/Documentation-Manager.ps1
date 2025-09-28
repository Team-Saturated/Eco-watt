# ECO-WATT API Documentation Generator (PowerShell)
# Enhanced script with better error handling and features

param(
    [switch]$Clean,
    [switch]$OpenAfter,
    [switch]$GeneratePDF,
    [switch]$Help
)

# Display help
if ($Help) {
    Write-Host @"
ECO-WATT API Documentation Generator

USAGE:
    .\Documentation-Manager.ps1 [OPTIONS]

OPTIONS:
    -Clean        Clean previous documentation before generating
    -OpenAfter    Automatically open documentation in browser
    -GeneratePDF  Generate PDF documentation (requires LaTeX)
    -Help         Show this help message

EXAMPLES:
    .\Documentation-Manager.ps1                 # Basic generation
    .\Documentation-Manager.ps1 -Clean -OpenAfter    # Clean and open
    .\Documentation-Manager.ps1 -GeneratePDF         # Generate with PDF

"@
    exit 0
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "   ECO-WATT API Documentation Generator" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if Doxygen is installed
$doxygenPath = Get-Command doxygen -ErrorAction SilentlyContinue
if (-not $doxygenPath) {
    Write-Host "❌ ERROR: Doxygen is not installed or not in PATH" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please install Doxygen using one of these methods:" -ForegroundColor Yellow
    Write-Host "• Download from: https://www.doxygen.nl/download.html"
    Write-Host "• Chocolatey: choco install doxygen.install"
    Write-Host "• Scoop: scoop install doxygen"
    Write-Host "• Winget: winget install DimitriVanHeesch.Doxygen"
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host "✅ Doxygen found: $($doxygenPath.Source)" -ForegroundColor Green
$version = & doxygen --version
Write-Host "📦 Version: $version" -ForegroundColor Green
Write-Host ""

# Clean previous documentation if requested
if ($Clean) {
    Write-Host "🧹 Cleaning previous documentation..." -ForegroundColor Yellow
    
    if (Test-Path "docs\html") {
        Remove-Item -Recurse -Force "docs\html"
        Write-Host "   Removed HTML documentation" -ForegroundColor Gray
    }
    
    if (Test-Path "docs\latex") {
        Remove-Item -Recurse -Force "docs\latex"
        Write-Host "   Removed LaTeX documentation" -ForegroundColor Gray
    }
    
    Write-Host ""
}

# Check if Doxyfile exists
if (-not (Test-Path "Doxyfile")) {
    Write-Host "❌ ERROR: Doxyfile not found in current directory" -ForegroundColor Red
    Write-Host "Please run this script from the project root directory." -ForegroundColor Yellow
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host "🚀 Generating API documentation..." -ForegroundColor Cyan
Write-Host ""

# Generate documentation
$process = Start-Process -FilePath "doxygen" -ArgumentList "Doxyfile" -NoNewWindow -Wait -PassThru

if ($process.ExitCode -eq 0) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "   Documentation Generated Successfully!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""
    
    # Show file locations
    $htmlPath = Resolve-Path "docs\html\index.html" -ErrorAction SilentlyContinue
    $latexPath = Resolve-Path "docs\latex" -ErrorAction SilentlyContinue
    
    if ($htmlPath) {
        Write-Host "📄 HTML Documentation: $htmlPath" -ForegroundColor Cyan
    }
    
    if ($latexPath) {
        Write-Host "📑 LaTeX Documentation: $latexPath" -ForegroundColor Cyan
    }
    
    Write-Host ""
    
    # Count generated files
    $htmlFiles = Get-ChildItem "docs\html" -Recurse -File | Measure-Object
    Write-Host "📊 Generated $($htmlFiles.Count) HTML files" -ForegroundColor Gray
    
    # Open documentation if requested
    if ($OpenAfter -and $htmlPath) {
        Write-Host "🌐 Opening documentation in browser..." -ForegroundColor Yellow
        Start-Process $htmlPath
    }
    
    # Generate PDF if requested
    if ($GeneratePDF -and (Test-Path "docs\latex")) {
        Write-Host ""
        Write-Host "📄 Generating PDF documentation..." -ForegroundColor Yellow
        
        Push-Location "docs\latex"
        try {
            if (Test-Path "make.bat") {
                $pdfProcess = Start-Process -FilePath "make.bat" -NoNewWindow -Wait -PassThru
                
                if (Test-Path "refman.pdf") {
                    $pdfPath = Resolve-Path "refman.pdf"
                    Write-Host "✅ PDF generated: $pdfPath" -ForegroundColor Green
                    
                    if ($OpenAfter) {
                        Start-Process $pdfPath
                    }
                } else {
                    Write-Host "⚠️  PDF generation failed - LaTeX may not be installed" -ForegroundColor Yellow
                    Write-Host "   Install MiKTeX or TeX Live to generate PDF documentation" -ForegroundColor Gray
                }
            } else {
                Write-Host "⚠️  LaTeX makefile not found" -ForegroundColor Yellow
            }
        }
        finally {
            Pop-Location
        }
    }
    
    # Show summary
    Write-Host ""
    Write-Host "🎉 Documentation generation completed!" -ForegroundColor Green
    
    if (-not $OpenAfter -and $htmlPath) {
        $open = Read-Host "Open documentation in browser? (y/n)"
        if ($open -eq "y" -or $open -eq "Y") {
            Start-Process $htmlPath
        }
    }
    
} else {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "   Documentation Generation Failed!" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
    Write-Host ""
    Write-Host "❌ Doxygen exited with code: $($process.ExitCode)" -ForegroundColor Red
    Write-Host ""
    Write-Host "Common issues:" -ForegroundColor Yellow
    Write-Host "• Missing source files in INPUT directories" -ForegroundColor Gray
    Write-Host "• Invalid file paths in Doxyfile" -ForegroundColor Gray
    Write-Host "• Doxygen syntax errors in source comments" -ForegroundColor Gray
    Write-Host "• Insufficient permissions to create docs directory" -ForegroundColor Gray
    Write-Host ""
    Write-Host "💡 Check the Doxyfile configuration and source code comments" -ForegroundColor Cyan
}

Write-Host ""
Read-Host "Press Enter to exit"
#!/bin/bash
# ECO-WATT API Documentation Generator (Linux/macOS)
# This script generates comprehensive API documentation using Doxygen

echo ""
echo "========================================"
echo "  ECO-WATT API Documentation Generator"
echo "========================================"
echo ""

# Check if Doxygen is installed
if ! command -v doxygen &> /dev/null; then
    echo "❌ ERROR: Doxygen is not installed or not in PATH"
    echo ""
    echo "Please install Doxygen:"
    echo "  Ubuntu/Debian: sudo apt-get install doxygen"
    echo "  CentOS/RHEL:   sudo yum install doxygen"
    echo "  Fedora:        sudo dnf install doxygen"
    echo "  macOS:         brew install doxygen"
    echo "  Arch Linux:    sudo pacman -S doxygen"
    echo ""
    exit 1
fi

echo "✅ Doxygen found"
doxygen --version
echo ""

# Clean previous documentation
if [ -d "html" ]; then
    echo "🧹 Cleaning previous HTML documentation..."
    rm -rf html
fi

if [ -d "latex" ]; then
    echo "🧹 Cleaning previous LaTeX documentation..."
    rm -rf latex
fi

echo ""
echo "🔨 Generating API documentation..."
echo ""

# Generate documentation
doxygen Doxyfile

if [ $? -eq 0 ]; then
    echo ""
    echo "========================================"
    echo "  Documentation Generated Successfully!"
    echo "========================================"
    echo ""
    echo "📁 HTML Documentation: docs/html/index.html"
    echo "📁 LaTeX Documentation: docs/latex/"
    echo ""
    
    # Ask user if they want to open documentation
    read -p "🌐 Open documentation in browser? (y/n): " open_browser
    if [[ $open_browser == "y" || $open_browser == "Y" ]]; then
        if command -v xdg-open &> /dev/null; then
            xdg-open html/index.html
        elif command -v open &> /dev/null; then
            open html/index.html
        else
            echo "ℹ️  Please manually open: html/index.html"
        fi
    fi
    
    # Ask user if they want to generate PDF
    read -p "📄 Generate PDF documentation? (y/n): " gen_pdf
    if [[ $gen_pdf == "y" || $gen_pdf == "Y" ]]; then
        if [ -d "latex" ]; then
            echo "📄 Generating PDF documentation..."
            cd latex
            if command -v make &> /dev/null; then
                make pdf
                if [ $? -eq 0 ]; then
                    echo "✅ PDF generated: latex/refman.pdf"
                else
                    echo "❌ PDF generation failed. Check if LaTeX is installed."
                fi
            else
                echo "❌ Make command not found. Please install build tools."
            fi
            cd ..
        else
            echo "❌ LaTeX directory not found. PDF generation skipped."
        fi
    fi
    
    echo ""
    echo "✨ Documentation generation complete!"
    echo "📖 View at: html/index.html"
    
else
    echo ""
    echo "❌ Documentation generation failed!"
    echo "Please check the Doxyfile configuration and try again."
    exit 1
fi

echo ""
echo "Press Enter to continue..."
read
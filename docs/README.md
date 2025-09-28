# EcoWatt Documentation

Welcome to the EcoWatt project documentation! This folder contains comprehensive API documentation, guides, and technical references for the EcoWatt solar inverter monitoring system.

## 📚 Documentation Overview

This documentation system provides complete API references, setup guides, and technical documentation for all components of the EcoWatt system.

### Available Documentation

- **API Documentation** - Complete C++ class and function references
- **Server Documentation** - Flask server and MQTT integration guides
- **Setup Guides** - Installation and configuration instructions
- **Technical References** - Architecture and implementation details

## 🚀 Quick Setup Guide

### Prerequisites

Before viewing documentation, ensure you have:
- **Doxygen** installed for generating API docs
- **Web browser** for viewing HTML documentation
- **Python 3.x** for server components (optional)

### Viewing Documentation

#### 1. Generate Documentation
```bash
# Navigate to docs directory
cd docs

# Windows - Run batch file
.\generate_docs.bat

# Linux/macOS - Run shell script
chmod +x generate_docs.sh
./generate_docs.sh

# Or use PowerShell script (Windows)
.\Documentation-Manager.ps1
```

#### 2. Open Documentation
```bash
# Windows - Open in default browser
start html\index.html

# Linux - Open in default browser
xdg-open html/index.html

# macOS - Open in default browser
open html/index.html

# Or navigate directly to: docs/html/index.html
```

#### 3. Browse Documentation
- **Main Index**: `html/index.html` - Start here
- **Classes**: Browse all C++ classes and their methods
- **Files**: Source code with syntax highlighting
- **Modules**: Organized by functionality

### Command Reference

| Platform | Command | Description |
|----------|---------|-------------|
| **Windows** | `.\generate_docs.bat` | Generate complete API documentation |
| **Linux/macOS** | `./generate_docs.sh` | Generate complete API documentation |
| **Windows** | `start html\index.html` | Open documentation in browser |
| **Linux** | `xdg-open html/index.html` | Open documentation in browser |
| **macOS** | `open html/index.html` | Open documentation in browser |
| **Windows** | `.\Documentation-Manager.ps1` | PowerShell documentation manager |

### Regenerating Documentation
Documentation should be regenerated when:
- Code changes are made
- New classes or functions are added
- Comments are updated
- Configuration changes occur

```bash
# Clean and regenerate
cd docs

# Windows
.\generate_docs.bat

# Linux/macOS
./generate_docs.sh
```

### Documentation Updates
- **Automatic**: Code comments are automatically included
- **Manual**: Update `mainpage.md` for overview changes
- **Configuration**: Modify `Doxyfile` for generation settings

##  Support

For documentation issues or questions:
- Check the main project README: `../README.md`
- Review server documentation: `../Server/README.md`
- Browse generated docs: `html/index.html`
- Visit project repository: [GitHub](https://github.com/Team-Saturated/Eco-watt)

---

*Last updated: September 28, 2025*
*Generated documentation includes 100+ pages of comprehensive API references*
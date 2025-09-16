# Developer Guide - Project Restructure

## Overview

This project has been restructured from a flat file organization into a modular architecture. This guide explains the new structure and what developers need to know.

## New Project Structure

```
Invenesisapp/
├── CMakeLists.txt                 # Root build configuration
├── app/                          # Main application entry point
│   ├── CMakeLists.txt
│   └── main.cpp
├── src/                          # Core application modules
│   ├── authentication/           # Login and user management
│   ├── database/                # Database connectivity and models
│   ├── ui/                      # Main UI windows and dialogs
│   ├── plate_management/        # Plate widgets and containers
│   ├── tecan_integration/       # Tecan-specific functionality
│   └── common/                  # Shared utilities and components
├── external/                    # Third-party libraries
│   └── openwall_crypt/         # Password hashing library
├── resources/                   # Application resources
│   ├── icons/                  # Icon files (moved from icon/)
│   ├── data/                   # JSON files (moved from jsonfile/)
│   └── ui/                     # Shared UI resources
└── build/                      # Build output directory
```

## Module Dependencies

- **app** → links all modules
- **ui** → depends on database, plate_management, tecan_integration, common
- **tecan_integration** → depends on plate_management, database, common
- **plate_management** → depends on common
- **authentication** → depends on common, openwall_crypt
- **database** → depends on common
- **common** → depends on Qt libraries only
- **external/openwall_crypt** → no dependencies

## Building the Project

### Prerequisites
- CMake 3.16 or higher
- Qt5 or Qt6 (Core, Widgets, Sql components)
- C++17 compatible compiler

### Build Commands
```bash
# Configure the build
cmake -B build -G Ninja  # or your preferred generator

# Build the project
cmake --build build

# Run the application
./build/bin/Invenesisapp  # Linux/macOS
./build/bin/Invenesisapp.exe  # Windows
```

### Building Individual Modules
You can now build individual modules for faster development:
```bash
cmake --build build --target authentication
cmake --build build --target database
# etc.
```

## Key Changes for Developers

### 1. Include Paths
- All modules are accessible via their module name
- Example: `#include "authentication/logindialog.h"`
- The root `src/` directory is in the include path

### 2. Resource Files
- Icons moved from `icon/` to `resources/icons/`
- JSON data moved from `jsonfile/` to `resources/data/`
- Resource paths in code should use the Qt resource system: `:/icons/filename.png`

### 3. CMake Targets
Each module is now a separate CMake target:
- `authentication` - Login and password management
- `database` - Database connectivity and models
- `ui` - Main application windows
- `plate_management` - Plate widgets and functionality
- `tecan_integration` - Tecan-specific features
- `common` - Shared utilities
- `openwall_crypt` - External password hashing library

### 4. Adding New Files
When adding new files to a module:
1. Place source files in the appropriate module directory
2. Update the module's `CMakeLists.txt` to include the new files
3. Ensure proper dependencies are declared

### 5. Qt-Specific Files
- `.ui` files should be placed in the same directory as their corresponding source files
- MOC, UIC, and RCC are handled automatically by CMake
- No need to manually run Qt tools

## Development Workflow

### Adding a New Feature
1. Determine which module the feature belongs to
2. Add source files to the appropriate module directory
3. Update the module's CMakeLists.txt
4. Add any new dependencies between modules if needed

### Debugging Build Issues
1. Check that all required Qt components are found
2. Verify module dependencies are correctly declared
3. Ensure include paths are correct
4. Check that all source files are listed in CMakeLists.txt

### IDE Integration
- The project works with Qt Creator, Visual Studio, CLion, and other CMake-compatible IDEs
- The main executable target is `Invenesisapp`
- All modules can be built and debugged individually

## Migration Notes

### What Was Moved
- All icon files: `icon/` → `resources/icons/`
- JSON data files: `jsonfile/` → `resources/data/`
- Source files organized by functionality into `src/` subdirectories
- Main application moved to `app/` directory

### What Was Removed
- `CMakeLists.txt.user` (IDE-specific file)
- `CMakeLists_new.txt` (temporary file)
- Old `icon/` and `jsonfile/` directories

### Compatibility
- All original functionality is preserved
- Resource loading paths updated to use new structure
- Build system supports both Qt5 and Qt6

## Troubleshooting

### Common Issues
1. **Missing Qt components**: Ensure Qt5 or Qt6 is properly installed with Core, Widgets, and Sql components
2. **Build failures**: Check that all CMakeLists.txt files are present and properly configured
3. **Resource loading issues**: Verify that resources.qrc reflects the new directory structure
4. **Include errors**: Make sure include paths use the new module structure

### Getting Help
- Check the CMake configuration output for any warnings or errors
- Verify that all module dependencies are correctly declared
- Ensure that Qt's AUTOMOC, AUTOUIC, and AUTORCC are working correctly

## Performance Benefits

The new modular structure provides:
- **Faster incremental builds**: Changes to one module don't rebuild everything
- **Better parallelization**: Multiple modules can be built simultaneously
- **Cleaner dependencies**: Clear separation of concerns between modules
- **Easier maintenance**: Related code is grouped together logically
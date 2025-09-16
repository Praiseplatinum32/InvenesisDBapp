# Include Path Fixes Summary

## Changes Made

### 1. Updated CMakeLists.txt Include Directories
- Added `${CMAKE_SOURCE_DIR}/src` to all module CMakeLists.txt files
- This allows modules to include each other using paths like `module_name/header.h`

### 2. Fixed Include Statements
- Changed relative paths like `../authentication/logindialog.h` to `authentication/logindialog.h`
- Updated all cross-module includes to use the new module-based paths
- Fixed UI form includes (removed `./` prefix)

### 3. Added Missing Qt::Sql Dependencies
- Added Qt::Sql to authentication, ui, and tecan_integration modules
- These modules use QSqlQuery, QSqlDatabase, and other SQL components

### 4. Fixed Specific Files
- `src/tecan_integration/tecanwindow.cpp`: Fixed PlateMapDialog and daughterplatewidget includes
- `src/ui/databaseviewwindow.h`: Fixed customproxymodel include
- `src/ui/databaseviewwindow.cpp`: Fixed authentication and tecan_integration includes
- `src/authentication/logindialog.cpp`: Fixed ClickableLabel include
- `src/authentication/qtbcrypt.cpp`: Removed duplicate include
- `app/main.cpp`: Updated to use module-based includes

### 5. Maintained Correct Paths
- External library includes (openwall_crypt) kept as relative paths since they're outside src/
- UI form includes (ui_*.h) kept as local includes for AUTOUIC processing
- Qt system includes unchanged

## Result
All modules can now properly include headers from other modules using clean, module-based paths. The build system is configured to find all necessary headers and dependencies.
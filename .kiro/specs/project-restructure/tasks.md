# Implementation Plan

- [x] 1. Create directory structure and prepare build system





  - Create the new modular directory structure with all required folders
  - Set up basic CMakeLists.txt files for each module with minimal configuration
  - _Requirements: 1.1, 1.2, 2.1, 2.2, 5.1, 5.2_

- [x] 2. Move and organize external dependencies





  - Move openwall_crypt directory to external/ folder
  - Create CMakeLists.txt for openwall_crypt module as a static library
  - Update include paths and ensure the library builds independently
  - _Requirements: 3.3, 2.2, 2.3_

- [x] 3. Create and populate common utilities module





  - Create src/common/ directory structure
  - Move any shared utility files to common module (if identified during analysis)
  - Create CMakeLists.txt for common module with Qt::Core and Qt::Widgets dependencies
  - Set up proper include directories and export headers
  - _Requirements: 3.1, 2.2, 5.2, 5.3_

- [x] 4. Implement authentication module







  - Move authentication-related files (logindialog.*, resetpassworddialog.*, qtbcrypt.*) to src/authentication/
  - Move corresponding UI files (logindialog.ui, resetpassworddialog.ui) to authentication module
  - Create CMakeLists.txt for authentication module linking to common and openwall_crypt
  - Update include paths and ensure MOC/UIC processing works correctly
  - _Requirements: 1.1, 1.3, 2.2, 6.1, 6.3_

- [x] 5. Implement database module









  - Move database-related files (Database.h, customproxymodel.*, draggabletableview.*) to src/database/
  - Create CMakeLists.txt for database module with Qt::Sql dependency
  - Set up proper include directories and ensure the module compiles independently
  - _Requirements: 1.1, 1.3, 2.2, 6.4_

- [x] 6. Implement plate management module





  - Move all plate-related files (PlateWidget.*, PlateMapDialog.*, Plate1536Dialog.*, matrixplate*.*, daughterplatewidget.*) to src/plate_management/
  - Move corresponding UI files (PlateMapDialog.ui) to plate_management module
  - Create CMakeLists.txt for plate_management module linking to common
  - Ensure all plate widgets compile and UIC processes UI files correctly
  - _Requirements: 1.1, 1.3, 2.2, 6.1, 6.3_

- [x] 7. Implement UI module for main windows and dialogs





  - Move main UI files (databaseviewwindow.*, additemdialog.*, loadexperimentdialog.*, ClickableLabel.h) to src/ui/
  - Move corresponding UI files (databaseviewwindow.ui, additemdialog.ui, loadexperimentdialog.ui) to ui module
  - Create CMakeLists.txt for ui module linking to database and common modules
  - Update include paths and verify MOC/UIC processing
  - _Requirements: 1.1, 1.3, 2.2, 2.3, 6.1, 6.3_

- [x] 8. Implement Tecan integration module






  - Move Tecan-related files (tecanwindow.*, gwlgenerator.*, generategwldialog.*, standardlibrary.*, standardselectiondialog.*) to src/tecan_integration/
  - Move corresponding UI files (tecanwindow.ui, standardselectiondialog.ui) to tecan_integration module
  - Create CMakeLists.txt for tecan_integration module linking to plate_management, database, and common
  - Ensure all Tecan functionality compiles with proper dependencies
  - _Requirements: 1.1, 1.3, 2.2, 2.3, 6.1, 6.3_

- [x] 9. Organize and restructure resources





  - Create resources/ directory with subdirectories (icons/, data/, ui/)
  - Move icon files from icon/ to resources/icons/
  - Move JSON files from jsonfile/ to resources/data/
  - Update resources.qrc file to reflect new directory structure with proper prefixes
  - Test resource compilation and loading
  - _Requirements: 3.2, 3.4, 6.2_

- [x] 10. Create main application module





  - Create app/ directory and move main.cpp to app/main.cpp
  - Create CMakeLists.txt for app module linking to all other modules
  - Update main.cpp includes to work with new module structure
  - Ensure the main application compiles and links all modules correctly
  - _Requirements: 4.1, 4.2, 4.3, 4.4_

- [x] 11. Create root CMakeLists.txt and configure build system





  - Create new root CMakeLists.txt that coordinates all submodules using add_subdirectory()
  - Set up proper Qt configuration (AUTOMOC, AUTOUIC, AUTORCC) at root level
  - Configure project-wide settings (C++ standard, version, etc.)
  - Ensure proper target dependencies between all modules
  - _Requirements: 2.1, 2.2, 2.3, 5.3, 6.1, 6.2, 6.3, 6.4_

- [x] 12. Update include paths and fix compilation issues





  - Update all #include statements throughout the codebase to work with new module structure
  - Fix any missing includes or circular dependency issues
  - Ensure all modules compile without errors
  - Verify that Qt's MOC can process all Q_OBJECT headers correctly
  - _Requirements: 2.3, 5.4, 6.3_

- [x] 13. Test and validate the restructured build system









  - Perform full clean build of the entire project
  - Test individual module compilation to ensure modularity works
  - Verify that incremental builds work correctly (changing one module doesn't rebuild everything)
  - Test that the application runs correctly with the new structure
  - _Requirements: 2.2, 2.4, 6.1, 6.2, 6.3, 6.4_

- [x] 14. Clean up and finalize project structure





  - Remove old CMakeLists.txt.user and any build artifacts from root directory
  - Update .gitignore if necessary to account for new structure
  - Verify that all original functionality is preserved
  - Document any changes needed for developers working with the new structure
  - _Requirements: 5.1, 5.2, 5.3, 5.4_
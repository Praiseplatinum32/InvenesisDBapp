# PostgreSQL Driver Setup Guide

## The Problem
Qt can see the QPSQL driver but can't load it because PostgreSQL client libraries are missing.

## Solution 1: Install PostgreSQL (Recommended)

### Download and Install PostgreSQL
1. Go to https://www.postgresql.org/download/windows/
2. Download the PostgreSQL installer for Windows
3. Run the installer and follow the setup wizard
4. **Important**: Make sure to install the "Command Line Tools" component
5. Add PostgreSQL's bin directory to your system PATH

### Typical Installation Path
```
C:\Program Files\PostgreSQL\16\bin
```

Add this to your system PATH environment variable.

## Solution 2: Copy Required DLLs

If you don't want to install full PostgreSQL, copy these DLLs to your application directory:

### Required DLLs (from PostgreSQL installation):
- `libpq.dll`
- `libeay32.dll` (or `libcrypto-1_1-x64.dll`)
- `ssleay32.dll` (or `libssl-1_1-x64.dll`)
- `libintl-8.dll`
- `libiconv-2.dll`

### Where to place them:
```
build/Desktop_Qt_6_9_1_MinGW_64_bit-Debug/
```

## Solution 3: Alternative Database for Development

If PostgreSQL setup is problematic, you can temporarily switch to SQLite for development:

### Change in Database.h:
```cpp
// Change from:
QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL");

// To:
QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
db.setDatabaseName("invenesis.db"); // Local file
```

## Verification Steps

### 1. Check Available Drivers
Add this debug code to verify drivers:
```cpp
#include <QSqlDatabase>
#include <QDebug>

// In your main() or connection function:
qDebug() << "Available SQL drivers:" << QSqlDatabase::drivers();
```

### 2. Test PostgreSQL Connection
```cpp
QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL");
if (!db.isValid()) {
    qDebug() << "Driver not loaded!";
    return false;
}
```

### 3. Check System PATH
Open Command Prompt and run:
```cmd
where libpq.dll
```

## Common Issues

### Issue 1: Architecture Mismatch
- Ensure PostgreSQL architecture (32-bit/64-bit) matches your Qt installation
- MinGW 64-bit requires 64-bit PostgreSQL libraries

### Issue 2: Missing Dependencies
- PostgreSQL DLLs have dependencies on OpenSSL libraries
- Make sure all dependent DLLs are available

### Issue 3: Qt Version Compatibility
- Some Qt versions have specific PostgreSQL version requirements
- Qt 6.9.1 should work with PostgreSQL 12-16

## Quick Fix for Development

If you need to get running quickly, modify your Database.h temporarily:

```cpp
// Quick SQLite fallback
static bool connect(const QString &host = "", int port = 0, 
                   const QString &dbName = "invenesis.db", 
                   const QString &user = "", const QString &password = "") {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbName);
    return db.open();
}
```

This will create a local SQLite database file for development purposes.
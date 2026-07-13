\[ English | [简体中文](../../../../zh-cn/api/framework/services/pms.md) \]

# PMS API

Package Manager Service (PMS) is the package management module in the openvela XMS system.

## Features

- Package installation
- Package information query
- Package uninstallation

## Examples

**Package management via command line**

Install a package:

```
pm install [packagename]
```

List installed packages:

```
pm list
```

**Package management via source code**

Install a package:

```cpp
#include <pm/PackageManager.h>

PackageManager pm;
InstallParam parms;
pm.installPackage(parms);
```

Get all package information:

```cpp
#include <pm/PackageManager.h>

PackageManager pm;
std::vector<PackageInfo> pgInfos;
pm.getAllPackageInfo(&pgInfos);
```

Uninstall a package:

```cpp
#include <pm/PackageManager.h>

PackageManager pm;
UninstallParam parms;
pm.uninstallPackage(parms);
```

## Core Classes

### PackageManager

Header: `#include <pm/PackageManager.h>`

The facade class for accessing PMS capabilities on the client side. Main operations:

- `installPackage(InstallParam)` — Install an application package
- `uninstallPackage(UninstallParam)` — Uninstall an application package
- `getAllPackageInfo(std::vector<PackageInfo>*)` — Query all installed package information
- `getPackageInfo(packageName, PackageInfo*)` — Query information for a specific package
- `getAllPackageName(std::vector<std::string>*)` — Query all installed package names
- `getPackageSizeInfo(packageName, ...)` — Query package storage usage
- `clearAppCache(packageName)` — Clear application cache
- `isFirstBoot()` — Query whether this is the first boot

Applications typically construct a `PackageManager` instance and call the above methods directly. Internally it communicates with `PackageManagerService` via Binder.

### PackageManagerService

Header: `#include <pm/PackageManagerService.h>`

The server-side implementation class of PMS, registered as a system service. It maintains installed package metadata, performs actual install/uninstall operations, and handles permission and signature verification. Developers generally do not use this class directly.

### PackageInfo

Header: `#include <pm/PackageInfo.h>`

A structure describing the metadata of a single installed package. Main fields include:

- `packageName` / `name` — Package name and application name
- `version` / `priority` / `appType` — Version, priority, and application type
- `installedPath` / `installTime` / `size` — Installation path, installation time, and storage size
- `execfile` / `entry` / `manifest` — Executable file, entry point, and manifest
- `activitiesInfo` / `servicesInfo` — Internal Activity and Service lists
- `shasum` — Signature digest
- `userId` / `isSystemUI` — User ID and whether it is a system UI
- `windowEnterAnim` / `windowExitAnim` — Window enter/exit animation configuration

Query interfaces of `PackageManager` return results of this type.

### PackageTrace

Header: `#include <PackageTrace.h>`

A collection of trace macros for PMS, used to trace package management operation paths. Works with openvela trace tools for performance analysis.

\[ English | [简体中文](../../../../zh-cn/api/framework/services/ams.md) \]

# AMS API

Activity Manager Service (AMS) is the activity management service module in the openvela XMS system, responsible for managing application lifecycles and scheduling tasks and activities.

## Features

- **Activity Lifecycle Management**: AMS manages the lifecycle of Activities within applications, including creation, starting, pausing, resuming, and destruction.
- **Task Management**: AMS manages application tasks and task stacks, including task switching and scheduling, ensuring a smooth user experience.
- **Process Management**: AMS is responsible for starting, stopping, and monitoring application processes, ensuring effective utilization of system resources.
- **Intent Handling**: AMS handles Intent communication between applications, allowing different applications to start Activities and Services.
- **Permission Management**: AMS participates in permission checks, ensuring applications meet system security requirements when starting Activities.
- **Application State Tracking**: AMS tracks application states (such as foreground, background, stopped) and allocates resources accordingly.
- **Multi-Window Support**: AMS provides Activity management in multi-window mode, allowing multiple applications to be displayed simultaneously.
- **Background Task Restrictions**: AMS imposes restrictions on background tasks and services to optimize system performance and battery usage.
- **Service and Broadcast Management**: AMS also manages the lifecycle of Services and BroadcastReceivers, ensuring system responsiveness and stability.

## Examples

The following example code demonstrates how to use the openvela AMS module, typically through the `ActivityManager` class to manage Activities and control tasks.

**Start a New Activity**

```cpp
Intent intent;
makeIntent(intent);
intent.setFlag(intent.mFlag | Intent::FLAG_ACTIVITY_NEW_TASK);
android::sp<android::IBinder> token = new android::BBinder();
ActivityManager am;
am.startActivity(token, intent, -1);
```

**Stop an Activity**

```cpp
Intent intent;
makeIntent(intent);
ActivityManager am;
am.stopActivity(intent, intent.mFlag);
```

## Core Classes

### ActivityManager

Header: `#include <app/ActivityManager.h>`

Facade class for accessing AMS capabilities on the client side. Main methods provided:

- `startActivity()` / `stopActivity()` / `finishActivity()` — Activity start/stop
- `startService()` / `stopService()` / `stopServiceByToken()` / `bindService()` / `unbindService()` — Service operations
- `publishService()` / `getService()` — Service publishing and retrieval
- `sendBroadcast()` / `registerReceiver()` / `unregisterReceiver()` — Broadcast and receivers
- `attachApplication()` / `stopApplication()` — Application binding and termination
- `moveActivityTaskToBackground()` — Move Activity task to background
- `reportActivityStatus()` / `reportServiceStatus()` — Status reporting (application reports lifecycle status to AMS)
- `postIntent()` — Deliver Intent to a specified component

### ActivityManagerService

Header: `#include <am/ActivityManagerService.h>`

Server-side implementation class of AMS, registered as a system service, receiving calls from applications via Binder and performing scheduling. Developers generally do not use this class directly.

### Activity

Header: `#include <app/Activity.h>`

Base class for UI units in application development. Applications implement a screen by inheriting this class and overriding lifecycle callbacks such as `onCreate` / `onStart` / `onResume` / `onPause` / `onStop` / `onDestroy` / `onRestart`. Also provides operations and extension points such as `finish` / `setResult` / `getWindow` / `moveToBackground` / `onBackPressed` / `onActivityResult` / `onNewIntent`.

### Application

Header: `#include <app/Application.h>`

Global singleton base class for the application process. Applications typically inherit `Application` to hold process-level resources. Main methods include:

- Lifecycle: `onCreate` / `onDestroy` / `onForeground` / `onBackground` / `onReceiveIntent`
- Component management: `createActivity` / `createService` / `addActivity` / `addService` / `findActivity` / `findService` / `deleteActivity` / `deleteService`
- Metadata: `getPackageName` / `getUid` / `isSystemUI` / `getMainLoop` / `getWindowManager`

### ApplicationThread

Header: `#include <app/ApplicationThread.h>`

Scheduling thread abstraction on the Application side, receiving scheduling requests from AMS and dispatching execution within the application process. This is an internal framework collaboration class; application developers generally do not call it directly.

### AppMain

Header: `#include <app/AppMain.h>`

Application process entry helper class. Defines the basic flow from process startup to connecting with AMS, encapsulating the main event loop and initialization steps.

### Context

Header: `#include <app/Context.h>`

The most fundamental context base class, providing access to system capabilities. Typical methods include:

- `getPackageName()` / `getApplication()` / `getComponentName()` — Application and component information
- `startActivity()` / `startActivityForResult()` / `stopActivity()` — Activity start/stop
- `startService()` / `stopService()` / `bindService()` / `unbindService()` — Service operations
- `sendBroadcast()` / `registerReceiver()` / `unregisterReceiver()` — Broadcast and receivers
- `getActivityManager()` / `getWindowManager()` — System service access
- `getMainLoop()` / `getCurrentLoop()` — Event loop retrieval

### ContextImpl

Header: `#include <app/ContextImpl.h>`

Default implementation of the `Context` base class, assembled by the framework when creating Application / Activity / Service instances. Application developers typically do not construct `ContextImpl` directly, but obtain instances through methods like `Activity::getContext()`.

### Intent

Header: `#include <app/Intent.h>`

Data structure carrying communication intent between components. Contains fields such as action, data, target, bundle, flag, and launch flags like `FLAG_ACTIVITY_*`. Provides read/write methods including `setAction` / `setData` / `setTarget` / `setBundle` / `setFlag` / `readFromParcel` / `writeToParcel`.

### Service

Header: `#include <app/Service.h>`

Base class for long-lived components without a UI. Developers implement background services by inheriting `Service` and overriding `onCreate` / `onStartCommand` / `onBind` / `onUnbind` / `onDestroy` / `onReceiveIntent`.

### ServiceConnection

Header: `#include <app/ServiceConnection.h>`

Connection callback interface for `bindService`. Contains two callback methods `onServiceConnected` / `onServiceDisconnected`, used to notify the client when binding succeeds or disconnects.

### BroadcastReceiver

Header: `#include <app/BroadcastReceiver.h>`

Base class for broadcast receivers. Applications handle matched system or application broadcasts by inheriting this class and overriding `onReceive(Intent)`.

### MessageService

Header: `#include <app/MessageService.h>`

Service helper class for message-based communication, encapsulating the request-response pattern based on Intent, facilitating the construction of message-dispatching background services. Main methods: `sendMessage` / `receiveMessage` / `receiveMessageAndReply` / `reply` / `onBind` / `onBindExt` / `onReply`.

### Dialog

Header: `#include <app/Dialog.h>`

Base class for dialog components. Provides operations such as `show` / `hide` / `setLayout` / `setRect` / `getLayout` / `getRoot` / `createDialog`; applications can inherit to implement custom dialogs.

### UvLoop

Header: `#include <app/UvLoop.h>`

Event loop wrapper based on libuv, reused by the application main thread and other framework components. Provides capabilities such as timers, IO events, and work queues.

### Logger

Header: `#include <app/Logger.h>`

Common logging macro definitions for AMS/application side, encapsulating leveled log output (`APP_LOGI` / `APP_LOGW` / `APP_LOGE`, etc.).

### ActivityTrace

Header: `#include <ActivityTrace.h>`

Collection of trace instrumentation macros for Activity lifecycle, enabling visualization of application startup and switching paths with openvela trace analysis tools.

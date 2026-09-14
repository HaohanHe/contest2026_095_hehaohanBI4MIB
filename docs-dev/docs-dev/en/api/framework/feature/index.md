\[ English | [简体中文](../../../../zh-cn/api/framework/feature/index.md) \]

# Feature Framework API

The Feature framework is the Native extension development framework for openvela QuickApp, providing interoperability between JS and C/C++. Developers can extend new system capabilities for QuickApps through the Feature framework, which handles parameter conversion, lifecycle management, asynchronous programming models, and automatic interface generation (JIDL).

## Framework Overview

- **[Feature Framework Overview](feature_framework.md)** — Architecture, concept model (Module / Prototype / Instance), JIDL interface description language

## Core Data Types

- **[Type Definitions](feature_framework_types.md)** — Basic type aliases, handle types, enumerations, structures

## Runtime Interfaces

- **[Context and Data Conversion](feature_framework_context.md)** — `ft_value_t` creation/destruction, type conversion, array/object operations
- **[Feature Export Interface](feature_framework_export.md)** — Full runtime API for Feature developers (memory, callbacks, Promise, events, Worker, JSON)
- **[Framework Management Interface](feature_framework_main_export.md)** — APIs for QuickApp framework implementors to create and configure the Feature manager

## Frontend Interoperability

- **[QuickJS Interoperability](feature_framework_qjs_export.md)** — `ft_value_t` and `JSValue` conversion (QuickJS frontend only)

## Debugging and Performance

- **[Trace Instrumentation](feature_framework_trace.md)** — Built-in sched_note performance tracing macros for the Feature framework

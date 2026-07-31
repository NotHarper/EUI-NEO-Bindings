---
name: eui-neo-jni
description: Working on the EUI-NEO Java/JNI bridge: C ABI functions, JNI bindings, Java facade/builder classes, event system, CMake build config, or debugging JNI load/runtime failures.
---

# EUI-NEO Java/JNI Bridge Development

## Goal

Help a developer working on the three-layer JNI bridge between Java and the EUI-NEO C++17 UI engine. The layers are:

1. **C ABI** — `include/eui/neo_c_api.h` + `core/api/neo_c_api.cpp` — stable C-compatible contract
2. **JNI layer** — `bindings/java/native/eui_neo_jni.cpp` — translates JNI types to/from C ABI calls
3. **Java facade** — `bindings/java/src/main/java/com/sudoevolve/euineo/` — type-safe Java API with builder DSL

Use this skill when the task involves: adding or modifying C ABI functions, writing JNI native method implementations, updating Java facade or builder classes, configuring the CMake JNI build, diagnosing load failures, extending the JSON UI schema, or working on the event system.

## First Pass

Before touching any code, read these files in order:

1. `include/eui/neo_c_api.h` — the C ABI contract; any new API must appear here first
2. `core/api/neo_c_api.cpp` — engine struct definition, lifecycle, JSON composer, event queue
3. `bindings/java/native/eui_neo_jni.cpp` — all JNI entry points; study the error-handling helpers
4. `bindings/java/src/main/java/com/sudoevolve/euineo/NeoEngine.java` — primary Java facade
5. `bindings/java/src/main/java/com/sudoevolve/euineo/NeoUi.java` — builder root + callback registry
6. `bindings/java/src/main/java/com/sudoevolve/euineo/nodes/NeoNode.java` — base builder node
7. `CMakeLists.txt` — grep for `EUI_BUILD_JNI` to find the JNI CMake block

## Key Files

| File | Role |
|------|------|
| `include/eui/neo_c_api.h` | C ABI header — stable cross-language contract |
| `core/api/neo_c_api.cpp` | C ABI impl; owns `eui_neo_engine` struct, event queue, JSON composer |
| `bindings/java/native/eui_neo_jni.cpp` | JNI entry points; maps Java native methods to C ABI |
| `bindings/java/src/main/java/com/sudoevolve/euineo/NeoEngine.java` | Engine facade (AutoCloseable, thread affinity, pollEvent, setUi) |
| `bindings/java/src/main/java/com/sudoevolve/euineo/NeoUi.java` | Builder root: factory methods + callback registry + dispatchEvent |
| `bindings/java/src/main/java/com/sudoevolve/euineo/NeoConfig.java` | Immutable config builder |
| `bindings/java/src/main/java/com/sudoevolve/euineo/NeoFrameInfo.java` | Per-frame state (record, Java 16+) |
| `bindings/java/src/main/java/com/sudoevolve/euineo/NeoException.java` | RuntimeException with integer error code |
| `bindings/java/src/main/java/com/sudoevolve/euineo/NativeLoader.java` | Extracts and loads native lib from JAR |
| `bindings/java/src/main/java/com/sudoevolve/euineo/nodes/NeoNode.java` | Base builder: all layout/visual/transform/event/scroll props + JSON serializer |
| `bindings/java/src/main/java/com/sudoevolve/euineo/nodes/NeoLayoutNode.java` | column/row/stack/flow with justify/align |
| `bindings/java/src/main/java/com/sudoevolve/euineo/nodes/NeoRectNode.java` | rect builder |
| `bindings/java/src/main/java/com/sudoevolve/euineo/nodes/NeoTextNode.java` | text-specific builder |
| `bindings/java/src/main/java/com/sudoevolve/euineo/nodes/NeoImageNode.java` | image builder (source, fit, flipVertically) |
| `bindings/java/src/main/java/com/sudoevolve/euineo/nodes/NeoSvgNode.java` | SVG builder (source) |
| `bindings/java/src/main/java/com/sudoevolve/euineo/nodes/NeoPolygonNode.java` | polygon builder (points array) |
| `bindings/java/src/main/java/com/sudoevolve/euineo/nodes/NeoLoaderNode.java` | loader/spinner builder |
| `bindings/java/src/main/java/com/sudoevolve/euineo/nodes/NeoAlign.java` | Align enum (START/CENTER/END) |
| `bindings/java/src/main/java/com/sudoevolve/euineo/nodes/NeoEase.java` | Ease enum for transitions |
| `bindings/java/src/main/java/com/sudoevolve/euineo/nodes/NeoImageFit.java` | ImageFit enum (COVER/CONTAIN/STRETCH) |
| `bindings/java/src/main/java/com/sudoevolve/euineo/events/NeoEvent.java` | Polled event (type, handlerId, x, y, deltaX, deltaY, textInput) |
| `bindings/java/src/main/java/com/sudoevolve/euineo/events/NeoEventType.java` | Event type enum (NONE/CLICK/PRESS/RELEASE/HOVER_ENTER/HOVER_LEAVE/TEXT_INPUT/SCROLL/DRAG) |
| `tests/java/SmokeTest.java` | Headless smoke test (no display required) |
| `CMakeLists.txt` | Top-level CMake; search `EUI_BUILD_JNI` for JNI targets |
| `build-jni.bat` | One-click build script (Windows) — configure + build + JAR + smoke test |

## Architecture

```
Java application
  ↓  NeoUi builder DSL → toJson() → JSON string
  ↓  engine.setUi(ui, root)  /  engine.pollEvent()
bindings/java/src/.../NeoEngine.java  (owner-thread checked facade)
  ↓  JNI (private static native methods)
bindings/java/native/eui_neo_jni.cpp
  — throwException() / throwNeoException() / checkResult() helpers
  — Java_com_sudoevolve_euineo_NeoEngine_* entry points
  ↓  C ABI calls only (no C++ headers included here)
core/api/neo_c_api.cpp  (eui_neo_engine struct, lifecycle, event queue)
  ↓  C++17 internal API
EUI-NEO DSL runtime, GLFW/SDL windowing, OpenGL/Vulkan rendering
```

**Critical invariants to preserve:**
- JNI layer must NOT include C++ DSL headers — only `neo_c_api.h`
- All GUI calls must happen on the owner thread (thread that called `nativeCreate`)
- Float values passed through JNI use `memcpy`-based bit conversion (`Float.intBitsToFloat` / `memcpy(&bits, &f, 4)`), not `reinterpret_cast`, to avoid strict-aliasing UB
- `NeoException` is constructed with `GetMethodID`/`NewObject`/`Throw` — not `ThrowNew` — because the constructor takes both a String and an int code
- The `Cleaner` in `NeoEngine` only prints a leak warning; it never calls `nativeDestroy` (OpenGL context cannot be released from the GC thread). `eui_neo_destroy()` will also silently return without freeing if `initialized` is true and called from a non-owner thread — this is intentional to prevent GPU resource corruption
- `eui_neo_engine` is an **opaque** (incomplete) type from JNI's perspective — never access its fields directly in JNI code; use C ABI accessor functions
- After a failed `eui_neo_create()`, call `eui_neo_create_last_error()` (thread-local, no engine handle required) to retrieve the error message
- `eui_neo_request_update()` is safe to call from any thread; it guards `postEmptyEvent()` behind a mutex so it cannot race with platform shutdown

## Event System

The engine uses a pull-based event queue. On each frame the Java event loop should drain the queue and dispatch to registered handlers:

```java
NeoUi ui = new NeoUi();
NeoLayoutNode root = ui.column().fill()
    .add(ui.rect().width(80).height(80).color(0.3f,0.6f,0.9f,1)
            .onClick(() -> System.out.println("clicked!")));

engine.setUi(ui, root);

while (engine.isRunning()) {
    engine.pumpEvents(16);
    engine.drainEvents(ui);   // polls queue, dispatches to NeoUi callbacks
    NeoFrameInfo info = engine.frame();
    if (!info.running()) break;
}
```

**Event flow:**
1. C++ DSL lambda (e.g., `element->onClick`) fires → pushes `eui_neo_event` onto `engine->eventQueue`
2. `engine.pollEvent()` calls `eui_neo_poll_event()` via JNI → returns `NeoEvent` (or `NONE` when empty)
3. `engine.drainEvents(ui)` loops until NONE, passing each event to `ui.dispatchEvent(event)`
4. `NeoUi.dispatchEvent` looks up handler by `event.handlerId` in the callback registry and calls it

**Callback registry:**
- `NeoUi.registerCallback(Object handler)` returns an auto-generated ID like `"h_0"`, `"h_1"`, …
- The ID is stored as the JSON prop value: `"onClick": "h_0"`
- The C++ lambda fires with that ID in `evt.handler_id`
- Supported handler types: `Runnable` (onClick), `Consumer<NeoEvent>` (onPress/onRelease/onScroll/onDrag/onTextInput), `Consumer<Boolean>` (onHover)

## Common Tasks

### Adding a new C ABI function

1. Declare in `include/eui/neo_c_api.h` with `EUI_NEO_C_API` macro
2. Implement in `core/api/neo_c_api.cpp` — validate handle, check thread if GUI-touching
3. If the function needs to expose internal engine state to JNI, add a C accessor in both the header and cpp (never expose struct fields directly across the ABI boundary)
4. Add `private static native` declaration in `NeoEngine.java`
5. Add `JNIEXPORT` entry point in `eui_neo_jni.cpp` following `Java_com_sudoevolve_euineo_NeoEngine_nativeFunctionName`
6. Wrap in a public Java method with thread affinity documented in the comment
7. Add a headless-testable case to `SmokeTest.java` if possible

### Adding a new node type to the builder

1. Create `bindings/java/src/main/java/com/sudoevolve/euineo/nodes/NeoFooNode.java` in package `com.sudoevolve.euineo.nodes`, extending `NeoNode` with `super("foo", ui)` — constructor must be `public`
2. Add `import com.sudoevolve.euineo.NeoUi;` at the top of the new file
3. Add a factory method `fooNode()` to `NeoUi.java` (which imports `com.sudoevolve.euineo.nodes.*`)
4. Add the new source path to `EUI_JAVA_SOURCES` in `CMakeLists.txt`
5. Add an `else if (type == "foo")` branch in `configureNodeBuilder` in `neo_c_api.cpp`
6. Add a new `case` in `composeNode` if the node needs a different DSL builder (e.g., `ui.rect(id)` vs `ui.text(id)`)

### Adding a new event type

1. Add `EUI_NEO_EVENT_FOO` to the enum in `neo_c_api.h`
2. Add the push-lambda in `applyEventCallbacks()` in `neo_c_api.cpp`
3. Add `FOO` to `NeoEventType.java` with its code
4. Add a dispatch `case` in `NeoUi.dispatchEvent()`
5. Add an `onFoo(...)` method to `NeoNode.java`

### Debugging "UnsatisfiedLinkError" or DLL load failures

- On Windows: verify `eui_neo_jni.dll` has no `lib` prefix (CMake sets `PREFIX ""`)
- Check for missing MinGW runtime DLLs: the DLL should be statically linked (`-static-libgcc -static-libstdc++ -Wl,-Bstatic,--whole-archive -lwinpthread`)
- Use `dumpbin /dependents eui_neo_jni.dll` (Windows) or `ldd libeui_neo_jni.so` (Linux) to inspect dependencies
- Verify JAR contains `natives/<classifier>/eui_neo_jni.<ext>` by running `jar tf eui-neo-java.jar`

### Debugging JNI exception not thrown

- Check that `FindClass` succeeded (class not found = silently returns null on some JVMs)
- Check that `GetMethodID` signature string is correct: `"(Ljava/lang/String;I)V"` for `(String, int)`
- Local refs (`jstring`, `jobject`) must be `DeleteLocalRef`'d before returning to Java even on error paths

### Modifying the JSON UI schema

The composer lives in `core/api/neo_c_api.cpp` in `composeNode` and `configureNodeBuilder`. The `applyCommon` function handles all shared layout/visual/transform/event properties. To add a new property:
- If it's shared across all types: add to `applyCommon`
- If it's type-specific: add inside the `if (type == "...")` branch in `configureNodeBuilder`
- If it's an event: add to `applyEventCallbacks` and push the appropriate `eui_neo_event_type`

The forward declaration of `composeNode` must appear before the `configureNodeBuilder` template.

## Build and Test

```sh
# Full JNI build (GLFW + OpenGL, offline deps)
cmake -S . -B build-jni \
  -DEUI_BUILD_APPS=OFF \
  -DEUI_BUILD_JNI=ON \
  -DEUI_DEPS_MODE=bundled
cmake --build build-jni --parallel
cmake --build build-jni --target eui_neo_java_classes

# Verify JAR contents
jar tf build-jni/eui-neo-java.jar | grep natives

# Headless smoke test (Windows)
javac --release 17 -encoding UTF-8 \
  -cp build-jni/eui-neo-java.jar \
  -d build-jni/smoke-classes \
  tests/java/SmokeTest.java
java -cp "build-jni/eui-neo-java.jar;build-jni/smoke-classes" \
  com.sudoevolve.euineo.SmokeTest

# Regression: default C++ build must not break
cmake -S . -B build-default -DEUI_DEPS_MODE=bundled
cmake --build build-default --parallel
```

## Constraints

- **`decorated` is creation-time only**: `NeoConfig.decorated(false)` removes the title bar / border. Set before `new NeoEngine(config)` — there is no runtime setter. Translates to `GLFW_DECORATED = GLFW_FALSE` (GLFW) or `SDL_WINDOW_BORDERLESS` (SDL2).
- **Single active engine per process**: the DSL runtime has process-level static state; creating two `NeoEngine` instances concurrently is undefined behaviour.
- **Owner thread**: every `NeoEngine` method (except `requestUpdate` and `isRunning`) must be called from the thread that created the engine. Violation throws `NeoException(code=3)`.
- **Event queue is pull-based**: the queue is not thread-safe; only drain events on the owner thread after `pumpEvents`.
- **Callback registry lifetime**: callbacks registered in `NeoUi` are held for the lifetime of the `NeoUi` instance. Rebuild the `NeoUi` (and call `setUi` again) to clear old handlers.
- **Java 16+ for `NeoFrameInfo` record**: if Java 11 support is needed, convert to a regular `final class`.
- **`NeoException` is `RuntimeException`**: callers don't have to declare it, but engine failure is usually unrecoverable so catch it at the top-level event loop.
- **Windows DLL naming**: `System.mapLibraryName("eui_neo_jni")` returns `"eui_neo_jni.dll"` (no `lib` prefix). CMake sets `set_target_properties(eui_neo_jni PROPERTIES PREFIX "")` to match.
- **`eui_neo_engine` is opaque to JNI**: the struct definition is internal to `neo_c_api.cpp`. JNI code must only call C ABI functions — never access `engine->` fields directly.

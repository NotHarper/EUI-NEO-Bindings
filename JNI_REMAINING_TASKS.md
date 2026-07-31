# EUI-NEO JNI 剩余任务

## 当前目标

让 EUI-NEO 可以由 Java 通过 JNI 使用，覆盖稳定 C ABI、native engine 生命周期、JNI 封装、Java API、构建安装、测试、文档和跨平台发布。

## 已完成

### 第一阶段：核心骨架

- `include/eui/neo_c_api.h` — 稳定 C ABI：opaque handle、config/frame_info 结构体、生命周期函数声明
- `core/api/neo_c_api.cpp` — engine 实现：单实例限制、GLFW/SDL 平台初始化、窗口、render backend、runtime 生命周期、JSON UI composer（row/column/stack/flow/rect/text/image）
- `bindings/java/native/eui_neo_jni.cpp` — JNI 桥接：
  - float bit conversion：`memcpy`（避免 strict-aliasing UB）
  - NeoException：`GetMethodID`/`NewObject`/`Throw` 正确传递 error code
  - `composeNode` 前置声明（修复模板依赖错误）
- Java 基础类：`NeoEngine`、`NeoConfig`、`NeoFrameInfo`、`NeoException`、`NativeLoader`
- `CMakeLists.txt` — JNI 构建：platform classifier、PREFIX=""、MinGW 静态链接
- `tests/java/SmokeTest.java` — 无界面 smoke test
- `README.md` — Java/JNI 章节

### 第二阶段：事件系统 + Builder DSL + 扩展 C ABI

- **C ABI 扩展**（`neo_c_api.h` + `neo_c_api.cpp`）：
  - `eui_neo_poll_event()` — pull 模式事件队列（`std::queue<eui_neo_event>` 内置于 `eui_neo_engine`）
  - `eui_neo_last_event_handler_id()` / `eui_neo_last_event_text_input()` — C accessor 解决 JNI 不能访问不完整类型字段的问题
  - `eui_neo_set_window_title()` / `eui_neo_set_window_size()` — 窗口管理
  - `eui_neo_api_version()` — 整数版本号
  - `eui_neo_event` 结构体 + `eui_neo_event_type` 枚举（NONE/CLICK/PRESS/RELEASE/HOVER_ENTER/HOVER_LEAVE/TEXT_INPUT/SCROLL/DRAG）
  - C++ namespace 修复：`CursorShape`/`PointerEvent`/`ScrollEvent`/`KeyboardEvent`/`HorizontalAlign`/`VerticalAlign`/`ImageFit` 均在 `core::` 而非 `core::dsl::`（`DragEvent` 除外）
- **JNI 层**（`eui_neo_jni.cpp`）：新增 6 个 native 方法：`nativePollEvent`、`nativeLastEventHandlerId`、`nativeLastEventTextInput`、`nativeSetWindowTitle`、`nativeSetWindowSize`、`nativeApiVersion`
- **Java Builder DSL**（`com.sudoevolve.euineo.nodes`）：
  - `NeoNode` — 通用属性 builder（布局/视觉/变换/过渡/滚动/事件）+ 手写 JSON 序列化器
  - `NeoLayoutNode`（column/row/stack/flow）、`NeoRectNode`、`NeoTextNode`、`NeoImageNode`、`NeoSvgNode`、`NeoPolygonNode`、`NeoLoaderNode`
  - `NeoAlign`、`NeoEase`、`NeoImageFit` 枚举
- **Java 事件类**（`com.sudoevolve.euineo.events`）：`NeoEvent`、`NeoEventType`
- **NeoUi**（`com.sudoevolve.euineo`）：节点工厂方法 + callback registry（`AtomicInteger` counter，`h_0`/`h_1`…）+ `dispatchEvent`
- **NeoEngine 扩展**：`setUi(NeoUi, NeoNode)`、`pollEvent()`、`drainEvents(NeoUi)`、`setWindowTitle`、`setWindowSize`、`apiVersion()`
- **包目录重组**：`nodes/`（11 个文件）和 `events/`（2 个文件）子包
- **CMakeLists.txt**：`EUI_JAVA_SOURCES` 更新为全 19 个源文件路径
- **构建验证**：`cmake --build build-jni-check --target eui_neo_java_classes` 零错误

## 剩余任务

### 1. C ABI engine 待改进（非阻塞，但需在正式发布前解决）

- [ ] `eui_neo_create()` 失败时 lastError 只能在销毁前从 engine 读取；考虑增加线程局部 create error 或 `eui_neo_create_ex(out_error)`
- [ ] `eui_neo_destroy()` 若由非 owner 线程调用仍会直接 delete，可能绕过 OpenGL 上下文清理
- [ ] `eui_neo_request_update()` 与 shutdown 的并发安全：`core::window::postEmptyEvent()` 的 platform lifetime 需要保护
- [ ] `frames_per_second` 字段目前未用于帧节流
- [ ] 完善 GLFW 事件回调：content scale、refresh、focus/iconify
- [ ] 完善 SDL2 事件映射：键盘、文本/IME、滚轮、窗口事件
- [ ] 验证 Vulkan loader 初始化在 GLFW/SDL 两种 backend 下的顺序

### 2. JNI / Java API 待补充

- [ ] 增加 `JNI_OnLoad`：校验 JNI 版本，缓存 `JavaVM*`（为后续异步 callback 做准备）
- [ ] JSON UI schema 版本字段与正式文档
- [ ] Java 版本兼容基线：`NeoFrameInfo` 使用 `record`（需 Java 16+）；如需支持 Java 11，改为普通 final class

### 3. 构建与安装

- [ ] CMake `install()` 规则验证：`eui_neo_jni` + JAR + C API header 安装路径 + `find_package` consumer 测试
- [ ] Linux/macOS 上 JNI 目标的 PIC 和 rpath 设置
- [ ] SDL2 和 Vulkan backend 下的 JNI 构建验证

### 4. 测试

- [ ] C ABI unit/smoke tests（version、invalid args、null struct、重复 lifecycle、invalid JSON）
- [ ] Java lifecycle tests（create/close、重复 close、exception mapping、wrong-thread）
- [ ] 无 display 的 Linux CI：使用 Xvfb
- [ ] ASan/UBSan 验证（JNI strings、handle 生命周期、runtime shutdown）

### 5. CI 与发布

- [ ] `.github/workflows/release.yml`：安装 JDK，启用 `EUI_BUILD_JNI=ON`，构建/测试/上传 Java artifact
- [ ] 平台 classifier JAR 上传（含版本、OS、arch）
- [ ] Java smoke test 在每个平台 CI 上运行

### 6. 文档

- [ ] `docs/集成指南.md`：JNI 架构、JSON UI schema、event loop、线程规则、资源关闭
- [ ] `docs/开发与发布.md`：JNI build/test/install/release 命令
- [ ] `README.zh-CN.md` 同步 Java 章节

## 端到端验证命令（参考）

```bat
build-jni.bat
```

或手动：

```sh
# 默认 C++ 回归
cmake -S . -B build-default -DEUI_DEPS_MODE=bundled
cmake --build build-default --parallel

# JNI 构建
cmake -S . -B build-jni ^
  -DEUI_BUILD_APPS=OFF ^
  -DEUI_BUILD_JNI=ON ^
  -DEUI_DEPS_MODE=bundled
cmake --build build-jni --parallel
cmake --build build-jni --target eui_neo_java_classes

# JAR 内容验证
jar tf build-jni/eui-neo-java.jar | findstr natives

# Headless smoke test
javac --release 17 -encoding UTF-8 ^
  -cp build-jni/eui-neo-java.jar ^
  -d build-jni/smoke-classes ^
  tests/java/SmokeTest.java
java -cp "build-jni/eui-neo-java.jar;build-jni/smoke-classes" ^
  com.sudoevolve.euineo.SmokeTest
```

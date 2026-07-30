# EUI-NEO JNI 剩余任务

## 当前目标

让 EUI-NEO 可以由 Java 通过 JNI 使用，覆盖稳定 C ABI、native engine 生命周期、JNI 封装、Java API、构建安装、测试、文档和跨平台发布。

## 已完成 (本 session)

- `include/eui/neo_c_api.h` — 稳定 C ABI，opaque handle，config/frame_info 结构体，生命周期函数声明
- `core/api/neo_c_api.cpp` — engine 实现：单实例限制、GLFW/SDL 平台初始化、窗口、render backend、runtime 生命周期、JSON UI composer（row/column/stack/flow/rect/text/image）
- `bindings/java/native/eui_neo_jni.cpp` — JNI 桥接，修复了：
  - float bit conversion（由 `reinterpret_cast` 改为 `memcpy`，消除 strict-aliasing UB）
  - NeoException 构造：改用 `GetMethodID`/`NewObject`/`Throw` 正确传递 error code
  - `composeNode` 前置声明（修复模板依赖错误）
- `bindings/java/src/main/java/com/sudoevolve/euineo/`
  - `NeoEngine` — AutoCloseable，close() 改为 try-finally 保证 nativeDestroy 调用；Cleaner 改为仅报告泄漏，不从 Cleaner 线程销毁 native GUI 资源
  - `NeoConfig`, `NeoFrameInfo`, `NeoException`, `NativeLoader` — 完整 Java API
- `CMakeLists.txt` — 完整 JNI 构建配置：
  - 平台 classifier 检测（windows-x86_64, linux-x86_64, macos-aarch64 等）
  - make_directory + native 资源打包（remove_directory 保证幂等）
  - Windows PREFIX="" 修复（MinGW 不加 lib 前缀，与 System.mapLibraryName 一致）
  - MinGW 静态链接 libgcc/libstdc++/winpthread（JNI DLL 自包含，无外部 MinGW runtime 依赖）
  - javac --release 17 -encoding UTF-8
- `bindings/java/smoke/SmokeTest.java` — 无界面 smoke test（library load、version、config builder）
- `README.md` — 新增 Java/JNI 快速开始章节（构建、代码示例、native 加载、线程亲和性）

## 已验证

- `cmake --build build-jni-check --parallel` 全量编译成功（native + JNI DLL + Java classes + JAR）
- JAR 内容：`natives/windows-x86_64/eui_neo_jni.dll` + 所有 `.class` 文件
- `SmokeTest` 通过：`version() = 0.5.3`，`NeoConfig` 默认值正确，builder chain 正确
- 默认 C++ 回归构建（`build-default`）无回归

## 剩余任务

### 1. C ABI engine 待改进（非阻塞，但需在正式发布前解决）

- [ ] `BuilderBase::element()` 当前作为公开 DSL API 暴露，仅供 JSON composer 使用；应改为 internal/friend 或专用访问方式，避免污染公开 API
- [ ] `eui_neo_create()` 失败时 lastError 只能在销毁前从 engine 读取；考虑增加线程局部 create error 或 `eui_neo_create_ex(out_error)` 供调用方在 create 失败时获取详细信息
- [ ] `eui_neo_destroy()` 若由非 owner 线程调用仍会直接 delete，可能绕过 OpenGL 上下文清理；应改为安全失败或延迟清理
- [ ] `eui_neo_request_update()` 与 shutdown 的并发安全：`core::window::postEmptyEvent()` 的 platform lifetime 需要保护
- [ ] `frames_per_second` 字段目前未用于帧节流
- [ ] 完善 GLFW 事件回调：content scale、refresh、focus/iconify
- [ ] 完善 SDL2 事件映射：键盘、文本/IME、滚轮、窗口事件
- [ ] 验证 Vulkan loader 初始化在 GLFW/SDL 两种 backend 下的顺序

### 2. JNI / Java API 待补充

- [ ] 增加 `JNI_OnLoad`：校验 JNI 版本，缓存 `JavaVM*`（为后续异步 callback 做准备）
- [ ] 事件回传：设计 event queue（首选 pull 模式）支持点击等交互事件从 native 回传 Java；当前 JSON UI 只能显示，不能把交互事件交给 Java
- [ ] JSON UI schema 版本字段与正式文档
- [ ] Java 版本兼容基线：`NeoFrameInfo` 使用 `record`（需 Java 16+）；如需支持 Java 11，应改为普通 final class

### 3. 构建与安装

- [ ] CMake `install()` 规则验证：`eui_neo_jni` + JAR + C API header 的安装路径和 `find_package` consumer 测试
- [ ] Linux/macOS 上 JNI 目标的 PIC 和 rpath 设置
- [ ] SDL2 和 Vulkan backend 下的 JNI 构建验证（目前只验证了 GLFW + OpenGL）

### 4. 测试

- [ ] C ABI unit/smoke tests（version、invalid args、null struct、重复 lifecycle、invalid JSON）
- [ ] Java lifecycle tests（create/close、重复 close、exception mapping、wrong-thread）
- [ ] 无 display 的 Linux CI：使用 Xvfb，不能把图形初始化失败误当成功
- [ ] ASan/UBSan 验证（JNI strings、handle 生命周期、runtime shutdown）

### 5. CI 与发布

- [ ] `.github/workflows/release.yml`：安装 JDK，启用 `EUI_BUILD_JNI=ON`，构建/测试/上传 Java artifact
- [ ] 平台 classifier JAR 上传（包含版本、OS、arch）
- [ ] Java smoke test 在每个平台 CI 上运行
- [ ] 保留现有 C++ runtime/SDK release 产物不回归

### 6. 文档

- [ ] `docs/集成指南.md`：JNI 架构、JSON UI schema、event loop 驱动方式、线程规则、资源关闭
- [ ] `docs/开发与发布.md`：JNI build/test/install/release 命令
- [ ] README.zh-CN.md 同步 Java 章节
- [ ] 第一版限制说明：单进程单 active engine、GUI thread affinity、支持 JDK/OS/arch/backend

## 端到端验证命令（参考）

```sh
# 默认 C++ 回归
cmake -S . -B build-default -DEUI_DEPS_MODE=bundled
cmake --build build-default --parallel

# JNI 构建
cmake -S . -B build-jni \
  -DEUI_BUILD_APPS=OFF \
  -DEUI_BUILD_JNI=ON \
  -DEUI_DEPS_MODE=bundled
cmake --build build-jni --parallel
cmake --build build-jni --target eui_neo_java_classes

# Headless smoke test
javac --release 17 -encoding UTF-8 \
  -cp build-jni/eui-neo-java.jar \
  -d build-jni/smoke-classes \
  bindings/java/smoke/SmokeTest.java
java -cp "build-jni/eui-neo-java.jar;build-jni/smoke-classes" \
  com.sudoevolve.euineo.SmokeTest
```

# EUI-NEO Java / JNI 集成指南

本文介绍如何将 EUI-NEO 接入 Java 项目，涵盖架构、构建、C ABI、Java Builder DSL、事件系统、JSON UI Schema、线程模型、错误处理和已知限制。

## 架构概览

三层架构，各层只依赖正下方层：

```
┌──────────────────────────────────┐
│          Java 应用               │
│  NeoUi builder → NeoEngine       │
└──────────────┬───────────────────┘
               │ JNI (native 方法)
┌──────────────▼───────────────────┐
│       eui_neo_jni.cpp            │
│   bindings/java/native/          │
└──────────────┬───────────────────┘
               │ C ABI 函数调用
┌──────────────▼───────────────────┐
│   neo_c_api.cpp / neo_c_api.h    │
│   core/api/  include/eui/        │
└──────────────┬───────────────────┘
               │ C++17 内部 API
┌──────────────▼───────────────────┐
│        EUI-NEO 核心              │
│  DSL Runtime · Window · Render   │
└──────────────────────────────────┘
```

### 设计原则

- Java 不直接持有 C++ `Ui`、`Element`、`Screen` 或 `std::function`；所有 C++ 对象隐藏在 opaque handle 后面。
- UI 通过 Java Builder DSL 描述，构建时序列化为 JSON，由 C++ compositor 解析并渲染。
- 事件回调通过 pull queue 返回 Java：C++ lambda 推入队列 → Java 每帧 poll → 分发到注册的 Java handler。
- C ABI 结构体携带 `size`/`version` 字段，保证二进制向后兼容；调用方须先调用 `_init()` 初始化。
- 所有 GUI 操作（initialize / pumpEvents / frame / setUi / close）必须在创建 engine 的线程执行。
- 第一版限制进程内只能存在一个 active engine（底层 DSL Runtime 含进程级静态状态）。
- JNI 层把 native 错误码映射为 Java 异常，不向上泄漏 C++ 细节。

## 快速开始

### Builder DSL（推荐）

```java
import com.sudoevolve.euineo.*;
import com.sudoevolve.euineo.nodes.*;
import com.sudoevolve.euineo.events.*;

NeoConfig config = new NeoConfig().title("Hello EUI-NEO").size(960, 640);

try (NeoEngine engine = new NeoEngine(config)) {
    engine.initialize();

    NeoUi ui = new NeoUi();
    NeoLayoutNode root = ui.column().fill().padding(24).gap(16)
        .add(ui.text().text("Hello from Java").fontSize(28)
                      .textColor(0.2f, 0.2f, 0.2f, 1))
        .add(ui.rect().width(120).height(48).radius(8)
                      .color(0.3f, 0.6f, 0.9f, 1)
                      .onClick(() -> System.out.println("clicked!")));

    engine.setUi(ui, root);

    while (engine.isRunning()) {
        engine.pumpEvents(16);
        engine.drainEvents(ui);   // 轮询事件队列并分发到 Java handler
        NeoFrameInfo info = engine.frame();
        if (!info.running()) break;
    }
}
```

### 直接使用 JSON（低层）

```java
engine.setUiJson("""
    {"type":"column","padding":24,"children":[
      {"type":"text","text":"Hello","fontSize":28}
    ]}
    """);
```

## 构建

### 要求

- CMake 3.14+、支持 C++17 的编译器
- JDK 17+（`javac`/`jar` 须在 `PATH` 或通过 `JAVA_HOME` 指定）
- OpenGL 开发文件 + GLFW 或 SDL2（与标准 C++ 构建相同）

### 一键构建（Windows）

```bat
build-jni.bat
```

支持可选 `clean` 参数（`build-jni.bat clean`）以删除旧构建目录后重新配置。脚本依次执行：CMake 配置 → native 编译 → Java 编译 + 打包 JAR → 验证 JAR 内容 → smoke test。

### 手动构建命令

```sh
cmake -S . -B build-jni \
  -DEUI_BUILD_APPS=OFF \
  -DEUI_BUILD_JNI=ON \
  -DEUI_DEPS_MODE=bundled
cmake --build build-jni --parallel
cmake --build build-jni --target eui_neo_java_classes
```

### 构建产物

| 文件 | 说明 |
|------|------|
| `build-jni/eui_neo_jni.dll` | JNI 共享库（Linux: `.so`，macOS: `.dylib`） |
| `build-jni/eui-neo-java.jar` | Java facade + native 库打包（自包含） |

JAR 内 native 库路径为 `natives/<os>-<arch>/eui_neo_jni.<ext>`，由 `NativeLoader` 自动提取加载。

### Headless Smoke Test

```sh
javac --release 17 -encoding UTF-8 \
  -cp build-jni/eui-neo-java.jar \
  -d build-jni/smoke-classes \
  tests/java/SmokeTest.java

# Windows
java -cp "build-jni/eui-neo-java.jar;build-jni/smoke-classes" \
  com.sudoevolve.euineo.SmokeTest
```

## C ABI 参考

头文件：`include/eui/neo_c_api.h`。

### 版本与 API 版本

```c
#define EUI_NEO_VERSION_MAJOR 0
#define EUI_NEO_VERSION_MINOR 5
#define EUI_NEO_VERSION_PATCH 3

const char* eui_neo_version_string(void);
uint32_t    eui_neo_api_version(void);    // 整数版本，用于兼容性检测
```

### 错误码

| 枚举值 | 值 | 含义 |
|--------|-----|------|
| `EUI_NEO_OK` | 0 | 成功 |
| `EUI_NEO_ERROR` | 1 | 通用错误，见 `eui_neo_last_error()` |
| `EUI_NEO_NOT_INITIALIZED` | 2 | 未初始化时调用了需要初始化的函数 |
| `EUI_NEO_WRONG_THREAD` | 3 | 调用线程不是 owner thread |
| `EUI_NEO_INVALID_ARGUMENT` | 4 | 参数为 null 或超范围 |
| `EUI_NEO_ALREADY_INITIALIZED` | 5 | 重复调用 `eui_neo_initialize()` |

### 生命周期函数

```c
eui_neo_engine* eui_neo_create(const eui_neo_config* config);
eui_neo_result  eui_neo_initialize(eui_neo_engine* engine);
eui_neo_result  eui_neo_pump_events(eui_neo_engine* engine, int32_t timeout_ms);
eui_neo_result  eui_neo_frame(eui_neo_engine* engine, eui_neo_frame_info* info);
eui_neo_result  eui_neo_request_update(eui_neo_engine* engine);  // 线程安全
int32_t         eui_neo_is_running(const eui_neo_engine* engine);
eui_neo_result  eui_neo_shutdown(eui_neo_engine* engine);
void            eui_neo_destroy(eui_neo_engine* engine);
const char*     eui_neo_last_error(const eui_neo_engine* engine);
```

### UI 内容与事件

```c
eui_neo_result  eui_neo_set_ui_json(eui_neo_engine* engine, const char* json_utf8);
eui_neo_result  eui_neo_poll_event(eui_neo_engine* engine, eui_neo_event* out_event);
const char*     eui_neo_last_event_handler_id(const eui_neo_engine* engine);
const char*     eui_neo_last_event_text_input(const eui_neo_engine* engine);
```

### 窗口管理

```c
eui_neo_result  eui_neo_set_window_title(eui_neo_engine* engine, const char* title_utf8);
eui_neo_result  eui_neo_set_window_size(eui_neo_engine* engine, int32_t w, int32_t h);
```

### 事件结构体

```c
typedef enum eui_neo_event_type {
    EUI_NEO_EVENT_NONE        = 0,
    EUI_NEO_EVENT_CLICK       = 1,
    EUI_NEO_EVENT_PRESS       = 2,
    EUI_NEO_EVENT_RELEASE     = 3,
    EUI_NEO_EVENT_HOVER_ENTER = 4,
    EUI_NEO_EVENT_HOVER_LEAVE = 5,
    EUI_NEO_EVENT_TEXT_INPUT  = 6,
    EUI_NEO_EVENT_SCROLL      = 7,
    EUI_NEO_EVENT_DRAG        = 8
} eui_neo_event_type;

typedef struct eui_neo_event {
    uint32_t size;               // sizeof(eui_neo_event)
    uint32_t type;               // eui_neo_event_type
    char     handler_id[128];    // 对应 NeoUi callback registry 中的 ID
    float    x, y;               // 指针坐标（PRESS/RELEASE/DRAG）
    float    delta_x, delta_y;   // 滚动增量（SCROLL）或拖拽增量（DRAG）
    char     text_input[64];     // 输入文本（TEXT_INPUT）
    uint8_t  reserved[8];
} eui_neo_event;

void eui_neo_event_init(eui_neo_event* event);   // 零初始化，调用前必须调用
```

`poll_event` 返回 `EUI_NEO_OK` 且 `type == EUI_NEO_EVENT_NONE` 表示队列为空。

## Java Builder DSL

### NeoUi — 根上下文

`NeoUi` 持有 callback registry 并提供所有节点工厂方法。每次重建 UI 树时创建新的 `NeoUi` 实例以清除旧 handler。

```java
NeoUi ui = new NeoUi();

// 工厂方法
NeoLayoutNode col    = ui.column();
NeoLayoutNode row    = ui.row();
NeoLayoutNode stack  = ui.stack();
NeoLayoutNode flow   = ui.flow();
NeoRectNode   rect   = ui.rect();
NeoTextNode   text   = ui.text();
NeoImageNode  image  = ui.image();
NeoSvgNode    svg    = ui.svg();
NeoPolygonNode poly  = ui.polygon();
NeoLoaderNode loader = ui.loader();

// 提交 UI 到 engine
engine.setUi(ui, root);

// 分发队列中的事件到对应 handler
ui.dispatchEvent(event);
```

### NeoNode — 通用属性

所有节点类型均继承以下方法（均返回 `this` 支持链式调用）：

**尺寸与位置**

```java
.width(float)  .width("fill")  .width("wrap")
.height(float) .height("fill") .height("wrap")
.fill()          // 等价于 .width("fill").height("fill")
.x(float)  .y(float)
.minWidth(float)  .minHeight(float)
.maxWidth(float)  .maxHeight(float)
.flexGrow(float)  .flexShrink(float)
.zIndex(int)  .ignoreLayout(boolean)
```

**间距**

```java
.padding(float)
.paddingX(float)  .paddingY(float)
.paddingLeft(float)  .paddingTop(float)  .paddingRight(float)  .paddingBottom(float)
.margin(float)
.marginX(float)  .marginY(float)
.marginLeft(float)  .marginTop(float)  .marginRight(float)  .marginBottom(float)
.gap(float)   // 子节点间距
.lineGap(float)
```

**视觉**

```java
.color(r, g, b, a)        // 背景色（0.0–1.0 RGBA）
.radius(float)            // 圆角
.opacity(float)           // 透明度
.clip(boolean)
.blur(float)
.hoverColor(r, g, b, a)
.pressedColor(r, g, b, a)
.border(width, r, g, b, a)
.shadow(blur, offsetX, offsetY, r, g, b, a)
.shadow(blur, offsetX, offsetY, r, g, b, a, inset)
.gradient(sr,sg,sb,sa, er,eg,eb,ea, vertical)
```

**变换**

```java
.translate(x, y)
.scale(s)         .scale(sx, sy)
.rotate(radians)
.rotateX(float)   .rotateY(float)
.perspective(float)
.transformOrigin(x, y)
```

**过渡动画**

```java
.transition(duration)
.transition(duration, NeoEase.OUT_CUBIC)
.transition(duration, NeoEase.OUT_CUBIC, delay)
```

**交互**

```java
.interactive()   .focusable()   .disabled(boolean)
```

**滚动**

```java
.scrollState(id, offset, maxOffset, step)
.scrollContentFrom(sourceId)
```

**事件**

```java
.onClick(Runnable handler)
.onPress(Consumer<NeoEvent> handler)
.onRelease(Consumer<NeoEvent> handler)
.onHover(Consumer<Boolean> handler)       // true = 鼠标进入，false = 离开
.onScroll(Consumer<NeoEvent> handler)     // event.deltaX/deltaY = 滚动增量
.onDrag(Consumer<NeoEvent> handler)       // event.x/y = 当前位置，deltaX/deltaY = 增量
.onTextInput(Consumer<NeoEvent> handler)  // event.textInput = 输入文本
```

**子节点**

```java
.add(NeoNode child)
.add(NeoNode... children)
```

### NeoLayoutNode（column/row/stack/flow）

在 `NeoNode` 基础上增加：

```java
.justify(NeoAlign.START / CENTER / END)   // 主轴对齐（mainAlign）
.align(NeoAlign.START / CENTER / END)     // 交叉轴对齐（crossAlign）
```

### NeoTextNode

```java
.text(String)
.fontSize(float)
.fontWeight(int)          // 100–900
.fontFamily(String)
.textColor(r, g, b, a)
.wrap(boolean)
.lineHeight(float)
.horizontalAlign(NeoAlign)
.verticalAlign(NeoAlign)
```

### NeoImageNode

```java
.source(String path)
.fit(NeoImageFit.COVER / CONTAIN / STRETCH)
.flipVertically(boolean)
```

### NeoSvgNode

```java
.source(String svgPath)
```

### NeoPolygonNode

```java
.points(float[]... points)   // 每个 point 是 float[]{x, y}
// 例：.points(new float[]{0,0}, new float[]{100,0}, new float[]{50,100})
```

### NeoLoaderNode

```java
.active(boolean)
.mode(String)    // 如 "spin"、"dots" 等，取决于主题
```

### 枚举类型

| 类 | 值 |
|---|----|
| `NeoAlign` | `START` `CENTER` `END` |
| `NeoImageFit` | `COVER` `CONTAIN` `STRETCH` |
| `NeoEase` | `LINEAR` `IN_QUAD` `OUT_QUAD` `IN_OUT_QUAD` `OUT_CUBIC` `IN_OUT_CUBIC` `OUT_EXPO` `OUT_BACK` |
| `NeoEventType` | `NONE` `CLICK` `PRESS` `RELEASE` `HOVER_ENTER` `HOVER_LEAVE` `TEXT_INPUT` `SCROLL` `DRAG` |

## Java API 参考

### `NeoEngine`

```java
NeoEngine(NeoConfig config)
void           initialize()
boolean        isRunning()
void           pumpEvents(int timeoutMs)
NeoFrameInfo   frame()
void           requestUpdate()          // 任意线程安全

// UI
void           setUiJson(String json)
void           setUi(NeoUi ui, NeoNode root)  // 序列化 root.toJson() 后调用 setUiJson

// 事件
NeoEvent       pollEvent()              // 取出一个事件；type==NONE 表示队列为空
void           drainEvents(NeoUi ui)    // 循环 pollEvent 并 ui.dispatchEvent

// 窗口
void           setWindowTitle(String title)
void           setWindowSize(int w, int h)

// 状态
String         lastError()
static String  version()
static int     apiVersion()

@Override void close()                  // owner thread，幂等
```

### `NeoEvent`

```java
NeoEventType type
String       handlerId   // 对应 NeoUi callback registry 中的 ID
float        x, y        // 指针坐标
float        deltaX, deltaY  // 滚动/拖拽增量
String       textInput   // TEXT_INPUT 时有值

boolean isNone()
```

### `NeoConfig`

```java
NeoConfig()
NeoConfig title(String)
NeoConfig size(int w, int h)
NeoConfig dpiScale(float)
NeoConfig framesPerSecond(int)
NeoConfig pageId(String)
NeoConfig uiJson(String)
NeoConfig clearColor(float r, float g, float b, float a)
NeoConfig resizable(boolean)
```

### `NeoFrameInfo`（Java record）

```java
record NeoFrameInfo(
    long    frameNumber,
    int     framebufferWidth,
    int     framebufferHeight,
    float   dpiScale,
    boolean rendered,
    boolean running
)
```

### `NeoException`

```java
NeoException(String message, int code)
int code()
```

| code | 触发场景 |
|------|----------|
| 1 | 通用 native 错误 |
| 2 | 未初始化时调用了需要初始化的方法 |
| 3 | 从非 owner thread 调用 |
| 4 | 映射为 `IllegalArgumentException` |
| 5 | 重复调用 `initialize()` |

## 事件循环与线程模型

### 推荐写法

```java
Thread uiThread = new Thread(() -> {
    NeoUi ui = new NeoUi();
    NeoLayoutNode root = buildUi(ui);

    try (NeoEngine engine = new NeoEngine(new NeoConfig().title("App").size(960, 640))) {
        engine.initialize();
        engine.setUi(ui, root);

        while (engine.isRunning()) {
            engine.pumpEvents(16);
            engine.drainEvents(ui);
            NeoFrameInfo info = engine.frame();
            if (!info.running()) break;
        }
    }
});
uiThread.setName("eui-neo-ui");
uiThread.start();
```

### 从后台线程触发重绘

```java
// 后台线程（只允许 requestUpdate）
pendingUiJson.set(newJson);
engine.requestUpdate();   // 线程安全，唤醒 pumpEvents

// UI 线程（帧循环内）
String next = pendingUiJson.getAndSet(null);
if (next != null) engine.setUiJson(next);
```

### 线程约束

| 操作 | 允许的线程 |
|------|-----------|
| `new NeoEngine` | 任意（此线程成为 owner thread） |
| `initialize` / `pumpEvents` / `frame` | owner thread |
| `setUiJson` / `setUi` / `pollEvent` / `drainEvents` | owner thread |
| `setWindowTitle` / `setWindowSize` | owner thread |
| `close` | owner thread |
| `requestUpdate` | **任意线程** |
| `isRunning` | 任意线程（volatile 读） |

## JSON UI Schema

`setUiJson` / `setUi` 接受 UTF-8 JSON 字符串描述 UI 树，根节点为单个对象，`children` 递归嵌套。

### 节点类型

| type | 说明 |
|------|------|
| `column` | 垂直线性布局 |
| `row` | 水平线性布局 |
| `stack` | 层叠布局 |
| `flow` | 自动换行流式布局 |
| `rect` | 矩形色块 |
| `text` | 文本节点 |
| `image` | 图片节点 |
| `svg` | SVG 图形节点 |
| `polygon` | 多边形节点 |
| `loader` | 加载动画节点 |

### 通用属性（所有类型）

```json
{
  "type": "rect",
  "id": "my-id",
  "width": 120,          "height": 80,
  "width": "fill",       "height": "wrap",
  "x": 0,               "y": 0,
  "minWidth": 40,        "maxWidth": 400,
  "padding": 12,
  "paddingX": 8,         "paddingY": 4,
  "paddingLeft": 8,      "paddingTop": 4,
  "paddingRight": 8,     "paddingBottom": 4,
  "margin": 8,
  "gap": 12,             "lineGap": 4,
  "flexGrow": 1,         "flexShrink": 0,
  "zIndex": 2,           "ignoreLayout": true,
  "opacity": 0.8,        "clip": true,
  "radius": 8,
  "color": [0.2, 0.4, 0.8, 1.0],
  "hoverColor": [0.3, 0.5, 0.9, 1.0],
  "pressedColor": [0.1, 0.3, 0.7, 1.0],
  "interactive": true,   "focusable": true,  "disabled": false,
  "blur": 4.0,
  "border": { "width": 1, "color": [0, 0, 0, 0.3] },
  "shadow": { "blur": 8, "offsetX": 0, "offsetY": 2, "color": [0,0,0,0.2], "inset": false },
  "gradient": { "start": [0.2,0.4,0.8,1], "end": [0.1,0.2,0.5,1], "direction": "vertical" },
  "translate": [4, 8],   "scale": 1.1,       "rotate": 0.1,
  "rotateX": 0.05,       "rotateY": 0.05,    "perspective": 800,
  "transformOrigin": [0.5, 0.5],
  "transition": { "duration": 0.25, "ease": "OutCubic", "delay": 0 },
  "scrollState": { "id": "s1", "offset": 0, "maxOffset": 500, "step": 48 },
  "scrollContentFrom": "content-id",
  "onClick": "h_0",
  "onPress": "h_1",       "onRelease": "h_2",
  "onHover": "h_3",
  "onScroll": "h_4",      "onDrag": "h_5",    "onTextInput": "h_6"
}
```

事件属性值为 callback registry 中的 handler ID（由 `NeoUi` 自动生成）。使用 Java builder 时这些 ID 自动填写，无需手动管理。

### 容器特有属性（column/row/stack/flow）

```json
{ "justify": "center", "align": "center" }
```

### text 特有属性

```json
{
  "text": "Hello", "fontSize": 18, "fontWeight": 700, "fontFamily": "Inter",
  "textColor": [0.1, 0.1, 0.1, 1],
  "wrap": true, "lineHeight": 1.5,
  "horizontalAlign": "center",
  "verticalAlign": "center"
}
```

### image 特有属性

```json
{ "source": "assets/logo.png", "fit": "contain", "flipVertically": false }
```

### svg 特有属性

```json
{ "source": "assets/icon.svg" }
```

### polygon 特有属性

```json
{ "points": [[0,0],[100,0],[50,100]] }
```

### loader 特有属性

```json
{ "active": true, "mode": "spin" }
```

## Native 库加载

`NativeLoader` 在 `NeoEngine` 类加载时静态初始化，按以下优先级加载：

1. 系统属性 `eui.neo.native`（绝对路径）
2. JAR 内 `natives/<os>-<arch>/eui_neo_jni.<ext>`，解压到临时目录后 `System.load`
3. `System.loadLibrary("eui_neo_jni")`（从 `java.library.path` 搜索）

```sh
# 覆盖路径示例
java -Deui.neo.native=/opt/eui/eui_neo_jni.so -jar my-app.jar
```

## 已知限制

| 限制 | 说明 |
|------|------|
| 单 engine 实例 | 进程内同时只能存在一个 `NeoEngine`；底层 DSL Runtime 含进程级静态状态 |
| Callback registry 不自动清理 | 重建 UI 树时需创建新 `NeoUi` 实例以释放旧 handler |
| `framesPerSecond` 未生效 | 配置字段存在但帧节流未实现，实际帧率由 `pumpEvents` 超时控制 |
| Java 16+ | `NeoFrameInfo` 使用 `record`；Java 11 需改为普通 `final class` |
| GUI 必须有 display | `initialize()` 在无 display 的 headless 服务器上会失败；CI 须使用 Xvfb |
| 不支持多窗口（Java API） | C ABI 层只暴露主窗口；子窗口/托盘计划后续版本 |
| SDL2/Vulkan 未完整验证 | JNI 构建默认使用 GLFW + OpenGL；SDL2 和 Vulkan backend 编译通过但未完整测试 |

## 参见

- `tests/java/SmokeTest.java` — 无界面功能验证
- `include/eui/neo_c_api.h` — 完整 C ABI 声明
- `docs/skills/eui-neo-jni/SKILL.md` — Claude Code skill（开发参考）

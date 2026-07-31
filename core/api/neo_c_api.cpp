#include "eui/neo_c_api.h"

#include "core/dsl_runtime.h"
#include "core/input/input_state.h"
#include "core/platform/platform.h"
#include "core/render/render_backend.h"
#include "core/window/window_backend.h"
#include "eui/json.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <queue>
#include <string>
#include <thread>

#if defined(EUI_WINDOW_BACKEND_SDL2)
#include <SDL.h>
#if defined(EUI_RENDER_BACKEND_VULKAN)
#include <SDL_vulkan.h>
#endif
#else
#include <GLFW/glfw3.h>
#endif

namespace {

constexpr const char* kVersion = "0.5.3";
std::atomic<eui_neo_engine*> activeEngine{nullptr};

struct UiDocument {
    eui::json::Document document;
    std::string source;
};

std::string stringValue(const eui::json::Value& value, const char* key, const std::string& fallback = {}) {
    std::string result;
    return value.get(key).string(result) ? result : fallback;
}

float numberValue(const eui::json::Value& value, const char* key, float fallback) {
    double result = 0.0;
    return value.get(key).number(result) ? static_cast<float>(result) : fallback;
}

bool boolValue(const eui::json::Value& value, const char* key, bool fallback) {
    bool result = false;
    return value.get(key).boolean(result) ? result : fallback;
}

core::Color colorValue(const eui::json::Value& value, const core::Color& fallback) {
    if (value.type() != eui::json::Type::Array || value.size() < 3) return fallback;
    core::Color result = fallback;
    double c = 0.0;
    if (value.at(0).number(c)) result.r = static_cast<float>(c);
    if (value.at(1).number(c)) result.g = static_cast<float>(c);
    if (value.at(2).number(c)) result.b = static_cast<float>(c);
    if (value.size() > 3 && value.at(3).number(c)) result.a = static_cast<float>(c);
    return result;
}

core::Align alignValue(const std::string& s) {
    if (s == "center") return core::Align::CENTER;
    if (s == "end")    return core::Align::END;
    return core::Align::START;
}

float sanitizeScale(float value) {
    return std::isfinite(value) && value > 0.0f ? value : 1.0f;
}

} // namespace

struct eui_neo_engine {
    eui_neo_config config{};
    std::string title;
    std::string pageId;
    UiDocument ui;
    std::string lastError;
    std::thread::id ownerThread;
    core::window::Handle window = nullptr;
    std::unique_ptr<core::render::RenderBackend> renderBackend;
    core::dsl::Runtime runtime;
    std::atomic<bool> updateRequested{true};
    std::atomic<bool> platformInitialized{false};
    std::mutex platformMutex;
    std::queue<eui_neo_event> eventQueue;
    std::string lastEventHandlerId;
    std::string lastEventTextInput;
    bool initialized = false;
    bool running = false;
    bool composed = false;
    float logicalWidth = 0.0f;
    float logicalHeight = 0.0f;
    double lastFrameTime = 0.0;
    uint64_t frameNumber = 0;
};

namespace {

thread_local std::string g_createError;
thread_local std::string g_clipboard_buffer;

void setError(eui_neo_engine* engine, const std::string& message) {
    if (engine != nullptr) engine->lastError = message;
}

eui_neo_result requireEngine(eui_neo_engine* engine) {
    if (engine == nullptr) return EUI_NEO_INVALID_ARGUMENT;
    if (engine->ownerThread != std::this_thread::get_id()) {
        setError(engine, "The engine must be used from the thread that created it.");
        return EUI_NEO_WRONG_THREAD;
    }
    return EUI_NEO_OK;
}

bool parseUi(eui_neo_engine* engine, const char* source) {
    const char* json = source != nullptr && source[0] != '\0'
        ? source
        : R"({"type":"column","id":"root","width":"fill","height":"fill","padding":32,"gap":12,"children":[{"type":"text","id":"title","text":"EUI-NEO for Java","fontSize":28},{"type":"text","id":"message","text":"Call setUiJson or setUi to set the UI.","fontSize":16}]})";
    UiDocument replacement;
    replacement.source = json;
    if (!replacement.document.parse(replacement.source)) {
        const eui::json::ParseError& error = replacement.document.error();
        setError(engine, "UI JSON parse error at byte " + std::to_string(error.offset) + ": " + error.message);
        return false;
    }
    if (replacement.document.root().type() != eui::json::Type::Object) {
        setError(engine, "The UI JSON root must be an object.");
        return false;
    }
    engine->ui = std::move(replacement);
    engine->composed = false;
    engine->updateRequested.store(true, std::memory_order_release);
    return true;
}

static core::Ease easeFromString(const std::string& s) {
    if (s == "Linear")     return core::Ease::Linear;
    if (s == "InQuad")     return core::Ease::InQuad;
    if (s == "OutQuad")    return core::Ease::OutQuad;
    if (s == "InOutQuad")  return core::Ease::InOutQuad;
    if (s == "InOutCubic") return core::Ease::InOutCubic;
    if (s == "OutExpo")    return core::Ease::OutExpo;
    if (s == "OutBack")    return core::Ease::OutBack;
    return core::Ease::OutCubic;
}

void applyCommon(core::dsl::Element& element, const eui::json::Value& node) {
    // width / height
    double num = 0.0;
    std::string mode;
    const eui::json::Value width = node.get("width");
    const eui::json::Value height = node.get("height");
    if (width.number(num))  element.width  = core::SizeValue::fixed(static_cast<float>(num));
    else if (width.string(mode)  && mode == "fill") element.width  = core::SizeValue::fill();
    else if (mode == "wrap") element.width  = core::SizeValue::wrapContent();
    mode.clear();
    if (height.number(num)) element.height = core::SizeValue::fixed(static_cast<float>(num));
    else if (height.string(mode) && mode == "fill") element.height = core::SizeValue::fill();
    else if (mode == "wrap") element.height = core::SizeValue::wrapContent();

    // absolute position
    if (node.get("x").number(num)) { element.hasX = true; element.x = static_cast<float>(num); }
    if (node.get("y").number(num)) { element.hasY = true; element.y = static_cast<float>(num); }

    // padding (uniform, then per-axis, then per-side)
    float pad = numberValue(node, "padding", 0.0f);
    float padX = numberValue(node, "paddingX", pad);
    float padY = numberValue(node, "paddingY", pad);
    element.padding = {
        std::max(0.0f, numberValue(node, "paddingLeft",   padX)),
        std::max(0.0f, numberValue(node, "paddingTop",    padY)),
        std::max(0.0f, numberValue(node, "paddingRight",  padX)),
        std::max(0.0f, numberValue(node, "paddingBottom", padY))
    };

    // margin (uniform, then per-axis, then per-side)
    float mar = numberValue(node, "margin", 0.0f);
    float marX = numberValue(node, "marginX", mar);
    float marY = numberValue(node, "marginY", mar);
    element.margin = {
        std::max(0.0f, numberValue(node, "marginLeft",   marX)),
        std::max(0.0f, numberValue(node, "marginTop",    marY)),
        std::max(0.0f, numberValue(node, "marginRight",  marX)),
        std::max(0.0f, numberValue(node, "marginBottom", marY))
    };

    element.spacing      = std::max(0.0f, numberValue(node, "gap",      element.spacing));
    element.lineSpacing  = std::max(0.0f, numberValue(node, "lineGap",  element.lineSpacing));
    element.radius       = std::max(0.0f, numberValue(node, "radius",   element.radius));
    element.opacity      = std::clamp(numberValue(node, "opacity", element.opacity), 0.0f, 1.0f);
    element.flexGrow     = std::max(0.0f, numberValue(node, "flexGrow",   element.flexGrow));
    element.flexShrink   = std::max(0.0f, numberValue(node, "flexShrink", element.flexShrink));
    element.clip         = boolValue(node, "clip", element.clip);

    // size constraints & layout
    if (node.get("minWidth").number(num))  element.minLayoutWidth  = std::max(0.0f, static_cast<float>(num));
    if (node.get("minHeight").number(num)) element.minLayoutHeight = std::max(0.0f, static_cast<float>(num));
    if (node.get("maxWidth").number(num))  element.maxLayoutWidth  = std::max(0.0f, static_cast<float>(num));
    if (node.get("maxHeight").number(num)) element.maxLayoutHeight = std::max(0.0f, static_cast<float>(num));
    if (node.get("zIndex").number(num))    element.zIndex          = static_cast<int>(num);
    element.ignoreLayout = boolValue(node, "ignoreLayout", element.ignoreLayout);

    // color + align
    element.color      = colorValue(node.get("color"), element.color);
    element.mainAlign  = alignValue(stringValue(node, "justify"));
    element.crossAlign = alignValue(stringValue(node, "align"));

    // transforms
    const eui::json::Value trans = node.get("translate");
    if (trans.type() == eui::json::Type::Array && trans.size() >= 2) {
        double tx = 0, ty = 0; trans.at(0).number(tx); trans.at(1).number(ty);
        element.transform.translate = {static_cast<float>(tx), static_cast<float>(ty)};
        if (trans.size() >= 3) { double tz = 0; trans.at(2).number(tz); element.transform.translateZ = static_cast<float>(tz); }
    }
    const eui::json::Value scaleNode = node.get("scale");
    if (scaleNode.number(num)) { element.transform.scale = {static_cast<float>(num), static_cast<float>(num)}; }
    else if (scaleNode.type() == eui::json::Type::Array && scaleNode.size() >= 2) {
        double sx = 1, sy = 1; scaleNode.at(0).number(sx); scaleNode.at(1).number(sy);
        element.transform.scale = {static_cast<float>(sx), static_cast<float>(sy)};
    }
    if (node.get("rotate").number(num))    element.transform.rotate    = static_cast<float>(num);
    if (node.get("rotateX").number(num))   element.transform.rotateX   = static_cast<float>(num);
    if (node.get("rotateY").number(num))   element.transform.rotateY   = static_cast<float>(num);
    if (node.get("perspective").number(num)) element.transform.perspective = static_cast<float>(num);
    const eui::json::Value origin = node.get("transformOrigin");
    if (origin.type() == eui::json::Type::Array && origin.size() >= 2) {
        double ox = 0.5, oy = 0.5; origin.at(0).number(ox); origin.at(1).number(oy);
        element.transform.origin = {static_cast<float>(ox), static_cast<float>(oy)};
    }

    // visual: blur, shadow, gradient, border
    if (node.get("blur").number(num)) element.blur = std::max(0.0f, static_cast<float>(num));

    const eui::json::Value shadowNode = node.get("shadow");
    if (shadowNode.type() == eui::json::Type::Object) {
        element.shadow.enabled  = true;
        element.shadow.blur     = std::max(0.0f, numberValue(shadowNode, "blur",    0.0f));
        element.shadow.offset.x = numberValue(shadowNode, "offsetX", 0.0f);
        element.shadow.offset.y = numberValue(shadowNode, "offsetY", 4.0f);
        element.shadow.color    = colorValue(shadowNode.get("color"), {0.0f, 0.0f, 0.0f, 0.5f});
        element.shadow.inset    = boolValue(shadowNode, "inset", false);
    }

    const eui::json::Value gradNode = node.get("gradient");
    if (gradNode.type() == eui::json::Type::Object) {
        element.gradient.enabled   = true;
        element.gradient.start     = colorValue(gradNode.get("start"), {0.0f, 0.0f, 0.0f, 1.0f});
        element.gradient.end       = colorValue(gradNode.get("end"),   {1.0f, 1.0f, 1.0f, 1.0f});
        const std::string dir      = stringValue(gradNode, "direction", "vertical");
        element.gradient.direction = (dir == "horizontal")
            ? core::GradientDirection::Horizontal : core::GradientDirection::Vertical;
    }

    const eui::json::Value borderNode = node.get("border");
    if (borderNode.type() == eui::json::Type::Object) {
        element.border.width = std::max(0.0f, numberValue(borderNode, "width", 1.0f));
        element.border.color = colorValue(borderNode.get("color"), {1.0f, 1.0f, 1.0f, 1.0f});
    }

    // state colors
    const eui::json::Value hoverCol   = node.get("hoverColor");
    const eui::json::Value pressedCol = node.get("pressedColor");
    if (hoverCol.type()   == eui::json::Type::Array) { element.hoverColor   = colorValue(hoverCol,   element.hoverColor);   element.hasStateColors = true; }
    if (pressedCol.type() == eui::json::Type::Array) { element.pressedColor = colorValue(pressedCol, element.pressedColor); element.hasStateColors = true; }

    // interaction flags
    bool interactiveVal = false;
    if (node.get("interactive").boolean(interactiveVal) && interactiveVal) { element.interactive = true; element.cursor = core::CursorShape::Hand; }
    if (node.get("focusable").boolean(interactiveVal)   && interactiveVal) { element.focusable   = true; element.interactive = true; }
    if (node.get("disabled").boolean(interactiveVal))   element.disabled = interactiveVal;

    // transition
    const eui::json::Value trNode = node.get("transition");
    if (trNode.type() == eui::json::Type::Object) {
        element.transition = core::Transition::make(
            numberValue(trNode, "duration", 0.3f),
            easeFromString(stringValue(trNode, "ease")));
        element.transition.delaySeconds = numberValue(trNode, "delay", 0.0f);
    }

    // scroll state
    const eui::json::Value scrollNode = node.get("scrollState");
    if (scrollNode.type() == eui::json::Type::Object) {
        element.scrollStateId   = stringValue(scrollNode, "id");
        element.scrollOffset    = numberValue(scrollNode, "offset",    0.0f);
        element.scrollMaxOffset = numberValue(scrollNode, "maxOffset", 0.0f);
        element.scrollStep      = numberValue(scrollNode, "step",      48.0f);
    }
    const std::string scrollContent = stringValue(node, "scrollContentFrom");
    if (!scrollContent.empty()) element.scrollContentSourceId = scrollContent;
}

void applyEventCallbacks(core::dsl::Element& element, eui_neo_engine* engine, const eui::json::Value& node) {
    auto pushEvent = [engine](uint32_t type, const std::string& id,
                              float x = 0, float y = 0, float dx = 0, float dy = 0,
                              const std::string& text = {}) {
        eui_neo_event evt{};
        evt.size = sizeof(evt);
        evt.type = type;
        std::strncpy(evt.handler_id, id.c_str(), sizeof(evt.handler_id) - 1);
        evt.x = x; evt.y = y; evt.delta_x = dx; evt.delta_y = dy;
        if (!text.empty()) std::strncpy(evt.text_input, text.c_str(), sizeof(evt.text_input) - 1);
        engine->eventQueue.push(evt);
    };

    const std::string onClickId = stringValue(node, "onClick");
    if (!onClickId.empty()) {
        element.onClick = [pushEvent, onClickId] { pushEvent(EUI_NEO_EVENT_CLICK, onClickId); };
        element.interactive = true;
        element.cursor = core::CursorShape::Hand;
    }

    const std::string onPressId = stringValue(node, "onPress");
    if (!onPressId.empty()) {
        element.onPress = [pushEvent, onPressId](const core::PointerEvent& e, const core::Rect&) {
            pushEvent(EUI_NEO_EVENT_PRESS, onPressId, static_cast<float>(e.x), static_cast<float>(e.y));
        };
        element.interactive = true;
        element.cursor = core::CursorShape::Hand;
    }

    const std::string onReleaseId = stringValue(node, "onRelease");
    if (!onReleaseId.empty()) {
        element.onRelease = [pushEvent, onReleaseId](const core::PointerEvent& e, const core::Rect&) {
            pushEvent(EUI_NEO_EVENT_RELEASE, onReleaseId, static_cast<float>(e.x), static_cast<float>(e.y));
        };
        element.interactive = true;
        element.cursor = core::CursorShape::Hand;
    }

    const std::string onHoverId = stringValue(node, "onHover");
    if (!onHoverId.empty()) {
        element.onHoverChanged = [pushEvent, onHoverId](bool entered) {
            pushEvent(entered ? EUI_NEO_EVENT_HOVER_ENTER : EUI_NEO_EVENT_HOVER_LEAVE, onHoverId);
        };
        element.interactive = true;
    }

    const std::string onScrollId = stringValue(node, "onScroll");
    if (!onScrollId.empty()) {
        element.onScroll = [pushEvent, onScrollId](const core::ScrollEvent& e) {
            pushEvent(EUI_NEO_EVENT_SCROLL, onScrollId, 0, 0,
                      static_cast<float>(e.x), static_cast<float>(e.y));
        };
    }

    const std::string onDragId = stringValue(node, "onDrag");
    if (!onDragId.empty()) {
        element.onDrag = [pushEvent, onDragId](const core::dsl::DragEvent& e) {
            pushEvent(EUI_NEO_EVENT_DRAG, onDragId,
                      static_cast<float>(e.x), static_cast<float>(e.y),
                      static_cast<float>(e.deltaX), static_cast<float>(e.deltaY));
        };
    }

    const std::string onTextId = stringValue(node, "onTextInput");
    if (!onTextId.empty()) {
        element.onTextInput = [pushEvent, onTextId](const core::KeyboardEvent& e) {
            pushEvent(EUI_NEO_EVENT_TEXT_INPUT, onTextId, 0, 0, 0, 0, e.text);
        };
        element.focusable   = true;
        element.interactive = true;
    }
}

void composeNode(core::dsl::Ui& ui, eui_neo_engine* engine, const eui::json::Value& node, const std::string& fallbackId);

template <typename Builder>
void configureNodeBuilder(Builder& builder, core::dsl::Ui& ui, eui_neo_engine* engine,
                          const eui::json::Value& node, const std::string& type, const std::string& id) {
    core::dsl::Element* element = builder.element();
    applyCommon(*element, node);
    applyEventCallbacks(*element, engine, node);

    if (type == "text") {
        element->text        = stringValue(node, "text");
        element->fontSize    = std::max(1.0f, numberValue(node, "fontSize", 16.0f));
        element->fontWeight  = static_cast<int>(numberValue(node, "fontWeight", 400.0f));
        element->fontFamily  = stringValue(node, "fontFamily");
        element->textColor   = colorValue(node.get("textColor"), element->textColor);
        element->wrap        = boolValue(node, "wrap", element->wrap);
        element->lineHeight  = numberValue(node, "lineHeight", element->lineHeight);
        if (node.get("maxWidth").type() != eui::json::Type::Null) {
            double mv = 0; if (node.get("maxWidth").number(mv)) element->maxWidth = static_cast<float>(mv);
        }
        const std::string ha = stringValue(node, "horizontalAlign");
        if (ha == "center") element->horizontalAlign = core::HorizontalAlign::Center;
        else if (ha == "right") element->horizontalAlign = core::HorizontalAlign::Right;
        const std::string va = stringValue(node, "verticalAlign");
        if (va == "center") element->verticalAlign = core::VerticalAlign::Center;
        else if (va == "bottom") element->verticalAlign = core::VerticalAlign::Bottom;
    } else if (type == "image") {
        element->imageSource = stringValue(node, "source");
        element->imageFlipVertically = boolValue(node, "flipVertically", false);
        const std::string fit = stringValue(node, "fit");
        if (fit == "contain") element->imageFit = core::ImageFit::Contain;
        else if (fit == "stretch") element->imageFit = core::ImageFit::Stretch;
        else element->imageFit = core::ImageFit::Cover;
    } else if (type == "svg") {
        element->svgSource = stringValue(node, "source");
    } else if (type == "polygon") {
        const eui::json::Value pts = node.get("points");
        if (pts.type() == eui::json::Type::Array) {
            for (std::size_t i = 0; i < pts.size(); ++i) {
                const eui::json::Value& pt = pts.at(i);
                if (pt.type() == eui::json::Type::Array && pt.size() >= 2) {
                    double px = 0, py = 0; pt.at(0).number(px); pt.at(1).number(py);
                    element->polygonPoints.push_back({static_cast<float>(px), static_cast<float>(py)});
                }
            }
        }
    }

    const eui::json::Value children = node.get("children");
    if (children.type() == eui::json::Type::Array && children.size() > 0) {
        builder.content([&] {
            for (std::size_t i = 0; i < children.size(); ++i)
                composeNode(ui, engine, children.at(i), id + "." + std::to_string(i));
        });
    }
}

void composeNode(core::dsl::Ui& ui, eui_neo_engine* engine, const eui::json::Value& node, const std::string& fallbackId) {
    if (node.type() != eui::json::Type::Object) return;
    const std::string type = stringValue(node, "type", "column");
    const std::string id   = stringValue(node, "id",   fallbackId);
    if (type == "row")    { auto b = ui.row(id);    configureNodeBuilder(b, ui, engine, node, type, id); }
    else if (type == "column") { auto b = ui.column(id); configureNodeBuilder(b, ui, engine, node, type, id); }
    else if (type == "stack")  { auto b = ui.stack(id);  configureNodeBuilder(b, ui, engine, node, type, id); }
    else if (type == "flow")   { auto b = ui.flow(id);   configureNodeBuilder(b, ui, engine, node, type, id); }
    else if (type == "rect")   { auto b = ui.rect(id);   configureNodeBuilder(b, ui, engine, node, type, id); }
    else if (type == "text")   { auto b = ui.text(id);   configureNodeBuilder(b, ui, engine, node, type, id); }
    else if (type == "image")  { auto b = ui.image(id);  configureNodeBuilder(b, ui, engine, node, type, id); }
    else if (type == "svg")    { auto b = ui.image(id);  configureNodeBuilder(b, ui, engine, node, "svg", id); }
    else if (type == "polygon"){ auto b = ui.rect(id);   configureNodeBuilder(b, ui, engine, node, type, id); }
    else { auto b = ui.column(id); configureNodeBuilder(b, ui, engine, node, "column", id); }
}

void composeUi(eui_neo_engine* engine, float logicalWidth, float logicalHeight) {
    engine->runtime.compose(engine->pageId, logicalWidth, logicalHeight,
        [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
            composeNode(ui, engine, engine->ui.document.root(), "root");
        });
    engine->composed    = true;
    engine->logicalWidth  = logicalWidth;
    engine->logicalHeight = logicalHeight;
}

#if defined(EUI_WINDOW_BACKEND_SDL2)
void framebufferSize(eui_neo_engine* engine, int& width, int& height) {
#  if defined(EUI_RENDER_BACKEND_VULKAN)
    SDL_Vulkan_GetDrawableSize(static_cast<SDL_Window*>(engine->window), &width, &height);
#  else
    SDL_GL_GetDrawableSize(static_cast<SDL_Window*>(engine->window), &width, &height);
#  endif
}
float pointerScale(eui_neo_engine* engine) {
    int ww = 0, wh = 0, w = 0, h = 0;
    SDL_GetWindowSize(static_cast<SDL_Window*>(engine->window), &ww, &wh);
    framebufferSize(engine, w, h);
    return ww > 0 && wh > 0
        ? sanitizeScale((static_cast<float>(w)/ww + static_cast<float>(h)/wh) * 0.5f) : 1.0f;
}
#else
void framebufferSize(eui_neo_engine* engine, int& width, int& height) {
    glfwGetFramebufferSize(static_cast<GLFWwindow*>(engine->window), &width, &height);
}
float pointerScale(eui_neo_engine* engine) {
    int ww = 0, wh = 0, w = 0, h = 0;
    GLFWwindow* win = static_cast<GLFWwindow*>(engine->window);
    glfwGetWindowSize(win, &ww, &wh);
    glfwGetFramebufferSize(win, &w, &h);
    return ww > 0 && wh > 0
        ? sanitizeScale((static_cast<float>(w)/ww + static_cast<float>(h)/wh) * 0.5f) : 1.0f;
}
#endif

float dpiScale(eui_neo_engine* engine) {
#if defined(EUI_WINDOW_BACKEND_SDL2)
    return pointerScale(engine);
#else
    float x = 1.0f, y = 1.0f;
    glfwGetWindowContentScale(static_cast<GLFWwindow*>(engine->window), &x, &y);
    return sanitizeScale((x + y) * 0.5f);
#endif
}

void shutdownInternal(eui_neo_engine* engine) {
    if (engine == nullptr) return;
    if (engine->initialized && engine->renderBackend) {
        engine->renderBackend->makeCurrent();
        engine->runtime.shutdown();
        engine->renderBackend->releaseRenderCache();
    }
    if (engine->window != nullptr) {
        core::releaseInputQueue(engine->window);
        core::window::destroyWindow(engine->window);
        engine->window = nullptr;
    }
    engine->renderBackend.reset();
    const bool wasInitialized = [&] {
        std::lock_guard<std::mutex> lock(engine->platformMutex);
        const bool was = engine->platformInitialized.load(std::memory_order_relaxed);
        engine->platformInitialized.store(false, std::memory_order_relaxed);
        return was;
    }();
#if defined(EUI_WINDOW_BACKEND_SDL2)
    if (wasInitialized) SDL_Quit();
#else
    if (wasInitialized) glfwTerminate();
#endif
    engine->initialized = false;
    engine->running     = false;
    eui_neo_engine* expected = engine;
    activeEngine.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
}

} // namespace

extern "C" {

uint32_t eui_neo_api_version(void) { return EUI_NEO_C_API_VERSION; }
const char* eui_neo_version_string(void) { return kVersion; }

void eui_neo_config_init(eui_neo_config* config) {
    if (config == nullptr) return;
    std::memset(config, 0, sizeof(*config));
    config->size              = sizeof(*config);
    config->version           = EUI_NEO_C_API_VERSION;
    config->title_utf8        = "EUI-NEO Java";
    config->page_id_utf8      = "java";
    config->width             = 960;
    config->height            = 640;
    config->frames_per_second = 60.0;
    config->clear_color_r     = 0.16f;
    config->clear_color_g     = 0.18f;
    config->clear_color_b     = 0.20f;
    config->clear_color_a     = 1.0f;
    config->resizable         = 1;
    config->decorated         = 1;
}

void eui_neo_frame_info_init(eui_neo_frame_info* fi) {
    if (fi == nullptr) return;
    std::memset(fi, 0, sizeof(*fi));
    fi->size    = sizeof(*fi);
    fi->version = EUI_NEO_C_API_VERSION;
}

void eui_neo_event_init(eui_neo_event* event) {
    if (event == nullptr) return;
    std::memset(event, 0, sizeof(*event));
    event->size = sizeof(*event);
}

const char* eui_neo_create_last_error(void) {
    return g_createError.c_str();
}

eui_neo_engine* eui_neo_create(const eui_neo_config* config) {
    eui_neo_config resolved;
    eui_neo_config_init(&resolved);
    if (config != nullptr) {
        if (config->size < sizeof(eui_neo_config) || config->version != EUI_NEO_C_API_VERSION) {
            g_createError = "Invalid config: size or version mismatch.";
            return nullptr;
        }
        resolved = *config;
    }
    std::unique_ptr<eui_neo_engine> engine(new (std::nothrow) eui_neo_engine());
    if (!engine) { g_createError = "Memory allocation failed."; return nullptr; }
    engine->config      = resolved;
    engine->title       = resolved.title_utf8   != nullptr ? resolved.title_utf8   : "EUI-NEO Java";
    engine->pageId      = resolved.page_id_utf8 != nullptr ? resolved.page_id_utf8 : "java";
    engine->ownerThread = std::this_thread::get_id();
    if (!parseUi(engine.get(), resolved.ui_json_utf8)) {
        g_createError = engine->lastError;
        return nullptr;
    }
    g_createError.clear();
    return engine.release();
}

eui_neo_result eui_neo_initialize(eui_neo_engine* engine) {
    eui_neo_result check = requireEngine(engine);
    if (check != EUI_NEO_OK) return check;
    if (engine->initialized) { setError(engine, "The engine is already initialized."); return EUI_NEO_INVALID_STATE; }
    eui_neo_engine* expected = nullptr;
    if (!activeEngine.compare_exchange_strong(expected, engine, std::memory_order_acq_rel)) {
        setError(engine, "Only one EUI-NEO engine may be active in a process.");
        return EUI_NEO_INVALID_STATE;
    }
    core::platform::repairCurrentWorkingDirectory();
#if defined(EUI_WINDOW_BACKEND_SDL2)
#  if defined(EUI_RENDER_BACKEND_VULKAN)
    core::render::initializeRenderBackendLoader();
#  endif
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        setError(engine, SDL_GetError());
        activeEngine.store(nullptr, std::memory_order_release);
        return EUI_NEO_PLATFORM_ERROR;
    }
#else
    core::render::initializeRenderBackendLoader();
    if (!glfwInit()) {
        setError(engine, "GLFW initialization failed.");
        activeEngine.store(nullptr, std::memory_order_release);
        return EUI_NEO_PLATFORM_ERROR;
    }
#endif
    engine->platformInitialized = true;
    core::window::WindowCreateRequest request;
    request.width     = std::max(160, engine->config.width);
    request.height    = std::max(120, engine->config.height);
    request.title     = engine->title.c_str();
    request.resizable = engine->config.resizable != 0;
    request.decorated = engine->config.decorated != 0;
    request.renderApi = core::render::windowRenderApi();
    engine->window    = core::window::createWindow(request);
    if (engine->window == nullptr) { setError(engine, "Native window creation failed."); shutdownInternal(engine); return EUI_NEO_PLATFORM_ERROR; }
    if (engine->config.decorated == 0) {
        core::window::centerWindow(engine->window, request.width, request.height);
    }
#if !defined(EUI_WINDOW_BACKEND_SDL2)
    core::installInputCallbacks(engine->window);
    glfwSetWindowUserPointer(static_cast<GLFWwindow*>(engine->window), engine);
    glfwSetFramebufferSizeCallback(static_cast<GLFWwindow*>(engine->window), [](GLFWwindow* w, int, int) {
        auto* e = static_cast<eui_neo_engine*>(glfwGetWindowUserPointer(w));
        if (e) e->updateRequested.store(true, std::memory_order_release);
    });
    glfwSetWindowContentScaleCallback(static_cast<GLFWwindow*>(engine->window), [](GLFWwindow* w, float, float) {
        auto* e = static_cast<eui_neo_engine*>(glfwGetWindowUserPointer(w));
        if (e) e->updateRequested.store(true, std::memory_order_release);
    });
    glfwSetWindowFocusCallback(static_cast<GLFWwindow*>(engine->window), [](GLFWwindow* w, int) {
        auto* e = static_cast<eui_neo_engine*>(glfwGetWindowUserPointer(w));
        if (e) e->updateRequested.store(true, std::memory_order_release);
    });
    glfwSetWindowIconifyCallback(static_cast<GLFWwindow*>(engine->window), [](GLFWwindow* w, int) {
        auto* e = static_cast<eui_neo_engine*>(glfwGetWindowUserPointer(w));
        if (e) e->updateRequested.store(true, std::memory_order_release);
    });
    glfwSetWindowRefreshCallback(static_cast<GLFWwindow*>(engine->window), [](GLFWwindow* w) {
        auto* e = static_cast<eui_neo_engine*>(glfwGetWindowUserPointer(w));
        if (e) e->updateRequested.store(true, std::memory_order_release);
    });
#endif
    engine->renderBackend = core::render::createRenderBackend(engine->window);
    if (!engine->renderBackend || !engine->renderBackend->initialize()) {
        setError(engine, "Render backend initialization failed."); shutdownInternal(engine); return EUI_NEO_PLATFORM_ERROR;
    }
    if (!engine->runtime.initialize(engine->window)) {
        setError(engine, "EUI runtime initialization failed."); shutdownInternal(engine); return EUI_NEO_PLATFORM_ERROR;
    }
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_StartTextInput();
#endif
    engine->lastFrameTime = core::window::timeSeconds();
    engine->initialized   = true;
    engine->running       = true;
    engine->lastError.clear();
    return EUI_NEO_OK;
}

eui_neo_result eui_neo_pump_events(eui_neo_engine* engine, int32_t waitMs) {
    eui_neo_result check = requireEngine(engine);
    if (check != EUI_NEO_OK) return check;
    if (!engine->initialized) return EUI_NEO_INVALID_STATE;
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_Event event{};
    if (waitMs > 0 && SDL_WaitEventTimeout(&event, waitMs)) SDL_PushEvent(&event);
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE))
            engine->running = false;
        else if (event.type == SDL_TEXTINPUT)    { core::queueTextInput(engine->window, event.text.text);   engine->updateRequested.store(true, std::memory_order_release); }
        else if (event.type == SDL_TEXTEDITING)  { core::queueTextEditing(engine->window, event.edit.text); engine->updateRequested.store(true, std::memory_order_release); }
        else if (event.type == SDL_MOUSEWHEEL)   { core::queueScrollInput(engine->window, event.wheel.preciseX, event.wheel.preciseY); engine->updateRequested.store(true, std::memory_order_release); }
        else if (event.type == SDL_KEYDOWN) {
            const bool ctrl  = (event.key.keysym.mod & (KMOD_CTRL | KMOD_GUI)) != 0;
            const bool shift = (event.key.keysym.mod & KMOD_SHIFT) != 0;
            switch (event.key.keysym.sym) {
            case SDLK_BACKSPACE: core::queueKeyInput(engine->window, core::InputKey::Backspace, ctrl, shift); break;
            case SDLK_DELETE:    core::queueKeyInput(engine->window, core::InputKey::Delete,    ctrl, shift); break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:  core::queueKeyInput(engine->window, core::InputKey::Enter,     ctrl, shift); break;
            case SDLK_LEFT:      core::queueKeyInput(engine->window, core::InputKey::Left,       ctrl, shift); break;
            case SDLK_RIGHT:     core::queueKeyInput(engine->window, core::InputKey::Right,      ctrl, shift); break;
            case SDLK_UP:        core::queueKeyInput(engine->window, core::InputKey::Up,         ctrl, shift); break;
            case SDLK_DOWN:      core::queueKeyInput(engine->window, core::InputKey::Down,       ctrl, shift); break;
            case SDLK_HOME:      core::queueKeyInput(engine->window, core::InputKey::Home,       ctrl, shift); break;
            case SDLK_END:       core::queueKeyInput(engine->window, core::InputKey::End,        ctrl, shift); break;
            case SDLK_ESCAPE:    core::queueKeyInput(engine->window, core::InputKey::Escape,     ctrl, shift); break;
            case SDLK_a:         core::queueKeyInput(engine->window, core::InputKey::A,          ctrl, shift); break;
            case SDLK_c:         core::queueKeyInput(engine->window, core::InputKey::C,          ctrl, shift); break;
            case SDLK_v:         core::queueKeyInput(engine->window, core::InputKey::V,          ctrl, shift); break;
            case SDLK_x:         core::queueKeyInput(engine->window, core::InputKey::X,          ctrl, shift); break;
            case SDLK_y:         core::queueKeyInput(engine->window, core::InputKey::Y,          ctrl, shift); break;
            case SDLK_z:         core::queueKeyInput(engine->window, core::InputKey::Z,          ctrl, shift); break;
            default: break;
            }
            engine->updateRequested.store(true, std::memory_order_release);
        }
        else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP ||
                 event.type == SDL_MOUSEMOTION)  { engine->updateRequested.store(true, std::memory_order_release); }
        else if (event.type == SDL_WINDOWEVENT)  { engine->updateRequested.store(true, std::memory_order_release); }
    }
#else
    if (waitMs > 0) glfwWaitEventsTimeout(static_cast<double>(waitMs) / 1000.0);
    else glfwPollEvents();
    engine->running = glfwWindowShouldClose(static_cast<GLFWwindow*>(engine->window)) == GLFW_FALSE;
#endif
    return EUI_NEO_OK;
}

eui_neo_result eui_neo_frame(eui_neo_engine* engine, eui_neo_frame_info* frameInfo) {
    eui_neo_result check = requireEngine(engine);
    if (check != EUI_NEO_OK) return check;
    if (!engine->initialized) return EUI_NEO_INVALID_STATE;
    if (frameInfo && (frameInfo->size < sizeof(*frameInfo) || frameInfo->version != EUI_NEO_C_API_VERSION)) {
        setError(engine, "Invalid frame info structure."); return EUI_NEO_INVALID_ARGUMENT;
    }
    const double frameStart = core::window::timeSeconds();
    int width = 0, height = 0;
    framebufferSize(engine, width, height);
    const float dpi     = dpiScale(engine);
    const float pointer = pointerScale(engine);
    bool rendered = false;
    if (engine->running && width > 0 && height > 0) {
        const float lw = static_cast<float>(width) / dpi;
        const float lh = static_cast<float>(height) / dpi;
        const bool requested = engine->updateRequested.exchange(false, std::memory_order_acq_rel);
        if (!engine->composed || requested || lw != engine->logicalWidth || lh != engine->logicalHeight)
            { composeUi(engine, lw, lh); engine->runtime.requestFullPaint(); }
        const double now   = core::window::timeSeconds();
        const float  delta = static_cast<float>(std::max(0.0, now - engine->lastFrameTime));
        engine->lastFrameTime = now;
        const bool changed = engine->runtime.update(engine->window, delta, pointer, dpi, true);
        if (changed || requested || engine->runtime.paintRequested() || engine->runtime.isAnimating()) {
            engine->renderBackend->makeCurrent();
            engine->renderBackend->beginFrame({engine->window, core::window::nativeWindowInfo(engine->window), width, height, dpi});
            core::render::ScopedRenderBackend scoped(*engine->renderBackend);
            engine->runtime.render(width, height, dpi, {engine->config.clear_color_r, engine->config.clear_color_g, engine->config.clear_color_b, engine->config.clear_color_a});
            engine->renderBackend->present();
            core::render::publishRenderFrameStats();
            rendered = true;
        }
        ++engine->frameNumber;
    }
    if (frameInfo) {
        frameInfo->frame_number      = engine->frameNumber;
        frameInfo->framebuffer_width  = width;
        frameInfo->framebuffer_height = height;
        frameInfo->dpi_scale          = dpi;
        frameInfo->rendered           = rendered ? 1 : 0;
        frameInfo->running            = engine->running ? 1 : 0;
    }
    if (engine->config.frames_per_second > 1.0) {
        const double targetMs  = 1000.0 / engine->config.frames_per_second;
        const double elapsedMs = (core::window::timeSeconds() - frameStart) * 1000.0;
        const int64_t sleepUs  = static_cast<int64_t>((targetMs - elapsedMs) * 1000.0);
        if (sleepUs > 500)
            std::this_thread::sleep_for(std::chrono::microseconds(sleepUs));
    }
    return EUI_NEO_OK;
}

eui_neo_result eui_neo_set_ui_json(eui_neo_engine* engine, const char* uiJsonUtf8) {
    eui_neo_result check = requireEngine(engine);
    if (check != EUI_NEO_OK) return check;
    if (uiJsonUtf8 == nullptr) return EUI_NEO_INVALID_ARGUMENT;
    return parseUi(engine, uiJsonUtf8) ? EUI_NEO_OK : EUI_NEO_PARSE_ERROR;
}

eui_neo_result eui_neo_request_update(eui_neo_engine* engine) {
    if (engine == nullptr) return EUI_NEO_INVALID_ARGUMENT;
    engine->updateRequested.store(true, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(engine->platformMutex);
        if (engine->platformInitialized.load(std::memory_order_relaxed))
            core::window::postEmptyEvent();
    }
    return EUI_NEO_OK;
}

int32_t eui_neo_is_running(const eui_neo_engine* engine) {
    return engine != nullptr && engine->running ? 1 : 0;
}

eui_neo_result eui_neo_shutdown(eui_neo_engine* engine) {
    eui_neo_result check = requireEngine(engine);
    if (check != EUI_NEO_OK) return check;
    shutdownInternal(engine);
    return EUI_NEO_OK;
}

void eui_neo_destroy(eui_neo_engine* engine) {
    if (engine == nullptr) return;
    if (engine->ownerThread != std::this_thread::get_id()) {
        if (engine->initialized) {
            // GPU/window resources can only be released from the owner thread.
            // Call eui_neo_shutdown() then eui_neo_destroy() from the owner thread.
            return;
        }
        delete engine;
        return;
    }
    shutdownInternal(engine);
    delete engine;
}

const char* eui_neo_last_error(const eui_neo_engine* engine) {
    return engine != nullptr ? engine->lastError.c_str() : "Invalid EUI-NEO engine handle.";
}

eui_neo_result eui_neo_poll_event(eui_neo_engine* engine, eui_neo_event* outEvent) {
    if (engine == nullptr || outEvent == nullptr) return EUI_NEO_INVALID_ARGUMENT;
    eui_neo_event_init(outEvent);
    if (engine->eventQueue.empty()) return EUI_NEO_OK;
    *outEvent = engine->eventQueue.front();
    engine->eventQueue.pop();
    engine->lastEventHandlerId   = outEvent->handler_id;
    engine->lastEventTextInput   = outEvent->text_input;
    return EUI_NEO_OK;
}

const char* eui_neo_last_event_handler_id(const eui_neo_engine* engine) {
    if (engine == nullptr) return "";
    return engine->lastEventHandlerId.c_str();
}

const char* eui_neo_last_event_text_input(const eui_neo_engine* engine) {
    if (engine == nullptr) return "";
    return engine->lastEventTextInput.c_str();
}

eui_neo_result eui_neo_set_window_title(eui_neo_engine* engine, const char* titleUtf8) {
    eui_neo_result check = requireEngine(engine);
    if (check != EUI_NEO_OK) return check;
    if (!engine->initialized || engine->window == nullptr) return EUI_NEO_INVALID_STATE;
    if (titleUtf8 == nullptr) return EUI_NEO_INVALID_ARGUMENT;
    engine->title = titleUtf8;
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_SetWindowTitle(static_cast<SDL_Window*>(engine->window), titleUtf8);
#else
    glfwSetWindowTitle(static_cast<GLFWwindow*>(engine->window), titleUtf8);
#endif
    return EUI_NEO_OK;
}

eui_neo_result eui_neo_set_window_size(eui_neo_engine* engine, int32_t width, int32_t height) {
    eui_neo_result check = requireEngine(engine);
    if (check != EUI_NEO_OK) return check;
    if (!engine->initialized || engine->window == nullptr) return EUI_NEO_INVALID_STATE;
    if (width <= 0 || height <= 0) return EUI_NEO_INVALID_ARGUMENT;
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_SetWindowSize(static_cast<SDL_Window*>(engine->window), width, height);
#else
    glfwSetWindowSize(static_cast<GLFWwindow*>(engine->window), width, height);
#endif
    engine->updateRequested.store(true, std::memory_order_release);
    return EUI_NEO_OK;
}

eui_neo_result eui_neo_begin_window_drag(eui_neo_engine* engine) {
    eui_neo_result check = requireEngine(engine);
    if (check != EUI_NEO_OK) return check;
    if (!engine->initialized) { setError(engine, "Engine is not initialized."); return EUI_NEO_INVALID_STATE; }
    core::window::beginWindowDrag(engine->window);
    return EUI_NEO_OK;
}

eui_neo_result eui_neo_get_cursor_position(eui_neo_engine* engine, double* out_x, double* out_y) {
    eui_neo_result check = requireEngine(engine);
    if (check != EUI_NEO_OK) return check;
    if (!out_x || !out_y) { setError(engine, "Null output pointer."); return EUI_NEO_INVALID_ARGUMENT; }
    if (!engine->initialized) { setError(engine, "Engine is not initialized."); return EUI_NEO_INVALID_STATE; }
    core::window::getCursorPosition(engine->window, *out_x, *out_y);
    return EUI_NEO_OK;
}

eui_neo_result eui_neo_center_window(eui_neo_engine* engine, int32_t width, int32_t height) {
    eui_neo_result check = requireEngine(engine);
    if (check != EUI_NEO_OK) return check;
    if (!engine->initialized) { setError(engine, "Engine is not initialized."); return EUI_NEO_INVALID_STATE; }
    if (width <= 0 || height <= 0) { setError(engine, "Invalid dimensions."); return EUI_NEO_INVALID_ARGUMENT; }
    core::window::centerWindow(engine->window, width, height);
    return EUI_NEO_OK;
}

eui_neo_result eui_neo_set_clipboard_text(eui_neo_engine* engine, const char* text_utf8) {
    eui_neo_result check = requireEngine(engine);
    if (check != EUI_NEO_OK) return check;
    if (!text_utf8) { setError(engine, "Null text pointer."); return EUI_NEO_INVALID_ARGUMENT; }
    if (!engine->initialized) { setError(engine, "Engine is not initialized."); return EUI_NEO_INVALID_STATE; }
    core::window::setClipboardText(text_utf8);
    return EUI_NEO_OK;
}

const char* eui_neo_get_clipboard_text(eui_neo_engine* engine) {
    if (engine == nullptr) { g_clipboard_buffer.clear(); return ""; }
    if (!engine->initialized) { g_clipboard_buffer.clear(); return ""; }
    g_clipboard_buffer = core::window::clipboardText(engine->window);
    return g_clipboard_buffer.c_str();
}

} // extern "C"


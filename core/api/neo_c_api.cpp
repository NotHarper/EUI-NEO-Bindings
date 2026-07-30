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
    if (value.type() != eui::json::Type::Array || value.size() < 3) {
        return fallback;
    }
    core::Color result = fallback;
    double component = 0.0;
    if (value.at(0).number(component)) result.r = static_cast<float>(component);
    if (value.at(1).number(component)) result.g = static_cast<float>(component);
    if (value.at(2).number(component)) result.b = static_cast<float>(component);
    if (value.size() > 3 && value.at(3).number(component)) result.a = static_cast<float>(component);
    return result;
}

core::Align alignValue(const std::string& value) {
    if (value == "center") return core::Align::CENTER;
    if (value == "end") return core::Align::END;
    return core::Align::START;
}

void applyCommon(core::dsl::Element& element, const eui::json::Value& node) {
    const eui::json::Value width = node.get("width");
    const eui::json::Value height = node.get("height");
    double number = 0.0;
    std::string mode;
    if (width.number(number)) element.width = core::SizeValue::fixed(static_cast<float>(number));
    else if (width.string(mode) && mode == "fill") element.width = core::SizeValue::fill();
    else if (mode == "wrap") element.width = core::SizeValue::wrapContent();
    mode.clear();
    if (height.number(number)) element.height = core::SizeValue::fixed(static_cast<float>(number));
    else if (height.string(mode) && mode == "fill") element.height = core::SizeValue::fill();
    else if (mode == "wrap") element.height = core::SizeValue::wrapContent();

    element.spacing = std::max(0.0f, numberValue(node, "gap", element.spacing));
    element.lineSpacing = std::max(0.0f, numberValue(node, "lineGap", element.lineSpacing));
    element.radius = std::max(0.0f, numberValue(node, "radius", element.radius));
    element.opacity = std::clamp(numberValue(node, "opacity", element.opacity), 0.0f, 1.0f);
    element.flexGrow = std::max(0.0f, numberValue(node, "flexGrow", element.flexGrow));
    element.flexShrink = std::max(0.0f, numberValue(node, "flexShrink", element.flexShrink));
    element.clip = boolValue(node, "clip", element.clip);
    element.color = colorValue(node.get("color"), element.color);
    element.padding = core::EdgeInsets::all(std::max(0.0f, numberValue(node, "padding", 0.0f)));
    element.margin = core::EdgeInsets::all(std::max(0.0f, numberValue(node, "margin", 0.0f)));
    element.mainAlign = alignValue(stringValue(node, "justify"));
    element.crossAlign = alignValue(stringValue(node, "align"));
}

void composeNode(core::dsl::Ui& ui, const eui::json::Value& node, const std::string& fallbackId);

template <typename Builder>
void configureNodeBuilder(Builder& builder, core::dsl::Ui& ui, const eui::json::Value& node,
                          const std::string& type, const std::string& id) {
    core::dsl::Element* element = builder.element();
    applyCommon(*element, node);
    if (type == "text") {
        element->text = stringValue(node, "text");
        element->fontSize = std::max(1.0f, numberValue(node, "fontSize", 16.0f));
        element->fontWeight = static_cast<int>(numberValue(node, "fontWeight", 400.0f));
        element->textColor = colorValue(node.get("textColor"), element->textColor);
        element->wrap = boolValue(node, "wrap", element->wrap);
    } else if (type == "image") {
        element->imageSource = stringValue(node, "source");
    }

    const eui::json::Value children = node.get("children");
    if (children.type() == eui::json::Type::Array && children.size() > 0) {
        builder.content([&] {
            for (std::size_t index = 0; index < children.size(); ++index) {
                composeNode(ui, children.at(index), id + "." + std::to_string(index));
            }
        });
    }
}

void composeNode(core::dsl::Ui& ui, const eui::json::Value& node, const std::string& fallbackId) {
    if (node.type() != eui::json::Type::Object) return;
    const std::string type = stringValue(node, "type", "column");
    const std::string id = stringValue(node, "id", fallbackId);
    if (type == "row") {
        auto builder = ui.row(id); configureNodeBuilder(builder, ui, node, type, id);
    } else if (type == "column") {
        auto builder = ui.column(id); configureNodeBuilder(builder, ui, node, type, id);
    } else if (type == "stack") {
        auto builder = ui.stack(id); configureNodeBuilder(builder, ui, node, type, id);
    } else if (type == "flow") {
        auto builder = ui.flow(id); configureNodeBuilder(builder, ui, node, type, id);
    } else if (type == "rect") {
        auto builder = ui.rect(id); configureNodeBuilder(builder, ui, node, type, id);
    } else if (type == "text") {
        auto builder = ui.text(id); configureNodeBuilder(builder, ui, node, type, id);
    } else if (type == "image") {
        auto builder = ui.image(id); configureNodeBuilder(builder, ui, node, type, id);
    } else {
        auto builder = ui.column(id); configureNodeBuilder(builder, ui, node, "column", id);
    }
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
    bool platformInitialized = false;
    bool initialized = false;
    bool running = false;
    bool composed = false;
    float logicalWidth = 0.0f;
    float logicalHeight = 0.0f;
    double lastFrameTime = 0.0;
    uint64_t frameNumber = 0;
};

namespace {

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
        : R"({"type":"column","id":"root","width":"fill","height":"fill","padding":32,"gap":12,"children":[{"type":"text","id":"title","text":"EUI-NEO for Java","fontSize":28},{"type":"text","id":"message","text":"Set NeoConfig.uiJson to replace this document.","fontSize":16}]})";
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

void composeUi(eui_neo_engine* engine, float logicalWidth, float logicalHeight) {
    engine->runtime.compose(engine->pageId, logicalWidth, logicalHeight,
        [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
            composeNode(ui, engine->ui.document.root(), "root");
        });
    engine->composed = true;
    engine->logicalWidth = logicalWidth;
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
    int windowWidth = 0, windowHeight = 0, width = 0, height = 0;
    SDL_GetWindowSize(static_cast<SDL_Window*>(engine->window), &windowWidth, &windowHeight);
    framebufferSize(engine, width, height);
    return windowWidth > 0 && windowHeight > 0
        ? sanitizeScale((static_cast<float>(width) / windowWidth + static_cast<float>(height) / windowHeight) * 0.5f)
        : 1.0f;
}
#else
void framebufferSize(eui_neo_engine* engine, int& width, int& height) {
    glfwGetFramebufferSize(static_cast<GLFWwindow*>(engine->window), &width, &height);
}

float pointerScale(eui_neo_engine* engine) {
    int windowWidth = 0, windowHeight = 0, width = 0, height = 0;
    GLFWwindow* window = static_cast<GLFWwindow*>(engine->window);
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glfwGetFramebufferSize(window, &width, &height);
    return windowWidth > 0 && windowHeight > 0
        ? sanitizeScale((static_cast<float>(width) / windowWidth + static_cast<float>(height) / windowHeight) * 0.5f)
        : 1.0f;
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
#if defined(EUI_WINDOW_BACKEND_SDL2)
    if (engine->platformInitialized) SDL_Quit();
#else
    if (engine->platformInitialized) glfwTerminate();
#endif
    engine->platformInitialized = false;
    engine->initialized = false;
    engine->running = false;
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
    config->size = sizeof(*config);
    config->version = EUI_NEO_C_API_VERSION;
    config->title_utf8 = "EUI-NEO Java";
    config->page_id_utf8 = "java";
    config->width = 960;
    config->height = 640;
    config->frames_per_second = 60.0;
    config->clear_color_r = 0.16f;
    config->clear_color_g = 0.18f;
    config->clear_color_b = 0.20f;
    config->clear_color_a = 1.0f;
    config->resizable = 1;
}

void eui_neo_frame_info_init(eui_neo_frame_info* frameInfo) {
    if (frameInfo == nullptr) return;
    std::memset(frameInfo, 0, sizeof(*frameInfo));
    frameInfo->size = sizeof(*frameInfo);
    frameInfo->version = EUI_NEO_C_API_VERSION;
}

eui_neo_engine* eui_neo_create(const eui_neo_config* config) {
    eui_neo_config resolved;
    eui_neo_config_init(&resolved);
    if (config != nullptr) {
        if (config->size < sizeof(eui_neo_config) || config->version != EUI_NEO_C_API_VERSION) return nullptr;
        resolved = *config;
    }
    std::unique_ptr<eui_neo_engine> engine(new (std::nothrow) eui_neo_engine());
    if (!engine) return nullptr;
    engine->config = resolved;
    engine->title = resolved.title_utf8 != nullptr ? resolved.title_utf8 : "EUI-NEO Java";
    engine->pageId = resolved.page_id_utf8 != nullptr ? resolved.page_id_utf8 : "java";
    engine->ownerThread = std::this_thread::get_id();
    if (!parseUi(engine.get(), resolved.ui_json_utf8)) return nullptr;
    return engine.release();
}

eui_neo_result eui_neo_initialize(eui_neo_engine* engine) {
    eui_neo_result check = requireEngine(engine);
    if (check != EUI_NEO_OK) return check;
    if (engine->initialized) {
        setError(engine, "The engine is already initialized.");
        return EUI_NEO_INVALID_STATE;
    }
    eui_neo_engine* expected = nullptr;
    if (!activeEngine.compare_exchange_strong(expected, engine, std::memory_order_acq_rel)) {
        setError(engine, "Only one EUI-NEO engine may be active in a process.");
        return EUI_NEO_INVALID_STATE;
    }

    core::platform::repairCurrentWorkingDirectory();
#if defined(EUI_WINDOW_BACKEND_SDL2)
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
    request.width = std::max(160, engine->config.width);
    request.height = std::max(120, engine->config.height);
    request.title = engine->title.c_str();
    request.resizable = engine->config.resizable != 0;
    request.renderApi = core::render::windowRenderApi();
    engine->window = core::window::createWindow(request);
    if (engine->window == nullptr) {
        setError(engine, "Native window creation failed.");
        shutdownInternal(engine);
        return EUI_NEO_PLATFORM_ERROR;
    }

#if !defined(EUI_WINDOW_BACKEND_SDL2)
    core::installInputCallbacks(engine->window);
    glfwSetWindowUserPointer(static_cast<GLFWwindow*>(engine->window), engine);
    glfwSetFramebufferSizeCallback(static_cast<GLFWwindow*>(engine->window), [](GLFWwindow* window, int, int) {
        auto* current = static_cast<eui_neo_engine*>(glfwGetWindowUserPointer(window));
        if (current != nullptr) current->updateRequested.store(true, std::memory_order_release);
    });
    glfwSetWindowContentScaleCallback(static_cast<GLFWwindow*>(engine->window), [](GLFWwindow* window, float, float) {
        auto* current = static_cast<eui_neo_engine*>(glfwGetWindowUserPointer(window));
        if (current != nullptr) current->updateRequested.store(true, std::memory_order_release);
    });
#endif

    engine->renderBackend = core::render::createRenderBackend(engine->window);
    if (!engine->renderBackend || !engine->renderBackend->initialize()) {
        setError(engine, "Render backend initialization failed.");
        shutdownInternal(engine);
        return EUI_NEO_PLATFORM_ERROR;
    }
    if (!engine->runtime.initialize(engine->window)) {
        setError(engine, "EUI runtime initialization failed.");
        shutdownInternal(engine);
        return EUI_NEO_PLATFORM_ERROR;
    }
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_StartTextInput();
#endif
    engine->lastFrameTime = core::window::timeSeconds();
    engine->initialized = true;
    engine->running = true;
    engine->lastError.clear();
    return EUI_NEO_OK;
}

eui_neo_result eui_neo_pump_events(eui_neo_engine* engine, int32_t waitTimeoutMillis) {
    eui_neo_result check = requireEngine(engine);
    if (check != EUI_NEO_OK) return check;
    if (!engine->initialized) return EUI_NEO_INVALID_STATE;
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_Event event{};
    if (waitTimeoutMillis > 0 && SDL_WaitEventTimeout(&event, waitTimeoutMillis)) SDL_PushEvent(&event);
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)) {
            engine->running = false;
        } else if (event.type == SDL_TEXTINPUT) {
            core::queueTextInput(engine->window, event.text.text);
            engine->updateRequested.store(true, std::memory_order_release);
        } else if (event.type == SDL_TEXTEDITING) {
            core::queueTextEditing(engine->window, event.edit.text);
            engine->updateRequested.store(true, std::memory_order_release);
        } else if (event.type == SDL_MOUSEWHEEL) {
            core::queueScrollInput(engine->window, event.wheel.preciseX, event.wheel.preciseY);
            engine->updateRequested.store(true, std::memory_order_release);
        } else if (event.type == SDL_WINDOWEVENT) {
            engine->updateRequested.store(true, std::memory_order_release);
        }
    }
#else
    if (waitTimeoutMillis > 0) glfwWaitEventsTimeout(static_cast<double>(waitTimeoutMillis) / 1000.0);
    else glfwPollEvents();
    engine->running = glfwWindowShouldClose(static_cast<GLFWwindow*>(engine->window)) == GLFW_FALSE;
#endif
    return EUI_NEO_OK;
}

eui_neo_result eui_neo_frame(eui_neo_engine* engine, eui_neo_frame_info* frameInfo) {
    eui_neo_result check = requireEngine(engine);
    if (check != EUI_NEO_OK) return check;
    if (!engine->initialized) return EUI_NEO_INVALID_STATE;
    if (frameInfo != nullptr && (frameInfo->size < sizeof(*frameInfo) || frameInfo->version != EUI_NEO_C_API_VERSION)) {
        setError(engine, "Invalid frame info structure.");
        return EUI_NEO_INVALID_ARGUMENT;
    }

    int width = 0, height = 0;
    framebufferSize(engine, width, height);
    const float dpi = dpiScale(engine);
    const float pointer = pointerScale(engine);
    bool rendered = false;
    if (engine->running && width > 0 && height > 0) {
        const float logicalWidth = static_cast<float>(width) / dpi;
        const float logicalHeight = static_cast<float>(height) / dpi;
        const bool requested = engine->updateRequested.exchange(false, std::memory_order_acq_rel);
        if (!engine->composed || requested || logicalWidth != engine->logicalWidth || logicalHeight != engine->logicalHeight) {
            composeUi(engine, logicalWidth, logicalHeight);
            engine->runtime.requestFullPaint();
        }
        const double now = core::window::timeSeconds();
        const float delta = static_cast<float>(std::max(0.0, now - engine->lastFrameTime));
        engine->lastFrameTime = now;
        const bool changed = engine->runtime.update(engine->window, delta, pointer, dpi, true);
        if (changed || requested || engine->runtime.paintRequested() || engine->runtime.isAnimating()) {
            engine->renderBackend->makeCurrent();
            engine->renderBackend->beginFrame({engine->window, core::window::nativeWindowInfo(engine->window), width, height, dpi});
            core::render::ScopedRenderBackend scoped(*engine->renderBackend);
            engine->runtime.render(width, height, dpi, {
                engine->config.clear_color_r,
                engine->config.clear_color_g,
                engine->config.clear_color_b,
                engine->config.clear_color_a
            });
            engine->renderBackend->present();
            core::render::publishRenderFrameStats();
            rendered = true;
        }
        ++engine->frameNumber;
    }

    if (frameInfo != nullptr) {
        frameInfo->frame_number = engine->frameNumber;
        frameInfo->framebuffer_width = width;
        frameInfo->framebuffer_height = height;
        frameInfo->dpi_scale = dpi;
        frameInfo->rendered = rendered ? 1 : 0;
        frameInfo->running = engine->running ? 1 : 0;
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
    core::window::postEmptyEvent();
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
    if (engine->ownerThread == std::this_thread::get_id()) shutdownInternal(engine);
    delete engine;
}

const char* eui_neo_last_error(const eui_neo_engine* engine) {
    return engine != nullptr ? engine->lastError.c_str() : "Invalid EUI-NEO engine handle.";
}

} // extern "C"

#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(EUI_NEO_C_API_SHARED)
#  if defined(EUI_NEO_C_API_BUILD)
#    define EUI_NEO_C_API __declspec(dllexport)
#  else
#    define EUI_NEO_C_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define EUI_NEO_C_API __attribute__((visibility("default")))
#else
#  define EUI_NEO_C_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define EUI_NEO_C_API_VERSION 1u

typedef struct eui_neo_engine eui_neo_engine;

typedef enum eui_neo_result {
    EUI_NEO_OK = 0,
    EUI_NEO_INVALID_ARGUMENT = 1,
    EUI_NEO_INVALID_STATE = 2,
    EUI_NEO_WRONG_THREAD = 3,
    EUI_NEO_PLATFORM_ERROR = 4,
    EUI_NEO_PARSE_ERROR = 5,
    EUI_NEO_INTERNAL_ERROR = 6
} eui_neo_result;

typedef struct eui_neo_config {
    uint32_t size;
    uint32_t version;
    const char* title_utf8;
    const char* page_id_utf8;
    const char* ui_json_utf8;
    int32_t width;
    int32_t height;
    double frames_per_second;
    float clear_color_r;
    float clear_color_g;
    float clear_color_b;
    float clear_color_a;
    uint8_t resizable;
    uint8_t reserved[7];
} eui_neo_config;

typedef struct eui_neo_frame_info {
    uint32_t size;
    uint32_t version;
    uint64_t frame_number;
    int32_t framebuffer_width;
    int32_t framebuffer_height;
    float dpi_scale;
    uint8_t rendered;
    uint8_t running;
    uint8_t reserved[6];
} eui_neo_frame_info;

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
    uint32_t size;
    uint32_t type;
    char     handler_id[128];
    float    x;
    float    y;
    float    delta_x;
    float    delta_y;
    char     text_input[64];
    uint8_t  reserved[8];
} eui_neo_event;

EUI_NEO_C_API uint32_t eui_neo_api_version(void);
EUI_NEO_C_API const char* eui_neo_version_string(void);
EUI_NEO_C_API void eui_neo_config_init(eui_neo_config* config);
EUI_NEO_C_API void eui_neo_frame_info_init(eui_neo_frame_info* frame_info);
EUI_NEO_C_API void eui_neo_event_init(eui_neo_event* event);
EUI_NEO_C_API eui_neo_engine* eui_neo_create(const eui_neo_config* config);
EUI_NEO_C_API eui_neo_result eui_neo_initialize(eui_neo_engine* engine);
EUI_NEO_C_API eui_neo_result eui_neo_pump_events(eui_neo_engine* engine, int32_t wait_timeout_millis);
EUI_NEO_C_API eui_neo_result eui_neo_frame(eui_neo_engine* engine, eui_neo_frame_info* frame_info);
EUI_NEO_C_API eui_neo_result eui_neo_set_ui_json(eui_neo_engine* engine, const char* ui_json_utf8);
EUI_NEO_C_API eui_neo_result eui_neo_request_update(eui_neo_engine* engine);
EUI_NEO_C_API int32_t eui_neo_is_running(const eui_neo_engine* engine);
EUI_NEO_C_API eui_neo_result eui_neo_shutdown(eui_neo_engine* engine);
EUI_NEO_C_API void eui_neo_destroy(eui_neo_engine* engine);
EUI_NEO_C_API const char* eui_neo_last_error(const eui_neo_engine* engine);
EUI_NEO_C_API eui_neo_result eui_neo_poll_event(eui_neo_engine* engine, eui_neo_event* out_event);
EUI_NEO_C_API eui_neo_result eui_neo_set_window_title(eui_neo_engine* engine, const char* title_utf8);
EUI_NEO_C_API eui_neo_result eui_neo_set_window_size(eui_neo_engine* engine, int32_t width, int32_t height);
EUI_NEO_C_API const char*    eui_neo_last_event_handler_id(const eui_neo_engine* engine);
EUI_NEO_C_API const char*    eui_neo_last_event_text_input(const eui_neo_engine* engine);
EUI_NEO_C_API uint32_t       eui_neo_api_version(void);

#ifdef __cplusplus
}
#endif

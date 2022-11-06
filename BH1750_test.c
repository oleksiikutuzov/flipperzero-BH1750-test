#include <stdio.h>
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <notification/notification_messages.h>

#include <BH1750.h>

#define APP_NAME "BH1750 Test"

typedef enum {
    EventTypeTick,
    EventTypeInput,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} HelloWorldEvent;

typedef struct {
    ViewPort* view_port;
} LightMeter;

static void draw_callback(Canvas* canvas, void* ctx) {
    // UNUSED(ctx);
    LightMeter* app = acquire_mutex((ValueMutex*)ctx, 25);

    char str[30];
    float lux;

    if(bh1750_trigger_manual_conversion() == BH1750_OK) {
        FURI_LOG_D(APP_NAME, "Trigger manual conversion successful");
    } else {
        FURI_LOG_D(APP_NAME, "Could not trigger manual conversion");
    }

    furi_delay_ms(120);

    if(bh1750_read_light(&lux) == BH1750_OK) {
        FURI_LOG_D(APP_NAME, "Read light successful, lux = %f", (double)lux);
    } else {
        FURI_LOG_D(APP_NAME, "Could not read light");
    }

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    snprintf(str, sizeof(str), "Received value: %f lux", (double)lux);
    canvas_draw_str(canvas, 0, 10, str);

    elements_button_center(canvas, "Send");
    release_mutex((ValueMutex*)ctx, app);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;

    HelloWorldEvent event = {.type = EventTypeInput, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void timer_callback(FuriMessageQueue* event_queue) {
    furi_assert(event_queue);

    HelloWorldEvent event = {.type = EventTypeTick};
    furi_message_queue_put(event_queue, &event, 0);
}

int32_t bh1750_test_app(void* p) {
    UNUSED(p);

    HelloWorldEvent event;

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(HelloWorldEvent));

    // Alloc lightmeter
    LightMeter* app = malloc(sizeof(LightMeter));
    ValueMutex lightmeter_mutex;
    if(!init_mutex(&lightmeter_mutex, app, sizeof(LightMeter))) {
        FURI_LOG_E(APP_NAME, "cannot create mutex\r\n");
        free(app);
        return -1;
    }

    FURI_LOG_D(APP_NAME, "Initializing sensor\n");

    // Initialize sensor and return result
    if(bh1750_reset() == BH1750_OK) {
        FURI_LOG_D(APP_NAME, "Init successful\n");
    } else {
        FURI_LOG_D(APP_NAME, "Could not init sensor\n");
    }

    // BH1750_SetMTreg(200);

    FURI_LOG_D(APP_NAME, "Setting power state to on\n");

    // Initialize sensor and return result
    if(bh1750_set_power_state(true) == BH1750_OK) {
        FURI_LOG_D(APP_NAME, "Power state set successful\n");
    } else {
        FURI_LOG_D(APP_NAME, "Could not set power state\n");
    }

    FURI_LOG_D(APP_NAME, "Setting mode\n");

    // Initialize sensor and return result
    if(bh1750_set_mode(ONETIME_HIGH_RES_MODE) == BH1750_OK) {
        FURI_LOG_D(APP_NAME, "Mode set successful\n");
    } else {
        FURI_LOG_D(APP_NAME, "Could not set mode\n");
    }

    app->view_port = view_port_alloc();

    view_port_draw_callback_set(app->view_port, draw_callback, &lightmeter_mutex);
    view_port_input_callback_set(app->view_port, input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, app->view_port, GuiLayerFullscreen);

    FuriTimer* timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, event_queue);
    furi_timer_start(timer, furi_ms_to_ticks(500));

    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);

    while(furi_message_queue_get(event_queue, &event, FuriWaitForever) == FuriStatusOk) {
        furi_check(furi_message_queue_get(event_queue, &event, FuriWaitForever) == FuriStatusOk);

        if(event.type == EventTypeInput) {
            if(event.input.key == InputKeyBack) {
                break;
            }

        } else if(event.type == EventTypeTick) {
            notification_message(notifications, &sequence_blink_blue_100);
            view_port_update(app->view_port);
        }
    }

    furi_message_queue_free(event_queue);

    gui_remove_view_port(gui, app->view_port);
    view_port_free(app->view_port);
    bh1750_set_power_state(false);
    furi_record_close(RECORD_GUI);
    furi_timer_free(timer);
    furi_record_close(RECORD_NOTIFICATION);

    return 0;
}

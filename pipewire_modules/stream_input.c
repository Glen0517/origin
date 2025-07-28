#include <pipewire/pipewire.h>
#include <pipewire/extensions/metadata.h>
#include <spa/param/audio/format-utils.h>
#include <alsa/asoundlib.h>
#include "stream_common.h"
#include "audio_buffer.h"

struct stream_input {
    struct pw_main_loop *loop;
    struct pw_context *context;
    struct pw_core *core;
    struct pw_stream *stream;
    struct spa_hook stream_listener;
    struct audio_buffer *buffer;
    bool started;
};

static struct stream_input *input = NULL;

static void on_process(void *data)
{
    struct stream_input *inp = data;
    struct pw_buffer *b;
    struct spa_buffer *buf;

    if (!inp->started || !inp->buffer) return;

    if ((b = pw_stream_dequeue_buffer(inp->stream)) == NULL) {
        pw_log_warn("out of buffers");
        return;
    }

    buf = b->buffer;
    if (buf->datas[0].data == NULL) return;

    // Copy audio data to shared buffer
    audio_buffer_write(inp->buffer, buf->datas[0].data, buf->datas[0].chunk->size);

    pw_stream_queue_buffer(inp->stream, b);
}

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .process = on_process,
};

static void create_stream(struct stream_input *inp)
{
    struct pw_properties *props;
    struct spa_audio_info_raw info = {
        .format = SPA_AUDIO_FORMAT_S16_LE,
        .channels = 2,
        .rate = 48000,
    };
    const struct spa_pod *params[1];
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Communication",
        PW_KEY_NODE_NAME, "audio-framework-input",
        PW_KEY_AUDIO_CHANNELS, "2",
        PW_KEY_AUDIO_RATE, "48000",
        NULL);

    inp->stream = pw_stream_new(inp->core, "audio-input", props);

    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,
            &info);

    pw_stream_add_listener(inp->stream, &inp->stream_listener,
            &stream_events, inp);

    pw_stream_connect(inp->stream,
            PW_DIRECTION_INPUT,
            PW_ID_ANY,
            PW_STREAM_FLAG_AUTOCONNECT |
            PW_STREAM_FLAG_MAP_BUFFERS,
            params, 1);
}

int stream_input_init(struct audio_buffer *buffer)
{
    if (input) return -EBUSY;

    input = calloc(1, sizeof(struct stream_input));
    if (!input) return -ENOMEM;

    input->buffer = buffer;

    pw_init(NULL, NULL);

    input->loop = pw_main_loop_new(NULL);
    input->context = pw_context_new(pw_main_loop_get_loop(input->loop), NULL, 0);
    input->core = pw_context_connect(input->context, NULL, 0);

    create_stream(input);

    input->started = true;
    pw_log_info("Audio stream input initialized");

    return 0;
}

void stream_input_start(void)
{
    if (!input || !input->loop) return;

    pw_main_loop_run(input->loop);
}

void stream_input_stop(void)
{
    if (!input) return;

    input->started = false;

    if (input->stream) {
        pw_stream_disconnect(input->stream);
        pw_stream_destroy(input->stream);
    }

    if (input->core) {
        pw_core_disconnect(input->core);
    }

    if (input->context) {
        pw_context_destroy(input->context);
    }

    if (input->loop) {
        pw_main_loop_quit(input->loop);
        pw_main_loop_destroy(input->loop);
    }

    free(input);
    input = NULL;

    pw_deinit();
    pw_log_info("Audio stream input stopped");
}

bool stream_input_is_running(void)
{
    return input && input->started;
}
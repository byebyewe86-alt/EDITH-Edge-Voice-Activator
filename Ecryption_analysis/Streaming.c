/*
 * kws_to_streamer_integration.c
 * ---------------------------------------------------------
 * Wires your existing TFLite-Micro KWS model (the .h array you
 * already generated) to the AES-256-GCM audio streamer.
 *
 * Architecture:
 *   [mic DMA] -> [ring buffer] -> [KWS task]  -- wake event --> [queue]
 *                       |                                          |
 *                       +---------> [streamer task] <---------------+
 *
 * The ring buffer is continuously filled by the mic (I2S DMA).
 * The KWS task reads sliding windows from it for inference, same
 * as it does today. The streamer task normally sleeps, waiting on
 * a queue. When KWS detects the wake word, it pushes an event and
 * the streamer wakes up and starts pulling chunks from the SAME
 * ring buffer (including a bit of pre-roll audio from just before
 * the wake word fired) and streaming them out encrypted.
 *
 * Target: ESP32-S3 (dual-core Xtensa LX7). Pin KWS inference and
 * networking to separate cores so a slow Wi-Fi send never delays
 * the next inference window, and vice versa.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "esp_log.h"

static const char *TAG = "KWS_INTEGRATION";

#define RING_BUF_BYTES      (32 * 1024)   // ~1s of 16kHz 16-bit mono audio, tune as needed
#define PREROLL_BYTES        3200          // ~100ms pre-roll included when we start streaming
#define STREAM_CHUNK_BYTES    640          // matches CHUNK_BYTES in the streamer (20ms @16kHz)
#define SILENCE_TIMEOUT_MS   1200          // stop streaming after this much silence post-wake

typedef struct {
    uint32_t ring_read_offset_at_wake;  // where in the ring buffer the wake happened
} wake_event_t;

static RingbufHandle_t audio_ring;      // populated continuously by your existing mic/I2S code
static QueueHandle_t   wake_queue;

// ---- Called from wherever your KWS inference loop already lives ----
// Your existing code presumably does something like:
//
//   float score = run_inference(tflite_model, window);
//   if (score > THRESHOLD && debounce_ok()) {
//       on_wake_detected();
//   }
//
// Add this call at the point where "wake word detected" already fires:
void on_wake_detected(void)
{
    wake_event_t evt = {
        .ring_read_offset_at_wake = 0  // fill with actual ring buffer position if you track it
    };
    // Non-blocking send: if the streamer is already mid-utterance, don't queue duplicates
    if (xQueueSend(wake_queue, &evt, 0) != pdTRUE) {
        ESP_LOGW(TAG, "wake event dropped, streamer already active");
    }
}

// ---- Streamer task: now event-driven instead of running forever ----
void audio_streamer_task(void *pvParameters)
{
    wake_event_t evt;
    uint8_t chunk[STREAM_CHUNK_BYTES];
    size_t item_size;

    while (1) {
        // Sleep here until KWS fires a wake event — this is the only change
        // in control flow vs. the always-on version from before.
        if (xQueueReceive(wake_queue, &evt, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        ESP_LOGI(TAG, "wake event received, starting stream");

        TickType_t last_audio_tick = xTaskGetTickCount();
        uint16_t seq = 0;
        bool preroll_sent = false;

        while (1) {
            uint8_t *data = (uint8_t *)xRingbufferReceiveUpTo(
                audio_ring, &item_size, pdMS_TO_TICKS(50), STREAM_CHUNK_BYTES);

            if (data == NULL) {
                // No new audio — check if we've been silent long enough to stop
                if ((xTaskGetTickCount() - last_audio_tick) * portTICK_PERIOD_MS
                        > SILENCE_TIMEOUT_MS) {
                    ESP_LOGI(TAG, "silence timeout, ending utterance stream");
                    break;
                }
                continue;
            }

            last_audio_tick = xTaskGetTickCount();

            if (!preroll_sent) {
                // OPTIONAL: pull PREROLL_BYTES of audio from just before the
                // wake point here (e.g. by tracking ring buffer offsets)
                // and send it first, so the ASR stage sees the word that
                // triggered the wake, not just what came after it.
                preroll_sent = true;
            }

            // ---- Reuse your existing encrypt_chunk()/build_nonce() from
            // ---- esp32_c6_audio_streamer.c unchanged — AES-GCM code and
            // ---- packet framing don't depend on chip family.
            // encrypt_chunk(data, item_size, nonce, ciphertext, tag);
            // sendto(sock, packet, ..., 0, (struct sockaddr *)&dest, sizeof(dest));

            vRingbufferReturnItem(audio_ring, data);
            seq++;
        }
    }
}

// ---- Setup: call once at boot ----
void kws_streamer_integration_init(void)
{
    audio_ring = xRingbufferCreate(RING_BUF_BYTES, RINGBUF_TYPE_BYTEBUF);
    wake_queue = xQueueCreate(1, sizeof(wake_event_t)); // depth 1: only care if a wake is pending

    // Pin each task to its own core on the S3's dual-core Xtensa LX7 so
    // inference and networking never contend for the same core.
    xTaskCreatePinnedToCore(kws_task,            "kws_task",      8192, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(audio_streamer_task, "streamer_task", 8192, NULL, 5, NULL, 1);
}
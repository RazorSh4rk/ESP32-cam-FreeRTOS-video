#include <AViShaESPCam.h>

#define FRAMERATE 20

AViShaESPCam espcam;
QueueHandle_t queue;

int frameDelay = 1000 / FRAMERATE;

void producer(void* param) {
  while (true) {
    FrameBuffer* frame = espcam.capture();
    if (frame) {
      if (xQueueSendToBack(queue, &frame, 0) != pdPASS) {
        espcam.returnFrame(frame);  // queue full, discard
      }
    }
    vTaskDelay(frameDelay / portTICK_PERIOD_MS);
  }
}

void consumer(void* param) {
  FrameBuffer* frame;
  while (true) {
    if (xQueueReceive(queue, &frame, 0) == pdPASS) {
      espcam.saveToSD(frame, "i");
      espcam.returnFrame(frame);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);
  espcam.enableLogging(false);

  if (!espcam.init(AI_THINKER(), VGA)) {
    Serial.println("Camera init failed!");
    ESP.restart();
  }

  if (espcam.initSDCard()) {
    Serial.println("SD card ready");
  }

  const size_t VGA_FRAME_BYTES = 30000; // roughly 26kb per frame
  size_t psramSize  = ESP.getPsramSize();
  size_t queueDepth = (psramSize * 0.9) / VGA_FRAME_BYTES;

  Serial.printf("PSRAM: %u bytes\n", psramSize);
  Serial.printf("Queue depth: %u frames\n", queueDepth);

  queue = xQueueCreate(queueDepth, sizeof(FrameBuffer*));
  xTaskCreatePinnedToCore(producer, "producer", 8192, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(consumer, "consumer", 16384, NULL, 1, NULL, 1);
}

void loop() {}

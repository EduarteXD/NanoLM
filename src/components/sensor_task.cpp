#include "sensor_task.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../core/state_store.h"
#include "led_status_component.h"
#include "sensor_component.h"

void sensorTask(void *param) {
  (void)param;

  vTaskDelay(pdMS_TO_TICKS(SensorComponent::currentIntegrationMs() + 60U));

  for (;;) {
    const SensorData sd = SensorComponent::sampleOnce();
    storeSensorData(sd);
    LedStatusComponent::setSensorState(sd.state);

    vTaskDelay(pdMS_TO_TICKS(SensorComponent::recommendedDelayMs()));
  }
}

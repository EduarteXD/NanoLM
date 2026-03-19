#include "sensor_sampling_task.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../components/led_status_component.h"
#include "../components/sensor_component.h"
#include "../core/state_store.h"

void sensorSamplingTask(void *param)
{
  (void)param;

  vTaskDelay(pdMS_TO_TICKS(SensorComponent::currentIntegrationMs() + 60U));

  for (;;)
  {
    const SensorData sd = SensorComponent::sampleOnce();
    storeSensorData(sd);
    LedStatusComponent::setSensorState(sd.state);

    vTaskDelay(pdMS_TO_TICKS(SensorComponent::recommendedDelayMs()));
  }
}

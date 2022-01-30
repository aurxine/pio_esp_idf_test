#include<freertos/FreeRTOS.h>
#include<freertos/task.h>
#include<freertos/queue.h>
#include<driver/gpio.h>
#include<esp_log.h>
#include<stdio.h>



#define BOARD_LED                               2
#define PUSH_BUTTON                             4
#define OUTPUT_PIN_SEL                          (1ULL << BOARD_LED)
#define INPUT_PIN_SEL                           (1ULL << PUSH_BUTTON)

#define DEFAULT_TICKS                           portMAX_DELAY
#define DEBOUNCE_TIME_IN_TICKS                  100


xTaskHandle push_button_handle;
xTaskHandle led_blink_handle;

static xQueueHandle push_button_event_queue = NULL;
bool new_event = 0;
TickType_t ticks, last_tick = 0;


static void IRAM_ATTR push_button_isr_handler(void* arg);
static void svLEDTask(void* args);



void app_main() 
{
    gpio_config_t io_config = {};
    io_config.mode = GPIO_MODE_OUTPUT;
    io_config.pin_bit_mask = OUTPUT_PIN_SEL;
    gpio_config(&io_config);
    
    io_config.mode = GPIO_MODE_INPUT;
    io_config.pin_bit_mask = INPUT_PIN_SEL;
    io_config.pull_up_en = 1;
    io_config.pull_down_en = 0;
    io_config.intr_type = GPIO_INTR_POSEDGE;
    gpio_config(&io_config);


    // create queue
    push_button_event_queue = xQueueCreate(10, sizeof(TickType_t)); // store 10 TickType_t values

    // install isr service (this means each gpio will have different types of isr)
    gpio_install_isr_service(0); // default flag
    // add new isr
    gpio_isr_handler_add(PUSH_BUTTON, push_button_isr_handler, NULL);

    xTaskCreate(svLEDTask, "Blink LED", 2048, NULL, 1, led_blink_handle);

}


static void IRAM_ATTR push_button_isr_handler(void* arg)
{
    if(!new_event)
    {
        ticks = xTaskGetTickCountFromISR();
        new_event = 1;
        xQueueSendFromISR(push_button_event_queue, &ticks, NULL);
    }

    else
    {
        if(xTaskGetTickCountFromISR() - ticks >= DEBOUNCE_TIME_IN_TICKS)
        {
            // xQueueSendFromISR(push_button_event_queue, &ticks, NULL);
            new_event = 0;
        }
        ticks = xTaskGetTickCountFromISR();
    }
}

static void svLEDTask(void* args)
{
    TickType_t ticks_now;
    uint32_t led_toggle_delay = 500;
    bool led_toggle = 1;
    while (1)
    {
        if(xQueueReceiveFromISR(push_button_event_queue, &ticks_now, pdMS_TO_TICKS(1)))
        {
            led_toggle_delay = pdTICKS_TO_MS(ticks_now) - pdTICKS_TO_MS(last_tick);
            printf("Ticks: %d   Delay:%d\n", ticks_now, led_toggle_delay);
            last_tick = ticks_now;
        }

        gpio_set_level(BOARD_LED, led_toggle);
        led_toggle = !led_toggle;
        if(led_toggle_delay <= 0)
            led_toggle_delay = 500; // because I don't trust my code
        vTaskDelay(pdMS_TO_TICKS(led_toggle_delay));
    }
    
}
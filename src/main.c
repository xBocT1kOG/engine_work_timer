#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h" // Основной заголовочный файл для работы с таймерами

// --- НАСТРОЙКА ПИНОВ ---
#define MOTOR_PIN      4   // Сигнал на транзистор мотора
#define LED_RED_PIN    5   // Красный светодиод (Мотор стоит)
#define LED_GRN_PIN    6   // Зеленый светодиод (Мотор работает)

// --- ПЕРЕМЕННЫЕ ВРЕМЕНИ (задаются в миллисекундах) ---
uint32_t motor_on_duration_ms = 7000;  // 7 секунд работы
uint32_t motor_off_duration_ms = 3000; // 3 секунды паузы (в сумме 10 секунд цикла)

// Глобальные переменные управления
volatile bool motor_is_active = false;
esp_timer_handle_t motor_timer_handle;

// Прототип функции обратного вызова таймера
static void motor_timer_callback(void* arg);

/**
 * Функция управления состояниями железа и логирования
 */
void update_system_state(bool activate_motor) {
    if (activate_motor) {
        // Включаем мотор и зеленый ЛЭД, выключаем красный
        gpio_set_level(MOTOR_PIN, 1);
        gpio_set_level(LED_GRN_PIN, 1);
        gpio_set_level(LED_RED_PIN, 0);
        
        printf("[ЛОГ] [%lld] СТАТУС: Мотор РАБОТАЕТ | ЛЭД: ЗЕЛЕНЫЙ. Ожидание: %lu мс\n", 
               esp_timer_get_time() / 1000, motor_on_duration_ms);
        
        // Перезапускаем этот же таймер один раз на время РАБОТЫ мотора
        // Время передается в микросекундах (мс * 1000)
        esp_timer_start_once(motor_timer_handle, (uint64_t)motor_on_duration_ms * 1000);
    } else {
        // Выключаем мотор и зеленый ЛЭД, включаем красный
        gpio_set_level(MOTOR_PIN, 0);
        gpio_set_level(LED_GRN_PIN, 0);
        gpio_set_level(LED_RED_PIN, 1);
        
        printf("[ЛОГ] [%lld] СТАТУС: Мотор ОТДЫХАЕТ | ЛЭД: КРАСНЫЙ. Ожидание: %lu мс\n", 
               esp_timer_get_time() / 1000, motor_off_duration_ms);
        
        // Перезапускаем таймер один раз на время ПАУЗЫ мотора
        esp_timer_start_once(motor_timer_handle, (uint64_t)motor_off_duration_ms * 1000);
    }
}

/**
 * Обработчик прерывания таймера (вызывается в фоновом потоке esp_timer)
 */
static void motor_timer_callback(void* arg) {
    // Инвертируем состояние флага
    motor_is_active = !motor_is_active;
    
    // Применяем изменения
    update_system_state(motor_is_active);
}

void app_main(void) {
    // 1. Конфигурация пинов GPIO на выход
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MOTOR_PIN) | (1ULL << LED_RED_PIN) | (1ULL << LED_GRN_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // 2. Настройка параметров конфигурации таймера
    const esp_timer_create_args_t timer_args = {
        .callback = &motor_timer_callback, // Ссылка на функцию-обработчик
        .name = "motor_asymmetric_timer"   // Имя таймера для отладки
    };

    // 3. Создаем таймер
    esp_timer_create(&timer_args, &motor_timer_handle);

    printf("==================================================\n");
    printf(" Асинхронная система управления мотором запущена \n");
    printf("==================================================\n");

    // 4. Запускаем первый цикл. Начнем с паузы (мотор выключен).
    motor_is_active = false;
    update_system_state(motor_is_active);
}
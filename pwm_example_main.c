/* pwm example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
//#include <ESP8266WiFi.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "esp8266/gpio_register.h"
#include "esp8266/pin_mux_register.h"

#include "driver/adc.h"
#include "driver/pwm.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_log.h"
#include "mqtt_client.h"


static const char *TAG1 = "adc example";

#define wifi_ssid "<wifi ssid>"
#define wifi_password "<wifi pass>"

    #define PWM_0_OUT_IO_NUM   13
//#define PWM_1_OUT_IO_NUM   13
//#define PWM_2_OUT_IO_NUM   14
//#define PWM_3_OUT_IO_NUM   12

// PWM period 1000us(1Khz), same as depth
int PWM_PERIOD1 = 100;
int PWM_PERIOD2 = 75;
int PWM_PERIOD3 = 30;
int PWM_PERIOD =100 ;
static const char *TAG2 = "pwm_example";

#define EXAMPLE_ESP_WIFI_SSID      "dev_iotB"
#define EXAMPLE_ESP_WIFI_PASS      "kikikiki"
#define EXAMPLE_ESP_MAXIMUM_RETRY  4

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// pwm pin number
const uint32_t pin_num =    PWM_0_OUT_IO_NUM ;//,
   // PWM_1_OUT_IO_NUM,
  //  PWM_2_OUT_IO_NUM,
    //PWM_3_OUT_IO_NUM


// duties table, real_duty = duties[x]/PERIOD
uint32_t duties_percent =  50 ;//, 500, 500, 500,


// phase table, delay = (phase[x]/360)*PERIOD
float phase = 0;//, 0, 90.0, -90.0,
int nilai = 0;
static uint16_t adc_reading;
static void adc_task()
{
    int x;
    uint16_t adc_data[100];

    while (1) {
        // if (ESP_OK == adc_read(&adc_data[0])) {
        //     ESP_LOGI(TAG1, "adc read: %d", adc_data[0]);
        //     adc_reading = adc_data[0];
        // }

        // // ESP_LOGI(TAG1, "adc read fast:\r\n");

        uint32_t adc_sum = 0;
        if (ESP_OK == adc_read_fast(adc_data, 100)) {
            for (x = 0; x < 100; x++) {
                adc_sum = adc_sum + adc_data[x];
                // printf("%d\n", adc_data[x]);
                
            }
            uint32_t adc_avg = adc_sum/100;
            nilai = (adc_avg * 2935) /1023;
            // int nilai = (adc_avg * 2.8796) - 97.785;
            ESP_LOGI(TAG1, "ADC Avg : %u", adc_avg);
            //ESP_LOGI(TAG1,  "Voltage : %i", voltage);
            printf ("voltage : %d\n", nilai);
        }

            vTaskDelay(1000 / portTICK_RATE_MS);
        
    }
}

static const char *TAG = "MQTT_TCP";

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    tcpip_adapter_init();

    s_wifi_event_group = xEventGroupCreate();

    // ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    //esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_ERROR_CHECK(esp_event_handler_register      (WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register       (IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
                },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    vEventGroupDelete(s_wifi_event_group);
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        esp_mqtt_client_subscribe(client, "my_topic", 0);
        esp_mqtt_client_publish(client, "my_topic", "connect publish", 0, 1, 0);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        if (strncmp(event->data, "button1", 7)==0) 
        {
            PWM_PERIOD = PWM_PERIOD1;
        } else if (strncmp(event->data,"button2", 7)==0) {
            PWM_PERIOD = PWM_PERIOD2;
        } else if (strncmp(event->data, "button3", 7)==0) {
            PWM_PERIOD = PWM_PERIOD3;
        }
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("\nTOPIC=%.*s\r\n", event->topic_len, event->topic);
        //printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}
// void wifi_connect() {
//   // Loop until we're connected
//   while (!WiFi.status() == WL_CONNECTED) {
//     WiFi.begin(wifi_ssid, wifi_password);
//     digitalWrite(led, 1);
//     delay(200);
//     digitalWrite(led, 0);
//     delay(3000);
//   }
// }

// void mqtt_connect() {
//   // Loop until we're connected
//   while (!client.connected()) {
//     client.connect("ESP8266_bedroom");
//     digitalWrite(led, 0);
//     delay(200);
//     digitalWrite(led, 1);
//     delay(3000);
//   }
// }

// esp_err_t pwm_task()
// {
//       int16_t count = 0;  
    
//     // while (1) {
//     //     if () {
//     //        
//     //     } else if (count == 30) {
//     //         pwm_start();
//     //         ESP_LOGI(TAG2, "PWM re-start\n");
//     //         count = 0;
            
//     //     }
//         vTaskDelay (1000/ portTICK_RATE_MS);
//         count++;
       
//     }
// }

void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    printf("WIFI was initiated ...........\n");
    //mqtt_app_start();
    
    esp_mqtt_client_config_t mqtt_cfg = {
        // .uri = "wss://mqtt.eclipseprojects.io:443/mqtt",
         .uri = "mqtt://mqtt.eclipseprojects.io",
        //.uri = "ws://mqtt.eclipseprojects.io",
        // .cert_pem = (const char *)mqtt_eclipseprojects_io_pem_start,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
     // 1. init adc
    adc_config_t adc_config;

    // Depend on menuconfig->Component config->PHY->vdd33_const value
    // When measuring system voltage(ADC_READ_VDD_MODE), vdd33_const must be set to 255.
    adc_config.mode = ADC_READ_TOUT_MODE;
    adc_config.clk_div = 8; // ADC sample collection clock = 80MHz/clk_div = 10MHz
    ESP_ERROR_CHECK(adc_init(&adc_config));

    // 2. Create a adc task to read adc value
    xTaskCreate(adc_task, "adc_task", 1024*2, NULL, 5, NULL);
        char data1 [10];
        

    // pwm_init(PWM_PERIOD1, &duties, 1, &pin_num);
    // pwm_set_phases(&phase);
    // ESP_LOGI (TAG2, "Frekuensi 1");
    // pwm_start();
        uint32_t duties = duties_percent*PWM_PERIOD/100;
         pwm_init(PWM_PERIOD, &duties, 1, &pin_num);
while (1) 
    {
            
    duties = duties_percent*PWM_PERIOD/100;
    pwm_set_duties (&duties);
    pwm_set_period (PWM_PERIOD);
    pwm_set_phases(&phase);
    // // ESP_LOGI (TAG2, "Frekuensi 1");
    pwm_start();
    vTaskDelay (1000/ portTICK_RATE_MS);
   // pwm_stop (0x0);

//      duties = duties_percent*PWM_PERIOD2/100;
//      pwm_set_duties(&duties);
//      pwm_set_period (PWM_PERIOD2);
//     pwm_set_phases(&phase);
//   //  // ESP_LOGI (TAG2, "Frekuensi 2");
//     pwm_start();
//     vTaskDelay (1000/ portTICK_RATE_MS);
//     pwm_stop (0x0);

//      duties = duties_percent*PWM_PERIOD3/100;
//      pwm_set_duties(&duties);
//      pwm_set_period (PWM_PERIOD3);
//     pwm_set_phases(&phase);
//     // // ESP_LOGI (TAG2, "Frekuensi 3");
//     pwm_start();
//     vTaskDelay (1000/ portTICK_RATE_MS);
//     pwm_stop (0x0);
   // // xTaskCreate(pwm_task, "pwm_task", 1024*2, NULL, 5, NULL);
    
    const char *my_topic = "my_topic";
    sprintf( data1, "%d", nilai);
    
   // //maESP_LOGI(TAG1,  "kirim : %s", data1);
    esp_mqtt_client_publish(client, my_topic, &data1, 0, 0, 0);
    }


}


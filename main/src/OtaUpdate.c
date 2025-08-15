
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"

#include "OtaUpdate.h"
#include "TaskPriorities.h"
#include "Utilities.h"
#include "NotificationEvents.h"

// Internal Function Declarations
static esp_err_t HttpEventHandler(esp_http_client_event_t *evt);
static esp_err_t CheckUpdateRequired(OtaUpdate * this, esp_app_desc_t * new_app_info);
static void OtaUpdateTask(void *pvParameters);

// Internal Constants
static const char * TAG = "ota_task";

#if defined(TRON_BADGE)
#define OTA_URL CONFIG_OTA_UPDATE_URL"_TRON"
#elif defined(REACTOR_BADGE)
#define OTA_URL CONFIG_OTA_UPDATE_URL"_REACTOR"
#elif defined(CREST_BADGE)
#define OTA_URL CONFIG_OTA_UPDATE_URL"_CREST"
#elif defined(FMAN25_BADGE)
#define OTA_URL CONFIG_OTA_UPDATE_URL"_FMAN25"
#endif

#define HTTP_RESPONSE_BUFFER_SIZE 2048

#define OTA_STATUS_PRINT_STEP   10
#define OTA_CHECK_DELAY_HOURS   1
#define HOURS_TO_MS             60 * 60 * 1000                      // 1 Hour
#define OTA_CHECK_DELAY_MS      OTA_CHECK_DELAY_HOURS * HOURS_TO_MS
#define OTA_WIFI_WAIT_TIME_MS   0 * 60 * 1000                       // Immediately turn on wifi

esp_err_t OtaUpdate_Init(OtaUpdate *this, WifiClient *pWifiClient, NotificationDispatcher * pNotificationDispatcher)
{
    assert(this);
    memset(this, 0, sizeof(*this));

    this->pNotificationDispatcher = pNotificationDispatcher;
    this->pWifiClient = pWifiClient;

    assert(xTaskCreatePinnedToCore(OtaUpdateTask, "OtaUpdateTask", configMINIMAL_STACK_SIZE * 3, this, OTA_UPDATE_TASK_PRIORITY, NULL, APP_CPU_NUM) == pdPASS);
    return ESP_OK;
}

// Checks header information to see if updating should proceed
static esp_err_t CheckUpdateRequired(OtaUpdate * this, esp_app_desc_t * new_app_info)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);

    if (new_app_info)
    {
        const esp_partition_t *running = esp_ota_get_running_partition();
        esp_app_desc_t running_app_info;
        if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) 
        {
            ESP_LOGI(TAG, "current firmware version:");
            ESP_LOG_BUFFER_HEX_LEVEL(TAG, running_app_info.app_elf_sha256, sizeof(running_app_info.app_elf_sha256), ESP_LOG_INFO);
            ESP_LOGI(TAG, "new firmware version:");
            ESP_LOG_BUFFER_HEX_LEVEL(TAG, new_app_info->app_elf_sha256, sizeof(new_app_info->app_elf_sha256), ESP_LOG_INFO);
            if (memcmp(new_app_info->app_elf_sha256, running_app_info.app_elf_sha256, sizeof(new_app_info->app_elf_sha256)) == 0) 
            {
                ESP_LOGI(TAG, "Current version matches update. OTA Skip");
            }
            else
            {
                // TODO: Do we need to validate this version?
                ESP_LOGI(TAG, "OTA Update Starting");
                ret = ESP_OK;
                NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_OTA_REQUIRED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
            }
        }

        // TODO: We can implement anti-rollback later if we care, excluding checks for now
    }

    return ret;
}

#ifdef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
static void CancelRollback(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
    {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
        {
            if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK)
            {
                ESP_LOGI(TAG, "App is valid, rollback cancelled successfully");
            }
            else
            {
                ESP_LOGE(TAG, "Failed to cancel rollback");
            }
        }
    }
}
#endif

static void OtaUpdateTask(void *pvParameters)
{
    OtaUpdate * this = (OtaUpdate *)pvParameters;
    assert(this);

    char * response_buffer = calloc(1, HTTP_RESPONSE_BUFFER_SIZE + 1); // +1 for null terminator
    // Subscribe to wifi client events
    while(true && response_buffer)
    {
        // Turn on wifi to check
        WifiClient_RequestConnect(this->pWifiClient, OTA_WIFI_WAIT_TIME_MS);

        // Wait for connect
        if(WifiClient_WaitForConnected(this->pWifiClient) == ESP_OK)
        {
            ESP_LOGI(TAG, "Connected to WiFi");
            vTaskDelay(pdMS_TO_TICKS(5000));

            ESP_LOGI(TAG, "Making request to %s", OTA_URL);
            esp_http_client_config_t http_config = 
            {
                .url = OTA_URL,
                .timeout_ms = CONFIG_OTA_UPDATE_RECV_TIMEOUT,
                .crt_bundle_attach = esp_crt_bundle_attach,         // Attach the default certificate bundle
                .skip_cert_common_name_check = false,               // Allow any CN with cert
                .event_handler = HttpEventHandler,
                .user_data = response_buffer,
                .disable_auto_redirect = true,
                .keep_alive_enable = true,
            };

            // Grab the new OTA image
            esp_https_ota_config_t ota_config = 
            {
                .http_config = &http_config,
            };
            esp_https_ota_handle_t https_ota_handle = NULL;
            if(esp_https_ota_begin(&ota_config, &https_ota_handle) == ESP_OK)
            {
                esp_app_desc_t app_desc;
                if (esp_https_ota_get_img_desc(https_ota_handle, &app_desc) == ESP_OK) 
                {
                    if (CheckUpdateRequired(this, &app_desc) == ESP_OK) 
                    {
                        ESP_LOGI(TAG, "image update required");
                        ESP_LOGI(TAG, "image download starting");
                        NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_OTA_DOWNLOAD_INITIATED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);

                        // Retrieve the ota image
                        esp_err_t ota_status;
                        int ota_image_size = esp_https_ota_get_image_size(https_ota_handle);
                        while((ota_status = esp_https_ota_perform(https_ota_handle)) == ESP_ERR_HTTPS_OTA_IN_PROGRESS)
                        {
                            static int print_percent = OTA_STATUS_PRINT_STEP;
                            float current_percent = ((float)esp_https_ota_get_image_len_read(https_ota_handle)) / ota_image_size * 100;
                            if(current_percent > print_percent)
                            {
                                ESP_LOGI(TAG, "Firmware image download progress(%.0f%%)", current_percent);
                                print_percent += OTA_STATUS_PRINT_STEP;
                            }
                        }

                        // Check if transfer completed
                        if(esp_https_ota_is_complete_data_received(https_ota_handle))
                        {
                            ESP_LOGI(TAG, "Firmware image download complete");

                            esp_err_t ota_finish_err = esp_https_ota_finish(https_ota_handle);
                            if ((ota_status == ESP_OK) && (ota_finish_err == ESP_OK)) 
                            {
                                ESP_LOGI(TAG, "Firmware upgrade successful. Rebooting in one");
                                NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_OTA_DOWNLOAD_COMPLETE, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
                                vTaskDelay(pdMS_TO_TICKS(1000));
                                esp_restart();
                            }
                            else
                            {
                                if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) 
                                {
                                    ESP_LOGE(TAG, "firmware validation failed, image corrupted");
                                }
                                ESP_LOGE(TAG, "firmware upgrade failed 0x%x", ota_finish_err);
                                NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_OTA_DOWNLOAD_COMPLETE, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
                            }
                        }
                        else
                        {
                            ESP_LOGE(TAG, "Failed to retrieve complete firmware image");
                            NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_OTA_DOWNLOAD_COMPLETE, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
                        }
                    }
                    else
                    {
                        esp_https_ota_abort(https_ota_handle);
                        ESP_LOGI(TAG, "update not required");
                    }
                }
                else
                {
                    esp_https_ota_abort(https_ota_handle);
                    ESP_LOGE(TAG, "esp_https_ota_get_img_desc failed");
                }
            }
            else
            {
                ESP_LOGE(TAG, "esp_https_ota_begin failed");
            }
        }
        else
        {
            ESP_LOGW(TAG, "Failed to connect to WiFi");
        }

        WifiClient_Disconnect(this->pWifiClient);

#ifdef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
        // TODO: Is this enough testing or do we need more?
        // Currently if we can enable and disable wifi after an OTA check, we will cancel app rollback
        CancelRollback();
#endif

        vTaskDelay(pdMS_TO_TICKS(OTA_CHECK_DELAY_MS));
    }

    ESP_LOGE(TAG, "OtaUpdateTask exiting...");

    // This shouldn't exit but just in case
    free(response_buffer);
    response_buffer = NULL;
    vTaskDelete(NULL);
}

static esp_err_t HttpEventHandler(esp_http_client_event_t *evt)
{
    static int output_len;
    switch(evt->event_id) 
    {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);

            // Clean the buffer in case of a new request
            if (output_len == 0 && evt->user_data)
            {
                memset(evt->user_data, 0, HTTP_RESPONSE_BUFFER_SIZE);
            }
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) 
            {
                int32_t copy_len = 0;
                // Copy data from http response
                if (evt->user_data)
                {
                    // The last byte in evt->user_data is kept for the NULL character in case of out-of-bound access.
                    copy_len = MIN(evt->data_len, (HTTP_RESPONSE_BUFFER_SIZE - output_len));
                    if (copy_len)
                    {
                        memcpy(evt->user_data + output_len, evt->data, copy_len);
                    }
                }
                output_len += copy_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH: %d", output_len);
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) 
            {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            output_len = 0;
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            // Not going to follow redirect for now
            // esp_http_client_set_header(evt->client, "Accept", "text/html");
            //esp_http_client_set_redirection(evt->client);
            break;
    }
    return ESP_OK;
}

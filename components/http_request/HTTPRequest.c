
#include <stdio.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/list.h"

#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"

#include "cJSON.h"

#include "HTTPRequest.h"
#include "Utilities.h"
#include "TimeUtils.h"


// Internal Function Declarations
static esp_err_t HttpEventHandler(esp_http_client_event_t *evt);
static void HTTPRequestTask(void *pvParameters);

// Internal Constants
#define MAX_PENDING_REQUESTS        10
#define WIFI_CONNECT_IMMEDIATELY    0
#define MUTEX_WAIT_TIME_MS          10000
#define WIFI_WAIT_TIMEOUT_MS        12000
#define HTTP_TIMEOUT_MS             10000
#define HTTP_REQUEST_EXPIRE_TIME_MS WIFI_WAIT_TIMEOUT_MS

static const char * TAG             = "HTTP";

esp_err_t HTTPRequest_Init(HTTPRequest *this, WifiClient *pWifiClient, int taskPriority, int cpu)
{
    assert(this);
    memset(this, 0, sizeof(*this));
    
    // Initialize queue for requests and responses
    assert(Vector_Init(&this->pendingRequests, sizeof(HTTPRequest_Request), MAX_PENDING_REQUESTS) == ESP_OK);

    // Intialize rest of structure variables
    this->pWifiClient = pWifiClient;
    this->requestMutex = xSemaphoreCreateMutex();
    assert(this->requestMutex);

    assert(xTaskCreatePinnedToCore(HTTPRequestTask, "HTTPRequestTask", configMINIMAL_STACK_SIZE * 4, this, taskPriority, NULL, cpu) == pdPASS);
    return ESP_OK;
}

static void HTTPRequest_Request_Init(HTTPRequest_Request *this, HTTPRequest_HTTPMethodTypes methodType, 
                                     uint32_t waitTimeMs, const uint8_t *url, uint8_t *pData, uint32_t dataLength,
                                     const uint8_t *pContentType, uint32_t contentTypeLength,
                                     HTTPRequest_ResponseHandler responseHandler,
                                     bool disableAutoRedirect, bool skipCertCommonNameCheck, 
                                     uint32_t timeoutMs)
{
    assert(this);
    memset(this, 0, sizeof(*this));
    this->sendTime = TimeUtils_GetFutureTimeTicks(waitTimeMs);
    this->expireTime = TimeUtils_GetFutureTimeTicks(waitTimeMs + HTTP_REQUEST_EXPIRE_TIME_MS);
    this->methodType = methodType;
    this->waitTimeMs = waitTimeMs;
    strncpy((char *)this->url, (char *)url, MIN(HTTP_REQUEST_MAX_URL_LENGTH, strlen((char *)url)));
    if (pData != NULL)
    {
        memcpy((char *)this->pData, (char *)pData, MIN(HTTP_REQUEST_MAX_REQUEST_DATA_SIZE, dataLength));
        this->dataLength = MIN(HTTP_REQUEST_MAX_REQUEST_DATA_SIZE, dataLength);
    }
    if (pContentType != NULL)
    {
        memcpy((char *)this->contentType, (char *)pContentType, MIN(HTTP_REQUEST_MAX_CONTENT_TYPE_LENGTH, contentTypeLength));
        this->contentTypeLength = MIN(HTTP_REQUEST_MAX_CONTENT_TYPE_LENGTH, contentTypeLength);
    }
    this->responseHandler = responseHandler;
    this->disableAutoRedirect = disableAutoRedirect;
    this->skipCertCommonNameCheck = skipCertCommonNameCheck;
    this->timeoutMs = timeoutMs;
}

esp_err_t HTTPRequest_Create(HTTPRequest *this, HTTPRequest_HTTPMethodTypes methodType, 
                             uint32_t waitTimeMs, const uint8_t *url, HTTPRequest_ResponseHandler responseHandler,
                             uint8_t *pData, uint32_t dataLength,
                             const uint8_t *pContentType, uint32_t contentTypeLength,
                             bool disableAutoRedirect, bool skipCertCommonNameCheck,
                             uint32_t timeoutMs)
{
    assert(this);

    esp_err_t retVal = ESP_OK;

    HTTPRequest_Request request;
    HTTPRequest_Request_Init(&request, methodType, waitTimeMs, url, pData, dataLength, pContentType, contentTypeLength, responseHandler, disableAutoRedirect, skipCertCommonNameCheck, timeoutMs);

    // Issue wifi connect request specifying acceptable wait time
    WifiClient_RequestConnect(this->pWifiClient, waitTimeMs);

    if(xSemaphoreTake(this->requestMutex, pdMS_TO_TICKS(MUTEX_WAIT_TIME_MS)) == pdTRUE)
    {
        if (Vector_PushBack(&this->pendingRequests, &request) != ESP_OK)
        {
            // No more space left
            retVal = ESP_ERR_NO_MEM;
        }
        xSemaphoreGive(this->requestMutex);
    }
    else
    {
        ESP_LOGE(TAG, "HTTPRequest_Create failed to obtain mutex");
        retVal = ESP_ERR_TIMEOUT;
    }

    return retVal;
}


// Assumes WIFI is enabled already from task
void _HTTPRequest_ServiceRequestList(HTTPRequest *this)
{
    assert(this);

    if (this->pendingRequests.size == 0)
    {
        return;
    }

    // if wifi is connected
    if (WifiClient_GetState(this->pWifiClient) != WIFI_CLIENT_STATE_CONNECTED)
    {
        ESP_LOGI(TAG, "Wifi not connected");
        return;
    }
    
    // Process all requests in list once wifi is connected
    HTTPRequest_Request * pRequest = NULL;
    while (true)
    {
        // if wifi is connected
        if (WifiClient_GetState(this->pWifiClient) != WIFI_CLIENT_STATE_CONNECTED)
        {
            ESP_LOGE(TAG, "Wifi lost connection while processing HTTP request list");
            // Pop front of queue
            if(xSemaphoreTake(this->requestMutex, pdMS_TO_TICKS(MUTEX_WAIT_TIME_MS)) == pdTRUE)
            {
                for (uint32_t i = 0; i < this->pendingRequests.size; i++)
                {
                    pRequest = Vector_At(&this->pendingRequests, i);
                    if (pRequest != NULL && pRequest->expireTime < TimeUtils_GetCurTimeTicks())
                    {
                        Vector_Erase(&this->pendingRequests, i);
                        i--;
                        WifiClient_Disconnect(this->pWifiClient);
                    }
                }
                
                xSemaphoreGive(this->requestMutex);
            }
            else
            {
                ESP_LOGE(TAG, "HTTPRequest_ServiceRequestList failed to obtain mutex");
                break;
            }
        }
        
        HTTPRequest_Request request;
        // Pop front of queue
        if(xSemaphoreTake(this->requestMutex, pdMS_TO_TICKS(MUTEX_WAIT_TIME_MS)) == pdTRUE)
        {
            esp_err_t err = Vector_PopFront(&this->pendingRequests, &request, sizeof(HTTPRequest_Request));
            xSemaphoreGive(this->requestMutex);

            // exit loop when the pendingRequests vector is empty
            if (err != ESP_OK)
            {
                break;
            }
        }
        else
        {
            ESP_LOGE(TAG, "HTTPRequest_ServiceRequestList failed to obtain mutex");
            break;
        }
        
        // Process request
        esp_http_client_config_t http_config = 
        {
            .timeout_ms = request.timeoutMs,                                   // Timeout
            .crt_bundle_attach = esp_crt_bundle_attach,                         // Attach the default certificate bundle
            .skip_cert_common_name_check = request.skipCertCommonNameCheck,     // Allow any CN with cert
            .event_handler = HttpEventHandler,                                  // Event handler
            .user_data = request.response.pData,                                // User data
            .disable_auto_redirect = request.disableAutoRedirect,               // Disable auto redirect
        };

        esp_http_client_handle_t client = esp_http_client_init(&http_config);
        switch(request.methodType)
        {
            case HTTP_REQUEST_HTTP_METHOD_GET:
                esp_http_client_set_method(client, HTTP_METHOD_GET);
                break;
            case HTTP_REQUEST_HTTP_METHOD_POST:
                esp_http_client_set_method(client, HTTP_METHOD_POST);
                if (request.contentTypeLength > 0)
                {
                    esp_http_client_set_header(client, "Content-Type", (char *)request.contentType);
                }
                esp_http_client_set_post_field(client, (char *)request.pData, request.dataLength);
                break;
            default:
                ESP_LOGW(TAG, "Invalid method type");
                break;
        }

        // Block and wait for response or error
        esp_err_t err = esp_http_client_perform(client);
        if(err == ESP_OK)
        {
            request.response.statusCode =  esp_http_client_get_status_code(client);
            request.response.dataLength = esp_http_client_get_content_length(client);

            ESP_LOGI(TAG, "HTTP Status = %lu, content_length = %lu", request.response.statusCode, request.response.dataLength);
            ESP_LOGI(TAG, "Response: %s", request.response.pData);

            // Verify first byte is not NULL
            if (request.response.pData[0] != 0)
            {
                ESP_LOG_BUFFER_HEX_LEVEL(TAG, request.response.pData, request.response.dataLength, ESP_LOG_DEBUG);
                request.responseHandler(this, &request.response);
            }
            else
            {
                ESP_LOGE(TAG, "JSON null");
            }
        }
        else
        {
            ESP_LOGE(TAG, "HTTP Request Failed: %s", esp_err_to_name(err));
        }

        esp_http_client_cleanup(client);
        WifiClient_Disconnect(this->pWifiClient);
    }
}

static void HTTPRequestTask(void *pvParameters)
{
    HTTPRequest * this = (HTTPRequest *)pvParameters;
    assert(this);

    while(true)
    {
        _HTTPRequest_ServiceRequestList(this);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGE(TAG, "HTTPRequestTask exiting...");
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
                memset(evt->user_data, 0, HTTP_REQUEST_MAX_RESPONSE_DATA_SIZE);
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
                    copy_len = MIN(evt->data_len, (HTTP_REQUEST_MAX_RESPONSE_DATA_SIZE - output_len));
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

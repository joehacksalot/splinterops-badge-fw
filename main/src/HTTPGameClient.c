
#include <stdio.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"

#include "cJSON.h"

#include "BatterySensor.h"
#include "GameState.h"
#include "HTTPGameClient.h"
#include "TaskPriorities.h"
#include "TimeUtils.h"
#include "Utilities.h"

// Internal Function Declarations
static esp_err_t HttpEventHandler(esp_http_client_event_t *evt);
static void HTTPGameClientTask(void *pvParameters);
static void HTTPGameClient_GameStateRequestNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData);
static esp_err_t _ParseJsonResponseString(char * pData, HeartBeatResponse *pHeartBeatResponse);
static void _PrintHeartBeatResponse(HeartBeatResponse *pHeartBeatResponse);


// Internal Constants
#define MAX_PENDING_REQUESTS        3
#define WIFI_CONNECT_IMMEDIATELY    0
#define MUTEX_WAIT_TIME_MS          10000
#define WIFI_WAIT_TIMEOUT_MS        12000
#define HTTP_TIMEOUT_MS             10000
#define HTTP_REQUEST_EXPIRE_TIME_MS WIFI_WAIT_TIMEOUT_MS

static const char * TAG             = "HGC";
static const char * HEARTBEAT_URL   = "https://us-central1-iwc-dc32.cloudfunctions.net/heartbeat";
static const char * PEER_REPORT_JSON_TEMPLATE = "{\"uuid\":\"%s\", \"peakRssi\":%d, \"eventUuid\":\"%s\"}";
static const char * HEARTBEAT_JSON_TEMPLATE   = 
"{\
    \"uuid\": \"%s\",\
    \"key\": \"%s\",\
    \"provisionKey\": \"0ec91eff86a15baad0759477770f0698\",\
    \"peerReport\": [%s],\
    \"enrolledEvent\": \"%s\",\
    \"badgeRequestTime\": %d,\
    \"badgeType\": \"%d\",\
    \"songs\": [%s],\
    \"stats\":{\
      \"numPowerOns\": %d,\
      \"numTouches\": %d,\
      \"numTouchCmds\": %d,\
      \"numLedCycles\": %d,\
      \"numBattChecks\": %d,\
      \"numBleEnables\": %d,\
      \"numBleDisables\": %d,\
      \"numBleSeqXfers\": %d,\
      \"numBleSetXfers\": %d,\
      \"numUartInputs\": %d,\
      \"numNetworkTests\": %d,\
      \"numBattery\": %d,\
      \"timestamp\": %d\
    }\
}";

// NOTE: ASSUMES THE CALLER HAS THE MUTEX
esp_err_t _RequestQueue_Enqueue(HTTPGameClientRequestList* this, HTTPGameClient_Request* request)
{
    assert(this);
    assert(request);

    esp_err_t retVal = ESP_OK;

    uint32_t copySize = request->dataLength >= HTTPGAMECLIENT_MAX_REQUEST_DATA_SIZE ? HTTPGAMECLIENT_MAX_REQUEST_DATA_SIZE : request->dataLength;

    if(this->size == 0)
    {
        // First insert
        HTTPGameClient_RequestItem * pNewItem = calloc(1, sizeof(HTTPGameClient_RequestItem));
        assert(pNewItem);
        // Copy the data into the new item
        memcpy(pNewItem->request.pData, request->pData, copySize);
        pNewItem->request.dataLength = copySize;
        pNewItem->request.requestType = request->requestType;
        pNewItem->request.methodType = request->methodType;
        pNewItem->request.waitTimeMs = request->waitTimeMs;
        pNewItem->sendTime = TimeUtils_GetCurTimeTicks() + pdMS_TO_TICKS(request->waitTimeMs);
        pNewItem->expireTime = pNewItem->sendTime + pdMS_TO_TICKS(HTTP_REQUEST_EXPIRE_TIME_MS);

        this->head = pNewItem;
        this->tail = pNewItem;
        ++this->size;
    }
    else if(this->capacity > this->size)
    {
        // Search to see if there is another request of the same type
        HTTPGameClient_RequestItem * pItem = NULL;
        for(HTTPGameClient_RequestItem * pCurr = this->head; pCurr != NULL; pCurr = pCurr->pNext)
        {
            if(pCurr->request.requestType == request->requestType && 
               pCurr->request.methodType == request->methodType)
            {
                pItem = pCurr;
                break;
            }
        }

        if(pItem != NULL)
        {
            // Overwrite the data
            memcpy(pItem->request.pData, request->pData, copySize);
            pItem->request.dataLength = copySize;
            pItem->request.waitTimeMs = request->waitTimeMs;
            pItem->sendTime = TimeUtils_GetCurTimeTicks() + pdMS_TO_TICKS(request->waitTimeMs);
            pItem->expireTime = pItem->sendTime + pdMS_TO_TICKS(HTTP_REQUEST_EXPIRE_TIME_MS);
        }
        else
        {
            HTTPGameClient_RequestItem * pNewItem = calloc(1, sizeof(HTTPGameClient_RequestItem));
            assert(pNewItem);

            // Copy the data into the new item
            memcpy(pNewItem->request.pData, request->pData, copySize);
            pNewItem->request.dataLength = copySize;
            pNewItem->request.requestType = request->requestType;
            pNewItem->request.methodType = request->methodType;
            pNewItem->request.waitTimeMs = request->waitTimeMs;
            pNewItem->sendTime = TimeUtils_GetCurTimeTicks() + pdMS_TO_TICKS(request->waitTimeMs);
            pNewItem->expireTime = pNewItem->sendTime + pdMS_TO_TICKS(HTTP_REQUEST_EXPIRE_TIME_MS);

            // Update linked list
            this->tail->pNext = pNewItem;
            this->tail = pNewItem;
            ++this->size;
        }
    }
    else
    {
        // No more space left
        retVal = ESP_ERR_NO_MEM;
    }

    return retVal;
}

// This will delete the item pointed to by request pointer (needs to match pointer exactly)
// NOTE: ASSUMES THE CALLER HAS THE MUTEX
esp_err_t _RequestQueue_Dequeue(HTTPGameClientRequestList* this, HTTPGameClient_RequestItem* request)
{
    assert(this);
    assert(request);

    esp_err_t retVal = ESP_FAIL;

    HTTPGameClient_RequestItem * pDelete = NULL;
    HTTPGameClient_RequestItem * pBeforeDelete = NULL;

    //Search for the one to delete
    for(HTTPGameClient_RequestItem * pCurr = this->head; pCurr != NULL; pCurr = pCurr->pNext)
    {
        if(pCurr == request)
        {
            pDelete = pCurr;
            break;
        }
        pBeforeDelete = pCurr;
    }

    if(pDelete != NULL)
    {
        if(this->size == 1)
        {
            // Delete last item
            free(pDelete);
            pDelete = this->head = this->tail = NULL;
        }
        else
        {
            if(pDelete == this->head)
            {
                // If we delete the first item in the list, update head
                this->head = pDelete->pNext;
            }
            else if(pDelete == this->tail)
            {
                // If we delete the last item in the list, update tail
                this->tail = pBeforeDelete;
                this->tail->pNext = NULL;
            }
            else
            {
                // Deleting something in the middle of the list
                // Point over deleted item, even if NULL
                pBeforeDelete->pNext = pDelete->pNext;
            }
            free(pDelete);
            pDelete = NULL;
        }
        

        ESP_LOGI(TAG, "Item deleted from list");
        --this->size;
        retVal = ESP_OK;
    }
    else
    {
        ESP_LOGI(TAG, "No item deleted from list");
    }

    return retVal;
}

esp_err_t HTTPGameClient_Init(HTTPGameClient *this, WifiClient *pWifiClient, NotificationDispatcher *pNotificationDispatcher, BatterySensor *pBatterySensor)
{
    assert(this);
    memset(this, 0, sizeof(*this));
    
    // Initialize queue for requests and responses
    memset(&this->requestQueue, 0, sizeof(this->requestQueue));
    memset(&this->response, 0, sizeof(this->response));
    this->requestQueue.capacity = MAX_PENDING_REQUESTS;

    // Intialize rest of structure variables
    this->pNotificationDispatcher = pNotificationDispatcher;
    this->pWifiClient = pWifiClient;
    this->pBatterySensor = pBatterySensor;
    this->requestMutex = xSemaphoreCreateMutex();
    assert(this->requestMutex);

    // char * responseTest = "{\"stones\":[1],\"songs\":[5,2],\"event\":{\"event\":\"QlgVrlHvkZs=\",\"stoneColor\":\"Yellow\",\"power\":64.12,\"msRemaining\":304.05200004577637},\"badgeRequestTime\":7429,\"serverResponseTime\":{\"tv_sec\":1722212789,\"tv_nsec\":948000000}}";
    // HeartBeatResponse response;
    // _ParseJsonResponseString(responseTest, &response);
    // _PrintHeartBeatResponse(&response);

    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(this->pNotificationDispatcher, NOTIFICATION_EVENTS_WIFI_HEARTBEAT_READY_TO_SEND, &HTTPGameClient_GameStateRequestNotificationHandler, this));

    assert(xTaskCreatePinnedToCore(HTTPGameClientTask, "HTTPGameClientTask", configMINIMAL_STACK_SIZE * 4, this, HTTP_GAME_CLIENT_TASK_PRIORITY, NULL, APP_CPU_NUM) == pdPASS);
    return ESP_OK;
}

// NOTE: ASSUMES THE CALLER HAS THE MUTEX
void _HTTPGameClient_RemoveExpiredFromList(HTTPGameClient * this)
{
    assert(this);

    // See if any requests have expired, remove them
    if(this->requestQueue.size > 0)
    {
        HTTPGameClient_RequestItem * pCurr = this->requestQueue.head;
        HTTPGameClient_RequestItem * pNext = pCurr->pNext;
        uint32_t queueSize = this->requestQueue.size;  // Save off the size while we delete
        for(unsigned int i = 0; i < queueSize; ++i)
        {
            if(TimeUtils_IsTimeExpired(pCurr->expireTime))
            {
                ESP_LOGI(TAG, "Request(%d, %d) expired, removing from queue", pCurr->request.methodType, pCurr->request.requestType);
                _RequestQueue_Dequeue(&this->requestQueue, pCurr);
            }

            if(pNext != NULL)
            {
                // There are still more items in the List
                pCurr = pNext;
                pNext = pCurr->pNext;
            }
        }
    }
}


static const char * _GetGameStatus(GameState_EventColor eventColor)
{
    switch(eventColor)
    {
        case GAMESTATE_EVENTCOLOR_RED:
            return "Red";
            break;
        case GAMESTATE_EVENTCOLOR_GREEN:
            return "Green";
            break;
        case GAMESTATE_EVENTCOLOR_YELLOW:
            return "Yellow";
            break;
        case GAMESTATE_EVENTCOLOR_MAGENTA:
            return "Magenta";
            break;
        case GAMESTATE_EVENTCOLOR_BLUE:
            return "Blue";
            break;
        case GAMESTATE_EVENTCOLOR_CYAN:
            return "Cyan";
            break;
        default:
            return "Unknown";
            break;
    }
}

static void _PrintHeartBeatResponse(HeartBeatResponse *pHeartBeatResponse)
{
    ESP_LOGI(TAG, "HeartBeatResponse: ");
    ESP_LOGI(TAG, "    stoneBits:         0x%02x", pHeartBeatResponse->status.statusData.stoneBits);
    ESP_LOGI(TAG, "    songUnlockedBits:  0x%04x", pHeartBeatResponse->status.statusData.songUnlockedBits);
    ESP_LOGI(TAG, "    currentEventIdB64: %s", pHeartBeatResponse->status.eventData.currentEventIdB64);
    ESP_LOGI(TAG, "    currentEventColor: %s", _GetGameStatus(pHeartBeatResponse->status.eventData.currentEventColor));
    ESP_LOGI(TAG, "    powerLevel:        %u", pHeartBeatResponse->status.eventData.powerLevel);
    ESP_LOGI(TAG, "    mSecRemaining:     %lu", pHeartBeatResponse->status.eventData.mSecRemaining);
}

static esp_err_t _ParseJsonResponseString(char * pData, HeartBeatResponse *pHeartBeatResponse)
{
    esp_err_t ret = ESP_FAIL;
    ESP_LOGI(TAG, "Parsing JSON Response: %s", pData);
    cJSON * root = cJSON_Parse(pData);
    if (root != NULL)
    {
        ret = ESP_OK;
        HeartBeatResponse tmpHeartBeatResponse = {0};
        cJSON *stonesArray = cJSON_GetObjectItem(root, "stones");
        if (stonesArray != NULL)
        {
            int stonesArraySize = cJSON_GetArraySize(stonesArray);
            for (int stonesIndex = 0; stonesIndex < stonesArraySize; stonesIndex++)
            {
                cJSON *stone = cJSON_GetArrayItem(stonesArray, stonesIndex);
                int stoneIndex = (int)stone->valueint;
                if (stoneIndex > 0 && stoneIndex <= NUM_GAMESTATE_EVENTCOLORS)
                {
                    tmpHeartBeatResponse.status.statusData.stoneBits |= (1 << (stoneIndex - 1));
                }
                else
                {
                    ESP_LOGE(TAG, "Stone index %d out of range", stoneIndex);
                }
            }
        }

        cJSON *songsArray = cJSON_GetObjectItem(root, "songs");
        if (songsArray != NULL)
        {
            int songsArraySize = cJSON_GetArraySize(songsArray);
            for (int songsIndex = 0; songsIndex < songsArraySize; songsIndex++)
            {
                cJSON *song = cJSON_GetArrayItem(songsArray, songsIndex);
                int songIndex = (int)song->valueint;
                if (songIndex > 0 && songIndex <= OCARINA_NUM_SONGS)
                {
                    tmpHeartBeatResponse.status.statusData.songUnlockedBits |= (1 << (songIndex - 1));
                }
                else
                {
                    ESP_LOGE(TAG, "Song index %d out of range", songIndex);
                }
            }
        }
        else
        {
            ESP_LOGE(TAG, "No timestamp found");
        }

        cJSON *event = cJSON_GetObjectItem(root, "event");
        if (event != NULL)
        {
            cJSON *eventId = cJSON_GetObjectItem(event, "event");
            if (eventId != NULL)
            {
                strncpy((char *)tmpHeartBeatResponse.status.eventData.currentEventIdB64, eventId->valuestring, sizeof(tmpHeartBeatResponse.status.eventData.currentEventIdB64));
                ESP_LOGI(TAG, "Event id: %s", tmpHeartBeatResponse.status.eventData.currentEventIdB64);
            }
            else
            {
                strncpy((char *)tmpHeartBeatResponse.status.eventData.currentEventIdB64, "AAAAAAAAAAA=", sizeof(tmpHeartBeatResponse.status.eventData.currentEventIdB64));
            }
            cJSON *stoneColor = cJSON_GetObjectItem(event, "stoneColor");
            if (stoneColor != NULL)
            {
                tmpHeartBeatResponse.status.eventData.currentEventColor = (GameState_EventColor)stoneColor->valueint-1;
            }
            
            cJSON *power = cJSON_GetObjectItem(event, "power");
            if (power != NULL)
            {
                double powerDbl = power->valuedouble;
                tmpHeartBeatResponse.status.eventData.powerLevel = (uint8_t)powerDbl;
            }
            
            cJSON *msRemaining = cJSON_GetObjectItem(event, "msRemaining");
            if (msRemaining != NULL)
            {
                tmpHeartBeatResponse.status.eventData.mSecRemaining = (uint32_t)msRemaining->valueint;
            }
        }
        else
        {
            strncpy((char *)tmpHeartBeatResponse.status.eventData.currentEventIdB64, "AAAAAAAAAAA=", sizeof(tmpHeartBeatResponse.status.eventData.currentEventIdB64));
        }
        
        cJSON *badgeRequestTime = cJSON_GetObjectItem(root, "badgeRequestTime");
        uint32_t rttMsec = 0;
        if (badgeRequestTime != NULL)
        {
            uint32_t badgeRequestTimeTicks = badgeRequestTime->valueint;
            uint32_t currTimeTicks = TimeUtils_GetCurTimeTicks();
            uint32_t rttTicks = (currTimeTicks - badgeRequestTimeTicks);
            uint32_t halfRttTicks = rttTicks / 2;
            rttMsec = TimeUtils_GetMSecFromTicks(halfRttTicks);
        }

        cJSON *serverResponseTime = cJSON_GetObjectItem(root, "serverResponseTime");
        if (serverResponseTime != NULL)
        {
            cJSON *tv_sec = cJSON_GetObjectItem(serverResponseTime, "tv_sec");
            cJSON *tv_nsec = cJSON_GetObjectItem(serverResponseTime, "tv_nsec");
            if (tv_sec != NULL && tv_nsec != NULL)
            {
                struct timeval newTime = { .tv_sec = (time_t)tv_sec->valueint, .tv_usec = (suseconds_t)(tv_nsec->valueint/1000) + rttMsec };
                
                // Set the system time
                if (settimeofday(&newTime, NULL) == 0)
                {
                    struct timeval tv;

                    gettimeofday(&tv, NULL);
                    ESP_LOGI(TAG, "Successfully set the system time to %s", ctime(&tv.tv_sec));
                }
                else
                {
                    ESP_LOGE(TAG, "Failed to set the system time");
                }
            }
            else
            {
                ESP_LOGE(TAG, "No tv_sec or tv_nsec found");
            }
        }
        else
        {
            ESP_LOGE(TAG, "No timestamp found");
        }

        cJSON_Delete(root);
        memcpy(pHeartBeatResponse, &tmpHeartBeatResponse, sizeof(*pHeartBeatResponse));
        ret = ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "JSON parse failed. json = \"%s\"", (char *)pData);
    }
    return ret;
}


// Assumes WIFI is enabled already from task
void _HTTPGameClient_ProcessRequestList(HTTPGameClient * this)
{
    assert(this);
    assert(this->pNotificationDispatcher);

    ESP_LOGI(TAG, "Processing list");

    if(xSemaphoreTake(this->requestMutex, pdMS_TO_TICKS(MUTEX_WAIT_TIME_MS)) == pdTRUE)
    {
        // Clear out requests that may have expired while waiting for wifi
        // Edge condition where request will expire as we process other HTTP requests, going to let that happen for now
        _HTTPGameClient_RemoveExpiredFromList(this);

        for(HTTPGameClient_RequestItem * pCurr = this->requestQueue.head; pCurr != NULL; pCurr = pCurr->pNext)
        {
            if(WifiClient_GetState(this->pWifiClient) != WIFI_CLIENT_STATE_CONNECTED)
            {
                ESP_LOGI(TAG, "Wifi no longer connected");
                break;
            }

            // Go through queue and process all requests
            esp_http_client_config_t http_config = 
            {
                .timeout_ms = HTTP_TIMEOUT_MS,
                .crt_bundle_attach = esp_crt_bundle_attach,         // Attach the default certificate bundle
                .skip_cert_common_name_check = false,               // Allow any CN with cert
                .event_handler = HttpEventHandler,
                .user_data = this->response.pData,
                //.disable_auto_redirect = true,
            };

            switch(pCurr->request.requestType)
            {
                // case HTTPGAMECLIENT_HTTPREQUEST_LOGIN:
                //     http_config.url = LOGIN_URL;
                //     break;
                case HTTPGAMECLIENT_HTTPREQUEST_HEARTBEAT:
                    http_config.url = HEARTBEAT_URL;
                    break;
                // case HTTPGAMECLIENT_HTTPREQUEST_VERIFY:
                //     http_config.url = VERIFY_URL;
                //     break;
                case HTTPGAMECLIENT_HTTPREQUEST_NONE:
                default:
                    ESP_LOGW(TAG, "Invalid request type");
                    continue;
                    break;
            }

            esp_http_client_handle_t client = esp_http_client_init(&http_config);
            switch(pCurr->request.methodType)
            {
                case HTTPGAMECLIENT_HTTPMETHOD_GET:
                    esp_http_client_set_method(client, HTTP_METHOD_GET);
                    break;
                case HTTPGAMECLIENT_HTTPMETHOD_POST:
                    esp_http_client_set_method(client, HTTP_METHOD_POST);
                    esp_http_client_set_header(client, "Content-Type", "application/json");
                    esp_http_client_set_post_field(client, (char *)pCurr->request.pData, pCurr->request.dataLength);
                    break;
                default:
                    ESP_LOGW(TAG, "Invalid method type");
                    break;
            }

            // Block and wait for response or error
            esp_err_t err = esp_http_client_perform(client);
            if(err == ESP_OK)
            {
                this->response.statusCode =  esp_http_client_get_status_code(client);
                this->response.dataLength = esp_http_client_get_content_length(client);

                ESP_LOGI(TAG, "HTTP Status = %lu, content_length = %lu", this->response.statusCode, this->response.dataLength);
                ESP_LOGI(TAG, "Response: %s", this->response.pData);

                switch(pCurr->request.requestType)
                {
                    // case HTTPGAMECLIENT_HTTPREQUEST_LOGIN:
                    //     ESP_LOGI(TAG, "Login Response Sent");
                    //     NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_WIFI_HEARTBEAT_RESPONSE_RECV, &this->response, sizeof(HTTPGameClient_Response), DEFAULT_NOTIFY_WAIT_DURATION);
                    //     break;
                    case HTTPGAMECLIENT_HTTPREQUEST_HEARTBEAT:
                        ESP_LOGI(TAG, "Heartbeat Response Sent");
                        // Verify first byte is not NULL
                        if (this->response.pData[0] != 0)
                        {
                            if (_ParseJsonResponseString((char *)this->response.pData, &this->responseStruct) == ESP_OK)
                            {
                                // ESP_LOGI(TAG, "Event id: %s", this->responseStruct.status.eventData.currentEventIdB64);
                                _PrintHeartBeatResponse(&this->responseStruct);
                                NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_WIFI_HEARTBEAT_RESPONSE_RECV, &this->responseStruct, sizeof(this->responseStruct), DEFAULT_NOTIFY_WAIT_DURATION);
                            }
                            else
                            {
                                ESP_LOGE(TAG, "Failed to parse JSON response");
                            }

                        }
                        else
                        {
                            ESP_LOGE(TAG, "JSON null");
                        }
                        break;
                    // case HTTPGAMECLIENT_HTTPREQUEST_VERIFY:
                    //     ESP_LOGI(TAG, "Verify Response Sent");
                    //     NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_WIFI_HEARTBEAT_RESPONSE_RECV, &this->response, sizeof(HTTPGameClient_Response), DEFAULT_NOTIFY_WAIT_DURATION);
                    //     break;
                    case HTTPGAMECLIENT_HTTPREQUEST_NONE:
                    default:
                        // This should never get here!
                        ESP_LOGW(TAG, "Invalid request type received");
                        break;
                }
            }
            else
            {
                ESP_LOGE(TAG, "HTTP Request Failed: %s", esp_err_to_name(err));
            }

            esp_http_client_cleanup(client);
        }

        // Clear out List
        while(this->requestQueue.size > 0)
        {
            _RequestQueue_Dequeue(&this->requestQueue, this->requestQueue.head);
        }

        xSemaphoreGive(this->requestMutex);
    }
    else
    {
        ESP_LOGE(TAG, "_HTTPGameClient_ProcessRequestList failed to obtain mutex");
    }
}

static void HTTPGameClientTask(void *pvParameters)
{
    HTTPGameClient * this = (HTTPGameClient *)pvParameters;
    assert(this);

    while(true)
    {
        // Check if queue has data (I know this does not grab mutex but in the worst case we wait another 10 ms. We use the mutex when we use the queue)
        if(this->requestQueue.size > 0)
        {
            int nextStartTimeMS = -INT_MAX;
            HTTPGameClient_RequestItem * pShortestRequest = NULL;

            if(xSemaphoreTake(this->requestMutex, pdMS_TO_TICKS(MUTEX_WAIT_TIME_MS)) == pdTRUE)
            {
                // See if any requests have expired, remove them
                _HTTPGameClient_RemoveExpiredFromList(this);

                // Find the next request that needs to be sent, based on wait time
                for(HTTPGameClient_RequestItem * pCurr = this->requestQueue.head; pCurr != NULL; pCurr = pCurr->pNext)
                {
                    // ElapsedTime returns time since the argument passed in. We want positive values for times that have since started
                    int startTimeMS = TimeUtils_GetElapsedTimeMSec(pCurr->sendTime);
                    if(startTimeMS > nextStartTimeMS)
                    {
                        nextStartTimeMS = startTimeMS;
                        pShortestRequest = pCurr;
                    }
                }

                xSemaphoreGive(this->requestMutex);
            }
            else
            {
                ESP_LOGE(TAG, "HTTPGameClientTask failed to obtain mutex");
            }

            if(pShortestRequest != NULL)
            {
                int timeToWaitMS = 0;
                if(nextStartTimeMS >= 0)
                {
                    // Start time has already passed, enable immediately
                    WifiClient_RequestConnect(this->pWifiClient, WIFI_CONNECT_IMMEDIATELY);
                    timeToWaitMS = WIFI_WAIT_TIMEOUT_MS;
                }
                else
                {
                    WifiClient_RequestConnect(this->pWifiClient, abs(nextStartTimeMS));
                    timeToWaitMS = abs(nextStartTimeMS) + WIFI_WAIT_TIMEOUT_MS;
                }

                TickType_t timeout = TimeUtils_GetFutureTimeTicks(timeToWaitMS);
                ESP_LOGI(TAG, "Waiting for wifi to connect (%d), timeout in %d ms", nextStartTimeMS, timeToWaitMS);

                // Wait for wifi to connect with a timeout
                while((WifiClient_GetState(this->pWifiClient) != WIFI_CLIENT_STATE_CONNECTED &&
                       WifiClient_GetState(this->pWifiClient) != WIFI_CLIENT_STATE_FAILED)   && 
                       !TimeUtils_IsTimeExpired(timeout))
                {
                    if(xSemaphoreTake(this->requestMutex, pdMS_TO_TICKS(MUTEX_WAIT_TIME_MS)) == pdTRUE)
                    {
                        // We will also check the queue if a shorter request has been made
                        // TODO: FINISH THIS
                        xSemaphoreGive(this->requestMutex);
                    }
                    else
                    {
                        ESP_LOGE(TAG, "HTTPGameClientTask failed to obtain mutex");
                    }
                    vTaskDelay(pdMS_TO_TICKS(100));
                }

                // TODO: Handle processing of queue
                if(WifiClient_GetState(this->pWifiClient) == WIFI_CLIENT_STATE_CONNECTED)
                {
                    // With wifi enabled, process queue
                    ESP_LOGI(TAG, "Connected to WiFi");

                    _HTTPGameClient_ProcessRequestList(this);

                }
                else
                {
                    ESP_LOGW(TAG, "Failed to connect to WiFi");
                }

                // Disconnect even if we aren't successful to decrement number of users
                WifiClient_Disconnect(this->pWifiClient);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGE(TAG, "HTTPGameClientTask exiting...");
}

static void HTTPGameClient_GameStateRequestNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData)
{
    assert(pObj);
    assert(notificationData);
    HTTPGameClient * this = (HTTPGameClient *)pObj;
    HeartBeatRequest *pRequest = (HeartBeatRequest *)notificationData;

    // Prepare peer report json
    memset(this->peerReport, 0, sizeof(this->peerReport));
    int offset = 0;
    if (pRequest->numPeerReports > 0)
    {
        offset = snprintf(this->peerReport, sizeof(this->peerReport), PEER_REPORT_JSON_TEMPLATE, pRequest->peerReports[0].badgeIdB64, pRequest->peerReports[0].peakRssi, pRequest->peerReports[0].eventIdB64);
        if (offset < 0)
        {
            ESP_LOGE(TAG, "Failed to add peer report to json");
            return;
        }
    }

    for (int i = 1; i < MIN(pRequest->numPeerReports, MAX_PEER_MAP_DEPTH); i++)
    {
        int res1 = snprintf(this->peerReport + offset, sizeof(this->peerReport) - offset, ",");
        if (res1 < 0)
        {
            ESP_LOGE(TAG, "Failed to add peer report to json");
            return;
        }

        offset += res1;
        int res2 = snprintf(this->peerReport + offset, sizeof(this->peerReport) - offset, PEER_REPORT_JSON_TEMPLATE, pRequest->peerReports[i].badgeIdB64, pRequest->peerReports[i].peakRssi, pRequest->peerReports[i].eventIdB64);
        if (res2 < 0)
        {
            ESP_LOGE(TAG, "Failed to add peer report to json");
            return;
        }

        offset += res2;
    }

    char eventIdStr[sizeof(pRequest->gameStateData.status.eventData.currentEventIdB64)+1] = {0};
    memcpy(eventIdStr, pRequest->gameStateData.status.eventData.currentEventIdB64, sizeof(pRequest->gameStateData.status.eventData.currentEventIdB64));

    // Prepare full request json
    // HTTPGameClient_Request *pHttpRequest = (HTTPGameClient_Request *)malloc(sizeof(HTTPGameClient_Request));
    HTTPGameClient_Request *pHttpRequest = (HTTPGameClient_Request *)malloc(sizeof(HTTPGameClient_Request));
    
    pHttpRequest->methodType = HTTPGAMECLIENT_HTTPMETHOD_POST;
    pHttpRequest->requestType = HTTPGAMECLIENT_HTTPREQUEST_HEARTBEAT;
    pHttpRequest->waitTimeMs = pRequest->waitTimeMs;
    
    struct timeval tv;
    gettimeofday(&tv, NULL); // timezone structure is obsolete
    int coded_badge_type = GetBadgeType();
    char songsStr[27] = {0}; // size based on max possible size of song string with null terminator: "1,2,3,4,5,6,7,8,9,10,11,12"
    int songStrOffset = 0;
    bool first = true;
    for (int i = 0; i < OCARINA_NUM_SONGS; i++)
    {
        if (pRequest->gameStateData.status.statusData.songUnlockedBits & (1 << i))
        {
            if (first)
            {
                first = false;
            }
            else
            {
                int res1 = snprintf(songsStr + songStrOffset, sizeof(songsStr) - songStrOffset, ",");
                if (res1 < 0)
                {
                    ESP_LOGE(TAG, "Failed to add song to json");
                    return;
                }

                songStrOffset += res1;
            }

            int res2 = snprintf(songsStr + songStrOffset, sizeof(songsStr) - songStrOffset, "%d", i + 1);
            if (res2 < 0)
            {
                ESP_LOGE(TAG, "Failed to add song to json");
                return;
            }

            songStrOffset += res2;
        }
    }

    TickType_t curTimeTicks = TimeUtils_GetCurTimeTicks();
    int len = snprintf((char *)pHttpRequest->pData, sizeof(pHttpRequest->pData), HEARTBEAT_JSON_TEMPLATE, 
                        pRequest->badgeIdB64,  // uuid %s
                        pRequest->keyB64,      // key %s
                                               // provisionKey hard coded
                        this->peerReport,      // peerReport %s 
                        eventIdStr,            // enrolledEvent %s
                        curTimeTicks,          // badgeRequestTime %d
                        coded_badge_type,      // badgeType %d
                        songsStr,              // songs %s
                        pRequest->badgeStats.numPowerOns,
                        pRequest->badgeStats.numTouches,
                        pRequest->badgeStats.numTouchCmds,
                        pRequest->badgeStats.numLedCycles,
                        pRequest->badgeStats.numBattChecks,
                        pRequest->badgeStats.numBleEnables,
                        pRequest->badgeStats.numBleDisables,
                        pRequest->badgeStats.numBleSeqXfers,
                        pRequest->badgeStats.numBleSetXfers,
                        pRequest->badgeStats.numUartInputs,
                        pRequest->badgeStats.numNetworkTests,
                        BatterySensor_GetBatteryPercent(this->pBatterySensor),
                        tv.tv_sec);
    ESP_LOGI(TAG, "Heartbeat JSON: %s", pHttpRequest->pData);
    pHttpRequest->dataLength = len;

    ESP_LOGI(TAG, "Handling GameState Request Notification: %s, %lu", eventBase, notificationEvent);

    if(xSemaphoreTake(this->requestMutex, pdMS_TO_TICKS(MUTEX_WAIT_TIME_MS)) == pdTRUE)
    {
        // Try to add to queue to be processed later
        if(!_RequestQueue_Enqueue(&this->requestQueue, pHttpRequest) == ESP_OK)
        {
            ESP_LOGE(TAG, "GameStateRequest failed to enqueue request");
        }

        xSemaphoreGive(this->requestMutex);
    }
    else
    {
        ESP_LOGE(TAG, "GameStateRequest failed to obtain mutex");
    }
    // free((void*)pHttpRequest);
    free((void*)pHttpRequest);
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
                memset(evt->user_data, 0, HTTPGAMECLIENT_MAX_RESPONSE_DATA_SIZE);
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
                    copy_len = MIN(evt->data_len, (HTTPGAMECLIENT_MAX_RESPONSE_DATA_SIZE - output_len));
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

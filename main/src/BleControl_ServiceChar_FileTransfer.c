
#include <string.h>

#include "esp_log.h"
#include "host/ble_gatt.h"

#include "BleControl.h"
#include "BleControl_Service.h"
#include "JsonUtils.h"
#include "LedSequences.h"

static esp_err_t _BleControl_ProcessTransferedFile(BleControl *this);
static esp_err_t _BleControl_VerifyAllFramesPresent(BleControl *this);
void _BleControl_ResetFrameContext(BleControl *this);

/**
 * @file BleControl_ServiceChar_FileTransfer.c
 * @brief BLE File Transfer characteristic implementation.
 *
 * Handles reception of multi-frame files over the GATT File Transfer
 * characteristic. Supports three payload types:
 * - Settings JSON, applied to UserSettings and forwarded to the system.
 * - Custom LED sequence JSON, installed into the LedSequences module.
 * - Pairing test JSON, used to confirm and persist a new pairId.
 *
 * Responsibilities:
 * - Parse and validate the configuration frame, detect pairing updates, and
 *   initialize the transfer context.
 * - Receive and assemble data frames into an internal buffer with bounds
 *   checking and progress notifications.
 * - Verify completeness and process the final file according to type.
 * - Reset the transfer context for subsequent transfers.
 */
#define TAG "BLE"

/**
 * @brief Receive a chunk of file-transfer data over BLE.
 *
 * Accepts either a configuration frame (frame 0) or a data frame (frame >= 1).
 * On config, initializes the transfer context and updates pairing if needed.
 * On data, copies bytes into the receive buffer, emits progress notifications,
 * and processes the file when all frames are present.
 *
 * @param this Pointer to BleControl instance.
 * @param data Incoming ATT write payload buffer.
 * @param size Length of the payload in bytes.
 * @param final Indicates end-of-transfer hint from client (not strictly
 *              required; completeness is verified via frame counts).
 * @return ESP_OK on success; ESP_FAIL on invalid frames or bounds errors.
 */
esp_err_t _BleControl_BleReceiveFileDataAction(BleControl *this, uint8_t * data, int size, bool final)
{
    esp_err_t ret = ESP_OK;
    assert(this);
    if (!this->fileTransferFrameContext.configFrameProcessed && size == CONFIG_FRAME_HEADER_SIZE)
    {
        uint16_t curFrame  = (((uint16_t)data[0] << 8) | data[1]); // 0 based frame index
        uint16_t numFrames = (((uint16_t)data[2] << 8) | data[3]); // 0 based frame count
        uint16_t frameLen  = (((uint16_t)data[4] << 8) | data[5]);
        uint8_t  fileType  = (uint8_t)data[6];
        uint8_t  *pairId   = (uint8_t*)&data[7];

        if (curFrame == 0 && numFrames == 0 && frameLen > DATA_FRAME_HEADER_SIZE && frameLen < DATA_FRAME_MAX_SIZE)
        {
            if (memcmp(this->pUserSettings->settings.pairId, pairId, PAIR_ID_SIZE))
            {
                ESP_LOGI(TAG, "Pairing to new device. mobileId = %s", pairId);
                UserSettings_SetPairId(this->pUserSettings, pairId);
                _BleControl_RefreshServiceUuid(this);
            }
        }
        else if (curFrame == 0 && numFrames > 0 && frameLen > DATA_FRAME_HEADER_SIZE && frameLen < DATA_FRAME_MAX_SIZE)
        {
            this->fileTransferFrameContext.configFrameProcessed = true;
            this->fileTransferFrameContext.frameReceived[curFrame] = 1;
            this->fileTransferFrameContext.curNumFrames = numFrames+1; // store 1 based frame count
            this->fileTransferFrameContext.frameLen = frameLen;
            this->fileTransferFrameContext.fileType = fileType;

            if (memcmp(this->pUserSettings->settings.pairId, pairId, PAIR_ID_SIZE))
            {
                ESP_LOGI(TAG, "Pairing to new device. pairId = %s", pairId);
                UserSettings_SetPairId(this->pUserSettings, pairId);
                _BleControl_RefreshServiceUuid(this);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Invalid frame count");
        }
    }
    else
    {
        if (size > DATA_FRAME_HEADER_SIZE)
        {
            uint16_t curFrame = (((uint16_t)data[0] << 8) | data[1]);
            int curFrameSize = this->fileTransferFrameContext.frameLen - DATA_FRAME_HEADER_SIZE;
            int curBufferOffset = (curFrame - 1) * curFrameSize; // -1 because the data frames begin at 1
            const uint8_t *frameBuffer = &data[DATA_FRAME_HEADER_SIZE];

            if ((curBufferOffset + curFrameSize < MAX_BLE_FILE_TRANSFER_FILE_SIZE) && (curFrame < MAX_BLE_FRAMES))
            {
                ESP_LOGD(TAG, "Loading frame %d data at offset %d:%d", curFrame, curBufferOffset, curBufferOffset+curFrameSize);
                uint32_t percentComplete;

                // avoid divide by zero
                if (this->fileTransferFrameContext.curNumFrames == 0)
                {
                    percentComplete = 100;
                }
                else
                {
                    percentComplete = (((curFrame+1)*100)/this->fileTransferFrameContext.curNumFrames);
                }

                ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_FILE_SERVICE_PERCENT_CHANGED, &percentComplete, sizeof(percentComplete), DEFAULT_NOTIFY_WAIT_DURATION);
                memcpy((void*)&this->fileTransferFrameContext.rcvBuffer[curBufferOffset], frameBuffer, curFrameSize);
                this->fileTransferFrameContext.frameReceived[curFrame] = 1;
                this->fileTransferFrameContext.frameBytesReceived += curFrameSize;
                if (_BleControl_VerifyAllFramesPresent(this) == ESP_OK)
                {
                    ESP_LOGI(TAG, "Processing completed file. file size=%d", this->fileTransferFrameContext.frameBytesReceived);
                    ret = _BleControl_ProcessTransferedFile(this);
                    if (ret == ESP_FAIL)
                    {
                        ESP_LOGE(TAG, "_BleControl_BleReceiveFileDataAction: _BleControl_ProcessTransferedFile failed ret=%s", esp_err_to_name(ret));
                    }
                    // ret = BleControl_DisableBleService(this, true);
                    // if (ret == ESP_FAIL)
                    // {
                    //     ESP_LOGE(TAG, "_BleControl_BleReceiveFileDataAction: BleControl_DisableBleService failed ret=%s", esp_err_to_name(ret));
                    // }
                }
            }
            else
            {
                ret = ESP_FAIL;
                ESP_LOGE(TAG, "Frame would exceed maximum file size. curBufferOffset = %d, size = %d", curBufferOffset, size);
            }
        }
        else
        {
            ret = ESP_FAIL;
            ESP_LOGE(TAG, "Frame has insufficient data. size = %d", size);
        }
    }
    return ret;
}

/**
 * @brief Verify that all expected data frames have been received.
 *
 * @param this Pointer to BleControl instance.
 * @return ESP_OK if all frames are present; ESP_FAIL otherwise.
 */
static esp_err_t _BleControl_VerifyAllFramesPresent(BleControl *this)
{
    esp_err_t ret = ESP_FAIL;
    int count = 0;
    assert(this);
    if (this->fileTransferFrameContext.curNumFrames > 0)
    {
        for (int i = 0; i < this->fileTransferFrameContext.curNumFrames; i++)
        {
            count += this->fileTransferFrameContext.frameReceived[i];
        }
        if (count == this->fileTransferFrameContext.curNumFrames)
        {
            ret = ESP_OK;
        }
    }
    return ret;
}

/**
 * @brief Build a read response for the File Transfer characteristic.
 *
 * Populates a compact status/identity structure including badge id,
 * select settings, badge type, song unlock bits, and current SSID.
 *
 * @param this Pointer to BleControl instance.
 * @param buffer Output buffer to write response bytes into.
 * @param size Size of the buffer in bytes.
 * @param pLength Out: number of bytes written to buffer.
 * @return ESP_OK on success.
 */
esp_err_t _BleControl_GetFileTransferReadResponse(BleControl *this, uint8_t * buffer, uint32_t size, uint16_t * pLength)
{
    assert(this);
    assert(buffer);
    assert(pLength);

    BleFileTransferResponseData settingsResponseData;

    memcpy(settingsResponseData.badgeId, this->pUserSettings->badgeId, sizeof(settingsResponseData.badgeId));
    settingsResponseData.packedSettings.soundEnabled = this->pUserSettings->settings.soundEnabled;
    settingsResponseData.packedSettings.vibrationEnabled = this->pUserSettings->settings.vibrationEnabled;
    settingsResponseData.badgeType = GetBadgeType();
    settingsResponseData.songBits = this->pGameState->gameStateData.status.statusData.songUnlockedBits;
    memcpy(settingsResponseData.ssid, this->pUserSettings->settings.wifiSettings.ssid, sizeof(settingsResponseData.ssid));

    memcpy(buffer, &settingsResponseData, sizeof(settingsResponseData));
    *pLength = sizeof(settingsResponseData);
    return ESP_OK;
}

/**
 * @brief Process a fully received file according to its type.
 *
 * Validates JSON and performs one of:
 * - Update custom LED sequence and notify completion.
 * - Forward settings JSON to NotificationDispatcher for application.
 * - Acknowledge pairing success and notify the system.
 * Emits a NOTIFICATION_EVENTS_BLE_FILE_COMPLETE or _FAILED event at the end.
 *
 * @param this Pointer to BleControl instance.
 * @return ESP_OK if processing succeeded; ESP_FAIL otherwise.
 */
static esp_err_t _BleControl_ProcessTransferedFile(BleControl *this)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);
    if (this->fileTransferFrameContext.fileProcessed == false)
    {
        this->fileTransferFrameContext.fileProcessed = true;
        if (JsonUtils_ValidateJson((const char *)this->fileTransferFrameContext.rcvBuffer))
        {
            ESP_LOGI(TAG, "Valid JSON");
            if (this->fileTransferFrameContext.fileType == FILE_TYPE_LED_SEQUENCE)
            {
                ESP_LOGI(TAG, "Updating custom led sequence");
                if (LedSequences_UpdateCustomLedSequence(0, (const char * const)this->fileTransferFrameContext.rcvBuffer, MAX_BLE_FILE_TRANSFER_FILE_SIZE) != ESP_FAIL)
                {
                    ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_FILE_LEDJSON_RECVD, &this->fileTransferFrameContext.curCustomSeqSlot, sizeof(this->fileTransferFrameContext.curCustomSeqSlot), DEFAULT_NOTIFY_WAIT_DURATION);
                    ESP_LOGI(TAG, "NotificationDispatcher_NotifyEvent for NOTIFICATION_EVENTS_BLE_FILE_LEDJSON_RECVD event ret=%s", esp_err_to_name(ret));
                    _BleControl_ResetFrameContext(this);
                    ret = ESP_OK;
                }
                else
                {
                    ESP_LOGE(TAG, "Update of custom led sequence %d failed", this->fileTransferFrameContext.curCustomSeqSlot);
                }
            }
            else if (this->fileTransferFrameContext.fileType == FILE_TYPE_SETTINGS_FILE)
            {
                ESP_LOGI(TAG, "Updating settings");
                ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_FILE_SETTINGS_RECVD, (void *)this->fileTransferFrameContext.rcvBuffer, MIN(this->fileTransferFrameContext.frameBytesReceived, MAX_BLE_FILE_TRANSFER_FILE_SIZE), DEFAULT_NOTIFY_WAIT_DURATION);
                ESP_LOGI(TAG, "NotificationDispatcher_NotifyEvent for NOTIFICATION_EVENTS_BLE_FILE_SETTINGS_RECVD event ret=%s", esp_err_to_name(ret));
                _BleControl_ResetFrameContext(this);
                ret = ESP_OK;
            }
            else if (this->fileTransferFrameContext.fileType == FILE_TYPE_TEST)
            {
                ESP_LOGI(TAG, "Pairing Successful. Pair JSON = %s", this->fileTransferFrameContext.rcvBuffer);
                ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_NEW_PAIR_RECV, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
                ESP_LOGI(TAG, "NotificationDispatcher_NotifyEvent for NOTIFICATION_EVENTS_BLE_NEW_PAIR_RECV event ret=%s", esp_err_to_name(ret));
                _BleControl_ResetFrameContext(this);
                ret = ESP_OK;
            }
            else
            {
                ESP_LOGE(TAG, "Invalid file type %d", this->fileTransferFrameContext.fileType);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Invalid JSON");
        }
        
    }
    else
    {
        ESP_LOGI(TAG, "Frame already processed");
    }

    if (ret == ESP_OK)
    {
        ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_FILE_COMPLETE, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
        ESP_LOGI(TAG, "NotificationDispatcher_NotifyEvent for NOTIFICATION_EVENTS_BLE_FILE_COMPLETE event ret=%s", esp_err_to_name(ret));
    }
    else
    {
        ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_FILE_FAILED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
        ESP_LOGE(TAG, "NotificationDispatcher_NotifyEvent for NOTIFICATION_EVENTS_BLE_FILE_FAILED event ret=%s", esp_err_to_name(ret));
    }
    return ret;
}

/**
 * @brief Reset the file transfer frame context to initial state.
 *
 * Clears frame bookkeeping and receive buffer for the next transfer.
 *
 * @param this Pointer to BleControl instance.
 */
void _BleControl_ResetFrameContext(BleControl *this)
{
    assert(this);
    memset((void*)this->fileTransferFrameContext.frameReceived, 0, MAX_BLE_FRAMES);
    this->fileTransferFrameContext.curNumFrames = 0;
    this->fileTransferFrameContext.frameLen = 0;
    this->fileTransferFrameContext.curCustomSeqSlot = 0;
    this->fileTransferFrameContext.fileType = 0;
    this->fileTransferFrameContext.configFrameProcessed = false;
    this->fileTransferFrameContext.fileProcessed = false;
    this->fileTransferFrameContext.frameBytesReceived = 0;
    this->fileTransferFrameContext.frameInProgress = false;
    if(this->fileTransferFrameContext.rcvBuffer)
    {
        memset((void*)this->fileTransferFrameContext.rcvBuffer, 0, MAX_BLE_FILE_TRANSFER_FILE_SIZE);
    }
}

/**
 * @brief Initialize the File Transfer characteristic state.
 *
 * Currently a placeholder; retained for symmetry and future expansion.
 *
 * @param this Pointer to BleControl instance.
 */
void BleControl_ServiceChar_FileTransfer_Init(BleControl *this)
{

}
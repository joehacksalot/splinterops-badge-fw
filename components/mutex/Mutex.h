
#ifndef MUTEX_JOSE_H_
#define MUTEX_JOSE_H_

esp_err_t Mutex_Create(SemaphoreHandle_t *pMutex);
esp_err_t Mutex_Free(SemaphoreHandle_t *pMutex);
esp_err_t Mutex_Lock(SemaphoreHandle_t *pMutex, TickType_t xTicksToWait);
esp_err_t Mutex_Unlock(SemaphoreHandle_t *pMutex);

#endif // MUTEX_JOSE_H_

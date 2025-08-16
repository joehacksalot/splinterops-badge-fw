
#ifndef DISK_UTILITIES_H_
#define DISK_UTILITIES_H_

#include "BatterySensor.h"
#include "esp_err.h"

esp_err_t DiskUtilities_InitFs(void);
esp_err_t DiskUtilities_InitNvs(void);
extern esp_err_t ReadFileFromDisk(char *filename, char *buffer, int bufferSize, int * pReadBytes, int expectedFileSize);
extern esp_err_t WriteFileToDisk(BatterySensor * pBatterySensor, char *filename, char *buffer, int bufferSize);

#endif // DISK_UTILITIES_H_

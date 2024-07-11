
#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_


#define MOUNT_PATH "/data"

#include "BatterySensor.h"
#include "DiskUtilities.h"
#include "esp_err.h"

esp_err_t DiskUtilities_InitFs(void);
esp_err_t DiskUtilities_InitNvs(void);
extern esp_err_t ReadFileFromDisk(char *filename, char *buffer, int buffer_size, int expected_file_size);
extern esp_err_t WriteFileToDisk(BatterySensor * pBatterySensor, char *filename, char *buffer, int buffer_size);

#endif // FILESYSTEM_H_

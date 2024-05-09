
#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_


#define MOUNT_PATH "/data"

#include "esp_err.h"

esp_err_t DiscUtils_InitFs(void);
esp_err_t DiscUtils_InitNvs(void);

#endif // FILESYSTEM_H_


#include "SystemState.h"

void app_main()
{
   SystemState *systemState = SystemState_GetInstance();
   SystemState_Init(systemState);
}

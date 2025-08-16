
#ifndef UTILITIES_H_
#define UTILITIES_H_

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif
#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

extern uint32_t GetRandomNumber(uint32_t min, uint32_t max);

#endif // UTILITIES_H_
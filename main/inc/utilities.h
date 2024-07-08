
#ifndef UTILITIES_H_
#define UTILITIES_H_

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

extern void registerCurrentTaskInfo(void);
extern void displayTaskInfoArray(void);

#endif // UTILITIES_H_
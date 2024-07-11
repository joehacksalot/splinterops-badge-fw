
#ifndef UTILITIES_H_
#define UTILITIES_H_

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif
#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

extern void registerCurrentTaskInfo(void);
extern void displayTaskInfoArray(void);
extern int GetBadgeType(void);

#endif // UTILITIES_H_
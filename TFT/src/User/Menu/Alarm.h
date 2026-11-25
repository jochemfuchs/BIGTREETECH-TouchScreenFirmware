#ifndef _ALARM_H_
#define _ALARM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

void menuAlarm(void);
// called from parseAck() for every received line; returns true if it was an "ALARM:" line
bool alarmRx(const char * line);

#ifdef __cplusplus
}
#endif

#endif

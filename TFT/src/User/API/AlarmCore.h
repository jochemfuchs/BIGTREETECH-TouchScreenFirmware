#ifndef _ALARM_CORE_H_
#define _ALARM_CORE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ALARM_PIN_MIN         4
#define ALARM_PIN_MAX         10
#define ALARM_TEXT_MAX        48
#define ALARM_LINE_MAX        64
#define ALARM_WATCHDOG_MS     90000u
#define ALARM_PIN_TIMEOUT_MS  30000u
#define ALARM_SEND_TIMEOUT_MS 10000u
#define ALARM_POPUP_MS        5000u

typedef enum
{
  ALARM_GREEN,
  ALARM_RED,
  ALARM_ORANGE,
  ALARM_TRIGGERED,
  ALARM_GREY,
} ALARM_COLOR;

typedef struct
{
  char state[ALARM_TEXT_MAX + 1];  // last raw HA state, "" until the first STATE line
  char pin[ALARM_PIN_MAX + 1];     // digits typed so far
  char popup[ALARM_TEXT_MAX + 1];  // "" = no popup
  uint32_t lastRxMs;
  uint32_t lastKeyMs;
  uint32_t sendStartMs;
  uint32_t popupStartMs;
  bool everRx;
  bool sending;
} ALARM_CORE;

void alarmCoreInit(ALARM_CORE * c, uint32_t now);
// returns true if the line was an "ALARM:" line (consumed, even if malformed)
bool alarmCoreRx(ALARM_CORE * c, const char * line, uint32_t now);
// key: '0'..'9', 'B' = backspace, 'C' = clear
void alarmCoreKey(ALARM_CORE * c, char key, uint32_t now);
// action: "away" | "home" | "disarm". On success writes "ALARM:CMD <action> <pin>\n" to out,
// clears the PIN and enters "sending". Returns false (and changes nothing) if not allowed.
bool alarmCoreCmd(ALARM_CORE * c, const char * action, char * out, size_t outSize, uint32_t now);
void alarmCoreTick(ALARM_CORE * c, uint32_t now);
void alarmCoreDismissPopup(ALARM_CORE * c);
bool alarmCoreConnected(const ALARM_CORE * c, uint32_t now);
bool alarmCoreCanSend(const ALARM_CORE * c, uint32_t now);
ALARM_COLOR alarmCoreColor(const ALARM_CORE * c, uint32_t now);
void alarmCoreLabel(const ALARM_CORE * c, uint32_t now, char * out, size_t outSize);

#ifdef __cplusplus
}
#endif

#endif

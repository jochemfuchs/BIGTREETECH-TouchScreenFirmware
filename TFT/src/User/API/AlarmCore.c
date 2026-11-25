#include "AlarmCore.h"
#include <stdio.h>
#include <string.h>

static void copyField(char * dst, const char * src, size_t n)
{
  if (n > ALARM_TEXT_MAX)
    n = ALARM_TEXT_MAX;

  memcpy(dst, src, n);
  dst[n] = '\0';
}

void alarmCoreInit(ALARM_CORE * c, uint32_t now)
{
  memset(c, 0, sizeof(*c));
  c->lastRxMs = now;  // grace period: show "CONNECTING..." for one watchdog interval after boot
}

bool alarmCoreRx(ALARM_CORE * c, const char * line, uint32_t now)
{
  if (strncmp(line, "ALARM:", 6) != 0)
    return false;

  size_t len = strcspn(line, "\r\n");

  if (len > ALARM_LINE_MAX)
    return true;  // too long: consumed and ignored

  const char * p = line + 6;
  size_t plen = len - 6;

  if (plen > 6 && strncmp(p, "STATE ", 6) == 0)
  {
    copyField(c->state, p + 6, plen - 6);
  }
  else if (plen > 4 && strncmp(p, "ERR ", 4) == 0)
  {
    copyField(c->popup, p + 4, plen - 4);
    c->popupStartMs = now;
  }
  else
  {
    return true;  // malformed: consumed and ignored, doesn't feed the watchdog
  }

  c->lastRxMs = now;
  c->everRx = true;
  c->sending = false;
  return true;
}

void alarmCoreKey(ALARM_CORE * c, char key, uint32_t now)
{
  size_t n = strlen(c->pin);

  if (key >= '0' && key <= '9')
  {
    if (n < ALARM_PIN_MAX)
    {
      c->pin[n] = key;
      c->pin[n + 1] = '\0';
    }
  }
  else if (key == 'B')
  {
    if (n > 0)
      c->pin[n - 1] = '\0';
  }
  else if (key == 'C')
  {
    memset(c->pin, 0, sizeof(c->pin));
  }

  c->lastKeyMs = now;
}

bool alarmCoreCmd(ALARM_CORE * c, const char * action, char * out, size_t outSize, uint32_t now)
{
  if (strcmp(action, "away") != 0 && strcmp(action, "home") != 0 && strcmp(action, "disarm") != 0)
    return false;

  if (!alarmCoreCanSend(c, now))
    return false;

  int n = snprintf(out, outSize, "ALARM:CMD %s %s\n", action, c->pin);

  if (n < 0 || (size_t)n >= outSize)
    return false;

  memset(c->pin, 0, sizeof(c->pin));
  c->sending = true;
  c->sendStartMs = now;
  return true;
}

void alarmCoreTick(ALARM_CORE * c, uint32_t now)
{
  if (c->pin[0] != '\0' && now - c->lastKeyMs >= ALARM_PIN_TIMEOUT_MS)
    memset(c->pin, 0, sizeof(c->pin));

  if (c->sending && now - c->sendStartMs >= ALARM_SEND_TIMEOUT_MS)
  {
    c->sending = false;
    copyField(c->popup, "No response", strlen("No response"));
    c->popupStartMs = now;
  }

  if (c->popup[0] != '\0' && now - c->popupStartMs >= ALARM_POPUP_MS)
    c->popup[0] = '\0';
}

void alarmCoreDismissPopup(ALARM_CORE * c)
{
  c->popup[0] = '\0';
}

bool alarmCoreConnected(const ALARM_CORE * c, uint32_t now)
{
  return c->everRx && now - c->lastRxMs < ALARM_WATCHDOG_MS;
}

ALARM_COLOR alarmCoreColor(const ALARM_CORE * c, uint32_t now)
{
  if (!alarmCoreConnected(c, now))
    return ALARM_GREY;

  if (strcmp(c->state, "disarmed") == 0)
    return ALARM_GREEN;

  if (strncmp(c->state, "armed_", 6) == 0)
    return ALARM_RED;

  if (strcmp(c->state, "arming") == 0 || strcmp(c->state, "pending") == 0)
    return ALARM_ORANGE;

  if (strcmp(c->state, "triggered") == 0)
    return ALARM_TRIGGERED;

  return ALARM_GREY;  // "unavailable" and unknown states
}

bool alarmCoreCanSend(const ALARM_CORE * c, uint32_t now)
{
  return !c->sending && strlen(c->pin) >= ALARM_PIN_MIN && alarmCoreColor(c, now) != ALARM_GREY;
}

void alarmCoreLabel(const ALARM_CORE * c, uint32_t now, char * out, size_t outSize)
{
  const char * fixed = NULL;

  if (c->sending)
    fixed = "SENDING...";
  else if (!c->everRx)
    fixed = now - c->lastRxMs < ALARM_WATCHDOG_MS ? "CONNECTING..." : "NO CONNECTION";
  else if (!alarmCoreConnected(c, now))
    fixed = "NO CONNECTION";
  else if (strcmp(c->state, "unavailable") == 0)
    fixed = "HA OFFLINE";

  if (fixed != NULL)
  {
    snprintf(out, outSize, "%s", fixed);
    return;
  }

  // "armed_away" -> "ARMED AWAY"
  size_t i;

  for (i = 0; c->state[i] != '\0' && i + 1 < outSize; i++)
  {
    char ch = c->state[i];
    out[i] = (ch == '_') ? ' ' : (ch >= 'a' && ch <= 'z') ? (char)(ch - 'a' + 'A') : ch;
  }

  out[i] = '\0';
}

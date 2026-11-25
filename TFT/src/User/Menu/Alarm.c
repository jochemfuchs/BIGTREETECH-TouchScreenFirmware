#include "Alarm.h"
#include <string.h>
#include "includes.h"
#include "AlarmCore.h"

#define ALARM_IDLE_MS  60000u
#define ALARM_FLASH_MS 500u

enum
{
  K_1, K_2, K_3,
  K_4, K_5, K_6,
  K_7, K_8, K_9,
  K_BACK, K_0, K_CLEAR,
  K_AWAY, K_HOME, K_DISARM,
  K_COUNT
};

static const char keyChar[K_COUNT] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', 'B', '0', 'C'};
static const char * const keyText[K_COUNT] = {
  "1", "2", "3", "4", "5", "6", "7", "8", "9", "<", "0", "C", "AWAY", "HOME", "DISARM"};
static const char * const keyAction[K_COUNT] = {[K_AWAY] = "away", [K_HOME] = "home", [K_DISARM] = "disarm"};

// 480x320 layout: status bar on top, keypad left, action buttons right
#define KX(col)          (10 + (col) * 88)
#define KY(row)          (104 + (row) * 54)
#define KEY_RECT(col, row) {KX(col), KY(row), KX(col) + 80, KY(row) + 48}
#define ACT_RECT(row)    {290, 72 + (row) * 82, 470, 72 + (row) * 82 + 70}

static const GUI_RECT keyRect[K_COUNT] = {
  KEY_RECT(0, 0), KEY_RECT(1, 0), KEY_RECT(2, 0),
  KEY_RECT(0, 1), KEY_RECT(1, 1), KEY_RECT(2, 1),
  KEY_RECT(0, 2), KEY_RECT(1, 2), KEY_RECT(2, 2),
  KEY_RECT(0, 3), KEY_RECT(1, 3), KEY_RECT(2, 3),
  ACT_RECT(0), ACT_RECT(1), ACT_RECT(2),
};
static const GUI_RECT statusRect = {0, 0, LCD_WIDTH, 60};
static const GUI_RECT pinRect    = {10, 62, 270, 100};
static const GUI_RECT popupRect  = {40, 90, 440, 230};
static const GUI_RECT idleRect   = {0, 130, LCD_WIDTH, 190};
static const GUI_RECT fullRect   = {0, 0, LCD_WIDTH, LCD_HEIGHT};

static ALARM_CORE core;
static bool coreReady = false;
static bool idle = false;
static uint32_t lastTouchMs;

// what is currently on screen, so we only redraw what changed
static char drawnLabel[ALARM_TEXT_MAX + 1];
static uint16_t drawnColor;
static uint8_t drawnPinLen;
static bool drawnPopup, drawnIdle, drawnCanSend;

static void ensureCore(void)
{
  if (!coreReady)
  {
    alarmCoreInit(&core, OS_GetTimeMs());
    coreReady = true;
  }
}

bool alarmRx(const char * line)
{
  ensureCore();
  return alarmCoreRx(&core, line, OS_GetTimeMs());
}

static uint16_t colorValue(ALARM_COLOR col, uint32_t now)
{
  switch (col)
  {
    case ALARM_GREEN:     return MAT_GREEN;
    case ALARM_RED:       return MAT_RED;
    case ALARM_ORANGE:    return MAT_ORANGE;
    case ALARM_TRIGGERED: return ((now / ALARM_FLASH_MS) % 2) ? MAT_RED : BLACK;
    default:              return MAT_DARKGRAY;
  }
}

static void drawText(const GUI_RECT * r, const char * s, uint16_t fg, uint16_t bg)
{
  GUI_FillRectColor(r->x0, r->y0, r->x1, r->y1, bg);
  GUI_SetColor(fg);
  GUI_SetTextMode(GUI_TEXTMODE_TRANS);
  setFontSize(FONT_SIZE_LARGE);
  // call the string variant directly: the GUI_Disp* macros treat char * as a label index
  _GUI_DispStringInPrect(r, (const uint8_t *)s);
  setFontSize(FONT_SIZE_NORMAL);
}

static void alarmDrawKey(uint8_t i, uint8_t pressed)
{
  if (i >= K_COUNT || idle || core.popup[0] != '\0')
    return;

  bool enabled = i < K_AWAY || alarmCoreCanSend(&core, OS_GetTimeMs());
  uint16_t bg = pressed ? WHITE : (enabled ? MAT_SLATE : DARKGRAY);
  uint16_t fg = pressed ? BLACK : (enabled ? WHITE : GRAY);

  drawText(&keyRect[i], keyText[i], fg, bg);
}

static void alarmRefresh(bool force)
{
  uint32_t now = OS_GetTimeMs();
  char label[ALARM_TEXT_MAX + 1];

  alarmCoreLabel(&core, now, label, sizeof(label));

  uint16_t color = colorValue(alarmCoreColor(&core, now), now);
  bool popup = core.popup[0] != '\0';
  bool canSend = alarmCoreCanSend(&core, now);
  uint8_t pinLen = strlen(core.pin);
  bool statusChanged = color != drawnColor || strcmp(label, drawnLabel) != 0;

  if (popup != drawnPopup || idle != drawnIdle)
    force = true;

  if (force)
    GUI_Clear(BLACK);

  if (idle)
  {
    if (force || statusChanged)
      drawText(&idleRect, label, color, BLACK);
  }
  else
  {
    if (force || statusChanged)
      drawText(&statusRect, label, WHITE, color);

    if (popup)
    {
      if (force)
        drawText(&popupRect, core.popup, WHITE, MAT_DARKGRAY);
    }
    else
    {
      if (force || pinLen != drawnPinLen)
      {
        char dots[ALARM_PIN_MAX * 2 + 1] = "";

        for (uint8_t i = 0; i < pinLen; i++)
          strcat(dots, "* ");

        drawText(&pinRect, dots, WHITE, BLACK);
      }

      for (uint8_t i = 0; i < K_COUNT; i++)
      {
        if (force || (i >= K_AWAY && canSend != drawnCanSend))
          alarmDrawKey(i, 0);
      }
    }
  }

  strcpy(drawnLabel, label);
  drawnColor = color;
  drawnPinLen = pinLen;
  drawnPopup = popup;
  drawnIdle = idle;
  drawnCanSend = canSend;
}

static void alarmRedraw(void)
{
  setMenu(MENU_TYPE_FULLSCREEN, NULL, K_COUNT, keyRect, alarmDrawKey, alarmRedraw);
  alarmRefresh(true);
}

static void alarmHandleKey(uint16_t key, uint32_t now)
{
  lastTouchMs = now;

  if (idle)
  {
    idle = false;  // the waking touch does nothing else
    return;
  }

  if (core.popup[0] != '\0')
  {
    alarmCoreDismissPopup(&core);
    return;
  }

  if (key < K_AWAY)
  {
    BUZZER_PLAY(SOUND_KEYPRESS);
    alarmCoreKey(&core, keyChar[key], now);
  }
  else if (key < K_COUNT)
  {
    char line[ALARM_LINE_MAX + 2];

    if (alarmCoreCmd(&core, keyAction[key], line, sizeof(line), now))
    {
      BUZZER_PLAY(SOUND_KEYPRESS);
      Serial_Put(SERIAL_PORT, line);  // direct write: bypasses the gcode queue, which would wait for "ok"
      memset(line, 0, sizeof(line));  // don't leave the PIN on the stack
    }
    else
    {
      BUZZER_PLAY(SOUND_DENY);
    }
  }
}

void menuAlarm(void)
{
  ensureCore();
  idle = false;
  lastTouchMs = OS_GetTimeMs();
  alarmRedraw();

  while (MENU_IS(menuAlarm))
  {
    uint32_t now = OS_GetTimeMs();
    // ponytail: in idle/popup the whole screen is one key. If a popup expires mid-press, the
    // release lands on key 0 ("1"): at worst one stray PIN digit.
    uint16_t key = (idle || core.popup[0] != '\0') ? KEY_GetValue(1, &fullRect)
                                                   : KEY_GetValue(K_COUNT, keyRect);

    if (key != IDLE_TOUCH)
      alarmHandleKey(key, now);

    if (!idle && core.popup[0] == '\0' && now - lastTouchMs >= ALARM_IDLE_MS)
      idle = true;

    alarmCoreTick(&core, now);
    alarmRefresh(false);
    loopProcess();
  }
}

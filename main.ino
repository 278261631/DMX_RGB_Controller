#include <GyverFilters.h>

#include <EEPROM.h>
#include <DMXSerial.h>
#include <SSD1306AsciiWire.h>

#pragma pack(push, 1)
struct DMX_CHANNELS {
  unsigned char chan_r;
  unsigned char chan_g;
  unsigned char chan_b;
  unsigned char chan_br;
  unsigned char chan_strobe;
  unsigned char chan_auto;
  unsigned char chan_autoSpeed;
  unsigned char chan_autoStep;
  unsigned char chan_micMode;
};
#pragma pack(pop)

#define BEAST_NUM 666
#define CHANNELS_COUNT sizeof(DMX_CHANNELS)
#define MAX_DMX_CHANNEL 512 - CHANNELS_COUNT + 1
#define AUTO_MODES_COUNT 10
#define MAX_SLAVES_COUNT 16

#define BLINK_PERIOD 500

#define DMX_TIMEOUT 3000
#define DMX_MODE_PIN 5

struct EEPROM_Data
{
  unsigned int dmxChan;
  bool enableAuto;
  byte autoPreset;
  bool isMaster;
  unsigned int slavesCount;
  bool micBright;
  bool micBeats;
  byte bright;
  byte speed;
  byte step;
  bool rotateScreen;
  bool showLogo;
  uint16_t micLowPass;
} eData;

#pragma pack(push, 1)
struct Session_Data
{
  unsigned long lastActionTime; 
  unsigned long valueBlinkTime;
  int tmpVal;
  char menuNameEndCol;
  byte messageId;
  byte pageIndex;
  bool reboot;
  bool factory;
  bool calibrateMic;
  
  bool hideOnBlink : 1; 
  bool updateScreen : 1;
  bool updateValue : 1;
  bool editMode : 1;
  bool processNewValue : 1;
  bool displayOn : 1;
  bool dmxInputDetected : 1;
};
#pragma pack(pop)

struct Session_Data sData;

enum OledPage
{
  DMXAddr = 0,
  AutoSettings,
  EnableAuto,
  AutoPreset,
  Bright,
  Speed,
  Step,
  Master,
  Slaves,
  MicBright,
  MicBeats,
  RotateScreen,
  MicCalibrate,
  ShowLogo,
  Reboot,
  Factory,
  FactoryConfirm,
};

enum Messages
{
  NoMessage = 0,
  AnotherMasterFound, 
  LineCheck, 
  MicCalibration,
  MicCalibrationDone,
};

typedef union {
  int uIntValue;
  unsigned char uCharValue;
  bool boolValue;
} ValUnion;

typedef struct                             // Структура описывающая меню
{
  char id;                               // Идентификационный уникальный индекс ID
  char parentid;                         // ID родителя
  String _name;                         // Название
  void *value;                            // Актуальное значение
  char valueType;
  int _min;                             // Минимально возможное значение параметра
  int _max;                             // Максимально возможное значение параметра
} MenuItem;

enum ValueType {
  Menu,
  Bool,
  UInt,
  UChar,
};

#define menuArraySize 17                 // Задаем размер массива
MenuItem menus[] = {                        // Задаем пункты меню
  {DMXAddr, -1, "DMX", &eData.dmxChan, UInt, 1, MAX_DMX_CHANNEL},
  {AutoSettings, -1, "Auto", 0, Menu, 0, 0},
    {EnableAuto, AutoSettings, "Enable", &eData.enableAuto, Bool, 0, 1},
    {AutoPreset, AutoSettings,  "Preset", &eData.autoPreset, UChar, 1, AUTO_MODES_COUNT},
    {Bright, AutoSettings,  "Bright", &eData.bright, UChar, 0, 255},
    {Speed, AutoSettings,  "Speed", &eData.speed, UChar, 0, 255},
    {Step, AutoSettings,  "Step", &eData.step, UChar, 0, 255},
    {MicBright, AutoSettings,  "Vol->Br", &eData.micBright, Bool, 0, 1},
    {MicBeats, AutoSettings,  "Bt->Fx", &eData.micBeats, Bool, 0, 1},
    {Master, AutoSettings, "Master", &eData.isMaster, Bool, 0, 1},
    {Slaves, AutoSettings, "Slaves", &eData.slavesCount, UInt, 0, 16},
  {RotateScreen, -1,  "Rotate", &eData.rotateScreen, Bool, 0, 1}, 
  {MicCalibrate, -1,  "Mic calibr", &sData.calibrateMic, Bool, 0, 1}, 
  {ShowLogo, -1,  "Logo", &eData.showLogo, Bool, 0, 1},  
  {Reboot, -1,  "Reboot", &sData.reboot, Bool, 0, 1}, 
  {Factory, -1, "Reset/Wipe", 0, Menu, 0, 0},
    {FactoryConfirm, Factory, "Sure?", &sData.factory, Bool, 0, 1},
};

SSD1306AsciiWire oled;

void(* rebootFunc) (void) = 0;

void putUpdate(void *data, int size)
{
  if(!data)
  {
    return;
  }
  
  byte hash = 0;
  byte *ptr = reinterpret_cast<byte *>(data);
  for(int i = 0; i < size; ++i)
  {
    EEPROM.update(i, *ptr);
    hash += *ptr;
    ++ptr;
  }
  hash += BEAST_NUM;
  EEPROM.update(size, hash);
}

bool safeRead(void *data, int size)
{
  if(!data)
  {
    return;
  }
  
  byte hash = 0;
  byte *ptr = reinterpret_cast<byte *>(data);
  for(int i = 0; i < size; ++i)
  {
    hash += (*ptr = EEPROM.read(i));
    ++ptr;
  }
  hash += BEAST_NUM;
  return hash == EEPROM.read(size);
}

void initEData(struct EEPROM_Data *data) {
  data -> dmxChan = 1;
  data -> enableAuto = false;
  data -> autoPreset = 1;
  data -> bright = 255;
  data -> speed = 128;
  data -> step = 0;
  data -> isMaster = false;
  data -> slavesCount = 0;
  data -> micBright = false;
  data -> micBeats = false;
  data -> rotateScreen = false;
  data -> showLogo = true;
  data -> micLowPass = 100;
}

void initSData(struct Session_Data *data, const struct EEPROM_Data *eData)
{
  data -> lastActionTime = 0;
  data -> valueBlinkTime = 0;
  data -> hideOnBlink = true;
  data -> pageIndex = menuArraySize + 1;
  for(int i = 0; i < menuArraySize; ++i)
  {
    if(menus[i].id == (eData -> enableAuto ? AutoPreset : DMXAddr)) {
      data -> pageIndex = i;
      break;
    }
  }
  if(data -> pageIndex == menuArraySize + 1)
  {
    data -> pageIndex = 0;
  }
  
  data -> updateScreen = true;
  data -> updateValue = false;
  data -> editMode = false;
  data -> tmpVal = 0;
  data -> menuNameEndCol = 0;
  data -> processNewValue = false;
  data -> displayOn = true;
  data -> reboot = false;
  data -> factory = false;
  data -> dmxInputDetected = true;
  data -> messageId = NoMessage;
  data -> calibrateMic = false;
}

void setup() {
  if(!safeRead(&eData, sizeof(EEPROM_Data)))
  {
    initEData(&eData);
    putUpdate(&eData, sizeof(EEPROM_Data));
  }

  initSData(&sData, &eData);

  DMXSerial.init(DMXReceiver, DMX_MODE_PIN);
  setupOutput();
  if(eData.enableAuto && eData.isMaster) {
    showMessage(LineCheck);
    delay(DMX_TIMEOUT);
    if(DMXSerial.noDataSince() < DMX_TIMEOUT) {
      showMessage(AnotherMasterFound);
    }
    else
    {
      showMessage(NoMessage);
    }
    DMXSerial.init(DMXController, DMX_MODE_PIN);
  }
  
  setupInput();
  setUpRGB();
}

void loop() {
  unsigned long currentTime = millis();
  processInput(currentTime);
  
  if(sData.processNewValue)
  {
    switch(menus[sData.pageIndex].id)
    {
      case RotateScreen:
        oled.clear();
        oled.displayRemap(!eData.rotateScreen);
        sData.updateScreen = true;
        break;
      case Reboot:
        rebootFunc();
        break;
      case FactoryConfirm:
        initEData(&eData);
        putUpdate(&eData, sizeof(EEPROM_Data));
        rebootFunc();
        break;
      case Master:
      case EnableAuto:
        if(eData.enableAuto && eData.isMaster) {
          if(sData.dmxInputDetected) {
            showMessage(AnotherMasterFound);
          }
          DMXSerial.init(DMXController, DMX_MODE_PIN);
        } else {
          DMXSerial.init(DMXReceiver, DMX_MODE_PIN);
        }
        break;
      case MicCalibrate:
        showMessage(MicCalibration);
        eData.micLowPass = autoLowPass();
        sData.calibrateMic = false;
        putUpdate(&eData, sizeof(EEPROM_Data));
        showMessage(MicCalibrationDone);
        break;
      default:
        break;
    }
    putUpdate(&eData, sizeof(EEPROM_Data));
    sData.processNewValue = false;
  }
  
  doOutput(currentTime);
  processRGB(currentTime);
}

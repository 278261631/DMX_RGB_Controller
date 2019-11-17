#include <Wire.h>
#include <SSD1306Ascii.h>

#define OLED_COMMAND_MODE       0x00
#define OLED_ONE_COMMAND_MODE   0x80
#define OLED_DATA_MODE          0x40
#define OLED_ONE_DATA_MODE      0xC0

#define BACKL_TOUT 60000       // таймаут неактивности отключения дисплея, секунды
#define CONTRAST 150        // контрастность (яркость) дисплея 0-255
#define I2C_ADDRESS 0x3C    // адрес дисплея

void PlotPoint (int x, int y, byte val) {
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(OLED_COMMAND_MODE);
  Wire.write(0x21); Wire.write(x); Wire.write(x);  // Column range
  Wire.write(0x22); Wire.write(y); Wire.write(y);  // Page range
  Wire.endTransmission();
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(OLED_ONE_DATA_MODE);
  Wire.write(val);
  Wire.endTransmission();
}

#define LOGO_SIZE 112
static const byte logo_Mem[LOGO_SIZE] PROGMEM = {
  0x0, 0xD0, 0x0, 0x70, 0x0, 0xB8, 0x1, 0x78, 
  0x80, 0x7B, 0xFD, 0xBB, 0xC0, 0xF7, 0x0, 0xD8, 
  0xE0, 0xEF, 0x0, 0x60, 0xE0, 0x61, 0x0, 0x4, 
  0xE0, 0xE, 0x0, 0x24, 0x40, 0x17, 0xE8, 0x12, 
  0x40, 0xE7, 0xED, 0x10, 0x0, 0x0, 0xEC, 0x9, 
  0x0, 0x1, 0xDA, 0xB, 0x0, 0x1, 0xB6, 0x9, 
  0x80, 0xE8, 0x75, 0x5, 0x80, 0xDC, 0xFB, 0x4, 
  0x40, 0x3E, 0xE0, 0x4, 0x40, 0xFE, 0x7, 0x2, 
  0x20, 0xFE, 0xF, 0x2, 0x20, 0xF0, 0xF, 0x2, 
  0x10, 0xEE, 0xF, 0x1, 0x8, 0xDC, 0x87, 0x0, 
  0x0, 0xDD, 0xB, 0x0, 0x36, 0xBD, 0xD3, 0x3, 
  0xB7, 0xB8, 0xC3, 0x3, 0x77, 0x0, 0xA0, 0x3, 
  0x6F, 0xFF, 0x77, 0x4, 0x5F, 0x0, 0xF0, 0x7, 
  0x3E, 0x0, 0xE0, 0x7, 0x3E, 0x0, 0xE0, 0x3 };
  
void drawLogo()
{
  for(int i = 0; i < 28; ++i)
  {
    for(int j = 0; j < 4; ++j)
    {
      PlotPoint(i, j, pgm_read_byte(&logo_Mem[j + i * 4]));
    }
    delay(20);
  }
}

void oledClear()
{
  for(int i = 0; i < 128; ++i)
  {
    for(int j = 0; j < 4; ++j)
    {
      PlotPoint(i, j, 0);
    }
  }
}

void showLogo()
{
  oled.setFont(Callibri14);
  
  oledClear();
  oled.clear();
  oled.setCursor(50, 0);
  String s = "struktura";
  for(int i = 0; i < s.length(); ++i) {
    oled.print(s[i]);
    delay(50);
  }
  oled.println();
  oled.setCursor(50, oled.row());
  s = "rave";
  for(int i = 0; i < s.length(); ++i) {
    oled.print(s[i]);
    delay(50);
  }
  oled.println();
  drawLogo();
  delay(1000);

  oledClear();
  oled.clear();
  oled.setCursor(50, 0);
  s = "powered by";
  for(int i = 0; i < s.length(); ++i) {
    oled.print(s[i]);
    delay(50);
  }
  oled.println();
  oled.setCursor(50, oled.row());
  s = "igemon";
  for(int i = 0; i < s.length(); ++i) {
    oled.print(s[i]);
    delay(50);
  }
  oled.println();
  drawLogo();
  delay(1000);
  
  oledClear();
  oled.clear();
}

void setupOutput()
{
  delay(1500);
  Wire.begin();
  Wire.setClock(400000L);
  oled.begin(&Adafruit128x32, I2C_ADDRESS);
  oled.setContrast(CONTRAST);

  if (!eData.rotateScreen) {
    oled.ssd1306WriteCmd(SSD1306_SEGREMAP);
    oled.ssd1306WriteCmd(SSD1306_COMSCANINC);
  }

  if(eData.showLogo) {
    showLogo();
  }

  oled.setFont(CalLite24);
}

void inline valueOut()
{
  if(!menus[sData.pageIndex].value) {
    return;
  }

  if(menus[sData.pageIndex].id == DMXAddr && !sData.dmxInputDetected) {
    oled.print("! ");
  }
  
  const ValUnion *val = reinterpret_cast<const ValUnion *>(menus[sData.pageIndex].value);
    if(!sData.editMode) {
      switch(menus[sData.pageIndex].valueType)
      {
        case Bool:
          oled.print(val -> boolValue ? "v" : "x");
          break;
        case UInt:
          oled.print(val -> uIntValue);
          break;
        case UChar:
          oled.print(val -> uCharValue);
          break;
        default:
          break;
      }
    }
    else if(sData.hideOnBlink) {
      switch(menus[sData.pageIndex].valueType) {
      case Bool:
          oled.print(sData.tmpVal ? "v" : "x");
          break;
        case UInt:
          oled.print(sData.tmpVal);
          break;
        case UChar:
          oled.print(sData.tmpVal);
          break;
        default:
          break;
      }
    }
}

void showMessage(unsigned char id)
{
  sData.messageId = id;
  if(id)
  {
    oled.clear();
    oled.setFont(Callibri14);
    switch(id) {
      case AnotherMasterFound:
        oled.println("Another DMX");
        oled.println("controller found");
        break;
      case LineCheck:
        oled.println("Checking line for");
        oled.println("another controllers");
        break;
      case MicCalibration:
        oled.println("Keep silence,");
        oled.println("please wait...");
        break;
      case MicCalibrationDone:
        oled.println("Mic calibration");
        oled.println("finished");
        break;
      default:
        break;
    }
  }
  else
  {
    oled.clear();
    oled.setFont(CalLite24);
    sData.updateScreen = true;
  }
}

void doOutput(unsigned long currentTime)
{
  if(sData.displayOn) {
    if(currentTime - sData.lastActionTime > BACKL_TOUT)
    {
      oled.clear();
      sData.updateScreen = true;
      sData.displayOn = false;
      return;
    }
  }
  else {
    return;
  }

  if(sData.messageId != NoMessage)
  {
    if(sData.updateScreen) {
      showMessage(sData.messageId);
      sData.updateScreen = false;
    }
    return;
  }

  if(sData.editMode && currentTime >= sData.valueBlinkTime) {
    sData.updateValue = true;
    sData.hideOnBlink = !sData.hideOnBlink;
    sData.valueBlinkTime = currentTime + BLINK_PERIOD;
  }

  if(sData.updateValue) {
    oled.setCursor(sData.menuNameEndCol, 0);
    oled.clearToEOL();

    valueOut();
    
    sData.updateValue = false;
  }

  if(sData.updateScreen)  {
    oled.clear();
    oled.print(menus[sData.pageIndex]._name);
    oled.print("   ");
    sData.menuNameEndCol = oled.col();
    
    valueOut();
    
    sData.updateScreen = false;
  }
}

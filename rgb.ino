//strobe values
unsigned long strobTimer = 0;
bool strobePhase = true;
int deltaOn = 0;
int deltaOff = 0;
unsigned char strobe_prev = 0;

float counterValue = 0;
float tickCounterValue = 0;
unsigned long counterTimer = 0;

#define R_PIN 9
#define G_PIN 10
#define B_PIN 11

#define MODE_STEP 25

//mic variables
float averageLevel = 0;
int maxLevel = 1000;
int currentLevel;
int lenght;
float soundLevel, soundLevel_f;
float averK = 0.05;
float maxK = 0.006;
float SMOOTH = 0.8;               // коэффициент плавности анимации VU (по умолчанию 0.5)
#define MAX_COEF 2.1              // коэффициент громкости (максимальное равно срднему * этот коэф) (по умолчанию 1.8)
#define EXP 1.4                   // степень усиления сигнала (для более "резкой" работы) (по умолчанию 1.4)
#define LOW_PASS_ADD 10  
#define TICK_DECAY 100
unsigned long tickTimer = 0;
bool hasTick = false;
unsigned long tickSpeed = 0;

#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

void setUpRGB()
{
  pinMode(R_PIN, OUTPUT);
  pinMode(G_PIN, OUTPUT);
  pinMode(B_PIN, OUTPUT);

  pinMode(A7, INPUT);
  analogReference(EXTERNAL);

  sbi(ADCSRA, ADPS2);
  cbi(ADCSRA, ADPS1);
  sbi(ADCSRA, ADPS0);
}

uint16_t autoLowPass() {
  delay(100);
  int thisMax = 0;                          
  int thisLevel;
  for (byte i = 0; i < 200; i++) {
    thisLevel = analogRead(A7);
    if (thisLevel > thisMax)   
    {
      thisMax = thisLevel;                  
    }
    delay(10);                              
  }
  return thisMax + LOW_PASS_ADD;        
}

void loadDmxValues(struct DMX_CHANNELS *chans)
{
  if (sData.dmxInputDetected) {
      unsigned char *ptr = reinterpret_cast<unsigned char *>(chans);
      for(int i = 0; i < sizeof(DMX_CHANNELS); ++i)
      {
        *ptr = DMXSerial.read(eData.dmxChan + i);
        ++ptr;
      }
    } else {
      memset(chans, 0, sizeof(DMX_CHANNELS));
    }
}

void sendDmxValues(byte r, byte g, byte b, byte br, byte strobe, int addr) {
  struct DMX_CHANNELS chans;
  memset(&chans, 0, sizeof(DMX_CHANNELS));
  chans.chan_r = r;
  chans.chan_g = g;
  chans.chan_b = b;
  chans.chan_br = br;
  chans.chan_strobe = strobe;
  unsigned char *ptr = reinterpret_cast<unsigned char *>(&chans);
  for(int i = 0; i < sizeof(DMX_CHANNELS); ++i)
  {
        DMXSerial.write(addr + i, *ptr);
        ++ptr;
 }
}

void checkDmxSignal() {
  bool val = (eData.enableAuto && eData.isMaster) ? true : (DMXSerial.noDataSince() < DMX_TIMEOUT);
  if(val != sData.dmxInputDetected && menus[sData.pageIndex].id == DMXAddr)
  {
    sData.updateValue = true;
  }
  sData.dmxInputDetected = val;
}

inline void setRAWLevels(const unsigned char &r, const unsigned char &g, const unsigned char &b, const unsigned char &br, const unsigned char &strobe, const unsigned long &currentTime) {
  unsigned char rOut = 0;
  unsigned char gOut = 0;
  unsigned char bOut = 0;

  if(strobe != strobe_prev)
  {
    if(strobe)
    {
      deltaOff = 12750 / (strobe + 11);
      deltaOn = deltaOff > 150 ? 150 : deltaOff;
   
      if(!strobe_prev)
      {
        strobTimer = currentTime;
        strobePhase = true;
      }
    }
    strobe_prev = strobe;
  }

  if(currentTime >= strobTimer)
  {
    strobePhase = !strobePhase;
    strobTimer = currentTime + (strobePhase ? deltaOn : deltaOff);
  }
    
  if(!strobe || strobePhase)
  {
    rOut = (r * br) >> 8;
    gOut = (g * br) >> 8;
    bOut = (b * br) >> 8;
  }

  analogWrite(R_PIN, rOut);
  analogWrite(G_PIN, gOut);
  analogWrite(B_PIN, bOut);
}

void colorWheel(int color, byte &_r, byte &_g, byte &_b) {
  if (color <= 255) {           // красный макс, зелёный растёт
    _r = 255;
    _g = color;
    _b = 0;
  }
  else if (color > 255 && color <= 510) {   // зелёный макс, падает красный 
    _r = 510 - color;
    _g = 255;
    _b = 0;
  }
  else if (color > 510 && color <= 765) {   // зелёный макс, растёт синий
    _r = 0;
    _g = 255;
    _b = color - 510;
  }
  else if (color > 765 && color <= 1020) {  // синий макс, падает зелёный
    _r = 0;
    _g = 1020 - color;
    _b = 255;
  }
  else if (color > 1020 && color <= 1275) {   // синий макс, растёт красный
    _r = color - 1020;
    _g = 0;
    _b = 255;
  }
  else if (color > 1275 && color <= 1530) { // красный макс, падает синий
    _r = 255;
    _g = 0;
    _b = 1530 - color;
  }
}

unsigned long getCounter(byte counterSpeed, byte counterStep, unsigned long currentTime)
{
  if(currentTime > counterTimer + map(counterStep, 0, 255, 10, 10000))
  {
    unsigned long delta = currentTime - counterTimer;
    counterValue += 0.005 * counterSpeed  * delta;
    counterTimer = currentTime;
  }
  return counterValue;
}

unsigned long getTickCounter()
{
  if(hasTick)
  {
    tickCounterValue += 100 * tickSpeed * tickSpeed;
  }
  return tickCounterValue;
}

void getMicLevel(unsigned long currentTime) 
{
  hasTick = false;
  soundLevel = 0;

  for (byte i = 0; i < 100; i ++) 
  {
    currentLevel = analogRead(A7);                            
    if (soundLevel < currentLevel) 
    {
      soundLevel = currentLevel;   
    }
  }

  soundLevel = map(soundLevel, eData.micLowPass, 1023, 0, 500);
  soundLevel = constrain(soundLevel, 0, 500);
  soundLevel = pow(soundLevel, EXP);
  soundLevel_f = soundLevel * SMOOTH + soundLevel_f * (1 - SMOOTH);

  if(soundLevel_f / averageLevel > 3 && currentTime > tickTimer + TICK_DECAY && averageLevel > 10)
  {
    hasTick = true;
    tickSpeed = 2000 / (currentTime - tickTimer);
    tickTimer = currentTime;
  }
  else if(currentTime - tickTimer > 2000)
  {
    tickSpeed = 0;
  }

  averageLevel = soundLevel_f * averK + averageLevel * (1 - averK);
  maxLevel = soundLevel_f * maxK + maxLevel * (1 - maxK);
}

void processAutoDMX(byte r, byte g, byte b, byte br, byte strobe, byte mode, byte modeSpeed, byte modeStep, byte micMode, unsigned long currentTime)
{
  unsigned int modeN = mode / MODE_STEP;
  bool useTicks = false;
  if((micMode > 50 && micMode < 100) || micMode > 150)
  {
    useTicks = true;
  }
  
  switch(modeN)
  {
    case 0:
      colorWheel((useTicks ? getTickCounter() : getCounter(modeSpeed, modeStep, currentTime)) % 1531, r, g, b);
      setRAWLevels(r, g, b, br, strobe, currentTime);
      break;
    default:
      setRAWLevels(0, 0, 0, 0, 0, currentTime);
      break;
  }
}

void processStandaloneAuto(unsigned long currentTime)
{
  switch(eData.autoPreset)
  {
    case 1:
      {
        unsigned int counter = eData.micBeats ? getTickCounter() : getCounter(eData.speed, eData.step, currentTime);
        byte r, g, b;
        byte br = eData.bright;
        if(eData.micBright)
        {
          br = map(soundLevel_f, 0, maxLevel * MAX_COEF, 0.1 * br, br);
        }
        colorWheel(counter % 1531, r, g, b);
        setRAWLevels(r, g, b, br, 0, currentTime);

        if(eData.isMaster) 
        {
          sendDmxValues(r, g, b, br, 0, eData.dmxChan);
          for(int i = 1; i <= eData.slavesCount; ++i) 
          {
            colorWheel((counter + i * 1531 / eData.slavesCount) % 1531, r, g, b);
            sendDmxValues(r, g, b, br, 0, eData.dmxChan + i * CHANNELS_COUNT);
          }
        }
      }
      break;
    default:
      setRAWLevels(0, 0, 0, 0, 0, currentTime);
      if(eData.isMaster) 
      {
        sendDmxValues(0, 0, 0, 0, 0, eData.dmxChan);
        for(int i = 1; i <= eData.slavesCount; ++i) 
        {
          sendDmxValues(0, 0, 0, 0, 0, eData.dmxChan + i * CHANNELS_COUNT);
        }
      }
      break;
  }
}

void processRGB(unsigned long currentTime) {
  checkDmxSignal();
  getMicLevel(currentTime);
  
  if(!eData.enableAuto)  {
    DMX_CHANNELS chans;
    loadDmxValues(&chans);

    if(chans.chan_micMode > 100)
    {
      chans.chan_br = map(soundLevel_f, 0, maxLevel * MAX_COEF, 0.1 * chans.chan_br, chans.chan_br);
      //chans.chan_br *= 1.0 + soundLevel_f / (1.0 + maxLevel * MAX_COEF);
    }
    
    if(!chans.chan_auto) {
      setRAWLevels(chans.chan_r, chans.chan_g, chans.chan_b, chans.chan_br, chans.chan_strobe, currentTime);
    }
    else {
      processAutoDMX(chans.chan_r, chans.chan_g, chans.chan_b, chans.chan_br, chans.chan_strobe, chans.chan_auto, chans.chan_autoSpeed, chans.chan_autoStep, chans.chan_micMode, currentTime);
    }
  }
  else
  {
    processStandaloneAuto(currentTime);
  }
}

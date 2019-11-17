#define pinA  2 //Пин прерывания
#define pinButton 3 //Пин прерывания нажатия кнопки
#define pinB  4 //Любой другой пин

#define timeButtonPressed 1500 // Долгое удержание кнопки после 1,5 секунд

volatile char state = 0;       // Переменная хранящая статус вращения

// Переменные хранящие состояние действия до его выполнения
volatile bool flagCW            = false;     // Было ли вращение по часовой стрелке
volatile bool flagCCW           = false;     // Было ли вращение против часовой стрелки
volatile bool flagButton        = false;     // Было ли нажатие кнопки
volatile bool flagButtonLong    = false;     // Было ли долгое удержание кнопки

volatile long timeButtonDown    = 0;         // Переменная хранящая время нажатия кнопки
volatile bool isButtonDown      = false;     // Переменная хранящая время нажатия кнопки
volatile bool longPressReleased = false;     // Переменная для фиксации срабатывания долгого нажатия

void setupInput()
{
  pinMode(pinA, INPUT);           // Пины в режим приема INPUT
  pinMode(pinB, INPUT);           // Пины в режим приема INPUT
  pinMode(pinButton, INPUT);      // Пины в режим приема INPUT

  attachInterrupt(0, A, CHANGE);  // Настраиваем обработчик прерываний по изменению сигнала
  attachInterrupt(1, Button, CHANGE);   // Настраиваем обработчик прерываний по изменению сигнала нажатия кнопки
}

inline void stepInto()
{
  for(int i = 0; i < menuArraySize; ++i)
  {
    if(menus[i].parentid == menus[sData.pageIndex].id)
    {
      sData.pageIndex = i;
      break;
    }
  }
}

inline void stepOut()
{
  for(int i = 0; i < menuArraySize; ++i)
  {
    if(menus[i].id == menus[sData.pageIndex].parentid)
    {
      sData.pageIndex = i;
      break;
    }
  }
}

inline void stepNext()
{
  for(int i = sData.pageIndex + 1; i < menuArraySize; ++i)
  {
    if(menus[i].parentid == menus[sData.pageIndex].parentid)
    {
      sData.pageIndex = i;
      return;
    }
  }

  for(int i = 0; i < sData.pageIndex; ++i)
  {
    if(menus[i].parentid == menus[sData.pageIndex].parentid)
    {
      sData.pageIndex = i;
      return;
    }
  }
}

inline void stepPrev()
{
  for(int i = sData.pageIndex - 1; i >= 0; --i)
  {
    if(menus[i].parentid == menus[sData.pageIndex].parentid)
    {
      sData.pageIndex = i;
      return;
    }
  }

  for(int i = menuArraySize - 1; i >= sData.pageIndex; --i)
  {
    if(menus[i].parentid == menus[sData.pageIndex].parentid)
    {
      sData.pageIndex = i;
      return;
    }
  }
}

inline void saveTmpVal()
{
  ValUnion *val = reinterpret_cast<ValUnion *>(menus[sData.pageIndex].value);
  switch(menus[sData.pageIndex].valueType)
  {
      case Bool:
        if(val -> boolValue != sData.tmpVal)
        {
          val -> boolValue = sData.tmpVal;
          sData.processNewValue = true;
        }
        break;
      case UInt:
        if(val -> uIntValue != sData.tmpVal)
        {
          val -> uIntValue = sData.tmpVal;
          sData.processNewValue = true;
        }
        break;
      case UChar:
        if(val -> uCharValue != sData.tmpVal)
        {
          val -> uCharValue = sData.tmpVal;
          sData.processNewValue = true;
        }
        break;
      default:
        break;
  }
}

inline void loadTmpVal()
{
  const ValUnion *val = reinterpret_cast<const ValUnion *>(menus[sData.pageIndex].value);
  switch(menus[sData.pageIndex].valueType)
  {
      case Bool:
        sData.tmpVal = val -> boolValue;
        break;
      case UInt:
        sData.tmpVal = val -> uIntValue;
        break;
      case UChar:
        sData.tmpVal = val -> uCharValue;
        break;
      default:
        sData.tmpVal = -1;
        break;
  }
}

inline void incTmpVal()
{
  ++sData.tmpVal;
  if(sData.tmpVal > menus[sData.pageIndex]._max) {
    sData.tmpVal = menus[sData.pageIndex]._min;
  }
}

inline void decTmpVal()
{
  --sData.tmpVal;
  if(sData.tmpVal < menus[sData.pageIndex]._min) {
    sData.tmpVal = menus[sData.pageIndex]._max;
  }
}

void processInput(unsigned long currentTime)
{
  if (isButtonDown && currentTime - timeButtonDown > timeButtonPressed) { // Время длительного удержания наступило
    flagButtonLong = true;
  }

  if (flagCW) {               // Шаг вращения по часовой стрелке
    if(sData.displayOn && sData.messageId == NoMessage) {
      if(sData.editMode) {
        incTmpVal();
        sData.hideOnBlink = false;
        sData.valueBlinkTime = currentTime;
        sData.updateValue = true;
      }
      else {
        stepNext();
        sData.updateScreen = true;
      }
    }
    else if(!sData.displayOn)
    {
      sData.displayOn = true;
    }
    else {
      showMessage(NoMessage);
    }
    sData.lastActionTime = currentTime;
    flagCW = false;           
  }
  if (flagCCW) {              // Шаг вращения против часовой стрелки
    if(sData.displayOn && sData.messageId == NoMessage) {
      if(sData.editMode) {
        decTmpVal();
        sData.hideOnBlink = false;
        sData.valueBlinkTime = currentTime;
        sData.updateValue = true;
      }
      else {
        stepPrev();
        sData.updateScreen = true;
      }
    }
    else if(!sData.displayOn)
    {
      sData.displayOn = true;
    }
    else {
      showMessage(NoMessage);
    }
    sData.lastActionTime = currentTime;
    flagCCW = false;          
  }
  
  if (flagButton) {           // Кнопка нажата
    if(sData.displayOn && sData.messageId == NoMessage) {
      if(menus[sData.pageIndex].valueType == Menu) {
        stepInto();
        sData.updateScreen = true;
      }
      else if(sData.editMode) {
        saveTmpVal();
        sData.updateValue = true;
        sData.editMode = false;
      }
      else
      {
        if(menus[sData.pageIndex].value) {
          loadTmpVal();
          sData.editMode = true;
          sData.hideOnBlink = true;
          sData.updateValue = true;
        }
        sData.valueBlinkTime = currentTime;
      }
    }
    else if(!sData.displayOn)
    {
      sData.displayOn = true;
    }
    else {
      showMessage(NoMessage);
    }
    sData.lastActionTime = currentTime;
    flagButton = false;       
  }
  
  if (flagButtonLong && isButtonDown) {   // Кнопка удерживается
    if(sData.displayOn  && sData.messageId == NoMessage) {
      if(sData.editMode) {
        sData.updateValue = true;
        sData.editMode = false;
      }
      else
      {
        stepOut();
        sData.updateScreen = true;
      }
    }
    else if(!sData.displayOn)
    {
      sData.displayOn = true;
    }
    else {
      showMessage(NoMessage);
    }
    sData.lastActionTime = currentTime;
    isButtonDown = false;                 
    flagButtonLong = false;               
  }
}

void A()
{
  int pinAValue = bitRead(PIND, pinA);        // Получаем состояние пинов A и B
  int pinBValue = bitRead(PIND, pinB);

  cli();                                    // Запрещаем обработку прерываний, чтобы не отвлекаться
  if (!pinAValue &&  pinBValue) state = 1;  // Если при спаде линии А на линии B лог. единица, то вращение в одну сторону
  if (!pinAValue && !pinBValue) state = -1; // Если при спаде линии А на линии B лог. ноль, то вращение в другую сторону
  if (pinAValue && state != 0) {
    if (state == 1 && !pinBValue || state == -1 && pinBValue) { // Если на линии А снова единица, фиксируем шаг
      if (state == 1)   flagCW = true;      // Флаг вращения по часовой стрелке
      if (state == -1) flagCCW = true;      // Флаг вращения против часовой стрелки
      state = 0;
    }
  }
  sei();                                    // Разрешаем обработку прерываний
}

void Button()
{
  int pinButValue = bitRead(PIND, pinButton);   // Получаем состояние пина кнопки

  cli();                                      // Запрещаем обработку прерываний, чтобы не отвлекаться
  timeButtonDown = millis();                  // Запоминаем время нажатия/отжатия

  if (!pinButValue) {                         // При нажатии подается инвертированный сигнал
    isButtonDown = true;                      // Устанавливаем флаг нажатия кнопки
  }
  else if (isButtonDown) {                    // Если кнопка отжата, смотрим не было ли выполнено действие
    if (!longPressReleased) {                 // Если долгое нажатие не было ни разу отработано, то...
      flagButton = true;                      // Если не было удержания, ставим флаг события обычного нажатия
    }
    isButtonDown = false;                     // Сбрасываем флаг нажатия
    longPressReleased = false;                // Сбрасываем флаг длительного удержания
  }
  sei();                                      // Разрешаем обработку прерываний
}

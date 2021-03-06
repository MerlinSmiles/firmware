#include <Wire.h>
#include "DS3231.h"
#include <IRremote.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#include <NewTone.h>

// Pins definition.
const unsigned char pinSDI = A0;
const unsigned char pinCLK = A1;
const unsigned char pinLE = 6;
const unsigned char pinBLNK = 7;

const unsigned char pinHV_Brightness = 1;

const uint8_t anode0 = 2;
const uint8_t anode1 = 3;
const uint8_t anode2 = 4;

const unsigned char pinPirSensor = A3;

const unsigned char pinBuzzer = A2;
const unsigned char pinDot = 10;

const unsigned char pinButton = 5;
const unsigned char pinEncoderA = 6;
const unsigned char pinEncoderB = 7;

const unsigned char pinRGB = 11;
const unsigned char pin12VSwitch = 8;

const unsigned char pinIR = 9;

const unsigned int spinningTime = 3;
const int iterationsDimmDigits = 35;

int sensorTime = 1;
long iterationSensorStep = 5000; // 5000 ~ approximately 1 minute.
long iterationSensor = sensorTime * iterationSensorStep;

int slotMachineFrequency = 0;
const int slotMachineFrequencyMAX = 5;

const int sensorTimeMAX = 5;

// Constants and defines.
#define NUMBER_MAX 100
#define PIXELS     6

#define IR_24_KEY
// else IR_7_KEY

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(PIXELS, pinRGB, NEO_GRB + NEO_KHZ800);

const float brightnessStep = 5;
const float brightnessMAX = 50;
unsigned char brightnessR = 0;
unsigned char brightnessG = 0;
unsigned char brightnessB = 0;

bool beepOnHour = false;
bool beeped = false;

bool powerON = true;

bool pressed = false;
bool HoursMode12 = false;

bool showDate = true;
bool sensorActivated = true;

int tubeBrightness = 1;
const int tubeBrightnessStep = 10;
const int tubeBrightnessMAX = 25;

bool sleep = false;
bool countDownMode = false;

bool activateAnimation = false;

int iterationsInMenu = 0;
int iterationsButtonPressed = 0;
int iterationsShowed = 0;

unsigned char clockMode = 0;
unsigned char encoder_A_prev = 0;
unsigned char menu = 0;

int sleepHourStart = 1;
int sleepHourEnd = 6;

bool fireAlarm = false;
bool alarmModeEnabled = false;
int alarmHour = 7;
int alarmMinute = 30;

DS3231 rtc;
DateTime now;

IRrecv irrcv(pinIR);
decode_results res;

enum DotMode
{
    Blink = 0,
    Permanent,
    Off,
    MODE_MAX
};

enum DateFormat
{
    DDMMYY = 0,
    MMDDYY,
    YYMMDD,
    MAX,
};

enum ScrollMode
{
    NONE = 0,
    CHANGING,
    // POPULATE,
    UPDATE,
    SCROLL_MAX
};

int currentFormat = MMDDYY;
int scrollMode = CHANGING;

enum Menu
{
    MENU_NONE = 0,

    BacklightRed = 1,
    BacklightGreen = 2,
    BacklightBlue = 3,

    AlarmMode = 4,
    AlarmHour = 5,
    AlarmMinute = 6,

    DotSetup = 7,
    BeepSetup = 8,

    SlotMachine,
    SpinChangingNumbers,
    AnimateColorsMode,

    HourModeSetup,
    DateMode,

    HoursSetup,
    MinutesSetup,
    SecondsSetup,

    MonthSetup,
    DaySetup,
    YearSetup,

    SleepStart,
    SleepEnd,

    ShowDate,
    MotionSensorTime,
    ActivateSensor,

    MENU_MAX,
    TubeBrightness,
};

void Beep(int size)
{
    NewTone(pinBuzzer, 4000, 50);
}

bool MenuPressed()
{
    return ((PIND & (1 << pinButton)) == 0);
}

void SetBackgroundColor(unsigned char red, unsigned char green, unsigned char blue)
{
    pixels.clear();

    for (int i = 0; i < PIXELS; ++i)
    {
        pixels.setPixelColor(i, red, green, blue);
    }

    pixels.show();
}

void SaveRGBColors(unsigned char red, unsigned char green, unsigned char blue)
{
    brightnessR = red;
    brightnessG = green;
    brightnessB = blue;

    SetBackgroundColor(red, green, blue);

    EEPROM.write(BacklightBlue, blue);
    EEPROM.write(BacklightGreen, green);
    EEPROM.write(BacklightRed, red);
}

void RestoreBacklight()
{
    digitalWrite(pin12VSwitch, HIGH);

    if (!activateAnimation)
    {
        SetBackgroundColor(brightnessR, brightnessG, brightnessB);
    }
}

void AnimateColors()
{
    const int seconds = now.second();
    const int brightness = (seconds % 10) * 8;
    const int negativeBrightness = (10 - (seconds % 10)) * 8;

    if (seconds >= 0 && seconds < 20)
    {
        SetBackgroundColor((seconds >= 10) ? negativeBrightness : brightness, 0, 0);
    }
    else if (seconds >= 20 && seconds < 40)
    {
        SetBackgroundColor(0, 0, (seconds >= 30) ? negativeBrightness : brightness);
    }
    else
    {
        SetBackgroundColor(0, (seconds >= 50) ? negativeBrightness : brightness, 0);
    }
}

void shift5812PJ(uint8_t dataByte)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        const bool bit = !!(dataByte & (1 << (7 - i)));

        if (bit)
        {
            PORTC |= 1 << 0; // digitalWrite(pinSDI, HIGH);
        }
        else
        {
            PORTC &= ~(1 << 0); // digitalWrite(pinSDI, LOW);
        }

        PORTC |= 1 << 1; // digitalWrite(pinCLK, HIGH);
        PORTC &= ~(1 << 1); // digitalWrite(pinCLK, LOW);
    }
}

unsigned int writeTwoNumbers(unsigned char left, unsigned char right, unsigned char anode)
{
    unsigned char byte1 = 0xFF;
    unsigned char byte2 = 0xFF;
    unsigned char byte3 = 0xFF;

    if (left > 0 && left <= 8)
    {
        byte1 &= ~(1 << (left - 1));
    }
    else if (left == 0)
    {
        byte2 &= ~(1 << 1);
    }
    else if (left == 9)
    {
        byte2 &= ~(1 << 0);
    }

    if (right > 0 && right <= 6)
    {
        byte2 &= ~(1 << (right + 1));
    }
    else if (right > 6 && right <= 9)
    {
        byte3 &= ~(1 << (right - 7));
    }
    else if (right == 0)
    {
        byte3 &= ~(1 << 3);
    }

    PORTD &= ~(1 << 6); // digitalWrite(pinLE, LOW);

    shift5812PJ(byte3);
    shift5812PJ(byte2);
    shift5812PJ(byte1);

    PORTD |= 1 << 6; // digitalWrite(pinLE, HIGH);

    PORTD |= 1 << anode; //digitalWrite(anode, HIGH);
    delay(1);
    PORTD &= ~(1 << anode); //digitalWrite(anode, LOW);
    delay(1);
}

void DisplayNumbers(unsigned char number1, unsigned char number2, unsigned char number3, unsigned char number4, unsigned char number5, unsigned char number6)
{
    writeTwoNumbers(number1, number4, anode0);
    writeTwoNumbers(number2, number5, anode1);
    writeTwoNumbers(number3, number6, anode2);
}

void DisplayThreeNumbers(const uint8_t one, const uint8_t two, const uint8_t three, ScrollMode mode = NONE)
{
    // Get the high and low order values for three pairs.
    unsigned char lowerFirst = one % 10;
    unsigned char upperFirst = one / 10;
    unsigned char lowerSecond = two % 10;
    unsigned char upperSecond = two / 10;
    unsigned char lowerThird = three % 10;
    unsigned char upperThird = three / 10;

    if (one >= 100)
    {
        upperFirst = lowerFirst = 10;
    }
    if (two >= 100)
    {
        upperSecond = lowerSecond = 10;
    }
    if (three >= 100)
    {
        upperThird = lowerThird = 10;
    }

    //SetDot();
    static unsigned char s_upperFirst = 0,
        s_lowerFirst = 0,
        s_upperSecond = 0,
        s_lowerSecond = 0,
        s_upperThird = 0,
        s_lowerThird = 0;

    switch (mode)
    {
    case CHANGING:
    {
        if (s_lowerThird != lowerThird)
        {
            for (int i = 0; i < 10; ++i)
            {
                int numbers[6] = {
                  (s_upperFirst != upperFirst) ? ((upperFirst + i) % 10) : upperFirst,
                  (s_lowerFirst != lowerFirst) ? ((lowerFirst + i) % 10) : lowerFirst,
                  (s_upperSecond != upperSecond) ? ((upperSecond + i) % 10) : upperSecond,
                  (s_lowerSecond != lowerSecond) ? ((lowerSecond + i) % 10) : lowerSecond,
                  (s_upperThird != upperThird) ? ((upperThird + i) % 10) : upperThird,
                  (s_lowerThird != lowerThird) ? ((lowerThird + i) % 10) : lowerThird
                };

                for (int j = 0; j < spinningTime; ++j)
                {
                    DisplayNumbers(numbers[0], numbers[1], numbers[2], numbers[3], numbers[4], numbers[5]);
                    delayMicroseconds(1100);
                }
            }
        }
        break;
    }
    case UPDATE:
    {
        if (s_lowerThird != lowerThird)
        {
            for (int k = 0; k < 6; ++k)
            {
                for (int i = 0; i < 10; ++i)
                {
                    for (unsigned char j = 0; j < 2; ++j)
                    {
                        DisplayNumbers(
                            (k == 0) ? ((upperFirst + i) % 10) : upperFirst,
                            (k == 1) ? ((lowerFirst + i) % 10) : lowerFirst,
                            (k == 2) ? ((upperSecond + i) % 10) : upperSecond,
                            (k == 3) ? ((lowerSecond + i) % 10) : lowerSecond,
                            (k == 4) ? ((upperThird + i) % 10) : upperThird,
                            (k == 5) ? ((lowerThird + i) % 10) : lowerThird
                            );
                        delayMicroseconds(1000);
                    }
                    if (MenuPressed())
                        break;
                }
                if (MenuPressed())
                    break;
            }
        }

        break;
    }
    }
    DisplayNumbers(upperFirst, lowerFirst, upperSecond, lowerSecond, upperThird, lowerThird);

    s_upperFirst = upperFirst;
    s_lowerFirst = lowerFirst;
    s_upperSecond = upperSecond;
    s_lowerSecond = lowerSecond;
    s_upperThird = upperThird;
    s_lowerThird = lowerThird;
}

void SpinAllNumbers(unsigned char spinTimes = 5)
{
    for (unsigned char i = 0; i < 10; i++)
    {
        for (unsigned char j = 0; j < spinTimes; ++j)
        {
            DisplayNumbers(i, i, i, i, i, i);
        }
    }
}

void TestColorChanels()
{
    const int maxBrightness = 80;
    for (int i = 0; i < maxBrightness; ++i)
    {
        SetBackgroundColor(i, 0, 0);
        delay(5);
    }
    for (int i = maxBrightness; i > 0; --i)
    {
        SetBackgroundColor(i, 0, 0);
        delay(5);
    }

    for (int i = 0; i < maxBrightness; ++i)
    {
        SetBackgroundColor(0, i, 0);
        delay(5);
    }
    for (int i = maxBrightness; i > 0; --i)
    {
        SetBackgroundColor(0, i, 0);
        delay(5);
    }

    for (int i = 0; i < maxBrightness; ++i)
    {
        SetBackgroundColor(0, 0, i);
        delay(5);
    }
    for (int i = maxBrightness; i > 0; --i)
    {
        SetBackgroundColor(0, 0, i);
        delay(5);
    }
}

void RunSelfTesting()
{
    TestColorChanels();

    SpinAllNumbers(60);
}

void ReadSettings()
{
    brightnessR = EEPROM.read(BacklightRed);
    brightnessB = EEPROM.read(BacklightBlue);
    brightnessG = EEPROM.read(BacklightGreen);

    clockMode = EEPROM.read(DotSetup);

    if (clockMode >= MODE_MAX)
    {
        clockMode = Blink;
    }

    beepOnHour = EEPROM.read(BeepSetup);

    tubeBrightness = EEPROM.read(TubeBrightness);

    if (tubeBrightness > tubeBrightnessMAX)
    {
        tubeBrightness = tubeBrightnessMAX;
    }

    analogWrite(pinHV_Brightness, tubeBrightness * tubeBrightnessStep);

    slotMachineFrequency = EEPROM.read(SlotMachine);

    if (slotMachineFrequency > slotMachineFrequencyMAX)
    {
        slotMachineFrequency = 1;
    }

    sensorTime = EEPROM.read(MotionSensorTime);

    if (sensorTime > sensorTimeMAX)
    {
        sensorTime = 1;
    }


    scrollMode = EEPROM.read(SpinChangingNumbers);

    if (scrollMode >= SCROLL_MAX)
    {
        scrollMode = ScrollMode::CHANGING;
    }

    currentFormat = (DateFormat)EEPROM.read(DateMode);

    if (currentFormat > DateFormat::MAX)
    {
        currentFormat = DateFormat::MMDDYY;
    }

    activateAnimation = EEPROM.read(AnimateColorsMode);

    sensorActivated = EEPROM.read(ActivateSensor);
    HoursMode12 = EEPROM.read(HourModeSetup);
    showDate = EEPROM.read(ShowDate);
    sleepHourStart = EEPROM.read(SleepStart);
    sleepHourEnd = EEPROM.read(SleepEnd);

    if (sleepHourStart > 24)
    {
        sleepHourStart = 1;
    }

    if (sleepHourEnd > 24)
    {
        sleepHourEnd = 6;
    }

    alarmModeEnabled = EEPROM.read(AlarmMode);
    alarmHour = EEPROM.read(AlarmHour);
    alarmMinute = EEPROM.read(AlarmMinute);

    if (alarmHour > 24)
    {
        alarmHour = 7;
    }

    if (alarmMinute > 60)
    {
        alarmMinute = 30;
    }
}

void setup()
{
    // Shift register pins.
    pinMode(pinSDI, OUTPUT);
    pinMode(pinCLK, OUTPUT);
    pinMode(pinLE, OUTPUT);
    pinMode(pinBLNK, OUTPUT);
    pinMode(pin12VSwitch, OUTPUT);
    pinMode(pinHV_Brightness, OUTPUT);

    digitalWrite(pin12VSwitch, HIGH);
    digitalWrite(pinBLNK, LOW);

    // Anode pins.
    pinMode(anode0, OUTPUT);
    pinMode(anode1, OUTPUT);
    pinMode(anode2, OUTPUT);

    // Back light init.
    pixels.begin();

    pinMode(pinDot, OUTPUT);
    pinMode(pinBuzzer, OUTPUT);

    // Encoder setup. Two pins.
    DDRB &= ~(1 << pinEncoderA);
    DDRB &= ~(1 << pinEncoderB);
    PORTB |= (1 << pinEncoderA);  // turn on pull-up resistor
    PORTB |= (1 << pinEncoderB);  // turn on pull-up resistor

    // Button pin.
    DDRD &= ~(1 << pinButton);
    PORTD |= (1 << pinButton);  // turn on pull-up resistor

    Wire.begin();
    rtc.begin();

    pinMode(pinPirSensor, INPUT_PULLUP);

    ReadSettings();

    /// Start selftesting.
    RunSelfTesting();
    /// End selftesting.

    RestoreBacklight();

    // Start IR receiver.
    irrcv.enableIRIn();
    Beep(50);
}

void SetChanelBrightness(bool decrease, unsigned char& brightness)
{
    if (decrease)
    {
        if ((brightness + brightnessStep) <= brightnessMAX)
        {
            brightness += brightnessStep;
        }
    }
    else
    {
        if ((brightness - brightnessStep) >= 0)
        {
            brightness -= brightnessStep;
        }
    }

    SetBackgroundColor(brightnessR, brightnessG, brightnessB);
}

void ProcessEncoderChange(bool decrease)
{
    switch (menu)
    {
    case BacklightRed:
    {
        SetChanelBrightness(decrease, brightnessR);
        EEPROM.write(menu, brightnessR);
        break;
    }
    case BacklightBlue:
    {
        SetChanelBrightness(decrease, brightnessB);
        EEPROM.write(menu, brightnessB);
        break;
    }
    case BacklightGreen:
    {
        SetChanelBrightness(decrease, brightnessG);
        EEPROM.write(menu, brightnessG);
        break;
    }
    case DotSetup:
    {
        if (decrease)
        {
            if (++clockMode == MODE_MAX)
            {
                clockMode = 0;
            }
        }
        else
        {
            if (clockMode-- == 0)
            {
                clockMode = MODE_MAX - 1;
            }
        }
        EEPROM.write(menu, clockMode);
        break;
    }
    case BeepSetup:
    {
        beepOnHour = !beepOnHour;
        EEPROM.write(menu, beepOnHour);
        break;
    }
    case TubeBrightness:
    {
        tubeBrightness += (decrease ? 1 : -1);
        tubeBrightness = (tubeBrightness < 1 ? tubeBrightnessMAX : tubeBrightness);
        tubeBrightness = (tubeBrightness > tubeBrightnessMAX ? 1 : tubeBrightness);
        analogWrite(pinHV_Brightness, tubeBrightness * tubeBrightnessStep);
        EEPROM.write(menu, tubeBrightness);
        break;
    }
    case SlotMachine:
    {
        slotMachineFrequency += (decrease ? 1 : -1);
        slotMachineFrequency = (slotMachineFrequency < 1 ? 0 : slotMachineFrequency);
        slotMachineFrequency = (slotMachineFrequency > slotMachineFrequencyMAX ? 0 : slotMachineFrequency);
        EEPROM.write(menu, slotMachineFrequency);
        break;
    }
    case SpinChangingNumbers:
    {
        scrollMode += (decrease ? 1 : -1);
        scrollMode = (scrollMode < 0 ? ScrollMode::UPDATE : scrollMode);
        scrollMode = (scrollMode > ScrollMode::UPDATE ? ScrollMode::NONE : scrollMode);

        EEPROM.write(menu, scrollMode);
        break;
    }
    case AnimateColorsMode:
    {
        activateAnimation = !activateAnimation;
        EEPROM.write(menu, activateAnimation);
        SetBackgroundColor(brightnessR, brightnessG, brightnessB);
        break;
    }
    case DateMode:
    {
        currentFormat += (decrease ? 1 : -1);
        currentFormat = (currentFormat < 0 ? DateFormat::YYMMDD : currentFormat);
        currentFormat = (currentFormat > DateFormat::YYMMDD ? DateFormat::DDMMYY : currentFormat);
        EEPROM.write(menu, currentFormat);
        break;
    }
    case HourModeSetup:
    {
        HoursMode12 = !HoursMode12;
        EEPROM.write(menu, HoursMode12);
        break;
    }
    case MinutesSetup:
    {
        if (decrease)
        {
            if (++now.mm == 60)
            {
                now.mm = 0;
            }
        }
        else
        {
            if (now.mm-- == 0)
                now.mm = 59;
        }
        rtc.adjust(now);
        break;
    }
    case SecondsSetup:
    {
        if (decrease)
        {
            if (++now.ss == 60)
                now.ss = 0;
        }
        else
        {
            if (now.ss-- == 0)
                now.ss = 59;
        }
        rtc.adjust(now);
        break;
    }
    case HoursSetup:
    {
        if (decrease)
        {
            if (++now.hh == 24)
                now.hh = 0;
        }
        else
        {
            if (now.hh-- == 0)
                now.hh = 23;
        }
        rtc.adjust(now);
        break;
    }
    case YearSetup:
    {
        now.yOff += (decrease ? 1 : -1);
        rtc.adjust(now);
        break;
    }
    case MonthSetup:
    {
        if (decrease)
        {
            if (++now.m == 13)
                now.m = 1;
        }
        else
        {
            if (now.m-- == 1)
                now.m = 12;
        }
        rtc.adjust(now);
        break;
    }
    case DaySetup:
    {
        if (decrease)
        {
            if (++now.d == 32)
                now.d = 1;
        }
        else
        {
            if (now.d-- == 1)
                now.d = 31;
        }

        rtc.adjust(now);
        break;
    }
    case SleepStart:
    {
        sleepHourStart += (decrease ? 1 : -1);
        sleepHourStart = (sleepHourStart < 0 ? 24 : sleepHourStart);
        sleepHourStart = (sleepHourStart > 24 ? 0 : sleepHourStart);
        EEPROM.write(menu, sleepHourStart);
        break;
    }
    case SleepEnd:
    {
        sleepHourEnd += (decrease ? 1 : -1);
        sleepHourEnd = (sleepHourEnd < 0 ? 24 : sleepHourEnd);
        sleepHourEnd = (sleepHourEnd > 24 ? 0 : sleepHourEnd);
        EEPROM.write(menu, sleepHourEnd);
        break;
    }
    case AlarmMode:
    {
        alarmModeEnabled = !alarmModeEnabled;
        EEPROM.write(menu, alarmModeEnabled);
        break;
    }
    case AlarmHour:
    {
        alarmHour += (decrease ? 1 : -1);
        alarmHour = (alarmHour < 0 ? 24 : alarmHour);
        alarmHour = (alarmHour > 24 ? 0 : alarmHour);
        EEPROM.write(menu, alarmHour);
        break;
    }
    case AlarmMinute:
    {
        alarmMinute += (decrease ? 1 : -1);
        alarmMinute = (alarmMinute < 0 ? 60 : alarmMinute);
        alarmMinute = (alarmMinute > 60 ? 0 : alarmMinute);
        EEPROM.write(menu, alarmMinute);
        break;
    }
    case ShowDate:
    {
        showDate = !showDate;
        EEPROM.write(menu, showDate);
        break;
    }
    case ActivateSensor:
    {
        sensorActivated = !sensorActivated;
        EEPROM.write(menu, sensorActivated);
        break;
    }
    case MotionSensorTime:
    {
        sensorTime += (decrease ? 1 : -1);
        sensorTime = (sensorTime < 1 ? 1 : sensorTime);
        sensorTime = (sensorTime > sensorTimeMAX ? 1 : sensorTime);
        EEPROM.write(menu, sensorTime);
        break;
    }
    default:
    {
        SpinAllNumbers();
        break;
    }
    }
}

void CheckAlarm()
{
    unsigned char hours = now.hour();
    unsigned char minutes = now.minute();
    unsigned char seconds = now.second();

    if (alarmModeEnabled &&
        hours == alarmHour &&
        minutes == alarmMinute &&
        seconds == 0)
    {
        fireAlarm = true;
    }
}

bool TimeToSleep()
{
    unsigned char hours = now.hour();

    if (sleepHourStart > sleepHourEnd)
    {
        // If sleep period exceeds one day (22:00 - 7:00)
        if (hours >= sleepHourStart || hours < sleepHourEnd)
        {
            return true;
        }
    }
    else
    {
        // Regular time period (1:00 - 7:00).
        if (hours >= sleepHourStart && hours < sleepHourEnd)
        {
            return true;
        }
    }

    return false;
}

void DimmDot()
{
    PORTB &= ~(1 << 2);
}

void SetDot()
{
    bool lightUp = false;

    switch (clockMode)
    {
    case Blink:
    {
        lightUp = (now.second() % 2 != 0);
        break;
    }
    case Permanent:
    {
        lightUp = true;
        break;
    }
    case Off:
    default:
    {
        lightUp = false;
        break;
    }
    }

    if (lightUp)
    {
        PORTB |= 1 << 2;
    }
    else
    {
        PORTB &= ~(1 << 2);
    }
}

void DisplayDate(bool blink = false)
{
    unsigned char years = now.year() % 100;
    unsigned char months = now.month();
    unsigned char days = now.date();

    if (blink)
    {
        switch (menu)
        {
        case YearSetup:
            years = NUMBER_MAX;
            break;
        case MonthSetup:
            months = NUMBER_MAX;
            break;
        case DaySetup:
            days = NUMBER_MAX;
            break;
        }
    }

    switch (currentFormat)
    {
    case DDMMYY:
    {
        DisplayThreeNumbers(days, months, years);
        break;
    }
    case MMDDYY:
    {
        DisplayThreeNumbers(months, days, years);
        break;
    }
    case YYMMDD:
    {
        DisplayThreeNumbers(years, months, days);
        break;
    }
    }
}

void ScrollFromTimeToDate()
{
    unsigned char hours = now.hour();
    unsigned char minutes = now.minute();
    unsigned char seconds = now.second();

    unsigned char years = now.year() % 100;
    unsigned char months = now.month();
    unsigned char days = now.date();

    // Get the high and low order values for three pairs.
    unsigned char lowerFirst = hours % 10;
    unsigned char upperFirst = hours / 10;
    unsigned char lowerSecond = minutes % 10;
    unsigned char upperSecond = minutes / 10;
    unsigned char lowerThird = seconds % 10;
    unsigned char upperThird = seconds / 10;

    for (int k = 0; k < 6; ++k)
    {
        for (int i = 0; i < 10; ++i)
        {
            for (unsigned char j = 0; j < 2; ++j)
            {
                DisplayNumbers(
                    (k == 0) ? ((upperFirst + i) % 10) : upperFirst,
                    (k == 1) ? ((lowerFirst + i) % 10) : lowerFirst,
                    (k == 2) ? ((upperSecond + i) % 10) : upperSecond,
                    (k == 3) ? ((lowerSecond + i) % 10) : lowerSecond,
                    (k == 4) ? ((upperThird + i) % 10) : upperThird,
                    (k == 5) ? ((lowerThird + i) % 10) : lowerThird
                    );
                delayMicroseconds(1000);
            }
            if (MenuPressed())
                break;
        }

        switch (currentFormat)
        {
        case DDMMYY:
        {
            if (k == 0) upperFirst = days / 10;
            if (k == 1) lowerFirst = days % 10;
            if (k == 2) upperSecond = months / 10;
            if (k == 3) lowerSecond = months % 10;
            if (k == 4) upperThird = years / 10;
            if (k == 5) lowerThird = years % 10;

            break;
        }
        case MMDDYY:
        {
            if (k == 0) upperFirst = months / 10;
            if (k == 1) lowerFirst = months % 10;
            if (k == 2) upperSecond = days / 10;
            if (k == 3) lowerSecond = days % 10;
            if (k == 4) upperThird = years / 10;
            if (k == 5) lowerThird = years % 10;

            break;
        }
        case YYMMDD:
        {
            if (k == 0) upperFirst = years / 10;
            if (k == 1) lowerFirst = years % 10;
            if (k == 2) upperSecond = months / 10;
            if (k == 3) lowerSecond = months % 10;
            if (k == 4) upperThird = days / 10;
            if (k == 5) lowerThird = days % 10;

            break;
        }
        }

        if (MenuPressed())
            break;
    }

    for (int i = 0; i < 400; ++i)
    {
        DisplayDate();
    }

    years = rtc.now().year() % 100;
    months = rtc.now().month();
    days = rtc.now().date();

    switch (currentFormat)
    {
    case DDMMYY:
    {
        upperFirst = days / 10;
        lowerFirst = days % 10;
        upperSecond = months / 10;
        lowerSecond = months % 10;
        upperThird = years / 10;
        lowerThird = years % 10;

        break;
    }
    case MMDDYY:
    {
        upperFirst = months / 10;
        lowerFirst = months % 10;
        upperSecond = days / 10;
        lowerSecond = days % 10;
        upperThird = years / 10;
        lowerThird = years % 10;

        break;
    }
    case YYMMDD:
    {
        upperFirst = years / 10;
        lowerFirst = years % 10;
        upperSecond = months / 10;
        lowerSecond = months % 10;
        upperThird = days / 10;
        lowerThird = days % 10;

        break;
    }
    }

    hours = rtc.now().hour();
    minutes = rtc.now().minute();
    seconds = rtc.now().second();

    for (int k = 0; k < 6; ++k)
    {
        for (int i = 0; i < 10; ++i)
        {
            for (unsigned char j = 0; j < 2; ++j)
            {
                DisplayNumbers(
                    (k == 0) ? ((upperFirst + i) % 10) : upperFirst,
                    (k == 1) ? ((lowerFirst + i) % 10) : lowerFirst,
                    (k == 2) ? ((upperSecond + i) % 10) : upperSecond,
                    (k == 3) ? ((lowerSecond + i) % 10) : lowerSecond,
                    (k == 4) ? ((upperThird + i) % 10) : upperThird,
                    (k == 5) ? ((lowerThird + i) % 10) : lowerThird
                    );
                delayMicroseconds(1000);
            }
            if (MenuPressed())
                break;
        }

        if (k == 0) upperFirst = hours / 10;
        if (k == 1) lowerFirst = hours % 10;
        if (k == 2) upperSecond = minutes / 10;
        if (k == 3) lowerSecond = minutes % 10;
        if (k == 4) upperThird = seconds / 10;
        if (k == 5) lowerThird = seconds % 10;

        if (MenuPressed())
            break;
    }
}

void DisplayTime(bool blink = false)
{
    unsigned char hours = now.hour();
    unsigned char minutes = now.minute();
    unsigned char seconds = now.second();

    if (seconds == 0 && minutes == 0 && beepOnHour && !beeped)
    {
        // Beep once on hour start.
        Beep(50);
        beeped = true;
    }

    if (seconds == 30)
    {
        beeped = false;
    }

    // if is time to show date and prevent cathode poisoning.
    if (seconds == 56 && (minutes % slotMachineFrequency) == 0 && slotMachineFrequency)
    {
        if (showDate)
        {
            ScrollFromTimeToDate();
        }
        else
        {
            SpinAllNumbers(60);
        }
        return;
    }

    if (HoursMode12 && hours > 12)
    {
        hours -= 12;
    }

    if (blink)
    {
        switch (menu)
        {
        case MinutesSetup:
            minutes = NUMBER_MAX;
            break;
        case HoursSetup:
            hours = NUMBER_MAX;
            break;
        case SecondsSetup:
            seconds = NUMBER_MAX;
            break;
        }
    }

    DisplayThreeNumbers(hours, minutes, seconds, (menu != MENU_NONE) ? NONE : scrollMode);
}

void ProcessMenu()
{
    bool blink = (++iterationsShowed < iterationsDimmDigits);

    switch (menu)
    {
    case HoursSetup:
    case MinutesSetup:
    case SecondsSetup:
        DisplayTime(blink);
        break;
    case YearSetup:
    case MonthSetup:
    case DaySetup:
        DisplayDate(blink);
        break;
    case SleepStart:
        DisplayThreeNumbers((byte)menu, blink ? NUMBER_MAX : sleepHourStart, sleepHourEnd);
        break;
    case SleepEnd:
        DisplayThreeNumbers((byte)menu, sleepHourStart, blink ? NUMBER_MAX : sleepHourEnd);
        break;
    case BacklightRed:
        DisplayThreeNumbers((byte)menu, 0, blink ? NUMBER_MAX : (brightnessR / brightnessStep));
        break;
    case BacklightGreen:
        DisplayThreeNumbers((byte)menu, 0, blink ? NUMBER_MAX : (brightnessG / brightnessStep));
        break;
    case BacklightBlue:
        DisplayThreeNumbers((byte)menu, 0, blink ? NUMBER_MAX : (brightnessB / brightnessStep));
        break;
    case DotSetup:
        DisplayThreeNumbers((byte)menu, 0, blink ? NUMBER_MAX : clockMode);
        break;
    case BeepSetup:
        DisplayThreeNumbers((byte)menu, 0, blink ? NUMBER_MAX : beepOnHour);
        break;
    case TubeBrightness:
        DisplayThreeNumbers((byte)menu, 0, blink ? NUMBER_MAX : tubeBrightness);
        break;
    case SlotMachine:
        DisplayThreeNumbers((byte)menu, 0, blink ? NUMBER_MAX : slotMachineFrequency);
        break;
    case SpinChangingNumbers:
        DisplayThreeNumbers((byte)menu, 0, blink ? NUMBER_MAX : scrollMode);
        break;
    case AnimateColorsMode:
        DisplayThreeNumbers((byte)menu, 0, blink ? NUMBER_MAX : activateAnimation);
        break;
    case DateMode:
        DisplayThreeNumbers((byte)menu, 0, blink ? NUMBER_MAX : currentFormat);
        break;
    case HourModeSetup:
        DisplayThreeNumbers((byte)menu, 0, blink ? NUMBER_MAX : HoursMode12);
        break;
    case AlarmMode:
        DisplayThreeNumbers((byte)menu, 0, blink ? NUMBER_MAX : alarmModeEnabled);
        break;
    case AlarmHour:
        DisplayThreeNumbers((byte)menu, blink ? NUMBER_MAX : alarmHour, alarmMinute);
        break;
    case AlarmMinute:
        DisplayThreeNumbers((byte)menu, alarmHour, blink ? NUMBER_MAX : alarmMinute);
        break;
    case ShowDate:
        DisplayThreeNumbers((byte)menu, 0, blink ? NUMBER_MAX : showDate);
        break;
    case ActivateSensor:
        DisplayThreeNumbers((byte)menu, 0, blink ? NUMBER_MAX : sensorActivated);
        break;
    case MotionSensorTime:
        DisplayThreeNumbers((byte)menu, 0, blink ? NUMBER_MAX : sensorTime);
        break;
    }

    if (iterationsShowed > (iterationsDimmDigits * 2))
    {
        iterationsShowed = 0;
    }
}

void ProcessButton()
{
    if (MenuPressed())
    {
        pressed = true;
        if (iterationsButtonPressed++ > 200)
        {
            pressed = false;
            menu = MENU_NONE;
            iterationsButtonPressed = 0;
            //sleep = true;
        }
    }
    else if (pressed && !sleep)
    {
        iterationsButtonPressed = 0;
        iterationsInMenu = 0;
        pressed = false;
        fireAlarm = false;

        RestoreBacklight();
        Beep(50);

        if (++menu == MENU_MAX)
        {
            menu = MENU_NONE;
        }
    }
}

void ReadEncoder()
{
    // Read encoder pins.
    unsigned char encoder_A = (PINB & (1 << pinEncoderA));//digitalRead(encoderA);
    unsigned char encoder_B = (PINB & (1 << pinEncoderB));//digitalRead(encoderB);
    if ((!encoder_A) && (encoder_A_prev))
    {
        sleep = false;
        powerON = true;
        RestoreBacklight();
        fireAlarm = false;
        iterationsButtonPressed = 0;
        iterationsInMenu = 0;
        ProcessEncoderChange(!encoder_B);

        iterationSensor = sensorTime * iterationSensorStep;
    }

    //Store value of A for next time.
    encoder_A_prev = encoder_A;
}
//
void ReadIRCommand()
{
    if (irrcv.decode(&res))
    {
        sleep = false;
        RestoreBacklight();
        fireAlarm = false;

        switch (res.value)
        {
        case 0xF7C03F: // ON
        {
            powerON = true;
            Beep(50);
            break;
        }
        case 0xF740BF: // OFF
        {
            Beep(50);
            powerON = false;
            break;
        }
        case 16195807: // Red
        {
            SaveRGBColors(brightnessMAX, 0, 0);
            Beep(50);
            break;
        }
        case 16228447: // Green
        {
            SaveRGBColors(0, brightnessMAX, 0);
            Beep(50);
            break;
        }
        case 16212127: // Blue
        {
            SaveRGBColors(0, 0, brightnessMAX);
            Beep(50);
            break;
        }
        // Plus
#ifdef IR_24_KEY
        case 0xF7807F:
        case 0xFFB847:
#else
        case 0x937BB355:
        case 0xCED4C7A9:
#endif
        {
            static bool plusToggled = false;
#ifndef IR_24_KEY
            if (plusToggled)
#endif
            {
                Beep(50);
                iterationsInMenu = 0;
                ProcessEncoderChange(true);
            }
            plusToggled = !plusToggled;
            break;
        }
        // Minus
#ifdef IR_24_KEY
        case 0xF700FF:
        case 0xFF906F:
#else
        case 0x967BB80C:
        case 0xD1D4CC60:
#endif
        {
            static bool minusToggled = false;

#ifndef IR_24_KEY
            if (minusToggled)
#endif
            {
                Beep(50);
                iterationsInMenu = 0;
                ProcessEncoderChange(false);
            }
            minusToggled = !minusToggled;
            break;
        }
        // W key (menu)
#ifdef IR_24_KEY
        case 0xF7E01F:
        case 0xFFA857:
            //case 0x9BA392C1:
            //case 0xFFFFFFFF:
#else
        case 0x971BB598:
        case 0x5BC2A144:
#endif
        {
            static bool menuToggled = false;
#ifndef IR_24_KEY
            if (menuToggled)
#endif
            {
                Beep(50);
                iterationsInMenu = 0;
                if (++menu == MENU_MAX)
                {
                    menu = MENU_NONE;
                }
            }

            menuToggled = !menuToggled;
            break;
        }
        default:
            irrcv.resume();
            return;
        }
        // Receive the next value

        irrcv.resume();
    }
}

void ReadMotionSensor()
{
    if (sensorActivated)
    {
        if (iterationSensor < 0)
        {
            iterationSensor = 0;

            if (digitalRead(pinPirSensor) == LOW)
            {
                sleep = true;
            }
            else
            {
                sleep = false;
                RestoreBacklight();
                iterationSensor = sensorTime * iterationSensorStep;
            }
        }

        iterationSensor--;
    }
}

void loop()
{
    now = rtc.now();

    ReadIRCommand();
    ProcessButton();
    ReadEncoder();
    ReadMotionSensor();
    CheckAlarm();

    if (fireAlarm)
    {
        static int second = now.second();

        if (second != now.second())
        {
            Beep(100);
        }

        second = now.second();
    }

    if (now.second() == 30 && (now.minute() % 2) == 0)
    {
        sleep = TimeToSleep();

        if (!sleep && powerON && !sensorActivated) // exit sleep timer
        {
            RestoreBacklight();
        }
    }

    if (sleep || !powerON)
    {
        pressed = false;
        DimmDot();
        pixels.clear();
        pixels.show();
        digitalWrite(pin12VSwitch, LOW);

        return;
    }

    SetDot();
    if (activateAnimation)
    {
        AnimateColors();
    }

    if (menu != MENU_NONE)
    {
        ProcessMenu();

        if (iterationsInMenu++ > 800)
        {
            menu = MENU_NONE;
        }

        return;
    }

    DisplayTime();
}

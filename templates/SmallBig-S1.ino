// SmallBig-1KB Matter Matters by PROTOROOM <protoroom@gmail.com>
// https://protoroom.kr/smallbig
// based on Byteseeker Jr.
// Adapted from Michael Smith's PCM Audio sketch on the Arduino Playground

/* @WEBARDU_META
{
  "id": "smallbig_s1",
  "name": "SmallBig S1",
  "description": "SmallBig bytebeat firmware. case 1 수식을 교체할 수 있습니다.",
  "params": [
    {
      "key": "CASE1_EXPR",
      "label": "Case 1 Bytebeat Expression",
      "type": "code",
      "default": "(t * t * (t & 255) * x / 156 + (t * (t ^ 15) + t) * ((h | t / 2048 + 1 & 127) - h) / 64 & 127 - x * ((t >> 5 & 127) * 2 / 3 + 32))",
      "description": "세미콜론 없이 C 표현식만 입력하세요. 예: t*(t>>5|t>>8)"
    }
  ]
}
*/


#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <EEPROM.h>
#include <avr/pgmspace.h>

#include "U8g2lib.h"
// U8G2_SH1106_128X64_NONAME_2_4W_SW_SPI u8g2(U8G2_R0, 13, 5, 3, 9, 8); // D0, D1, CS, DC, Reset
// U8G2_SH1106_128X64_NONAME_2_4W_HW_SPI u8g2(U8G2_R0, 9, 8); // CS, DC, CLK(13), MOSI(11)
// U8G2_SSD1309_128X64_NONAME2_2_4W_SW_SPI u8g2(U8G2_R0, 13, 5, 3, 9, 8); // D0, D1, CS, DC, Reset
U8G2_SSD1309_128X64_NONAME2_1_4W_HW_SPI u8g2(U8G2_R0, 9, 8); // CS, DC, CLK(13), MOSI(11)

#define SAMPLE_RATE 8000


const char expr0[] PROGMEM = "(t*t*(t&255)*x/156+(t*(t^15)+t)*((h|t/2048+1&127)-h)/64&127-x*((t>>5&127)*2/3+32))";
const char expr1[] PROGMEM = "(~t >> 2) * ((127 & t * (7 & t >> a)) < (245 & t * (2 + (55 & t >> b))))";
const char expr2[] PROGMEM = "(t * (t >> (a / 10) | t >> (b / 10))) >> (t >> ((b / 10) * 2))";
const char expr3[] PROGMEM = "t * (((t >> a) & (t >> 5)) & ((b + 73) & (t >> 3)))";
const char expr4[] PROGMEM = "t * (((t >> a) | (t >> (1 * 3))) & (58 & (t >> b)))";
const char expr5[] PROGMEM = "((t >> a & t) - (t >> a) + (t >> a & t)) + (t * ((t >> b)&b))";
const char expr6[] PROGMEM = "((t % 42) * (t >> a) | (0x577338) - (t >> a)) / (t >> b) ^ (t | (t >> a))";
const char expr7[] PROGMEM = "(t >> a | t | t >> (t >> 8)) * b + ((t >> (b + 1)) & (a + 1))";
const char expr8[] PROGMEM = "((t * (t >> a | t >> (a + 1))&b & t >> 8)) ^ (t & t >> 13 | t >> 6)";
const char expr9[] PROGMEM = "((t >> 16) * 7 | (t >> a) * 8 | (t >> b) * 7) & (t >> 7)";
const char expr10[] PROGMEM = "t>>a | -t>>b";
const char expr11[] PROGMEM = "t%a+t%64 | t%b+t%28";

const char* const exprList[] PROGMEM = {
  expr0, expr1, expr2, expr3, expr4, expr5,
  expr6, expr7, expr8, expr9, expr10, expr11
};
char formula_buffer[150];

// System Mode 
// 0 = default, 
int mode = 0;
int modeCount = 0;
int modeCount2 = 0;
int mSwitch1 = 12;
int mSwitch1Prev = 0;
int mSwitch2 = 5;
int mSwitch2Prev = 0;
boolean needToReadEEPROM = true;


// Knob Pin
int knobA = 0;
int knobB = 1;

// Encoder Pin
const int enc1A = A2;
const int enc1B = A3;
const int enc2A = A4;
const int enc2B = A5; 

int enc1CurrentState = 0;
int enc1LastState = 0;
int enc2CurrentState = 0;
int enc2LastState = 0;

int enc1Value = 0;
float enc1SmoothValue = 0;
int enc1FinalValue = 0;
int enc2Value = 0;
// encoder debounce test
unsigned long lastEncoderCheck = 0;
const int encoderCheckInterval = 1; // 1ms
uint8_t lastEncoded = 0;


// Button Pin
int ButtPlus = 6;
int ButtMinus = 7;
int ButtLoop = 4;
int ButtReset = 2;

int ledPin = 12; //13 -> 12
int speakerPin = 3; 



boolean endOpening;
int logoY = 64;

int step = 0; // every pattern repeats 8 steps.
int stepS = 1;
int stepE = 8;
int loopState = 0; // 0:check stepS, 1:check stepE, 2: nothing.
unsigned int ft = 0; // for test
unsigned int t = 0; // 0 ~ 65,535
unsigned int tSave = 0;
int h = 0;
int x = 0;

int j = 0;
int k = 0;


unsigned int lastTime = 0;
unsigned int thisTime = 0;
volatile int a, b, c, d;
volatile int oldA, oldB;
//volatile int value;
float value;
float f_value;
float value_1;
float f_value_1;

int col;
int state = 99;
int oldState = 0;
int states = 12;
int buttonPressed = 0;
int buttonPressed2 = 0;
volatile int aTop = 99;
volatile int aBottom = 0;
volatile int bTop = 99;
volatile int bBottom = 0;

// Function prototypes
// boolean showStaticOpening();
// void resetStep();
// void readEEPROM();
// void writeEEPROM();
// void showControl(int value, int a, int b);
// void printText(int value);

void stopPlayback()
{
  // Disable playback per-sample interrupt.
  TIMSK1 &= ~_BV(OCIE1A);

  // Disable the per-sample timer completely.
  TCCR1B &= ~_BV(CS10);

  // Disable the PWM timer.
  TCCR2B &= ~_BV(CS10);

  digitalWrite(speakerPin, LOW);
}


// This is called at 8000 Hz to load the next sample.
ISR(TIMER1_COMPA_vect) {

  switch (state) {
    case 0:
      value = 0;
      aTop = 128;
      aBottom = 0;
      bTop = 64;
      bBottom = 0;
      break;

    case 1:
      x = t >> b & 1;
      h = (t >> a) + 4;
      value = {{CASE1_EXPR}};
      //      value = ((t&((t>>a)))+(t|((t>>b))))&(t>>(a+1))|(t>>a)&(t*(t>>b));
      aTop = 20;
      aBottom = 0;
      bTop = 20;
      bBottom = 0;
      break;

    case 2:
      value = (~t >> 2) * ((127 & t * (7 & t >> a)) < (245 & t * (2 + (55 & t >> b))));

      //  value =(~t>>2)*((127&t*(7&t>>a))<(245&t*(2+(5&t>>b))));
      //  value =(~t>>2)*((127&t*(7&t>>10))<(245&t*(2+(5&t>>14))));

      //  j = (t-41024&t+1024)>>11&255;
      //  k = t&16383;
      //  value = (t*(j&2550/2&127)+((100000/(t&4095))/4&63)+((int(k/k>>a)-136)/8)&((32*(1+(j>7))-1)));

      //  value = (t*(j&2550/2&127)+((100000/(t&4095))/4&63)+((int(k/k>>7)-136)/8)&((32*(1+(j>7))-1)));
      //  value =(t*(t>>(a/10)|t>>(b/10)))>>(t>>((b/10)*2));
      aTop = 18;
      aBottom = 0;
      bTop = 16;
      bBottom = 0;
      break;

    case 3:
      //      value = t * (t >> ((t & a) / ((t * t) / 2048) | (t / 4096))) | (t << (t / b)) | (t >> 4);
      //      value = t*(((t>>(a*3))|(t>>(10+a)))&(b&(t>>(a*2))));
      value = (t * (t >> (a / 10) | t >> (b / 10))) >> (t >> ((b / 10) * 2));
      aTop = 70;
      aBottom = 0;
      bTop = 70;
      bBottom = 0;
      break;
    //    value = t*(((t>>(a*3))|(t>>(10+a)))&(b&(t>>(a*2))));
    //    aTop = 6;
    //    aBottom =0;
    //    bTop = 50;
    //    bBottom = 0;
    //    break;

    case 4:
      value = t * (((t >> a) & (t >> 5)) & ((b + 73) & (t >> 3)));
      aTop = 7;
      aBottom = 0;
      bTop = 200;
      bBottom = 0;
      break;

    case 5:
      value = t * (((t >> a) | (t >> (1 * 3))) & (58 & (t >> b)));
      aTop = 100;
      aBottom = 0;
      bTop = 10;
      bBottom = 0;
      break;

    case 6:
      value = ((t >> a & t) - (t >> a) + (t >> a & t)) + (t * ((t >> b)&b)); // original song
      aTop = 15;
      aBottom = 0;
      bTop = 15;
      bBottom = 0;
      break;

    case 7:
      value = ((t % 42) * (t >> a) | (0x577338) - (t >> a)) / (t >> b) ^ (t | (t >> a));
      aTop = 8;
      aBottom = 0;
      bTop = 16;
      bBottom = 0;
      break;

    case 8:
      value = (t >> a | t | t >> (t >> 8)) * b + ((t >> (b + 1)) & (a + 1));
      aTop = 15;
      aBottom = 0;
      bTop = 60;
      bBottom = 0;
      break;

    case 9:
      value = ((t * (t >> a | t >> (a + 1))&b & t >> 8)) ^ (t & t >> 13 | t >> 6);
      aTop = 16;
      aBottom = 0;
      bTop = 200;
      bBottom = 0;
      break;

    case 10:
      value = ((t >> 16) * 7 | (t >> a) * 8 | (t >> b) * 7) & (t >> 7);
      aTop = 30;
      aBottom = 0;
      bTop = 40;
      bBottom = 0;
      break;

    case 11:
      value = t>>a | -t>>b;
      aTop = 15;
      aBottom = 0;
      bTop = 15;
      bBottom = 0;
      break;

    case 12:
      value = t%a+t%64 | t%b+t%28;
      aTop = 40;
      aBottom = 0;
      bTop = 40;
      bBottom = 0;
      break;

    case 99: // booting sound
      a = 14;
      b = 10;
      //      value = ((t >> a & t) - (t >> a) + (t >> a & t)) + (t * ((t >> b)&b));
      //      value = ((t % 42) * (t >> a) | (0x577338) - (t >> a)) / (t >> b) ^ (t | (t >> a));
      // value = (t * (t >> (a / 10) | t >> (b / 10))) >> (t >> ((b / 10) * 2));  // original song
      value = (~t >> 2) * ((127 & t * (7 & t >> a)) < (245 & t * (2 + (55 & t >> b))));
      aTop = 15;
      aBottom = 0;
      bTop = 15;
      bBottom = 0;
      break;

  }

  //  f_value = 0.5 * (value + value_1);


  // filter ?
  value_1 = value;
  f_value_1 = f_value;

  //  OCR2A = 0xff & value;
  //  OCR2A = f_value;
  OCR2B = value;

  if (modeCount2 == 1){
    --t;
    if (t <= (stepS - 1) * 8192) t = (stepE * 8192) - 1;
  } else {
    ++t;
    if (t >= (stepE * 8192) - 1) t = (stepS - 1) * 8192;
  }


    // --t;
  // if (t <= (stepS - 1) * 8192) t = (stepE * 8192) - 1;

  step = (t / 8192) + 1;
  //  t += 4;
  //
  //  ++ft;
  //  if (ft % 8 == 0) ++t;
}


void startPlayback()
{
  pinMode(3, OUTPUT);  // Use pin 3 (OC2B) for speaker output
  pinMode(10, OUTPUT);  // Pots + side hooked up to this
  digitalWrite(10, HIGH);
  
  // Set up Timer 2 to do pulse width modulation on the speaker pin
  // Use internal clock (datasheet p.160)
  ASSR &= ~(_BV(EXCLK) | _BV(AS2));
  
  // Set fast PWM mode (p.157)
  TCCR2A |= _BV(WGM21) | _BV(WGM20);
  TCCR2B &= ~_BV(WGM22);
  
  // Do non-inverting PWM on pin OC2B (p.155)
  // On the Arduino this is pin 3
  TCCR2A = (TCCR2A | _BV(COM2B1)) & ~_BV(COM2B0);  // Enable OC2B (pin 3)
  // TCCR2A &= ~(_BV(COM2A1) | _BV(COM2A0));          // Disable OC2A (pin 11)
  
  // No prescaler (p.158)
  TCCR2B = (TCCR2B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10);
  
  // Set initial pulse width to the first sample
  OCR2B = 0;  // Use OCR2B for pin 3 (OC2B)
  
  // Set up Timer 1 to send a sample every interrupt
  cli();
  // Set CTC mode (Clear Timer on Compare Match) (p.133)
  // Have to set OCR1A *after*, otherwise it gets reset to 0!
  TCCR1B = (TCCR1B & ~_BV(WGM13)) | _BV(WGM12);
  TCCR1A = TCCR1A & ~(_BV(WGM11) | _BV(WGM10));
  
  // No prescaler (p.134)
  TCCR1B = (TCCR1B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10);
  
  // Set the compare register (OCR1A)
  // OCR1A is a 16-bit register, so we have to do this with
  // interrupts disabled to be safe
  OCR1A = F_CPU / SAMPLE_RATE;    // 16e6 / 8000 = 2000
  
  // Enable interrupt when TCNT1 == OCR1A (p.136)
  TIMSK1 |= _BV(OCIE1A);
  sei();
}


void setup() {
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);

 pinMode(mSwitch1, INPUT);
 pinMode(mSwitch2, INPUT);
 digitalWrite(mSwitch1, HIGH);
 digitalWrite(mSwitch2, HIGH);

  pinMode(ButtPlus, INPUT);
  digitalWrite(ButtPlus, HIGH);
  pinMode(ButtMinus, INPUT);
  digitalWrite(ButtMinus, HIGH); // set pullup

  pinMode(ButtLoop, INPUT);
  digitalWrite(ButtLoop, HIGH);
  pinMode(ButtReset, INPUT);
  digitalWrite(ButtReset, HIGH); // set pullup

  pinMode(enc1A, INPUT);
  pinMode(enc1B, INPUT);
  // pinMode(enc2A, INPUT);
  // pinMode(enc2B, INPUT);

  enc1LastState = digitalRead(enc1A);
  // enc2LastState = digitalRead(enc2A);

  u8g2.begin();
  startPlayback();

  //printProg(0);
  lastTime = millis();
  thisTime = millis();
   Serial.begin(9600);   // Debugging

   a = 7;
   b = 7;
   readModeEEPROM();
}

void loop() {


  if (!endOpening) {
    endOpening = showStaticOpening();
  }


  // Is this working? May be broken by the timer action above
  thisTime = millis();


  if ((thisTime - lastTime) > 5 && endOpening) {

    if (digitalRead(ButtLoop) == LOW && buttonPressed2 == 0 && loopState == 0) {
      buttonPressed2 = 1;
      //      t = tSave;
      stepS = step;
      loopState = 1;
    }
    if (digitalRead(ButtLoop) == LOW && buttonPressed2 == 0 && loopState == 1) {
      buttonPressed2 = 1;
      //      t = tSave;
      stepE = step;
      loopState = 2;
    }


    if (digitalRead(ButtReset) == LOW) {
      //      tSave = ((t/8192)*8192)-1;
      resetStep();
    }


    //updateScreen();
    lastTime = thisTime;
    if (modeCount <= 1) {
      a = map(analogRead(knobA), 1023, 0, aBottom, aTop);
      b = map(analogRead(knobB), 1023, 0, bBottom, bTop);
    } else if (modeCount == 2) {
         // Serial.println(enc1Value);
      enc1Value = map(analogRead(enc1A), 0, 1023, 0, 255);
      enc1SmoothValue = 0.9 * enc1SmoothValue + 0.1 * enc1Value;
      enc1FinalValue = round(enc1SmoothValue);
      a = map(analogRead(knobA), 1023, 0, aBottom, aTop);
      b = map(enc1Value, 0, 255, bBottom, bTop*2); 
      // b = map(analogRead(knobB), 1023, 0, bBottom, bTop);
    } else {
      // read from EEPROM
      if (needToReadEEPROM) {
        readEEPROM();
        needToReadEEPROM = false;
      }
      // a = 20;
      // b = 30;
    }

    if (modeCount == 1) {
      showFormulas(state, a, b);
    } else {
      showControl(state, a, b);
    }

    if ((digitalRead(ButtPlus) == LOW) && (digitalRead(ButtMinus) == LOW) && (buttonPressed = 1)) {
      state = 1;
      delay(20);
      //      digitalWrite(13, LOW);
      delay(400);
    }
    else if  ((digitalRead(ButtPlus) == LOW) && (buttonPressed == 0)) {
      buttonPressed = 1;
      int exitFlag = 0;
      delay(2);
      for (int i = 0; i < 3; i++) {
        if (digitalRead(ButtPlus) == HIGH) {
          delay(2);
          exitFlag = 1;
        }
      }
      if (!exitFlag) {
        state = (state + 1) % (states + 1);
        t = (stepS - 1) * 8192;
      }
    }

    else  if ((digitalRead(ButtMinus) == LOW) && (buttonPressed == 0)) {
      buttonPressed = 1;
      int exitFlag = 0;
      delay(10);
      for (int i = 0; i < 3; i++) {
        if (digitalRead(ButtMinus) == HIGH) {
          delay(2);
          exitFlag = 1;
        }
      }
      if (!exitFlag) {
        state--;
        t = (stepS - 1) * 8192;
        if (state < 0) {
          state = states;

        }

      }
    }

    if ((digitalRead(ButtPlus) == LOW) && (digitalRead(ButtMinus) == LOW)) {
      state = 1;
    }
    if ((digitalRead(ButtPlus) == HIGH)  && (digitalRead(ButtMinus) == HIGH) && (buttonPressed == 1)) {
      buttonPressed = 0;
    }
    if ((digitalRead(ButtLoop) == HIGH)  && (digitalRead(ButtReset) == HIGH) && (buttonPressed2 == 1)) {
      buttonPressed2 = 0;
    }


// mode switch check 
  if (digitalRead(mSwitch1) == HIGH) {
    if (mSwitch1Prev == 0)  {
      modeCount++;
      modeCount = modeCount%4;
      needToReadEEPROM = true;
      writeModeEEPROM();
    }
    mSwitch1Prev = 1;
  } else {
    mSwitch1Prev = 0;
  }

  // mode switch 2 check
  if (digitalRead(mSwitch2) == LOW) {
    if (mSwitch2Prev == 0)  {
      modeCount2++;
      modeCount2 = modeCount2%4;
    }
    mSwitch2Prev = 1;
  } else {
    mSwitch2Prev = 0;
  }
  if (modeCount == 0 && modeCount2 == 3) {
    writeEEPROM();
    modeCount2 = 0;
  } 



    // Encoder Process, Polling method
    // uint8_t encoded = (digitalRead(enc1A) << 1) | digitalRead(enc1B);
    // uint8_t sum = (lastEncoded << 2) | encoded;
    // if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) enc1Value++;
    // if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) enc1Value--;
    // lastEncoded = encoded;

    // Serial.println(enc1Value);
  //   enc1Value = map(analogRead(enc1A), 0, 1023, 0, 255);
  //   enc1SmoothValue = 0.9 * enc1SmoothValue + 0.1 * enc1Value;
  //   enc1FinalValue = round(enc1SmoothValue);
  //  a = map(enc1Value, 0, 255, aBottom, aTop); 
  
  
  
  
  }
}

void writeEEPROM() {
  if (EEPROM.read(10) != state) EEPROM.write(10, state); // Code Number
  if (EEPROM.read(11) != stepS) EEPROM.write(11, stepS); // Step Start
  if (EEPROM.read(12) != stepE) EEPROM.write(12, stepE); // Step End
  if (EEPROM.read(13) != a) EEPROM.write(13, a); // A
  if (EEPROM.read(14) != b) EEPROM.write(14, b); // B
  needToReadEEPROM = true;
}

void writeModeEEPROM() {
  if (EEPROM.read(15) != modeCount) EEPROM.write(15, modeCount); // Mode Count
}

void readEEPROM() {
  state = EEPROM.read(10); // Code Number
  stepS = EEPROM.read(11); // Step Start
  stepE = EEPROM.read(12); // Step End
  a = EEPROM.read(13); // A
  b = EEPROM.read(14); // B
  step = stepS;
}

void readModeEEPROM() {
  modeCount = EEPROM.read(15); // Mode Count
}


// ==== Control
void resetStep() {
  stepS = 1;
  stepE = 8;
  loopState = 0;
}





// ==== UI

boolean showStaticOpening() {
  logoY = 32;
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_koleeko_tr);
    u8g2.drawStr(0, 64 - logoY - 7, "SmallBig-MM v1.0");
    u8g2.setFont(u8g2_font_commodore64_tr);

    u8g2.drawStr(0, logoY + 7, "PROTOROOM");
  } while ( u8g2.nextPage() );
  delay(800);
  state = 0;
  t = 0;
  return true;
}

boolean showOpening() {
  u8g2.firstPage();
  do {
    //    u8g2.setFont(u8g2_font_ncenB14_tr);
    u8g2.setFont(u8g2_font_koleeko_tr);
    //    u8g2.setFont(u8g2_font_dystopia_tr);
    //    u8g2.drawStr(0,y,"SmallBig-S0");
    u8g2.drawStr(0, 64 - logoY - 7, "SmallBig-S0 v1.0");
    u8g2.setFont(u8g2_font_commodore64_tr);

    u8g2.drawStr(0, logoY + 7, "PROTOROOM");
  } while ( u8g2.nextPage() );
  delay(4);
  if (logoY > 32) {
    logoY--;
    return false;
  } else {
    delay(900);
    state = 0;
    delay(400);
    t = 0;
    return true;
  }
}

void showFormulas(int codeNumber, int a, int b) {
  u8g2.firstPage();
  do {


    u8g2.setFont(u8g2_font_micropixel_tr);
    u8g2.drawStr(0, 9, "CODE="); 
    u8g2.setCursor(24, 9);
    u8g2.print(codeNumber);
    u8g2.drawStr(36, 9, "STEP=");
    u8g2.setCursor(60, 9);
    u8g2.print(step);
    u8g2.drawStr(72, 9, "A=");
    u8g2.setCursor(81, 9);
    u8g2.print(a);
    u8g2.drawStr(100, 9, "B=");
    u8g2.setCursor(109, 9);
    u8g2.print(b);

    u8g2.setFont(u8g2_font_6x10_mr);
    byte fontWidth = 6;
    byte fontHeight = 10;
    byte lineGap = 0;
    byte lineY = 21;


      if (codeNumber >= 1 && codeNumber <= 12) {
        strcpy_P(formula_buffer, (PGM_P)pgm_read_word(&(exprList[codeNumber - 1])));
        drawLongStr(0, lineY, formula_buffer, fontWidth, fontHeight, lineGap);
      }
    
    // u8g2.setFont(u8g2_font_5x7_tr);
    // drawLongStr(0, 18, "(~t >> 2) * ((127 & t * (7 & t >> a)) < (245 & t * (2 + (55 & t >> b))))", 5, 7, 1);
    // u8g2.drawStr(0, 13, "(127 & t"); // * (7 & t >> a)) < (245 & t * (2 + (55 & t >> b))))");
    // u8g2.drawStr(0, 26, "(~t>>2)*((127&t*(7&t>"); //> a)) < (245 & t * (2 + (55 & t >> b))))");
    // u8g2.drawStr(0, 47, "(~t >> 2) * ((127 & t"); // * (7 & t >> a)) < (245 & t * (2 + (55 & t >> b))))");
    // u8g2.drawStr(0, 54, "(~t>>2)*((127&t*(7&t>"); //> a)) < (245 & t * (2 + (55 & t >> b))))");
    u8g2.drawStr(0, 63, "= ");
    u8g2.setCursor(12, 63);
    u8g2.print(value);
  } while ( u8g2.nextPage() );
}

void showControl(int value, int a, int b) {
  u8g2.firstPage();
  do {

// check screen size with cute dot :)
    if (value == 0) u8g2.drawPixel(a, b);


    //    u8g2.setFont(u8g2_font_koleeko_tr);
    u8g2.setFont(u8g2_font_shylock_nbp_tf);
    u8g2.drawStr(3, 20, "CODE : ");
    u8g2.setCursor(42, 20);
    u8g2.print(value);

    u8g2.drawStr(67, 20, "STEP : ");
    u8g2.setCursor(106, 20);
    //    int tt = int(t / 8000);
    u8g2.print(step);



    int ctrlTop = 63;
    u8g2.drawStr(15, ctrlTop, "A= ");
    u8g2.setCursor(35, ctrlTop);
    u8g2.print(a);
    // u8g2.print(enc1FinalValue);
    //    u8g2.print(stepS);

    u8g2.drawStr(75, ctrlTop, "B= ");
    u8g2.setCursor(95, ctrlTop);
    u8g2.print(b);
    //    u8g2.print(stepE);

    // print mode count
    // for debugging
    // printText(0, modeCount);
    // printText(55, modeCount2);
    for (int i=0; i<modeCount; i++) {
      u8g2.drawPixel(55+i*2, 15);
    }
    for (int i=0; i<modeCount2; i++) {
      u8g2.drawPixel(55+i*2, 17);
    }


    //    u8g2.setFont(u8g2_font_unifont_t_symbols);
    u8g2.setFont(u8g2_font_twelvedings_t_all);
    for (int i = 0; i < 8; i++) {
      if ((step - 1) % 8 == i)
        u8g2.drawDisc(5 + i * 16, 30, 3, U8G2_DRAW_ALL);
      else
        u8g2.drawCircle(5 + i * 16, 30, 3, U8G2_DRAW_ALL);
      if (stepS == stepE && i + 1 == stepS) {
        //        u8g2.drawGlyph(i * 15+1, 46, 0x21ba); // xx, 0x25c6, 0x21ba
        // t_all
        u8g2.drawGlyph(i * 16 + 1, 47, 0x0055);
      } else {
        //        if (i + 1 == stepS) u8g2.drawGlyph(i * 15+2, 46, 0x21aa); // 0x2192, 0x25ba, 0x21aa
        //        if (i + 1 == stepE) u8g2.drawGlyph(i * 15+2, 46, 0x21a9); // 0x2190, 0x25c4, 0x21a9
        // t_all
        if (i + 1 == stepS) u8g2.drawGlyph(i * 16 + 1, 46, 0x002e); // 0x2192, 0x25ba, 0x21aa
        if (i + 1 == stepE) u8g2.drawGlyph(i * 16 + 2, 46, 0x002c); // 0x2190, 0x25c4, 0x21a9
      }
    }

  } while ( u8g2.nextPage() );
  //  delay(50);
}
void drawLongStr(uint8_t x, uint8_t y, const char* text, uint8_t fontWidth, uint8_t fontHeight, uint8_t lineGap) {
  const uint8_t maxWidth = 128;
  const uint8_t maxHeight = 64;
  const uint8_t maxCharsPerLine = (maxWidth - x) / fontWidth;
  const uint8_t maxLines = (maxHeight - y) / (fontHeight + lineGap);

  uint8_t line = 0;
  uint16_t i = 0;

  char buffer[129]; // 충분한 크기 확보 (최대 라인 수용 + 널문자)

  uint8_t bufIndex = 0;

  while (text[i] != '\0' && line < maxLines) {
    if (text[i] == '\n' || bufIndex >= maxCharsPerLine) {
      buffer[bufIndex] = '\0'; // 널 문자 종료
      u8g2.drawStr(x, y + line * (fontHeight + lineGap), buffer);
      bufIndex = 0;
      line++;

      if (text[i] == '\n') {
        i++;
        continue;
      }
    } else {
      if (bufIndex < sizeof(buffer) - 1) { // 버퍼 오버플로 방지
        buffer[bufIndex++] = text[i];
      }
      i++;
    }
  }

  // 마지막 라인 그리기
  if (bufIndex > 0 && line < maxLines) {
    buffer[bufIndex] = '\0';
    u8g2.drawStr(x, y + line * (fontHeight + lineGap), buffer);
  }
}


void printText(int xPos, int value) {
    u8g2.setCursor(xPos, 63);
    u8g2.print(value);
}

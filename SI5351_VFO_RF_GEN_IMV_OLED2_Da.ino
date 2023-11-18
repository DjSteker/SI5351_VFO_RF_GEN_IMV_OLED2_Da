/*
  Name:    GeneradorVFO_RF.ino
  Created: 03/04/2023 17:13:51
  Author:  ivanm
*/

/********************************************************************************************************
  10kHz to 160MHz VFO / RF Generator with Si5351 and Arduino Nano, with Intermediate  Frequency (IF)
  offset (+ or -). See the schematics for wiring details. By J.  CesarSound - ver 1.0 - Dec/2020.
*********************************************************************************************************/
#define ENCODER_A     2       // Encoder pin A clk INT0/PCINT18 D2
#define ENCODER_B     3       // Encoder pin B dt INT1/PCINT19 D3
#define ENCODER_BTN   4       // Encoder pushbutton D4
#define adc        A3        //The pin used by  Signal Meter A/D input.
#define aFwdPin        A2        // señal hacia adelante
#define aRefPin        A1        // señal hacia atras
//#define band_Switch       5
#define TX_Switch     7




//Libraries
#include "TramasMicros2.h"
#include  <Wire.h>                 //IDE Standard
//#include "Rotary.h"               //Ben  Buxton https://github.com/brianlow/Rotary
#include "si5351.h"               //Etherkit  https://github.com/etherkit/Si5351Arduino
#include <Adafruit_GFX.h>         //Adafruit  GFX https://github.com/adafruit/Adafruit-GFX-Library
//#include <Adafruit_SSD1306.h>     //Adafruit SSD1306 https://github.com/adafruit/Adafruit_SSD1306
#include <Adafruit_SH110X.h>
//User  preferences
//------------------------------------------------------------------------------------------------------------
#define IF  455                //Enter your IF frequency, ex: 455 = 455kHz, 10700 = 10.7MHz,  0 = to direct convert receiver or RF generator, + will add and - will subtract IF  offfset.
#define FREQ_INIT  7000000   //Enter your initial frequency at startup,  ex: 7000000 = 7MHz, 10000000 = 10MHz, 840000 = 840kHz.
#define XT_CAL_F   33000     //Si5351 calibration factor, adjust to get exatcly 10MHz. Increasing this value  will decreases the frequency and vice versa.
//#define tunestep   A0        //Change  the pin used by encoder push button if you want.
#define S_GAIN     303       //Adjust the sensitivity of Signal Meter A/D input: 101 = 500mv; 202 = 1v;  303 = 1.5v; 404 = 2v; 505 = 2.5v; 1010 = 5v (max).
//------------------------------------------------------------------------------------------------------------


#define OLED_RESET    -1
#define i2c_Address 0x3C
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
//Adafruit_SSD1306 display = Adafruit_SSD1306(128, 64, &Wire);
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Si5351  si5351;


//------------------------------------------------------------------------------------------------------------


unsigned long freq = FREQ_INIT;
unsigned long freqold, fstep;
long interfreq = IF;
long interfreqold = 0;
long cal = XT_CAL_F;
unsigned int smval;
unsigned long long pll_freq = 90000000000ULL;
//unsigned long pll_freq = 90000000000ULL;
//byte encoder = 1;
int stp;
//byte n = 1;
//byte count, x, xo;
byte  x;
bool sts = 0;

int aFwd, aRef;
float vSWR;

bool encoderUpdate = false;
bool encoderDir = false;

void set_frequency(short dir) {
  //if (encoder == 1) {                         //Up/Down frequency
  if (dir == 1) freq = freq + fstep;
  if (freq >= 160000000) freq = 160000000;
  if (dir == -1) freq = freq - fstep;
  if (fstep == 1000000 && freq <= 1000000)  freq = 1000000;
  else if (freq < 10000) freq = 10000;
  //}
  //delayMicroseconds(100000);
}

unsigned long ultimaInterrupcion = 0;  // variable static con ultimo valor de
void encoderVoid() {
  //static unsigned long ultimaInterrupcion = 0;  // variable static con ultimo valor de
  // tiempo de interrupcion
  unsigned long tiempoInterrupcion = millis();  // variable almacena valor de func. millis

  if (tiempoInterrupcion < ultimaInterrupcion) {
    unsigned long TMr = (4294967295 - ultimaInterrupcion);
    tiempoInterrupcion = 7 - TMr;
  }


  if (tiempoInterrupcion - ultimaInterrupcion > 7) {  // rutina antirebote desestima
    // pulsos menores a 5 mseg.
    encoderUpdate = true;
    if (digitalRead(ENCODER_B) == HIGH)     // si B es HIGH, sentido horario
    {
      //set_frequency(1); //POSICION++ ;        // incrementa POSICION en 1
      encoderDir = true;
    }
    else {          // si B es LOW, senti anti horario
      //set_frequency(-1); // POSICION-- ;        // decrementa POSICION en 1
      encoderDir = false;
    }
    ultimaInterrupcion = tiempoInterrupcion;  // guarda valor actualizado del tiempo
  }
}




void tunegen() {
  //delay(40);
  //uint64_t frecuenciaCLK0 = (freq + (interfreq  * 1000ULL)) * 100ULL;
  //uint32_t frecuenciaInterCLK0 =  (interfreq  * 1000ULL);
  //uint64_t frecuenciaInterCLK0 =  (freq + (interfreq  * 1000ULL)) ;
  si5351.set_freq_manual((freq + (interfreq * 1000ULL)) * 100ULL, pll_freq, SI5351_CLK0);
  //si5351.set_freq_manual(frecuenciaInterCLK0 * 100ULL, SI5351_CLK0);
  //delay(400);
}

void displayfreq() {
  unsigned int m = freq / 1000000;
  unsigned int k = (freq % 1000000) / 1000;
  unsigned int h = (freq % 1000) / 1;


  display.setTextSize(2);

  char buffer[15] = "";
  if (m < 1) {
    display.setCursor(41, 1);
    sprintf(buffer, "%003d.%003d", k, h);
  }
  else if (m < 100) {
    display.setCursor(5, 1);
    sprintf(buffer, "%2d.%003d.%003d", m, k, h);
  }
  else if (m >= 100) {
    unsigned int h = (freq % 1000) / 10;
    display.setCursor(5, 1);
    sprintf(buffer, "%2d.%003d.%02d", m, k, h);
  }
  display.print(buffer);
  //delayMicroseconds(2000);
}

void setstep() {
  switch (stp) {
    case 1:
      stp = 2;
      fstep = 1;
      break;
    case 2:
      stp = 3;
      fstep = 10;
      break;
    case 3:
      stp = 4;
      fstep = 1000;
      break;
    case 4:
      stp = 5;
      fstep = 5000;
      break;
    case 5:
      stp = 6;
      fstep = 10000;
      break;
    case 6:
      stp = 1;
      fstep = 1000000;
      break;
  }
}

void layout() {
  display.setTextColor(SH110X_WHITE);
  display.drawLine(0, 20, 127, 20, SH110X_WHITE);
  display.drawLine(0, 43, 127, 43, SH110X_WHITE);
  display.drawLine(102, 44, 102, 59, SH110X_WHITE);
  display.setTextSize(2);
  display.setCursor(2, 25);
  display.print("TS:");
  if (stp == 2) display.print("1Hz");  if (stp == 3) display.print("10Hz"); if (stp == 4) display.print("1k");
  if (stp == 5) display.print("5k"); if (stp == 6) display.print("10k"); if (stp == 1) display.print("1M");
  display.setCursor(80, 30);
  display.setTextSize(1);
  display.print("IF:");
  display.print(interfreq);
  display.print("k");
  display.setTextSize(1);
  display.setCursor(108, 45);
  if (freq < 1000000) display.print("kHz");
  if (freq >= 1000000) display.print("MHz");
  display.setCursor(108, 54);
  if (interfreq == 0) display.print("VFO");
  if (interfreq != 0) display.print("L O");
  display.setCursor(90, 46);
  if (!sts) {
    display.print("RX");
    interfreq = IF;
  }
  else {
    display.print(int(vSWR));
    display.setCursor(90, 56);
    display.print("TX");
    interfreq = 0;
  }



  //delayMicroseconds(1000);
}

void PintarBarras() {

  //byte y = map(vSWR, 1, 42, 1,  14);
  byte y = map(vSWR, 1, 5, 1, 14);
  display.setTextSize(1);

  //Pointer
  display.setCursor(0, 48);  display.print("TU"); display.fillRect(15, 48, 75, 6, SH110X_BLACK);
  switch (y) {
    case 14: display.fillRect(15, 48, 2, 6, SH110X_WHITE);
    case 13: display.fillRect(20, 48, 2, 6, SH110X_WHITE);
    case 12: display.fillRect(25, 48, 2, 6, SH110X_WHITE);
    case 11: display.fillRect(30, 48, 2, 6, SH110X_WHITE);
    case 10: display.fillRect(35, 48, 2, 6, SH110X_WHITE);
    case 9: display.fillRect(40, 48, 2, 6, SH110X_WHITE);
    case 8: display.fillRect(45, 48, 2, 6, SH110X_WHITE);
    case 7: display.fillRect(50, 48, 2, 6, SH110X_WHITE);
    case 6: display.fillRect(55, 48, 2, 6, SH110X_WHITE);
    case 5: display.fillRect(60, 48, 2, 6, SH110X_WHITE);
    case 4: display.fillRect(65, 48, 2, 6, SH110X_WHITE);
    case 3: display.fillRect(70, 48, 2, 6, SH110X_WHITE);// break;
    case 2: display.fillRect(75, 48, 2, 6, SH110X_WHITE);// break;
    case 1: display.fillRect(80, 48, 2, 6, SH110X_WHITE);// break;
  }

  //Bargraph
  display.setCursor(0, 57);  display.print("SM"); display.fillRect(15, 58, 75, 6, SH110X_BLACK);
  switch (x) {
    case 14: display.fillRect(80, 58, 2, 6, SH110X_WHITE);
    case 13: display.fillRect(75, 58, 2, 6, SH110X_WHITE);
    case 12: display.fillRect(70, 58, 2, 6, SH110X_WHITE);
    case 11: display.fillRect(65, 58, 2, 6, SH110X_WHITE);
    case 10: display.fillRect(60, 58, 2, 6, SH110X_WHITE);
    case 9: display.fillRect(55, 58, 2, 6, SH110X_WHITE);
    case 8: display.fillRect(50, 58, 2, 6, SH110X_WHITE);
    case 7: display.fillRect(45, 58, 2, 6, SH110X_WHITE);
    case 6: display.fillRect(40, 58, 2, 6, SH110X_WHITE);
    case 5: display.fillRect(35, 58, 2, 6, SH110X_WHITE);
    case 4: display.fillRect(30, 58, 2, 6, SH110X_WHITE);
    case 3: display.fillRect(25, 58, 2, 6, SH110X_WHITE);
    case 2: display.fillRect(20, 58, 2, 6, SH110X_WHITE);
    case 1: display.fillRect(15, 58, 2, 6, SH110X_WHITE);
  }
}

void updateScreen() {
  LecturaSensores();
  delayMicroseconds(100);
  display.clearDisplay();
  delayMicroseconds(2000);
  displayfreq();
  layout();
  PintarBarras();
  display.display();
}

void LecturaSensores() {

  aFwd = analogRead(aFwdPin);
  aRef = analogRead(aRefPin);

  vSWR = (aFwd + aRef) / (aFwd - aRef);
  if (vSWR > 5) {
    vSWR = 5;
  }
  else if (vSWR < -1) {
    vSWR = -1;
  }
}

void VoidBtnEncoder() {
  if (encoderUpdate == true) {
    if (encoderDir == true) {
      set_frequency(1);
    }
    else {
      set_frequency(-1);
    }
    encoderUpdate = false;
  }

}

void  sgnalread() {

  smval = analogRead(adc);
  x = map(smval, 0, S_GAIN, 1, 14);
  if (x > 14) x = 14;

}

//------------------------------------------------------------------------------------------------------------


TramaTiempo blink_loopScreen = TramaTiempo(150007, updateScreen);
TramaTiempo blink_loopSignal = TramaTiempo(75003, sgnalread);
TramaTiempo blink_loopButtonStep = TramaTiempo(5000, VoidBtnEncoder);
//------------------------------------------------------------------------------------------------------------


void setup() {
  Wire.begin();
  //display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.begin(i2c_Address, true);
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.display();

  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);
  pinMode(ENCODER_BTN, INPUT_PULLUP);
  pinMode(TX_Switch, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(ENCODER_A), encoderVoid, LOW);// interrupcion sobre pin A con
  //attachInterrupt(digitalPinToInterrupt(ENCODER_A), encoderVoid, FALLING);// interrupcion sobre pin A con
  delay(300);
  display.setTextSize(1);
  display.setCursor(4, 5);
  display.print("Si5351");
  display.setCursor(4, 20);
  display.print("VFO / RF Generator");
  display.setCursor(4, 35);
  display.print("Version 2.1Da");
  display.setCursor(4, 50);
  display.print("JCR RADIO;IvanMV");
  display.display();
  delay(3000);
  display.clearDisplay();
  //  display.setCursor(1, 1);
  //  display.println("iniciado");
  //  display.display();
  //  delay(750);
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  si5351.set_correction(cal, SI5351_PLL_INPUT_XO);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA); // no es necesario, borrar en caso de error
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
  si5351.output_enable(SI5351_CLK0, 1);                  //1 - Enable  / 0 - Disable CLK
  si5351.output_enable(SI5351_CLK1, 0);
  si5351.output_enable(SI5351_CLK2, 0);
  //si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_2MA);  //Output current  2MA, 4MA, 6MA or 8MA

  display.clearDisplay();
  display.setCursor(1, 1);
  display.println("ok");
  display.display();
  delay(750);

  //  PCICR |= (1 << PCIE2);
  //  PCMSK2 |= (1 << PCINT18)  | (1 << PCINT19);
  //  sei();

  stp = 5;
  setstep();
  layout();
  displayfreq();
  PintarBarras();
  display.display();
  delayMicroseconds(20000);
  blink_loopScreen.reset();
  blink_loopSignal.reset();
}


void loop() {
  //delayMicroseconds(5000);
  bool FlusScreen = false;
  if (freqold != freq) {
    //time_now  = millis();
    tunegen();
    freqold = freq;
    FlusScreen = false;
    //delayMicroseconds(2000);
  }
  //  delayMicroseconds(5000);
  if (digitalRead(ENCODER_BTN) == LOW) {
    //time_now = (millis() + 300);
    delay(7);
    setstep();
    FlusScreen = true;
    delayMicroseconds(700000);
  }


  //delayMicroseconds(5000);
  if (digitalRead(TX_Switch) == LOW) {
    //time_now = (millis() + 300);
    if (sts == 0) FlusScreen = true;
    sts = 1;

  }
  else {
    if (sts == 1) FlusScreen = true;
    sts = 0; //displayfreq();
  }
  //delayMicroseconds(5000);
  if (FlusScreen == true) {
    FlusScreen = false;
    //    displayfreq();
    //    layout();
    delay(20);
    updateScreen();
    //delay(10);
    blink_loopScreen.reset();
    //delay(300);
  }
  delayMicroseconds(500);
  blink_loopButtonStep.check();
  delayMicroseconds(1000);
  blink_loopScreen.check();
  delayMicroseconds(15000);
  blink_loopSignal.check();
  //delayMicroseconds(5);
}

#include <Arduino.h>
#include <U8g2lib.h>
#include <AccelStepper.h>
#include <Rotary.h>
#include <Bounce2.h>
#include <Wire.h>

// MENU

#define MENU_NONE 0
#define MENU_PREV 1
#define MENU_NEXT 2
#define MENU_SELECT 3

#define CHANGE_MENU 4
#define CHANGE_VALUES 5

#define PICTURES 0
#define DEGS 1
#define DELAY 2
#define RPM 3
#define SLEEP 4
#define DIR 5
#define TIMELAPSE 6
#define TURN 7
#define FILM 8

// OLED 
U8X8_SH1106_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);

Rotary r = Rotary(3,2);   // define rotary encoder at pin 2 und 3 (push button at pin 9)

// Instantiate a Bounce object
Bounce menuSelect = Bounce();

//Funtions

#define setPhotos     8
#define setDegs       7
#define setSeconds    6
#define setPosition   2
#define setRPM        5
#define timelapse     4
#define film          3
#define setSleep      1

#define MENU_ITEMS 8

uint8_t menuCodeFirst = MENU_NONE;  // menu code states
uint8_t menuCodeSecond = MENU_NONE; //
uint8_t menuCode = MENU_NONE;       //

uint8_t menu_current = 0;            // current menu item
uint8_t mode = CHANGE_MENU;   // which menu is active (left or right)

uint8_t menu_redraw_required = 0;
uint8_t last_code = MENU_NONE;

int selectCounter = 0;              // counter for the number of button presses
int selectState = 0;                // current state of the button
int lastSelectState = 0;            // previous state of the button

int turnDirection = 1;              // Drehrichtung

//PIN
const int stepPIN = 6;
const int stepDirPIN = 5;
const int sleepPIN = 7;

const int optoPIN = 8;

const int selectPIN = 9;

const int ledPIN = 13;

// Initialize for A498 stepper driver
AccelStepper stepper(AccelStepper::DRIVER, stepPIN, stepDirPIN);

//Variables

int photos = 50;
int degs = 90;
int seconds = 5;
int rpm = 50;
int stepsPerTurn = 2038;        // A9488 + 1/64 gear + 1/4

long stepsPerDegree = 22.6444;
long stepsPerPhoto = 0;
int speedStepper = 200;
int motorStopDuration = 0;

boolean sleep = false;



// Calculators
void calcSpeedStepper()
{
  double x = stepsPerTurn/60.0;
  speedStepper = rpm * x;
  stepper.setMaxSpeed(speedStepper);
  stepper.setAcceleration(speedStepper/4); // Speed Stepper
}

void calcStepsPerPhoto()
{
  if(photos == 1) stepsPerPhoto = 0;
  else stepsPerPhoto = stepsPerDegree*degs/(photos-1);
}

void calcMotorStopDuration()
{
  long spinDuration = (stepsPerPhoto/speedStepper) * 1000;
  long runningTime = (( (seconds-1) * 1000 ) - spinDuration); // long runningTime = (( 2 * seconds * 1000 ) - spinDuration);
  if(runningTime > 0) motorStopDuration = runningTime * 0.92;
  else motorStopDuration = 0; 
}

//Controllers

// execute Steps
void doSteps(long steps)
{ 
  calcSpeedStepper(); 
  delay(1000); // delay(seconds * 1000);
  stepper.setCurrentPosition(0);
  stepper.move(steps);
  stepper.runToPosition();
  
  if(sleep)digitalWrite(sleepPIN, LOW);
}

// optocoupler Steuerung
void optocoupler()
{
  digitalWrite(optoPIN, HIGH);
  delay(1000); // delay(seconds * 1000);
  digitalWrite(optoPIN, LOW);
}

//Run Timelapse
void playTimeLapse()
{
  calcStepsPerPhoto();
  calcMotorStopDuration();

  digitalWrite(sleepPIN, HIGH);
  
  u8x8.clearDisplay();
  u8x8.setCursor(0, 0);
  u8x8.setFont( u8x8_font_victoriabold8_r);
  u8x8.print("   Timelapse   ");
  u8x8.setFont( u8x8_font_victoriamedium8_r);
  Serial.println("  Timelapse !   ");
  Serial.println(stepsPerDegree*degs);
  Serial.println(stepsPerPhoto);
  delay(50);                            
  
  for(int i = 1 ; i < photos ; i++)
    {
       u8x8.setCursor(0, 2);
       String stringOne = "Photo " + String((int)i) + " - " + String((int)photos);
       u8x8.print(stringOne);
       u8x8.setCursor(0, 4);
       int restZeit = (photos-i) * seconds;
       Serial.println(restZeit);
       String stringTwo = "";
       if (restZeit > 60) stringTwo = "Rest: >" + String(((int)restZeit)/60) + " min  ";
        else stringTwo = "Rest: " + String((int)restZeit) + " sec  ";
       u8x8.print(stringTwo);
       
       optocoupler();             //Kamera Shutter
       delay(5);                            
       doSteps(stepsPerPhoto*turnDirection);
       delay(motorStopDuration);
     }

     // last picture
     u8x8.setCursor(0, 2);
     String stringOne = "Photo " + String((int)photos) + " - " + String((int)photos) + "   ";
     u8x8.print(stringOne);
     optocoupler();                   
     
     u8x8.setCursor(0, 2);
     u8x8.print("   Rewinding... ");
     u8x8.setCursor(0, 3);
     u8x8.print("                 ");

     doSteps(-1*stepsPerDegree*degs*turnDirection); // homing

     menu_current = 0;
     selectCounter = 0;
     digitalWrite(sleepPIN, LOW);
     u8x8.clearDisplay();
}

void playFilm()
  {
    calcStepsPerPhoto();
    long pos = stepsPerDegree*degs;
    u8x8.clearDisplay();
    u8x8.setCursor(0, 1);
    u8x8.setFont( u8x8_font_victoriabold8_r);
    u8x8.print("    Filming !  ");
    Serial.println(pos);
    digitalWrite(sleepPIN, HIGH);
    doSteps(pos*turnDirection);
    u8x8.setCursor(0, 3);
    u8x8.setFont( u8x8_font_victoriamedium8_r);
    u8x8.print("   Rewinding... ");
    doSteps(-1*pos*turnDirection);

    menu_current = 0;
    selectCounter = 0;
    digitalWrite(sleepPIN, LOW);
    u8x8.clearDisplay();
  }

void turn()
  {
    u8x8.clearDisplay();
    u8x8.setCursor(0, 1);
    u8x8.print("    Turning !  ");
    if (turnDirection == 0) {
      u8x8.print("   cw    ");
      //doSteps(1);
      Serial.println("cw");

    }
    else {
      u8x8.print("   ccw   ");
      //doSteps(-1);
      Serial.println("ccw");
    }
    
  }


void paintMenu()
{
  u8x8.setInverseFont(0);
  
  u8x8.setFont( u8x8_font_victoriamedium8_r);
  u8x8.setCursor(0, 0);
  u8x8.print("Bilder:  ");
  if (menu_current == PICTURES) u8x8.setInverseFont(1);
      else u8x8.setInverseFont(0);
  u8x8.print(String(photos));
  u8x8.print("  ");
  u8x8.setInverseFont(0);  
  
  u8x8.setCursor(0, 1.5);
  u8x8.print("Winkel:  ");
  if (menu_current == DEGS) u8x8.setInverseFont(1);
    else u8x8.setInverseFont(0); 
  u8x8.print(String(degs));
  u8x8.print(" deg");
  u8x8.setInverseFont(0);  
  
  u8x8.setCursor(0, 2);
  u8x8.print("Pause:   ");
  if (menu_current == DELAY) u8x8.setInverseFont(1);
    else u8x8.setInverseFont(0);    
  u8x8.print(String((int)seconds));
  u8x8.print(" sec");
  u8x8.setInverseFont(0);  
  
  u8x8.setCursor(0, 3.5);
  u8x8.print("Geschw.: "); 
  if (menu_current == RPM) u8x8.setInverseFont(1);
    else u8x8.setInverseFont(0);  
  u8x8.print(String((int)rpm)); 
  u8x8.setInverseFont(0);  
  
  u8x8.setCursor(0, 4);
  u8x8.print("Sleep:   ");
  if(sleep) {
    if (menu_current == SLEEP) u8x8.setInverseFont(1);
      else u8x8.setInverseFont(0);  
    u8x8.print("ein");
  }
    else {
      if (menu_current == SLEEP) u8x8.setInverseFont(1);
        else u8x8.setInverseFont(0);  
      u8x8.print("aus");
    }
    u8x8.setInverseFont(0);

    u8x8.setCursor(0, 5.5);
  u8x8.print("Dir.:    ");
  if(turnDirection == 1) {
    if (menu_current == DIR) u8x8.setInverseFont(1);
      else u8x8.setInverseFont(0);  
    u8x8.print("CCW ");
  }
    else {
      if (menu_current == DIR) u8x8.setInverseFont(1);
        else u8x8.setInverseFont(0);  
      u8x8.print("CW ");
    }
    u8x8.setInverseFont(0);     
    
  u8x8.setCursor(0, 6);
  //u8x8.print("________________"); 
  u8x8.setFont(u8x8_font_amstrad_cpc_extended_f);
  u8x8.drawString(0, 6, "\x81");  
  u8x8.drawString(1, 6, "\x81");  
  u8x8.drawString(2, 6, "\x81");  
  u8x8.drawString(3, 6, "\x81");  
  u8x8.drawString(4, 6, "\x81");  
  u8x8.drawString(5, 6, "\x81");  
  u8x8.drawString(6, 6, "\x81");  
  u8x8.drawString(7, 6, "\x81");  
  u8x8.drawString(8, 6, "\x81");  
  u8x8.drawString(9, 6, "\x81");  
  u8x8.drawString(10, 6, "\x81");  
  u8x8.drawString(11, 6, "\x81");  
  u8x8.drawString(12, 6, "\x81");  
  u8x8.drawString(13, 6, "\x81");  
  u8x8.drawString(14, 6, "\x81");  
  u8x8.drawString(15, 6, "\x81");
  
//  u8x8.setFont(u8g2_font_lucasfont_alternate_tr);
//  u8x8.print("----------------"); 

  u8x8.setInverseFont(1);
  
  u8x8.setFont( u8x8_font_victoriabold8_r);
  if (menu_current == TIMELAPSE) u8x8.setInverseFont(1);
    else u8x8.setInverseFont(0);  
  u8x8.setCursor(0, 7);
  u8x8.print("Time");
  u8x8.setInverseFont(0);  

  if (menu_current == TURN) u8x8.setInverseFont(1);
    else u8x8.setInverseFont(0);  
  u8x8.setCursor(6, 7);
  u8x8.print("Turn");
  u8x8.setInverseFont(0);  
  
  if (menu_current == FILM) u8x8.setInverseFont(1);
    else u8x8.setInverseFont(0);  
  u8x8.setCursor(12, 7);
  u8x8.print("Film");
  u8x8.setInverseFont(0);  

  u8x8.setInverseFont(0);
}

void setup()
{
  //PC
  Serial.begin(9600);
  Serial.println("Start");

  // OLED
  u8x8.begin();
  u8x8.setFlipMode(1);
  u8x8.setFont( u8x8_font_victoriabold8_r);   
  u8x8.setCursor(0, 0);
  u8x8.print("  LAB-O-LAPSE   ");
  u8x8.setCursor(0, 2);
  u8x8.print("     v2.0     "); 
  delay(3000);
  u8x8.clear();

  //Stepper
  stepper.setMaxSpeed(200); // 1000
  stepper.setAcceleration(500); //3000
  stepper.setSpeed(200);

  //PIN
  pinMode(stepPIN, OUTPUT);
  pinMode(stepDirPIN, OUTPUT);
  pinMode(sleepPIN, OUTPUT);
  pinMode(optoPIN, OUTPUT);
  pinMode(ledPIN, OUTPUT);

  pinMode(selectPIN, INPUT_PULLUP);

  //setPIN mode
  digitalWrite(stepPIN, LOW);
  digitalWrite(stepDirPIN, LOW);
  digitalWrite(sleepPIN, LOW);
  digitalWrite(optoPIN, LOW);
  digitalWrite(ledPIN, LOW);

  menuSelect.attach(selectPIN);          // debounced push buttons
  menuSelect.interval(25);

  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();
}

void loop()
{

  menuSelect.update(); 
  if ( menuSelect.fell() ) {  // Call code if button transitions from HIGH to LOW
     selectCounter++;
     Serial.println(selectCounter);
   }
  
  if (selectCounter % 2 == 0) {     
    mode = CHANGE_MENU;
  } else {
    if (menu_current == TIMELAPSE) playTimeLapse();
      else if (menu_current == FILM) playFilm();
      else {
        mode = CHANGE_VALUES;
      }
  }  
  paintMenu();
}

ISR(PCINT2_vect) {
  unsigned char result = r.process();

if (result == DIR_CW) {                     // rotary encoder dial 
  
  if (mode == CHANGE_MENU) {
    menu_current++;
      if (  menu_current > MENU_ITEMS ) {
        menu_current = 0;
      }
  }
  else {
    switch (menu_current) {
      case PICTURES: 
      {
        photos = photos +1;
        break;
      }
      case DEGS:
      {
        degs = degs +1;
        break;
      }
      case DELAY:
      {
        seconds = seconds +1;
        break;
      }
      case RPM:
      {
        rpm = rpm +1;
        break;
      }
      case SLEEP:
      {
        if (sleep) sleep = 0;
          else {
            sleep = 1;
            digitalWrite(sleepPIN, LOW);
          }
          break;
      }
      case DIR:
      {
        turnDirection = turnDirection *-1;
        break;
      }
      case TURN:
      {
        //turnDirection = 0;
        Serial.println("ccw");
        break;
      }
      
    }
  }
  
  }
  else if (result == DIR_CCW) {
    if (mode == CHANGE_MENU) {
       if ( menu_current == 0 ) {
          menu_current = MENU_ITEMS;
        }
        else menu_current--;
    }
      else {
    switch (menu_current) {
      case PICTURES: 
      {
        photos = photos - 1;
        break;
      }
      case DEGS:
      {
        degs = degs - 1;
        break;
      }
      case DELAY:
      {
        seconds = seconds - 1;
        break;
      }
      case RPM:
      {
        rpm = rpm - 1;
        break;
      }
      case SLEEP:
      {
        if (sleep) sleep = 0;
          else {
            sleep = 1;
            digitalWrite(sleepPIN, LOW);
          }
          break;
      }
      case DIR:
      {
        turnDirection = turnDirection *-1;
        break;
      }
      case TURN:
      {
        //turnDirection = 1;
        Serial.println("cw");
        break;
      }
      
    }
  }
  }
}

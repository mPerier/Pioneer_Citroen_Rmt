#include <Arduino.h>
#include <EEPROM.h>

// ------------------- DEFINES ----------------------
#define inWire1 A2  // 4
#define inWire2 A3  // 3
#define confgBP 2   // 2
#define outWire 1   // 1
#define led     0   // 0

#define trig_inRes_Ohm 470 // The trigger resistance and voltage
#define in_supplyVoltage 5

#define out_inRes_Ohm 1000 // The output resistance and voltage
#define out_supplyVoltage 12 

#define tolerance_V 0.025

#define MWORD_BYTE 0
#define VSEUIL_BYTE 2
#define VOLTAGES_BYTE 6
#define WIRES_BYTE 38

#define MAGICWORD 101

enum {VOLM, VOLP, PREV, NEXT, FNC1, FNC2, FNC3, FNC4};

// --------------------- DATA -----------------------
// Represents the resistance of each button
// WIRE1: VOL- | VOL + | PREV | NEXT
// WIRE2: MEM- | MEM+  | MUTE | MODE
float triggers_V[8];
bool trig_Wires[8];

// Because the readed voltage is inversly proportional to the trigger resistance,
//  The loops that will use triggers_V will go from the last element to the first one.
float actions_V[8] = {};

// --------------- GLOBAL VARIABLES -----------------

double seuil_max_V; // The voltage under which no button is pressed
double infoLed_V = 0; 

// ------------------ PROTOTYPES --------------------

void sendPulse(float level_V);
void autoCal(void);
void blink(int nbBlink, int blinkDuration_ms);
int getRmtCmd(void);
void writeConfig(void);
unsigned char toByte(bool b[8]);
void fromByte(unsigned char c, bool b[8]);
int VoltsToByte(double voltage);
double ByteToVolts(int byteToConvert);
double getNormalVoltage();

// -------------------- SETUP -----------------------
void setup() {
  pinMode(inWire1,INPUT);
  pinMode(inWire2,INPUT);
  pinMode(outWire,OUTPUT);
  pinMode(confgBP,INPUT);
  pinMode(led,    OUTPUT);

  int i;
  int magicWord;
  float retreivedVoltage;

  // If the user maintains the confgBP during 1s at startup, the autocal process will be launched
  if (digitalRead(confgBP)){
    delay(1000);
    if (digitalRead(confgBP)){
      EEPROM.put(MWORD_BYTE,0);
    } 
  }
  EEPROM.get(MWORD_BYTE,magicWord);
  if (magicWord != MAGICWORD){
    autoCal();
  }
  else {
    // Vseuil
    EEPROM.get(VSEUIL_BYTE,seuil_max_V);
        
    // Voltages
    for (i=0;i<8;i++){
      EEPROM.get(VOLTAGES_BYTE+i*4,retreivedVoltage);
      triggers_V[i] = retreivedVoltage;
    }
    // Wires
    fromByte(EEPROM.read(WIRES_BYTE),trig_Wires);
  }
}

// -------------------- LOOP -----------------------
void loop() {
  int i;
  float lastVoltage;

  float readedVoltage1 = ByteToVolts(analogRead(inWire1));
  float readedVoltage2 = ByteToVolts(analogRead(inWire2));

  bool triggered = false;
  
  // Wire1 Actions
  lastVoltage = seuil_max_V;
  for (i=3;i>=0;i--){
    // If the readed voltage is inside one of the ranges
    if (readedVoltage1 > lastVoltage && readedVoltage1 < triggers_V[i]){
      // We send the pulse corresponding to the action ID i
      sendPulse(actions_V[i]);
      triggered = true;
    }
    // lastVoltage is used to define the lower limit of the range
    lastVoltage = triggers_V[i];
  }
  
  // Wire2 Actions only if Wire1 haven't raised a trigger
  if (triggered == false){
    lastVoltage = seuil_max_V;
    for (i=3;i>=0;i--){
      if (readedVoltage2 > lastVoltage && readedVoltage2 < triggers_V[i]){
        sendPulse(actions_V[i+4]);
      }
      lastVoltage = triggers_V[i];
    }
  }
}

// ------------------ FUNCTIONS --------------------
void sendPulse(float level_V){
  // The output is set to the given value (8 bit resolution)
  analogWrite(outWire,VoltsToByte(level_V));
  // While the button is maintained, the output stays high
  while (ByteToVolts(analogRead(inWire1)) < seuil_max_V || ByteToVolts(analogRead(inWire2)) < seuil_max_V){
    delay(50);
  }
  digitalWrite(outWire,0);
}

void blink(int nbBlink, int blinkDuration_ms){
  int i;
  for (i=0;i<=nbBlink;i++){
    digitalWrite(led,false);
    delay((blinkDuration_ms/nbBlink)/2);
    analogWrite(led,VoltsToByte(infoLed_V));
    delay((blinkDuration_ms/nbBlink)/2);
  }
}

void autoCal(){
  int i;
  bool cmdFound = false;
  float wire1Read;
  float wire2Read;

  seuil_max_V = getNormalVoltage();

  // The Led will return to full brightness after blink
  infoLed_V = 5;
  
  for (i=0;i<8;i++){

    while (cmdFound == false) {
      wire1Read = ByteToVolts(analogRead(inWire1));
      wire2Read = ByteToVolts(analogRead(inWire2));

      if (wire1Read <= seuil_max_V && digitalRead(confgBP)) {
        triggers_V[i] = wire1Read;
        trig_Wires[i] = false;
        cmdFound = true;
      }
      else if (wire2Read <= seuil_max_V && digitalRead(confgBP)){
        triggers_V[i] = wire2Read;
        trig_Wires[i] = true;
        cmdFound = true;
      }
      else {
        blink(4,500);
      }
    }
    blink(1,1);
    cmdFound = false;
    delay(500);
  }

  cmdFound = false;
  float voltage = 2.5;
  for (i=0;i<8;i++){
    while (!cmdFound) {
      switch (getRmtCmd()) {
        case VOLM:
          voltage-=tolerance_V;
        break;
        case VOLP:
          voltage+=tolerance_V;
        break;
        case NEXT:
          voltage-=4*tolerance_V;
        break;
        case PREV:
          voltage+=4*tolerance_V;
        break;
        case FNC1:
          sendPulse(voltage);
        break;
        case FNC2:
          cmdFound = true;
        break;
      }
    analogWrite(led,VoltsToByte(voltage));
    }
    actions_V[i] = voltage;
    cmdFound = false;
  }

  writeConfig();
}

int getRmtCmd(){
  int i;
  bool cmdFound = false;
  
  float wire1Read;
  float wire2Read;

  float min_V;
  float max_V;

  while (!cmdFound){
    wire1Read = ByteToVolts(analogRead(inWire1));
    wire2Read = ByteToVolts(analogRead(inWire2));

    for (i=0;i<8;i++){
      min_V = triggers_V[i] - tolerance_V;
      max_V = triggers_V[i] + tolerance_V;

      if ((trig_Wires[i] == 0 && wire1Read > min_V && wire1Read < max_V) || (trig_Wires[i] == 1 && wire2Read > min_V && wire2Read < max_V)) {
        cmdFound = true;
        break;
      } 
    }
  }
  return i;
}

void writeConfig(void){
  int i;
  // Vseuil
  EEPROM.put(VSEUIL_BYTE,seuil_max_V);
  // Voltages
  for (i=0;i<8;i++){
    EEPROM.put(VOLTAGES_BYTE+4*i,triggers_V[i]);
  }
  // Wires data
  EEPROM.write(WIRES_BYTE,toByte(trig_Wires));
  // Finally the magic word to validate the process
  EEPROM.put(MWORD_BYTE,MAGICWORD);
}

unsigned char toByte(bool b[8])
{
    unsigned char c = 0;
    for (int i=0; i < 8; ++i)
        if (b[i])
            c |= 1 << i;
    return c;
}

void fromByte(unsigned char c, bool b[8])
{
    for (int i=0; i < 8; ++i)
        b[i] = (c & (1<<i)) != 0;
}

double getNormalVoltage(){
  unsigned long beginTime = millis();
  double returnVoltage = 6; // The return voltage is put over the maximal measurable voltage to force the first reading
  double readedVoltage1;
  double readedVoltage2;

  // The led is on during the process
  digitalWrite(led,true);

  // The returned voltage is averaged during half a second
  while ((millis() - beginTime) < 500){
    readedVoltage1 = ByteToVolts(analogRead(inWire1));
    readedVoltage2 = ByteToVolts(analogRead(inWire2));
    
    if (readedVoltage1 < returnVoltage) {
      returnVoltage = readedVoltage1;
    }
    else if (readedVoltage2 < returnVoltage) {
      returnVoltage = readedVoltage2; 
    }
  return returnVoltage;  
  }
  digitalWrite(led,false);
  return returnVoltage;
}

int VoltsToByte(double voltage){
  return (int)(((double)voltage/5)*1024)/4;
}

double ByteToVolts(int byteToConvert){
  return ((double)(byteToConvert)/(double)1024)*5;
}
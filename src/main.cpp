#include <EEPROM.h>

// ------------------- DEFINES ----------------------
#define inWire1 A2  // 4
#define inWire2 A3  // 3
#define confgBP 2   // 2
#define outWire 1   // 1
#define led     0   // 0


#define seuil_min_V 1.5 // The voltage under which no button is pressed

#define trig_inRes_Ohm 470 // The trigger resistance and voltage
#define in_supplyVoltage 5

#define out_inRes_Ohm 1000 // The output resistance and voltage
#define out_supplyVoltage 12 

#define tolerance_V 0.1

#define blinkDuration_ms 1000

enum {VOLM, VOLP, NEXT, PREV, FNC1, FNC2, FNC3, FNC4};

// --------------------- DATA -----------------------
// Represents the resistance of each button
// WIRE1: VOL- | VOL + | PREV | NEXT
// WIRE2: MEM- | MEM+  | MUTE | MODE
float triggers_V[8];
bool trig_Wires[8];

// Because the readed voltage is inversly proportional to the trigger resistance,
//  The loops that will use triggers_V will go from the last element to the first one.

float actions_V[8] = {
              // _______________________
              // | OUT_K | FUNC | IN_R |
              // |_____________________|
  2.18,       // | 20    | VOL- |  10  |
  1.86,       // | 13.5  | VOL+ |  57  |
  1.43,       // | 7.5   | NEXT | 125  |
  1.63,       // | 10    | PREV | 285  |
  0.66,       // | 0.175 | SRCE |  10  |
  1.24,       // | 5.5   | DISP |  57  |
  1.24,       // | 5.5   | DISP | 125  |
  0.89        // | 2.5   | PLAY | 285  |
  };          // |_____________________|

// ------------------ PROTOTYPES --------------------

void sendPulse(float level_V);
void autoCal(void);
void blink(int nbBlink);
int getRmtCmd(void);
void writeConfig(void);
unsigned char toByte(bool b[8]);
void fromByte(unsigned char c, bool b[8]);


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
      EEPROM.put(0,0);
    } 
  }
  EEPROM.get(0,magicWord);
  if (magicWord != 101){
    autoCal();
  }
  else {
    // Voltages
    for (i=0;i<8;i++){
      EEPROM.get(2+i*4,retreivedVoltage);
      triggers_V[i] = retreivedVoltage;
    }
    // Wires
    fromByte(EEPROM.read(34),trig_Wires);
  }
}

// -------------------- LOOP -----------------------
void loop() {
  int i;
  float lastVoltage;

  float readedVoltage1 = (analogRead(inWire1)/(float)1024)*5;
  float readedVoltage2 = (analogRead(inWire2)/(float)1024)*5;

  bool triggered = false;
  
  // Wire1 Actions
  lastVoltage = seuil_min_V;
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
    lastVoltage = seuil_min_V;
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
  analogWrite(outWire,((level_V/(float)5)*(float)1024)/4);
  // While the button is maintained, the output stays high
  while (analogRead(inWire1) > seuil_min_V || analogRead(inWire1) > seuil_min_V){
    delay(50);
  }
  digitalWrite(outWire,0);
}

void blink(int nbBlink){
  int i;
  for (i=0;i<=nbBlink;i++){
    delay((blinkDuration_ms/nbBlink)/2);
    digitalWrite(led,true);
    delay((blinkDuration_ms/nbBlink)/2);
    digitalWrite(led,false);
  }
}

void autoCal(){
  int i;
  bool cmdFound = false;
  float wire1Read;
  float wire2Read;

  for (i=0;i<8;i++){

    while (cmdFound == false) {
      wire1Read = ((float)analogRead(inWire1)/(float)1024)*5;
      wire2Read = ((float)analogRead(inWire2)/(float)1024)*5;

      if (wire1Read >= seuil_min_V && digitalRead(confgBP)) {
        triggers_V[i] = wire1Read;
        trig_Wires[i] = false;
        cmdFound = true;
      }
      else if (wire2Read >= seuil_min_V && digitalRead(confgBP)){
        triggers_V[i] = wire2Read;
        trig_Wires[i] = true;
        cmdFound = true;
      }
      else {
        blink(8);
      }
    }
    blink(1);
    cmdFound = false;
    delay(1500);
  }

  cmdFound = false;
  float voltage = 2.5;
  for (i=0;i<8;i++){
    while (!cmdFound) {
      switch (getRmtCmd()) {
        case VOLM:
          voltage-=0.02;
        break;
        case VOLP:
          voltage+=0.02;
        break;
        case NEXT:
          voltage-=0.1;
        break;
        case PREV:
          voltage+=0.1;
        break;
        case FNC1:
          sendPulse(voltage);
        break;
        case FNC2:
          cmdFound = true;
        break;
      }
    blink(1);
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

  float min;
  float max;

  while (!cmdFound){
    wire1Read = ((float)analogRead(inWire1)/(float)1024)*(float)5;
    wire2Read = ((float)analogRead(inWire2)/(float)1024)*(float)5;

    for (i=0;i<8;i++){
      min = triggers_V[i] - tolerance_V;
      max = triggers_V[i] + tolerance_V;

      if ((trig_Wires[i] == 0 && wire1Read > min && wire1Read < max) || (trig_Wires[i] == 1 && wire2Read > min && wire2Read < max)) {
        cmdFound = true;
        break;
      } 
    }
  }
  return i;
}

void writeConfig(void){
  int i;
  // Voltages
  for (i=0;i<8;i++){
    EEPROM.put(2+4*i,triggers_V[i]);
  }
  // Wires data
  EEPROM.write(34,toByte(trig_Wires));
  // Finally the magic word to validate the process
  EEPROM.put(0,101);
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


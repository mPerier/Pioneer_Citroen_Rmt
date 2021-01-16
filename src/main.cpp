#include <Arduino.h>

// ------------------- DEFINES ----------------------
#define inWire1 A0
#define inWire2 A1
#define outWire 10

#define seuil_min 1.5 // The voltage under which no button is pressed

#define trig_inRes_Ohm 470 // The trigger resistance and voltage
#define in_supplyVoltage 5

#define out_inRes_Ohm 1000 // The output resistance and voltage
#define out_supplyVoltage 12 


// --------------------- DATA -----------------------
// Represents the resistance of each button
// WIRE1: VOL- | VOL + | PREV | NEXT
// WIRE2: MEM- | MEM+  | MUTE | MODE
const unsigned int triggers_ohm[4] = {10,57,125,285};

// Because the readed voltage is inversly proportional to the trigger resistance,
//  The loops that will use triggers_V will go from the last element to the first one.
float triggers_V[4];

const float actions_V[8] = {
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


// -------------------- SETUP -----------------------
void setup() {
  int i;
  pinMode(inWire1,INPUT);
  pinMode(inWire2,INPUT);
  pinMode(outWire,OUTPUT);

  Serial.begin(9600);

  // Init Triggers_V
  Serial.println("TRIGGERS:");
  for (i=0;i<4;i++) {
    triggers_V[i] = (float)in_supplyVoltage * ((float)trig_inRes_Ohm / ((float)triggers_ohm[i] + (float)trig_inRes_Ohm));
    Serial.println(triggers_V[i]);
  }
}

// -------------------- LOOP -----------------------
void loop() {
  int i;
  float lastVoltage;
  char msg[50];

  float readedVoltage1 = (analogRead(inWire1)/(float)1024)*5;
  float readedVoltage2 = (analogRead(inWire2)/(float)1024)*5;

  bool triggered = false;
  
  // Wire1 Actions
  lastVoltage = seuil_min;
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
    lastVoltage = seuil_min;
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
  while (analogRead(inWire1) > seuil_min || analogRead(inWire1) > seuil_min){
    delay(50);
  }
  digitalWrite(outWire,0);
}
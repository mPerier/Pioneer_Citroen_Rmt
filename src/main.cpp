#include <Arduino.h>

// ------------------- DEFINES ----------------------
#define inWire1 A0
#define inWire2 A1
#define outWire 3

#define seuil_min 2.5 // The voltage under which no button is pressed

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

const unsigned int actions_ohm[8] = {
              // _________________________
              // | OUT_V | FUNC | IN_OHM |
              // |_______________________|
  20000,      // | 4.000 | VOL- |   10   |
  13500,      // | 5.106 | VOL+ |   57   |
  7500,       // | 6.857 | NEXT |  125   |
  10000,      // | 6.000 | PREV |  285   |
  175,        // | 11.79 | SRCE |   10   |
  5500,       // | 7.742 | DISP |   57   |
  5500,       // | 7.742 | DISP |  125   |
  2500        // | 9.600 | PLAY |  285   |
  };          // |_______________________|

// 
unsigned int actions_V[8];


// ------------------ PROTOTYPES --------------------

void sendPulse(unsigned int level);


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
  Serial.println("ACTIONS:");
  // Init action_V
  for (i=0;i<8;i++){
    actions_V[i] = ((float)out_supplyVoltage * ((float)out_inRes_Ohm / ((float)actions_ohm[i] + (float)out_inRes_Ohm))/(float)5)*1024;  
    Serial.println(actions_V[i]);
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
  lastVoltage = seuil_min;
  for (i=3;i>=0;i--){
    // If the readed voltage is inside one of the ranges
    if (readedVoltage1 > lastVoltage && readedVoltage1 < triggers_V[i]){
      // We send the pulse corresponding to the action ID i
      sendPulse(actions_V[i]);
      triggered = true;
      break;
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
        break;
      }
      lastVoltage = triggers_V[i];
    }
  }

}


// ------------------ FUNCTIONS --------------------
void sendPulse(unsigned int level){
  // The output is set to the given value
  analogWrite(outWire,level);
  // While the button is maintained, the output stays high
  while (analogRead(inWire1) > seuil_min || analogRead(inWire1) > seuil_min){
    delay(50);
    Serial.println("Waiting for button release...");
  }
  analogWrite(outWire, 0);
  Serial.print("--- Sended: [");
  Serial.print(((float)level/(float)1024)*5);
  Serial.println("]V ---");
}
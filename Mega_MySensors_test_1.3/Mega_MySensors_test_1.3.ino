// Example sketch showing how to control physical relays. 

#include <Relay.h>
#include <SPI.h>
#include <EEPROM.h>  
#include <RF24.h>

#define RELAY_1  13  // Arduino Digital I/O pin number for first relay (second on pin+1 etc)
#define NUMBER_OF_RELAYS 1
#define RELAY_ON 1
#define RELAY_OFF 0


const int can_group_pin_count = 12;
const int can_group_pins[] = {23,25,27,29,22,24,26,28,31,33,35,30};
const int stairs_pin = 37;
const int porch_pin = 32;

const int dev_id_cans = 101;
const int dev_id_stairs = 112;
const int dev_id_porch = 114;


// Alternate pins for Mega
Sensor gw(49,53);


void setup()  
{
  
  gw.begin();

  // Send the sketch version information to the gateway and Controller
  gw.sendSketchInfo("LR Mega", "1.0");

  // Register all sensors to gw (they will be created as child devices)
  for (int i=0; i<NUMBER_OF_RELAYS;i++) {
    gw.sendSensorPresentation(RELAY_1+i, S_LIGHT);
  }
  // Fetch relay status
  for (int i=0; i<NUMBER_OF_RELAYS;i++) {
    // Make sure relays are off when starting up
    digitalWrite(RELAY_1+i, RELAY_OFF);
    // Then set relay pins in output mode
    pinMode(RELAY_1+i, OUTPUT);   
      
    // Request/wait for relay status
    gw.getStatus(RELAY_1+i, V_LIGHT);
    setRelayStatus(gw.getMessage()); // Wait here until status message arrive from gw
  }
  
  
  // SSR setup
  for (int ii=0; ii<can_group_pin_count;ii++) {
    digitalWrite(can_group_pins[ii],0);
    pinMode(can_group_pins[ii], OUTPUT);
  }
  digitalWrite(stairs_pin,0);
  pinMode(stairs_pin, OUTPUT);
  digitalWrite(porch_pin,0);
  pinMode(porch_pin, OUTPUT);
  
  gw.sendSensorPresentation(dev_id_cans, S_LIGHT);
  gw.sendSensorPresentation(dev_id_stairs, S_LIGHT);
  gw.sendSensorPresentation(dev_id_porch, S_LIGHT);
  
  gw.getStatus(dev_id_cans, V_LIGHT);
  setRelayStatus(gw.getMessage());
  gw.getStatus(dev_id_stairs, V_LIGHT);
  setRelayStatus(gw.getMessage());
  gw.getStatus(dev_id_porch, V_LIGHT);
  setRelayStatus(gw.getMessage());

}


/*
*  Example on how to asynchronously check for new messages from gw
*/
void loop() 
{
  if (gw.messageAvailable()) {
    message_s message = gw.getMessage(); 
    setRelayStatus(message);
  }
}

void setRelayStatus(message_s message) {
  if (message.header.messageType==M_SET_VARIABLE &&
      message.header.type==V_LIGHT) {
     
     // New/requested status
     int incomingRelayStatus = atoi(message.data);
     
     // Change relay state
     switch (message.header.childId) {
       case RELAY_1:
         digitalWrite(message.header.childId, incomingRelayStatus==1?RELAY_ON:RELAY_OFF);
         break;
       case dev_id_cans:
         for (int ii=0; ii<can_group_pin_count; ii++) {
           digitalWrite(can_group_pins[ii], incomingRelayStatus==1?RELAY_ON:RELAY_OFF);
         }
         break;
       case dev_id_stairs:
         digitalWrite(stairs_pin, incomingRelayStatus==1?RELAY_ON:RELAY_OFF);
         break;
       case dev_id_porch:
         digitalWrite(porch_pin, incomingRelayStatus==1?RELAY_ON:RELAY_OFF);
         break;
     
     // Write some debug info
     Serial.print("Incoming change for light ID: ");
     Serial.print(message.header.childId);
     Serial.print(", New status: ");
     Serial.println(incomingRelayStatus);
   }
}

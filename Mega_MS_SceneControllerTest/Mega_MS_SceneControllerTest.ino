// Simple SceneController test

#include <MySensor.h>
#include <SPI.h>
#include <Wire.h>
#include <PCA9555.h>


#define SN "SceneControllerTest"
#define SV "1.1"
#define DEV_NODE_ID 198

#define CHILD_ID 1
#define LED_PIN 13
#define NUM_BUTTONS 4

// Devices
MySensor gw(49,53);
PCA9555 inxp(0x00);

// Messages
MyMessage msgSceneOn(CHILD_ID,V_SCENE_ON);
MyMessage msgSceneOff(CHILD_ID,V_SCENE_OFF);

// Global vars
bool status;
bool sceneState[NUM_BUTTONS];


void setup()  
{
    for (int ii=0; ii<NUM_BUTTONS; ii++) sceneState[ii] = 0;
    
    pinMode(LED_PIN, OUTPUT);

    Serial.println( SN ); 
    gw.begin( incomingMessage, DEV_NODE_ID, false );
    gw.sendSketchInfo(SN, SV);
    
    gw.present(CHILD_ID, S_SCENE_CONTROLLER);

    Wire.begin();
    status = inxp.begin();
    status &= inxp.setPolarity(0xFFFF);
    digitalWrite(LED_PIN, status);

}


//  Check if digital input has changed and send in new value
void loop() 
{
    gw.process();
    
    // Get any changed I/Os
    status = inxp.read();
    word changed = inxp.getChanged();
    word value = inxp.getValues();
    bool thisBit, rising;
    
    for (int button=0; button < NUM_BUTTONS; button++){
    
        thisBit = ((changed & ((word)0x0001 << button)) != 0);
        rising = ((value & ((word)0x0001 << button)) != 0);
        
        if (thisBit && rising){
        
            Serial.print("Pressed: ");
            Serial.println(button);
            
            if (sceneState[button] == 0){
                gw.send(msgSceneOn.set(button));
                sceneState[button] = 1;
            } else {
                gw.send(msgSceneOff.set(button));
                sceneState[button] = 0;
            }
        }
    }
}


void incomingMessage(const MyMessage &message) {
    Serial.print("Incoming Message. Type=");
    Serial.print(message.type);
    Serial.print(" Data=");
    Serial.println(message.data);
}
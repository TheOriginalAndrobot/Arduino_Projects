/*
 * Copyright (C) 2014 Andy Swing
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * DESCRIPTION
 * Master lighting control unit for the living room.
 *
 * This code is designed for an Arduino Mega
 *
 */

#include <MySensor.h>
#include <SPI.h>
#include <Wire.h>
#include <PCA9555.h>
#include <ButtonTracker.h>


#define SKETCH_NAME     "LivingRoomController"
#define SKETCH_VERSION  "1.3"
#define DEV_NODE_ID     200

// Uncomment to pull boot up SSR values from controller
//#define LRC__RESTORE_SSRS_FROM_GW


//
// Pin mapping
//
const int LED_PIN = 13;
// I/O expander
const int IOXP_RST_PIN = 48;
const int IOXP_INT_PIN = 19;
const int IOXP_INT_NUM = 4;     // Pin 19 is int.4 on Mega
const int IOXP_NUM_INPUTS = 12;  // # of inputs to process, starting at 1st input
const int IOXP_SCENE_HELD_OFFSET = 100; // Scene number offset for held button
const int IOXP_SCENE_NUMBER_MAP[IOXP_NUM_INPUTS] = {1,1,2,2,3,3,4,5,6,6,7,7};
const bool IOXP_SCENE_TYPE_MAP[IOXP_NUM_INPUTS]  = {1,0,1,0,1,0,1,1,1,0,1,0};
// SSR number (index) to pin (value)
const int SSR_COUNT = 16;
const int SSR_PINS[SSR_COUNT]  = {23,25,27,29,22,24,26,28,31,33,35,37,30,32,34,36};
const bool SSR_BOOT[SSR_COUNT] = { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
// PWM number (index) to pin (value)
const int PWM_COUNT = 5;
const int PWM_PINS[PWM_COUNT] = {5,6,7,8,9};


//
// Child ID mapping
//
const int SSR_BASE_ID = 1;
const int PWM_BASE_ID = 50;
const int SCENE_BASE_ID = 100;

//
// Create devices
//
MySensor gw(49,53);
PCA9555 inxp(0x00);

//
// Default messages
//
MyMessage msgDimmer(0, V_DIMMER);
MyMessage msgLight(0, V_LIGHT);
MyMessage msgSceneOn(SCENE_BASE_ID, V_SCENE_ON);
MyMessage msgSceneOff(SCENE_BASE_ID, V_SCENE_OFF);

//
// Global vars
//
volatile bool ioxpNeedsReading = false;
ButtonTracker sceneButtons[IOXP_NUM_INPUTS];


void setup(){
    bool ioxpStatus;

    // Pin setup & init
    setupPins();

    // I/O expander set up
    Wire.begin();
    resetIOXP();
    ioxpStatus = inxp.begin();
    ioxpStatus &= inxp.setPolarity(0xFFFF);

    // MySensors sketch/node init
    Serial.println( SKETCH_NAME );
    gw.begin(incomingMessage, DEV_NODE_ID, true);
    gw.sendSketchInfo(SKETCH_NAME, SKETCH_VERSION);

    // MySesnors present devices
    presentAllDevices();
    delay(10);

    // Restore light values from the gateway
    requestCurrentValuesFromGW();
    
    // Enable IOXP interrupts to allow sending events
    attachInterrupt(IOXP_INT_NUM, ioxpISR, FALLING);

    // Setup done, set status light
    digitalWrite(LED_PIN, 1);
}


// Main processing loop
void loop()
{

    // process incoming messages (like config from server)
    gw.process();

    // Service IOXP inputs
    if(ioxpNeedsReading){
        ioxpNeedsReading = false;   // Set early to catch more
        processIOXPInputs();
    }

}


// Pin setup & init
void setupPins(){
    int ii;

    // Initialize outputs

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, 0);

    pinMode(IOXP_RST_PIN, OUTPUT);
    digitalWrite(IOXP_RST_PIN, 0);

    for(ii=0; ii<SSR_COUNT; ii++){
        pinMode(SSR_PINS[ii], OUTPUT);
        digitalWrite(SSR_PINS[ii], 0);
    }

    for(ii=0; ii<PWM_COUNT; ii++){
        pinMode(PWM_PINS[ii], OUTPUT);
        digitalWrite(PWM_PINS[ii], 0);
    }


    // Initialize inputs

    pinMode(IOXP_INT_PIN, INPUT);
}


// Send reset pulse to the IO Expander
void resetIOXP(){
    digitalWrite(IOXP_RST_PIN, 0);
    delay(1);
    digitalWrite(IOXP_RST_PIN, 1);
    delay(1);
}


// Present all devices to the gateway
void presentAllDevices(){
    int ii;
    int childId;

    // SSRs (binary lights)
    for(ii=1; ii<=SSR_COUNT; ii++){
        childId = SSR_BASE_ID + (ii-1);
        gw.present(childId, S_LIGHT);
    }

    // PWMs (dimmable lights)
    for(ii=1; ii<=PWM_COUNT; ii++){
        childId = PWM_BASE_ID + (ii-1);
        gw.present(childId, S_DIMMER);
    }

    // Scene controller
    gw.present(SCENE_BASE_ID, S_SCENE_CONTROLLER);
}

// Pull gateway's current value for lights, in order to restore upon node boot.
// Responses are handled by incommingMessage just like any other command.
void requestCurrentValuesFromGW(){
    int childID;
    int index;

    // SSRs
#ifdef LRC__RESTORE_SSRS_FROM_GW
    for (index=0; index<SSR_COUNT; index++){
        childID = SSR_BASE_ID + index;
        gw.request(childID, V_LIGHT);
        // Make sure we service enough incoming messages to prevent overflow
        delay(50);
        gw.process();
    }
#else
    // Set SSRs to manual boot-up value
    for (index=0; index<SSR_COUNT; index++){
        setLight((SSR_BASE_ID+index),SSR_BOOT[index], true);
    }
#endif

    // PWMs
    for (index=0; index<PWM_COUNT; index++){
        childID = PWM_BASE_ID + index;
        gw.request(childID, V_DIMMER);
        // Make sure we service enough incoming messages to prevent overflow
        delay(50);
        gw.process();
    }
}

// IOXP interrupt handler
void ioxpISR(){
    ioxpNeedsReading = true;
}

// Process any changed IOXP inputs
void processIOXPInputs(){
    bool status;
    unsigned int bitNum;
    word changes;
    word values;
    bool changed;
    bool value;
    int sceneNum;
    bool sceneType;
    MyMessage* msgPtr;

    // Get any changed I/Os
    status = inxp.read();
    changes = inxp.getChanged();
    values = inxp.getValues();

    // Error checking
    if (!status){
        // Bail out if there was trouble reading the inputs
        return;
    }

    // Step through each scene-enabled input one by one
    for (bitNum=0; bitNum < IOXP_NUM_INPUTS; bitNum++){

        // Parse info for current input bit
        changed = ((changes & ((word)0x0001 << bitNum)) != 0);
        value = ((values & ((word)0x0001 << bitNum)) != 0);

        // Did this scene input change?
        if (changed){

            // Update this button's tracker
            sceneButtons[bitNum].update(value);
            
            // Look up scene info
            sceneNum = IOXP_SCENE_NUMBER_MAP[bitNum];
            sceneType = IOXP_SCENE_TYPE_MAP[bitNum];
            
            // Decide message type
            if (sceneType == 1){
                msgPtr = &msgSceneOn;
            } else {
                msgPtr = &msgSceneOff;
            }
            
            // Short press
            if (sceneButtons[bitNum].wasShort()){
                gw.send(msgPtr->set(sceneNum));
            }
            // Long press
            else if (sceneButtons[bitNum].wasLong()){
                gw.send(msgPtr->set(sceneNum + IOXP_SCENE_HELD_OFFSET));
            }
        }
    }
}


// Handle incoming MySensors messages from the gateway
void incomingMessage(const MyMessage &message) {
    // Debug
    /*
    Serial.print("Incoming Message. Sensor=");
    Serial.print(message.sensor);
    Serial.print(" Type=");
    Serial.print(message.type);
    Serial.print(" Data=");
    Serial.println(message.data);
    */

    // Binary/dimmable lights
    if (message.type == V_LIGHT || message.type == V_DIMMER) {
        //  Retrieve the power or dim level from the incoming request message
        int requestedLevel = atoi( message.data );
        // Adjust incoming level if this is a V_LIGHT variable update [0 == off, 1 == on]
        requestedLevel *= ( message.type == V_LIGHT ? 100 : 1 );
        // Clip incoming level to valid range of 0 to 100
        requestedLevel = requestedLevel > 100 ? 100 : requestedLevel;
        requestedLevel = requestedLevel < 0   ? 0   : requestedLevel;
        // Change the light's value
        setLight(message.sensor, requestedLevel);
    }
}


// Set the correct light output
void setLight(byte childID, int level){
    setLight(childID, level, false);
}
void setLight(byte childID, int level, bool updateGW){
    int lightIndex;
    int pinNum;

    // ID is in SSR range
    if (childID >= SSR_BASE_ID && childID < (SSR_BASE_ID+SSR_COUNT)){
        lightIndex = childID - SSR_BASE_ID;
        pinNum = SSR_PINS[lightIndex];
        digitalWrite(pinNum, (level>0)?HIGH:LOW);
        // Inform gateway of the current SwitchPower1 value
        if(updateGW) gw.send(msgLight.setSensor(childID).set((level>0)?1:0));
    }
    // ID is in PWM range
    else if (childID >= PWM_BASE_ID && childID < (PWM_BASE_ID+PWM_COUNT)){
        lightIndex = childID - PWM_BASE_ID;
        pinNum = PWM_PINS[lightIndex];
        // TODO: replace with a nice fade effect
        analogWrite(pinNum, (int)(level / 100. * 255));
        // Inform gateway of the current SwitchPower1 and LoadLevelStatus value
        if(updateGW) gw.send(msgLight.setSensor(childID).set((level>0)?1:0));
        //gw.send(msgDimmer.setSensor(childID).set(level));
    }
}




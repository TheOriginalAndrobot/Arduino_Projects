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
#define SKETCH_VERSION  "1.1"
#define DEV_NODE_ID     200

//
// Pin mapping
//
const int LED_PIN = 13;
// I/O expander
const int IOXP_RST_PIN = 48;
const int IOXP_INT_PIN = 19;
const int IOXP_INT_NUM = 4;     // Pin 19 is int.4 on Mega
const int IOXP_NUM_SCENES = 8;  // # of scenes to process, starting at 1st input
const int IOXP_SCENE_START = 1; // Starts at scene #1
// SSR number (index) to pin (value)
const int SSR_PINS[17] = {0,23,25,27,29,22,24,26,28,31,33,35,37,30,32,34,36};
const int SSR_COUNT = 16;
// PWM number (index) to pin (value)
const int PWM_PINS[6] = {0,5,6,7,8,9};
const int PWM_COUNT = 5;

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
ButtonTracker sceneButtons[IOXP_NUM_SCENES];


void setup(){
    bool ioxpStatus;

    // Pin setup & init
    setupPins();
    
    // I/O expander set up
    Wire.begin();
    resetIOXP();
    ioxpStatus = inxp.begin();
    ioxpStatus &= inxp.setPolarity(0xFFFF);
    attachInterrupt(IOXP_INT_NUM, ioxpISR, FALLING);
    
    // MySensors sketch/node init
    Serial.println( SKETCH_NAME ); 
    gw.begin(incomingMessage, DEV_NODE_ID, true);
    gw.sendSketchInfo(SKETCH_NAME, SKETCH_VERSION);
    
    // MySesnors present devices
    presentAllDevices();
    
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


// Handle incoming MySensors messages from the gateway
void incomingMessage(const MyMessage &message) {
    Serial.print("Incoming Message. Type=");
    Serial.print(message.type);
    Serial.print(" Data=");
    Serial.println(message.data);
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
    for (bitNum=0; bitNum < IOXP_NUM_SCENES; bitNum++){
    
        // Parse info for current input bit
        changed = ((changes & ((word)0x0001 << bitNum)) != 0);
        value = ((values & ((word)0x0001 << bitNum)) != 0);
        
        // Did this scene input change?
        if (changed){
        
            // Update this button's tracker
            sceneButtons[bitNum].update(value);
            
            // Short press is a "scene on" event
            if (sceneButtons[bitNum].wasShort()){
                gw.send(msgSceneOn.set(IOXP_SCENE_START + bitNum));
            }
            // Long press is a "scene off" event
            else if (sceneButtons[bitNum].wasLong()){
                gw.send(msgSceneOff.set(IOXP_SCENE_START + bitNum));
            }
        }
    }
}

// Pin setup & init
void setupPins(){
    int ii;
    
    //
    // Initialize outputs
    //
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, 0);
    
    pinMode(IOXP_RST_PIN, OUTPUT);
    digitalWrite(IOXP_RST_PIN, 0);
    
    for(ii=1; ii<SSR_COUNT; ii++){
        pinMode(SSR_PINS[ii], OUTPUT);
        digitalWrite(SSR_PINS[ii], 0);
    }
    
    for(ii=1; ii<PWM_COUNT; ii++){
        pinMode(PWM_PINS[ii], OUTPUT);
        digitalWrite(PWM_PINS[ii], 0);
    }
    
    //
    // Initialize inputs
    //
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
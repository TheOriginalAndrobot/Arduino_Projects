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


#define SKETCH_NAME     "LivingRoomController"
#define SKETCH_VERSION  "1.1"
#define DEV_NODE_ID     200

//
// Pin mapping
//
const int LED_PIN = 13;
const int IOXP_RST_PIN = 48;
const int IOXP_INT_PIN = 19;
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
MyMessage msgSceneOn(0, V_SCENE_ON);
MyMessage msgSceneOff(0, V_SCENE_OFF);



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
    
    // Setup done, set status light
    digitalWrite(LED_PIN, 1);
}


// Main processing loop
void loop() 
{
    bool status;

    // process incoming messages (like config from server)
    gw.process();
    
    // Get any changed I/Os
    status = inxp.read();
    word changed = inxp.getChanged();
    word value = inxp.getValues();
    bool thisBit, rising;
    
    for (int button=0; button < 16; button++){
    
        thisBit = ((changed & ((word)0x0001 << button)) != 0);
        rising = ((value & ((word)0x0001 << button)) != 0);
        
        if (thisBit && rising){
        
            Serial.print("Pressed: ");
            Serial.println(button);
            
/*             if (sceneState[button] == 0){
                gw.send(msgSceneOn.set(button));
                sceneState[button] = 1;
            } else {
                gw.send(msgSceneOff.set(button));
                sceneState[button] = 0;
            } */
        }
    }
}


// Handle incoming MySensors messages from the gateway
void incomingMessage(const MyMessage &message) {
    Serial.print("Incoming Message. Type=");
    Serial.print(message.type);
    Serial.print(" Data=");
    Serial.println(message.data);
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
    for(ii=1; ii<SSR_COUNT; ii++){
        childId = SSR_BASE_ID + (ii-1);
        gw.present(childId, S_LIGHT);
    }
    
    // PWMs (dimmable lights)
    for(ii=1; ii<PWM_COUNT; ii++){
        childId = PWM_BASE_ID + (ii-1);
        gw.present(childId, S_DIMMER);
    }
    
    // Scene controller
    gw.present(SCENE_BASE_ID, S_SCENE_CONTROLLER);
}
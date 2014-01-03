/**************************************************************************                                                   
 Name        : remote_cmd                                                                                                         
 Version     : 0.1                                                                                                            
                                                                                                                              
 Copyright (C) 2014 Constantin Petra                                                                                          
                                                                                                                              
This program is free software; you can redistribute it and/or                                                                 
modify it under the terms of the GNU Lesser General Public                                                                    
License as published by the Free Software Foundation; either                                                                  
version 2.1 of the License, or (at your option) any later version.                                                            
                                                                                                                              
This program is distributed in the hope that it will be useful,                                                               
but WITHOUT ANY WARRANTY; without even the implied warranty of                                                                
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU                                                             
Lesser General Public License for more details.                                                                               
***************************************************************************/
#include <math.h>

#define FILTER_SIZE 20

const int temperature_pin = A0;
const int relay_pin =  A1;
const int batt_level_pin = A3;

int relay_state = 0;
float temperature;
float batt_voltage;

const float B = 3975.0;                  //B value of the thermistor

void setup() {
    // initialize serial communications at 9600 bps:
    Serial.begin(19200); 
    pinMode(relay_pin, OUTPUT);
    pinMode(batt_level_pin, INPUT);
    pinMode(temperature_pin, INPUT);
}

float get_temperature(void)
{
    float r;
    float t;
    int i;
    int val = 0;

    for (i = 0; i < FILTER_SIZE; i++) {
        val += analogRead(temperature_pin);
    }
    val /= FILTER_SIZE;
    r = (float) (1023 - val) * 10000 / val; //get the resistance of the sensor;
    t = 1 / (log(r / 10000.0) / B + 1 / 298.15) - 273.15;//convert to temperature;
    return t;
}

float get_batt(void)
{
    int val = 0;
    float voltage;
    int i;
    for (i = 0; i < FILTER_SIZE; i++) {
        val += analogRead(batt_level_pin);
    }
  
    val /= FILTER_SIZE;
  
    voltage = (float)(val * 5.24) / 1083.0f * ((2200.0f + 820.0f) / 820.0f * 12.7 / 12.08 );
    return voltage;
}

void set_relay(int state)
{
    relay_state = state;
    digitalWrite(relay_pin, relay_state == 0? LOW : HIGH);
}

static void print_state(void)
{
   Serial.print("T: ");
   Serial.print(temperature);  
   Serial.print(" V: ");
   Serial.print(batt_voltage);   
   Serial.print(" R: "); 
   Serial.println(relay_state);            
 
}

void loop() {
    float t;
    int v;
    if (Serial.available() > 0) {
        v = Serial.read();
        switch (v) {
        case 'r':
            temperature = get_temperature();
            batt_voltage = get_batt();
            print_state();     
            break;
        case 'n':
            set_relay(1);
            break;
        case 'f':
            set_relay(0);
            break;
        default:
          break;
        } 
    }
}


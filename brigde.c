#include "bridge.h"
#include <pthread.h>
#include <stdbool.h>

#define LEFT 0
#define RIGHT 1
static bool vehicle_present = false;
static int Waiting[2]; // number of vehicles waiting on each side

void initBridge(){
    // code to initialize the bridge
    vehicle_present = false;
    Waiting[LEFT] = 0;
    Waiting[RIGHT] = 0;
}

void depart(){
    // code to start departure from one side of the bridge
}

void exitBridge(){
    // code to exit the bridge
}
#include <SPI.h>
#include "DAC_MCP49xx.h"
#include <i2c_t3.h>

// Teensy's max and min macros use non-standard gnu extensions... these are simpler for integers etc.
#define simple_max(a,b) (((a)>(b)) ? (a) : (b))
#define simple_min(a,b) (((a)<(b)) ? (a) : (b))

#define CS_FLASH        2
#define CS_DAC0		    7
#define CS_DAC1		    8
#define LATCH_PIN	    9
#define CS_SD           10

#define THROTTLE0_PIN	A0
#define THROTTLE1_PIN	A1
#define STEERING0_PIN   A2
#define STEERING1_PIN   A3

#define ENC_TO_RPM		150000

#define LEFT_ENC_PIN	5
#define RIGHT_ENC_PIN	6

#define POLLING_TIME	5000  // 5ms
#define RPM_TIME		5000  // 5ms

#define WHEELBASE_IN    72      // In Inches
#define REAR_TRACK_IN   60      // In inches

#define DIFFERENTIAL_MODE 0



// Comment or remove these definitions to stop respective debug code from being compiled
#define DEBUG_THROTTLE
//#define DEBUG_RPM

DAC_MCP49xx dac0(DAC_MCP49xx::MCP4921, CS_DAC0);
DAC_MCP49xx dac1(DAC_MCP49xx::MCP4921, CS_DAC1);

uint32_t lastTime, thisTime;

uint32_t omega_left;
uint32_t omega_right;
uint32_t omega_vehicle;

uint16_t leftThrottle;
uint16_t rightThrottle;

uint16_t steeringPot0;
uint16_t steeringPot1;

uint16_t throttleMin;
uint16_t throttleMax;
uint16_t throttleRange;
uint16_t requestedThrottle;

uint16_t steeringLeft;
uint16_t steeringRight;
uint16_t steeringCenter;

volatile uint32_t leftPulses;
volatile uint32_t rightPulses;

double leftSteer;
double rightSteer;

void setup()
{
    // Set pin modes
    pinMode(CS_FLASH, OUTPUT);
    pinMode(CS_DAC0, OUTPUT);
    pinMode(CS_DAC1, OUTPUT);
    pinMode(CS_SD, OUTPUT);
    pinMode(LATCH_PIN, OUTPUT);

    digitalWrite(CS_FLASH, HIGH);
    digitalWrite(CS_DAC0, HIGH);
    digitalWrite(CS_DAC1, HIGH);
    digitalWrite(CS_SD, HIGH);

    digitalWrite(LATCH_PIN, LOW);   // LOW if you want the DAC values to change immediately.

    // Attach functions to interrupts for the encoders
    attachInterrupt(digitalPinToInterrupt(LEFT_ENC_PIN), pulseLeft, RISING);
    attachInterrupt(digitalPinToInterrupt(RIGHT_ENC_PIN), pulseRight, RISING);

#if defined(DEBUG_THROTTLE) || defined(DEBUG_RPM)

    Serial.begin(115200);
    delay(1000);

#endif // DEBUG

    // Set Up DACs
    dac0.setSPIDivider(SPI_CLOCK_DIV16);
    dac1.setSPIDivider(SPI_CLOCK_DIV16);
    dac0.setPortWrite(false);
    dac1.setPortWrite(false);
    dac0.output(0);
    dac1.output(0);

    // Set up ADCs
    analogReadResolution(12);
    analogReadAveraging(8);

    // Sample the throttle pots, set that value as minimum
    throttleMin = analogRead(THROTTLE1_PIN);
    throttleRange = 1500;
    throttleMax = throttleMin + throttleRange;

#ifdef DEBUG_THROTTLE
    Serial.printf("Throttle Min:\t%d\n", throttleMin);
    delay(1000);
    Serial.printf("Throttle Max:\t%d\n", throttleMax);
#endif

    // Take a first time reading
    lastTime = micros();
}


void loop()
{
    if ((micros() - lastTime) >= POLLING_TIME)
    {
        cli();
        omega_left = leftPulses*ENC_TO_RPM / (POLLING_TIME);                                         // in RPM
        omega_right = rightPulses*ENC_TO_RPM / (POLLING_TIME);                                       // in RPM
        omega_vehicle = (simple_max(omega_left,omega_right) + simple_min(omega_left,omega_right)) / 2;

        leftPulses = 0;
        rightPulses = 0;
        sei();

        // Read Throttle && steering pots once every millisecond (+ .1ms / analogRead)
        lastTime = micros();
        steeringPot0 = analogRead(STEERING0_PIN);

        requestedThrottle = getUnsafeThrottle();

        if (requestedThrottle < 75)
            requestedThrottle = 0;

        switch (DIFFERENTIAL_MODE)
        {
        case 0:
            leftThrottle = requestedThrottle;
            rightThrottle = requestedThrottle;
            break;
        }

        // Write to the DACs
        dac0.output(leftThrottle);
        dac1.output(rightThrottle);

        lastTime = micros();
    }
}

void pulseLeft(){
    leftPulses++;
}

void pulseRight(){
    rightPulses++;
}

// Just get the average of the two throttles
int16_t getUnsafeThrottle()
{
    uint16_t throttlePot0;
    uint16_t throttlePot1;
    uint16_t throttle0;
    uint16_t throttle1;

    throttlePot0 = analogRead(THROTTLE0_PIN);
    throttlePot1 = analogRead(THROTTLE1_PIN);

#ifdef DEBUG_THROTTLE
    Serial.print("Throttle 0:\t");
    Serial.print(throttlePot0);
    Serial.print("\tThrottle 1:\t");
    Serial.print(throttlePot1);
#endif

    // Constrain throttleMin < throttle < throttleMax
    throttle0 = ((throttlePot0 < throttleMin) ? throttleMin : ((throttlePot0 > throttleMax) ? throttleMax : throttlePot0));
    throttle1 = ((throttlePot1 < throttleMin) ? throttleMin : ((throttlePot1 > throttleMax) ? throttleMax : throttlePot1));

    return (throttle0 + throttle1) / 2;
}

// Check for plausibility and agreement, otherwise return -1
int16_t getSafeThrottle()
{
    uint16_t throttlePot0;
    uint16_t throttlePot1;
    uint16_t throttle0;
    uint16_t throttle1;

    throttlePot0 = analogRead(THROTTLE0_PIN);
    throttlePot1 = analogRead(THROTTLE1_PIN);

#ifdef DEBUG_THROTTLE
    Serial.print("Throttle 0:\t");
    Serial.print(throttlePot0);
    Serial.print("\tThrottle 1:\t");
    Serial.print(throttlePot1);
#endif

    if (throttlePot0 > 3684 || throttlePot0 < 410){
        Serial.printf("Warning:  Throttle 0 out of range: %d", throttlePot0);
        return -1;
    }
    else if (throttlePot1 > 3684 || throttlePot1 < 410)
    {
        Serial.printf("Warning:  Throttle 1 out of range: %d", throttlePot1);
        return -1;
    }

    throttle0 = ((throttlePot0 < throttleMin) ? throttleMin : ((throttlePot0 > throttleMax) ? throttleMax : throttlePot0));
    throttle1 = ((throttlePot1 < throttleMin) ? throttleMin : ((throttlePot1 > throttleMax) ? throttleMax : throttlePot1));

    if ((simple_max(throttle0, throttle1) - simple_min(throttle0, throttle1)) > 410)
    {
        Serial.printf("Warning:  Throttle Mismatch!");
        return -1;
    }

    return (throttle0 + throttle1) / 2;     // Return average of the two throttles
}
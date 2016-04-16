// 
// 
// 

#include "DataLogger.h"
#include "SdFat.h"

#define error(s) sd.errorHalt(F(s))

SdFat sd;
SdFile file;

DataLogger::DataLogger(uint8_t chipSelect, uint8_t spiSpeed)
{
    this->chipSelect = chipSelect;
    this->spiSpeed = spiSpeed;
    numEntries = 0;
}

void DataLogger::init()
{

}

void DataLogger::begin(const char* fileName)
{
    // Make sure the SD Card exists
    if (!sd.begin(chipSelect, spiSpeed)) {
        sd.initErrorHalt();
    }

    int number = 0;
    char sName[80];
    
    // Find a filename that hasn't been used already
    do{
        sprintf(sName, "%s%d.csv", fileName, number++);
        Serial.println(sName);
    } while (!file.open(sName, O_CREAT | O_WRITE | O_EXCL));
    // Write the first line of the csv file
    writeHeader();
}

void DataLogger::addEntry(uint32_t time, uint16_t throttle, int16_t left, int16_t right, double steering, uint16_t speed){
    this->arrEntries[numEntries].time = time;
    this->arrEntries[numEntries].throttle = throttle;
    this->arrEntries[numEntries].left = left;
    this->arrEntries[numEntries].right = right;
    this->arrEntries[numEntries].steer = steering;
    this->arrEntries[numEntries].speed = speed;
    numEntries++;
    if (numEntries >= 10){
        logData();
        if (!file.sync() || file.getWriteError()) {
            error(F("write error"));
        }
    }

}

// Write the buffered data to the card
void DataLogger::logData()
{
    for (int i = 0; i < 10; i++)
    {
        file.printf("%d,%d,%d,%d,%0.2f,%d\n", arrEntries[i].time, arrEntries[i].throttle, arrEntries[i].left, arrEntries[i].right, arrEntries[i].steer, arrEntries[i].speed);
    }
    numEntries = 0;
}

// Write the first line of the 
void DataLogger::writeHeader() {
    file.printf(F("Millis,throttle,Left,Right,Steering Angle,Wheel Speed\n"));
    if (!file.sync() || file.getWriteError()) {
        error("write error");
    }
}
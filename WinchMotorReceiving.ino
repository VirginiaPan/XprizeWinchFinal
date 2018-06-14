/* Demonstrates the most basic example of RX and TX operation.
 *  See wiring diagrams to see how the nRF24 module whould be connected to each Arduinno *
Radio -> Arduino
CE    -> 9
CSN   -> 10 (Hardware SPI SS)
MOSI  -> 11 (Hardware SPI MOSI)
MISO  -> 12 (Hardware SPI MISO)
SCK   -> 13 (Hardware SPI SCK)
IRQ   -> No connection in this example
VCC   -> No more than 3.6 volts
GND   -> GND
*/

// the setup function runs once when you press reset or power the board
// To use VescUartControl stand alone you need to define a config.h file, that should contain the Serial or you have to comment the line

#include <VescUart.h>
//Include libraries copied from VESC
//#include "VescUart.h"
//#include "datatypes.h"
//#include Config.h out in VescUart.h

#include <SPI.h>
#include <NRFLite.h>

NRFLite _radio;
uint8_t _data;

  #define SERIALIO Serial1  
  #define DEBUGSERIAL Serial
  //NOTE ABOUT RPM: you can set RPM number, but it is not very accurate. Increasing RPM will lead to relatively higher speeds and vise, versa, although the number of RPMs you set it to is not accurate.

  //NOTE about posStart, posEnd, negStart, negEnd: these numbers may seem to be "flipped" but that is due to the way we mounted the motor. 
  //Giving the motor negative RPMs will cause it to let out line and giving the motor positive RPM will cause it to raise the line. 
  
  //for lowering winch
  #define negStart 0 //RPM to start 
  #define negLast -1.5 //RPM to end
  //for raising winch
  #define posStart 0 //RPM to end
  #define posLast 1.5 //RPM to start 
  #define DEBUG 0
  unsigned long count;
  unsigned long startTime;
  int go; 
  boolean stopAll = false;
  
void setup() {
	//Setup UART port
	Serial1.begin(9600);
  #ifdef DEBUG
	//Setup debug port
	Serial.begin(115200);
	#endif

  //RECEIVE START SIGNAL 
  if(_radio.init(0, 9, 10)) // radio id, CE pin, CSN pin
  //Should print if Uno can send message to Mega and Mega can receive it
    Serial.println("Radio init - success"); 
  else
  //Should print if Unoe cannot send of Mega cannot receive signal. If you receive this, check that both Arduinos have a power supply and that they are both correctly connected to their nRF24 modules. 
    Serial.println("Radio init - failure"); 
}

struct bldcMeasure measuredValues;
//Use these to set current limit to ensure that the motor does not pull too much current. 
//Were going to integrate this ass part of the ramping and sustaining functions, but did not have time and the motor worked without it at the speed we were winching at. 
float MaxCurrent = 12.0; 
float MinCurrent = -12.0; 
float pos; 
	
// the loop function runs over and over again until power down or reset
void loop() {  
  int go = comm(); 
  detMovement(go); 
  stopAll = false;
}

void detMovement(int go)
{
  if(go!=0)
  {
    //RUN MOTOR
    if(go ==98) //b for begin 
      { 
        Serial.println("Ready to Start");
        //data(); //can print out motor's data if you want to see what is going on (should be nothing at this point)
      }

    else if(go == 102) // f for forward; lets out line
      {
        float legOne = ramp(negStart,negLast,2500); //ramping up to let out line
        //prints number of revolutions, but unfortunately not very accurate so it is commented out
        // Serial.print("number of revolutions leg one"); Serial.println(legOne); 
        sustain(negLast,5000); //sustaining letting out line
        float legTwo = ramp(negLast,negStart,2500); //ramping down to let out line
        //prints number of revolutions, but unfortunately not very accurate so it is commented out
        //Serial.print("number of revolutions leg two"); Serial.println(legTwo);
      }

    else if(go == 114) //r for reverse; brings in line
      {
        float legThree = ramp(posStart,posLast,2500); //ramp up to bring line in
        //prints number of revolutions, but unfortunately not very accurate so it is commented out
        //Serial.print("number of revolutions leg three"); Serial.println(legThree); 
        sustain(posLast,5000); //sustain bringing line in
        float legFour = ramp(posLast,posStart,2500); //ramp down while bringing line in
        //prints number of revolutions, but unfortunately not very accurate so it is commented out
        //Serial.print("number of revolutions leg four"); Serial.println(legFour);   
      }
    else if(go == 112) //p for pause
    {
        Serial.println("Stopping!"); 
        pause(); 
    }
    
    else if(go == 101) //e for end  
    {
        Serial.println("All Done!"); 
        //data(); //can print out motor's data if you want to see what is going on (should be nothing at this point)
    }
     
  }
  else
  {
    //if you sent a character that does not mean anything to the motor
    Serial.println("Not a coded signal!"); 
  }
}

//Prints info about state of motor
void data()
{
  if (VescUartGetValue(measuredValues)) {
    Serial.print("avgMotorCurrent"); Serial.println(measuredValues.avgMotorCurrent);
    Serial.print("avgInputCurrent"); Serial.println(measuredValues.avgInputCurrent); 
    Serial.print("RPM"); Serial.println(measuredValues.rpm); 
  }
  else
  {
    Serial.println("Failed to get data!");
  }
}

//Sustains a given RPM for a period of time speccified in duration. Make sure you give both arguments as floats (number with a decimal) rather than just an int (ex 1.0 rather than 1) 
void sustain(float rpm, float duration){
  startTime = millis();
  while(millis() - startTime < duration){
        //checks to see if it is supposed to stop
        if(checkSerialStop() || stopAll)
        {
          pause();
          return;
        }
        //if not, procede to privide set RPM
        else
        {
          VescUartSetRPM(rpm);
        } 
  }
}

//Stops motor, signal this by typing in a 'p' in the serial monitor of your computer. Motor should stop and wait for further instructions. 
void pause()
{
  VescUartSetRPM(0); 
}

//used to ramp up and ramp down motor. Set the start RPM, end RPM and how long it should take from go from the start to end RPM (duration). Be sure to declare argumants as floats! 
float ramp(float start, float last, float duration)
{
  startTime = millis();
  while (millis()-startTime < duration){
    //check to see if you are supposed to stop 
    if(checkSerialStop() || stopAll)
    {
      pause();
      return;
    }
    //if not, continue to ramp 
    else
    {
       VescUartSetRPM((millis()-startTime)*1.0/duration*(last-start)+start);
    }
  }
  //Number being returned was supposed to be the number of revolutions it made while ramping up, but it is not very accurate
  return ((start+last)/2/60)*duration/1000; 
}

//Checks if incoming signal is a 'p' for pause and stops the motor if so
boolean checkSerialStop()
{
  if(_radio.hasData()) 
  {
    _radio.readData(&_data);
    if(_data == 112) // is it the ascii value for p?
    {
     stopAll = true;
     return true;  
    }
    else
    {
      return false; 
    }
}
}

//gets the signal sent from the computer/Arduino Uno 
int comm() 
{
  while (!_radio.hasData()){
    Serial.println("waiting for data...");
    delay(100);
  }
  while (_radio.hasData()) {
    _radio.readData(&_data);
    Serial.println(char(_data));
    if(_data == 98) //b
    {
      Serial.println("INITIALIZED! Ready to start running motor!"); 
      return _data; 
    }
    else if(_data == 102) //f
    {
      Serial.println("ROTATING FORWARD! LOWERING!"); 
      return _data; 
    }
    else if(_data == 114) //r
    {
      Serial.println("ROTATING BACKWARRD! RAISING!");
      return _data; 
    }
    else if(_data == 101) //e
    {
       Serial.println("ENDING! Done running motor!"); 
       return _data; 
    }
    else if(_data ==112) //p
    {
       Serial.println("Stopping!"); 
       return _data; 
    }
    else
    {
      return 0; 
    }
}
}


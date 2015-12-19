
/******************************************************************************************************/
// Open_Close_Tx:  monitor a contact and send a wireless code when the contact closes and then send
//  another wireless code when the contact opens.  The wireless code is a 24 bit code word + sync
//  that is compatible with the PT2262 and EV1527 protocols.  
//  
//  A 315 MHz or 433 MHz wireless transmitter is connected to Arduino digital pin 4.  A contact
//  is connected to Arduino digital pin 8.  Arduino internal pullup is used on the contact.
//
//  This software uses blocking code.  This is OK since it monitors only one sensor (contact).
//  This software has been bebugged and tested using RC-SWITCH.  
//  Use of this software is subject to the Terms of Use, which can be found at:
//    https://github.com/SISProject/SISDocs/blob/master/Terms_of_Use_License_and_Disclaimer.pdf
//
//  This software uses code extracted from the Arduino RC-Switch library:
//    https://github.com/sui77/rc-switch
// Portions of this software that have been extracted from RC-Switch are subject to the RC-Switch
// license, as well as the the SIS Terms of Use.
//
//
//  (c) 2015 by Bob Glicksman and Jim Schrempp
/******************************************************************************************************/
// Version 001.  Has two defined constants for the contact OPEN and CLOSE events.  Change these
//  contants and recompile to suit the code needs of any given system.

/**************************************** GLOBAL CONSTANTS ********************************************/
// #define DEBUG      //uncomment to use serial port to debug
const unsigned long OPEN_CODE = 321789ul;
const unsigned long CLOSE_CODE = 5810559ul;
const int TX_PIN = 4;                  // transmitter on Digital pin 4
const int LED_PIN = 13;
const int CONTACT_PIN = 8;             // pushbutton or other contact on Digital pin 8
const int BAUD_TIME = 500;             // basic signalling unit is 400 us

/**************************************** GLOBAL VARIABLES ********************************************/
boolean contactState = false;

/******************************************** setup() *************************************************/
void setup()
{
  pinMode(TX_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(CONTACT_PIN, INPUT);
  
  #ifdef DEBUG
    Serial.begin(9600);
  #endif
}
/***************************************** end of setup() *********************************************/

/******************************************** loop() **************************************************/
void loop()
{
  static boolean oldContactState = false;
  unsigned long codeWord;
  
  // test the contact for a valid reading and state change if a valid reading
  if ( (readContact() == true) && (oldContactState != contactState) )    // valid change reading
  {
    #ifdef DEBUG
      Serial.print("State change to: ");
    #endif
    
    // select code word to send -- open or close
    if(contactState == false)
    {
      codeWord = OPEN_CODE;
      #ifdef DEBUG
        Serial.println("OPEN");
      #endif
    }
    else
    {
      #ifdef DEBUG
        Serial.println("CLOSE");
      #endif
      codeWord = CLOSE_CODE;
    }
    
    // send the code word 20 times - 
    digitalWrite(LED_PIN, HIGH);
    for (int i = 0; i < 20; i++)
    {
      sendCodeWord(codeWord);
    }
    digitalWrite(LED_PIN, LOW);
    oldContactState = contactState;
  }
}
/***************************************** end of loop() **********************************************/

/**************************************** sendCodeWord() **********************************************/
// sendCodeWord():  sends a 24 bit code to the transmitter data pin.  The code is encoded according
//  to the EV1527 format, with a zero being one baud unit high and three baun units low, a one being
//  three baud units high and one baun unit low, and a sync being one baud unit high and 31 baud units
//  low.  The pattern is shifted out MSB first with SYNC at the end of the code word.
//
// Parameters:
//  code:  the code word to send (the 24 LSBs of an unsigned long will be sent)

void sendCodeWord(unsigned long code)
{
  const unsigned long MASK = 0x00800000ul;  // mask off all but bit 23
  const int CODE_LENGTH = 24; // a code word is 24 bits + sync
  
  // send the code word bits  
  for (int i = 0; i < CODE_LENGTH; i++)
  {
    if ( (code & MASK) == 0)
    {
      sendZero();
      #ifdef DEBUG
        Serial.print("0");
      #endif
    }
    else 
    {
      sendOne();
      #ifdef DEBUG
        Serial.print("1");
      #endif
    }      
    code = code <<1;
  }
  //send the sync
  sendSync();
  #ifdef DEBUG
    Serial.println(" SYNC");
  #endif
  
  return;
}
/************************************ end of sendCodeWord() *******************************************/

/****************************************** sendZero() ************************************************/
// sendZero:  helper function to encode a zero as one baud unit high and three baud units low.

void sendZero()
{
  // a zero is represented by one baud high and three baud low
  digitalWrite(TX_PIN, HIGH);
  delayMicroseconds(BAUD_TIME);
  for(int i = 0; i < 3; i++)
  {
      digitalWrite(TX_PIN, LOW);
      delayMicroseconds(BAUD_TIME);
  }  
  return; 
}
/************************************** end of sendZero() *********************************************/

/****************************************** sendOne() *************************************************/
// sendOne:  helper function to encode a one as three baud units high and one baud unit low.

void sendOne()
{
  // a one is represented by three baud high and one baud low
  for(int i = 0; i < 3; i++)
  {
      digitalWrite(TX_PIN, HIGH);
      delayMicroseconds(BAUD_TIME);
  } 
  digitalWrite(TX_PIN, LOW);
  delayMicroseconds(BAUD_TIME); 
  return; 
}
/*************************************** end of sendOne() *********************************************/

/***************************************** sendSync() *************************************************/
// sendSync:  helper function to encode a sync as one baud unit high and 31 baud units low.

void sendSync()
{
  // a sync is represented by one baud high and 31 baud low
  digitalWrite(TX_PIN, HIGH);
  delayMicroseconds(BAUD_TIME);
  for(int i = 0; i < 31; i++)
  {
      digitalWrite(TX_PIN, LOW);
      delayMicroseconds(BAUD_TIME);
  }  
  return; 
}
/************************************** end of sendSync() *********************************************/

/**************************************** readContact() ***********************************************/
// readContact():  function that reads a digital value (input with internal pullup) on the CONTACT_PIN.
//  The contact value is tested for bounce and noise and if it is valid, the funtion sets the global
//  variable contactState accordingly and returns true.  Otherwise, the gobal variable is not changed
//  and the function returns false.  This function is blocking.
//
// Return:  true of a valid contact value is read; false otherwise.  A valid contact value is declared
//  if the same value is read from the CONTACT_PIN in two successive reading separated by 10 ms debounce
//  time.

boolean readContact()
{
  boolean firstRead, secondRead;
  
  if(digitalRead(CONTACT_PIN) == HIGH)
  {
    firstRead = false;
  }
  else
  {
    firstRead = true;
  }
  
  delay(10);      // wait 10 ms for noise and bounce
  if(digitalRead(CONTACT_PIN) == HIGH)
  {
    secondRead = false;
  }
  else
  {
    secondRead = true;
  }
  
  // if the two reads are the same, proices as valid contact state
  if (secondRead == firstRead)
  {
    contactState = secondRead;
    return true;
  }
  else
  {
    return false;  // noise
  }
}
/************************************* end of readContact() *******************************************/


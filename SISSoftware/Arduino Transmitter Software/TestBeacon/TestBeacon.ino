
/******************************************************************************************************/
// TestBeacon:  Used to send one of two alternating codes. Useful for testing the SIS Hub receive code.
// The wireless code is a 24 bit code word + sync that is compatible with the PT2262 and EV1527 protocols.
//
//  A 315 MHz or 433 MHz wireless transmitter is connected to Arduino digital pin 4.
//
//  This software uses blocking code.  This is OK since it only sends one code at a time.
//  This software has been bebugged and tested using RC-SWITCH.
//
//  (c) 2015 by Bob Glicksman and Jim Schrempp
/******************************************************************************************************/
// Version 001.  Has two defined constants for the codes: CODE_ONE and CODE_TWO. DELAY_SECONDS is the
//  minimum time between code sends; it could be longer depending upon how long it takes for a
//  code to actually be sent.
//  When testing an SIS Hub, these codes should be registered as "door separation sensors." If the codes
//  are registered as PIRs, then receiving them is subject to a blackout period between successive
//  receipts of the same code.

/**************************************** GLOBAL CONSTANTS ********************************************/
// #define DEBUG      //uncomment to use serial port to debug
const int DELAY_SECONDS = 4;            // minimum seconds to wait between successive code sends
const unsigned long CODE_ONE = 95117ul;
const unsigned long CODE_TWO = 94070ul;
const int TX_PIN = 4;                  // transmitter on Digital pin 4
const int LED_PIN = 7;
const int BAUD_TIME = 500;             // basic signalling unit is 400 us

/**************************************** GLOBAL VARIABLES ********************************************/
unsigned long codeWord = CODE_ONE;
/******************************************** setup() *************************************************/
void setup()
{
  pinMode(TX_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  #ifdef DEBUG
    Serial.begin(9600);
  #endif
}
/***************************************** end of setup() *********************************************/

/******************************************** loop() **************************************************/
void loop()
{

    static time_t lastSendTime = 0;
    if (Time.now() - lastSendTime > DELAY_SECONDS)
    {
        lastSendTime = Time.now();

        if (codeWord == CODE_ONE) {
            codeWord = CODE_TWO;
        } else {
            codeWord = CODE_ONE;
        }
        // send the code word 20 times -
        digitalWrite(LED_PIN, HIGH);
        for (int i = 0; i < 20; i++)
        {
            sendCodeWord(codeWord);
        }
        digitalWrite(LED_PIN, LOW);

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

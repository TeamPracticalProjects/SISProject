//#define SEND_EVENTS_TO_WEBPAGE // if on will send the events needed for the
                                 // config web page to display live events
//#define TESTRUN
//#define DEBUG                   // turns on Serial port messages
//#define DEBUG_TRIP
//#define DEBUG_EVENT
//#define DEBUG_ADVISORY
//#define DEBUG_COMMANDS
#define photon044                 // when present, enables functions that only work with 0.4.4
#define CLOUD_LOG
//
// Temporary fix for I2C library issue with Photon
#ifdef PLATFORM_ID
    #include "i2c_hal.h"
#endif
/***************************************************************************************************/
// saratogaSIS: Test of SIS application to chronically-ill/elder care activity monitoring
//  in a controlled environment.
//
//  Version 08j.  10/13/15.  Spark Only.
const String VERSION = "S08j";   	// current firmware version
//
//  (c) 2015 by Bob Glicksman and Jim Schrempp
/***************************************************************************************************/
// version 08j - MAX_WIRELESS_SENSORS at 15 and changed MAX_PIR to 9 and MAX_DOOR to
//  13 and ALARM_SENSOR to 14.  Left the circular buffer (BUF_LEN) at 25.
// version 08j - added blinks in setup() to show progress.
// version 08i1 - bug fix to check return code of Spark.publish in publishCircularBuffer
// version 08i - Added new process to publish circular buffer events to the cloud only
//  every 2 seconds. Using global g_numToPublish to track how many cbuf events are left to
//  send. Also removed the lastGTrip functionality introduced in 08d; turns out IFTTT didn't
//  work the way we thought.
// Version 08h - added serial debug error message if spark.publish fails. Protected
//  by ifdef photon044
// Version 08g - New #define for photon v0.4.4-rc2. The Spark.timeSync() daily call works.
// Version 8.0f: Added reportFatalError to blink D7 to show error code. Used this
//  in setup() to indicate that the time never synced with the internet. Also added
//  a Spark.process() in the time code in case that will help.
//
// Version 8.0e: Added a global variable ALARM_SENSOR = 11. If a sensor is tripped
//  we test for this location. If this matches then we publish an SISAlarm. This
//  can be monitored by IFTTT to do something more agressive, such as dialing a phone.
//  The intent is for this to be associated with a button sensor.
//
// Version 8.0d: added particle public function lastGTrip() and a global lastGenericTrip.
//  When a sensor in a position above MAX_DOOR is tripped, that position is stored in
//  lastGenericTrip. When lastGTrip is called it will return the value of lastGenericTrip
//  and then set lastGenericTrip to 0. The intention is for IFTTT to call lastGTrip and
//  then decide to take an action based on the value returned.
//  This is a bit of a test. The long view is to have a function alertMe() that will keep a
//  pointer into the circular buffer and return a history of sensor trips each time it is
//  called.
// Version 08d: added MAX_DOOR constant. Sensors in posistions above MAX_DOOR are considered
//  generic sensors and don't affect PIR or DOOR algorithms. Sensors in these positions are just
//  added to the log. Also increased number of sensors from 10 to 15.
//
// Version 08c: added ".c_str()" to the end of line 1145 per a suggestion from Forum to fix the
//  Photon string early termination problem ( bufferReadout += Time.timeStr(index).c_str(); )
//
// Version 08b:  included #ifdef and #incude temporary fix for I2C library issue on Photon.
//  Uncommented Wire.setSpeed() with this fix.
//
// Version 08a:  had to comment out line 215: Wire.setSpeed(CLOCK_SPEED_100KHZ); and line 374:
//  Spark.SyncTime(); in order to get the code to compile for Photon.  It compiles OK for Core.
//  Also had to change the varible name "registrationInfo" to "registration" (line 253) and
//  "circularBufferReadout" to "circularBuff" (line 252) and now these variables are accessible by SIS
//  Javascript code.
//
// Version 08: (1) fixed the registrar function to return -1 if an unkonwn command is issued. (2)
//  altered readBuffer() to call an new function that supports both cloud and local circular
//  buffer reading.
//
// Version 07 -- skipped (testing)
//
// Version 6a -- version 6 caused each of my two test cores to reset, at very different times.  This
//  was even though I could not cause a reset by power cycling my cable modem.  This version reduces
//  the buffer size to 25, does not publish each sensor trip, but publishes recorded events.  This
//  version is for robustness testing of a possible operational scenario where the main logging
//  is via publication to the cloud with a small local buffer as a backup.
//
// Version 6 - put in changes requested by Jim to baseline version 5:
//  - "no movement" will only be logged when no sensor is tripped for a period of one hour.  Any
//      subsequent sensor trip (even the same sensor as was last tripped) will be logged to
//      re-establish the time when movement is again detected.
//  - retain the ability, based upon #define, to publish to the cloud each of the following: trip,
//      event, and advisory.
//  - change the return value of register() to be -1 for any failure (memo - was already there -
//      no change required.
//
// Version 5 - increase buffer size to 50 (then reduced to 40) for testing Core resets.
//  Also, enable the old notificationand enable the daily time sync.  This is OK with buffer size of 40
//  but can reset with buffer size of 50.
//
// Version 4 - tried to initialize buffers but had flash problems with cBuf of 100.
// Version 3jbsc - same as (b) but with circular buffer at 25
// Version 3jbsb - same as (a) but reduced circular buffer to 8 from 100
// Version 3jbsa - commented out the notification and also the internet time update. Lies 287 and 345
//
// Version 3jbs - includes code to pubish advisories, events, trips and commands to the cloud.  Used
//  for recording these things to a Google spreadsheet for test analysis purposes.  Comment out
//  the #defines at the top of the code as desired.
//
//Version 3 - support I2C eeprom
//
// Version 2 - fixed case where core is reset and person is home.
//
//Version 01 (branched off of wirelessSensorReceiver version 23 of 3/22/15).
//  This firmware supports two types of wireless sensors:  PIR and door contact sensors.  PIR
//  sensors must be registered in sensor locations 0 through MAX_PIR.  All sensors registered in
//  locations MAX_PIR onward are assumed to be door/window contact sensors.  All sensors are SIS
//  wireless types operating at either 315 MHz or 433 MHz and using PT2262 or EV1527 protocols.
//
// This test is designed to investigate use of these sensors to detect significant levels of
//  activity around a small home.  The sensor trip data will be filtered prior to logging
//  according to the following rules:
//  1.  All door sensor trips are logged.
//  2.  PIR sensor trips are logged as long as the last sensor tripped was not the same one.
//  3.  If a door sensor trips but no PIR sensor trips within 10 minutes, "No one is home"
//  	is logged.
//  4.  If a door sensor trips and a PIR sensor trips within 10 minutes, "Person is home"
//  	is logged.
//  5.  If no PIR sensor is tripped within one hour of the last PIR sensor trip, "NO
//  	movement" is logged.
//  6.  If two different PIR sensors trip within 2 seconds of each other, "Multiple
//  	persons" is logged and all logging is surpressed until a door sensor is tripped.
//  7.  All log entries are prepended with a one-up sequence number and all log entries are
//  	timestamped.
//
//
// Debugging via the serial port.  Comment the next line out to disable debugging mode
//#define DEBUG
/************************************* Global Constants ****************************************************/

const int INTERRUPT_315 = 3;   // the 315 MHz receiver is attached to interrupt 3, which is D3 on an Spark
const int INTERRUPT_433 = 4;   // the 433 MHz receiver is attached to interrupt 4, which is D4 on an Spark
const int WINDOW = 200;    	// timing window (+/- microseconds) within which to accept a bit as a valid code
const int TOLERANCE = 60;  	// % timing tolerance on HIGH or LOW values for codes
const unsigned long ONE_DAY_IN_MILLIS = 86400000L;	// 24*60*60*1000
const int MAX_WIRELESS_SENSORS = 20;
const unsigned long FILTER_TIME = 5000L;  // Once a sensor trip has been detected, it requires 5 seconds before a new detection is valid
const int BUF_LEN = 100;     	// circular buffer size.
const int MAX_PIR = 9;      	// PIR sensors are registered in loc 0 through MAX_PIR.  Locations MAX_PIR + 1 to
                            	//  MAX_WIRELESS_SENSORS are non-PIR sensors
const int MAX_DOOR = 14;       // Sensors > MAX_PIR and <= MAX_DOOR are assumed to be exit doors.
const int ALARM_SENSOR = 15;  // When this sensor is tripped, publish an SISAlarm
const int MAX_SUBSTRINGS = 6;   // the largest number of comma delimited substrings in a command string
const byte NUM_BLINKS = 2;  	// number of times to blink the D7 LED when a sensor trip is received
const unsigned long BLINK_TIME = 300L; // number of milliseconds to turn D7 LED on and off for a blink
const int CONFIG_BUFR = 32; 	// buffer to use to store and retrieve data from non-volatile memory
const String id = "SIS-2015";   // the ID value for a valid config in non volatile memory
const String DOUBLEQ = "\"";

//  For I2C eeprom configuration
const int VIRTUAL_DEVICE_SIZE = 4096;   // divide device up into virtual eeproms 4K bytes each
const int MAX_VIRTUAL_DEVICES = 8;  	// 32 K bytes device = 8 pages of 4K each
const int VIRTUAL_DEVICE_NUM = 0;   	// use the first virtual device - change if it wears out

// additional global constants for Saratoga SIS Test
const unsigned long MULTI_TIME = 2ul;   	// 2 second interval to detect multiple persons
const unsigned long AWAY = 600ul;       	// 10 * 60 = 600; ten minutes for declaring away
const unsigned long COMATOSE = 3600ul;  	// 60*60 = 3600; 1 hour for for declaring no movement
const String messages[] = {             	// additional log messages for this application
                        	"No one is home",
                        	"Person is home",
                        	"No movement",
                        	"Multiple persons"
                      	};
const byte UKN = 0;     	// unknown
const byte HOME = 1;    	// person is home
const byte NOT_HOME = 2;	// person is not home

/************************************* Global Variables ****************************************************/
volatile boolean codeAvailable = false;  // set to true when a valid code is received and confirmed
volatile unsigned long receivedSensorCode; // decoded 24 bit value for a received and confirmed code from a wireless sensor
volatile unsigned int codeTimes315[52];  // array to hold times (microseconds) between interrupts from transitions in the received data
                         	//  times[0] holds the value of the sync interval (LOW).  HIGH and LOW times for codes are
                         	//  stored in times[1] - times[49] (HIGH and LOW pairs for 24 bits).  A few extra elements
                         	//  are provided for overflow.
volatile unsigned int codeTimes433[52];  // array to hold times (microseconds) between interrupts from transitions in the received data
                         	//  times[0] holds the value of the sync interval (LOW).  HIGH and LOW times for codes are
                         	//  stored in times[1] - times[49] (HIGH and LOW pairs for 24 bits).  A few extra elements
                         	//  are provided for overflow.
volatile unsigned int *codeTimes;  // pointer to 315 MHz or 433 MHz codeTimes array based upon the interrupt.
time_t resetTime;       	// variable to hold the time of last reset

   	//	Sensor registration data is held in parallel arrays.
String sensorName[] =      	{ "FrontRoom PIR",
                             	"MasterBed PIR",
                             	"SecondBed PIR",
                             	"UNREGISTERED S3",
                             	"UNREGISTERED S4",
                             	"FrontDoor Sep",
                             	"GarageDoor Sep",
                             	"UNREGISTERED S7",
                             	"UNREGISTERED S8",
                             	"UNREGISTERED S9",
                             	"UNREGISTERED S10",
                             	"UNREGISTERED S11",
                             	"UNREGISTERED S12",
                             	"UNREGISTERED S13",
                             	"UNREGISTERED S14",
                             	"UNREGISTERED S15",
                             	"UNREGISTERED S15",
                             	"UNREGISTERED S15",
                             	"UNREGISTERED S15",
                             	"UNREGISTERED S15"
                           	};

unsigned long activateCode[] = { 86101,   // sensor 0 (door/window) activation code
                             	10878847,	// sensor 1 (PIR) activation code
                             	5439423,	// sensor 2 (keyfob A button) activation code
                             	0,	// sensor 3 (keyfob B button) activation code
                             	0,	// sensor 4 (keyfob C button) activation code
                             	9230958,	// sensor 5 (keyfob D button) activation code
                             	12899438,   // sensor 6 (water level sensor) activation code
                             	0,      	// UNREGISTERED SENSOR
                             	0,      	// UNREGISTERED SENSOR
                             	0,      	// UNREGISTERED SENSOR
                             	0,      	// UNREGISTERED SENSOR
                             	0,      	// UNREGISTERED SENSOR
                             	0,      	// UNREGISTERED SENSOR
                             	0,      	// UNREGISTERED SENSOR
                             	0,      	// UNREGISTERED SENSOR
                             	0,      	// UNREGISTERED SENSOR
                             	0,      	// UNREGISTERED SENSOR
                             	0,      	// UNREGISTERED SENSOR
                             	0,      	// UNREGISTERED SENSOR
                             	0           // UNREGISTERED SENSOR
                           	};

unsigned long lastTripTime[MAX_WIRELESS_SENSORS];	// array to hold the last time a sensor was tripped - for filtering purposes
unsigned long upcount = 0L; // sequence number added to the circular buffer log entries

	// other data that needs to be stored in non-volatile memory
String utcOffset = "-8.0";	// Pacific Standard Time
String observeDST = "yes";	// no" if locale does not observe DST

	// variable to hold the virtual eeprom device used to store the config
int eepromOffset;

	// array to hold parsed substrings from a command string
String g_dest[MAX_SUBSTRINGS];

	// Strings to publish data to the cloud
String sensorCode = String("");
String g_bufferReadout = String("");
char cloudMsg[80];  	// buffer to hold last sensor tripped message
char cloudBuf[90];  	// buffer to hold message read out from circular buffer
char registrationInfo[80]; // buffer to hold information about registered sensors

String cBuf[BUF_LEN];   // circular buffer to store events and messages as they happen
                        // Expected format of the string stored in cBuf is:
                        // TYPE,SEQUENCENUMBER,INDEX,EPOCTIME
                        // where
                        //    TYPE is A (advisory) or S (sensor)
                        //    SEQUENCENUMBER is a monotonically increasing global (eventNumber)
                        //    INDEX is into sensorName[] for type sensor
                        //          or into messages[] for type advisory
                        //    EPOCTIME is when the entry happened
                        // see cBufInsert(), cBufRead(), readFromBuffer(), logSensor(), logMessage()

int head = 0;       	// index of the head of the circular buffer
int tail = 0;       	// index of the tail of the buffer
int g_numToPublish = -1; // Number of entries in cBuf[] that remain to be published to spark cloud.
                         // This is incremented when events are added to the cBuf[] and decremented
                         // when an entry is published to the spark cloud.

char config[120];    	// buffer to hold local configuration information
long eventNumber = 0;   // grows monotonically to make each event unique


#if defined(DEBUG_EVENT) || defined(DEBUG_ADVISORY) || defined(DEBUG_COMMANDS)
	const unsigned long FILTER_TIME_UNREGISTERED = 5000L; // same as above, but for unregistered sensors
	unsigned long lastUnregisteredTripTime = 0;
	String debugLogMessage = String("junk"); // used by define DEBUG sections
#endif

// Additional globals for Saratoga SIS Test
int lastPIR = -1;   	// last PIR to trip - initialize to invalid value
time_t lastPIRTime = 0; 	// trip time of the last PIR to trip
time_t lastDoorTime = 0;	// trip time of the last Door sensor to trip
boolean lastSensorIsDoor = false;   // for door then PIR detection
boolean supress = false;	// multiple person suppression flag
byte personHome = UKN;  // initialize to unknown status
boolean comatose = false;   // patient is not moving

/**************************************** setup() ***********************************************/

void setup()
{



  // Use D7 LED as a test indicator.  Light it for one second at setup time
  pinMode(D7, OUTPUT);


  // select virtual device on the eeprom
  if(VIRTUAL_DEVICE_NUM < MAX_VIRTUAL_DEVICES)
  {
    eepromOffset = VIRTUAL_DEVICE_NUM * VIRTUAL_DEVICE_SIZE;
  }
  else
  {
    eepromOffset = MAX_VIRTUAL_DEVICES - 1;
  }

    digitalWrite(D7, HIGH);
    delay(200);
    digitalWrite(D7, LOW);
    delay(200);

	// initialize the I2C comunication
  Wire.setSpeed(CLOCK_SPEED_100KHZ);
  Wire.stretchClock(false);
  Wire.begin();

    digitalWrite(D7, HIGH);
    delay(200);
    digitalWrite(D7, LOW);
    delay(200);



  #ifdef DEBUG
	 Serial.begin(9600);
  #endif

  pinMode(INTERRUPT_315, INPUT);
  pinMode(INTERRUPT_433, INPUT);

  attachInterrupt(INTERRUPT_315, isr315, CHANGE);   // 315 MHz receiver on interrupt 3 => that is pin #D3
  attachInterrupt(INTERRUPT_433, isr433, CHANGE);   // 433 MHz receiver on interrupt 4 => that is pin #D4

    digitalWrite(D7, HIGH);
    delay(200);
    digitalWrite(D7, LOW);
    delay(200);


  // restore the saved configuration from non-volatile memory
  restoreConfig();

    digitalWrite(D7, HIGH);
    delay(200);
    digitalWrite(D7, LOW);
    delay(200);


  // wait for the Core to synchronise time with the Internet
  while(Time.year() <= 1970 && millis() < 30000)
  {
  	delay(100);
    Spark.process();
  }

  if (Time.year() <= 1970)
  {
    reportFatalError(3);
    //never returns from here
  }

    digitalWrite(D7, HIGH);
    delay(200);
    digitalWrite(D7, LOW);
    delay(200);


  // Publish local configuration information in config[]
  resetTime = Time.now();    	// the current time = time of last reset
  publishConfig();

  // Make sensorCode and cBufreadout strings available to the cloud
  Spark.variable("Config", config, STRING);
  Spark.variable("sensorTrip", cloudMsg, STRING);
  Spark.function("ReadBuffer", readBuffer);
  Spark.variable("circularBuff", cloudBuf, STRING);
  Spark.variable("registration", registrationInfo, STRING);
  Spark.function("Register", registrar);

  // Publish a start up event notification
  Spark.function("publistTestE", publishTestE); // for testing events

    digitalWrite(D7, HIGH);
    delay(200);
    digitalWrite(D7, LOW);
    delay(200);


  // Initialize the lastTripTime[] array
  for (int i = 0; i < MAX_WIRELESS_SENSORS; i++)
  {
  	lastTripTime[i] = 0L;
  }


#ifdef DEBUG
  Serial.println("End of setup()");
#endif

}

/************************************ end of setup() *******************************************/

/**************************************** loop() ***********************************************/
void loop()
{

  boolean knownCode = false;
  static unsigned long lastTimeSync = millis();  // to resync time with the cloud daily
  static boolean blinkReady = true;  // to know when non-blocking blink is ready to be triggered

  // Test for received code from a wireless sensor
  if (codeAvailable) // new wireless sensor code received
  {
	int i; 	// index into known sensor arrays

	// Test to see if the code is for a known sensor
	knownCode = false;
	for (i = 0; i < MAX_WIRELESS_SENSORS; i++)
	{
    	if ( (receivedSensorCode == activateCode[i])  )
    	{
        	knownCode = true;
        	break;
    	}
	}

	// If code is from a known sensor, filter it
	if (knownCode == true)	// registered sensor was tripped
	{
    	unsigned long now;
    	now = millis();
    	if((now - lastTripTime[i]) > FILTER_TIME) // filter out multiple codes
    	{
        	// Code for the sensor trip message
        	sensorCode = "Received sensor code: ";
        	sensorCode += receivedSensorCode;
        	sensorCode += " for ";
        	sensorCode += sensorName[i];

        	#ifdef DEBUG
            	Serial.println(sensorCode);  // USB print for debugging
        	#endif

        	#ifdef DEBUG_EVENT
            	debug = "Event: ";
            	debug += String(sensorName[i]);
            	publishDebugRecord(debug);
        	#endif

        	sensorCode.toCharArray(cloudMsg, sensorCode.length() + 1 );  // publish to cloud
        	cloudMsg[sensorCode.length() + 2] = '\0';  // add in the string null terinator


#if SEND_EVENTS_TO_WEBPAGE
          // send notification of new sensor trip for web page
          // This can send events too fast, so it is ifdef'd until
          // we get a send queue for publishing
        	publishEvent(String(i));
#endif
        	// determine type of sensor and process accordingly
        	if(i <= MAX_PIR)    	// then the sensor is a PIR
        	{
            	processPIRSensor(i);
        	}
        	else                	// not a PIR, then a door sensor
        	{
            if (i <= MAX_DOOR)
            {
            	processDoorSensor(i);
        	  }
            else
            {
              processSensor(i);
            }
          }

          if (i == ALARM_SENSOR) {

            sparkPublish("SISAlarm", "Alarm sensor trip", 60 );

          }

           	// code to blink the D7 LED when a sensor trip is detected
        	if(blinkReady)
        	{
            	blinkReady = nbBlink(NUM_BLINKS, BLINK_TIME);
        	}

        	// update the trip time to filter for next trip
        	lastTripTime[i] = now;
      	}
	}
	else // not a code from a known sensor -- report without filtering; no entry in circular buffer
	{
    	sensorCode = "Received sensor code: ";
    	sensorCode += receivedSensorCode;
    	sensorCode += " for unknown sensor";
    	Serial.println(sensorCode);  // USB print for debugging
    	sensorCode.toCharArray(cloudMsg, sensorCode.length() );  // publish to cloud

    	#ifdef DEBUG_EVENT
        	unsigned long now;
        	now = millis();
        	if((now - lastUnregisteredTripTime) > FILTER_TIME_UNREGISTERED) // filter out multiple codes
        	{
            	lastUnregisteredTripTime = now;
            	debug = "Event: Unknown code ";
            	debug += String(receivedSensorCode);
            	publishDebugRecord(debug);
        	}
    	#endif
	}

	codeAvailable = false;  // reset the code available flag if it was set
  }

  if(!blinkReady)  // keep the non blocking blink going
	{
    	blinkReady = nbBlink(NUM_BLINKS, BLINK_TIME);
	}

  #ifdef TESTRUN
  	simulateSensor();
  #endif

#ifdef photon044
  // resync Time to the cloud once per day
  if (millis() - lastTimeSync > ONE_DAY_IN_MILLIS)
  {
 	  Spark.syncTime();
	  lastTimeSync = millis();
  }
#endif

  // Testing for "person not home"
  if(lastSensorIsDoor && (personHome == HOME) && ((Time.now() - lastDoorTime) > AWAY))
  {
	personHome = NOT_HOME;

	// log "person not home" message
	logMessage(0);

  }

  // Testing for "no movement"
  if(!lastSensorIsDoor && !comatose && (personHome == HOME) && ((Time.now() - lastPIRTime) > COMATOSE))
  {
  	logMessage(2);
  	comatose = true;
  }

  #ifdef CLOUD_LOG
    publishCircularBuffer();  // pushes new events to the cloud, if needed
  #endif

}
/************************************ end of loop() ********************************************/

/************************************ () *********************************************/
// logMessage(): function to create a log entry that is an advisory message.
//
// Arguments:
//  messageIndex:  index into the messages[] array of message strings
//
void logMessage(int messageIndex)
{
	// create timestamp substring
	String timeStamp = "";
	timeStamp += Time.now();

	// create sequence number substring
	String sequence = "";
	sequence += upcount;
	if (upcount < 9999)  // limit to 4 digits
	{
    	upcount ++;
	}
	else
	{
    	upcount = 0;
	}

	// create the log entry
	String logEntry = "A,";  // advisory type message
	logEntry += sequence;
	logEntry += ",";
	logEntry += messages[messageIndex];
	logEntry += ",";
	logEntry += timeStamp;
	logEntry += ",";

	// pad out to 20 characters
	while (logEntry.length() < 22)
	{
    	logEntry += "x";
	}

	cBufInsert("" + logEntry);

	#ifdef DEBUG_ADVISORY
    	debugLogMessage = "Advise: ";
    	debugLogMessage += String(messages[messageIndex]);
    	publishDebugRecord(debugLogMessage);
	#endif

	return;
}

/********************************** end of logMessage() ****************************************/

/************************************* logSensor() *********************************************/
// logSensor(): function to create a log entry for a sensor trip
//
// Arguments:
//  sensorIndex:  index into the sensorName[] and activeCode[] arrays of sensor data
//
void logSensor(int sensorIndex)
{
	// create timestamp substring
	String timeStamp = "";
	timeStamp += Time.now();

	// create sequence number substring
	String sequence = "";
	sequence += upcount;
	if (upcount < 9999)  // limit to 4 digits
	{
    	upcount ++;
	}
	else
	{
    	upcount = 0;
	}

	// create the sensor index substring
	String sensor = "";
	sensor += sensorIndex;

	// create the log entry
	String logEntry = "S,"; // Sensor trip log entry
	logEntry += sequence;
	logEntry += ",";
	logEntry += sensor;
	logEntry += ",";
	logEntry += timeStamp;
	logEntry += ",";

	// pad out to 20 characters
	while (logEntry.length() < 22)
	{
    	logEntry += "x";
	}

	cBufInsert("" + logEntry);

	return;
}

/*********************************** end of logSensor() ****************************************/

/********************************** processPIRSensor() *****************************************/
// processPIRSensor():  function to process a generic sensor trip.  This function creates a
//  log entry for each registered sensor trip and records the log entry in the circular buffer.
//
//  Arguments:
//  	sensorIndex: the index into the sensorName[] and activateCode[] arrays for the sensor
//       	to be logged.

void processPIRSensor(int sensorIndex)
{
	//Test if this is the last PIR tripped
	if( (sensorIndex != lastPIR) && (supress == false))  // then process the PIR as a new PIR trip
	{
    	// log the sensor trip
    	logSensor(sensorIndex);

    	//Test for multiple persons
    	unsigned long elapsedTime = Time.now() - lastPIRTime;
    	if(elapsedTime < MULTI_TIME) 	// two different PIR within multiple person time period
    	{
        	supress = true;    	// Set the suppress flag

        	// log the suppress message
        	logMessage(3);
    	}

    	//Test for Person is home
    	elapsedTime = Time.now() - lastDoorTime;
    	if( (elapsedTime < AWAY) && personHome != HOME) 	// less than 10 minutes since door trip
    	{
        	personHome = HOME;    	// Set the personHome state

        	// log the person is home message
        	logMessage(1);
    	}

    	lastPIR = sensorIndex;  // update what is the last PIR to trip
    	comatose = false;   // any PIR resets comatose flag
	}

	// log the PIR trip if the person was comatose (no movement)
	if(comatose && (sensorIndex == lastPIR)) // log the sensor trip if the person was comatose
	{
    	logSensor(sensorIndex);
    	comatose = false;   // any PIR resets comatose flag
	}

    // any PIR trip indicates that person is moving
    lastPIRTime = Time.now();

    // update globals for PIR trip detection
    lastSensorIsDoor = false;

	personHome = HOME;  // if a PIR trips, someone is home, regardless (but don't log a message)
	return;
}
/***********************************end of processPIRSensor() ****************************************/

/********************************** processDoorSensor() *****************************************/
// processDoorSensor():  function to process a generic door trip.  This function creates a
//  log entry for each registered sensor trip and records the log entry in the circular buffer.
//
//  Arguments:
//  	sensorIndex: the index into the sensorName[] and activateCode[] arrays for the sensor
//       	to be logged.

void processDoorSensor(int sensorIndex)
{

	logSensor(sensorIndex);

	//Update globals for a PIR trip detection
	lastDoorTime = Time.now();
	lastSensorIsDoor = true;
	supress = false;
	lastPIR = -1;   // clear out the last PIR when a door is opened.

	return;

}
/***********************************end of processDoorSensor() ****************************************/

/********************************** processSensor() *****************************************/
// processSensor():  function to process a generic sensor trip.  This function creates a
//  log entry for each registered sensor trip and records the log entry in the circular buffer.
//
//  Arguments:
//  	sensorIndex: the index into the sensorName[] and activateCode[] arrays for the sensor
//       	to be logged.

void processSensor(int sensorIndex)
{

    logSensor(sensorIndex);

	return;

}
/***********************************end of processSensor() ****************************************/

/******************************************** writeConfig() ******************************************/
// writeConfig():  writes the sensor configuration out to non-volatile memory using the Wire library
//  and custom I2C eeprom code.  The sensor configuration consists of data buffers, each CONFIG_BUFR long.
//  The buffers are written to non-volatile memory in the following order:
//  	ID: a constant string identifier: "SIS-2015"
//  	TZ: the Core's local timezone offset, as a string, e.g. "-8.0"
//  	DST: whether the Core's locale observes daylight savings time, e.g. "yes" or "no"
//  	NAME: the name of each registered sensor is stored in its own buffer.  The number of names = MAX_WIRELESS_SENSORS
//  	CODE: the code that a tripped sensor sends.  Each sensor's code is stored in its own buffer.

void writeConfig()
{
	String id = "SIS-2015"; 	// the ID value
	char buf[CONFIG_BUFR];  	// temporary buffer to write to non-volatile memory
	String temp;            	// temporary string storage

	int addr;

	// write the ID into non-volatile memory
	id.toCharArray(buf, id.length()+1);
	buf[id.length()+1] = '\0'; // terminate string with a null
	addr=eepromOffset + 0; //first address
	i2cEepromWritePage(0x50, addr, buf, sizeof(buf)); // write to EEPROM

	// write the timezone into non-volatile memory
	utcOffset.toCharArray(buf,utcOffset.length()+1);
	buf[id.length()+1] = '\0'; // terminate string with a null
	addr=eepromOffset + CONFIG_BUFR; //timezone address
	i2cEepromWritePage(0x50, addr, buf, sizeof(buf)); // write to EEPROM

	// write the dst info into non-volatile memory
	observeDST.toCharArray(buf,observeDST.length()+1);
	buf[id.length()+1] = '\0'; // terminate string with a null
	addr=eepromOffset + 2*CONFIG_BUFR; //dst address
	i2cEepromWritePage(0x50, addr, buf, sizeof(buf)); // write to EEPROM

	// write the sensor names and trip codes to non volatile memory
	for(int i = 0; i < MAX_WIRELESS_SENSORS; i++)
	{
    	temp = sensorName[i];
    	// check to see if name is too long, truncate if it is
    	if(temp.length() >= (CONFIG_BUFR - 2))
    	{
        	temp.substring(0, (CONFIG_BUFR - 2));
    	}

    	// store names
    	temp.toCharArray(buf, temp.length()+1);
    	buf[temp.length()+1] = '\0'; // terminate string with a null
    	addr=eepromOffset + (i+3) * CONFIG_BUFR; //names address
    	i2cEepromWritePage(0x50, addr, buf, sizeof(buf)); // write to EEPROM

    	// store trip codes
    	temp = "";
    	temp += activateCode[i];
    	temp.toCharArray(buf, temp.length()+1);
    	buf[temp.length()+1] = '\0'; // terminate string with a null
    	addr=eepromOffset + (i+3+MAX_WIRELESS_SENSORS) * CONFIG_BUFR; //trip codes address
    	i2cEepromWritePage(0x50, addr, buf, sizeof(buf)); // write to EEPROM
	}

	return;
}
/***************************************** end of writeConfig() ******************************************/

/******************************************* restoreConfig() *********************************************/
// restoreConfig():  restores the sensor configuration from non-volatile memory using the Wire library
//  and custom I2C eeprom code.  The sensor configuration consists of data buffers, each CONFIG_BUFR long.
//  The buffers are written to non-volatile memory in the following order:
//  	ID: a constant string identifier: "SIS-2015"
//  	TZ: the Core's local timezone offset, as a string, e.g. "-8.0"
//  	DST: whether the Core's locale observes daylight savings time, e.g. "yes" or "no"
//  	NAME: the name of each registered sensor is stored in its own buffer.  The number of names = MAX_WIRELESS_SENSORS
//  	CODE: the code that a tripped sensor sends.  Each sensor's code is stored in its own buffer.

void restoreConfig()
{
	String ID = "SIS-2015"; 	// the ID value
	char buf[CONFIG_BUFR];  	// temporary buffer to write to non-volatile memory
	int addr;

	// read the ID and return immediately if ID is not correct
	addr=eepromOffset + 0; //first address
	i2cEepromReadPage(0x50, addr, buf, CONFIG_BUFR);

	// make sure that the buffer contains a valid string
	buf[31] = '\0';
	String data(buf);

	if(data.equals(ID)) 	// restore the rest of the config
	{
    	// restore the timezone
    	addr=eepromOffset + CONFIG_BUFR; //timezone address
    	i2cEepromReadPage(0x50, addr, buf, CONFIG_BUFR);

    	// make sure that the buffer contains a valid string
    	buf[31] = '\0';
    	utcOffset = buf;

    	// restore the dst
    	addr=eepromOffset + 2*CONFIG_BUFR; //dst address
    	i2cEepromReadPage(0x50, addr, buf, CONFIG_BUFR);

    	// make sure that the buffer contains a valid string
    	buf[31] = '\0';
    	observeDST = buf;

    	// restore the sensor names and trip codes
    	for(int i = 0; i < MAX_WIRELESS_SENSORS; i++)
    	{
        	// retrieve names
        	addr=eepromOffset + (i+3) * CONFIG_BUFR; //names address
        	i2cEepromReadPage(0x50, addr, buf, CONFIG_BUFR);

        	// make sure that the buffer contains a valid string
        	buf[31] = '\0';
        	sensorName[i] = buf;

        	// retrieve trip codes
        	addr=eepromOffset + (i+3+MAX_WIRELESS_SENSORS) * CONFIG_BUFR; //trip codes address
        	i2cEepromReadPage(0x50, addr, buf, CONFIG_BUFR);

        	// make sure that the buffer contains a valid string
        	buf[31] = '\0';
        	String temp(buf);
        	activateCode[i] = temp.toInt();
    	}

	}

	return;

}
/****************************************** end of restoreConfig() ******************************************/

/******************************************* i2cEepromWritePage() *******************************************/
// i2cEepromWritePage():  writes a page of data to the I2C eeprom device at deviceAddress.  The
//  page size cannot exceed 32 characters owing to a limitation of the Spark Wire library.
//
// Arguments:
//  deviceAddress.  The I2C bus address of the EEPROM device; nominally 0x50
//  eeAddressPage.  The starting address in EEPROM of the page to be written
//  data.  Pointer to a character array (string) of data to be written to EEPROM
//  length. The number of characters to be written to EEPROM (i.e. page size)
//
void i2cEepromWritePage( int deviceAddress, unsigned int eeAddressPage, char* data, byte length )
{
	if(length > 32) return; // make sure you don't blow the I2C library buffer!
	Wire.beginTransmission(deviceAddress);
	Wire.write((byte)( (eeAddressPage & 0xFF00) >> 8)); // MSB
	Wire.write((byte)(eeAddressPage & 0xFF)); // LSB
	for (byte c = 0; c < length; c++)
	{
    	Wire.write(data[c]);
	}
	Wire.endTransmission();
	delay(5);   // recommended delay for I2C bus
	return;
}

/******************************************** end of i2cEepromWritePage() ************************************/

/********************************************** i2cEepromReadPage() ******************************************/
// i2cEepromReadPage():  reads a page of data from the I2C eeprom device at deviceAddress.  The
//  page size cannot exceed 32 characters owing to a limitation of the Spark Wire library.
//
// Arguments:
//  deviceAddress.  The I2C bus address of the EEPROM device; nominally 0x50
//  eeAddressPage.  The starting address in EEPROM of the page to be read
//  buffer.  Pointer to a character array (buffer) where the data from the EEPROM is to be written
//  length. The number of characters to be read from EEPROM (i.e. page size)
//
void i2cEepromReadPage( int deviceAddress, unsigned int eeAddressPage, char* buffer, int length )
{
	if(length > 32) return; // make sure you don't blow the I2C library buffer!
	Wire.beginTransmission(deviceAddress);
	Wire.write((byte)( (eeAddressPage & 0xFF00) >> 8)); // MSB
	Wire.write((byte)(eeAddressPage & 0xFF)); // LSB
	Wire.endTransmission();
	Wire.requestFrom(deviceAddress,length,true);
	int c = 0;
	for ( c = 0; c < length; c++ )
	{
    	while (!Wire.available())
    	{
        	// wait for data to be ready
    	}
    	buffer[c] = Wire.read();
	}
	delay(5);   // recommended delay for I2C bus
	return;
}
/********************************************* end of i2cEepromReadPage() *************************************/

/*************************************** parser() **********************************************/
// parser(): parse a comma delimited string into its individual substrings.
//  Arguments:
//  	source:  String object to parse
//  Return: the number of substrings found and parsed out
//
//  This functions uses the following global constants and variables:
//  	const int MAX_SUBSTRINGS -- nominal value is 6
//  	String g_dest[MAX_SUBSTRINGS] -- array of Strings to hold the parsed out substrings

int parser(String source)
{
	int lastComma = 0;
	int presentComma = 0;

	//parse the string argument until there are no more substrings or until MAX_SUBSTRINGS
	//  are parsed out
	int index = 0;
	do
	{
    	presentComma = source.indexOf(',', lastComma);
    	if(presentComma == -1)
    	{
        	presentComma = source.length();
    	}
      g_dest[index++] = "" + source.substring(lastComma, presentComma);
    	lastComma = presentComma + 1;

	} while( (lastComma < source.length() ) && (index < MAX_SUBSTRINGS) );

	return (index);
}
/************************************ end of parser() ********************************************/

/************************************ publishConfig() ********************************************/
// publishConfig():  function to make the local configuration available to the cloud
//

void publishConfig()
{
  String localConfig = "cBufLen: ";
  localConfig += String(BUF_LEN);
  localConfig += ", MaxSensors:";
  localConfig += String(MAX_WIRELESS_SENSORS);
  localConfig += ", version: ";
  localConfig += VERSION;
  localConfig += ", utcOffset: ";
  localConfig += utcOffset;
  localConfig += ", DSTyn: ";
  localConfig += observeDST;
  localConfig += ", resetAt: ";
  localConfig += String(resetTime);
  localConfig += "Z ";

  localConfig.toCharArray(config, localConfig.length() );

  return;
}
/******************************* end of publishConfig() ****************************************/

/************************************* registrar() ********************************************/
// registrar():  manage wireless sensor registration on the Spark Core.
//  Arguments:
//  	String action: a string representation of the registration action.  Options are:
//      	"read": read out the registration information about a sensor.  Format of "read" is
//          	"read,location", where location is a String representation of the location in
//          	the sensor registration arrays to read back.  The location is
//          	0 .. MAX_WIRELESS_SENSORS.
//      	"delete":  delete sensor registration info.  Format of "delete" is "delete,location",
//          	where location is a String representation of the location in the sensor registration
//           	arrays to read back.  The location is 0 .. MAX_WIRELESS_SENSORS.
//      	"register":  register a new sensor.  Format of "register" is
//          	"register,location,sensor_trip_code,description", where where location is a
//          	String representation of the location in the sensor registration arrays to read back.
//          	The location is 0 .. MAX_WIRELESS_SENSORS;  "sensor_trip_code" is a string (ASCII)
//          	representation of the code that the sensor sends when it is tripped; and "description"
//          	is a textual sensor trip message.
//      	"store": store the current configuration to non-volatile memory.  The format of "store" is
//          	just the single string "store" with no commas or other parameters.  "store" causes the
//          	following information to be stored into non-volatile memory:
//              	- an identifer:  "SIS-2015".  if the ID is incorrect, the config won't be restored
//                  	and defaults will be used instead.
//              	- utcOffset
//              	- DST
//              	- all sensor names
//              	- all sensor trip codes
//          	The config is restored automatically upon resetting the Core.
//

int registrar(String action)
{
	#ifdef DEBUG_COMMANDS
    	debugLogMessage = "Cmd: ";
    	debugLogMessage += String(action);
    	publishDebugRecord(debugLogMessage);
	#endif

   // requested actions
	const int READ = 0; 	// action is "read"
	const int DELETE = 1;   // action is "delete"
	const int REG = 2;  	// action is "register"
	const int OFFSET = 3;   // action is to set the local utc offset
	const int DST = 4;  	// action is to set the local observe DST (yes or no)
	const int STORE = 5;	// action is to store the sensor configuration to non-volatile memory
	const int UKN = -1; 	// action is unknown

	int requestedAction;
	int numSubstrings;
	String registrationInformation; 	// String to hold the information about the sensor
//	unsigned long sensorCode;       	// numerical value of the sensor trip code
	int location;                   	// numerical value of ordinal number of the sensor info

	// parse the string argument into its substrings
	numSubstrings = parser(action);

	if(numSubstrings < 1) //  invalid command string
	{
    	return -1;
	}

	// determine the command in g_dest[0]
	if(g_dest[0] == "read")
	{
    	requestedAction = READ;
	}
	else
	{
    	if(g_dest[0] == "delete")
    	{
        	requestedAction = DELETE;
    	}
    	else
    	{
        	if(g_dest[0] == "register")
        	{
            	requestedAction = REG;
        	}
        	else
        	{
            	if(g_dest[0] == "offset")
            	{
                	requestedAction = OFFSET;
            	}
            	else
            	{
                	if(g_dest[0] == "DST")
                	{
                    	requestedAction = DST;
                	}
                	else
                	{
                    	if(g_dest[0] == "store")
                    	{
                        	requestedAction = STORE;
                    	}
                    	else
                    	{
                        	requestedAction = UKN;
                    	}
                	}

            	}
        	}
    	}
	}

	// obtain the location from g_dest[1]
	location = g_dest[1].toInt();

	// perform the requested action


	switch(requestedAction)
	{
    	case READ:
        	if(location >= MAX_WIRELESS_SENSORS)
        	{
            	location = MAX_WIRELESS_SENSORS - 1; // clamp the location within array bounds
        	}

        	// read action
        	registrationInformation = "loc: ";
        	registrationInformation += String(location);
        	registrationInformation += ", sensor code: ";
        	registrationInformation += (String)activateCode[location];
        	registrationInformation += " is for ";
        	registrationInformation += sensorName[location];
        	Serial.println(registrationInformation);

        	// write to the cloud
        	registrationInformation.toCharArray(registrationInfo, registrationInformation.length() + 1);
        	registrationInfo[registrationInformation.length() + 2] = '\0';

        	break;

    	case DELETE:
        	if(location >= MAX_WIRELESS_SENSORS)
        	{
            	break; // don't delete an invalid location
        	}

        	// delete action
        	sensorName[location] = "UNREGISTERED SENSOR";
        	activateCode[location] = 0L;
        	break;

    	case REG:
        	if(location >= MAX_WIRELESS_SENSORS)
        	{
            	numSubstrings = -1; // return an error code
            	break; // don't register to an invalid location
        	}

        	// ensure that at least 4 substrings were received
        	if(numSubstrings < 4)
        	{
            	numSubstrings = -1; // return an error code
            	break;
        	}

        	// perform the new sensor registration function
        	sensorName[location] = g_dest[3];
        	activateCode[location] = g_dest[2].toInt();
        	break;

    	case OFFSET:
        	utcOffset = "" + g_dest[1];
        	publishConfig();
        	break;

    	case DST:
        	observeDST = "" + g_dest[1];
        	publishConfig();
        	break;

    	case STORE:
        	writeConfig();
        	break;

    	default:
            numSubstrings = -1; // return an error code for unknown command
        	break;
	}

	return numSubstrings;
}


/********************************** end of registrar() ****************************************/

/************************************* readBuffer() ********************************************/
// readBuffer(): read the contents of the circular buffer into the global variable "cloudBuf"
//  Arguments:
//  	String location:  numerical location of the buffer data to read.  The location is relative
//      	to the latest entry, which is "0".  The next to latest is "1", etc. back to BUF_LEN -1.
//       	BUF_LEN can be determined from the cloud variable "bufferSize".  If location exceeds
//       	BUF_LEN - 1, the value that is read out is the oldest value in the buffer, and
//       	-1 is returned by the function.  Otherwise, the value is determined by location
//       	and 0 is returned by this function.
//  Return:  0 if a valid location was specified, otherwise, -1.

int readBuffer(String location)
{
    int offset;
    int result;

    offset = location.toInt();
    result = readFromBuffer(offset, cloudBuf);

    return result;
}

/*********************************end of readBuffer() *****************************************/

/********************************** readFromBuffer() ******************************************/
// readFromBuffer(): utility fujction to read from the circular buffer into the
//  character array passed in as stringPtr[].
//  Arguments:
//      int offset: the offset into the circular buffer to read from. 0 is the latest entry.  The
//          next to latest entry is 1, etc. back to BUF_LEN -1.
//       	BUF_LEN can be determined from the cloud variable "bufferSize".  If location exceeds
//       	BUF_LEN - 1, the value that is read out is the oldest value in the buffer, and
//       	-1 is returned by the function.  Otherwise, the value is determined by location
//       	and 0 is returned by this function.
//      char stringPtr[]: pointer to the string that will be returned from this
//        function. The format of the string expected by the web site is one of:
//            (S:nnn) SENSORNAME tripped at DATETIME Z (epoc:EPOCTIME)
//            (S:nnn) SENSORNUMBER detected at DATETIME Z (epoc:EPOCTIME)
//
//  Return:  0 if a valid location was specified, otherwise, -1.

int readFromBuffer(int offset, char stringPtr[])
{
	int result;     	// the result code to return

	// check and fix, if necessary, the offset into the circular buffer
	if(offset >= BUF_LEN)
	{
    	offset = BUF_LEN - 1;   // the maximum offset possible
    	result = -1;        	// return the error code
	}
	else
	{
    	result = 0;         	// return no error code
	}


	// now retrieve the data requested from the circular buffer and place the result string
    // in g_bufferReadout
	g_bufferReadout = "" + cBufRead(offset);

	#ifdef DEBUG
    	Serial.println(g_bufferReadout);
	#endif

	// create the readout string for the cloud from the buffer data
	if(g_bufferReadout != "")  // skip empty log entries
	{
    	int index;

       // parse the comma delimited string into its substrings
      // result of parse is in global array g_dest[]

    	parser(g_bufferReadout);

    	// format the sequence number and place into g_bufferReadout
        g_bufferReadout = "(S:";
        g_bufferReadout += g_dest[1];
        g_bufferReadout += ")";

        // Determine message type
    	if(g_dest[0] == "S")  	// sensor type message
    	{

        	// format the sensor Name from the index
        	index = g_dest[2].toInt();
            g_bufferReadout += sensorName[index];
            g_bufferReadout += " tripped at ";
    	}
    	else    	// advisory type message
    	{
            g_bufferReadout += g_dest[2];
            g_bufferReadout += " detected at ";
    	}

    	// add in the timestamp

    	index = g_dest[3].toInt();
        g_bufferReadout += Time.timeStr(index).c_str();
        g_bufferReadout += " Z (epoch:";
        g_bufferReadout += g_dest[3];
        g_bufferReadout += "Z)";

	}

    g_bufferReadout.toCharArray(stringPtr, g_bufferReadout.length() + 1 );
	stringPtr[g_bufferReadout.length() + 2] = '\0';

	return result;

}

/********************************** end of readFromBuffer() ****************************************/

/******************************************** cBufInsert() *****************************************/
// cBufInsert():  insert a string into the circular buffer at the current tail position.
//  Arguments:
//	String data:  the string data (string object) to store in the circular buffer
//	return:  none.

void cBufInsert(String data)
{
  static boolean fullBuf = false;	// false for the first pass (empty buffer locations)

  cBuf[tail] = data;	// write the data at the end of the buffer
  g_numToPublish++;     // note that there is a new buffer entry to publish

  //  adjust the tail pointer to the next location in the buffer
  tail++;
  if(tail >= BUF_LEN)
  {
	tail = 0;
	fullBuf = true;
  }

  //  the first time through the buffer, the head pointer stays put, but after the
  //	buffer wraps around, the head of the buffer is the tail pointer position
  if(fullBuf)
  {
	head = tail;
  }

}
/***************************************** end of cBufInsert() **************************************/

/********************************************* cBufRead() *******************************************/
// cBufRead():  read back a String object from the "offset" location in the cirular buffer.  The
//	offset location of zero is the latest value in (tail of) the circular buffer.
//  Arguments:
//	int offset:  the offset into the buffer where zero is the most recent entry in the circular buffer
//       and 1 is the next most recent, etc.
//  Return:  the String at the offset location in the circular buffer.

String cBufRead(int offset)
{
  int locationInBuffer;

  locationInBuffer = tail -1 - offset;
  if(locationInBuffer < 0)
  {
    locationInBuffer += BUF_LEN;
  }

  return cBuf[locationInBuffer];

}
/****************************************** end of cBufRead() ***************************************/



/****************************************** publishCircularBuffer () ************************/
// publishCircularBuffer()
//
// This routine publishes recent events in the circular buffer cBuf[] to the Spark Cloud. It
// uses the global variable g_numToPublish to keep track of how many events are awaiting publication.
// It uses a local static variable to be sure that events are not published more frequently than once
// every 2 seconds.
// This routine is called once each time through the main loop. It could be called anytime.
//
void publishCircularBuffer() {

    static unsigned long lastPublishTime = 0;

    if (g_numToPublish >= 0 ) {

        unsigned long currentTime = millis();
        if (currentTime - lastPublishTime > 4000)
        {

            char localBuf[90];

            readFromBuffer(g_numToPublish, localBuf);      // read out the latest logged entry into localBuf

            if(sparkPublish("LogEntry", localBuf, 60))     // ... and publish it to the cloud for xteranl logging
            {
                g_numToPublish--;

            };
            lastPublishTime = currentTime;

        }

    }

}



/****************************************** End of publishCircularBuffer () ************************/




#ifdef TESTRUN

//This routine is called every iteration of the main loop. Based on the current
//time it decides if a sensor trip should be made.
void simulateSensor() {
  struct simulationEvent {
	unsigned long simTime; 	// time of a fake trip in msec
	int simPosition;       	// the sensor config position to trip
  } ;
  const int SIMULATE_EVENTS_MAX = 5;
  const simulationEvent simEvents[SIMULATE_EVENTS_MAX] = {
	{5000, 1},
	{6000, 2},
	{20000, 3},
	{21000, 2},
	{50000, 2}
  };
  static unsigned long simStartTime = 0;
  static int simLastEventFired = -1;

  if (simStartTime == 0) {
	simStartTime = millis();  // first time through we get the time of the start of the simulation
  }

  unsigned long simCurrentTime = millis() - simStartTime; // what is the current simulation time?

  int i = simLastEventFired + 1;
  while (i < SIMULATE_EVENTS_MAX)
  {   // check each simulation event after the last one we tripped

	if (simCurrentTime > simEvents[i].simTime){  // if we are later in simulation than time of an event, trip it

  	receivedSensorCode = activateCode[simEvents[i].simPosition]; // setting global
  	codeAvailable = true;   // setting global
  	simLastEventFired = i;  // next time we will check only after this event
  	break;              	// only trip the next event

	}
	i++;
  }
}

#endif  /* TESTRUN */


/************************************** isr315() ***********************************************/
//This is the interrupt service routine for interrupt 3 (315 MHz receiver)
void isr315()
{
  codeTimes = codeTimes315;	// set pointer to 315 MHz array
  process315();
  return;
}

/***********************************end of isr315() ********************************************/

/************************************** isr433() ***********************************************/
//This is the interrupt service routine for interrupt 4 (433 MHz receiver)
void isr433()
{
  codeTimes = codeTimes433;	// set pointer to 433 MHz array
  process433();
  return;
}

/***********************************end of isr433() ********************************************/

/************************************* process315() ***********************************************/
//This is the code to process and store timing data for interrupt 3 (315 MHz)

void process315()
{
  //this is right out of RC-SWITCH
  static unsigned int duration;
  static unsigned int changeCount;
  static unsigned long lastTime = 0L;
  static unsigned int repeatCount = 0;


  long time = micros();
  duration = time - lastTime;

  if (duration > 5000 && duration > codeTimes[0] - 200 && duration < codeTimes[0] + 200)
  {
	repeatCount++;
	changeCount--;
	if (repeatCount == 2)  // two successive code words found
	{
  	decode(changeCount); // decode the protocol from the codeTimes array
  	repeatCount = 0;
	}
	changeCount = 0;
  }
  else if (duration > 5000)
  {
	changeCount = 0;
  }

  if (changeCount >= 52) // too many bits before sync
  {
	changeCount = 0;
	repeatCount = 0;
  }

  codeTimes[changeCount++] = duration;
  lastTime = time;

  return;
}
/***********************************end of process315() ********************************************/

/************************************** process433() ***********************************************/
//This is the code to process and store timing data for interrupt 4 (433 MHz)

void process433()
{
  //this is right out of RC-SWITCH
  static unsigned int duration;
  static unsigned int changeCount;
  static unsigned long lastTime = 0L;
  static unsigned int repeatCount = 0;


  long time = micros();
  duration = time - lastTime;

  if (duration > 5000 && duration > codeTimes[0] - 200 && duration < codeTimes[0] + 200)
  {
	repeatCount++;
	changeCount--;
	if (repeatCount == 2)  // two successive code words found
	{
  	decode(changeCount); // decode the protocol from the codeTimes array
  	repeatCount = 0;
	}
	changeCount = 0;
  }
  else if (duration > 5000)
  {
	changeCount = 0;
  }

  if (changeCount >= 52) // too many bits before sync
  {
	changeCount = 0;
	repeatCount = 0;
  }

  codeTimes[changeCount++] = duration;
  lastTime = time;

  return;
}
/***********************************end of isr433() ********************************************/


/*************************************** decode() ************************************************/
// decode():  Function to decode data in the appropriate codeTimes array for the data that was
//  just processed.  This function supports PT2262 and EV1527 codes -- 24 bits of data where
//  0 = 3 low and 1 high, 1 = 3 high and 1 low.
// Arguments:
//  changeCount: the number of timings recorded in the codeTimes[] buffer.

void decode(unsigned int changeCount)
{

  unsigned long code = 0L;
  unsigned long delay;
  unsigned long delayTolerance;

  delay = codeTimes[0] / 31L;
  delayTolerance = delay * TOLERANCE * 0.01;

  for (int i = 1; i < changeCount ; i=i+2)
  {

	if (codeTimes[i] > delay-delayTolerance && codeTimes[i] < delay+delayTolerance && codeTimes[i+1] > delay*3-delayTolerance && codeTimes[i+1] < delay*3+delayTolerance)
	{
  	code = code << 1;
	}
	else
  	if (codeTimes[i] > delay*3-delayTolerance && codeTimes[i] < delay*3+delayTolerance && codeTimes[i+1] > delay-delayTolerance && codeTimes[i+1] < delay+delayTolerance)
  	{
    	code+=1;
    	code = code << 1;
  	}
  	else
  	{
    	// Failed
    	i = changeCount;
    	code = 0;
  	}
   }
   code = code >> 1;
   if (changeCount > 6) // ignore < 4bit values as there are no devices sending 4bit values => noise
   {
  	receivedSensorCode = code;
  	if (code == 0)
  	{
    	codeAvailable = false;
  	}
  	else
  	{
    	codeAvailable = true;
  	}

   }
   else	// too short -- noise
   {
 	codeAvailable = false;
 	receivedSensorCode = 0L;
   }

  return;
}

/************************************ end of decode() ********************************************/

/************************************** nbBlink() ************************************************/
// nbBlink():  Blink the D7 LED without blocking.  Note: setup() must declare
//          	pinMode(D7, OUTPUT) in order for this function to work
//  Arguments:
//  	numBlinks:  the number of blinks for this function to implement
//  	blinkTime:  the time, in milliseconds, for the LED to be on or off
//
//  Return:  true if function is ready to be triggered again, otherwise false
//

boolean nbBlink(byte numBlinks, unsigned long blinkTime)
{
	const byte READY = 0;
	const byte LED_ON = 1;
	const byte LED_OFF = 2;
	const byte NUM_BLINKS = 3;


	static byte state = READY;
	static unsigned long lastTime;
	static unsigned long newTime;
	static byte blinks;

	newTime = millis();

	switch(state)
	{
    	case(READY):
        	digitalWrite(D7, HIGH); 	// turn the LED on
        	state = LED_ON;
        	lastTime = newTime;
        	blinks = numBlinks;
        	break;

    	case(LED_ON):
        	if( (newTime - lastTime) >= blinkTime) // time has expired
        	{
            	state = LED_OFF;
            	lastTime = newTime;
        	}
        	break;

    	case(LED_OFF):
        	digitalWrite(D7, LOW);  	// turn the LED off
        	if( (newTime - lastTime) >= blinkTime)
        	{
            	if(--blinks > 0) 	// another blink set is needed
            	{
                	digitalWrite(D7, HIGH);
                	state = LED_ON;
                	lastTime = newTime;
            	}
            	else
            	{
                	state = READY;
            	}

        	}
        	break;

    	default:
        	digitalWrite(D7, LOW);
        	state = READY;

	}

	if(state == READY)
	{
    	return true;
	}
	else
	{
    	return false;
	}
}

/*********************************** end of nbBlink() ********************************************/

/************************************ publishEvent() *********************************************/
// publishEvent():  Function to pubish a Core event to the cloud in JSON format
//  Arguments:
//  	data: the message to publish to the cloud
//

int publishTestE(String data)
{
  eventNumber++;

  // Make it JSON ex: {"eventNum":"1","eventData":"data"}
  String msg = "{";
  msg += makeNameValuePair("eventNum",String(eventNumber));
  msg += ",";
  msg += makeNameValuePair("eventData", data);
  msg += "}";
  sparkPublish("SISEvent", msg , 60);
  return 0;

}


#if defined(DEBUG_EVENT) || defined(DEBUG_ADVISORY) || defined(DEBUG_COMMANDS)
int publishDebugRecord(String logData)
{
	eventNumber++;

	// Make it JSON ex: {"eventNum":"1","eventData":"data"}
  	String msg = "{";
  	msg += makeNameValuePair("num",String(eventNumber));
  	msg += ",";
  	msg += makeNameValuePair("info",logData);
  	msg += "}";

  	sparkPublish("SISLogData", msg, 60);
}
#endif /* publishDebugRecord */

// Keeping a separate publishEvent because we might want to send more than one
// data field.
int publishEvent(String sensorIndex)
{
  eventNumber++;

  // Make it JSON ex: {"eventNum":"1","eventData":"data"}
  String msg = "{";
  msg += makeNameValuePair("eventNum",String(eventNumber));
  msg += ",";
  msg += makeNameValuePair("sensorLocation",sensorIndex);
  msg += "}";

  sparkPublish("SISEvent", msg, 60);

}

int sparkPublish (String eventName, String msg, int ttl)
{
  bool success = true;

	if (millis() > 5000 )  // don't publish until spark has a chance to settle down
	{
    #ifdef photon044
        success = Spark.publish(eventName, msg, ttl, PRIVATE);
    #endif

    #ifndef photon044
      //  A return code from spark.publish is only supported on photo 0.4.4 and later
      Spark.publish(eventName, msg, ttl, PRIVATE);
    #endif
	}


#ifdef DEBUG
    Serial.println("sparkPublish called");

    if (success == false)
    {
      String message = "Spark.publish failed";
      Serial.print(message);

      message = " trying to publish " + eventName + ": " + msg;

      Serial.println(message);
      Spark.process();
    }

#endif

  return success;

  return success;

}



String makeNameValuePair(String name, String value)
{
	String nameValuePair = "";
	nameValuePair += DOUBLEQ + name + DOUBLEQ;
	nameValuePair += ":";
	nameValuePair += DOUBLEQ + value + DOUBLEQ;
	return nameValuePair;
}

/********************************* end of publishEvent() *******************************/

/********************************* fatal error code reporting *******************************/
// Call this to flash D7 continuously. This routine never exits.
//
// Error codes
//    3 flashes:  Failed to sync time to internet. Action: power cycle
//
void reportFatalError(int errorNum)
{

#ifdef DEBUG
    String message = "unknown error code";
    Serial.print("Fatal Error: ");
    switch(errorNum)
    {
    case 3:
      message = " could not sync time to internet";
      break;
    default:
      break;
    }
    Serial.println(message);
    Spark.process();
#endif

  while(true)  // never ending loop
  {
    for (int i=0; i < errorNum; i++)
    {
      digitalWrite(D7, HIGH);
      delay(100);
      Spark.process();
      digitalWrite(D7, LOW);
      delay(100);
      Spark.process();
    }
    digitalWrite(D7, LOW);

    // Now LED off for 1500 milliseconds
    for (int i=0; i < 3; i++)
    {
      delay(500);
      Spark.process();
    }
  }

  // we will never get here.
  return;

}




/********************************* fatal error code reporting *******************************/

#include "Adafruit_BLE.h"
#include "Adafruit_BLEMIDI.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "BluefruitConfig.h"
#include "Wire.h"             // I2C data bus library
#include "Adafruit_MPR121.h"  // touch sensor library

#define FACTORYRESET_ENABLE         1
#define MINIMUM_FIRMWARE_VERSION    "0.7.0"

#define BUTTONS  12
#define IRQ_PIN  A2            // The MPR121 will use the IRQ to let the controller know something has changed

// mpr121 init
Adafruit_MPR121 cap = Adafruit_MPR121();

// setup bluetooth and bluetooth midi
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);
Adafruit_BLEMIDI midi(ble);        // BLE MIDI object

bool isConnected = false;

// A small helper
void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}
void connected(void)
{
  isConnected = true;
  Serial.println(F(" CONNECTED!"));
  delay(1000);
}

void disconnected(void)
{
  Serial.println("disconnected");
  isConnected = false;
}

void BleMidiRX(uint16_t timestamp, uint8_t status, uint8_t byte1, uint8_t byte2)
{
  Serial.print("[MIDI ");
  Serial.print(timestamp);
  Serial.print(" ] ");

  Serial.print(status, HEX); Serial.print(" ");
  Serial.print(byte1 , HEX); Serial.print(" ");
  Serial.print(byte2 , HEX); Serial.print(" ");

  Serial.println();
}

// keep track of touched keys
uint16_t lasttouched = 0;
uint16_t currtouched = 0;

// MIDI note mapping
// Octave   Note Numbers
//          C   C#  D   D#  E   F   F#  G   G#  A   A#  B
//    0     0   1   2   3   4   5   6   7   8   9   10  11
//    1     12  13  14  15  16  17  18  19  20  21  22  23
//    2     24  25  26  27  28  29  30  31  32  33  34  35
//    3     36  37  38  39  40  41  42  43  44  45  46  47
//    4     48  49  50  51  52  53  54  55  56  57  58  59
//    5     60  61  62  63  64  65  66  67  68  69  70  71
//    6     72  73  74  75  76  77  78  79  80  81  82  83
//    7     84  85  86  87  88  89  90  91  92  93  94  95
//    8     96  97  98  99  100 101 102 103 104 105 106 107
//    9     108 109 110 111 112 113 114 115 116 117 118 119
//    10    120 121 122 123 124 125 126 127

// prime dynamic values
int channel = 0;

// C3 major
//int pitch[] = {36, 38, 40, 41, 43, 45, 47, 48, 50, 52, 53, 55};

// F# / Gb Major ( F#, G#, A#, B, C#, D#, F, F# )
//int pitch[] = {66, 68, 70, 71, 73, 75, 77, 78, 80, 82, 83, 85};

// E4 major ( E, F#, G#, A, B, C#, D#, E ) 
//int pitch[] = {52, 54, 56, 57, 59, 61, 63, 64, 66, 68, 69, 71};

// Dm Pentatonic Scale ( D, F, G, A, C, D )
int pitch[] = {38, 41, 43, 45, 48, 50, 53, 55, 57, 60, 62, 65};

int vel[] = {80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80};

void setup() {
  while (!Serial);  // required for Flora & Micro
  delay(500);
  
  Serial.begin(115200);
  
  Serial.print(F("Initialising the Bluefruit LE module: "));

  /* Initialise the module */
  Serial.print(F("Initialising the Bluefruit LE module: "));
  if ( !ble.begin(VERBOSE_MODE) )
  {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println( F("OK!") );

  if ( FACTORYRESET_ENABLE )
  {
    /* Perform a factory reset to make sure everything is in a known state */
    Serial.println(F("Performing a factory reset: "));
    if ( ! ble.factoryReset() ) {
      error(F("Couldn't factory reset"));
    }
  }
  
  // Change the device name to "My Bluetooth Device"
  if (! ble.sendCommandCheckOK(F("AT+GAPDEVNAME=sleeplesswaves midi keyboard")) ) {
    error(F("Could not set device name?"));
  }
  
  //ble.sendCommandCheckOK(F("AT+uartflow=off"));
  ble.echo(false);

  Serial.println("Requesting Bluefruit info:");
  /* Print Bluefruit information */
  ble.info();
  
  /* Set BLE callbacks */
  ble.setConnectCallback(connected);
  ble.setDisconnectCallback(disconnected);
  
  // Set MIDI RX callback
  midi.setRxCallback(BleMidiRX);

  Serial.println(F("Enable MIDI: "));
  if ( ! midi.begin(true) )
  {
    error(F("Could not enable MIDI"));
  }

  ble.verbose(false);
  Serial.print(F("Waiting for a connection..."));

  // set mpr121 IRQ pin to input
  pinMode(IRQ_PIN, INPUT);

  // endless loop if the mpr121 init fails
  if (! cap.begin(0x5A))
    while (1);

  // set sensitivity of touch surface, lower numbers increase sensitivity
  cap.setThreshholds(12, 1);
}

void loop() {

  ble.update(100);      // Check for incoming Bluetooth LE messages for 100ms (non blocking).

  // bail if not connected
  if (! isConnected)
    return;

  readButtons();        // Check for button presses

}

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
//                       BUTTON PRESS AND MIDI FUNCTIONS                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

// read the currently touched buttons
void readButtons() {

  // read current values
  currtouched = cap.touched();

  handle_note();

  // save current values to compare against in the next loop
  lasttouched = currtouched;

}

// deal with note on and off presses
void handle_note() {
  for (uint8_t i = 0; i < BUTTONS; i++) {
    // note on check
    if ((currtouched & _BV(i)) && !(lasttouched & _BV(i))) {
      // send MIDI note on message
      noteOn(channel, pitch[i], vel[i]);
    }
    // note off check
    if (!(currtouched & _BV(i)) && (lasttouched & _BV(i))) {
      // send MIDI note off message
      noteOff(channel, pitch[i], vel[i]);
    }
  }
}


// First parameter is the event type (0x09 = note on, 0x08 = note off).
// Second parameter is note-on/note-off, combined with the channel.
// Channel can be anything between 0-15. Typically reported to the user as 1-16.
// Third parameter is the note number (48 = middle C).
// Fourth parameter is the velocity (64 = normal, 127 = fastest).

void noteOn(byte channel, byte pitch, byte velocity) {
  midi.send(0x90 | channel, pitch, velocity);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midi.send(0x80 | channel, pitch, velocity);
}

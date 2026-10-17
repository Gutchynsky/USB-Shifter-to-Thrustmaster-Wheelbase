/* 
 *  Based on:
 *  Simplified Thrustmaster T.16000M FCS Joystick Report Parser (https://github.com/felis/USB_Host_Shield_2.0/blob/master/examples/HID/t16km/t16km.ino)
 *  &
 *  People at ISRTV.com - https://www.isrtv.com/forums/topic/24532-gearbox-connector-on-base/
 *  
 *  Modified by azzajess to suit USB Shifter 6+R+Switch (https://github.com/azzajess/USB-Shifter-to-Thrustmaster-Wheelbase)
 *  v1.2
*/

#include <usbhid.h>
#include <hiduniversal.h>
#include <usbhub.h>

// Satisfy the IDE, which needs to see the include statment in the ino too.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif

#include <SPI.h>
#include <Wire.h>

// Uncomment to print raw USB HID reports from the shifter (for finding gear codes)
//#define DEBUG_RAW
/*
Arduino 3.3V required or 5V with external power.

Base connector / note / Arduino pin
DIN6_1 /  nothing      /
DIN6_2 /  I2C-SCL      / A5
DIN6_3 /  /Shifter ON  / GND
DIN6_4 /  I2C-SDA      / A4
DIN6_5 /  Vdd          / RAW 3.3V
DIN6_6 /  Vss          / GND
*/

//commands seen from ociliscope in ISRTV forum - 
//apparently these codes have changed for different wheel versions however
//these worked for me...
byte command[14] = {
  0x00, // Shifter mode 0 - S / 0x80 - H
  0x0C, // Unknown
  0x01, // Unknown
  0x00, // Gear in H-mode
  0x00, // Gear in S-Mode 0x04 - center, 0x05 - down, 0x06 - up
  0x80, // Unknown
  0x80, // Unknown
  0x00, // Y cordinate
  0x00, // X cordinate
  0x00, // Unknown
  0x00, // Unknown
  0x00, // Unknown
  0x00, // Unknown
  0x00  // Unknown
};

//Sequential shift
enum position {
  center = 0x04,
  down = 0x05,
  up = 0x06
};

// Thrustmaster T.16000M HID report
struct GamePadEventData
{
  uint16_t  buttons;
  uint8_t   hat;
  uint16_t  x;
  uint16_t  y;
  uint8_t   twist;
  uint8_t   slider;
}__attribute__((packed));

class JoystickEvents
{
public:
  virtual void OnGamePadChanged(const GamePadEventData *evt);
};

#define RPT_GAMEPAD_LEN sizeof(GamePadEventData)

class JoystickReportParser : public HIDReportParser
{
  JoystickEvents    *joyEvents;

  uint8_t oldPad[RPT_GAMEPAD_LEN];

public:
  JoystickReportParser(JoystickEvents *evt);

  virtual void Parse(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf);
};


JoystickReportParser::JoystickReportParser(JoystickEvents *evt) :
  joyEvents(evt)
{}

void JoystickReportParser::Parse(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf)
{
/* Original.  Code caused commands to infinitely loop.
  // Checking if there are changes in report since the method was last called
  // match = true (1) when size of old pan and memcmp = 0
  //  bool match = (sizeof(oldPad) == len) && (memcmp(oldPad, buf, len) == 0);
*/
  // match is true when memcmp = 0 (basically if there is no shifting, then its false
  //so it doesnt infintely loop
  bool match = (memcmp(oldPad, buf, len) == 0);

#ifdef DEBUG_RAW
  // Debug: dump the raw HID report whenever it changes
  if (!match) {
    Serial.print("RAW len=");
    Serial.print(len);
    Serial.print(":");
    for (uint8_t i = 0; i < len; i++) {
      Serial.print(' ');
      if (buf[i] < 0x10) Serial.print('0');
      Serial.print(buf[i], HEX);
    }
    Serial.println();
  }
#endif

//   Calling Game Pad event handler
//check if nothing is happening otherwise activate commands
  if (!match && joyEvents) {
    joyEvents->OnGamePadChanged((const GamePadEventData*)buf);
    memcpy(oldPad, buf, len);
  }
}

//H mode shifting codes
void setHMode(bool isHMode) {
  if (isHMode) {
    command[0] |= 0x80;
  } else {
    command[0] &= ~0x80;
  }
}

//Switchs gear to specificed gear number and display in serial monitor
void switchHGear(byte gear) { // Gear num 0-N, 8-R
  command[3] = (0x80 >> (8-gear));
#ifdef DEBUG_RAW
  Serial.print("Gear ");
  Serial.println(gear);
#endif
}

//Switchs gear to specificed gear number and display in serial monitor
void switchSGear(position currpos) {
  command[4] = currpos;
#ifdef DEBUG_RAW
  Serial.print("SEQ Gear ");
  Serial.println(currpos);
#endif
}

#ifdef DEBUG_RAW
// Debug: I2C results (0 = base acknowledged, 2 = no ACK on address, 3 = no ACK on data, other = bus error)
uint16_t i2cOk = 0, i2cFail = 0;
uint8_t i2cLastErr = 0;
#endif

//sends command over PS2 port to T300 wheelbase
void sendCommand() {
  Wire.beginTransmission(0x01);
  Wire.write(command, 14);
  uint8_t rc = Wire.endTransmission();
#ifdef DEBUG_RAW
  if (rc == 0) i2cOk++;
  else { i2cFail++; i2cLastErr = rc; }
#else
  (void)rc;
#endif
}

void tryByte(byte nbyte, byte nbit) {
  command[nbyte] &= ~(0x01 << nbit-1);
  command[nbyte] |= (0x01 << nbit);
  // command[nbyte] |= 0x40;
  Serial.print("Check byte ");
  Serial.print(nbyte);
  Serial.print(" bit ");
  Serial.println(nbit);
}

void JoystickEvents::OnGamePadChanged(const GamePadEventData *evt)
{
/*
    States These are just example commands.
    setHMode(true); // Set to H-mode
    switchHGear(3); // Gear 3 (or any number from 0-8
    setHMode(false); // Set to Seq-mode
    switchSGear(up); // Press up in seq
    sendCommand(); 


    These numbers are exact codes i get when using debug and shifting in different gears
    these may be different depending on shifter? 
    Use debug below these if statements to find them from serial monitor and replace
    the codes from there

*/
    
  //check if any of the keypad codes are the sequential commands
  //replace as necessary.
  bool isSequential = (evt->y == 8207 || evt->y == 8271 || evt->y == 8335); // Sequential mode range
  uint8_t gear = 0; // Gear 0 (neutral) by default

  if (!isSequential) {
    // H-Mode gear selection
    setHMode(true);

    // Gear mapping based on evt->y value for H-mode
    //change values depending on your shifter (if its different)
    // Hall effect 6+R shifter (DragonRise 0079:0006): gear N = button N+2, R = button 1
    if (evt->y == 79)        gear = 1;
    else if (evt->y == 143)  gear = 2;
    else if (evt->y == 271)  gear = 3;
    else if (evt->y == 527)  gear = 4;
    else if (evt->y == 1039) gear = 5;
    else if (evt->y == 2063) gear = 6;
    else if (evt->y == 31)   gear = 8; // Reverse
     

    switchHGear(gear);
  }
  else {
    // Sequential mode gear selection
    setHMode(false);
    //gear mapping based off evt->y in S-mode
    if (evt->y == 8335)      switchSGear(up);    // Shift up
    else if (evt->y == 8271) switchSGear(down);  // Shift down
    else                     switchSGear(center); // Neutral
  }

  
  sendCommand();

/*
      Debug/Code Find Section
      Uncomment the below Code then flash the INO to arduino and connect to Shifter
      over USB. Then go through each of the gears on Shifter and mark down what
      each code is for each gear. If you have a switch like I do, then that changes
      the code of each shift. I used this to havea toggle between sequential and H shift
      For me, none of the other values had changed except for y:.
      I used this for the shift detection above.
*/

//  Serial.print("X: ");
//  Serial.print(evt->x);
//  Serial.print(" Y: ");
//  Serial.print(evt->y);
//  Serial.print(" Hat Switch: ");
//  Serial.print(evt->hat);
//  Serial.print(" Twist: ");
//  Serial.print(evt->twist);
//  Serial.print(" Slider: ");
//  Serial.print(evt->slider);
//  Serial.print(" Buttons: ");
//  Serial.print(evt->buttons);
//  Serial.println();


    //If its easier, just uncomment this for a clener clean debug once you know the only
    //value that changes is y.
#ifdef DEBUG_RAW
    Serial.print("y=");
    Serial.println(evt->y);
#endif
}

USB                                             Usb;
USBHub                                          Hub(&Usb);

// Some shifters (e.g. DragonRise 0079:0006 hall effect 6+R) NAK every interrupt poll
// until the host has read their HID report descriptor, like Windows does on attach.
class ShifterHID : public HIDUniversal {
public:
  ShifterHID(USB *p) : HIDUniversal(p) {}
protected:
  uint8_t OnInitSuccessful() override {
    uint8_t buf[64];
    pUsb->ctrlReq(bAddress, 0, bmREQ_HID_REPORT, USB_REQUEST_GET_DESCRIPTOR, 0x00,
                  HID_DESCRIPTOR_REPORT, 0x0000, sizeof(buf), sizeof(buf), buf, NULL);
    return 0;
  }
};

#ifdef DEBUG_RAW
// Debug: count interrupt poll results (NAK = device has nothing new, others = errors)
class DebugHID : public ShifterHID {
public:
  DebugHID(USB *p) : ShifterHID(p) {}
  uint16_t nakCount = 0;
  uint16_t errCount = 0;
  uint8_t lastErr = 0;
  uint8_t Poll() override {
    uint8_t rc = ShifterHID::Poll();
    if (rc == hrNAK) nakCount++;
    else if (rc) { errCount++; lastErr = rc; }
    return rc;
  }
  // Print what the driver found on the device: IDs, interfaces and endpoints
  void dump() {
    Serial.print("VID=0x"); Serial.print(VID, HEX);
    Serial.print(" PID=0x"); Serial.print(PID, HEX);
    Serial.print(" ifaces="); Serial.print(bNumIface);
    Serial.print(" eps="); Serial.print(bNumEP);
    Serial.print(" pollInterval="); Serial.println(pollInterval);
    for (uint8_t i = 0; i < bNumEP; i++) {
      Serial.print("  ep"); Serial.print(i);
      Serial.print(" addr=0x"); Serial.print(epInfo[i].epAddr, HEX);
      Serial.print(" maxPkt="); Serial.println(epInfo[i].maxPktSize);
    }
    for (uint8_t i = 0; i < bNumIface; i++) {
      Serial.print("  iface"); Serial.print(hidInterfaces[i].bmInterface);
      Serial.print(" inEpIndex="); Serial.println(hidInterfaces[i].epIndex[epInterruptInIndex]);
    }
  }
  static void printHex(const char *label, uint8_t rc, const uint8_t *buf, uint8_t len) {
    Serial.print(label); Serial.print(" rc=0x"); Serial.print(rc, HEX); Serial.print(":");
    for (uint8_t i = 0; i < len; i++) {
      if (i % 16 == 0) Serial.print("\r\n   ");
      Serial.print(' ');
      if (buf[i] < 0x10) Serial.print('0');
      Serial.print(buf[i], HEX);
    }
    Serial.println();
  }
  // Read the raw configuration descriptor and the HID report descriptor (Windows does the latter on attach)
  void dumpDescriptors() {
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    uint8_t rc = pUsb->getConfDescr(bAddress, 0, sizeof(buf), 0, buf);
    printHex("ConfDescr", rc, buf, sizeof(buf));
    memset(buf, 0, sizeof(buf));
    rc = pUsb->ctrlReq(bAddress, 0, bmREQ_HID_REPORT, USB_REQUEST_GET_DESCRIPTOR, 0x00,
                       HID_DESCRIPTOR_REPORT, 0x0000, sizeof(buf), sizeof(buf), buf, NULL);
    printHex("ReportDescr", rc, buf, sizeof(buf));
  }
};
DebugHID                                        Hid(&Usb);
#else
ShifterHID                                      Hid(&Usb);
#endif
JoystickEvents                                  JoyEvents;
JoystickReportParser                            Joy(&JoyEvents);

void setup()
{
  Serial.begin( 115200 );
#if !defined(__MIPSEL__)
  while (!Serial); // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
#endif
  Serial.println("Start");

  if (Usb.Init() == -1)
      Serial.println("OSC did not start.");

  delay( 200 );

  if (!Hid.SetReportParser(0, &Joy))
      ErrorMessage<uint8_t>(PSTR("SetReportParser"), 1  );

  //TH8A
  Wire.begin(0x03); // join i2c bus (address optional for master)
  Wire.setWireTimeout(3000, true); // never hang the loop if the bus gets stuck
  digitalWrite(13, HIGH);

  //On startup sends command neutral to initialise this. It isnt nessacary but
  //saves from moving to shift and back. Just be mindful of sequential switch i guess
  setHMode(true);
  switchHGear(0);
  sendCommand();  
}

void loop()
{
    Usb.Task();

    // Like a real TH8A, keep sending the current state to the base (every 10 ms)
    static uint32_t lastSend = 0;
    if ((uint32_t)(millis() - lastSend) >= 10) {
      lastSend = millis();
      sendCommand();
    }

#ifdef DEBUG_RAW
    // Debug: report USB host state changes (0x12 = waiting for device, 0x90 = running, 0xA0 = error)
    static uint8_t lastState = 0xFF;
    uint8_t state = Usb.getUsbTaskState();
    if (state != lastState) {
      Serial.print("USB state 0x");
      Serial.println(state, HEX);
      lastState = state;
    }

    // Debug: report whether the HID driver attached to the shifter
    static bool lastReady = false;
    bool ready = Hid.isReady();
    if (ready != lastReady) {
      Serial.print("HID ready ");
      Serial.println(ready);
      if (ready) { Hid.dump(); Hid.dumpDescriptors(); }
      lastReady = ready;
    }

    // Debug: actively ask the shifter for its input report twice a second
    static uint32_t lastPoll = 0;
    static uint8_t lastRep[8];
    static uint8_t lastRc = 0xFF;
    if (ready && (uint32_t)(millis() - lastPoll) >= 500) {
      lastPoll = millis();
      uint8_t rep[8] = {0};
      uint8_t rc = Hid.GetReport(0, 0, 0x01, 0, sizeof(rep), rep);
      if (rc != lastRc || memcmp(rep, lastRep, sizeof(rep)) != 0) {
        Serial.print("GetReport rc=0x");
        Serial.print(rc, HEX);
        Serial.print(":");
        for (uint8_t i = 0; i < sizeof(rep); i++) {
          Serial.print(' ');
          if (rep[i] < 0x10) Serial.print('0');
          Serial.print(rep[i], HEX);
        }
        Serial.println();
        memcpy(lastRep, rep, sizeof(rep));
        lastRc = rc;
      }
    }

    // Debug: every 2 s summarise interrupt polling results
    static uint32_t lastSummary = 0;
    if (ready && (uint32_t)(millis() - lastSummary) >= 2000) {
      lastSummary = millis();
      Serial.print("Poll NAK=");
      Serial.print(Hid.nakCount);
      Serial.print(" err=");
      Serial.print(Hid.errCount);
      Serial.print(" lastErr=0x");
      Serial.print(Hid.lastErr, HEX);
      Serial.print(" | I2C ok=");
      Serial.print(i2cOk);
      Serial.print(" fail=");
      Serial.print(i2cFail);
      Serial.print(" lastErr=");
      Serial.println(i2cLastErr);
      Hid.nakCount = 0;
      Hid.errCount = 0;
      i2cOk = 0;
      i2cFail = 0;
    }
#endif
}

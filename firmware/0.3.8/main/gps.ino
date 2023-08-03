

#include <TinyGPS++.h>

const unsigned long GPS_WaitAck_mS = 2000;        //number of mS to wait for an ACK response from GPS
uint8_t GPS_Reply[12];                //byte array for storing GPS reply to UBX commands (12 bytes)

uint32_t LatitudeBinary;
uint32_t LongitudeBinary;
uint16_t altitudeGps;
uint8_t hdopGps;
uint8_t sats;
char t[32]; // used to sprintf for Serial output

TinyGPSPlus _gps;

void gps_time(char * buffer, uint8_t size) {
    snprintf(buffer, size, "%02d:%02d:%02d", _gps.time.hour(), _gps.time.minute(), _gps.time.second());
}

uint16_t gps_year() {
  return _gps.date.year();
}

uint8_t gps_month() {
  return _gps.date.month();
}

uint8_t gps_day() {
  return _gps.date.day();
}

uint8_t gps_hour() {
  return _gps.time.hour();
}

uint8_t gps_minute() {
  return _gps.time.minute();
}

uint8_t gps_second() {
  return _gps.time.second();
}

uint64_t gps_time_age() {
    return _gps.time.age();
}

uint64_t gps_location_age() {
    return _gps.location.age();
}

float gps_latitude() {
    return _gps.location.lat();
}

float gps_longitude() {
    return _gps.location.lng();
}

float gps_altitude() {
    return _gps.altitude.meters();
}

float gps_hdop() {
    return _gps.hdop.hdop();
}

uint8_t gps_sats() {
    return _gps.satellites.value();
}

bool gps_time_valid() {
  return _gps.time.isValid();
}

void gps_setup() {
    ss.begin(GPS_BAUDRATE, SERIAL_8N1, GPS_TX_PIN, GPS_RX_PIN);           //format is baud, mode, UART RX data, UART TX data
    //_serial_gps.begin(GPS_BAUDRATE, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
}

boolean gps_available() {
  //return _serial_gps.available();
  return ss.available();
}

static void gps_read() {
    while (gps_available()) {
        //_gps.encode(_serial_gps.read());
        _gps.encode( ss.read() );
    }
}

bool if_good_gps_quality()
{ 
  float hdop = gps_hdop();
  
  // if 0 then gps not attached, if above 2 then it's not a good fix
  if ( hdop == 0 || hdop > 2.0 )
    return false;
  else
    return true;
}

void gps_power_off() {
  if (AXP192_FOUND)
  {
    if(axp.setPowerOutPut(GPS_POWER_PIN, AXP202_OFF) == AXP_PASS) {
      Serial.println(F("turned off GPS module"));
    } else {
      Serial.println(F("failed to turn off GPS module"));
    }
  } // end check for axp192
}

void gps_power_on() {
  if (AXP192_FOUND)
  {
    if(axp.setPowerOutPut(GPS_POWER_PIN, AXP202_ON) == AXP_PASS) {
      Serial.println("turned on GPS module");
    } else {
      Serial.println("failed to turn on GPS module");
    }
  } // end check for axp192
}

bool if_gps_on() {
  // NOTE: Assumes T-beam where GPS is on AXP's LDO3 pin
  if (AXP192_FOUND) 
    return axp.isLDO3Enable();
  else 
    return false;
}

// Careful to use this function when downlink is possible
//   because gps_read calls can cause 1/20th sec delay as it
//   reads a sentence
void gps_time_loop_wait( int milliseconds )
{
  int start = millis();
  while ( !time_loop_check( start, milliseconds ) )
  {
    os_runloop_once();
    gps_read();
    screen_loop();
  }
}

void save_gps_data()
{
  if ( gps_location_age() < 1500 && if_good_gps_quality() ) {
    LATITUDE = gps_latitude();
    LONGITUDE = gps_longitude();
    ELEVATION_GPS = gps_altitude();
  } else {
    LATITUDE = 0;
    LONGITUDE = 0;
    ELEVATION_GPS = 0;
  }
}


/*

void GPS_SetHAB()
{
  const uint8_t SetNavigation[]  = {0xB5, 0x62, 0x06, 0x24, 0x24, 0x00, 0xFF, 0xFF, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x10, 0x27, 0x00, 0x00, 0x05,
            0x00, 0xFA, 0x00, 0xFA, 0x00, 0x64, 0x00, 0x2C, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0xDC}; //44

  const byte GPS_attempts = 1; //number of times the sending of GPS config will be attempted.

  Serial.print(F("GPS High Altitude Balloon Settings "));
  size_t size = sizeof(SetNavigation); 
  GPS_Send_Config(SetNavigation, size, 4, GPS_attempts);
}

void GPS_Send_Config(const uint8_t *Progmem_ptr, byte arraysize, byte replylength, byte attempts)
{
  byte byteread,index;
  byte config_attempts = attempts;
  boolean GPS_Config_Error;

  Serial.print(F("("));
  Serial.print(arraysize); 
  Serial.print(F(") "));
  
  do
  {

    if (config_attempts == 0)
    {
      Serial.println(F("No Response from GPS"));
      GPS_Config_Error = true;
      break;
    }

   for (index = 0; index < arraysize; index++)
    {
      byteread = pgm_read_byte_near(Progmem_ptr++);
      _serial_gps.write(byteread);
      Serial.print(byteread, HEX);
      Serial.print(" ");
    }

    Progmem_ptr = Progmem_ptr - arraysize;     //put Progmem_ptr back to start value in case we need to re-send the config
 
    Serial.println();
    
    if (replylength == 0)
    {
      Serial.println(F("Reply not required"));
      break;
    }

    config_attempts--;
  } while (!GPS_WaitAck(GPS_WaitAck_mS, replylength));

  delay(50);                                         //GPS can sometimes be a bit slow getting ready for next config
}


boolean GPS_WaitAck(unsigned long waitms, byte length)
{
  //wait for Ack from GPS
  byte i, j;
  unsigned long endms;
  endms = millis() + waitms;
  byte ptr = 0;                             //used as pointer to store GPS reply
  
  do
  {
    //while (_serial_gps.available() > 0)
    //  i = _serial_gps.read();
    while ( ss.available() > 0 )
      i = ss.read();
  }
  while ((i != 0xb5) && (millis() < endms));

  if (i != 0xb5)
  {
    Serial.print(F("Timeout "));
    return false;
  }
  else
  {
    Serial.print(F("Ack "));
    Serial.print(i, HEX);

    length--;

    for (j = 1; j <= length; j++)
    {
      Serial.print(F(" "));

      //while (_serial_gps.available() == 0);
      //i = _serial_gps.read();
      while ( _ss.available() == 0 );
      i = ss.read();

      if (j < 12)
      {
        GPS_Reply[ptr++] = i;                  //save reply in buffer, but no more than 10 characters
      }

      if (i < 0x10)
      {
        Serial.print(F("0"));
      }
      Serial.print(i, HEX);
    }
  }
  Serial.println();
  return true;
}
*/

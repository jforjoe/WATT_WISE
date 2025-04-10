#include "ESP8266WiFi.h"
#include <SoftwareSerial.h>
SoftwareSerial mySerial(13, 15);  // RX, TX
//==============================================================
//
//
//=============================================================
#define LED       D0            // Led in NodeMCU at pin GPIO16 (D0).
#define LED1      D1            // Led in NodeMCU at pin GPIO5 (D1).
#define LED2      D2            // Led in NodeMCU at pin GPIO4 (D2).
#define RS485_DE  D6
#define RS485_RE  D5
//==============================================================
//
//
//=============================================================
const char* ssid = "Exe1";
const char* password = "123456789";
//==============================================================
//
//
//=============================================================
//#define ENABLE_DEBUG_MESSAGE
//==============================================================
//
//
//=============================================================
WiFiServer wifiServer(80);
char buf1[5], buf2[5];
//==============================================================
//
//
//=============================================================
unsigned char MODBUS_TXBuff[25];
//==============================================================
//
//
//=============================================================
unsigned char MODBUS_RXBuff[25];
//==============================================================
//
//
//=============================================================
char replyString[20];
//==============================================================
//
//
//=============================================================
float tempFloat;
//==============================================================
//
//
//=============================================================
union u_tag {
    byte b[4];
    float fval;
} u;
//==============================================================
//
//
//=============================================================
typedef enum
{
  UNDEFINED = 0,
  SUCCESS = 1,
  FAILED_UNKNOWN_METER_ID = 2,
  FAILED_UNKNOWN_COMMAND = 3,
  FAILED_METER_NOT_RESPONDING = 4,
  FAILED_UNKNOWN_REPLY_FROM_METER = 5
}commStatus;
//==============================================================
//
//
//=============================================================
void setup() 
{
  delay(1000);

  pinMode(LED, OUTPUT);    // On-board LED pin as output
  digitalWrite(LED, HIGH);   // Active-low, hence turn off LED

  pinMode(LED1, OUTPUT);
  digitalWrite(LED1, LOW); 
  pinMode(LED2, OUTPUT);
  digitalWrite(LED2, LOW);

  pinMode(RS485_DE, OUTPUT);
  digitalWrite(RS485_DE, LOW);  // Active High
  pinMode(RS485_RE, OUTPUT);
  digitalWrite(RS485_RE, HIGH);  // Active Low

  WiFi.begin(ssid, password);
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }
 
  wifiServer.begin();  

  // Indication of Blue LED that wifi is connected and server is running
  digitalWrite(LED, LOW);
 
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial)
  {
      ; // wait for serial port to connect. Needed for native USB port only
  }

  // Open serial communications and wait for port to open:
  mySerial.begin(9600);
  mySerial.setTimeout(1000);  // 1 second timeout
  while (!mySerial)
  {
      ; // wait for serial port to connect. Needed for native USB port only
  }  
}
//==============================================================
//
//
//=============================================================
unsigned int calcCRC(unsigned char *buf, unsigned int len)
{
    unsigned int crc = 0xFFFF;
    unsigned int pos = 0;
    while (pos < len)
    { 
        crc ^= buf[pos];          // XOR byte into least sig. byte of crc
        unsigned int i = 8;
        while (i != 0)
        {
        // Loop over each bit
            if ((crc & 0x0001) != 0)           // If the LSB is set
            {
                crc >>= 1;                       // Shift right and XOR 0xA001
                crc ^= 0xA001;
            }
            else                                // Else LSB is not set
            {
                crc >>= 1;                        // Just shift right
            }
            i = i - 1;
        }
        pos = pos + 1;
    }

    return crc;
}
//==============================================================
// When reading from input or holding register, MODBUS protocol has following bytes for transmit packet (from master nodeMCU board to EM2M)
//[0] = slave ID
//[1] = Function code, 4 = reading an input (read only) registers, 3 = reading a holding (read/ write) registers
//[2][3] = high and low address for register to be read or written into
//[4][5] = high and low count (16-bit in total) for number of points. Keep it "0  2"
//[6][7] = CRC

// When reading from input or holding register, MODBUS protocol has following bytes for received packet (from EM2M to master nodeMCU board)
//[0] = slave ID
//[1] = Function code, Same as transmit packet
//[2] = Number of bytes
//[3][4][5][6] = Number of bytes
//[7][8] = CRC  
//=============================================================
commStatus MODBUS_write_8_bytes_read_9_bytes(void)
{
  commStatus retStat = UNDEFINED;
  unsigned int calculatedCRC = 0;
  unsigned char msb, lsb;  
  unsigned char i;
  
  calculatedCRC = calcCRC(MODBUS_TXBuff, 6);
  MODBUS_TXBuff[6] = (calculatedCRC&0x00FF);
  MODBUS_TXBuff[7] = ((calculatedCRC>>8)&0x00FF);

  mySerial.flush();

  // Transmit MODBUS packet over RS485 to EM2M
  digitalWrite(RS485_DE, HIGH);
  while(mySerial.write(MODBUS_TXBuff, 8) < 8)
  {
    yield();
  }  
  digitalWrite(RS485_DE, LOW);

  // Delay added to make sure that the transmission has physically completed for the last byte. Otherwise, the last TX byte will be read as 
  // first RX byte. This delay avoids the situation.
  delay(15);

  // Wait to receive 9 bytes from EM2M over RS485, as per MODBUS standard
  digitalWrite(RS485_RE, LOW);
  if(mySerial.readBytes(MODBUS_RXBuff, 9) < 9)
  {
    retStat = FAILED_METER_NOT_RESPONDING;
  }
  else
  {
    retStat = SUCCESS;
  }
  digitalWrite(RS485_RE, HIGH);  

  //==========================================================
  // Below is a work-around. Sometime, the first byte received by nodeMCU (over RS485) is the last byte transmitted by same nodeMCU (over RS485).
  // I have inserted a delay after the transmission ends (to make sure that the last byte) is indeed transmitted physically over wire, before receiver
  // is enabled. But that delay is dangerous as too much delay may cause of first byte in reciever to be missed.
  // Hence this work around is added to check the first byte. As per MODBUS standard, the first byte should be same as first byte transmitted (which is slave ID). If it is something else like "0xFE"
  // or "0xC0", then the first byte is not really the first byte. In such case, consider the second byte as first byte.
  //==========================================================
  if(MODBUS_RXBuff[0] == MODBUS_TXBuff[0])
  {
    i = 0;
  }
  else
  {
    i = 1;
  }
  
  if(MODBUS_RXBuff[1+i] == 4) // if reply is for a function code-4 (reading input register, such as voltage, current, freqeuncy)
  {
    // Once data is received from EM2M over RS485, take the 4-bytes of data and convert them into float value
    u.b[0] = MODBUS_RXBuff[6+i];
    u.b[1] = MODBUS_RXBuff[5+i];
    u.b[2] = MODBUS_RXBuff[4+i];
    u.b[3] = MODBUS_RXBuff[3+i];
    tempFloat = u.fval;
    retStat = SUCCESS;
  }
  else if(MODBUS_RXBuff[1+i] == 3) // if reply is for a function code-3 (reading holding register, such as baud rate, meter ID, backlight state)
  {
    tempFloat = (float)MODBUS_RXBuff[4];  // In this case, data comes in [3] (MS-byte) and [4] (LS-byte) , but we are ignoring [3], since most of our data will fit in 8-bits of [4]
    retStat = SUCCESS;
  }
  else
  {
    tempFloat = 7.0;
    retStat = FAILED_UNKNOWN_REPLY_FROM_METER;
  }

  // Now, convert the float value into string to be sent to python client
  dtostrf(tempFloat, 8, 3, replyString);

#ifdef ENABLE_DEBUG_MESSAGE
   // Send data over USB-serial for debug purpose (in case, if its needed)
   Serial.print("Received data = ");
   Serial.print(MODBUS_RXBuff[0], HEX);
   Serial.print(' ');  
   Serial.print(MODBUS_RXBuff[1], HEX); 
   Serial.print(' ');
   Serial.print(MODBUS_RXBuff[2], HEX);
   Serial.print(' ');
   Serial.print(MODBUS_RXBuff[3], HEX);
   Serial.print(' ');
   Serial.print(MODBUS_RXBuff[4], HEX);
   Serial.print(' ');  
   Serial.print(MODBUS_RXBuff[5], HEX); 
   Serial.print(' ');  
   Serial.print(MODBUS_RXBuff[6], HEX); 
   Serial.print(' ');  
   Serial.print(MODBUS_RXBuff[7], HEX);  
   Serial.print(' ');  
   Serial.print(MODBUS_RXBuff[8], HEX);     
   Serial.print('\r');
   Serial.print('\n');
  #endif 

  return retStat;
}
//==============================================================
// When writing to holding register, MODBUS protocol has following bytes for transmit packet (from master nodeMCU board to EM2M)
//[0] = slave ID
//[1] = Function code, 0x10 = writing to holding (read/ write) registers
//[2][3] = high and low address for register to be read or written into
//[4][5] = high and low count (16-bit in total) for number of points. Keep it "0  2"
//[6] = number ot bytes to write = 4
//[7][8][9][10] = data of 4 bytes
//[11][12] = CRC

// When writing to holding register, MODBUS protocol has following bytes for received packet (from EM2M to master nodeMCU board)
//[0] = slave ID
//[1] = Function code, Same as transmit packet
//[2][3] = high and low address for register , same as transmit packet
//[4][5] = high and low count (16-bit in total) for number of points. Always "0  2"
//[6][7] = CRC  
//=============================================================
commStatus MODBUS_write_13_bytes_read_8_bytes(void)
{
  commStatus retStat = UNDEFINED;
  unsigned int calculatedCRC = 0;
  unsigned char msb, lsb;
  
  calculatedCRC = calcCRC(MODBUS_TXBuff, 11);
  MODBUS_TXBuff[11] = (calculatedCRC&0x00FF);
  MODBUS_TXBuff[12] = ((calculatedCRC>>8)&0x00FF);

  mySerial.flush();  

  // Transmit MODBUS packet over RS485 to EM2M
  digitalWrite(RS485_DE, HIGH);
  while(mySerial.write(MODBUS_TXBuff, 13) < 13)
  {
    yield();
  }
  digitalWrite(RS485_DE, LOW);

  // Wait to receive 8 bytes from EM2M over RS485, as per MODBUS standard
  digitalWrite(RS485_RE, LOW);
  if(mySerial.readBytes(MODBUS_RXBuff, 8) < 8)
  {
    retStat = FAILED_METER_NOT_RESPONDING;
  }
  else
  {
    retStat = SUCCESS;
  }
  digitalWrite(RS485_RE, HIGH); 

  // No need to return any value for these commands. Hence just send a dummy 0.0
  dtostrf(2.0, 8, 3, replyString);
  
#ifdef ENABLE_DEBUG_MESSAGE  
   // Send data over USB-serial for debug purpose (in case, if its needed)
   Serial.print("Received data = ");
   Serial.print(MODBUS_RXBuff[0], HEX);
   Serial.print(' ');  
   Serial.print(MODBUS_RXBuff[1], HEX); 
   Serial.print(' ');
   Serial.print(MODBUS_RXBuff[2], HEX);
   Serial.print(' ');
   Serial.print(MODBUS_RXBuff[3], HEX);
   Serial.print(' ');
   Serial.print(MODBUS_RXBuff[4], HEX);
   Serial.print(' ');  
   Serial.print(MODBUS_RXBuff[5], HEX); 
   Serial.print(' ');  
   Serial.print(MODBUS_RXBuff[6], HEX); 
   Serial.print(' ');  
   Serial.print(MODBUS_RXBuff[7], HEX);   
   Serial.print('\r');
   Serial.print('\n');
#endif   

  return retStat;  
}
//==============================================================
//
//
//=============================================================
commStatus MODBUS_EM2M(char meterID, char command)
{
  commStatus retStat = UNDEFINED;
  
  //===============================
  if(meterID == '1')
  {
    MODBUS_TXBuff[0] = 0x01;
  }
  else if(meterID == '2')
  {
    MODBUS_TXBuff[0] = 0x02;
  }
  else
  {
    return FAILED_UNKNOWN_METER_ID;
  }
  //===============================
 if(command == 'a') // read total active energy
 {
   MODBUS_TXBuff[1] = 4;   // As per MODBUS standard, '4' function code to read "input registers", which are read only
   MODBUS_TXBuff[2] = 0;   // High byte of address on EM2M is always zero
   MODBUS_TXBuff[3] = 0x01;  // Refer to EM2M manual, this is low byte of address specific to the "input register" to be read
   MODBUS_TXBuff[4] = 0;   // Number of points, high
   MODBUS_TXBuff[5] = 2;   // Number of points, low
   retStat = MODBUS_write_8_bytes_read_9_bytes();
   return retStat;
 }
 else if(command == 'b')  // "read import active energy"
 {
   MODBUS_TXBuff[1] = 4;   // As per MODBUS standard, '4' function code to read "input registers", which are read only
   MODBUS_TXBuff[2] = 0;   // High byte of address on EM2M is always zero
   MODBUS_TXBuff[3] = 0x03; // Refer to EM2M manual, this is low byte of address specific to the "input register" to be read
   MODBUS_TXBuff[4] = 0;   // Number of points, high
   MODBUS_TXBuff[5] = 2;   // Number of points, low
   retStat = MODBUS_write_8_bytes_read_9_bytes();  
   return retStat;
 }
 else if(command == 'c')  // "read export active energy"
 {
   MODBUS_TXBuff[1] = 4;   // As per MODBUS standard, '4' function code to read "input registers", which are read only
   MODBUS_TXBuff[2] = 0;   // High byte of address on EM2M is always zero
   MODBUS_TXBuff[3] = 0x05; // Refer to EM2M manual, this is low byte of address specific to the "input register" to be read
   MODBUS_TXBuff[4] = 0;   // Number of points, high
   MODBUS_TXBuff[5] = 2;   // Number of points, low 
   retStat = MODBUS_write_8_bytes_read_9_bytes(); 
   return retStat;  
 }
 else if(command == 'd')  // "read total reactive energy"
 {
   MODBUS_TXBuff[1] = 4;   // As per MODBUS standard, '4' function code to read "input registers", which are read only
   MODBUS_TXBuff[2] = 0;   // High byte of address on EM2M is always zero
   MODBUS_TXBuff[3] = 0x07; // Refer to EM2M manual, this is low byte of address specific to the "input register" to be read
   MODBUS_TXBuff[4] = 0;   // Number of points, high
   MODBUS_TXBuff[5] = 2;   // Number of points, low    
   retStat = MODBUS_write_8_bytes_read_9_bytes();
   return retStat;
 }
 else if(command == 'e')  // "read import reactive energy"
 {
   MODBUS_TXBuff[1] = 4;   // As per MODBUS standard, '4' function code to read "input registers", which are read only
   MODBUS_TXBuff[2] = 0;   // High byte of address on EM2M is always zero
   MODBUS_TXBuff[3] = 0x09; // Refer to EM2M manual, this is low byte of address specific to the "input register" to be read
   MODBUS_TXBuff[4] = 0;   // Number of points, high
   MODBUS_TXBuff[5] = 2;   // Number of points, low 
   retStat = MODBUS_write_8_bytes_read_9_bytes(); 
   return retStat;  
 }
 else if(command == 'f')  // "read export reactive energy"
 {
   MODBUS_TXBuff[1] = 4;   // As per MODBUS standard, '4' function code to read "input registers", which are read only
   MODBUS_TXBuff[2] = 0;   // High byte of address on EM2M is always zero
   MODBUS_TXBuff[3] = 0x0B; // Refer to EM2M manual, this is low byte of address specific to the "input register" to be read
   MODBUS_TXBuff[4] = 0;   // Number of points, high
   MODBUS_TXBuff[5] = 2;   // Number of points, low
   retStat = MODBUS_write_8_bytes_read_9_bytes(); 
   return retStat;   
 }
 else if(command == 'g')  // "read apparent energy"
 {
   MODBUS_TXBuff[1] = 4;   // As per MODBUS standard, '4' function code to read "input registers", which are read only
   MODBUS_TXBuff[2] = 0;   // High byte of address on EM2M is always zero
   MODBUS_TXBuff[3] = 0x0D; // Refer to EM2M manual, this is low byte of address specific to the "input register" to be read
   MODBUS_TXBuff[4] = 0;   // Number of points, high
   MODBUS_TXBuff[5] = 2;   // Number of points, low 
   retStat = MODBUS_write_8_bytes_read_9_bytes(); 
   return retStat;  
 }
  if(command == 'h')  // "read active power"
  {
    MODBUS_TXBuff[1] = 4;   // As per MODBUS standard, '4' function code to read "input registers", which are read only
    MODBUS_TXBuff[2] = 0;   // High byte of address on EM2M is always zero
    MODBUS_TXBuff[3] = 0x0F; // Refer to EM2M manual, this is low byte of address specific to the "input register" to be read
    MODBUS_TXBuff[4] = 0;   // Number of points, high
    MODBUS_TXBuff[5] = 2;   // Number of points, low 
    retStat = MODBUS_write_8_bytes_read_9_bytes();   
    return retStat;
  }
 else if(command == 'i')  // "read reactive power"
 {
   MODBUS_TXBuff[1] = 4;   // As per MODBUS standard, '4' function code to read "input registers", which are read only
   MODBUS_TXBuff[2] = 0;   // High byte of address on EM2M is always zero
   MODBUS_TXBuff[3] = 0x11; // Refer to EM2M manual, this is low byte of address specific to the "input register" to be read
   MODBUS_TXBuff[4] = 0;   // Number of points, high
   MODBUS_TXBuff[5] = 2;   // Number of points, low  
   retStat = MODBUS_write_8_bytes_read_9_bytes();  
   return retStat;
 } 
 else if(command == 'j')  // "read apparent power"
 {
   MODBUS_TXBuff[1] = 4;   // As per MODBUS standard, '4' function code to read "input registers", which are read only
   MODBUS_TXBuff[2] = 0;   // High byte of address on EM2M is always zero
   MODBUS_TXBuff[3] = 0x13; // Refer to EM2M manual, this is low byte of address specific to the "input register" to be read
   MODBUS_TXBuff[4] = 0;   // Number of points, high
   MODBUS_TXBuff[5] = 2;   // Number of points, low   
   retStat = MODBUS_write_8_bytes_read_9_bytes();
   return retStat; 
 }   
  else if(command == 'k') // "read voltage"
  {
    MODBUS_TXBuff[1] = 4;   // As per MODBUS standard, '4' function code to read "input registers", which are read only
    MODBUS_TXBuff[2] = 0;   // High byte of address on EM2M is always zero
    MODBUS_TXBuff[3] = 0x15;   // Refer to EM2M manual, this is low byte of address specific to the "input register" to be read
    MODBUS_TXBuff[4] = 0;   // Number of points, high
    MODBUS_TXBuff[5] = 2;   // Number of points, low   
    retStat = MODBUS_write_8_bytes_read_9_bytes(); 
    return retStat;
  }
  else if(command == 'l') // "read current"
  {
    MODBUS_TXBuff[1] = 4;   // As per MODBUS standard, '4' function code to read "input registers", which are read only
    MODBUS_TXBuff[2] = 0;   // High byte of address on EM2M is always zero
    MODBUS_TXBuff[3] = 0x17;  // Refer to EM2M manual, this is low byte of address specific to the "input register" to be read
    MODBUS_TXBuff[4] = 0;   // Number of points, high
    MODBUS_TXBuff[5] = 2;   // Number of points, low   
    retStat = MODBUS_write_8_bytes_read_9_bytes(); 
    return retStat;
  }
  else if(command == 'm') // "read power factor"
  {
    MODBUS_TXBuff[1] = 4;   // As per MODBUS standard, '4' function code to read "input registers", which are read only
    MODBUS_TXBuff[2] = 0;   // High byte of address on EM2M is always zero
    MODBUS_TXBuff[3] = 0x19;  // Refer to EM2M manual, this is low byte of address specific to the "input register" to be read
    MODBUS_TXBuff[4] = 0;   // Number of points, high
    MODBUS_TXBuff[5] = 2;   // Number of points, low  
    retStat = MODBUS_write_8_bytes_read_9_bytes();  
    return retStat;
  }
  else if(command == 'n') // "read frequency"
  {
    MODBUS_TXBuff[1] = 4;   // As per MODBUS standard, '4' function code to read "input registers", which are read only
    MODBUS_TXBuff[2] = 0;   // High byte of address on EM2M is always zero
    MODBUS_TXBuff[3] = 0x1B;  // Refer to EM2M manual, this is low byte of address specific to the "input register" to be read
    MODBUS_TXBuff[4] = 0;   // Number of points, high
    MODBUS_TXBuff[5] = 2;   // Number of points, low  
    retStat = MODBUS_write_8_bytes_read_9_bytes();  
    return retStat;
  }  
 else if(command == 'o')  // "read max demand active power"
 {
   MODBUS_TXBuff[1] = 4;   // As per MODBUS standard, '4' function code to read "input registers", which are read only
   MODBUS_TXBuff[2] = 0;   // High byte of address on EM2M is always zero
   MODBUS_TXBuff[3] = 0x1D;  // Refer to EM2M manual, this is low byte of address specific to the "input register" to be read
   MODBUS_TXBuff[4] = 0;   // Number of points, high
   MODBUS_TXBuff[5] = 2;   // Number of points, low  
   retStat = MODBUS_write_8_bytes_read_9_bytes();  
   return retStat;
 }
 else if(command == 'p')    // "read max demand reactive power"
 {
   MODBUS_TXBuff[1] = 4;   // As per MODBUS standard, '4' function code to read "input registers", which are read only
   MODBUS_TXBuff[2] = 0;   // High byte of address on EM2M is always zero
   MODBUS_TXBuff[3] = 0x1F;  // Refer to EM2M manual, this is low byte of address specific to the "input register" to be read
   MODBUS_TXBuff[4] = 0;   // Number of points, high
   MODBUS_TXBuff[5] = 2;   // Number of points, low 
   retStat = MODBUS_write_8_bytes_read_9_bytes();   
   return retStat;
 }
 else if(command == 'q')  // "read max demand apparent power"
 {
   MODBUS_TXBuff[1] = 4;   // As per MODBUS standard, '4' function code to read "input registers", which are read only
   MODBUS_TXBuff[2] = 0;   // High byte of address on EM2M is always zero
   MODBUS_TXBuff[3] = 0x21;  // Refer to EM2M manual, this is low byte of address specific to the "input register" to be read
   MODBUS_TXBuff[4] = 0;   // Number of points, high
   MODBUS_TXBuff[5] = 2;   // Number of points, low 
   retStat = MODBUS_write_8_bytes_read_9_bytes(); 
   return retStat;  
 }
 else if(command == 'r')  // "read baud rate"
 {
   MODBUS_TXBuff[1] = 3;   // As per MODBUS standard, '3' function code to READ "holding registers", which are read/ write
   MODBUS_TXBuff[2] = 0;   // High byte of address on EM2M is always zero
   MODBUS_TXBuff[3] = 0x0A;  // Refer to EM2M manual, this is low byte of address specific to the "holding register". Due to print error on EM2M manual, this register is called pulse dur.
   MODBUS_TXBuff[4] = 0;   // Number of points, high
   MODBUS_TXBuff[5] = 2;   // Number of points, low   
   retStat = MODBUS_write_8_bytes_read_9_bytes(); 
   return retStat;
 }
 else if(command == 's')  // "read backlight"
 {
   MODBUS_TXBuff[1] = 3;   // As per MODBUS standard, '3' function code to READ "holding registers", which are read/ write
   MODBUS_TXBuff[2] = 0;   // High byte of address on EM2M is always zero
   MODBUS_TXBuff[3] = 0x0D;  // Refer to EM2M manual, this is low byte of address specific to the "holding register". Due to print error on EM2M manual, this register is called Stop bit.
   MODBUS_TXBuff[4] = 0;   // Number of points, high
   MODBUS_TXBuff[5] = 2;   // Number of points, low    
   retStat = MODBUS_write_8_bytes_read_9_bytes();
   return retStat;
 }
 else if(command == 't')  // "read meterID"
 {
   MODBUS_TXBuff[1] = 3;   // As per MODBUS standard, '3' function code to READ "holding registers", which are read/ write
   MODBUS_TXBuff[2] = 0;   // High byte of address on EM2M is always zero
   MODBUS_TXBuff[3] = 0x01;  // Refer to EM2M manual, this is low byte of address specific to the "holding register". Due to print error on EM2M manual, this register is called password.
   MODBUS_TXBuff[4] = 0;   // Number of points, high
   MODBUS_TXBuff[5] = 2;   // Number of points, low   
   retStat = MODBUS_write_8_bytes_read_9_bytes(); 
   return retStat;
 }    
 else if(command == 'u')  // "backlight ON"
 {
   MODBUS_TXBuff[1] = 0x10;   // As per MODBUS standard, '0x10' function code to WRITE "holding registers", which are read/ write
   MODBUS_TXBuff[2] = 0;   // High byte of address on EM2M is always zero
   MODBUS_TXBuff[3] = 0x0D;  // Refer to EM2M manual, this is low byte of address specific to the "holding register". Due to print error on EM2M manual, this register is called Stop bit.
   MODBUS_TXBuff[4] = 0;   // Number of points, high
   MODBUS_TXBuff[5] = 2;   // Number of points, low 
   MODBUS_TXBuff[6] = 4;  // Byte count
   MODBUS_TXBuff[7] = 0;  // data-1
   MODBUS_TXBuff[8] = 0;  // data-2
   MODBUS_TXBuff[9] = 0;  // data-3
   MODBUS_TXBuff[10] = 0;  // data-4
   retStat = MODBUS_write_13_bytes_read_8_bytes(); 
   return retStat;
 }
 else if(command == 'v')  // "backlight OFF"
 {
   MODBUS_TXBuff[1] = 0x10;   // As per MODBUS standard, '0x10' function code to WRITE "holding registers", which are read/ write
   MODBUS_TXBuff[2] = 0;   // High byte of address on EM2M is always zero
   MODBUS_TXBuff[3] = 0x0D;  // Refer to EM2M manual, this is low byte of address specific to the "holding register". Due to print error on EM2M manual, this register is called Stop bit.
   MODBUS_TXBuff[4] = 0;   // Number of points, high
   MODBUS_TXBuff[5] = 2;   // Number of points, low 
   MODBUS_TXBuff[6] = 4;  // Byte count
   MODBUS_TXBuff[7] = 0;  // data-1
   MODBUS_TXBuff[8] = 1;  // data-2
   MODBUS_TXBuff[9] = 0;  // data-3
   MODBUS_TXBuff[10] = 0;  // data-4
   retStat = MODBUS_write_13_bytes_read_8_bytes(); 
   return retStat;
 }  
  else
  {
    // unknown command   
    return FAILED_UNKNOWN_COMMAND;    
  }
}
//==============================================================
//
//
//=============================================================
void loop() 
{ // run over and over

  commStatus stat = UNDEFINED;
  
  WiFiClient client = wifiServer.available();
 
  if (client)
  {
    while (client.connected())
    {
      if (client.available())
      {
        String line_1 = client.readStringUntil('\r');
        String line_2 = client.readStringUntil('\r');

        line_1.toCharArray(buf1, 2);  // read the meter ID into a buffer (from "String" to "char array")
        line_2.toCharArray(buf2, 2);  // read the meter ID into a buffer (from "String" to "char array")

        stat = MODBUS_EM2M(buf1[0], buf2[0]); // Meter ID is in buf1[0] and command is in buf2[0]
        if(stat == SUCCESS)
        {
          client.write("Success");
          client.write(replyString);    // NULL terminated string is must for this function to work
        }
        else if(stat == FAILED_UNKNOWN_METER_ID)
        {
          client.write("Unknown meter ID");
          client.write("3.0");
        }
        else if(stat == FAILED_UNKNOWN_COMMAND)
        {
          client.write("Uknown command");
          client.write("4.0");
        }        
        else if(stat == FAILED_METER_NOT_RESPONDING)
        {
          client.write("Meter not responding");
          client.write("5.0");
        }
        else if(stat == FAILED_UNKNOWN_REPLY_FROM_METER)
        {
          client.write("Unknown data received from meter");
          client.write("6.0");
        }        
      }
    }
 
    while (client.available()) {
      // but first, let client finish its request
      // that's diplomatic compliance to protocols
      // (and otherwise some clients may complain, like curl)
      // (that is an example, prefer using a proper webserver library)
      client.read();
    }
  
    client.stop();
  }  
}

#define ARM_MATH_CM0PLUS

#include <XBee.h>
#include <SPI.h>
#include <Wire.h>
#include <RV8523.h>

#include <ADE9153A.h>
#include <ADE9153AAPI.h>

/* Basic initializations */
#define SPI_SPEED 1000000       //SPI Speed
#define CS_PIN 17               //8-->Arduino Zero. 15-->ESP8266 
#define ADE9153A_RESET_PIN A5   //On-board Reset Pin
#define USER_INPUT 5            //On-board User Input Button Pin
#define LED 6                   //On-board LED pin

ADE9153AClass ade9153A;
RV8523 RTC;

XBee xbee = XBee();
uint8_t payload[] = {'$', ',', '1', ',', 0, 0, 0, 0, 0, 0, 0, ',', 0, 0, 0, 0, 0, '-', 0, 0, 0, '-', 0, 0, 0, ' ', 0, 0, 0, ':', 0, 0, 0, ':', 0, 0, 0, ',', '@'};

XBeeAddress64 addr64 = XBeeAddress64(0x0013A200, 0x421BE3A9);
ZBTxRequest tx = ZBTxRequest(addr64, payload, sizeof(payload));

struct EnergyRegs energyVals;  //Energy register values are read and stored in EnergyRegs structure
struct PowerRegs powerVals;    //Metrology data can be accessed from these structures
struct RMSRegs rmsVals;
struct PQRegs pqVals;
struct AcalRegs acalVals;
struct Temperature tempVal;

String outputString = "";
String inputString = "";
String subString[7] = {"", "", "", "" ,"", "", ""};
int index[5];
int data_byte_counter = 0;
uint8_t Ssecond;
uint8_t Sminute;
uint8_t Shour;
uint8_t Sday;
uint8_t Smonth;
uint16_t Syear;

bool accept_data = false;
bool stringComplete = false;

void readandwrite(void);
void resetADE9153A(void);

int ledState = LOW;
int inputState = LOW;
unsigned long lastReport = 0;
const long reportInterval = 1000;
const long blinkInterval = 500;

char vrmsstr[6];
char yearstr[4];
char monthstr[2];
char daystr[2];
char hourstr[2];
char minstr[2];
char secstr[2];

void setup()
{
  cli();
  CLKPR = _BV(CLKPCE);
  CLKPR = 0;
//  PCICR = _BV(PCIE0);
//  PCMSK0 = _BV(PCINT4);
//  MCUCR = _BV(IVCE)|_BV(IVSEL);
  sei();
  /* Pin and serial monitor setup */
  pinMode(LED, OUTPUT);
  pinMode(USER_INPUT, INPUT);
  pinMode(ADE9153A_RESET_PIN, OUTPUT);
  digitalWrite(ADE9153A_RESET_PIN, HIGH);
  Serial.begin(2400);
  Serial1.begin(2400);
  xbee.setSerial(Serial1);
  RTC.begin();

  RTC.set24HourMode();
  //RTC.set(0, 11, 14, 17, 11, 2023);
  RTC.batterySwitchOver(1);
  RTC.start();

  resetADE9153A();            //Reset ADE9153A for clean startup
  delay(1000);
  /*SPI initialization and test*/
  bool commscheck = ade9153A.SPI_Init(SPI_SPEED, CS_PIN); //Initialize SPI
  if (!commscheck) {
    Serial.println("ADE9153A Shield not detected. Plug in Shield and reset the Arduino");
    while (!commscheck) {     //Hold until arduino is reset
      delay(1000);
    }
  }

  ade9153A.SetupADE9153A(); //Setup ADE9153A according to ADE9153AAPI.h
  /* Read and Print Specific Register using ADE9153A SPI Library */
  Serial.println(String(ade9153A.SPI_Read_32(REG_VERSION_PRODUCT), HEX)); // Version of IC
  ade9153A.SPI_Write_32(REG_AIGAIN, -268435456); //AIGAIN to -1 to account for IAP-IAN swap
  delay(500);
  //SIGNAL STR LEDs
  pinMode(6, OUTPUT);
  pinMode(30, OUTPUT);
  pinMode(11, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  //STATUS LEDs
  pinMode(22, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(4, OUTPUT);
  //PCINT4 pin
  delay(1000);
  digitalWrite(4, HIGH);
  //SIGNAL STR LEDs
  digitalWrite(6, HIGH);
  digitalWrite(30, HIGH);
  digitalWrite(11, HIGH);
  digitalWrite(9, HIGH);
  digitalWrite(10, HIGH);
}


void loop()
{
  /* Main loop */
  /*test*/
  /* Returns metrology to the serial monitor and waits for USER_INPUT button press to run autocal */
  unsigned long currentReport = millis();
  readandwrite();
  //float irms = rmsVals.CurrentRMSValue / 1000;
  float vrms = rmsVals.VoltageRMSValue / 1000;
  //float rp = powerVals.ActivePowerValue / 1000;
  //float qp = powerVals.FundReactivePowerValue / 1000;
  //float sp = powerVals.ApparentPowerValue / 1000;
  //float pf = pqVals.PowerFactorValue;
  float freq = pqVals.FrequencyValue;
  //float temp = tempVal.TemperatureVal;

  uint8_t sec, min, hour, day, month;
  uint16_t year;
  //get time from RTC
  RTC.get(&sec, &min, &hour, &day, &month, &year);
  //outputString = String('$') + String(',') + String(DEVICE_ID) + String(',') + String(irms, 3) + String(',') + \
                 String(vrms, 3) + String(',') + String(rp, 3) + String(',') + String(qp, 3) + String(',') + String(sp, 3) + \
                 String(',') + String(pf, 3) + String(',') + String(freq, 3) + String(',') + String(temp, 3) + String(',') + \
                 String(year, DEC) + String('-') + String(month, DEC) + String('-') + String(day) + String(' ') + \
                 String(hour, DEC) + String(':') + String(min, DEC) + String(':') + String(sec, DEC) + String(',') + String('@');
  //outputString = String('$') + String(',') + String(DEVICE_ID) + String(',')+ String(vrms, 3) + String(',') + String(freq, 3) + \
                 String(',') + String(year, DEC) + String('-') + String(month, DEC) + String('-') + String(day) + \
                 String(' ') + String(hour, DEC) + String(':') + String(min, DEC) + String(':') + String(sec, DEC) + \
                 String(',') + String('@');
  
  dtostrf(vrms, 7, 2, vrmsstr);
  sprintf(yearstr,  "%u", year);
  sprintf(monthstr,  "%u", month);
  sprintf(daystr,  "%u", day);
  sprintf(hourstr,  "%u", hour);
  sprintf(minstr,  "%u", min);
  sprintf(secstr,  "%u", sec);
  payload[4] = vrmsstr[0];
  payload[5] = vrmsstr[1];
  payload[6] = vrmsstr[2];
  payload[7] = vrmsstr[3];
  payload[8] = vrmsstr[4];
  payload[9] = vrmsstr[5];
  payload[10] = vrmsstr[6];
  payload[12] = yearstr[0];
  payload[13] = yearstr[1];
  payload[14] = yearstr[2];
  payload[15] = yearstr[3];
  payload[16] = yearstr[4];
  payload[18] = monthstr[0];
  payload[19] = monthstr[1];
  payload[20] = monthstr[2];
  payload[22] = daystr[0];
  payload[23] = daystr[1];
  payload[24] = daystr[2];
  payload[26] = hourstr[0];
  payload[27] = hourstr[1];
  payload[28] = hourstr[2];
  payload[30] = minstr[0];
  payload[31] = minstr[1];
  payload[32] = minstr[2];
  payload[34] = secstr[0];
  payload[35] = secstr[1];
  payload[36] = secstr[2];

  xbee.send(tx);
  
  if (vrms < 210)
  {
    //outputString = String('$') + String(',') + String(DEVICE_ID) + String(',') + String(vrms, 3) + String(',') + String('@');
    Serial1.println(outputString);
    digitalWrite(4, LOW);
    digitalWrite(6, LOW);
    digitalWrite(30, LOW);
    digitalWrite(11, LOW);
    digitalWrite(9, LOW);
    digitalWrite(10, LOW);
    digitalWrite(12, LOW);
    digitalWrite(22, LOW);
    //Serial.println("VOLTAGE SAG!");
  }
  else
  {
    digitalWrite(4, HIGH);
    digitalWrite(6, HIGH);
    digitalWrite(30, HIGH);
    digitalWrite(11, HIGH);
    digitalWrite(9, HIGH);
    digitalWrite(10, HIGH);
    if ((currentReport - lastReport) >= reportInterval) {
      lastReport = currentReport;
      //Serial.println(outputString);
      Serial1.println(outputString);
      delay(50);
      digitalWrite(12, HIGH);
      digitalWrite(22, LOW);
      delay(50);
      digitalWrite(12, LOW);
      digitalWrite(22, HIGH);
    }
  }
  
  //FOR SETTING RTC
  while (Serial1.available() > 0) {
    // get the new byte:
    inputString = Serial1.readStringUntil('@');
    //Serial1.println(inputString);
    if (inputString.startsWith("$") && inputString.endsWith(","))
    {
      stringComplete = true;
    }
    else
    {
      stringComplete = false;
    }
  }
  if(stringComplete)
  {
    index[0] = inputString.indexOf(',');  //finds location of first ,
    subString[0] = inputString.substring(0, index[0]);   //captures first data String
    index[1] = inputString.indexOf(',', index[0]+1 );   
    subString[1] = inputString.substring(index[0]+1, index[1]+1);  //Sec
    index[2] = inputString.indexOf(',', index[1]+1 );
    subString[2] = inputString.substring(index[1]+1, index[2]+1); //Min
    index[3] = inputString.indexOf(',', index[2]+1 );
    subString[3] = inputString.substring(index[2]+1, index[3]+1); //Hour
    index[4] = inputString.indexOf(',', index[3]+1 );
    subString[4] = inputString.substring(index[3]+1, index[4]+1); //Day
    index[5] = inputString.indexOf(',', index[4]+1 );
    subString[5] = inputString.substring(index[4]+1, index[5]+1); //Month
    index[6] = inputString.indexOf(',', index[5]+1 );
    subString[6] = inputString.substring(index[5]+1, index[6]+1 ); //Year
    int Ssecond = subString[1].toInt();
    int Sminute = subString[2].toInt();
    int Shour = subString[3].toInt();
    int Sday = subString[4].toInt();
    int Smonth = subString[5].toInt();
    int Syear = subString[6].toInt();
    RTC.stop();
    delay(50);
    RTC.set(Ssecond, Sminute, Shour, Sday, Smonth, Syear);
    delay(50);
    RTC.start();
    Serial1.println("TIME SET!");
    stringComplete = false;
    inputString = "";
  }
}

void readandwrite()
{
  /* Read and Print WATT Register using ADE9153A Read Library */
  ade9153A.ReadPowerRegs(&powerVals);    //Template to read Power registers from ADE9000 and store data in Arduino MCU
  ade9153A.ReadRMSRegs(&rmsVals);
  ade9153A.ReadPQRegs(&pqVals);
  ade9153A.ReadTemperature(&tempVal);

}

void resetADE9153A(void)
{
  digitalWrite(ADE9153A_RESET_PIN, LOW);
  delay(100);
  digitalWrite(ADE9153A_RESET_PIN, HIGH);
  delay(1000);
  Serial.println("Reset Done");
}

/*
This is coworking door inside
rfid with arduino connected to the esp-link
*/

#include <Arduino.h>
#include "avr/wdt.h"

#include <SPI.h>
#include <MFRC522.h>

#include <ELClient.h>
#include <ELClientCmd.h>
#include <ELClientMqtt.h>

#define SS_PIN 10 //SS for RFID
#define RST_PIN 9 // RST for RFID (not connect)
#define open_pin 8

void printDec(byte *buffer, byte bufferSize);
void CheckCard(); //test dor with 4 cards
//void serialEvent();


//SoftwareSerial mySerial(2, 3); // RX, TX
String ID = "";
// String msg="";
int myTimeout = 50;
byte nuidPICC = 0;
static uint32_t last;
bool flagRaley = false;
#define del 2000

MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the RFID class

/*
esp-link Initialize block
*/
// Initialize a connection to esp-link using the normal hardware serial port both for
// SLIP and for debug messages.
ELClient esp(&Serial, &Serial);

// Initialize CMD client (for GetTime)
ELClientCmd cmd(&esp);

// Initialize the MQTT client
ELClientMqtt mqtt(&esp);

// Callback made from esp-link to notify of wifi status changes
// Here we just print something out for grins
void wifiCb(void* response) {
  ELClientResponse *res = (ELClientResponse*)response;
  if (res->argc() == 1) {
    uint8_t status;
    res->popArg(&status, 1);

    if(status == STATION_GOT_IP) {
      Serial.println("WIFI CONNECTED");
    } else {
      Serial.print("WIFI NOT READY: ");
      Serial.println(status);
    }
  }
}

bool connected;
const char* cardID_mqtt = "AS/DoorCoworkingIn/cardID";
const char* server_resp1 = "AS/DoorCoworkingIn/server_response";
const char* server_resp2 = "AS/DoorCoworkingOut/server_response";

// Callback when MQTT is connected
void mqttConnected(void* response) {
  Serial.println("MQTT connected!");
  //mqtt.subscribe(cardID_mqtt);
  mqtt.subscribe(server_resp1);
  mqtt.subscribe(server_resp2);

  connected = true;
}

void mqttDisconnected(void* response) {
  Serial.println("MQTT disconnected");
  connected = false;
}

void mqttData(void* response) {
  ELClientResponse *res = (ELClientResponse *)response;

  Serial.print("Received: topic=");
  String topic = res->popString();
  Serial.println(topic);

  Serial.print("data=");
  String data = res->popString();
  Serial.println(data);

  if ((topic == server_resp1) || (topic == server_resp2)) {
    if (data == "yes") {
      // tft.fillScreen(WHITE);
      // tft.setCursor (45, 100);
      // tft.setTextSize (3);
      // tft.setTextColor(GREEN);
      // tft.println(utf8rus("Відчинено"));
      digitalWrite(open_pin, LOW);
      nuidPICC = 0;
      flagRaley = true;
    }
    // else if (data == "no") {
    //   tft.fillScreen(WHITE);
    //   tft.setCursor (45, 100);
    //   tft.setTextSize (3);
    //   tft.setTextColor(RED);
    //   tft.println(utf8rus("Відмова"));
    //   nuidPICC = 0;
    // }

    last = millis();
    // flag_screen = true;
    return;
  }
}

void mqttPublished(void* response) {
  Serial.println("MQTT published");
}
//end esp-link Initialize block

//setup Arduino
void setup() {
  pinMode(open_pin, OUTPUT);
  digitalWrite(open_pin, HIGH);
  //delay(15000);
  //esp MQTT setup
  Serial.begin(57600);
  //mySerial.begin(9600);
  //Serial.setTimeout(myTimeout);
  //mySerial.setTimeout(myTimeout);
  pinMode(open_pin, OUTPUT);
  digitalWrite(open_pin, HIGH);

  // Sync-up with esp-link, this is required at the start of any sketch and initializes the
  // callbacks to the wifi status change callback. The callback gets called with the initial
  // status right after Sync() below completes.
  esp.wifiCb.attach(wifiCb); // wifi status change callback, optional (delete if not desired)
  bool ok;
  do {
    ok = esp.Sync();      // sync up with esp-link, blocks for up to 2 seconds
    if (!ok) Serial.println("EL-Client sync failed!");
  } while(!ok);
  Serial.println("EL-Client synced!");

  // Set-up callbacks for events and initialize with es-link.
  mqtt.connectedCb.attach(mqttConnected);
  mqtt.disconnectedCb.attach(mqttDisconnected);
  mqtt.publishedCb.attach(mqttPublished);
  mqtt.dataCb.attach(mqttData);
  mqtt.setup();
  if (connected)
  {
    Serial.println("EL-MQTT ready");
  }
  // else wdt_enable(WDTO_2S);


  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522
}

void loop() {
  esp.Process();

  // if (!connected)
  // {
  //   wdt_enable(WDTO_2S);
    // while (!connected) {}
    // wdt_disable();
  // }

  if (((millis()-last) > del) & flagRaley) {
    nuidPICC = 0;
    digitalWrite(open_pin, HIGH);
    flagRaley = false;
  }

  // while (Serial.available()) {
  //   String myString = Serial.readString();
  //
  //   if (myString == "yes")
  //   {
  //     digitalWrite(open_pin, LOW);
  //   }}
  // if (mySerial.available()) {
  //   msg = mySerial.readString();
  //   Serial.print(msg);
  // }
  //
  // if (Serial.available()) {
  //   msg = Serial.readString();
  //   mySerial.print(msg);
  // }
  //
  // msg = "";

  // Look for new cards
  if ( ! rfid.PICC_IsNewCardPresent())
    return;

  // Verify if the NUID has been readed
  if ( ! rfid.PICC_ReadCardSerial())
    return;

  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);

  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&
    piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
    piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("Your tag is not of type MIFARE Classic."));
    return;
  }
  if (rfid.uid.uidByte[0] != nuidPICC)
    {
      nuidPICC = rfid.uid.uidByte[0];

      printDec(rfid.uid.uidByte, rfid.uid.size);
      //CheckCard();
      Serial.println(ID);
      char buf[ID.length()+1];
      ID.toCharArray(buf, ID.length()+1);
      mqtt.publish(cardID_mqtt, buf);

      last = millis();
      flagRaley = true;
    }
  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();

}

// void CheckCard()
// {
//   String Cards[] = {"14441160124",
//                     "150162859",
//                     "112249150124",
//                     "23351217172"};
//   for (int i = 0; i < 4; i ++)
//   {
//     if (ID == Cards[i])
//     {
//       digitalWrite(open_pin, LOW);
//       //Serial.println("OK");
//     }
//     //else {Serial.println("NOT");}
//   }
// }
/**
 * Helper routine to dump a byte array as dec values to Serial.
 */
void printDec(byte *buffer, byte bufferSize)
{
  ID = "";
  for (byte i = 0; i < bufferSize; i++)
  {
    ID.concat(String(buffer[i], DEC));
  }
}

// void serialEvent() {
//   while (Serial.available()) {
//     String myString = Serial.readString();
//
//     if (myString == "yes")
//     {
//       digitalWrite(open_pin, LOW);
//     }
//
//   }
// }

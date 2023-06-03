/**
  Web server
    To send request such as
      Read RFID card
      Write RFID card
  Web socket
    To listen to processes such as
      Waiting while writing to rfid
      Read tag before adding batch
*/
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WebSocketsServer.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266TrueRandom.h>
#include "Hash.h"

#define SS_PIN D8
#define RST_PIN D0

void setupInitialHotspot();
void setupWifi();
void handle_OnConnect();
void handle_WifiConnect();
void setupWebSocket();
void setupRFC522();
int writeBlock(int blockNumber, byte arrayAddress[], int size);
int readBlock(int blockNumber, byte arrayAddress[], int num);
void writeCard(byte content[], int size);
void readCard(byte content[], int size, int num);
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t lenght);
void send_json_data(String msg, String type, int num);

ESP8266WebServer server(80);
WebSocketsServer socket(81);
MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

int block = 2;
String ssid = "sairaj";
String pass = "12345678";

const char *host = "birdflu";
const char *initial_ssid = "espHotpspot";
const char *initial_pass = "12345678";
const int TAG_SIZE = 16;
int card_read = 0;
byte readbackblock[TAG_SIZE];

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\nStarted module...");
  setupInitialHotspot();
}

void handle_OnConnect() {
  String s;
  s = "<html>";
  s += "<h1>Please enter wifi ssid and password</h1><br>\
          <form action='/connect-wifi' method='get'>\
            <label>ssid</label>\
            <input type='text' name='ssid'></input><br>\
            <label>password</label>\
            <input type='password' name='password'></input>\
            <button type='submit'>Submit</button>\
          </form>";
  s += "</html>\r\n\r\n";
  server.send(200, "text/html", s);
}

void handle_WifiConnect() {
  ssid = server.arg("ssid");
  pass = server.arg("password");

  String s;
  s = "<html>";
  s += "<h1>Connected</h1>";
  s += "</html>\r\n\r\n";
  server.send(200, "text/html", s);
  delay(2000);

  WiFi.disconnect();
  setupWifi();
  setupWebSocket();
  setupRFC522();
}

void setupInitialHotspot() {
  WiFi.softAP(initial_ssid, initial_pass);
  Serial.print("Access Point \"");
  Serial.print(initial_ssid);
  Serial.println("\" started");

  Serial.print("IP address:\t");
  Serial.println(WiFi.softAPIP());

  if (!MDNS.begin(host)) {
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }

  server.on("/", handle_OnConnect);
  server.on("/connect-wifi", handle_WifiConnect);

  server.begin();
  MDNS.addService("http", "tcp", 80);
}

void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf_P("\nConnected to ");
  Serial.print(ssid);
  Serial.print(" at ");
  Serial.print(WiFi.localIP());
}

void setupRFC522() {
  SPI.begin();
  mfrc522.PCD_Init();

  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  Serial.printf("\nRFC522 Setup done!");
}

void setupWebSocket() {
  socket.begin();
  socket.onEvent(webSocketEvent);
  Serial.printf("\nStarted websocket server at ip %s", WiFi.localIP().toString().c_str());
}

int writeBlock(int blockNumber, byte arrayAddress[], int size) {
  int largestModulo4Number = blockNumber / 4 * 4;
  int trailerBlock = largestModulo4Number + 3;

  if (blockNumber > 2 && (blockNumber + 1) % 4 == 0) {
    Serial.print(blockNumber);
    Serial.println(" is a trailer block:");
    return 2;
  }
  Serial.print(blockNumber);
  Serial.println(" is a data block:");

  byte status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    return 3;
  }

  status = mfrc522.MIFARE_Write(blockNumber, arrayAddress, TAG_SIZE);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("MIFARE_Write() failed: ");
    Serial.println(mfrc522.GetStatusCodeName((MFRC522::StatusCode)status));
    return 4;
  }
  Serial.println("block was written");

  return 0;
}

int readBlock(int blockNumber, byte arrayAddress[], int num) {
  int largestModulo4Number = blockNumber / 4 * 4;
  int trailerBlock = largestModulo4Number + 3;

  byte status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));

  if (status != MFRC522::STATUS_OK) {
    Serial.print("PCD_Authenticate() failed (read): ");
    Serial.println(mfrc522.GetStatusCodeName((MFRC522::StatusCode)status));
    return 3;
  }

  byte buffersize = TAG_SIZE + 2;
  status = mfrc522.MIFARE_Read(blockNumber, arrayAddress, &buffersize);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("MIFARE_read() failed: ");
    Serial.println(mfrc522.GetStatusCodeName((MFRC522::StatusCode)status));
    return 4;
  }
  Serial.println("block was read");
  card_read = 1;

  Serial.print("read block: ");
  for (int j = 0; j < TAG_SIZE; j++) {
    Serial.write(readbackblock[j]);
  }
  Serial.println("");

  String readTag = String((char *)readbackblock);
  send_json_data(readTag, "1", num);
  return 0;
}

void writeCard(byte content[], int size) {
  Serial.printf("\nFinding card (Timeout 20s)");
  int timeout = 0;
  for (;;) {
    Serial.print(".");
    delay(1000);

    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      Serial.printf("\n Card selected");

      int res = writeBlock(block, content, size);

      if (res == 0) {
        return;
      }
    }

    if (timeout++ == 20) {
      return;
    }
  }
}

void readCard(byte content[], int size, int num) {
  Serial.printf("\nFinding card (Timeout 20s)");
  int timeout = 0;
  byte *bufferATQA, *bufferSize;
  for (;;) {
    Serial.print(".");
    delay(1000);

    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      Serial.printf("\n Card selected");

      int res = readBlock(block, content, num);

      if (res == 0) {
        return;
      } else {
        Serial.print("\nErr");
        return;
      }
    } else {
      Serial.printf("\n Card selected");
      mfrc522.PICC_ReadCardSerial();
      int res = readBlock(block, content, num);

      if (res == 0) {
        return;
      } else if (res == 3) {
        continue;
      } else {
        Serial.print("\nErr");
        return;
      }

      if (timeout++ == 20) {
        return;
      }
    }
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t lenght) {
  char *operation, *data;
  IPAddress ip = socket.remoteIP(num);
  byte buffBlock[16];

  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("\n[%u] Disconnected.\n", num);
      break;
    case WStype_CONNECTED:
      Serial.printf("\n[%d] Connected from %d.%d.%d.%d url: %s", num, ip[0], ip[1], ip[2], ip[3], payload);
      break;
    case WStype_TEXT:
      if (std::strcmp((const char *)payload, "write") >= 0) {
        Serial.printf("\nWriting...");
        send_json_data(String("Waiting for NFC Card to write (Timeout 20s)..."), "0", num);
        long int rand = random(1, INT_MAX);
        String hash = sha1((uint8_t *)rand, TAG_SIZE);
        hash.getBytes(buffBlock, TAG_SIZE);
        writeCard(buffBlock, TAG_SIZE);
        send_json_data(String("Written..."), "0", num);
        delay(2000);
        ESP.restart();
      } else if (std::strcmp((const char *)payload, "read") >= 0) {
        Serial.printf("\nReading...");
        send_json_data(String("Waiting for NFC Card to read (Timeout 20s)..."), "0", num);
        readCard(readbackblock, TAG_SIZE, num);
        send_json_data(String("Read..."), "0", num);
        delay(2000);
      } else if (std::strcmp((const char *)payload, "get-tagid") >= 0 && card_read) {
        socket.sendTXT(num, readbackblock, TAG_SIZE);
      } else if (std::strcmp((const char *)payload, "__ping__") >= 0) {
        send_json_data(String("__pong__"), "0", num);
      } else {
        Serial.printf("\nInvalid payload %s", payload);
      }
      break;
  }
}

void send_json_data(String msg, String type, int num) {
  String res = "{ \"message\":";
  res += "\"" + msg + "\"";
  res += ", \"type\":" + type + " }";
  socket.sendTXT(num, res.c_str(), strlen(res.c_str()));
}

void loop() {
  socket.loop();
  server.handleClient();
  MDNS.update();
}

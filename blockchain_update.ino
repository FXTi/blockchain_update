#include <WiFi.h>
#include <Web3.h>
#include <Util.h>
#include <Contract.h>
#include <Update.h>
#include "time.h"

const char *ssid = "Yy";
const char *password = "1234567890";
#define INFURA_HOST "ropsten.infura.io"
#define INFURA_PATH "/v3/9058d2ffc70840baa8134aef9bdd01df"
#define MY_ADDRESS "0x39D3Eb6dE4E5E46029f6FBE8237C2565DCfCc89A"
String host = "192.168.43.63";
int port = 8000;
String path = "/Blink.bin.ino.lolin32.bin";
const char *target_contract = "0x5b29aedf984d5b0a3475e81af9eb78ef6d582809";
const char* ntpServer = "cn.ntp.org.cn";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 0;
uint256_t tid = 0;
string md5;
string device_type = "esp32";

int wificounter = 0;
Web3 web3(INFURA_HOST, INFURA_PATH);

void printLocalTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void setup_wifi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  if (WiFi.status() != WL_CONNECTED)
  {
    WiFi.persistent(false);
    WiFi.mode(WIFI_OFF);
    WiFi.mode(WIFI_STA);

    WiFi.begin(ssid, password);
  }

  wificounter = 0;
  while (WiFi.status() != WL_CONNECTED && wificounter < 10)
  {
    for (int i = 0; i < 500; i++)
    {
      delay(1);
    }
    Serial.print(".");
    wificounter++;
  }

  if (wificounter >= 10)
  {
    Serial.println("Restarting ...");
    ESP.restart(); //targetting 8266 & Esp32 - you may need to replace this
  }

  delay(10);

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void sync_time() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
}

double queryAccountBalance(const char *address)
{
  string addr(address);
  uint256_t balance = web3.EthGetBalance(&addr); //obtain balance in Wei
  string balanceStr = Util::ConvertWeiToEthString(&balance, 18); //Eth uses 18 decimal
  Serial.print("Balance: ");
  Serial.println(balanceStr.c_str());
  double balanceDbl = atof(balanceStr.c_str());
  return balanceDbl;
}

String getHeaderValue(String header, String headerName) {
  return header.substring(strlen(headerName.c_str()));
}

void submit2chain(String &&md5) {
  Serial.print("Submit md5 checksum to chain: ");
  Serial.println(md5);
  Contract contract(&web3, target_contract);
  contract.SetPrivateKey(MY_ADDRESS);
  string my_address(MY_ADDRESS), target(target_contract);
  uint32_t nonceVal = (uint32_t)web3.EthGetTransactionCount(&my_address);

  unsigned long long gasPriceVal = 43000000000ULL; // 43GWei
  uint32_t  gasLimitVal = 4300000;
  uint256_t valueStr(string("0x00"));
  Serial.println(tid.str().c_str());
  string param = contract.SetupContractData("verify(uint256,string)", &tid, &md5);
  string res = contract.SendTransaction(nonceVal, gasPriceVal, gasLimitVal, &target, &valueStr, &param);
  Serial.println(res.c_str());
}

void start_update() {
  WiFiClient client;
  int contentLength = 0;
  bool isValidContentType = false;

  if (client.connect(host.c_str(), port)) {
    Serial.println("Fetching Bin: " + String(path));

    // Get the contents of the bin file
    client.print(String("GET ") + path + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Cache-Control: no-cache\r\n" +
                 "Connection: close\r\n\r\n");

    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println("Client Timeout !");
        client.stop();
        return;
      }
    }

    while (client.available()) {
      // read line till /n
      String line = client.readStringUntil('\n');
      // remove space, to check if the line is end of headers
      line.trim();

      // if the the line is empty,
      // this is end of headers
      // break the while and feed the
      // remaining `client` to the
      // Update.writeStream();
      if (!line.length()) {
        //headers ended
        break; // and get the OTA started
      }

      // Check if the HTTP Response is 200
      // else break and Exit Update
      Serial.print("Get response: ");
      Serial.println(line);
      if (line.startsWith("HTTP/1.1")) {
        if (line.indexOf("200") < 0) {
          Serial.println("Got a non 200 status code from server. Exiting OTA Update.");
          break;
        }
      }
      // extract headers here
      // Start with content length
      if (line.startsWith("Content-Length: ")) {
        contentLength = atoi((getHeaderValue(line, "Content-Length: ")).c_str());
        Serial.println("Got " + String(contentLength) + " bytes from server");
      }
      // Next, the content type
      if (line.startsWith("Content-type: ")) {
        String contentType = getHeaderValue(line, "Content-type: ");
        Serial.println("Got " + contentType + " payload.");
        if (contentType == "application/octet-stream") {
          isValidContentType = true;
        }
      }
    }
  } else {
    // Connect to S3 failed
    // May be try?
    // Probably a choppy network?
    Serial.println("Connection to " + String(host) + " failed. Please check your setup");
    // retry??
    // execOTA();
    return;
  }
  // Check what is the contentLength and if content type is `application/octet-stream`
  Serial.println("contentLength : " + String(contentLength) + ", isValidContentType : " + String(isValidContentType));
  // check contentLength and content type
  if (contentLength && isValidContentType) {
    // Check if there is enough to OTA Update
    bool canBegin = Update.begin(contentLength);
    // If yes, begin
    if (canBegin) {
      Serial.print("Set update checksum: ");
      Serial.println(md5.c_str());
      if (Update.setMD5(md5.c_str())) {
        Serial.println("Expected md5 sum set!");
      }
      Serial.println("Begin OTA. This may take 2 - 5 mins to complete. Things might be quite for a while.. Patience!");
      // No activity would appear on the Serial monitor
      // So be patient. This may take 2 - 5mins to complete
      size_t written = Update.writeStream(client);
      if (written == contentLength) {
        Serial.println("Written : " + String(written) + " successfully");
      } else {
        Serial.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?" );
        // retry??
        // execOTA();
        return;
      }

      if (Update.end()) {
        //submit2chain(Update.md5String());
        Serial.println("OTA done!");
        //Serial.println(Update.md5String());
        if (Update.isFinished()) {
          Serial.println("Update successfully completed. Rebooting.");
          ESP.restart();
        } else {
          Serial.println("Update not finished? Something went wrong!");
        }
      } else {
        //submit2chain(Update.md5String());
        Serial.println("Error Occurred. Error #: " + String(Update.getError()));
      }
    } else {
      // not enough space to begin OTA
      // Understand the partitions and
      // space availability
      Serial.println("Not enough space to begin OTA");
      client.flush();
    }
  } else {
    Serial.println("There was no content in the response");
    client.flush();
  }

}

long stol(string str)
{
  long result;
  istringstream is(str);
  is >> result;
  return result;
}

bool check_timestamp(const char *addr) {
  Contract contract(&web3, addr);
  String tmp = addr;
  string param = contract.SetupContractData("get_timestamp()", &tmp);
  string result = contract.ViewCall(&param);
  uint256_t timestamp =  web3.getUint256(&result);
  time_t contract_t = stol(timestamp.str());
  Serial.print("Contract epoch timestamp: ");
  Serial.println(contract_t);
  time_t now_t;
  time(&now_t);
  Serial.print("UTC epoch timestamp: ");
  Serial.println(now_t);
  if (contract_t <= now_t && now_t - contract_t < 2 * 60 * 60) {
    Serial.println("New firmware update detect!");
    return true;
  } else {
    return false;
  }
  if (contract_t > now_t) {
    Serial.println("Contract's timestamp is ahead of current timestamp. Attack may happened on chain!");
  }
}

const char * getString(string *in) {
  return Util::InterpretStringResult(web3.getString(in).c_str()).c_str();
}

bool pull_config(const char *addr) {
  Contract contract(&web3, addr);
  String tmp = addr;
  string param, result;

  param = contract.SetupContractData("get_type()", &tmp);
  result = contract.ViewCall(&param);
  result = Util::InterpretStringResult(web3.getString(&result).c_str());
  Serial.print("From contract, type: ");
  Serial.println(result.c_str());
  if (result != device_type) {
    return false;
  }

  param = contract.SetupContractData("get_host()", &tmp);
  result = contract.ViewCall(&param);
  host = getString(&result);
  Serial.print("From contract, host: ");
  Serial.println(host);

  param = contract.SetupContractData("get_port()", &tmp);
  result = contract.ViewCall(&param);
  port = web3.getLongLong(&result);
  Serial.print("From contract, port: ");
  Serial.println(port);

  param = contract.SetupContractData("get_path()", &tmp);
  result = contract.ViewCall(&param);
  path = getString(&result);
  Serial.print("From contract, path: ");
  Serial.println(path);

  param = contract.SetupContractData("get_tid()", &tmp);
  result = contract.ViewCall(&param);
  tid = web3.getUint256(&result);
  Serial.print("From contract, tid: ");
  Serial.println(tid.str().c_str());

  param = contract.SetupContractData("get_md5(uint256)", &tid);
  result = contract.ViewCall(&param);
  md5 = getString(&result);
  Serial.print("From contract, md5: ");
  Serial.println(md5.c_str());

  return true;
}

void update() {
  if (
    (check_timestamp(target_contract)  ||
     check_timestamp(target_contract)  ||
     check_timestamp(target_contract)  ||
     check_timestamp(target_contract)  ||
     check_timestamp(target_contract)) &&
    pull_config(target_contract)) {
    start_update();
  } else {
    Serial.println("Have checked 5 times. No fimeware updates detected!");
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);

  setup_wifi();

  sync_time();

  double balance = queryAccountBalance(MY_ADDRESS);
  Serial.print("My Address: ");
  Serial.println(MY_ADDRESS);
  Serial.print("Balance of Address: ");
  Serial.println(balance);
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(1000);                       // wait for a second
  //digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  delay(1000);

  update();

  delay(1000*60);
}

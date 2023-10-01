/*
ESPboy pinger
www.espboy.com

Enter IP or web address to ping
or 1 symbol command
A - Add addr
L - List stored addrs
D - Del addr
S - Start pinging
T - Time between pings
G - Ping default gateway
H - Help
*/


#include <Pinger.h>
#include "lib/ESPboyInit.h"
#include "lib/ESPboyInit.cpp"
#include "lib/ESPboyTerminalGUI.h"
#include "lib/ESPboyTerminalGUI.cpp"
//#include "lib/ESPboyOTA2.h"
//#include "lib/ESPboyOTA2.cpp"


extern "C" {
  #include <lwip/icmp.h>
} // needed for icmp packet definitions

#define WIFI_TIMEOUT_CONNECTION 10000
#define DELAY_BETWEEN_PINGS 5000

uint32_t delayBetweenPings = DELAY_BETWEEN_PINGS;

struct wificlient2{
  String ssid;
  String pass;
};


struct wf2 {
    String ssid;
    uint8_t rssi;
    uint8_t encription;
};


struct lessRssi2{
    inline bool operator() (const wf2& str1, const wf2& str2) {return (str1.rssi > str2.rssi);}
};


ESPboyInit myESPboy;
ESPboyTerminalGUI terminalGUIobj2(&myESPboy.tft, &myESPboy.mcp);
//ESPboyTerminalGUI *terminalGUIobj = NULL;
//ESPboyOTA2 *OTA2obj = NULL;


Pinger pinger;
wificlient2 *wificl;
std::vector<wf2> wf2List; 

static String pingingStr = "www.ESPboy.com";
static uint8_t pingingIP[4] = {0,0,0,0};


String getWiFiStatusName() {
  String stat;
  switch (WiFi.status()) {
    case WL_IDLE_STATUS:
      stat = (F("Idle"));
      break;
    case WL_NO_SSID_AVAIL:
      stat = (F("No SSID available"));
      break;
    case WL_SCAN_COMPLETED:
      stat = (F("Scan completed"));
      break;
    case WL_CONNECTED:
      stat = (F("WiFi connected"));
      break;
    case WL_CONNECT_FAILED:
      stat = (F("Wrong passphrase"));
      break;
    case WL_CONNECTION_LOST:
      stat = (F("Connection lost"));
      break;
    case WL_DISCONNECTED:
      stat = (F("Wrong password"));
      break;
    default:
      stat = (F("Unknown"));
      break;
  };
  return stat;
}



uint16_t scanWiFi() {
  terminalGUIobj2.printConsole(F("Scaning WiFi..."), TFT_MAGENTA, 1, 0);
  int16_t WifiQuantity = WiFi.scanNetworks();
  if (WifiQuantity != -1 && WifiQuantity != -2 && WifiQuantity != 0) {
    for (uint8_t i = 0; i < WifiQuantity; i++) wf2List.push_back(wf2());
    if (!WifiQuantity) {
      terminalGUIobj2.printConsole(F("WiFi not found"), TFT_RED, 1, 0);
      delay(3000);
      ESP.reset();
    } else
      for (uint8_t i = 0; i < wf2List.size(); i++) {
        wf2List[i].ssid = WiFi.SSID(i);
        wf2List[i].rssi = WiFi.RSSI(i);
        wf2List[i].encription = WiFi.encryptionType(i);
        delay(0);
      }
    sort(wf2List.begin(), wf2List.end(), lessRssi2());
    return (WifiQuantity);
  } else
    return (0);
}



bool wifiConnect() {
 uint16_t wifiNo = 0;
 uint32_t timeOutTimer;
 static uint8_t connectionErrorFlag = 0;

  wificl = new wificlient2();
  
  if (!connectionErrorFlag && !(terminalGUIobj2.getKeys()&PAD_ESC)) {
    wificl->ssid = WiFi.SSID();
    wificl->pass = WiFi.psk();
    terminalGUIobj2.printConsole(F("Last network:"), TFT_MAGENTA, 0, 0);
    terminalGUIobj2.printConsole(wificl->ssid, TFT_GREEN, 0, 0);
  } 
  else 
  {
      wificl->ssid = "";
      wificl->pass = "";
    
    if (scanWiFi())
      for (uint8_t i = wf2List.size(); i > 0; i--) {
        String toPrint =
            (String)(i) + " " + wf2List[i - 1].ssid + " " + wf2List[i - 1].rssi +
            "" + ((wf2List[i - 1].encription == ENC_TYPE_NONE) ? "" : "*");
        terminalGUIobj2.printConsole(toPrint, TFT_YELLOW, 0, 0);
    }

    while (!wifiNo) {
      terminalGUIobj2.printConsole(F("Choose WiFi No:"), TFT_MAGENTA, 0, 0);
      wifiNo = terminalGUIobj2.getUserInput().toInt();
      if (wifiNo < 1 || wifiNo > wf2List.size()) wifiNo = 0;
    }

    wificl->ssid = wf2List[wifiNo - 1].ssid;
    terminalGUIobj2.printConsole(wificl->ssid, TFT_GREEN, 1, 0);

    while (!wificl->pass.length()) {
      terminalGUIobj2.printConsole(F("Password:"), TFT_MAGENTA, 0, 0);
      wificl->pass = terminalGUIobj2.getUserInput();
    }
    terminalGUIobj2.printConsole(/*pass*/F("******"), TFT_GREEN, 0, 0);
  }

  wf2List.clear();

  WiFi.mode(WIFI_STA);
  WiFi.begin(wificl->ssid, wificl->pass);

  terminalGUIobj2.printConsole(F("Connection..."), TFT_MAGENTA, 0, 0);
  timeOutTimer = millis();
  String dots = "";
  while (WiFi.status() != WL_CONNECTED &&
         (millis() - timeOutTimer < WIFI_TIMEOUT_CONNECTION)) {
    delay(700);
    terminalGUIobj2.printConsole(dots, TFT_MAGENTA, 0, 1);
    dots += ".";
  }

  if (WiFi.status() != WL_CONNECTED) {
    connectionErrorFlag = 1;
    terminalGUIobj2.printConsole(getWiFiStatusName(), TFT_RED, 0, 1);
    delay(1000);
    terminalGUIobj2.printConsole("", TFT_BLACK, 0, 0);
    delete (wificl);
    return (false);
  } else {
    terminalGUIobj2.printConsole(getWiFiStatusName(), TFT_MAGENTA, 0, 1);
    delete (wificl);
    return (true);
  }
}


void setCallBacks(){
  
  pinger.OnReceive([](const PingerResponse& response){
    String toPrint;
    if (response.ReceivedResponse){

      toPrint = "by:";
      toPrint += (String) (response.EchoMessageSize - sizeof(struct icmp_echo_hdr));
      toPrint += " tm:";
      toPrint += response.ResponseTime;
      toPrint += " tt:";
      toPrint += (String)response.TimeToLive;

      terminalGUIobj2.printConsole(toPrint, TFT_YELLOW, 1, 0);
    }
    else{
      terminalGUIobj2.printConsole("Time out", TFT_RED, 1, 0);
    }
    return true;
  });


  pinger.OnEnd([](const PingerResponse& response){

    float loss = 100;
    String toPrint;
    
    if(response.TotalReceivedResponses > 0){
      loss = (response.TotalSentRequests - response.TotalReceivedResponses) * 100 / response.TotalSentRequests;
    }
    
    toPrint = "Pack snt:";
    toPrint += (String)response.TotalSentRequests;
    toPrint += " rsv:";
    toPrint += (String)response.TotalReceivedResponses;
    
    terminalGUIobj2.printConsole(toPrint, TFT_GREEN, 1, 0);

    toPrint = "Time min:";
    toPrint += (String)response.MinResponseTime;
    toPrint += " max:";
    toPrint += (String)response.MaxResponseTime;
    terminalGUIobj2.printConsole(toPrint, TFT_GREEN, 1, 0);
    
    toPrint = response.DestIPAddress.toString();
    
    terminalGUIobj2.printConsole(toPrint, TFT_WHITE, 1, 0);
    
    if(response.DestHostname != "" && pingingStr.length()){
      toPrint = response.DestHostname;
      terminalGUIobj2.printConsole(toPrint, TFT_WHITE, 1, 0);
    }

    terminalGUIobj2.printConsole("", TFT_BLACK, 1, 0);
    return true;
  });
}



void enterPingingString(){
  String getInputStr, getTokStr;
  String getInputToks[4];
  uint8_t getInputIP[4]={0,0,0,0}; 
  uint16_t cnt;

  getInputStr = terminalGUIobj2.getUserInput();
  getTokStr = getInputStr; 

  getInputToks[0] = strtok((char *)getTokStr.c_str(), ".");
  getInputToks[1] = strtok(NULL, ".");
  getInputToks[2] = strtok(NULL, ".");
  getInputToks[3] = strtok(NULL, ".");
  
  getInputIP[0] = atoi(getInputToks[0].c_str());
  getInputIP[1] = atoi(getInputToks[1].c_str());
  getInputIP[2] = atoi(getInputToks[2].c_str());
  getInputIP[3] = atoi(getInputToks[3].c_str());
  
  cnt=getInputIP[0]+getInputIP[1]+getInputIP[2]+getInputIP[3];
  
  if(cnt){
    pingingStr="";
    memcpy(pingingIP, getInputIP, sizeof(pingingIP));
  }
  else {
    pingingStr=getInputStr;
    memset(pingingIP,0,sizeof(pingingIP));
  }
}



void setup(){  
  myESPboy.begin("Pinger");
/*
  //Check OTA2
  if (myESPboy.getKeys()&PAD_ACT || myESPboy.getKeys()&PAD_ESC) { 
     terminalGUIobj = new ESPboyTerminalGUI(&myESPboy.tft, &myESPboy.mcp);
     OTA2obj = new ESPboyOTA2(terminalGUIobj);
  }
*/  
  WiFi.mode(WIFI_STA);
  terminalGUIobj2.toggleDisplayMode(1);
  while (!wifiConnect());
  terminalGUIobj2.printConsole("", TFT_BLACK, 1, 0);
  
  Serial.begin(115200);

  setCallBacks();

  terminalGUIobj2.printConsole("Current gateway", TFT_MAGENTA, 1, 0);
  terminalGUIobj2.printConsole(WiFi.gatewayIP().toString(), TFT_MAGENTA, 1, 0);
  if(pinger.Ping(WiFi.gatewayIP()) == false)     
    terminalGUIobj2.printConsole("Ping error", TFT_RED, 1, 0);
  
  delay(delayBetweenPings);
}
  

void loop(){ 
 static uint32_t smartDelay;
 
  if (pingingStr.length()) terminalGUIobj2.printConsole(pingingStr, TFT_MAGENTA, 1, 0);
  else {
    String toPrint;
    toPrint = (String)pingingIP[0]; 
    toPrint += ".";
    toPrint += (String)pingingIP[1];
    toPrint += ".";
    toPrint += (String)pingingIP[2];
    toPrint += ".";
    toPrint += (String)pingingIP[3];
    terminalGUIobj2.printConsole(toPrint, TFT_MAGENTA, 1, 0);
  }
  
  if (pingingStr.length()){
    if(pinger.Ping(pingingStr) == false)
      terminalGUIobj2.printConsole("Ping error", TFT_RED, 1, 0);
    }
  else
    if(pinger.Ping(IPAddress(pingingIP[0],pingingIP[1],pingingIP[2],pingingIP[3])) == false)
      terminalGUIobj2.printConsole("Ping error", TFT_MAGENTA, 1, 0);
  
  while (millis()-smartDelay < delayBetweenPings){
    delay(100);
    if (myESPboy.getKeys()){
      enterPingingString();
      break;
    }
  }
    
  smartDelay = millis();
}

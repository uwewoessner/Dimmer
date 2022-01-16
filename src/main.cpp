

#if defined Wohnzimmer
#define IOMCP
#define Zimmer "Wohnzimmer"
#endif

#include <ArduinoOTA.h>
#ifdef ESP32
#include <esp_int_wdt.h>
#include <esp_task_wdt.h>

#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <Wire.h>
#include <PubSubClient.h>
#include "RBDdimmer.h"
#include "debouncebutton.h"


#include <ESP32Encoder.h>

ESP32Encoder encoder1;
ESP32Encoder encoder2;
#define DEBUG

#ifdef DEBUG

void DebugPrintf(const char *, ...); //Our printf function
#else
#define DebugPrintf(a);
#endif
char *convert(int, int);    //Convert integer number into octal, hex, etc.

const char *mqtt_server = "192.168.178.34";
#include "../../../wifiPasswd.h"

WiFiServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);



unsigned long pUpdate;


int pButton1 = 15;
int pButton2 = 14;
int pDimmer1 = 26;
int pDimmer2 = 27;
//int pDimmerZero = P_B0;
int pA2 = 16;
int pB2 = 17;
int pA1 = 23;
int pB1 = 21;

int pIntZero = 25;
int networkTimeout = 0;
bool PingArrived = false;
byte power1=50;
byte power2=50;
//default 

dimmerLamp dimmer1(pDimmer1,pIntZero);
dimmerLamp dimmer2(pDimmer2,pIntZero);
debounceButton button1(pButton1);
debounceButton button2(pButton2);

byte oldPower1=200;
byte oldPower2=200;
bool lightState1=false;
bool lightState2=false;





void setPower(int l)
{
  if(l==1)
  {
    oldPower1 = power1;
    byte rp = 22+(byte)(power1/100.0 * (float)(96-22));
    dimmer1.setPower(rp);
      DebugPrintf("power1 %d %d\n",power1,rp);
  }
  else if(l==2)
  {
    oldPower2 = power2;
    byte rp = 20+(byte)(power2/100.0 * (float)(96-20));
    dimmer2.setPower(rp);
      DebugPrintf("power2 %d %d\n",power2,rp);
  }
}

#ifdef DEBUG
char linebuf[200];
unsigned int linePos = 0;
unsigned int printLine = 0;
void outputChar(char c)
{

  if (linePos < 199)
  {
    linebuf[linePos] = c;
    linePos++;
    linebuf[linePos] = '\0';
  }
  if (c == '\n')
  {
    linePos = 0;
    if (client.connected())
    {
      char top[50];
      sprintf(top, "Debug/Dimmer/%d", printLine);
      client.publish(top, linebuf);
      printLine++;
      if (printLine > 20)
        printLine = 0;
    }
    else
    {
      Serial.print(linebuf);
    }
    linebuf[0] = '\0';
  }
}
void outputCharp(const char *s)
{
  const char *c;
  for (c = s; *c != '\0'; c++)
  {
    outputChar(*c);
  }
}
void DebugPrintf(const char *format, ...)
{
  const char *traverse;
  int i;
  const char *s;
  char iBuf[20];

  va_list arg;
  va_start(arg, format);

  for (traverse = format; *traverse != '\0'; traverse++)
  {
    while (*traverse != '%' && *traverse != '\0')
    {
      outputChar(*traverse);
      traverse++;
    }
    if (*traverse == '\0')
    {
      break;
    }

    traverse++;

    //Module 2: Fetching and executing arguments
    switch (*traverse)
    {
    case 'c':
      i = va_arg(arg, int); //Fetch char argument
      outputChar(i);
      break;

    case 'd':
      i = va_arg(arg, int); //Fetch Decimal/Integer argument
      if (i < 0)
      {
        i = -i;
        outputChar('-');
      }
      outputCharp(itoa(i,iBuf, 10));
      break;

    case 'o':
      i = va_arg(arg, unsigned int); //Fetch Octal representation
      outputCharp(itoa(i,iBuf, 8));
      break;

    case 's':
      s = va_arg(arg, char *); //Fetch string
      outputCharp(s);
      break;

    case 'x':
      i = va_arg(arg, unsigned int); //Fetch Hexadecimal representation
      outputCharp(itoa(i,iBuf, 16));
      break;
    }
  }

  //Module 3: Closing argument list to necessary clean-up
  va_end(arg);
}

char *convert(int num, int base)
{
  static char Representation[] = "0123456789ABCDEF";
  static char buffer[50];
  char *ptr;

  ptr = &buffer[49];
  *ptr = '\0';

  do
  {
    *--ptr = Representation[num % base];
    num /= base;
  } while (num != 0);

  return (ptr);
}
#endif

unsigned long runtime = 29000; // 29 Sekunden Laufzeit von auf bis zu oder umgekehrt

void localLoop();

void sendState();

const char *commandString = "dimmer/Dimmer/command";

void callback(char *topicP, byte *payloadP, unsigned int length)
{
  char topic[200];
  char payload[200];
  strncpy(topic, topicP, 200);
  strncpy(payload, (char *)payloadP, length);
  payload[length] = '\0';
  unsigned long now = millis();

  DebugPrintf("Message arrived [%s] %s\n", topic, payload);
  if (strcmp(topic, commandString) == 0)
  {
    if ((char)payload[0] == '0')
    {
    }
  }
  else if (strcmp(topic, "wohnzimmer/wohnen/command") == 0)
  {
    if((pUpdate+2000)< now)
    {
      // Switch on the LED if an N was received as second character
      if ((char)payload[1] == 'N') // "ON"
      {
        lightState2 = true;
      }
      else if (strncmp(payload, "res", 3) == 0)
      {
        ESP.restart();
      }
      else
      {
        lightState2 = false;
      }
      dimmer2.setState((ON_OFF_typedef)lightState2);
    }
  }
  else if (strcmp(topic, "wohnzimmer/essen/reset") == 0)
  {
    DebugPrintf("reset\n");
    ESP.restart();
  }
  else if (strcmp(topic, "wohnzimmer/essen/command") == 0)
  {
    if((pUpdate+2000)< now)
    {
      // Switch on the LED if an N was received as second character
      if ((char)payload[1] == 'N') // "ON"
      {
        lightState1 = true;
      }
      else
      {
        lightState1 = false;
      }
      dimmer1.setState((ON_OFF_typedef)lightState1);
    }
  }
  else if (strcmp(topic, "wohnzimmer/essen/brightness") == 0)
  {
    if((pUpdate+2000)< now)
    {
      int p = 20;
      float fp = 0;
      //sscanf(payload, "0,0,%d", &p);
      sscanf(payload, "%f", &fp);
      p = fp;
      bool ls = lightState1;
      if (p > 0)
        lightState1 = true;
      else
        lightState1 = false;
      if (ls != lightState1)
      {
        dimmer1.setState((ON_OFF_typedef)lightState1);
        client.publish("wohnzimmer/essen/status", "ON");
      }
      power1 = p;
      setPower(1);
      DebugPrintf("Message arrived [%d]\n", power1);
    }
  }
  else if (strcmp(topic, "wohnzimmer/wohnen/brightness") == 0)
  {
    
    if((pUpdate+2000)< now)
    {
      int p = 20;
      float fp=0;
      //sscanf(payload, "0,0,%d", &p);
      sscanf(payload, "%f", &fp);
      p = fp;
      bool ls = lightState2;
      if (p > 0)
        lightState2 = true;
      else
        lightState2 = false;
      if (ls != lightState2)
      {
        dimmer2.setState((ON_OFF_typedef)lightState2);
        client.publish("wohnzimmer/wohnen/status", "ON");
      }
      power2 = p;
      setPower(2);
      DebugPrintf("Message arrived [%d]\n", power2);
    }
  }
  else if(strcmp(topic,"IOT/Ping") == 0)
  {
    networkTimeout = 0;
    PingArrived = true;
  }
  sendState();
}

void sendState()
{
  char buf[50];
  sprintf(buf,"%d",power1);
  client.publish("wohnzimmer/essen/bstatus",buf);
  sprintf(buf,"%d",power2);
  client.publish("wohnzimmer/wohnen/bstatus",buf);
  if (lightState1)
  {
    client.publish("wohnzimmer/essen/status", "ON");
  }
  else
  {
    client.publish("wohnzimmer/essen/status", "OFF");
  }
  if (lightState2)
  {
    client.publish("wohnzimmer/wohnen/status", "ON");
  }
  else
  {
    client.publish("wohnzimmer/wohnen/status", "OFF");
  }
}

void reconnect()
{
  #ifdef NO_MQTT
  return;
  #endif

  DebugPrintf("Attempting MQTT connection...\n");
  // Attempt to connect
  if (client.connect("dimmer1"))
  {
    DebugPrintf("MQTTconnected\n");
    // Once connected, publish an announcement...
    sendState();
    // ... and resubscribe
    client.subscribe(commandString);
    client.subscribe("wohnzimmer/wohnen/command");
    client.subscribe("wohnzimmer/essen/command");
    client.subscribe("wohnzimmer/wohnen/brightness");
    client.subscribe("wohnzimmer/essen/brightness");
    client.subscribe("IOT/Ping");
    
  }
  else
  {
    DebugPrintf("failed, rc=");
    DebugPrintf("%d", client.state());
    DebugPrintf(" try again in 5 seconds\n");
  }
}
void setup()
{
  Serial.begin(115200);
  DebugPrintf("setup\n");
  
  Wire.setClock(400000L); // 400Khz i2c

  
  pinMode(pButton1, INPUT_PULLUP);
  pinMode(pButton2, INPUT_PULLUP);
  
  pinMode(pIntZero, INPUT_PULLUP);
  pinMode(pDimmer1, OUTPUT);
  pinMode(pDimmer2, OUTPUT);


  
  
	ESP32Encoder::useInternalWeakPullResistors=UP;

	encoder1.attachHalfQuad(pA1, pB1);
	encoder2.attachHalfQuad(pA2, pB2);
  
  pinMode(pA1, INPUT_PULLUP);
  pinMode(pB1, INPUT_PULLUP);

  pinMode(pA2, INPUT_PULLUP);
  pinMode(pB2, INPUT_PULLUP);
		
	encoder1.setCount(50);
	encoder2.setCount(50);

  DebugPrintf("vorDimmer1\n");
  dimmer1.begin(NORMAL_MODE,OFF);
  dimmer2.begin(NORMAL_MODE,OFF);
  
  DebugPrintf("Dimmer connecting to wifi\n");

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }

  ArduinoOTA.setPort(8266);
#ifdef Wohnzimmer
  ArduinoOTA.setHostname("dimmerWohnzimmer");
#endif
  ArduinoOTA.onStart([]() {
    DebugPrintf("Start\n");
  });
  ArduinoOTA.onEnd([]() {
    DebugPrintf("\nEnd\n");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    DebugPrintf("Progress: %u %% \r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    DebugPrintf("Error[ %u]: ", error);
    if (error == OTA_AUTH_ERROR)
      DebugPrintf("Auth Failed\n");
    else if (error == OTA_BEGIN_ERROR)
      DebugPrintf("Begin Failed\n");
    else if (error == OTA_CONNECT_ERROR)
      DebugPrintf("Connect Failed\n");
    else if (error == OTA_RECEIVE_ERROR)
      DebugPrintf("Receive Failed\n");
    else if (error == OTA_END_ERROR)
      DebugPrintf("End Failed\n");
  });
  ArduinoOTA.begin();

  DebugPrintf("%s\n", WiFi.localIP().toString().c_str());

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
#ifdef ESP32
  esp_task_wdt_init(25, true); //socket timeout is 15seconds
  esp_task_wdt_add(nullptr);
#endif

  button1.init(false);
  button2.init(false);
  server.begin();
}

void reconnectWifi()
{
  bool ledState = false;
  while (WiFi.status() != WL_CONNECTED)
  {
    long start = millis();
    while (millis() - start < 500)
    {
      localLoop();
    }
    ledState = !ledState;
  }
}
long lastReconnectAttempt = 0;
long lastReconnectWifiAttempt = 0;


void loop()
{
  unsigned long now = millis();
  if(now < pUpdate)
      pUpdate = now; // handle overrun

  if (WiFi.status() != WL_CONNECTED)
  {
    if (now - lastReconnectWifiAttempt > 60000) // every 60 seconds
    {
      lastReconnectWifiAttempt = now;
      // Attempt to reconnect
      reconnectWifi();
    }
  }
  if (!client.connected())
  {
    if (now - lastReconnectAttempt > 10000) // every 10 seconds
    {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      reconnect();
    }
  }
  else
  {
    // Client connected

    client.loop();
  }

  localLoop(); // local updates which have to be done during reconnect as well;
}

void localLoop()
{
  debounceButton::update();
  esp_task_wdt_reset(); // reset the watchdog
  ArduinoOTA.handle();
  static int64_t oldCounter1 = 0;
  static int64_t oldCounter2 = 0;
  int64_t currentCounter1 = 0;
  int64_t currentCounter2 = 0;
  currentCounter1 = encoder1.getCount();
  currentCounter2 = encoder2.getCount();
  if(currentCounter1 != oldCounter1)
  {
    power1 += currentCounter1 - oldCounter1;
    if(power1 > 200)
    {
      power1 = 0;
    }
    if(power1 > 100)
    {
      power1 = 100;
    }
    
    sendState();
    
      DebugPrintf("turn1 [%d]\n", power1);
    oldCounter1 = currentCounter1;
  }
  if(currentCounter2 != oldCounter2)
  {
    power2 += currentCounter2 - oldCounter2;
    if(power2 > 200)
    {
      power2 = 0;
    }
    if(power2 > 100)
    {
      power2 = 100;
    }
    sendState();
      DebugPrintf("turn2 [%d]\n", power2);
    oldCounter2 = currentCounter2;
  }
  
  unsigned long now = millis();
  if (power1 > 200)
  {
    power1 = 0;
    encoder1.setCount(power1);
  }
  if (power2 > 200)
  {
    power2 = 0;
    encoder2.setCount(power2);
  }
  if (power1 > 100)
  {
    power1 = 100;
    encoder1.setCount(power1);
  }
  if (power2 > 100)
  {
    power2 = 100;
    encoder2.setCount(power2);
  }

  //pollAll();
  if(oldPower1!=power1)
  {
    pUpdate = now;
    setPower(1);
  }
  if(oldPower2!=power2)
  {
    pUpdate = now;
    setPower(2);
  }
  
  if (button1.wasKlicked())
  {
    pUpdate = now;
    lightState2 = !lightState2;
    DebugPrintf("lightState2 b %d\n", lightState2);
    if (lightState2)
    {
      if (power2 < 10)
      {
        power2 = 10;
        encoder2.setCount(power2);
      }
    }
    dimmer2.setState((ON_OFF_typedef)lightState2);
    setPower(2);
    sendState();
  }
  if (button2.wasKlicked())
  {
    pUpdate = now;
    lightState1 = !lightState1;
    if (lightState1)
    {
      if (power1 < 10)
      {
        power1 = 10;
        encoder1.setCount(power1);
      }
    }
    dimmer1.setState((ON_OFF_typedef)lightState1);
    setPower(1);
    sendState();
  }
  if (button1.wasPressed())
  {
    pUpdate = now;
    lightState1 = lightState2 = !lightState2;
    DebugPrintf("lightState2 b %d\n", lightState2);
    if (lightState2)
    {
      if (power2 < 10)
      {
        power2 = 10;
        encoder2.setCount(power2);
      }
    }
    if (lightState1)
    {
      if (power1 < 10)
      {
        power1 = 10;
        encoder1.setCount(power1);
      }
    }
    dimmer2.setState((ON_OFF_typedef)lightState2);
    dimmer1.setState((ON_OFF_typedef)lightState1);
    setPower(1);
    setPower(2);
    sendState();
  }
  if (button2.wasPressed())
  {
    pUpdate = now;
    lightState2 = lightState1 = !lightState1;
    DebugPrintf("lightState2 b %d\n", lightState2);
    if (lightState2)
    {
      if (power2 < 10)
      {
        power2 = 10;
        encoder2.setCount(power2);
      }
    }
    if (lightState1)
    {
      if (power1 < 10)
      {
        power1 = 10;
        encoder1.setCount(power1);
      }
    }
    dimmer2.setState((ON_OFF_typedef)lightState2);
    dimmer1.setState((ON_OFF_typedef)lightState1);
    setPower(1);
    setPower(2);
    sendState();
  }
  if(button2.wasDoubleKlicked() || button1.wasDoubleKlicked())
  {
    ESP.restart();
  }
  
  static unsigned long networkMinute = 0;
  if((now - networkMinute) > 60000)
  {
    networkMinute = now;
    if(PingArrived) // only activate network timeout if at least one Ping has arrived
    {
      networkTimeout++; // this is reset whenever an mqtt network ping arrives
    }
  }
  if(networkTimeout > 5) // 5 minute timeout
  {
      DebugPrintf("network Timeout %d\n", networkTimeout);
    ESP.restart();
  }
}



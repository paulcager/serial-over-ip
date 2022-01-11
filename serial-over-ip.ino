

#include "secrets.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#define RESET_PIN 2
#define SHUTDOWN_PIN 0
#define MAX_CLIENTS 2

WiFiClient *clients[MAX_CLIENTS] = { NULL };

WiFiServer server(8001);
ESP8266WebServer webServer(80);

void togglePin(int pin) {
    digitalWrite(pin, 0);
    delay(250);
    digitalWrite(pin, 1);   
}

void setup() {
  pinMode(RESET_PIN, OUTPUT);
  pinMode(SHUTDOWN_PIN, OUTPUT);
  pinMode(4, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);

  digitalWrite(RESET_PIN, 1);
  digitalWrite(SHUTDOWN_PIN, 1);
  
  delay(3000);
  Serial.begin(115200);
  
  Serial.println();
  Serial.print("Connecting to access point ");
  Serial.print(MY_SSID);
  Serial.println("...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(MY_SSID, MY_SSID_PSK);

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }

  IPAddress myIP = WiFi.localIP();
  Serial.print("IP address: ");
  Serial.println(myIP);

  server.begin();

  webServer.on("/reset", []() {
    togglePin(RESET_PIN);
    webServer.send(200, "text/plain", "Done\n");
  });
  
  webServer.on("/shutdown", []() {
    togglePin(SHUTDOWN_PIN);
    webServer.send(200, "text/plain", "Done\n");
  });

  webServer.on("/wake-up", []() {
    togglePin(SHUTDOWN_PIN);
    webServer.send(200, "text/plain", "Done\n");
  });
  
  webServer.on("/reset-esp32", []() {
    webServer.send(200, "text/plain", "Done\n");
    delay(500);
    ESP.restart();
  });
  
  webServer.begin();

}

void loop() 
{
  webServer.handleClient();
  
  WiFiClient newClient = server.available();
  if (newClient) {
    bool found = false;
    for (int i=0 ; i<MAX_CLIENTS ; ++i) {
        if (clients[i] == NULL) {
            clients[i] = new WiFiClient(newClient);
            found = true;
            break;
        }
    }

    if (!found) {
      clients[0]->stop();
      delete(clients[0]);
      clients[0] = new WiFiClient(newClient);
    }
  }

  for (int i=0 ; i<MAX_CLIENTS ; ++i) {
    if (clients[i] != NULL) {
      if (!clients[i]->connected()) {
        clients[i]->stop();
        delete(clients[i]);
        clients[i] = NULL;
      } else {
        while(clients[i]->available()>0) {
          Serial.write(clients[i]->read()); 
        }
      }  
    }
  }
  
  while(Serial.available()>0)
  {
    char ch = Serial.read();
    for (int i=0 ; i<MAX_CLIENTS ; ++i) {
      if (clients[i] != NULL) {
        clients[i]->write(ch);
      }
    }
  }

  int r = digitalRead(4);
  Serial.print(r);
  r = digitalRead(5);
  Serial.println(r);
  delay(500);

  
}

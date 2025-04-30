
PsychicHttpServer server;
PsychicEventSource eventSource;

std::shared_ptr<char[]> ws_firstMessage;

const char *ssid = "midi_magic";
const char *password = "password";

char w_msg[100];
std::atomic<bool> sendToWebClient(false);

void sendWSClient( void * pvParameters ){
  for(;;){
    if (sendToWebClient.load()){
      eventSource.send(w_msg);
      sendToWebClient.store(false);
    }
    delay(5);
  }
}

void sendWS(const char* p_msg) {
  strncpy(w_msg, p_msg, sizeof(w_msg) - 1);
  w_msg[sizeof(w_msg) - 1] = '\0';
  
  sendToWebClient.store(true);
}



void ws_setup(){
    WiFi.softAP(ssid, password);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);


    server.listen(80);
    server.serveStatic("/", SD, "/web/");
    server.serveStatic("/tracks", SD, "/tracks/");

  eventSource.onOpen([](PsychicEventSourceClient *client) {
     //Serial.printf("[eventsource] connection #%u connected from %s\n", client->socket(), client->remoteIP().toString());
     snprintf(w_msg, sizeof(w_msg),"{\"song\":\"%s\"}", ui->songName);
     client->send(w_msg);
  });

  eventSource.onClose([](PsychicEventSourceClient *client) {
     Serial.printf("[eventsource] connection #%u closed from %s\n", client->socket(), client->remoteIP().toString());
  });

   server.on("/sse", &eventSource);


   xTaskCreate(sendWSClient, /* Task function. */
      "sendWSClient", /* String with name of task. */
      10000, /* Stack size in bytes. */
      NULL, /* Parameter passed as input of the task */
      1, /* Priority of the task. 0 = lowest */
      NULL); /* Task handle. */

}






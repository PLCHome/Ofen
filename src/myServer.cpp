#include "myServer.h"
#include <ArduinoJson.h>

#define _ASYNC_WEBSERVER_LOGLEVEL_ 4

#include <AsyncTCP.h>
#include <WebServer_WT32_ETH01.h>
#include <WebServer_WT32_ETH01_Debug.h>
#include <ESPAsyncWebServer.h>

FS *filesystem = &SPIFFS;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
File fsUploadFile;

void heartBeatPrint()
{
    static int num = 1;

    Serial.print(F("H")); // H means alive

    if (num == 80)
    {
        Serial.println();
        num = 1;
    }
    else if (num++ % 10 == 0)
    {
        Serial.print(F(" "));
    }
}

void check_status()
{
    static ulong checkstatus_timeout = 0;
    static ulong current_millis;

    current_millis = millis();

    // Print hearbeat every HEARTBEAT_INTERVAL (10) seconds.
    if ((current_millis > checkstatus_timeout) || (checkstatus_timeout == 0))
    {
        heartBeatPrint();
        checkstatus_timeout = current_millis + HEARTBEAT_INTERVAL;
    }
}

// format bytes
String formatBytes(size_t bytes)
{
    if (bytes < 1024)
    {
        return String(bytes) + "B";
    }
    else if (bytes < (1024 * 1024))
    {
        return String(bytes / 1024.0) + "KB";
    }
    else if (bytes < (1024 * 1024 * 1024))
    {
        return String(bytes / 1024.0 / 1024.0) + "MB";
    }
    else
    {
        return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
    }
}

String getContentType(String filename, AsyncWebServerRequest *request)
{
    if (request->hasArg("download"))
    {
        return "application/octet-stream";
    }
    else if (filename.endsWith(".htm"))
    {
        return "text/html";
    }
    else if (filename.endsWith(".html"))
    {
        return "text/html";
    }
    else if (filename.endsWith(".css"))
    {
        return "text/css";
    }
    else if (filename.endsWith(".js"))
    {
        return "application/javascript";
    }
    else if (filename.endsWith(".png"))
    {
        return "image/png";
    }
    else if (filename.endsWith(".gif"))
    {
        return "image/gif";
    }
    else if (filename.endsWith(".jpg"))
    {
        return "image/jpeg";
    }
    else if (filename.endsWith(".ico"))
    {
        return "image/x-icon";
    }
    else if (filename.endsWith(".xml"))
    {
        return "text/xml";
    }
    else if (filename.endsWith(".pdf"))
    {
        return "application/x-pdf";
    }
    else if (filename.endsWith(".zip"))
    {
        return "application/x-zip";
    }
    else if (filename.endsWith(".gz"))
    {
        return "application/x-gzip";
    }

    return "text/plain";
}

void handleFileList(AsyncWebServerRequest *request)
{
    if (!request->hasArg("dir"))
    {
        request->send(500, "text/plain", "BAD ARGS");
        return;
    }

    String path = request->arg("dir");
    Serial.println("handleFileList: " + path);

    File root = SPIFFS.open(path);
    path = String();

    String output = "[";

    if (root.isDirectory())
    {
        File file = root.openNextFile();

        while (file)
        {
            if (output != "[")
            {
                output += ',';
            }

            output += "{\"type\":\"";
            output += (file.isDirectory()) ? "dir" : "file";
            output += "\",\"name\":\"";
            output += String(file.name());
            output += "\",\"path\":\"";
            output += String(file.path());
            output += "\",\"size\":\"";
            output += formatBytes(file.size());
            output += "\"}";
            file = root.openNextFile();
        }
    }
    root.close();
    output += "]";

    Serial.println("handleFileList: " + output);

    request->send(200, "text/json", output);
}

void initFS()
{
    // Initialize LittleFS/SPIFFS file-system
    // Format SPIFFS if not yet
    if (!FileFS.begin(true))
    {
        Serial.println(F("SPIFFS/LittleFS failed! Formatting."));

        if (!FileFS.begin())
        {
            while (true)
            {
                Serial.println(F("SPIFFS failed!. Please use LittleFS."));
                // Stay forever here as useless to go further
                delay(5000);
            }
        }
    }
}

void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  //Handle upload
}

void initWebserver()
{
    // SERVER INIT
    // list directory
    server.on("/list", HTTP_GET, handleFileList);
    server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request)
              { ESP.restart(); });

    // Web Page handlers
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->redirect("/index.html"); });
    server.on("/index", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->redirect("/index.html"); });

    // server.serveStatic("/page2", SPIFFS, "/page2.html");
    // server.serveStatic("/page3", SPIFFS, "/page3.html");
    server.serveStatic("/", SPIFFS, "/www/");
    server.serveStatic("/config/", SPIFFS, "/config/");
    server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200);
    }, onUpload);    
    // Other usage with various web site assets:
    // server.serveStatic("/assets/css/test.css", SPIFFS, "/assets/css/test.css"); // Style sheet
    // server.serveStatic("/assets/js/test_script.js", SPIFFS,"/assets/js/test_script.js"); // Javascript
    // server.serveStatic("/assets/font/fonticons.ttf", SPIFFS,"/assets/font/fonticons.ttf"); // font
    // server.serveStatic("/assets/picture.png", SPIFFS,"/assets/picture.png"); // Picture

    server.begin();
}

void notifyClients(String sensorReadings)
{
    ws.textAll(sensorReadings);
}

void dadaChanged(change dataChange)
{
    switch (dataChange)
    {
    case ch_inputintern:
        notifyClients(jsonInputIntern());
        break;
    case ch_relay:
        notifyClients(jsonRelay());
        break;
    case ch_tempholdingreg:
        notifyClients(jsonTempHoldingReg());
        break;
    }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
    {
        data[len] = 0;
        String message = (char *)data;
        Serial.println(String("got message:") + message);
        //  Check if the message is "getReadings"
        if (strchr((char *)data, '{') == NULL)
        {
            if (strcmp((char *)data, "getReadings") == 0)
            {
                // if it is, send current sensor readings
                dadaChanged(ch_inputintern);
                dadaChanged(ch_relay);
                dadaChanged(ch_tempholdingreg);
            }
        }
        else
        {
            JsonDocument doc;
            deserializeJson(doc, (char *)data);
            JsonObject root = doc.as<JsonObject>();
            for (JsonPair kv : root)
            {
                String key = String(kv.key().c_str());
                if (key.startsWith("relay"))
                {
                    Serial.println(key.substring(5));
                    int pos = key.substring(5).toInt();
                    if (pos >= 0 and pos < RELAYS)
                    {
                        relay[pos] = kv.value().as<bool>();
                    }
                }
            }
        }
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
    case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;
    case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;
    case WS_EVT_DATA:
        handleWebSocketMessage(arg, data, len);
        break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
        break;
    }
}

void initWebSocket()
{
    ws.onEvent(onEvent);
    server.addHandler(&ws);
}

void WEBsetup()
{
    // Using this if Serial debugging is not necessary or not using Serial port
    // while (!Serial && (millis() < 3000));

    Serial.print("\nStarting WebServer on " + String(ARDUINO_BOARD));
    Serial.println(" with " + String(SHIELD_TYPE));
    Serial.println(WEBSERVER_WT32_ETH01_VERSION);

    // To be called before ETH.begin()
    WT32_ETH01_onEvent();

    // bool begin(uint8_t phy_addr=ETH_PHY_ADDR, int power=ETH_PHY_POWER, int mdc=ETH_PHY_MDC, int mdio=ETH_PHY_MDIO,
    //            eth_phy_type_t type=ETH_PHY_TYPE, eth_clock_mode_t clk_mode=ETH_CLK_MODE);
    ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_TYPE, ETH_CLK_MODE);
    // ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER);

    // Static IP, leave without this line to get IP via DHCP
    // bool config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1 = 0, IPAddress dns2 = 0);
    // ETH.config(myIP, myGW, mySN, myDNS);

    // WT32_ETH01_waitForConnect();

    initWebSocket();
    // start the web server on port 80
    initWebserver();
    setDATAchanged(dadaChanged);
}

void WEBloop()
{
    ws.cleanupClients();
}
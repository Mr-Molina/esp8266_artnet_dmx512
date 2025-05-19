#include "webinterface.h"
#include <WiFiManager.h> // Add this include

constexpr uint16_t UNIVERSE_MIN = 1;
constexpr uint16_t UNIVERSE_MAX = 32767;
constexpr uint16_t CHANNELS_MIN = 1;
constexpr uint16_t CHANNELS_MAX = 512;
constexpr uint16_t DELAY_MIN = 1;
constexpr uint16_t DELAY_MAX = 1000;

Config config;
extern ESP8266WebServer server;
extern unsigned long tic_web;
extern float fps;
extern uint32_t packetCounter;

/***************************************************************************/

void setupWebServer(ESP8266WebServer &server)
{
  // This serves all URIs that can be resolved to a file on the LittleFS filesystem
  server.onNotFound(handleNotFound);

  server.on("/", HTTP_GET, [&server]() { // capture server by reference
    tic_web = millis();
    handleRedirect("/index.html");
  });

  server.on("/defaults", HTTP_GET, [&server]()
            {
    tic_web = millis();
    Serial.println("handleDefaults");
    handleStaticFile("/reload_success.html");
    defaultConfig();
    saveConfig();
    server.close();
    server.stop();
    ESP.restart(); });

  server.on("/reconnect", HTTP_GET, [&server]()
            {
    tic_web = millis();
    Serial.println("handleReconnect");
    handleStaticFile("/reload_success.html");
    
    // Store the current WiFi credentials before resetting
    String ssid = WiFi.SSID();
    String pass = WiFi.psk();
    
    server.close();
    server.stop();
    delay(1000);
    
    WiFiManager wifiManager;
    // Only reset settings if explicitly requested with a query parameter
    if (server.hasArg("reset") && server.arg("reset") == "true") {
      wifiManager.resetSettings();
      Serial.println("WiFi settings reset requested");
    } else {
      // Otherwise, just start the portal but keep existing credentials
      Serial.println("Starting config portal with existing credentials");
    }
    
    wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
    
    // If we have existing credentials, try to connect with them first
    if (ssid.length() > 0 && !server.hasArg("reset")) {
      WiFi.begin(ssid, pass);
      
      // Wait up to 10 seconds for connection
      int timeout = 20;
      while (timeout > 0 && WiFi.status() != WL_CONNECTED) {
        delay(500);
        timeout--;
        Serial.print(".");
      }
      Serial.println();
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Reconnected to existing WiFi");
        server.begin();
        return;
      }
    }
    
    // If we couldn't connect with existing credentials, start the portal
    wifiManager.startConfigPortal("ARTNET");
    Serial.println("connected");
    server.begin(); });

  server.on("/restart", HTTP_GET, [&server]()
            {
    tic_web = millis();
    Serial.println("handleRestart");
    handleStaticFile("/reload_success.html");
    server.close();
    server.stop();
    LittleFS.end();
    delay(5000);
    ESP.restart(); });

  server.on("/dir", HTTP_GET, [&server]()
            {
    tic_web = millis();
    handleDirList(); });

  server.on("/json", HTTP_PUT, [&server]()
            {
    tic_web = millis();
    handleJSON(); });

  server.on("/json", HTTP_POST, [&server]()
            {
    tic_web = millis();
    handleJSON(); });

  server.on("/json", HTTP_GET, [&server]()
            {
    tic_web = millis();
    JsonDocument root;
    N_CONFIG_TO_JSON(universe, "universe");
    N_CONFIG_TO_JSON(channels, "channels");
    N_CONFIG_TO_JSON(delay, "delay");
    root["version"] = __DATE__ " / " __TIME__;
    root["uptime"]  = long(millis() / 1000);
    root["packets"] = packetCounter;
    root["fps"]     = fps;
    String str;
    serializeJson(root, str);
    server.setContentLength(str.length());
    server.send(200, "application/json", str); });

  server.on("/update", HTTP_GET, [&server]()
            {
    tic_web = millis();
    handleStaticFile("/update.html"); });

  server.on("/update", HTTP_POST, handleUpdate1, handleUpdate2);
}

static String getContentType(const String &path)
{
  if (path.endsWith(".html"))
    return "text/html";
  else if (path.endsWith(".htm"))
    return "text/html";
  else if (path.endsWith(".css"))
    return "text/css";
  else if (path.endsWith(".txt"))
    return "text/plain";
  else if (path.endsWith(".js"))
    return "application/javascript";
  else if (path.endsWith(".png"))
    return "image/png";
  else if (path.endsWith(".gif"))
    return "image/gif";
  else if (path.endsWith(".jpg"))
    return "image/jpeg";
  else if (path.endsWith(".jpeg"))
    return "image/jpeg";
  else if (path.endsWith(".ico"))
    return "image/x-icon";
  else if (path.endsWith(".svg"))
    return "image/svg+xml";
  else if (path.endsWith(".xml"))
    return "text/xml";
  else if (path.endsWith(".pdf"))
    return "application/pdf";
  else if (path.endsWith(".zip"))
    return "application/zip";
  else if (path.endsWith(".gz"))
    return "application/x-gzip";
  else if (path.endsWith(".json"))
    return "application/json";
  return "application/octet-stream";
}

/***************************************************************************/

bool defaultConfig()
{
  if (DEBUG_WEB) {
    Serial.println("defaultConfig");
  }

  config.universe = UNIVERSE_MIN;
  config.channels = CHANNELS_MAX;
  config.delay = 25;

  return saveConfig();
}

bool loadConfig()
{
  if (DEBUG_WEB) {
    Serial.println("loadConfig");
  }

  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile)
  {
    if (DEBUG_WEB) {
      Serial.println("Failed to open config file");
    }
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024)
  {
    if (DEBUG_WEB) {
      Serial.println("Config file size is too large");
    }
    configFile.close();
    return false;
  }

  std::unique_ptr<char[]> buf(new char[size]);
  if (!buf)
  {
    if (DEBUG_WEB) {
      Serial.println("Memory allocation failed");
    }
    configFile.close();
    return false;
  }

  configFile.readBytes(buf.get(), size);
  configFile.close();

  JsonDocument root;
  DeserializationError error = deserializeJson(root, buf.get(), size);
  if (error)
  {
    if (DEBUG_WEB) {
      Serial.println("Failed to parse config file");
    }
    return false;
  }

  if (root["universe"].is<uint16_t>())
  {
    uint16_t value = root["universe"].as<uint16_t>();
    config.universe = constrain(value, UNIVERSE_MIN, UNIVERSE_MAX);
  }

  if (root["channels"].is<uint16_t>())
  {
    uint16_t value = root["channels"].as<uint16_t>();
    config.channels = constrain(value, CHANNELS_MIN, CHANNELS_MAX);
  }

  if (root["delay"].is<uint16_t>())
  {
    uint16_t value = root["delay"].as<uint16_t>();
    config.delay = constrain(value, DELAY_MIN, DELAY_MAX);
  }

  return true;
}

bool saveConfig()
{
  if (DEBUG_WEB) {
    Serial.println("saveConfig");
  }
  JsonDocument root;

  root["universe"] = constrain(config.universe, UNIVERSE_MIN, UNIVERSE_MAX);
  root["channels"] = constrain(config.channels, CHANNELS_MIN, CHANNELS_MAX);
  root["delay"] = constrain(config.delay, DELAY_MIN, DELAY_MAX);

  config.universe = root["universe"].as<uint16_t>();
  config.channels = root["channels"].as<uint16_t>();
  config.delay = root["delay"].as<uint16_t>();

  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile)
  {
    if (DEBUG_WEB) {
      Serial.println("Failed to open config file for writing");
    }
    return false;
  }

  if (DEBUG_WEB) {
    Serial.println("Writing to config file");
  }
  size_t bytesWritten = serializeJson(root, configFile);
  configFile.close();

  if (bytesWritten == 0)
  {
    if (DEBUG_WEB) {
      Serial.println("Failed to write to config file");
    }
    return false;
  }

  if (DEBUG_WEB) {
    Serial.print("Config saved successfully (");
    Serial.print(bytesWritten);
    Serial.println(" bytes)");
  }
  return true;
}

void printRequest()
{
  if (!DEBUG_WEB) return;
  
  String message = "HTTP Request\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nHeaders: ";
  message += server.headers();
  message += "\n";
  for (uint8_t i = 0; i < server.headers(); i++)
  {
    message += " " + server.headerName(i) + ": " + server.header(i) + "\n";
  }
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  Serial.println(message);
}

void handleUpdate1()
{
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
  ESP.restart();
}

void handleUpdate2()
{
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START)
  {
    Serial.setDebugOutput(true);
    WiFiUDP::stopAll();
    Serial.printf("Update: %s\n", upload.filename.c_str());
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace))
    { // start with max available size
      Update.printError(Serial);
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
    {
      Update.printError(Serial);
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (Update.end(true))
    { // true to set the size to the current progress
      Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
    }
    else
    {
      Update.printError(Serial);
    }
    Serial.setDebugOutput(false);
  }
  yield();
}

void handleDirList()
{
  if (DEBUG_WEB) {
    Serial.println("handleDirList");
  }
  String str = "";
  const size_t maxSize = 4096; // Set a reasonable limit
  size_t currentSize = 0;

  Dir dir = LittleFS.openDir("/");
  while (dir.next() && currentSize < maxSize)
  {
    String entry = dir.fileName() + " " + String(dir.fileSize()) + " bytes\r\n";
    if (currentSize + entry.length() <= maxSize)
    {
      str += entry;
      currentSize += entry.length();
    }
    else
    {
      str += "[listing truncated]";
      break;
    }
  }
  server.send(200, "text/plain", str);
}

void handleNotFound()
{
  if (DEBUG_WEB) {
    Serial.print("handleNotFound: ");
    Serial.println(server.uri());
  }
  if (LittleFS.exists(server.uri()))
  {
    handleStaticFile(server.uri());
  }
  else
  {
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i = 0; i < server.args(); i++)
    {
      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.setContentLength(message.length());
    server.send(404, "text/plain", message);
  }
}

void handleRedirect(const char *filename)
{
  handleRedirect((String)filename);
}

void handleRedirect(String filename)
{
  if (DEBUG_WEB) {
    Serial.println("handleRedirect: " + filename);
  }
  server.sendHeader("Location", filename, true);
  server.setContentLength(0);
  server.send(302, "text/plain", "");
}

bool handleStaticFile(const char *path)
{
  return handleStaticFile((String)path);
}

bool handleStaticFile(String path)
{
  if (DEBUG_WEB) {
    Serial.println("handleStaticFile: " + path);
  }
  String contentType = getContentType(path); // Get the MIME type
  if (LittleFS.exists(path))
  {
    File file = LittleFS.open(path, "r"); // Open it
    if (!file)
    {
      if (DEBUG_WEB) {
        Serial.println("\tFailed to open file");
      }
      return false;
    }
    server.setContentLength(file.size());
    server.streamFile(file, contentType); // And send it to the client
    file.close();                         // Then close the file again
    return true;
  }
  if (DEBUG_WEB) {
    Serial.println("\tFile Not Found");
  }
  return false; // If the file doesn't exist, return false
}

void handleJSON()
{
  // this gets called in response to either a PUT or a POST
  if (DEBUG_WEB) {
    Serial.println("handleJSON");
    printRequest();
  }

  bool configChanged = false;

  if (server.hasArg("universe") || server.hasArg("channels") || server.hasArg("delay"))
  {
    // the body is key1=val1&key2=val2&key3=val3 and the ESP8266Webserver has already parsed it
    if (server.hasArg("universe"))
    {
      unsigned int value = server.arg("universe").toInt();
      config.universe = constrain(value, UNIVERSE_MIN, UNIVERSE_MAX);
      configChanged = true;
    }

    if (server.hasArg("channels"))
    {
      unsigned int value = server.arg("channels").toInt();
      config.channels = constrain(value, CHANNELS_MIN, CHANNELS_MAX);
      configChanged = true;
    }

    if (server.hasArg("delay"))
    {
      unsigned int value = server.arg("delay").toInt();
      config.delay = constrain(value, DELAY_MIN, DELAY_MAX);
      configChanged = true;
    }

    handleStaticFile("/reload_success.html");
  }
  else if (server.hasArg("plain"))
  {
    // parse the body as JSON object
    JsonDocument root;
    const size_t MAX_JSON_SIZE = 1024; // Set a reasonable limit

    if (server.arg("plain").length() > MAX_JSON_SIZE)
    {
      Serial.println("JSON data too large");
      handleStaticFile("/reload_failure.html");
      return;
    }

    DeserializationError error = deserializeJson(root, server.arg("plain").c_str(), server.arg("plain").length());
    if (error)
    {
      Serial.print("JSON parsing failed: ");
      Serial.println(error.c_str());
      handleStaticFile("/reload_failure.html");
      return;
    }

    // Load and validate configuration values
    if (root["universe"].is<unsigned int>())
    {
      unsigned int value = root["universe"].as<unsigned int>();
      config.universe = constrain(value, UNIVERSE_MIN, UNIVERSE_MAX);
      configChanged = true;
    }

    if (root["channels"].is<unsigned int>())
    {
      unsigned int value = root["channels"].as<unsigned int>();
      config.channels = constrain(value, CHANNELS_MIN, CHANNELS_MAX);
      configChanged = true;
    }

    if (root["delay"].is<unsigned int>())
    {
      unsigned int value = root["delay"].as<unsigned int>();
      config.delay = constrain(value, DELAY_MIN, DELAY_MAX);
      configChanged = true;
    }

    handleStaticFile("/reload_success.html");
  }
  else
  {
    handleStaticFile("/reload_failure.html");
    return; // do not save the configuration
  }

  // Only save if configuration actually changed
  if (configChanged)
  {
    saveConfig();
  }
}
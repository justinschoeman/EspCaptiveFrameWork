/*
 * Wifi functions
 */
 
/*void start_wifi() {
  wc.setDebug(true);
  wc.setAPCallback(config_mode_cb);
  wc.setAPModeTimeoutMins(5);
  //wc.resetSettings(); //helper to remove the stored wifi connection, comment out after first upload and re upload
  if(!wc.autoConnect()) {
    //wc.startConfigurationPortal(AP_WAIT);
    wc.startConfigurationPortal((WiFi.SSID() == "") ? AP_WAIT : AP_NONE); // if no wifi configuration stored on device, then remain in captive portal until configured. otherwise, wait 5 minutes then continue to run without wifi
    ESP.restart();
  }
}*/

/*
 *  Generic html helpe strings
 */

// Strings constructed from progmem randonly explode, so make these normal consts
static const char _page_head[] = "<html><head><meta charset=\"UTF-8\"></head><body>";
static const char  _page_tail_home[] = "<p><button onclick=\"window.location='index.html'\">HOME</button></p></body></html>";

// strings that we never modify can be progmem
static const char  _page_type_text[] PROGMEM = "text/plain";
static const char  _page_type_html[] PROGMEM = "text/html";
static const char  _page_type_json[] PROGMEM = "application/json";

/*
 * Really, really not found
 */
void page_404() {
  wa.server_page_headers(true);

  String message = "File Not Found\n\n";
  message += "URI: ";
  message += wa.server.uri();
  message += "\nMethod: ";
  message += (wa.server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += wa.server.args();
  message += "\n";

  for (uint8_t i = 0; i < wa.server.args(); i++) {
    message += " " + wa.server.argName(i) + ": " + wa.server.arg(i) + "\n";
  }

  wa.server.send(404, _page_type_text, message);
}

/*
 * Send a CPool as a JSON response
 */
void page_json(CPool & cp) {
  wa.server_page_headers(true);

  String json = "";
  cp.var_json(json);
  wa.server.send(200, _page_type_json, json);
}

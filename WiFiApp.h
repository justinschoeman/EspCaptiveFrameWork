#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>

class WiFiApp {
public:
  WiFiApp(): server(80) {}

  // override defaults

  // default none
  void set_wifi_ssid(const String & s) { wifi_ssid = s; }

  // default none
  void set_wifi_passwd(const String & s) { wifi_passwd = s; }

  // default 172.31.99.99
  void set_ap_ip(const IPAddress & ip) { ap_ip = ip; }

  // default 172.31.99.99
  void set_ap_nm(const IPAddress & ip) { ap_nm = ip; }

  // default ESPAP_<chipid>
  void set_ap_ssid(const String & s) { ap_ssid = s; }

  // default no password
  void set_ap_pass(const String & s) { ap_pass = s; }

  // default 53
  void set_dns_port(unsigned char i) { dns_port = i; }

  // default WiFiApp
  void set_mdns_name(const String & s) { mdns_name = s; }

  // setup handler
  void setup(void) {
    state_setup();
    server_setup();
  }

  // loop handler
  void loop(void) {
    // if we have an active network, run all services
    if(ap_running() || wifi_running()) all_loop();

    // wifi management state loop (at the intervals it requests
    unsigned long now = millis();
    now -= state_time;
    if(now >= state_interval) {
      state_interval = state_run();
      state_time = millis();
    }
  }

  /*
   * public server helpers
   */

  // server object is public - should be fully available to clients
  ESP8266WebServer server;

  /*
   * Send generic page headers for static or dynamic pages (based on bool dyn)
   */
  void server_page_headers(bool dyn) {
    // always allow cross origin requests (FIXME - disable in prod?)
    server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
    // disable keep-alive (somehow server does not always send correct length)
    server.keepAlive(false);
    if(dyn) {
      // dynamic content - never cache
      server.sendHeader(F("Cache-Control"), F("no-cache, no-store, must-revalidate"));
      server.sendHeader(F("Pragma"), F("no-cache"));
      server.sendHeader(F("Expires"), F("-1"));
    } else {
      // static content - cache long
      server.sendHeader(F("Cache-Control"), F("max-age=604800, must-revalidate"));
    }
  }


private:
  /*
   * overall state engine
   */
  int mode = -1; // -1 = pre-setup, 0 = uninit, 1 = ap, 2 = sta
  unsigned long state_time;
  unsigned long state_interval;

  void state_setup(void) {
    state_time = millis();
    state_interval = 0;
  }

  unsigned long state_run(void) {
    Serial.println(String("State mode: ")+mode);
    switch(mode) {
      case -1:
        // initial setup
        Serial.println("PREININIT");
        WiFi.persistent(true);
        Serial.println(String("Stored SSID: ")+WiFi.SSID());
        mode = 0;
        return 0;
      case 0:
        // choose operation mode
        Serial.println("UNINIT");
        if(wifi_ssid != "") {
          Serial.println(String("Hardcoded AP: ")+wifi_ssid);
          state_set_wifi();
          return 0;
        }
        if(WiFi.SSID() != "") {
          Serial.println(String("Stored AP: ")+WiFi.SSID());
          state_set_wifi();
          return 0;
        }
        Serial.println("No stored/configured SSID - revert to AP mode");
        state_set_ap();
        return 0;
      case 1:
        // ap mode
        Serial.println("AP");
        return ap_run();
      case 2:
        // station mode
        Serial.println("STA");
        return wifi_run();
      default:
        Serial.println(String("Invalid mode: ")+mode);
        while(1) {}
    }
  }

  // switch to / initialise AP mode
  void state_set_ap(void) {
    mode = 1;
    ap_state = 0;
  }

  // switch to / initialise station mode
  void state_set_wifi(void) {
    mode = 2;
    wifi_state = 0;
    wifi_retries = 0;
  }

  /*
   * WiFi
   */
  String wifi_ssid = "";
  String wifi_passwd = "";
  int wifi_state;
  unsigned long wifi_time;
  int wifi_retries;

  unsigned long wifi_run(void) {
    Serial.println(String("wifi_state: ")+wifi_state);
    switch(wifi_state) {
      case 0:
        // start connection
        if(WiFi.status() == WL_CONNECTED) {
          Serial.println("already connected...");
          wifi_state = 2;
          return 0;
        }
        if(wifi_ssid != "") {
          Serial.println(String("Try to connect to: ")+wifi_ssid);
          WiFi.begin(wifi_ssid, wifi_passwd);
        } else {
          Serial.println("Try to connect to last AP");
          WiFi.begin();
        }
        wifi_state = 1;
        wifi_time = millis();
        return 1000UL;
      case 1: {
        // wait for connection
        auto i = WiFi.status();
        Serial.println(String("Connect state: ")+i);
        // connected?
        if(i == WL_CONNECTED) {
          Serial.println("wifi (station mode) connected...");
          wifi_state = 2;
          return 0;
        }
        // definite permanent failures?
        if(i == WL_WRONG_PASSWORD) {
          // password failure = permanent
          Serial.println("Bad password - switch to AP mode");
          state_set_ap();
          return 0UL;
        }
        // failed/timeout?
        if(i == WL_NO_SSID_AVAIL || i == WL_CONNECT_FAILED || i == WL_CONNECTION_LOST || (millis() - wifi_time) > 30000UL) {
          Serial.println("Connect failed");
          // other failure - retry
          wifi_retries++;
          if(wifi_retries >= 3) {
            // too many retries
            Serial.println("Too many retries - switch to AP mode");
            state_set_ap();
            return 0UL;
          }
          Serial.println("retry");
          wifi_state = 0;
          return 0UL;
        }
        // other states? wait some more
        Serial.println("waiting to connect...");
        return 1000UL;
      }
      case 2:
        // connection startup
        Serial.println("STA STARTING");
        // stop anything which may have been running
        all_stop();
        // start what should be running in STA mode
        mdns_start();
        server_start();
        wifi_state = 3;
        return 0UL;
      case 3:
        // connected
        // still connected?
        if(WiFi.status() != WL_CONNECTED) {
          Serial.println("wifi connection lost...");
          state_set_wifi();
          return 0UL;
        }
        Serial.print("STA RUNNING ");
        Serial.println(WiFi.localIP());
        return 10000UL;
      default:
        Serial.println(String("Invalid wifi_state: ")+wifi_state);
        while(1) {}
    }
  }

  bool wifi_running(void) { return wifi_state == 3; }

  /*
   * AP
   */
  IPAddress ap_ip = IPAddress(172, 31, 99, 99);
  IPAddress ap_nm = IPAddress(255, 255, 255, 0);
  String ap_ssid = "";
  String ap_pass = "";
  int ap_state;

  void ap_scan_update(int cnt) {
    Serial.println(String("AP SCAN UPDATE: ")+cnt);
  }

  unsigned long ap_run(void) {
    Serial.println(String("ap_state: ")+ap_state);
    switch(ap_state) {
      case 0:
        WiFi.softAPConfig(ap_ip, ap_ip, ap_nm);
        if(ap_ssid == "") {
          ap_ssid = "ESPAP_";
          ap_ssid += ESP.getChipId();
        }
        Serial.println(String("AP NAME: ")+ap_ssid);
        Serial.println(String("AP PSWD: ")+ap_pass);
        WiFi.softAP(ap_ssid.c_str(), ap_pass.c_str());
        ap_state = 1;
        return 500UL;
      case 1:
        Serial.println(String("AP IP address: ")+WiFi.softAPIP().toString());
        // stop anything which may have been running
        all_stop();
        // start services which should be running in AP mode
        dns_start();
        mdns_start();
        ap_state = 2;
        return 0UL;
      case 2:
        Serial.println("AP UPDATE");
         WiFi.scanNetworksAsync([this](int a){ap_scan_update(a);});
         // FIXME - try connect if our target AP becomes available
        return 60000UL;
      default:
        Serial.println(String("Invalid ap_state: ")+ap_state);
        while(1) {}
    }
  }

  bool ap_running(void) { return ap_state == 2; }

  /*
   * Server
   */
  bool server_running = false;
  unsigned long server_timestamp;

  void server_setup(void) {
    server.onNotFound([this](){server_page_serve();});
    server.addHook([this](const String& method, const String& url, WiFiClient* client, ESP8266WebServer::ContentTypeFunction contentType){return server_hook(method, url, client, contentType);});
  }

  void server_start(void) {
    if(!server_running) {
      server.begin();
      server_running = true;
      server_timestamp = millis();
    }
  }

  void server_stop(void) {
    if(server_running) {
      server.close();
      server_running = false;
    }
  }

  void server_loop(void) {
    if(!server_running) return;
    //Serial.print("s");
    server.handleClient();
  }

  ESP8266WebServer::ClientFuture server_hook(const String& method, const String& url, WiFiClient* client, ESP8266WebServer::ContentTypeFunction contentType) {
    server_timestamp = millis();
    Serial.print("hook: ");
    Serial.println(server_timestamp);
    return ESP8266WebServer::CLIENT_REQUEST_CAN_CONTINUE;
  }

  void server_page_serve(void) {
    bool retr = false;
  
    // see if this matches a page on the filesystem?
    // build path
    String path = ESP8266WebServer::urlDecode(server.uri());
  retry:
    Serial.print("Fetch: ");
    Serial.println(path);
    if(path.endsWith("/")) { path += "index.html"; }
    if(!path.startsWith("/")) { path = "/"+path; }
    path = "/www"+path;
    Serial.print("File: ");
    Serial.println(path);
  
    // if path exists, then stream it
    if(LittleFS.exists(path)) {
      server_page_headers(false);
      String contentType = mime::getContentType(path);
      File file = LittleFS.open(path, "r");
      server.keepAlive(false);
      if(server.streamFile(file, contentType) != file.size()) {
        Serial.println("Sent less data than expected!");
      }
      file.close();
      return;
    }
  
    // for captive portal, serve index.html for everything...
    if(!retr) {
      retr = true;
      path = "/index.html";
      goto retry;
    }
  
    // otherwise, return a 404 page
    //page_404();

  }


  /*
   * DNS
   */
  DNSServer * dns = NULL;
  unsigned char dns_port = 53;

  void dns_start(void) {
    if(!dns) {
      dns = new DNSServer();
      dns->setErrorReplyCode(DNSReplyCode::NoError);
      dns->start(dns_port, "*", ap_ip);
    }
  }

  void dns_stop(void) {
    if(dns) {
      dns->stop();
      delete dns;
      dns = NULL;
    }
  }

  void dns_loop(void) {
    if(!dns) return;
    //Serial.print("d");
    dns->processNextRequest();
  }

  /*
   * MDNS
   */
  String mdns_name = "WiFiApp";
  bool mdns_running = false;

  void mdns_start(void) {
    if(!mdns_running) {
      if(MDNS.begin(mdns_name)) {
        Serial.println(String("MDNS responder started: ")+mdns_name);
      }
      // Add service to MDNS-SD
      MDNS.addService("http", "tcp", 80);
      mdns_running = true;
    }
  }

  void mdns_stop(void) {
    if(mdns_running) {
      MDNS.end();
      mdns_running = false;
    }
  }

  void mdns_loop(void) {
    if(!mdns_running) return;
    //Serial.print("m");
    MDNS.update();
  }

  void all_loop(void) {
    server_loop();
    dns_loop();
    mdns_loop();
  }

  void all_stop(void) {
    // stop all running services
    dns_stop();
    mdns_stop();
    server_stop();
  }
};

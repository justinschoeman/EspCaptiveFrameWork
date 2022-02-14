#include <LittleFS.h>
#include "WiFiApp.h"

WiFiApp wa;

// global dummy string used to force concatenation
String _s = "";


const char * PATH_UPLOAD = "/uploads";
const char * PATH_WWW = "/www";

/*
 * load and initialise config
 */
 
#include "config.h"

CPool c_status("status");
#define STATUS(t, n, v) t n = c_status.F_ ##t(#n, v)

CPool c_config("config.jsn");
//CPool c_config = c_status.F_CPool("config.jsn");
#define CONFIG(t, n, v) t n = c_config.F_ ##t(#n, v)

CONFIG(CString, cfg_mdns_name, "fan");
bool cfg_mdns_name_watch(void * sp) {
  char * s = (char*)sp;
  if(strlen(s) > 255) return false;
  const String goodchars("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-");
  for(; *s; s++) {
    if(goodchars.indexOf(*s) < 0) return false;
  }
  return true;
}

/*
 * load and initialise temperature module
 */
 
#include "temp.h"

/*
 * load and initialise wifi
 */


/*
 * common wifi functions
 */
 
#include "pages.h"

// custom post handler
void page_post() {
  wa.server_page_headers(true);

  // process form
  for(int i = 0; i < wa.server.args(); i++) {
    String name = wa.server.argName(i);
    String value = wa.server.arg(i);
    Serial.print("Arg: '");
    Serial.print(name);
    Serial.print("' = '");
    Serial.print(value);
    Serial.println("'");
    CVar * var = c_config.get_var(name);
    if(!var) {
      wa.server.send(404, _page_type_text, "VARIABLE NOT FOUND: "+name);
      return;
    }
    if(!var->set_var(value)) {
      wa.server.send(400, _page_type_text, "VARIABLE "+name+" VALUE DOES NOT DECODE CORRECTLY: "+value);
      return;
    }
  }
  if(!c_config.save()) {
    wa.server.send(500, _page_type_text, "VARIABLE CHANGES DID NOT SAVE");
    return;
  }
  wa.server.send(200, _page_type_text, "OK");
}

String _logstr;
void slog(String str) {
  Serial.println(str);
  _logstr += str+"\n";
}

/*
 * custom upload handler
 */

File page_upload_file;

void page_upload(const char * fin) {
  HTTPUpload& upload = wa.server.upload();
  if(upload.status == UPLOAD_FILE_START){
    Serial.println(String("Upload file "+upload.filename+" to "+fin));
    _logstr = _page_head;
    _logstr += "<h1>Upload file '"+upload.filename+"' to '"+fin+"'</h1><pre>";
    if(!LittleFS.exists(PATH_UPLOAD)) {
      slog("Making uploads folder");
      LittleFS.mkdir(PATH_UPLOAD);
    }
    if(LittleFS.exists(PATH_UPLOAD)) {
      String filename = PATH_UPLOAD;
      filename += "/";
      filename += fin;
      slog(String("Upload to: ")+filename);
      page_upload_file = LittleFS.open(filename, "w");
      if(!page_upload_file) slog("Failed to create file: "+filename);
    } else {
      slog("Making uploads folder failed!");
    }
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(page_upload_file) {
      int i = page_upload_file.write(upload.buf, upload.currentSize);
      Serial.println(_s+"Wrote "+i+" bytes");
      if(i != upload.currentSize) {
        slog("Write failed!");
        page_upload_file.close();
      }
    }
  } else if(upload.status == UPLOAD_FILE_END){
    if(page_upload_file) {
      page_upload_file.close();
      slog("Uploaded Size: "+upload.totalSize);
      slog(_s+"UPLOADED "+upload.totalSize+" BYTES OK");
    } else {
      slog("UPLOAD FAILED!");
    }
    _logstr += _s+"</pre>"+_page_tail_home;
    wa.server_page_headers(true);
    wa.server.send(200, _page_type_html, _logstr);
  }
}

/*
 * Dump status of current uploaded files.
 */
#include <MD5Builder.h>
void page_files(void) {
  wa.server_page_headers(true);

  Dir d = LittleFS.openDir(PATH_UPLOAD);
  String json = "{";
  while(d.next()) {
    if(!d.isFile()) continue;
    Serial.println(d.fileName().c_str());
    if(json != "{") json += ",";
    json += "\"";
    c_config.json_str(json, d.fileName());
    json += "\":\"";
    MD5Builder md5;
    md5.begin();
    File f = d.openFile("r");
    unsigned char buf[256];
    int i;
    while((i = f.read(buf, 256)) > 0) md5.add(buf, i);
    f.close();
    md5.calculate();
    c_config.json_str(json, md5.toString());
    json += "\"";
  }
  json += "}";
  wa.server.send(200, _page_type_json, json);
}

/*
 * Update from uploaded files
 */

void _prelog(String s) {
  Serial.println(s);
  wa.server.sendContent(s+"\n");
  yield();
}

#include "tar.h"
Tar tar;

void page_update_www(void) {
  wa.server_page_headers(true);

  // use HTTP/1.1 Chunked response for progressive output
  if(!wa.server.chunkedResponseModeStart(200, "text/html")) {
    wa.server.send(505, "text/html", "HTTP1.1 required");
    return;
  }
  wa.server.sendContent(F("<html><head><meta charset=\"UTF-8\"></head><body><h1>Updating Web Application...</h1><pre>"));
  String filename = PATH_UPLOAD;
  filename += "/www.tar";
  _prelog(_s+"Test file: "+filename);
  tar.clear();
  tar.set_logger(_prelog);
  //if(!untar(filename, NULL)) {
  if(!tar.untar(filename, NULL)) {
    _prelog(_s+"Error when testing file - not updating!");
    goto done;
  }
  _prelog(_s+"Tested OK - Install now: "+filename);
  //if(!untar(filename, PATH_WWW)) {
  if(!tar.untar(filename, PATH_WWW)) {
    _prelog(_s+"Error installing file!");
    goto done;
  }
  _prelog(_s+"Deleting file: "+filename);
  LittleFS.remove(filename);
 done:
  _prelog(_s+"Finishing up...");
  wa.server.sendContent(F("</pre><p><button onclick=\"window.location='index.html'\">STATUS</button></p></body></html>"));
  wa.server.chunkedResponseFinalize();
}

#include <Updater.h>
void page_update_firmware(void) {
  wa.server_page_headers(true);

  bool ok = false;
  // use HTTP/1.1 Chunked response for progressive output
  if(!wa.server.chunkedResponseModeStart(200, "text/html")) {
    wa.server.send(505, "text/html", "HTTP1.1 required");
    return;
  }
  wa.server.sendContent(F("<html><head><meta charset=\"UTF-8\"></head><body><h1>Updating Firmware...</h1><pre>"));
  String filename = PATH_UPLOAD;
  filename += "/firmware.bin";
  File f = LittleFS.open(filename, "r");
  int fs;
  if(!f) {
    _prelog(_s+"Error opening file: "+filename);
    goto done;
  }
  _prelog(_s+"File opened: "+filename);
  fs = f.size();
  _prelog(_s+"Starting update bytes: "+fs);
  if(!Update.begin(fs)) {
    _prelog(_s+"Could not start update?");
    goto done;
  }
  _prelog(_s+"Writing...");
  Update.writeStream(f);
  _prelog(_s+"Finishing...");
  if(!Update.end()) {
    _prelog(_s+"Update failed: "+Update.getError());
    goto done;
  }
  ok = true;
 done:
  if(f) f.close();
  if(ok) LittleFS.remove(filename);
  _prelog(_s+"Finished... ");
  if(ok) _prelog(_s+"(Rebooting now - please wait a few seconds before you click to continue.)");
  wa.server.sendContent(F("</pre><p><button onclick=\"window.location='index.html'\">STATUS</button></p></body></html>"));
  wa.server.chunkedResponseFinalize();
  yield();
  if(ok) ESP.restart();
}

void setup() {
  Serial.begin(115200);
  //gdbstub_init();
  while (!Serial) {}
  delay(1000);
  Serial.println("....");
  Serial.println("....");

  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    while(1) {}
  }
  Serial.println("FS Open");

  //load config
  c_config.load();
  // save any defaults not yet overloaded
  c_config.save();
  // custom watcher
  cfg_mdns_name.watch(cfg_mdns_name_watch);
  
  // set up temperature after loading config (active watchers during load can cause false failures with out of sequence updates)
  temp_setup();

  wa.set_wifi_ssid("valinor_24");
  wa.set_wifi_passwd("geelwortel");
  wa.set_mdns_name(cfg_mdns_name);
  wa.setup();
  

  // register custom pages
  wa.server.on("/status", [](){ page_json(c_status); });
  wa.server.on("/config", [](){ page_json(c_config); });
  wa.server.on("/files", page_files);
  wa.server.on("/post", page_post);
  wa.server.on("/upload_firmware", HTTP_POST, [](){ wa.server.send(200); }, [](){page_upload("firmware.bin");});
  wa.server.on("/update_firmware", page_update_firmware);
  wa.server.on("/upload_www", HTTP_POST, [](){ wa.server.send(200); }, [](){ page_upload("www.tar"); });
  wa.server.on("/update_www", page_update_www);

  Serial.println("HTTP server started");
}

void loop() {
  // run temperature controller
  temp_run();
  
  // run web server
  wa.loop();
}

#if 1

/*
 * no idea why this does not work...
 */

class Tar {
public:
  Tar(): tarmode(0), out_dir("") {
    secbuf = (unsigned char *)malloc(512);
  }
  
  ~Tar() {
    free(secbuf);
  }

  /*
   * clear context state (logger and upload filename)
   */
  void clear() {
    logfn = NULL;
  }

  /* 
   * set a log callback 
   */
  void set_logger(void (*fn)(String)) { logfn = fn; }

  /* 
   * start test untar 
   */
  bool begin_test(void) {
    tarmode = 1;
    out_dir = "";
    return begin_untar_common();
  }
  
  /* 
   * start real untar to 'dir' 
   */
  bool begin_untar(String dir) {
    tarmode = 2;
    out_dir = dir;
    // strip trailing '/' for consistency
    if(out_dir.endsWith("/")) out_dir.substring(0, out_dir.length()-1);
    log(_s+"Untar to: "+out_dir);
    if(!safe_mkdir(out_dir)) {
      log(_s+"Error getting/making tar output directory: "+out_dir);
      return false;
    }
    return begin_untar_common();
  }

  /* 
   * end tar/untar process and check 
   */
  bool end(void) {
    log(_s+"Ending mode: "+tarmode);
    if(tarmode < 1 || tarmode > 3) {
      log(_s+"Invalid mode "+tarmode+" for end...");
      return false;
    }
    // preemptively mark complete
    tarmode = 0;
    if(file) {
      log(_s+"end() called with open file.");
      file.close();
      return false;
    }
    if(file_len > 0) {
      log(_s+"end() called with incomplete file.");
      return false;
    }
    if(fill_len > 0 && fill_len < 512) {
      log(_s+"end() called with incomplete sector buffer.");
      return false;
    }
    return true;
  }

  /* 
   * push data into untar engine 
   */
  bool untar_push(unsigned char * buf, int buf_len) {
    // sanity check
    if(fill_len >= 512) {
      log(_s+"Huh? fill_len too big!");
      return false;
    }

    // consume all input data
    while(buf_len > 0) {
      // remaining buffer space
      int todo = 512 - fill_len;
      // if we have fewer bytes than this available, cap to this size
      if(buf_len < todo) todo = buf_len;

      // insert data into sector buffer
      memcpy(secbuf + fill_len, buf, todo);
      fill_len += todo;
      buf += todo;
      buf_len -= todo;

      // process full buffer?
      if(fill_len == 512) {
        if(!untar_buf()) {
          log(_s+"Error processing sector: "+scnt);
          return false;
        }
      }
    }
  }

  /* 
   * process a full untar sector buffer 
   */
  bool untar_buf(void) {
    // sanity check - buffer must be complete
    if(fill_len != 512) {
      log(_s+"untar_buf() with incomplete buffer!");
      return false;
    }
    // mark processing sector number
    scnt++;
    // preemptively mark buffer as processed
    fill_len = 0;
    // process head/file as required
    if(file_len == 0) return untar_buf_head();
    return untar_buf_file();
  }
  
  /* 
   * process untar header sector buffer
   */
  bool untar_buf_head(void) {
    // sanity check
    if(file_len != 0 || file) {
      log(_s+"Process head with incompelte file at sector:"+scnt);
      return false;
    }
    
    // shortcut for padding blocks...
    if(secbuf[0] == 0 && secbuf[124] == 0  && secbuf[156] == 0) {
      log(_s+"pad...");
      return true;
    }

    // parse header
    // filename 0..99
    if(strnlen((char*)secbuf, 100) >= 100) {
      log(_s+"Filename too long at sector "+scnt);
      return false;
    }
    file_name = (char*)secbuf;
    // strip leading path indicators
    while(file_name.startsWith(".") || file_name.startsWith("/")) file_name = file_name.substring(1);
    // build full path
    file_name = out_dir + "/" + file_name;
    log(_s+"Output Filename: "+file_name);

    // length 124..135
    if(!get_oct((char*)(secbuf+124), 12, &file_len)) {
      log(_s+"Invalid length in file: "+file_name);
      return false;
    }
    log(_s+"Length: "+file_len);
    if(file_len < 0) {
      log(_s+"Invalid length in file: "+file_name);
      return false;
    }

    // type 156
    // FIXME - check other types?
    file_type = 0;
    log(_s+"Type: "+secbuf[156]);
    if(secbuf[156] == '5' || file_name.endsWith("/")) {
      // directory?
      if(file_name.endsWith("/")) file_name = file_name.substring(0, file_name.length()-1);
      log(_s+"Directory: "+file_name);
      if(file_len != 0) {
        log(_s+"Directory with non-zero length at file: "+file_name);
        return false;
      }
      file_type = 1;
    }
    log(_s+"File type: "+file_type);

    // process header
    if(tarmode == 1) return untar_buf_head_test();
    return untar_buf_head_real();
  }

  bool untar_buf_head_test(void) {
    log(_s+"Test mode");
    return true;
  }
  
  bool untar_buf_head_real(void) {
    log(_s+"For real...");
    if(file_type == 1) {
      // handle directory
      // make directory
      if(!safe_mkdir(file_name)) {
        _prelog(_s+"Error making directory: "+file_name);
        return false;
      }
      return true;
    }
    // handle file start
    // create file
    file = LittleFS.open(file_name, "w");
    if(!file) {
      log(_s+"Error creating file: "+file_name);
      return false;
    }
    log(_s+"File created: "+file_name+" : "+file_len);
    if(file_len == 0) {
      // 0 length file? close immediately
      file.close();
      log(_s+"0 length file complete: "+file_name+" : "+file_len);
    }
    log(_s+"done");
  }

  /* 
   * process untar file sector buffer
   */
  bool untar_buf_file(void) {
    // determine how much to write
    int olen = file_len > 512 ? 512 : file_len;
    log(_s+"Out: "+olen);

    // write if we are not in test mode
    if(tarmode == 2) {
      if(!file) {
        log(_s+"entered untar_buf_file without open file???");
        return false;
      }
      int i = file.write(secbuf, olen);
      if(i != olen) {
        log(_s+"File write failed: "+file_name);
        return false;
      }
    }

    // mark data as sent
    file_len -= olen;

    // finish file if we are done and not in test mode
    if(file_len ==0) {
      if(tarmode == 2) file.close();
      log(_s+"File complete: "+file_name);
    }
    return true;
  }

  /*
   * untar filename (only test if dir is null)
   */
  bool untar(String & filename, const char * dir) {
    if(!LittleFS.exists(filename)) {
      log(_s+"Input file '"+filename+"' does not exist?");
      return false;
    }
    File f = LittleFS.open(filename, "r");
    if(!f) {
      log(_s+"Error opening input file '"+filename+"' for reading...");
      return false;
    }
    if(dir) {
      if(!begin_untar(dir)) {
        log(_s+"Error starting untar.");
        return false;
      }
    } else {
      if(!begin_test()) {
        log(_s+"Error starting untar test.");
        return false;
      }
    }
    bool ret = true;
    while(1) {
      fill_len = f.read(secbuf, 512);
      if(fill_len == 0 && file_len == 0) {
        log(_s+"Done!");
        break;
      }
      ret = untar_buf();
      if(!ret) {
        log(_s+"Error processing sector: "+scnt);
        break;
      }
    }
    f.close();
    if(!end()) {
      log(_s+"Error ending untar.");
      return false;
    }
    return ret;
  }

  /*
   * helpers
   */

  /*
   * make directory if it does not yet exist
   */
  bool safe_mkdir(String dirname) {
    if(LittleFS.exists(dirname)) {
      log(_s+"Directory already exists: "+dirname);
    } else {
      LittleFS.mkdir(file_name);
      if(!LittleFS.exists(dirname)) {
        log(_s+"Error making directory: "+dirname);
        return false;
      }
      log(_s+"Directory created: "+dirname);
    }
    return true;
  }

  /*
   * parse octal string into int
   */
  bool get_oct(char * buf, int len, int * out) {
    *out = 0;
    for(int i = 0; i < len; i++) {
      if(buf[i] == 0) return true;
      if(buf[i] < '0' || buf[i] > '7') {
        log(_s+" invalid octal length char at sector "+scnt);
        return false;
      }
      *out *= 8;
      *out += buf[i]-'0';
    }
    return true;
  }

private:
  /* 
   * logger 
   */
  const String _s = ""; // dummy string to force cast to String
  void log(String s) {
    if(logfn) (*logfn)(s);
  }

  /* 
   * global untar init 
   */
  bool begin_untar_common() {
    scnt = -1;
    fill_len = 0;
    file_len = 0;
    file_name = "";
    file = File();
    return true;
  }

  unsigned char * secbuf;       // sector buffer
  int tarmode;                  // 0 = uniniut, 1 = test untar, 2 = untar, 3 = tar
  String out_dir;               // output directory (no trailing /)
  void (*logfn)(String) = NULL; // logger function
  int scnt;                     // sector count
  int fill_len;                 // sector buffer fill length
  String file_name;             // active tar/untar file name
  int file_len;                 // remaining file length
  int file_type;                // 0 = file, 1 = dir, 2 = update....
  File file;                    // active tar/untar file
};

#else

bool untar(String & fin, const char * dout) {
  if(dout) {
    // check/build www folder
    if(!LittleFS.exists(dout)) {
      _prelog(_s+"Make www folder: " + dout);
      LittleFS.mkdir(dout);
      if(!LittleFS.exists(dout)) {
        _prelog(_s+"Error making www folder!");
        return false;
      }
    }
    _prelog(_s+"Output path exists: "+dout);
  }
  if(!LittleFS.exists(fin)) {
    _prelog(_s+"File '"+fin+"' does not exist?");
    return false;
  }
  File f = LittleFS.open(fin, "r");
  if(!f) {
    _prelog(_s+"Error opening file '"+fin+"' for reading...");
    return false;
  }
  String ofn; // output file name
  File of; // output file
  int len = 0; // length of file left to read, or 0 for sector
  unsigned char sec[512]; // sector buffer
  int scnt = -1;
  while(1) {
    scnt++;
    int i = f.read(sec, 512);
    //_prelog(_s+"Read: "+i);
    if(i == 0 && len == 0) {
      _prelog(_s+"Done!");
      f.close();
      return true;
    }
    if(i != 512) {
      _prelog(_s+"File truncated!");
      break;
    }
    if(len == 0) {
      // reading header
      // shortcut for padding blocks...
      if(sec[0] == 0 && sec[124] == 0  && sec[156] == 0) {
        Serial.println("pad...");
        continue;
      }
      // filename 0..99
      if(strnlen((char*)sec, 100) >= 100) {
        _prelog(_s+"Filename too long at sector "+scnt);
        break;
      }
      ofn = (char*)sec;
      while(ofn.startsWith(".") || ofn.startsWith("/")) ofn = ofn.substring(1);
      ofn = _s + dout + "/" + ofn;
      _prelog(_s+"Output Filename: "+ofn);
      // length 124..135
      len = 0;
      for(i = 124; i < 136; i++) {
        //_prelog(_s+"len char: "+sec[i]);
        if(sec[i] == 0) break;
        if(sec[i] < '0' || sec[i] > '7') {
          _prelog(_s+" invalid octal length char at sector "+scnt);
          len = -1;
          break;
        }
        len *= 8;
        len += sec[i]-'0';
      }
      _prelog(_s+"Length: "+len);
      if(len < 0) {
        _prelog(_s+"Invalid length");
        break;
      }
      // type 156
      _prelog(_s+"Type: "+sec[156]);
      // directory?
      bool dir = false;
      if(sec[156] == '5' || ofn.endsWith("/")) {
        if(ofn.endsWith("/")) ofn = ofn.substring(0, ofn.length()-1);
        _prelog(_s+"Directory: "+ofn);
        dir = true;
      }
      if(dir) {
        // handle directory
        if(len != 0) {
          _prelog(_s+"Directory with non-zero length?");
          break;
        }
        if(dout) {
          // make directory
          if(LittleFS.exists(ofn)) {
            _prelog(_s+"Directory already exists: "+ofn);
          } else {
            LittleFS.mkdir(ofn);
            if(!LittleFS.exists(ofn)) {
              _prelog(_s+"Error making directory: "+ofn);
              break;
            }
            _prelog(_s+"Directory created: "+ofn);
          }
        }
      } else {
        // handle file
        if(dout) {
          // create file
          of = LittleFS.open(ofn, "w");
          if(!of) {
            _prelog(_s+"Error creating file: "+ofn);
            break;
          }
          _prelog(_s+"File created: "+ofn);
          if(len == 0) {
            // 0 length file? close immediately
            of.close();
            _prelog(_s+"0 length file complete: "+ofn);
          }
        }
      }
    } else {
      // reading data...
      int olen = len > 512 ? 512 : len;
      _prelog(_s+"Out: "+olen);
      if(dout) {
        i = of.write(sec, olen);
        //_prelog(_s+"Written: "+i);
        if(i != olen) {
          _prelog(_s+"File write failed: "+ofn);
          break;
        }
      }
      len -= olen;
      if(len ==0 && dout) {
        of.close();
        _prelog(_s+"File complete: "+ofn);
      }
    }
  }
  f.close();
  return false;
}
#endif

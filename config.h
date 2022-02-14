/*
 * 
 * JSON is strictly tokenised with newlines separating each component.
 * 
 * One name/value pair per line with no extra spaces/characters
 * One character on every other line
 * 
 */

/*
 * 
 *  BASE CLASS
 *  
 */

class CVar {
  public:
    CVar(const char * name, unsigned int flags) : name(name), flags(flags), next(NULL), cb(NULL) { }
    ~CVar() {
      if(next) { delete next; }
    }
    virtual bool set_var(String & str) { return false; }  // set variable from string return true if ok
    virtual void var_json(String & str) { Serial.println("wtf?"); }                // append json representation to srt
    void watch(bool (*cbi)(void*)) { cb = cbi; }

    // append 'add' to 'str' with JSON escaping
    void json_str(String & str, String add) {
      for(int i = 0; i < add.length(); i++) {
        int e = esc_chars.indexOf(add[i]);
        if(e >= 0) {
            str += esc_repl[e];
        } else {
          str += add[i];
        }
      }
    }

    // return unescaped version of str
    String json_unstr(String str) {
      String r = "";
      bool e = false;
      for(int i = 0; i < str.length(); i++) {
        if(e) {
          int j = esc_unrepl.indexOf(str[i]);
          if(j < 0) {
            Serial.println(String("Unhandled escape ")+str[i]+" in "+str);
            r += str[i];
          }
          r += esc_chars[j];
          e = false;
          continue;
        }
        if(str[i] == '\\') {
          e = true;
        } else {
          r += str[i];
        }
      }
      return r;
    }

  protected:
    bool (*cb)(void*);
    
    const String esc_chars =          "\b"   "\f"   "\n"   "\r"   "\t"   "\""    "\\";
    const String esc_unrepl =         "b"    "f"    "n"    "r"    "t"    "\""    "\\";
    const char* const esc_repl[7] = { "\\b", "\\f", "\\n", "\\r", "\\t", "\\\"", "\\\\" };

    CVar * next;        // linked list
    const char * name;  // json/storage name of variable
    unsigned int flags; // flag bitmask
#define C_FLAG_DIRTY  0x0001
#define C_FLAG_POOL   0x0002
#define C_FLAG_DOUBLE 0x0004
#define C_FLAG_INT    0x0008
#define C_FLAG_STRING 0x0010
    
    friend class CPool;
};

/*
 * 
 * VARIABLE CLASSES
 * 
 */

// DOUBLE
class CDouble : public CVar {
  public:
    CDouble(const char * name, double value) : CVar(name, C_FLAG_DOUBLE|C_FLAG_DIRTY), value(value) {}

    double operator = (double d) {
      if(value != d) {
        if(cb) { if(!cb(&d)) return value; }
        flags |= C_FLAG_DIRTY;
      }
      value = d;
      return d;
    }

    operator double () { return value; }

    virtual bool set_var(String & str) {
      char *ep = NULL;
      double d = strtod(str.c_str(), &ep);
      if(!ep || *ep != 0) {
        Serial.println("Error converting "+str+" to double!");
        return false;
      }
      if(((*this) = d) != d) return false;
      return true;
    }

    virtual void var_json(String & str) {
      json_str(str, String(value));
    }

    double * get_addr() { return &value; }

  private:
    double value;
};

// INT
class CInt : public CVar {
  public:
    CInt(const char * name, int value) : CVar(name, C_FLAG_INT|C_FLAG_DIRTY), value(value) {}

    int operator = (int i) {
      if(value != i) {
        if(cb) { if(!cb(&i)) return value; }
        flags |= C_FLAG_DIRTY;
      }
      value = i;
      return i;
    }

    operator int () { return value; }

    virtual bool set_var(String & str) {
      char *ep = NULL;
      long int i = strtol(str.c_str(), &ep, 10);
      if(!ep || *ep != 0) {
        Serial.println("Error converting "+str+" to int!");
        return false;
      }
      if(((*this) = i) != i) return false;
      return true;
    }

    virtual void var_json(String & str) {
      json_str(str, String(value));
    }

  private:
    int value;
};

// STRING
class CString : public CVar {
  public:
    CString(const char * name, const char * value) : CVar(name, C_FLAG_STRING|C_FLAG_DIRTY), value(value) {}

    String operator = (String s) {
      if(value != s) {
        if(cb) { if(!cb((void*)(s.c_str()))) return value; }
        flags |= C_FLAG_DIRTY;
      }
      value = s;
      return s;
    }

    operator String () { return value; }
    operator const char * () { return value.c_str(); }

    virtual bool set_var(String & str) {
      if(((*this) = str) != str) return false;
      return true;
    }

    virtual void var_json(String & str) {
      Serial.println(String("foo:")+value);
      str += "\"";
      json_str(str, value);
      str += "\"";
    }

  private:
    String value;
};

/*
 * 
 * AGREGATE VARIABLE POOL
 * 
 */

class CPool : public CVar {
  public:
    CPool(const char * name) : CVar(name, C_FLAG_POOL), head(NULL) {}

    CDouble F_CDouble(const char * name, double value) {
      CDouble n = CDouble(name, value);
      add(&n);
      return n;
    }

    CInt F_CInt(const char * name, int value) {
      CInt n = CInt(name, value);
      add(&n);
      return n;
    }

    CString F_CString(const char * name, const char * value) {
      CString n = CString(name, value);
      add(&n);
      return n;
    }

    CPool F_CPool(const char * name) {
      CPool n = CPool(name);
      add(&n);
      return n;
    }

    virtual void var_json(String & str) {
      str += "{\n";
      for(CVar * p = head; p; p = p->next) {
        Serial.print("fetch config: ");
        Serial.println(p->name);
        str += "\"";
        json_str(str, p->name);
        str += "\"\n:\n";
        p->var_json(str);
        str += "\n";
        if(p->next) str += ",\n";
      }
      str += "}\n";
      Serial.println(str);
    }

    CVar * get_var(String & name) {
      for(CVar * p = head; p; p = p->next) {
        if(name == p->name) return p;
      }
      return NULL;
    }

    bool get_dirty() {
      for(CVar * p = head; p; p = p->next) {
        if(p->flags & C_FLAG_POOL) {
          if(((CPool*)p)->get_dirty()) return true;
        } else
        if(p->flags & C_FLAG_DIRTY) {
          Serial.println(String("is dirty: ")+p->name);
          return true;
        }
      }
      return false;
    }

    void set_clean() {
      for(CVar * p = head; p; p = p->next) {
        if(p->flags & C_FLAG_POOL) {
          ((CPool*)p)->set_clean();
        } else {
          p->flags &= ~C_FLAG_DIRTY;
          Serial.print("set clean ");
          Serial.print(p->name);
          Serial.print(" : ");
          Serial.println(p->flags, HEX);
        }
      }      
    }

    // load from file - consume leading and trailing {}
    bool load(File & f) {
      int mode = 0;
      int quoted;
      CVar * v;
      
      while(true) {
        // read next string
        if(!f.available()) {
          Serial.println("eof");
          break;
        }
        String str = f.readStringUntil('\n');
        if(str == "") continue;
        // unqoute strings as required
        if(str.startsWith("\"") && str.endsWith("\"")) {
          str = json_unstr(str.substring(1, str.length()-1));
          quoted = 1;
        } else quoted = 0;
        Serial.println(String("Load string: ")+mode+" "+str);

        // process each phase
        // opening {
        if(mode == 0) {
          if(str == "{") {
            mode = 1;
            continue;
          }
          Serial.println(String("Invalid line in mode 0: ")+str);
          break;
        }

        // name
        if(mode == 1) {
          if(str == ",") continue; // skip commas
          if(str == "}") {
            Serial.println("done!");
            return true;
          }
          if(!quoted) {
            Serial.println(String("Name string not quoted: ")+str);
            break;
          }
          v = get_var(str);
          if(!v) {
            Serial.println(String("Name not found: ")+str);
            break;
          }
          mode = 2;
          continue;
        }

        // colon
        if(mode == 2) {
          if(str != ":") {
            Serial.println(String("Separator not found after: ")+v->name);
            break;
          }
          if(v->flags & C_FLAG_POOL) {
            Serial.println(String("Recurse into: ")+v->name);
            if(!((CPool*)v)->load(f)) {
              Serial.println(String("Recurse failed: ")+v->name);
              break;
            }
            Serial.println(String("Recurse complete: ")+v->name);
            mode = 1;
            continue;
          }
          mode = 3;
          continue;
        }

        // value
        if(mode == 3) {
          Serial.println(String("")+v->name+" from "+str);
          if(!v->set_var(str)) {
            Serial.print(String("Error setting ")+v->name+" from "+str);
            break;
          }
          mode = 1;
          continue;
        }
      }
      Serial.println(String("file load failed for ")+f.fullName());
      return false;
    }

    // full path or null to use bas object name
    bool load(String path) {
      Serial.println("Load from: "+path);
      File f = LittleFS.open(path, "r");
      if(f) {
        Serial.println("Opened primary path");
        int r = load(f);
        f.close();
        if(r) {
          set_clean();
          return true;
        }
        Serial.println("Error parsing primary file");
      }
      Serial.println("Primary file load failed: "+path);
      f = LittleFS.open(path+"~", "r");
      if(f) {
        Serial.println("Opened backup path");
        int r = load(f);
        f.close();
        if(r) {
          set_clean();
          Serial.println("Rename backup to primary");
          LittleFS.remove(path);
          LittleFS.rename(path+"~", path);
          return true;
        }
        Serial.println("Error parsing backup file");
      }
      Serial.println("Backup file load failed: "+path);
      return false;
    }
    bool load(void) { return load(name); }

    // full path or null to use base object name
    bool save(String path) {
      Serial.println("Save to: "+path);
      if(!get_dirty()) {
        Serial.println("Config still clean - do not save");
        return true;
      }
      String str = "";
      var_json(str);
      Serial.println("save: ");
      Serial.println(str);
      File f = LittleFS.open(path+"~", "w");
      if(!f) {
        Serial.println("Could not open save file for writing: "+path+"~");
        return false;
      }
      int i = f.print(str);
      f.close();
      Serial.println("Wrote bytes: "+i);
      if(LittleFS.exists(path)) {
        Serial.println("Remove old file");
        if(!LittleFS.remove(path)) {
          Serial.println("Remove failed, try rename anyway");
        }
      }
      Serial.println("Rename file to: "+path);
      if(!LittleFS.rename(path+"~", path)) {
        Serial.println("rename failed!");
        return false;
      }
      set_clean();
      return true;
    }
    bool save(void) { return save(name); }

  private:
    void add(CVar * n) {
      if(!head) {
        head = n;
        return;
      }
      CVar * p = head;
      for(;;) {
        if(p->name == n->name) {
          Serial.print("Name ");
          Serial.print(n->name);
          Serial.println(" REPEATED!");
          while(1) {}
        }
        if(!p->next) break;
        p = p->next;
      }
      p->next = n;
    }

    CVar * head;
};

/* adc sampler */

/*
 * Published status variables
 */

STATUS(CDouble, temp_roof, 15.0);
STATUS(CDouble, temp_room, 15.0);
STATUS(CInt, temp_mode, 0);
STATUS(CInt, temp_try, 0);
STATUS(CInt, temp_out, 0);

/*
 * Published config variables
 * 
 * plus
 * 
 * Specific watch callbacks for config variables
 * 
 */

CONFIG(CInt, temp_cfg_force_off, 0);

CONFIG(CDouble, temp_cfg_heat_start, 20.0);
CONFIG(CDouble, temp_cfg_heat_end, 22.0);

bool temp_cfg_heat_start_watch(void * dp) {
  double d = *(double*)dp;
  if(d >= temp_cfg_heat_end) { return false; }
  return true;
}

bool temp_cfg_heat_end_watch(void * dp) {
  double d = *(double*)dp;
  if(d <= temp_cfg_heat_start) { return false; }
  return true;
}

CONFIG(CDouble, temp_cfg_cool_start, 23.0);
CONFIG(CDouble, temp_cfg_cool_end, 21.0);

bool temp_cfg_cool_start_watch(void * dp) {
  double d = *(double*)dp;
  if(d <= temp_cfg_cool_end) { return false; }
  return true;
}

bool temp_cfg_cool_end_watch(void * dp) {
  double d = *(double*)dp;
  if(d >= temp_cfg_cool_start) { return false; }
  return true;
}

CONFIG(CDouble, temp_cfg_hist_on_mn, 0.5);
unsigned long temp_cfg_hist_on_ms = 30000UL;

bool temp_cfg_hist_on_mn_watch(void * dp) {
  double d = *(double*)dp;
  Serial.println(String("temp_cfg_hist_on_mn_watch: ")+d);
  if(d < 0) { return false; }
  temp_cfg_hist_on_ms = (unsigned long)(d * 60000.0);
  Serial.println(String("temp_cfg_hist_on_mn_watch: ")+temp_cfg_hist_on_ms);
  return true;
}

CONFIG(CDouble, temp_cfg_hist_off_mn, 0.5);
unsigned long temp_cfg_hist_off_ms = 30000UL;

bool temp_cfg_hist_off_mn_watch(void * dp) {
  double d = *(double*)dp;
  Serial.println(String("temp_cfg_hist_off_mn: ")+d);
  if(d < 0) { return false; }
  temp_cfg_hist_off_ms = (unsigned long)(d * 60000.0);
  Serial.println(String("temp_cfg_hist_off_mn: ")+temp_cfg_hist_off_ms);
  return true;
}

CONFIG(CDouble, temp_cfg_fan_on_delta, 2.0);
CONFIG(CDouble, temp_cfg_fan_off_delta, 1.0);

bool temp_cfg_fan_off_delta_watch(void * dp) {
  double d = *(double*)dp;
  if(d >= temp_cfg_fan_on_delta) { return false; }
  return true;
}

CONFIG(CDouble, temp_cfg_cal_r, 23000.0);
CONFIG(CDouble, temp_cfg_cal_rt, 20.0);
CONFIG(CDouble, temp_cfg_cal_b, 3700.0);


void temp_setup_watch() {
  temp_cfg_heat_end.watch(temp_cfg_heat_end_watch);
  temp_cfg_heat_start.watch(temp_cfg_heat_start_watch);
  temp_cfg_cool_end.watch(temp_cfg_cool_end_watch);
  temp_cfg_cool_start.watch(temp_cfg_cool_start_watch);
  temp_cfg_hist_on_mn.watch(temp_cfg_hist_on_mn_watch);
  temp_cfg_hist_off_mn.watch(temp_cfg_hist_off_mn_watch);
  temp_cfg_fan_off_delta.watch(temp_cfg_fan_off_delta_watch);
  // call hist watchers initially to set ms values
  temp_cfg_hist_on_mn_watch(temp_cfg_hist_on_mn.get_addr());
  temp_cfg_hist_off_mn_watch(temp_cfg_hist_off_mn.get_addr());
}

// grab as many samples as we can to smooth it a bit without breaking wifi

#define ADC_SAMPLES 200
#define ADC_CALIB 0.98

double getsample() {
  double d = 0;
  for(int i = 0; i < ADC_SAMPLES; i++) {
    d += analogRead(A0);
  }
  d /= ADC_SAMPLES;
  d /= 1023.0; // 10 bits to 1V
  d *= ADC_CALIB;
  return d;
}

/* get resistance */
#define RVCC 3.27
#define RR1 219000.0
#define RR2 100000.0

double getr() {
  double v = getsample();
  double v_r1 = RVCC - v;
  double i_r1 = v_r1 / RR1;
  double i_r2 = v / RR2;
  double i_r = i_r1 - i_r2;
  return v / i_r;
}

/* get temperature */

// Kelvin offset
#define TEMP_K 273.15

double gettemp() {
  double r = getr();
  double temp_ref = TEMP_K + temp_cfg_cal_rt;
  //Serial.println(r);
  // calculate temp from r
  double c = (temp_cfg_cal_b * temp_ref);
  c /= temp_cfg_cal_b + (temp_ref * log(r / temp_cfg_cal_r));
  c -= TEMP_K;
  return c;
}

/*
// cal temp
#define TEMP_T 20
// Thermistor R@TEMP_T
#define TEMP_R 23000.0
// Thermistor B val
#define TEMP_B 3700.0
// Kelvin offset
#define TEMP_K 273.15
// 25C in Kelvin
#define TEMP_REF (TEMP_K + TEMP_T)

double gettemp() {
  double r = getr();
  //Serial.println(r);
  // calculate temp from r
  double c = (TEMP_B * TEMP_REF);
  c /= TEMP_B + (TEMP_REF * log(r / TEMP_R));
  c -= TEMP_K;
  return c;
}
*/

// GPIO 16 = D0
#define SEL_ROOF 16
// GPIO 14 = D5
#define SEL_ROOM 14
// GPIO 12 = D6
#define TEMP_OUT 12

int temp_phase;

void temp_sel_roof() {
  pinMode(SEL_ROOM, INPUT);
  pinMode(SEL_ROOF, OUTPUT);
  digitalWrite(SEL_ROOF, LOW);
  temp_phase = SEL_ROOF;
}

void temp_sel_room() {
  pinMode(SEL_ROOF, INPUT);
  pinMode(SEL_ROOM, OUTPUT);
  digitalWrite(SEL_ROOM, LOW);
  temp_phase = SEL_ROOM;
}

unsigned long tempms;
unsigned long temp_time;

void temp_set_out(int o) {
  temp_out = o;
  if(o) {
    digitalWrite(TEMP_OUT, HIGH);
  } else {
    digitalWrite(TEMP_OUT, LOW);
  }
  temp_time = millis();
}

void temp_try_out(int o) {
  temp_try = o;
  if(o == temp_out) {
    // no change required? reset timer
    temp_time = millis();
    return;
  }
  if((millis() - temp_time) > (o ? temp_cfg_hist_on_ms : temp_cfg_hist_off_ms)) {
    // not reset within hysteris window? make it permanent
    temp_set_out(o);
  }
}

void temp_setup() {
  // set up output and hysteresis
  pinMode(TEMP_OUT, OUTPUT);
  // set up temperatures and measurement state
  tempms = millis();
  temp_sel_roof();
  temp_set_out(0);
  temp_setup_watch();
}

#define TEMP_INTERVAL 200UL

void temp_run() {
  if((millis() - tempms) < TEMP_INTERVAL) return;
  tempms += TEMP_INTERVAL;

  if(temp_cfg_force_off) {
    temp_set_out(0);
    temp_mode = 999;
    return;
  }
  if(temp_phase == SEL_ROOF) {
    temp_roof = gettemp();
    temp_sel_room();
  } else {
    temp_room = gettemp();
    temp_sel_roof();
  }

  // update global mode
  if(temp_mode > 0) {
    // heating
    if(temp_room > temp_cfg_heat_end) {
      temp_mode = 0;
    }
  } else if(temp_mode < 0) {
    // cooling
    if(temp_room < temp_cfg_cool_end) {
      temp_mode = 0;
    }
  } else {
    // idle
    if(temp_room < temp_cfg_heat_start) {
      temp_mode = 1;
    } else if(temp_room > temp_cfg_cool_start) {
      temp_mode = -1;
    }
  }

  // update temp output
  if(temp_mode > 0) {
    // heating
    if(temp_out) {
      // evaulate whether to turn off
      temp_try_out(temp_roof < (temp_room + temp_cfg_fan_off_delta) ? 0 : 1);
    } else {
      // evaulate whether to turn on
      temp_try_out(temp_roof > (temp_room + temp_cfg_fan_on_delta) ? 1 : 0);
    }
  } else if(temp_mode < 0) {
    // cooling
    if(temp_out) {
      // evaulate whether to turn off
      temp_try_out(temp_roof > (temp_room - temp_cfg_fan_off_delta) ? 0 : 1);
    } else {
      // evaulate whether to turn on
      temp_try_out(temp_roof < (temp_room - temp_cfg_fan_on_delta) ? 1 : 0);
    }
  } else {
    // idle
    temp_try_out(0);
  }

#if 0
  Serial.print(temp_roof);
  Serial.print(" ");
  Serial.print(temp_room);
  Serial.print(" ");
  Serial.print(temp_out);
  Serial.println();
#endif
}

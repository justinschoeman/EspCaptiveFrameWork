'use strict';

// set to local server to test with local web server
console.log('url: ',document.location)
var _dev_path = ''
if(document.location.href.includes('localhost')) {
    _dev_path = 'http://fan.local/'
}

function get_json(path, cb, ecb = null) {
    console.log('get_json: ', _dev_path + path)
    fetch(_dev_path + path).then((r) => {
        if(r.ok) { return r.json() }
        console.error('error fetching status: ', r)
        throw('error fetching status ('+r.status+'): ' + r.statusText)
    }).then((data) => {
        //console.log('data: ', data)
        if(cb) { cb(data) }
    }).catch((e) => {
        console.error('error: ', e)
        if(ecb) { ecb(e) }
    })
}

function status_cb(json) {
    console.log('status json: ', json)
    Object.keys(json).forEach((n) => {
        let e = document.getElementById('status,'+n)
        if(e) { e.value = json[n] }
    })
    setTimeout(get_status, 1000)
}

function status_ecb(e) {
    console.log('status error: ', e)
    alert("GET STATUS: "+e)
    setTimeout(get_status, 10000)
}

function get_status() {
    get_json('status', status_cb, status_ecb)
}
window.get_status = get_status

function post_var(name, value, cb = null, ecb = null) {
    console.log('post_var ('+name+'): ', value)
    let form = new FormData()
    form.set(name, value)
    fetch(_dev_path + 'post', {method:'POST', body:form}).then((r) => {
        if(r.ok) {
            if(cb) { cb(name) }
        } else {
            console.error('error setting variable: ', r)
            throw('error setting variable ('+r.status+'): ' + r.statusText)
        }
    }).catch((e) => {
        console.error('post error: ', e)
        if(ecb) { ecb(name, e) }
    })
}


function post_config_cbe(n, e) {
    console.log('post_config_cbe ('+n+'): ', e)
    let r = document.getElementById('result,'+n)
    if(!r) {
        console.error('BAD FORM CBEB: ', n)
        alert("BAD FORM CBE!")
        return
    }
    r.innerHTML = 'FAILED ('+e+')'
    setTimeout(get_config, 0)
}

function post_config_cb(n) {
    console.log('post_config_cb: ', n)
    let r = document.getElementById('result,'+n)
    if(!r) {
        console.error('BAD FORM CB: ', n)
        alert("BAD FORM CB!")
        return
    }
    r.innerHTML = 'OK'
    setTimeout(get_config, 0)
}

function post_config(n) {
    console.log('post_config: ', n)
    let i = document.getElementById('input,'+n)
    let r = document.getElementById('result,'+n)
    if(!i || !r) {
        console.error('BAD FORM: ', n)
        alert("BAD FORM POST!")
        return
    }
    r.innerHTML = 'saving...'
    post_var(n, i.value, post_config_cb, post_config_cbe)
}
window.post_config = post_config

const config_data = {
    'temp_cfg_force_off':{'title':'Force Fan OFF', 'text':'0/1 - when set to non-zero value, fan is disabled.'},
    'cfg_mdns_name':{'title':'mDNS Name', 'text':'Only A-Z,a-z,0-9 or -, max 255 characters. Reboot to apply change.'},
    'temp_cfg_heat_start':{'title':'Start heating below (°C)', 'text':'Switch to heating mode when the temperature drops below this setting.'},
    'temp_cfg_heat_end':{'title':'Stop heating above (°C)', 'text':'End heating mode when temperature rises above this. Must be higher than the start temperature.'},
    'temp_cfg_cool_start':{'title':'Start cooling above (°C)', 'text':'Switch to cooling mode when the temperature rises above this setting.'},
    'temp_cfg_cool_end':{'title':'Stop cooling below (°C)', 'text':'End cooling mode when temperature drops below this. Must be lower than the start temperature.'},
    'temp_cfg_hist_on_mn':{'title':'Turn on hysteresis time (minutes)', 'text':'On condition must be continuously valid for this time before turning on. Must be >0.'},
    'temp_cfg_hist_off_mn':{'title':'Turn off hysteresis time (minutes)', 'text':'Off condition must be continuously valid for this time before turning off. Must be >0'},
    'temp_cfg_fan_on_delta':{'title':'Turn on temperature difference (°C)', 'text':'Roof temperature must differ by at least this much before turning fan on.'},
    'temp_cfg_fan_off_delta':{'title':'Turn off temperature difference (°C)', 'text':'Turn off when roof temperature difference drops below this. Must be below the on difference.'},
    'temp_cfg_cal_r':{'title':'Thermistor calibration resistance (Ohm).'},
    'temp_cfg_cal_rt':{'title':'Thermistor calibration resistance temperature (°C).'},
    'temp_cfg_cal_b':{'title':'Thermistor calibration Beta.'},
    //'':{'title':'', 'text':''},
}

function config_cb(json) {
    console.log('config json: ', json)
    let p = document.getElementById('p_edit')
    if(!p) {
        alert('MISSING p_edit ELEMENT!')
        return
    }
    // generate new forms when empty
    if(p.innerHTML == '') {
        // generate new forms
        Object.keys(json).forEach((n) => {
            let title=n
            let text=''
            if(n in config_data) {
                let cfg = config_data[n]
                if('title' in cfg) { title = cfg.title }
                if('text' in cfg) { text = '<br/><small>'+cfg.text+'<br/></small>' }
            }
            p.innerHTML += '<span id="span,'+n+'">'+title+' : <input id="input,'+n+'" /> <button onclick="post_config(\''+n+'\')">SAVE</button> <span id="result,'+n+'"></span>'+text+'</span><br/>'
        })
    }
    // populate input values
    Object.keys(json).forEach((n) => {
        let e = document.getElementById('input,'+n)
        if(e) { e.value = json[n] }
    })
}

function config_ecb(e) {
    console.log('config error: ', e)
    alert("GET CONFIG: "+e)
}

function get_config() {
    get_json('config', config_cb, config_ecb)
}
window.get_config = get_config


const file_data = {
    'firmware.bin': {'title':'Firmware', 'post':'update_firmware'},
    'www.tar': {'title':'Web application', 'post':'update_www'},
}

function files_cb(json) {
    console.log('files json: ', json)
    let p = document.getElementById('p_files')
    if(!p) {
        alert('MISSING p_files ELEMENT!')
        return
    }
    // generate new forms
    Object.keys(json).forEach((n) => {
        if(n in file_data) {
            let file = file_data[n]
            p.innerHTML += file.title+' (MD5: '+json[n]+') <button onclick="window.location=\''+file.post+'\'">INSTALL</button><br/>'
        } else {
            p.innerHTML += 'WARNING: Unhandled uploaded file "'+n+'"!<br/>'
        }
    })
    // populate input values
    Object.keys(json).forEach((n) => {
        let e = document.getElementById('input,'+n)
        if(e) { e.value = json[n] }
    })
}

function files_ecb(e) {
    console.log('files error: ', e)
    alert("GET FILES: "+e)
}

function get_files() {
    get_json('files', files_cb, files_ecb)
}
window.get_files = get_files

/*
 * HOME PAGE CONFIG BUTTONS
 */
function home_config_cb(json) {
    console.log('home config json: ', json)
    let p = document.getElementById('p_cont')
    if(!p) {
        alert('MISSING p_cont ELEMENT!')
        return
    }
    // regenerate each time
    p.innerHTML = '';
    if('temp_cfg_force_off' in json) {
        if(json.temp_cfg_force_off) {
            p.innerHTML += '<button onclick="post_onoff(0)">Turn ON</button><br/>'
        } else {
            p.innerHTML += '<button onclick="post_onoff(1)">Turn OFF</button><br/>'
        }
    }
}

function home_config_ecb(e) {
    console.log('home config error: ', e)
    alert("GET HOME CONFIG: "+e)
}

function get_home_config() {
    get_json('config', home_config_cb, home_config_ecb)
}
window.get_home_config = get_home_config

function post_onoff_ok(n) {
    get_home_config()
}

function post_onoff_fail(n, e) {
    alert("Error setting on/off: "+e)
    get_home_config()
}

function post_onoff(val) {
    post_var('temp_cfg_force_off', val, post_onoff_ok, post_onoff_fail)
}
window.post_onoff = post_onoff

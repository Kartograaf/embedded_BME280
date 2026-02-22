# ================================
# BME280 Monitor + Logger
# ================================

# ---- configuration ----
var SENSOR_NAME = "BME28"

var TEMP_LIMIT = 30.0      # °C alert threshold
var HUM_LIMIT  = 80.0      # %
var PRESS_LIMIT_LOW = 980  # hPa

var AVG_WINDOW = 5
var MAX_LOGS   = 100

# ---- runtime buffers ----
var temp_buf = []
var hum_buf  = []
var pres_buf = []

var last_log_hash = 0
var last_log_time = 0

# minimum seconds between flash writes
var LOG_COOLDOWN = 300      # 5 minutes

# persistent flash storage key
var LOG_KEY = "bme280_logs"

# ------------------------
# helper: moving average
# ------------------------
def moving_avg(buf, value)
    buf.push(value)
    if buf.size() > AVG_WINDOW
        buf.remove(0)
    end

    var sum = 0.0
    for v : buf
        sum += v
    end
    return sum / buf.size()
end

# ------------------------
# flash log handling
# ------------------------
def load_logs()
    var logs = tasmota.kv_get(LOG_KEY)
    if logs == nil
        return []
    end
    return logs
end

def save_logs(logs)
    # rotate
    while logs.size() > MAX_LOGS
        logs.remove(0)
    end
    tasmota.kv_set(LOG_KEY, logs)
end

def add_log(msg)
    var now = tasmota.time()

    # cooldown protection
    if (now - last_log_time) < LOG_COOLDOWN
        print("Log skipped (cooldown)")
        return
    end

    var hash = tasmota.hash(msg)

    # prevent duplicate writes
    if hash == last_log_hash
        print("Duplicate log ignored")
        return
    end

    var logs = load_logs()

    var entry = {
        "time": now,
        "msg": msg
    }

    logs.push(entry)

    save_logs(logs)

    last_log_hash = hash
    last_log_time = now

    print("LOG SAVED:", msg)
end

# ------------------------
# alert evaluation
# ------------------------
def check_alerts(t, h, p)

    var triggered = false
    var msg = ""

    if t > TEMP_LIMIT
        msg += format("TEMP ALERT %.1f°C ", t)
        triggered = true
    end

    if h > HUM_LIMIT
        msg += format("HUM ALERT %.1f%% ", h)
        triggered = true
    end

    if p < PRESS_LIMIT_LOW
        msg += format("LOW PRESS %.1f hPa ", p)
        triggered = true
    end

    if triggered
        add_log(msg)
    end
end

# ------------------------
# telemetry handler
# ------------------------
def sensor_handler(event)
    
    if !event.contains(SENSOR_NAME)
        return
    end

    var data = event[SENSOR_NAME]

    var temp = data["Temperature"]
    var hum  = data["Humidity"]
    var pres = data["Pressure"]

    # moving averages
    var t_avg = moving_avg(temp_buf, temp)
    var h_avg = moving_avg(hum_buf, hum)
    var p_avg = moving_avg(pres_buf, pres)

    print(
        format(
            "AVG -> T:%.2f H:%.2f P:%.2f",
            t_avg, h_avg, p_avg
        )
    )

    # evaluate alerts on averaged data
    check_alerts(t_avg, h_avg, p_avg)
end

# ------------------------
# register telemetry rule
# ------------------------

tasmota.set_timer(2000, def()
    print("Adding rule..")
    tasmota.add_rule("tele", sensor_handler)
end)

print("BME28 monitor started")
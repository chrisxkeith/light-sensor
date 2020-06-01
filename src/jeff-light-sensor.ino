
const String githubHash = "to be replaced manually (and code re-flashed) after 'git push'";

#include <limits.h>

class JSonizer {
  public:
    static void addFirstSetting(String& json, String key, String val);
    static void addSetting(String& json, String key, String val);
    static String toString(bool b);
};

class Utils {
  public:
    static const int publishRateInSeconds;
    static bool publishDelay;
    static int setInt(String command, int& i, int lower, int upper);
    static void publish(String event, String data);
    static void publishJson();
    static String getName();
};

class TimeSupport {
  private:
    unsigned long ONE_DAY_IN_MILLISECONDS;
    unsigned long lastSyncMillis;
    String timeZoneString;
    String getSettings();
  public:
    int timeZoneOffset;
    TimeSupport(int timeZoneOffset, String timeZoneString);
    String timeStrZ(time_t t);
    String nowZ();
    void handleTime();
    int setTimeZoneOffset(String command);
    void publishJson();
};

void JSonizer::addFirstSetting(String& json, String key, String val) {
    json.concat("\"");
    json.concat(key);
    json.concat("\":\"");
    json.concat(val);
    json.concat("\"");
}

void JSonizer::addSetting(String& json, String key, String val) {
    json.concat(",");
    addFirstSetting(json, key, val);
}

String JSonizer::toString(bool b) {
    if (b) {
        return "true";
    }
    return "false";
}

const int Utils::publishRateInSeconds = 5;
bool Utils::publishDelay = true;
int Utils::setInt(String command, int& i, int lower, int upper) {
    int tempMin = command.toInt();
    if (tempMin > lower && tempMin < upper) {
        i = tempMin;
        return 1;
    }
    return -1;
}
void Utils::publish(String event, String data) {
    Particle.publish(event, data, 1, PRIVATE);
    if (publishDelay) {
      delay(1000);
    }
}
void Utils::publishJson() {
    String json("{");
    JSonizer::addFirstSetting(json, "githubHash", githubHash);
    JSonizer::addSetting(json, "githubRepo", "https://github.com/chrisxkeith/light-sensor");
    json.concat("}");
    publish("Utils json", json);
}

String TimeSupport::getSettings() {
    String json("{");
    JSonizer::addFirstSetting(json, "lastSyncMillis", String(lastSyncMillis));
    JSonizer::addSetting(json, "timeZoneOffset", String(timeZoneOffset));
    JSonizer::addSetting(json, "timeZoneString", String(timeZoneString));
    JSonizer::addSetting(json, "internalTime", nowZ());
    json.concat("}");
    return json;
}

TimeSupport::TimeSupport(int timeZoneOffset, String timeZoneString) {
    this->ONE_DAY_IN_MILLISECONDS = 24 * 60 * 60 * 1000;
    this->timeZoneOffset = timeZoneOffset;
    this->timeZoneString = timeZoneString;
    Time.zone(timeZoneOffset);
    Particle.syncTime();
    this->lastSyncMillis = millis();
}

String TimeSupport::timeStrZ(time_t t) {
    String fmt("%a %b %d %H:%M:%S ");
    fmt.concat(timeZoneString);
    fmt.concat(" %Y");
    return Time.format(t, fmt);
}

String TimeSupport::nowZ() {
    return timeStrZ(Time.now());
}

void TimeSupport::handleTime() {
    if (millis() - lastSyncMillis > ONE_DAY_IN_MILLISECONDS) {    // If it's been a day since last sync...
                                                            // Request time synchronization from the Particle Cloud
        Particle.syncTime();
        lastSyncMillis = millis();
    }
}

int TimeSupport::setTimeZoneOffset(String command) {
    timeZoneString = "???";
    return Utils::setInt(command, timeZoneOffset, -24, 24);
}

void TimeSupport::publishJson() {
    Utils::publish("TimeSupport", getSettings());
}
TimeSupport    timeSupport(-8, "PST");

class Sensor {
  private:
    int     pin;
    String  name;
    int     nSamples;
    double  total;

    int getValue() {
        return round(total / nSamples);
    }

  public:
    Sensor(int pin, String name) {
      this->pin = pin;
      this->name = name;
      clear();
      pinMode(pin, INPUT);
    }
    
    void sample() {
      if (pin >= A0 && pin <= A5) {
          total += analogRead(pin);
      } else {
          total += digitalRead(pin);
      }
      nSamples++;
    }
    
    void clear() {
      nSamples = 0;
      total = 0.0;
    }

    int publishState() {
      String json("{");
      JSonizer::addFirstSetting(json, "name", name);
      JSonizer::addSetting(json, "nSamples", String(nSamples));
      JSonizer::addSetting(json, "total", String(total));
      json.concat("}");
      Utils::publish("Message", json);
      return 1;
    }

    void publishData() {
      Utils::publish(name, String(getValue()));
    }
};

const String cks = "1f0027001347363336383437";
const String jeffs = "2a0026000947363335343832";

Sensor lightSensor1(A0, (System.deviceID().equals(cks)) ? "Light sensor" : "Jeff Light sensor");
Sensor lightSensor2(A1, "Light sensor (10K)");

// getSettings() is already defined somewhere.
int pubSettings(String command) {
    if (command.compareTo("") == 0) {
        Utils::publishJson();
    } else if (command.compareTo("time") == 0) {
        timeSupport.publishJson();
    } else {
        Utils::publish("GetSettings bad input", command);
    }
    return 1;
}

int pubData(String command) {
  lightSensor1.publishState();
  lightSensor1.publishData();
  if (System.deviceID().equals(cks)) {
    lightSensor2.publishState();
    lightSensor2.publishData();
  }
  return 1;
}

void sample() {
  lightSensor1.sample();
  if (System.deviceID().equals(cks)) {
    lightSensor2.sample();
  }
}

void clear() {
  lightSensor1.clear();
  if (System.deviceID().equals(cks)) {
    lightSensor2.clear();
  }
}

int lastHour = -1;
void setup() {
  Utils::publish("Message", "Started setup...");
  Particle.function("getSettings", pubSettings);
  Particle.function("getData", pubData);
  sample();
  pubData("");
  clear();
  Utils::publish("Message", "Finished setup...");
}

void loop() {
  timeSupport.handleTime();
  sample();
  if (Time.minute() == 0 && lastHour != Time.hour()) {
    pubData("");
    clear();
    lastHour = Time.hour();
  }
}

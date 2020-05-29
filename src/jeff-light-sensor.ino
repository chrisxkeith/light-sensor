
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
String Utils::getName() {
  String location = "Unknown";
  String id = System.deviceID();
  if (id.equals("1f0027001347363336383437")) {
    location = "Light sensor";
  } else if (id.equals("2a0026000947363335343832")) {
    location = "Jeff Light sensor";
  }
  return location;
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

class UptimeMonitor {
  private:
    long lastHeartBeat = 0;
    const int HEARTBEAT_INTERVAL_IN_MINUTES = 10;

    void getMinutes() {
      String len = "Duration in minutes from EEPROM : ";
      long minutes;
      EEPROM.get(0, minutes);
      len.concat(minutes);
      Utils::publish("Message", len);
    }

  public:
    void setupEEPROM() {
      String len("EEPROM.length() : ");
      len.concat(EEPROM.length());
      Utils::publish("Message", len);
      getMinutes();
    }

    void writeEEPROM() {
      if (millis() - lastHeartBeat > 1000 * 60 * HEARTBEAT_INTERVAL_IN_MINUTES) {
        long minutes = millis() / 1000 / 60;
        EEPROM.put(0, minutes);
        lastHeartBeat = millis();
      }
    }
};
UptimeMonitor uptimeMonitor;

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

void publishVal(int value) {
  String s(value);
  Utils::publish(Utils::getName(), s);
}

int nSamples = 0;
double total = 0.0;
int publishData(String command) {
  String msg("nSamples = ");
  msg.concat(nSamples);
  msg.concat("; total = ");
  msg.concat(total);
  msg.concat(";");
  Utils::publish("Message", msg);

  int value = (int)round(total / nSamples);
  publishVal(value);
  nSamples = 0;
  total = 0.0;
  return 1;
}

int lastHour = -1;
void publishWithMessage() {
  publishData("");
  lastHour = Time.hour();
  Utils::publish("Message", "Next publish at the top of the hour.");
}

void setup() {
  Utils::publish("Message", "Started setup...");
  Particle.function("publishData", publishData);
  Particle.function("getSettings", pubSettings);

  pinMode(A0, INPUT);
  publishVal(analogRead(A0));
  lastHour = Time.hour();
  Utils::publish("Message", "Next publish at the top of the hour.");
//  uptimeMonitor.setupEEPROM();
  Utils::publish("Message", "Finished setup...");
}

void loop() {
  timeSupport.handleTime();
  total += analogRead(A0);
  nSamples++;
  if (Time.minute() == 0 && lastHour != Time.hour()) {
    publishWithMessage();
  }
//  uptimeMonitor.writeEEPROM();
}
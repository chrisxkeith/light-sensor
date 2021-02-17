const String githubHash = "to be replaced manually (and code re-flashed) after 'git push'";

#include <limits.h>

class JSonizer {
  public:
    static void addFirstSetting(String& json, String key, String val);
    static void addSetting(String& json, String key, String val);
    static String toString(bool b);
};

int publishRateInSeconds = 2;
class Utils {
  public:
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
system_tick_t lastPublishInSeconds = 0;
unsigned int lastDisplayInSeconds = 0;
unsigned int displayIntervalInSeconds = 2;

void Utils::publishJson() {
    String json("{");
    JSonizer::addFirstSetting(json, "githubHash", githubHash);
    JSonizer::addSetting(json, "githubRepo", "https://github.com/chrisxkeith/light-sensor");
    JSonizer::addSetting(json, "lastPublishInSeconds ", String(lastPublishInSeconds));
    JSonizer::addSetting(json, "publishRateInSeconds", String(publishRateInSeconds));
    JSonizer::addSetting(json, "lastDisplayInSeconds", String(lastDisplayInSeconds));
    JSonizer::addSetting(json, "displayIntervalInSeconds", String(displayIntervalInSeconds));
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

    int getValue() {
        return round(total / nSamples);
    }
};

#include <SparkFunMicroOLED.h>
#include <math.h>

class OLEDWrapper {
  public:
    MicroOLED* oled = new MicroOLED();

    OLEDWrapper() {
        oled->begin();    // Initialize the OLED
        oled->clear(ALL); // Clear the display's internal memory
        oled->display();  // Display what's in the buffer (splashscreen)
        delay(1000);     // Delay 1000 ms
        oled->clear(PAGE); // Clear the buffer.
    }

    void display(String title, int font, uint8_t x, uint8_t y) {
        oled->clear(PAGE);
        oled->setFontType(font);
        oled->setCursor(x, y);
        oled->print(title);
        oled->display();
    }

    void display(String title, int font) {
        display(title, font, 0, 0);
    }

    void invert(bool invert) {
      oled->invert(invert);
    }

    void displayNumber(String s) {
        // To reduce OLED burn-in, shift the digits (if possible) on the odd minutes.
        int x = 0;
        if (Time.minute() % 2) {
            const int MAX_DIGITS = 5;
            if (s.length() < MAX_DIGITS) {
                const int FONT_WIDTH = 12;
                x += FONT_WIDTH * (MAX_DIGITS - s.length());
            }
        }
        display(s, 3, x, 0);
    }

    bool errShown = false;
    void verify(int xStart, int yStart, int xi, int yi) {
      if (!errShown && (xi >= oled->getLCDWidth() || yi >= oled->getLCDHeight())) {
        String json("{");
        JSonizer::addSetting(json, "xStart", String(xStart));
        JSonizer::addSetting(json, "yStart", String(yStart));
        JSonizer::addSetting(json, "xi", String(xi));
        JSonizer::addSetting(json, "yi", String(yi));
        json.concat("}");
        Utils::publish("super-pixel coordinates out of range", json);
        errShown = true;
      }
    }

   void superPixel(int xStart, int yStart, int xSuperPixelSize, int ySuperPixelSize, int pixelVal,
         int left, int right, int top, int bottom) {
     int pixelSize = xSuperPixelSize * ySuperPixelSize;
     if (pixelVal < 0) {
       pixelVal = 0;
     } else if (pixelVal >= pixelSize) {
       pixelVal = pixelSize - 1;
     }
     for (int xi = xStart; xi < xStart + xSuperPixelSize; xi++) {
       for (int yi = yStart; yi < yStart + ySuperPixelSize; yi++) {
         verify(xStart, yStart, xi, yi);

         // Value between 1 and pixelSize - 2,
         // so pixelVal of 0 will have all pixels off
         // and pixelVal of pixelSize - 1 will have all pixels on. 
         int r = (rand() % (pixelSize - 2)) + 1;
         if (r < pixelVal) { // lower value maps to white pixel.
           oled->pixel(xi, yi);
         }
       }
     }
   }

    void publishJson() {
        String json("{");
        JSonizer::addFirstSetting(json, "getLCDWidth()", String(oled->getLCDWidth()));
        JSonizer::addSetting(json, "getLCDHeight()", String(oled->getLCDHeight()));
        json.concat("}");
        Utils::publish("OLED", json);
    }

    void testPattern() {
      int xSuperPixelSize = 6;	
      int ySuperPixelSize = 6;
      int pixelSize = xSuperPixelSize * ySuperPixelSize; 
      float diagonalDistance = sqrt((float)(xSuperPixelSize * xSuperPixelSize + ySuperPixelSize * ySuperPixelSize));
      float factor = (float)pixelSize / diagonalDistance;
      int pixelVals[64];
      for (int i = 0; i < 64; i++) {
        int x = (i % 8);
        int y = (i / 8);
        pixelVals[i] = (int)(round(sqrt((float)(x * x + y * y)) * factor));
      }
      displayArray(xSuperPixelSize, ySuperPixelSize, pixelVals);
      delay(5000);
      displayArray(xSuperPixelSize, ySuperPixelSize, pixelVals);
      delay(5000);	
    }

    void displayArray(int xSuperPixelSize, int ySuperPixelSize, int pixelVals[]) {
      oled->clear(PAGE);
      for (int i = 0; i < 64; i++) {
        int x = (i % 8) * xSuperPixelSize;
        int y = (i / 8) * ySuperPixelSize;
        int left = x;
        int right = x;
        int top = y;
        int bottom = y;
        if (x > 0) {
          left = pixelVals[i - 1];
        }
        if (x < 7) {
          right = pixelVals[i + 1];
        }
        if (y > 0) {
          top = pixelVals[i - 8];
        }
        if (y < 7) {
          top = pixelVals[i + 8];
        }
        // This (admittedly confusing) switcheroo of x and y axes is to make the orientation
        // of the sensor (with logo reading correctly) match the orientation of the OLED.
        superPixel(y, x, ySuperPixelSize, xSuperPixelSize, pixelVals[i],
            left, right, top, bottom);
      }
      oled->display();
    }

    void clear() {
      oled->clear(ALL);
    }
};

OLEDWrapper oledWrapper;

Sensor lightSensor1(A0,  "Light sensor");

class Spinner {
  private:
    int middleX = oledWrapper.oled->getLCDWidth() / 2;
    int middleY = oledWrapper.oled->getLCDHeight() / 2;
    int xEnd, yEnd;
    int lineWidth = min(middleX, middleY);
    int color;
    int deg;

  public:
    Spinner() {
      init();
    }

    bool doInvert = true;
    void display() {
      xEnd = lineWidth * cos(deg * M_PI / 180.0);
      yEnd = lineWidth * sin(deg * M_PI / 180.0);

      oledWrapper.oled->line(middleX, middleY, middleX + xEnd, middleY + yEnd, color, NORM);
      oledWrapper.oled->display();
      deg++;
      if (deg >= 360) {
        deg = 0;
        if (color == WHITE) {
          color = BLACK;
        } else {
          color = WHITE;
        }
      }
    }

    void init() {
      color = WHITE;
      deg = 0;
    }
};
Spinner spinner;

void display_digits(unsigned int num) {
  oledWrapper.displayNumber(String(num));
}

const int THRESHOLD = 350;
bool on = false;
int previousValue = 0;

void display_on_oled() {
  int value = lightSensor1.getValue();
  if ((value > THRESHOLD) != on) {
    String d("on: ");
    d.concat(String(on));
    d.concat(", ");
    d.concat(previousValue);
    d.concat("->");
    d.concat(value);
    Utils::publish("Diagnostic", d);
    on = !on;
    oledWrapper.clear();
    if (on) {
      spinner.init();
      spinner.display();
    }
  } else {
    if (on) {
      spinner.display();
    }
  }
  previousValue = value;
}

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
  lightSensor1.publishData();
  return 1;
}

int pubState(String command) {
  lightSensor1.publishState();
  return 1;
}

void sample() {
  lightSensor1.sample();
}

void clear() {
  lightSensor1.clear();
}

// Use Particle console to publish an event, usually for the server to pick up.
int pubConsole(String paramStr) {
    const unsigned int  BUFFER_SIZE = 64;       // Maximum of 63 chars in an argument. +1 to include null terminator
    char                paramBuf[BUFFER_SIZE];  // Pre-allocate buffer for incoming args

    paramStr.toCharArray(paramBuf, BUFFER_SIZE);
    char *pch = strtok(paramBuf, "=");
    String event(pch);
    pch = strtok (NULL, ",");
    Utils::publish(event, String(pch));
    return 1;
}

int setPubRate(String command) {
  Utils::setInt(command, publishRateInSeconds, 1, 60 * 60); // don't allow publish rate > 1 hour.
  return 1;
}

void setup() {
  Utils::publish("Message", "Started setup...");
  Particle.function("getSettings", pubSettings);
  Particle.function("getData", pubData);
  Particle.function("pubFromCon", pubConsole);
  Particle.function("setPubRate", setPubRate);
  Particle.function("pubState", pubState);
  sample();
  lastPublishInSeconds = millis() / 1000;
  pubData("");
  clear();
  pubSettings("");
  oledWrapper.clear();
  Utils::publish("Message", "Finished setup...");
}

void loop() {
  timeSupport.handleTime();
  sample();
  display_on_oled();
  if ((lastPublishInSeconds + publishRateInSeconds) <= (millis() / 1000)) {
    lastPublishInSeconds = millis() / 1000;
    pubData("");
    clear();
  }
}

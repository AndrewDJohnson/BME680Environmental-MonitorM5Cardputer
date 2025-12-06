 #include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <NTPClient.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include "M5Cardputer.h"
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ctime>
#include "config.h"
//#include <ESP32Time.h>
//#include <RTClib.h>
//ESP32Time Inrtc;

#include <Adafruit_NeoPixel.h>
// Which pin on the Arduino is connected to the NeoPixels?
#define PIN 21
// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS 1
//The screen will go off in 60 secs!
#define SCREEN_OFF_TIME 60

#define BATTERY_VOLTAGE_WHEN_CHGING 4135
typedef enum {
  TEMP_VAL = 0,
  PRESS_VAL,
  HUMID_VAL,
  TIME_BATTERY_VAL,
  IP_ADDR,
  
  MAX_SCREEN_INDEX
} SCREEN_INFO_INDEX;


extern struct Config config;

void setup_m5(void);
void setup_sd_card(void);

void setup_bme_sensor(void);
void setup_display(void);
void setup_wifi_web(void);
void save_readings(void);

//String check_keyboard();
void check_sensor(void);
void update_display(void);
void check_web_req(void);
void save_readings_to_sd(char *readings);
//void update_time(void);
void check_battery(void);

//BME Sensor variables etc
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME680 bme;  // Create a BME680 sensor object for I2C
//String saved_readings,days_readings="";
bool sd_card_ok, reload_config = true;
int screen_timeout = SCREEN_OFF_TIME;
int current_screen_state = true;
int sensor_connected;
long up_time_in_seconds = 0;

time_t restart_time;
int start_minute = -1, env_reading_index = 0;
char *headings_string = "Station,Date,Time,Temp,Pressure,Humidity,Gas Resistance\n";

//Used for Stamp LEDs
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

//Web and IP stuff
String local_ip_addr;
WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 0, 60000);

int current_screen_no;
float env_values[3];

char message_buffer[256];
//Time stuff
//String time_str;
time_t rawtime;
struct tm *timeinfo = 0;

typedef struct env_reading_struc {
  time_t time_val;
  float temp_val;
  float press_val;
  float humidity;
  float gas_resistance;
} ENV_READING;

//Currently, samples are taken every minute.
//Assuming one day (24*60=1440 readings, stored every minute)
#define NUM_READINGS_PER_DAY 1440
// Enum for selecting the type of data to plot
enum DataTypeEnum { TEMPERATURE,
                    PRESSURE,
                    HUMIDITY } DataType;
//#define READINGS_PER_DAY 1440
ENV_READING env_readings_data[NUM_READINGS_PER_DAY];
int env_reading_counter = 0;  //,final_env_reading_counter=0;


//Web page HTML will be held in memory - page templates are loaded and then data added in...
String home_page_html, charts_page_html, error_log = "Error log:\n";

/* Print a message to bottom of the screen and to the serial port.
*/
/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
void print_msg(char *msg, bool error_msg = false) {
  char temp_buffer[120];
  String temp;
  Serial.printf(msg);
  delay(20);
  if (error_msg) {
    if (timeinfo) {
      strftime(temp_buffer, sizeof(temp_buffer), "%F-%H:%M:%S ", timeinfo);
      temp = temp_buffer;
    }
    temp = temp + msg + " - Free memory: " + String(ESP.getFreeHeap()) + "\n";
    log_msg((char *)temp.c_str());
  }
  if (!current_screen_state) {
    return;
  }


  if (error_msg) {
    M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.setCursor(0, 85);
  } else {
    M5.Lcd.setTextColor(GREEN, BLACK);
  }

  M5.Lcd.printf(msg);
}
void log_msg(char *msg) {
  File file;
  if (sd_card_ok) {
    file = SD.open("/envlogger.log", FILE_APPEND);
    if (!file) {
      Serial.println("Couldn't open log file.");
      return;
    }
  }
  file.printf("%s\n", msg);
  file.close();
}
/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
//Setup display stuff
void setup_display() {
  M5.Lcd.setTextSize(2);
  M5.Lcd.setRotation(1);
  M5.Lcd.clear(BLACK);
  M5.Lcd.setCursor(0, 0);
}
/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
//General setup for M5
void setup_m5() {

  auto cfg = M5.config();        // Get the M5 configuration
  M5Cardputer.begin(cfg, true);  // Initialize the M5
  M5.Power.begin();              // Initialize the M5 power
  M5Cardputer.Speaker.begin();
  Wire.begin();
  pixels.begin();  // INITIALIZE NeoPixel strip object (REQUIRED)
  M5Cardputer.Speaker.tone(4000, 300);
  delay(100);
  M5Cardputer.Speaker.tone(6000, 300);
}
/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
void load_web_page(String &html_buffer, char *filename = "/index.html") {
  File file;
  char *file_buffer;

  file = SD.open(filename);
  if (!file) {
    Serial.println("Failed to open HTML file for reading");
    return;
  }

  // Determine the size of the file
  size_t fileSize = file.size();
  if (fileSize > 0) {
    // Allocate a buffer to hold the file contents
    // Note: Ensure there's enough memory to allocate the buffer
    file_buffer = new char[fileSize + 1];  // +1 for null terminator
    memset(file_buffer, 0, fileSize + 1);
    // Read the file into the buffer
    size_t bytesRead = file.readBytes(file_buffer, fileSize);
    file.close();
    html_buffer = file_buffer;
    Serial.printf("Web page file loaded - buffer size: %d\n", fileSize);
    //Serial.print (generateConfigHTMLForm().c_str());
  } else {
    html_buffer = "";
  }
}

/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
//Play a WAV File
void play_wav(char *fileName, int count = 1, int delay_secs = 1) {
  char *buffer;
  std::string wav_filename;

  if ((fileName == NULL) || !(*fileName)) {
    return;
  }

  wav_filename = fileName;
  if (*fileName != '/') {
    wav_filename = "/" + wav_filename;
  } else {
    wav_filename = fileName;
  }

  File file = SD.open(wav_filename.c_str());
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  // Determine the size of the file (playback seems to be limited to 128KB size buffer)
  size_t fileSize = file.size();
  if ((fileSize > 0) && (fileSize < 131072)) {
    //Serial.printf("File opened - %s (%d bytes)\n",fileName, fileSize);

    // Allocate a buffer to hold the file contents
    // Note: Ensure there's enough memory to allocate the buffer
    buffer = new char[fileSize + 1];  // +1 for null terminator
    if (!buffer) {
      Serial.println("Failed to allocate memory for WAV file!");
      return;
    }
    //Serial.println("Reading WAV file!");
    //delay (200);
    // Read the file into the buffer
    size_t bytesRead = file.readBytes(buffer, fileSize);
    while (count--) {
      //Serial.println("Playing WAV file on speaker!");

      M5Cardputer.Speaker.playWav((const uint8_t *)buffer);
      delay(delay_secs * 1000);
    }
    delete[] buffer;
  } else {
    Serial.printf("%s is %d bytes and is too big or small to be played!", fileName, fileSize);
  }
}


/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
//Set the BME Sensor up.
void setup_bme_sensor() {

  Wire.begin();
  while (!bme.begin()) {
    Serial.printf("Connect BME sensor\n");
    //print_msg("Connect BME680 sensor", true);
    sensor_connected = false;
    Wire.end();
    return;
  }
  // Configure sensor settings
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);  // 320°C for 150 ms
  sensor_connected = true;

  delay(100);
}

/*************************************************************************
* Function:      check_keyboard
* Parameters:    delay value (ms) - if this is < 0 it will wait "forever"
* Description:   Checks and optionally waits for a keypress.
*
* Returns:       keypress as a string
**************************************************************************/
String check_keyboard(int delay_val = 0) {
  String data = "";
  //long screen_off_counter = SCREEN_OFF_TIME * 1000;
  bool key_pressed = false;

  unsigned long end_millis;
  //end_millis = millis() + delay_val;

  //do {
  //delay(1);
  M5Cardputer.update();
  if (M5Cardputer.Keyboard.isChange()) {

    if (M5Cardputer.Keyboard.isPressed()) {
      if (!key_pressed) {
        key_pressed = true;
        Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
        //delay_val = -1;
        if (status.del) {
          data += "\b";
        } else if (status.enter) {
          data += "\n";
          //plotGraph(TEMPERATURE, 60);
          return data;
        } else {
          for (auto i : status.word) {
            data += i;
          }
        }
        lcd_screen_on_off(1);
        screen_timeout = SCREEN_OFF_TIME;
        display_info(current_screen_no);
        current_screen_no++;
        current_screen_no %= MAX_SCREEN_INDEX;

        //Try to stop double-key press registering...
        delay(1);
      }
    } else {
      key_pressed = false;
    }
  }
  // } while ((millis() < end_millis) || (delay_val < 0));

  return data;
}

/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
void check_sd_card() {
  static bool default_loaded = false;
  //Serial.printf("Checking SD card\n");
  //delay (100);
  if (!SD.begin()) {
    ////print_msg("No SD card2-can't save\n",true);
    //delay(5000);
    sd_card_ok = false;
    //Serial.printf("Card not inserted\n");
    print_msg("Card not inserted.", true);
    SD.end();

    reload_config = true;
    if (!default_loaded) {
      //Serial.printf("loading default conf\n");
      loadConfig(true);
      default_loaded = true;
    }
    return;
  } else {
    if (SD.exists("/")) {
      //Serial.printf("Card inserted. (after '/ exists')");
    } else {
      print_msg("Cannot find root folder.", true);
      reload_config = true;
      SD.end();
      return;
    }
    sd_card_ok = true;

    if (reload_config) {
      Serial.printf("Reloading config and web page file data\n.");
      loadConfig();
      set_config_parameters();
      reload_config = false;
      default_loaded = true;
      //Serial.printf("Network List: Orig SSIDs: %s %s\n", config.wifi_ssid, config.wifi_pass);
      load_web_page(home_page_html);
      load_web_page(charts_page_html, "/charts without data.html");
      //Serial.printf("Network List: Orig SSIDs: %s %s\n", config.wifi_ssid, config.wifi_pass);
      delay(10);
    }
  }
}
/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
// Main setup
void setup() {
  // put your setup code here, to run once:

  Serial.begin(9600);
  Serial.printf("Starting\n");
  delay(500);
  setup_m5();
  setup_display();
  M5.Lcd.setTextColor(GREEN, BLACK);
  M5.Lcd.clear(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0, 10);
  M5.Lcd.printf("M5 BME650 WEBLOGGER\n");

  check_sd_card();
  //print_msg("Checked SD in Setup");
  if (sd_card_ok) {
    //Serial.printf("Setting up WiFi");
    setup_wifi_web();
    //Serial.printf("After 1st call\n");
    delay(100);
  } else {
    Serial.printf("SD card not readable.\n");
    delay(50);
  }
  setup_bme_sensor();
  //Serial.printf("Entering main loop\n");
  delay(100);
  env_reading_counter = 0;
}
/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
/*void display_info2(int screen_no) {
  std::string info_to_print;
  char charging = '\0';

  if (M5.Power.getBatteryVoltage() > BATTERY_VOLTAGE_WHEN_CHGING) {
    charging = '*';
  }
  M5.Lcd.clear();
  M5.Lcd.setCursor(0, 5);

  switch (screen_no) {
    case TIME_IP_ETC:
      //M5.Lcd.printf("Time: %s\nIP:%s\n",time_str.c_str(),local_ip_addr.c_str());
      M5.Lcd.printf("Time: %s\nIP:%s\n", asctime(timeinfo), local_ip_addr.c_str());
      M5.Lcd.printf("Bat: %d%c", M5.Power.getBatteryLevel(), charging);

      break;
    case SENSOR_DATA:

      sprintf(message_buffer, "Tmp: %0.2f\n", env_values[0] = bme.temperature);
      print_msg(message_buffer);
      sprintf(message_buffer, "Prs: %0.2f\n", env_values[1] = bme.pressure / 100.0);
      print_msg(message_buffer);
      sprintf(message_buffer, "Hum: %0.2f%%\n", env_values[2] = bme.humidity);
      print_msg(message_buffer);
      break;

    case CONFIG_INFO1:
      info_to_print = get_config_element(TEMP_ALARM_HIGH) + get_config_element(TEMP_ALARM_LOW) + get_config_element(PRESSURE_ALARM_HIGH) + get_config_element(PRESSURE_ALARM_LOW);
      sprintf(message_buffer, info_to_print.c_str());
      print_msg(message_buffer);
      break;
    case CONFIG_INFO2:
      info_to_print = get_config_element(HMDTY_ALARM_HIGH) + get_config_element(HMDTY_ALARM_LOW) + get_config_element(SCREEN_BRIGHTNESS) + get_config_element(WIFI_TIMEOUT);
      sprintf(message_buffer, info_to_print.c_str());
      print_msg(message_buffer);
      break;
  }
}
*/
void display_info(int screen_no) {
  std::string info_to_print, reading_name, val_name;
  float current_val;
  float max_val, min_val;
  bool show_main, show_max_min;
  //char charging = '\0';
  std::string units;

  //if (M5.Power.getBatteryVoltage() > BATTERY_VOLTAGE_WHEN_CHGING) {
  //  charging = '*';
  //}

  show_max_min = true;
  show_main = true;
  M5.Lcd.clear();
  M5.Lcd.setTextColor(GREEN, BLACK);
  
  //M5.Lcd.setCursor(0, 0);
  M5.Lcd.setTextSize(3);
  


  switch (screen_no) {
    case 0:
      //M5.Lcd.printf("Time: %s\nIP:%s\n",time_str.c_str(),local_ip_addr.c_str());
      val_name = "Temp";
      current_val = env_values[0] = bme.temperature;
      max_val = config.temp_alm_hi;
      min_val = config.temp_alm_lo;
      units = "C";
      break;
    case 1:
      val_name = "Pressure";
      current_val = env_values[1] = bme.pressure / 100.0;
      max_val = config.pres_alm_hi;
      min_val = config.pres_alm_lo;
      units = "hpA";
      break;

    case 2:
      val_name = "Humidity";
      current_val = env_values[2] = bme.humidity;
      max_val = config.hmdty_alm_hi;
      min_val = config.hmdty_alm_lo;
      units = "\%";
      break;

    case 3:
      show_main = false;
      show_max_min = false;
      strftime(message_buffer, sizeof(message_buffer), "\n%F\n%H:%M:%S", timeinfo);
      M5.Lcd.setCursor(1, 0);
      //M5.Lcd.printf(message_buffer);
      M5.Lcd.printf("%s\nUp-time:\n%02d:%02d:%02d",message_buffer,
            up_time_in_seconds / 3600,         // Hours
            (up_time_in_seconds % 3600) / 60,  // Minutes
            up_time_in_seconds % 60   );  
      
      break;
    case 4:
      show_main = false;
      show_max_min = false;
      int octet1, octet2, octet3, octet4;

      // Parse the IP address string into four integer values
      sscanf(local_ip_addr.c_str(), "%d.%d.%d.%d", &octet1, &octet2, &octet3, &octet4);
      M5.Lcd.setCursor(0, 1);
      if (WiFi.status()== WL_CONNECTED)
      {
        sprintf(message_buffer,"IP:\n%03d.%03d.\n%03d.%03d", octet1, octet2, octet3, octet4);
      }
      else
      {
        sprintf(message_buffer,"No network\connection.");
      }
      M5.Lcd.printf("\n%s\nBattery:%d%%",message_buffer,M5.Power.getBatteryLevel());

      break;
    

  }

  if (show_main) {
    M5.Lcd.setCursor(20, 5);
    M5.Lcd.printf("%s:", val_name.c_str());
    M5.Lcd.setCursor(20, 50);
    M5.Lcd.printf("%.1f%s", current_val, units.c_str());
  }

  if (show_max_min) {
    // Display max temperature in smaller font below to the left
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 95);
    M5.Lcd.printf("Max:%.1f%s", max_val, units.c_str());

    // Display min temperature in smaller font below to the right
    M5.Lcd.setCursor(10, 110);
    M5.Lcd.printf("Min:%.1f%s", min_val, units.c_str());
  }
}

/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
void set_config_parameters() {
  M5.Speaker.setVolume(config.speaker_volume);
  M5.Lcd.setBrightness(config.screen_brightness);
  M5.update();
}


// Function to handle commands
void handleSerialCommand(String command) {
  static String old_pass = config.wifi_pass;
  command.trim();  // Remove whitespace and newline characters
  Serial.printf ("Command received: %s\n",command.c_str());
  if (command == "WiOff") {
    WiFi.disconnect();
    strcpy(config.wifi_pass, "12;12");
    Serial.printf("Disconnected Wifi\n");
  } else if (command == "WiOn") {
    strcpy(config.wifi_pass, old_pass.c_str());
    WiFi.reconnect();
    Serial.printf("Reconnected Wifi\n");
  }
}
/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
//Main loop
void loop() {
  char file_name[80];
  static int alarm_check_counter = 10;
  static int repeat_alarms = config.repeat_alarms, last_second = -1;
  static int alarm_active = false, last_alarm_active = false;

  M5Cardputer.update();
  check_keyboard(1);

  if (WiFi.status() == WL_CONNECTED) {

    server.handleClient();
  }
  rawtime = timeClient.getEpochTime();

 // Check if data is available on the serial port
  if (Serial.available() > 0) {
    // Read the incoming command
    String command = Serial.readStringUntil('\n'); // Read until newline
    handleSerialCommand(command); // Process the command
  }


  //rawtime++;
  //delay (10);
  timeinfo = localtime(&rawtime);

  //Check if 1 second has elapsed
  if (last_second != timeinfo->tm_sec) {
    if (!timeinfo->tm_sec) {
      //Serial.printf("Reading counter: %d, index - %d\n", env_reading_counter, env_reading_index);
      if (timeinfo->tm_year > 124) {
        //Save readings every minute when the clock is initialised!
        if (sensor_connected) {
          save_readings();
        }
      }
    }
    //current_minute= timeinfo->tm_hour*60+timeinfo->tm_min;
    if (current_screen_state)
    {
       display_info(current_screen_no);
    }
    last_second = timeinfo->tm_sec;
    up_time_in_seconds++;
    if (start_minute < 0) {
      start_minute = (timeinfo->tm_hour * 60) + timeinfo->tm_min;
      //current_minute=start_minute;
      //rawtime += 36300;
      Serial.printf("Start minute is: %d\n", start_minute);
    }
    //print_msg("1 sec passed");
    //time_str = timeClient.getFormattedTime();

    if (config.audible_alarms) {
      if (!alarm_check_counter--) {
        //Serial.printf("Checking limits (%d repeats)\n", repeat_alarms);
        alarm_check_counter = config.alarm_response_time;

        if (repeat_alarms > 0) {
          //If an alarm happens, we count down.
          if (check_env_values()) {
            //Set LED red to indicate an alarm
            pixels.setPixelColor(0, pixels.Color(100, 0, 0));
            repeat_alarms--;
          }
          else
          {
            pixels.setPixelColor(0, pixels.Color(0, 0, 0));
          }
          pixels.show();
        }
      }
    }

    //Every hour... check if we are near the top of the hour!
    if ((timeinfo->tm_sec == 30) && (timeinfo->tm_min == 59) && (timeinfo->tm_year > 124)) {
      //Make a file name in the form of the current date - YYYY-MM-DD etc
      strftime(file_name, 80, "/%F-readings.csv", timeinfo);
      save_readings_to_sd(file_name);

      //Resync the clock
      timeClient.update();
    }
    if (!screen_timeout) {
      lcd_screen_on_off(0);
    } else {
      screen_timeout--;
    }
    //Every 10 seconds
    if (!(timeinfo->tm_sec % 10)) {
      check_battery();
      check_sensor();

      //display_info(current_screen_no);
      check_sd_card();
      //Reset repeat alarm counter every 2 mins
      if (!(timeinfo->tm_min % 2) && !timeinfo->tm_sec) {
        Serial.printf("check alarm repeats %d\n", repeat_alarms);
        if (!repeat_alarms) {
          repeat_alarms = config.repeat_alarms;
        }
      }
      if (WiFi.status() != WL_CONNECTED) {
        //WiFi.disconnect();
        setup_wifi_web();
      }
    }
  }
}

/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
//Check the sensor reading and put the data in various places.
void check_sensor() {

  if (sensor_connected) {
    if (!bme.performReading()) {
      bme.endReading();
      delay(20);
      //If we've just detected disconnect, close the I2C bus
      if (sensor_connected) {
        Wire.end();
      }
      sensor_connected = false;
      print_msg("Sensor is not connected...", true);
    }
  } else {
    print_msg("Trying to connect sensor...");
    setup_bme_sensor();
    delay(100);
  }
}

/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
//Display battery info
void check_battery() {
  pixels.clear();
  M5.update();
  if (M5.Power.getBatteryVoltage() >= BATTERY_VOLTAGE_WHEN_CHGING) {
    //Illuminate green LED pixel
    pixels.setPixelColor(0, pixels.Color(0, 50, 0));
    //Serial.printf ("Charging");
  } else {
    //Illuminate green LED pixel
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    //print_msg ("Not Charging",true);
  }
  pixels.show();
}
/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
void save_readings() {


  //If we have both zero humidity and temp are zero, discard the readings
  if ((int)bme.temperature || (int)bme.humidity) {
    //Serial.printf("Current minute:%d\n",env_reading_index);
    env_readings_data[env_reading_index].humidity = bme.humidity;
    env_readings_data[env_reading_index].time_val = timeClient.getEpochTime();
    env_readings_data[env_reading_index].temp_val = bme.temperature;
    env_readings_data[env_reading_index].humidity = bme.humidity;
    env_readings_data[env_reading_index].press_val = bme.pressure / 100.0;
    env_readings_data[env_reading_index].gas_resistance = bme.gas_resistance / 1000.0;
    if (env_reading_counter <= NUM_READINGS_PER_DAY) {
      env_reading_counter++;
    }
    env_reading_index++;
    env_reading_index %= NUM_READINGS_PER_DAY;
  }
}
/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
void save_readings_to_sd(char *file_name) {
  bool write_headings;
  static int last_env_reading_counter = 0;
  int i, last_reading;
  char time_str[64];

  if (sd_card_ok) {
    write_headings = !SD.exists(file_name);

    File file = SD.open(file_name, FILE_APPEND);
    if (!file) {
      print_msg("Failed to open file for writing", true);
      return;
    }
    if (write_headings) {
      file.print(headings_string);
    }

    //Dump the readings we had since the last save.
    while (last_env_reading_counter != env_reading_index) {
      struct tm *timeinfo = localtime(&env_readings_data[last_env_reading_counter].time_val);
      strftime(time_str, sizeof(time_str), "%F,%H:%M:%S", timeinfo);
      sprintf(message_buffer, "%s,%s,%0.2f,%0.2f,%0.2f,%0.2f\n", config.station_name, time_str,
              env_readings_data[last_env_reading_counter].temp_val, env_readings_data[last_env_reading_counter].press_val, env_readings_data[last_env_reading_counter].humidity, env_readings_data[last_env_reading_counter].gas_resistance);
      file.print(message_buffer);
      //Serial.printf("%d - %s\n", last_env_reading_counter, message_buffer );
      last_env_reading_counter++;
      //Wrap around the index when we get to the end of the stored array.
      last_env_reading_counter %= NUM_READINGS_PER_DAY;
    }
    //long mem=ESP.getFreeHeap();
    //sprintf (message_buffer,"Free Mem:%ld\n",ESP.getFreeHeap());
    //file.print(message_buffer);
    file.close();
  }
}
/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
//Set screen on or off
void lcd_screen_on_off(bool state) {
  if (state == current_screen_state) {
    return;
  }

  if (state) {
    M5.Lcd.setBrightness(config.screen_brightness);
    M5.Lcd.wakeup();
    Serial.printf("Screen on...\n");
  } else {
    M5.Lcd.setBrightness(0);
    M5.Lcd.sleep();
    current_screen_no = 0;
    Serial.printf("Screen off...\n");
  }
  M5.update();
  current_screen_state = state;
}
/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       True if an alarm exists, false if not.
**************************************************************************/
bool check_env_values() {
  int i, config_element_index;

  for (i = 0; i < 3; i++) {
    config_element_index = i * 4;
    //Check each of the temp, press values etc against the limits and play an alarm
    if (env_values[i] > *(float *)config_elements[config_element_index].value_ptr) {
      //Serial.printf("Alarm started\n");
      play_wav((char *)config_elements[config_element_index + 2].value_ptr, 1);
      return true;
    }
    if (env_values[i] < *(float *)config_elements[config_element_index + 1].value_ptr) {
      //Serial.printf("Alarm started\n");
      play_wav((char *)config_elements[config_element_index + 3].value_ptr, 1);

      return true;
    }
  }
  return false;
}
/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
//Try to connect to WiFI and start webserver etc
void setup_wifi() {
  // Parse the SSID and password strings
  String separator = ";";  // Use the separator as ';' or space
  String ssids[10];
  String passwords[10];
  int networkCount = 0;
  String ssidList = config.wifi_ssid;
  String passwordList = config.wifi_pass;

  ssidList += ";";
  passwordList += ";";

  while (ssidList.length() > 0 && networkCount < 10) {
    int sepIndex = ssidList.indexOf(separator);
    if (sepIndex == -1) {
      sepIndex = ssidList.length();
    }
    ssids[networkCount] = ssidList.substring(0, sepIndex);
    ssidList = ssidList.substring(sepIndex + 1);

    sepIndex = passwordList.indexOf(separator);
    if (sepIndex == -1) {
      sepIndex = passwordList.length();
    }
    passwords[networkCount] = passwordList.substring(0, sepIndex);
    passwordList = passwordList.substring(sepIndex + 1);

    networkCount++;
  }

  int scanned_networkCount = WiFi.scanNetworks();

  if (scanned_networkCount == 0) {
    print_msg("No networks found.");
  } else {

    for (int i = 0; i < scanned_networkCount; i++) {
      print_msg((char *)ssids[i].c_str());
      print_msg("\n");
      // Attempt to connect to each network
      for (int j = 0; j < networkCount; j++) {
        //Do we have a match in our list for one of the found networks?
        if (ssids[j] == WiFi.SSID(i)) {
          WiFi.begin(ssids[j].c_str(), passwords[j].c_str());

          unsigned long startAttemptTime = millis();

          // Wait for connection or timeout (10 seconds)
          while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < config.wifi_timeout * 1000) {
            delay(500);
            Serial.print(".");
          }

          if (WiFi.status() == WL_CONNECTED) {

            return;
          } else {
            Serial.println("\nFailed to connect.");
          }
        }
      }
    }
  }

  // Check if no network was connected
  if (WiFi.status() != WL_CONNECTED) {
    //Serial.println("\nCould not connect to any network.");
    WiFi.disconnect();
    //delay(50);
    //Serial.println("\nAttempt to reconnect.");
    //delay(50);
    //WiFi.reconnect();
    //Serial.println("\nAttempted to reconnect.");
    //delay(200);
    //Serial.println("\nExiting attempt to reconnect.");
  }
}

/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
void setup_wifi_web(void) {
  int i;
  WiFi.mode(WIFI_STA);

  //Serial.printf("Network List: Orig SSIDs: %s %s\n", config.wifi_ssid, config.wifi_pass);
  setup_wifi();

  if (WiFi.status() != WL_CONNECTED) {
    print_msg("WiFi failed to connnect\n", true);
    local_ip_addr = "0.0.0.0";
    rawtime = 173791122;
    return;
  } else {
    local_ip_addr = WiFi.localIP().toString();

    for (i = 1; i < 5; i++) {
      timeClient.update();
      rawtime = timeClient.getEpochTime();
      timeinfo = localtime(&rawtime);
      //Check we were able to set the time up and have another try if it didn't work!
      if (timeinfo->tm_year > 70) {
        restart_time = rawtime;
        break;
      }
    }
    log_msg((char *)asctime(timeinfo));
    //break;
  }
  sprintf(message_buffer, "IP:%s\n", local_ip_addr.c_str());
  print_msg(message_buffer);


  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  } else {
    Serial.println("MDNS responder failed to start");
    return;
  }

  server.on("/", handleRoot);
  //User has asked for config page
  server.on("/config", handleShowConfig);
  server.on("/submit", handle_form_submit);
  server.on("/datafiles", handle_csv_list);
  //server.on("/test",handle_test);
  //server.on("/action", HTTP_POST, handle_post);


  server.on("/errors", []() {
    File file = SD.open("/envlogger.log", "r");
    //server.send(200, "text/plain", error_log.c_str());
    server.streamFile(file, "text/plain");
    file.close();
  });
  //server.onNotFound(handleNotFound);
  server.onNotFound(handleFileDownload);

  // Define the /readings route
  server.on("/readings", HTTP_GET, []() {
    // Format the JSON manually using string concatenation
    String jsonResponse = "{";
    jsonResponse += "\"station\":" + String(config.station_name) + ",";
    jsonResponse += "\"temperature\":" + String(bme.temperature, 2) + ",";
    jsonResponse += "\"pressure\":" + String(bme.pressure / 100.0, 2) + ",";
    jsonResponse += "\"humidity\":" + String(bme.humidity, 2) + ",";
    jsonResponse += "\"battery\":" + String(M5.Power.getBatteryLevel());
    sprintf(message_buffer, "%02d:%02d:%02d",
            up_time_in_seconds / 3600,         // Hours
            (up_time_in_seconds % 3600) / 60,  // Minutes
            up_time_in_seconds % 60);          // Seconds
    jsonResponse += ", \"up_time\":";
    jsonResponse += message_buffer;
    jsonResponse += "}";

    // Send the JSON response
    server.send(200, "application/json", jsonResponse);
  });


  server.begin();
  print_msg("HTTP server started");
}

/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
// Function to generate the HTML string with downloadable CSV links
/*String generateCSVListHTML() {
  String html = R"rawliteral(
    <h1>Download CSV Files</h1><ul>)rawliteral";

  // Open the root directory
  File root = SD.open("/");

  if (!root) {
    html += "<p>Error opening directory!</p>";
  } else {
    // List all files in the root directory
    File file = root.openNextFile();
    while (file) {
      String filename = file.name();
      if (filename.endsWith(".csv")) {  // Only process .csv files
        html += "<li><a href=\"/" + filename + "\" download>" + filename + "</a></li>";
      }
      file = root.openNextFile();
    }
  }

  //html += "</ul></body></html>";
  html += "</ul>";
  return html;
}*/

String generateCSVListHTML() {
  String html = R"rawliteral(
    <h1>Download CSV Files</h1><ul>)rawliteral";

  File root = SD.open("/");
  if (!root) {
    html += "<p>Error opening directory!</p>";
  } else {
    // Store filenames dynamically
    String* filenames = nullptr;
    int fileCount = 0;

    // Read files and store CSV filenames
    File file = root.openNextFile();
    while (file) {
      String filename = file.name();
      if (filename.endsWith(".csv")) {
        String* newArray = new String[fileCount + 1];
        for (int i = 0; i < fileCount; i++) {
          newArray[i] = filenames[i];
        }
        newArray[fileCount] = filename;
        delete[] filenames;
        filenames = newArray;
        fileCount++;
      }
      file = root.openNextFile();
    }

    // Sort filenames alphabetically (Bubble Sort for simplicity)
    for (int i = 0; i < fileCount - 1; i++) {
      for (int j = 0; j < fileCount - i - 1; j++) {
        if (filenames[j] > filenames[j + 1]) {
          String temp = filenames[j];
          filenames[j] = filenames[j + 1];
          filenames[j + 1] = temp;
        }
      }
    }

    // Generate HTML list from sorted filenames
    for (int i = 0; i < fileCount; i++) {
      html += "<li><a href=\"/" + filenames[i] + "\" download>" + filenames[i] + "</a></li>";
    }

    // Free dynamically allocated memory
    delete[] filenames;
  }

  html += "</ul>";
  return html;
}

/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
int find_next_element(String search_string, String sub_string, int &length) {
  int string_pos;
  string_pos = search_string.indexOf(sub_string);
  length = sub_string.length();
  //Serial.printf("Start pos:%d   Next Pos:%d\n",start_pos,string_pos);
  return string_pos;
}
/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
void send_chunk_to_server(String chunk) {
  String chunkSize = String(chunk.length(), HEX);
  server.sendContent(chunkSize + "\r\n");
  server.sendContent(chunk + "\r\n");
  //Serial.printf("\nChunk start: %s size:%d\n", chunk.substring(0, 30).c_str(), chunk.length());
  delay(5);
}

//String html_data2="123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
/*
void handle_test()
{
String html_data2="123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
  
  return;
  
  // Send start of chunked HTML data
  server.sendHeader("Content-Type", "text/html");
  server.sendHeader("Transfer-Encoding", "chunked");
  server.send(200);
  send_chunk_to_server(charts_page_html);
  for (int i=0;i<up_time_in_seconds*20; i++)
  {
  html_data2=html_data2+"const pressure_data = [{x:1737032640000,y:1028.4},{x:1737032700000,y:1028.5},{x:1737032760000,y:1028.5},{x:1737032820000,y:1028.5},{x:1737032880000,y:1028.5}]";
  }
  send_chunk_to_server(html_data2);
  server.sendContent("0\r\n\r\n");  // End of chunks

}*/
/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
#define CHUNK_SIZE 10000  // Maximum size of each HTML data chunk
String html_data;
//void send_charts_data(int &temp_max, int &temp_min, int &press_max, int &press_min, int &humid_max,int &humid_min) {
void send_charts_data() {

  int i, j, count, data_index, start_index;
  time_t time_value;
  float data_val;

  //We have 3 sets of data to send to the client - temp, pressure, humidity
  for (j = 0; j < 4; j++) {
    html_data += "const ";
    switch (j) {
      case 0:
        html_data += "temp";
        break;
      case 1:
        html_data += "pressure";
        break;
      case 2:
        html_data += "humidity";
        break;
      case 3:
        html_data += "timestamps";
        break;
    }
    html_data += "_data = [";
    //Serial.printf("Start Minute: %d - start index: %d\n",start_minute,start_minute%NUM_READINGS_PER_DAY);
    if (env_reading_counter < NUM_READINGS_PER_DAY) {
      count = env_reading_counter;
      start_index = 0;
    } else {
      count = NUM_READINGS_PER_DAY;
      start_index = env_reading_index;
    }

    for (i = 0; i < count; i++) {

      data_index = (start_index + i) % NUM_READINGS_PER_DAY;

      switch (j) {
        case 0:
          data_val = env_readings_data[data_index].temp_val;
          break;
        case 1:
          data_val = env_readings_data[data_index].press_val;
          break;
        case 2:
          data_val = env_readings_data[data_index].humidity;
          break;
        case 3:
          html_data += String(env_readings_data[data_index].time_val) + "000,";
          break;
      }
      if (j < 3) {
        snprintf(message_buffer, sizeof(message_buffer), "%0.1f,", data_val);
        html_data += message_buffer;
      }

      if (html_data.length() >= CHUNK_SIZE) {
        send_chunk_to_server(html_data);
        //Serial.printf (html_data.c_str());
        html_data = "";  // Reset the string
      }
    }
    if ((env_reading_counter) && html_data.length()) {
      html_data.remove(html_data.length() - 1);  // Remove trailing comma
    }
    html_data += "];\n";  // Close the array
  }
  send_chunk_to_server(html_data);
  html_data = "";  // Reset the string
}
/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
String output_html;
void handle_csv_list() {
  int i, tag_length;

  int string_pos, next_string_pos, next_string_pos2;

  //Generate chart data and replace fields in the chart data based on maxima and minima.
  output_html = charts_page_html;
  output_html.replace("{station_name}", config.station_name);

  // Send start of chunked HTML data
  server.sendHeader("Content-Type", "text/html");
  server.sendHeader("Transfer-Encoding", "chunked");
  server.send(200);

  //Send first part of HTML page we generated
  string_pos = find_next_element(output_html, "{chart_data}", tag_length);
  send_chunk_to_server(output_html.substring(0, string_pos));
  string_pos += tag_length;

  //Send data in chunks
  Serial.printf("Sending charts HTML data.\n");
  send_charts_data();

  next_string_pos = find_next_element(output_html, "{csv_files}", tag_length);
  if (!next_string_pos) {
    next_string_pos = string_pos;
  } else {
    //Send a chunk which is in between the bits of data we're substituting.
    send_chunk_to_server(output_html.substring(string_pos, next_string_pos));
  }
  //Send the generated CSV list
  send_chunk_to_server(generateCSVListHTML() + "Free Memory: " + String(ESP.getFreeHeap()));
  //Now send the remainder of the page.
  send_chunk_to_server(output_html.substring(next_string_pos + tag_length));

  server.sendContent("0\r\n\r\n");  // End of chunks
  //print_msg("Sent all chunks\n");
  //delay (50);
}

/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
// Web server handler to serve the actual CSV files
void handleFileDownload() {
  String path = server.uri();  // Get the requested file path
  if (SD.exists(path)) {
    File file = SD.open(path);
    server.streamFile(file, "text/csv");
    file.close();
  } else {
    server.send(404, "text/plain", "File Not Found");
  }
}

/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
//Function which responds to root Web Page request - so dump the enviromental data to the page!
void handleRoot() {

  char *response_txt;
  String html_file_list;
  int page_length = home_page_html.length(), bat_level = M5.Power.getBatteryLevel();

  //Allocate memory for the HTML
  if (page_length > 0) {
    //The final text will be longer as data is substituted in
    page_length += 200;
    //html_file_list=generateCSVListHTML();
    response_txt = new char[page_length];
    memset(response_txt, 0, page_length);

    strftime(message_buffer, sizeof(message_buffer), "%F - %H:%M:%S", timeinfo);
    //Print the data into the HTML (which will have %f's etc in it as data place holders)
    sprintf(response_txt, home_page_html.c_str(),
            config.station_name,
            config.station_name,
            message_buffer,
            bat_level,
            bme.temperature, config.temp_alm_lo, config.temp_alm_hi,
            bme.pressure / 100.0, config.pres_alm_lo, config.pres_alm_hi,
            bme.humidity, config.hmdty_alm_lo, config.hmdty_alm_hi);

    server.send(200, "text/html", response_txt);
    //Serial.printf(response_txt);
    delete[] response_txt;
  } else {

    server.send(200, "text/plain", "Error loading homepage.\n");
  }
}
/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
//Web server error report back
/*void handleNotFound() {
  
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  
}*/
/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
void handle_form_submit() {
  save_form_config_data(server);
  server.send(200, "text/html", generateConfigHTMLForm("/", "Back/Home", "Data Saved!").c_str());
  set_config_parameters();
  saveConfig();
}
/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
void handleShowConfig() {
  server.send(200, "text/html", generateConfigHTMLForm("/", "Back/Home", "Click 'Submit' to Save Data!").c_str());
}

/*************************************************************************
* Function:      
* Parameters:    
* Description:   
*
* Returns:       Nothing
**************************************************************************/
// Function to plot the graph
void plotGraph(int type, int periodMinutes) {
  int startIndex = max(0, NUM_READINGS_PER_DAY - periodMinutes);  // Start index for the specified period
  float minValue = 100000, maxValue = -100000;                    // Set initial min and max for the data type

  // Find min and max in the period
  for (int i = startIndex; i < NUM_READINGS_PER_DAY; i++) {
    float value = 0.0;
    switch (type) {
      case TEMPERATURE: value = env_readings_data[i].temp_val; break;
      case PRESSURE: value = env_readings_data[i].press_val; break;
      case HUMIDITY: value = env_readings_data[i].humidity; break;
    }
    if (value < minValue) minValue = value;
    if (value > maxValue) maxValue = value;
  }

  // Clear screen and prepare axes
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.drawLine(20, 115, 220, 115, TFT_WHITE);  // X-axis
  M5.Lcd.drawLine(20, 20, 20, 115, TFT_WHITE);    // Y-axis

  // Plot the data points within the min/max range
  for (int i = startIndex; i < NUM_READINGS_PER_DAY; i++) {
    float value = 0.0;
    switch (type) {
      case TEMPERATURE: value = env_readings_data[i].temp_val; break;
      case PRESSURE: value = env_readings_data[i].press_val; break;
      case HUMIDITY: value = env_readings_data[i].humidity; break;
    }

    // Scale and position
    int xPos = map(i - startIndex, 0, periodMinutes, 20, 220);  // Map index to X-axis (20 to 220 for 240-width screen)
    int yPos = map(value, minValue, maxValue, 115, 20);         // Map value to Y-axis (20 to 115 for 135-height screen)
    M5.Lcd.drawPixel(xPos, yPos, TFT_GREEN);                    // Draw pixel on graph
  }

  // Display labels for min, max, and units
  M5.Lcd.setTextColor(TFT_WHITE);
  M5.Lcd.setCursor(5, 5);
  M5.Lcd.printf("Max: %.2f", maxValue);
  M5.Lcd.setCursor(5, 120);
  M5.Lcd.printf("Min: %.2f", minValue);

  // Add legend based on data type
  M5.Lcd.setCursor(80, 5);
  switch (type) {
    case TEMPERATURE: M5.Lcd.print("Temperature (C)"); break;
    case PRESSURE: M5.Lcd.print("Pressure (hPa)"); break;
    case HUMIDITY: M5.Lcd.print("Humidity (%)"); break;
  }
}


//Structure for saving config data for the Application!
#define MAX_STRING_LEN 50
// Define the structure to store configuration items
struct Config {
  float temp_alm_hi;
  float temp_alm_lo;
	char temp_hi_alm_file[MAX_STRING_LEN];
	char temp_lo_alm_file[MAX_STRING_LEN];
	
  float pres_alm_hi;
  float pres_alm_lo;
	char pres_alm_hi_file[MAX_STRING_LEN];
	char pres_alm_lo_file[MAX_STRING_LEN];
	
  float hmdty_alm_hi;
  float hmdty_alm_lo;
	char hmdty_alm_hi_file[MAX_STRING_LEN];
	char hmdty_alm_lo_file[MAX_STRING_LEN];
	
	char wifi_pass[MAX_STRING_LEN];
	char wifi_ssid[MAX_STRING_LEN];
  char station_name[MAX_STRING_LEN];
  int screen_brightness;
  int wifi_timeout;
  int audible_alarms;
  int repeat_alarms;
  int alarm_response_time;
  int speaker_volume;
} ;

typedef struct config_element
{
	void *value_ptr;
	const char *value_name;
	//I for int, F for float or S for string`
	const char value_type;
  const char *default_value;
} CONFIG_ELEMENT;

typedef enum
{

TEMP_ALARM_HIGH,
TEMP_ALARM_LOW,
TEMP_HIGH_ALARM_FILE_NAME,
TEMP_LOW_ALARM_FILE_NAME,

PRESSURE_ALARM_HIGH,
PRESSURE_ALARM_LOW,
PRESSURE_ALARM_HIGH_FILE_NAME,
PRESSURE_ALARM_LOW_FILE_NAME,

HMDTY_ALARM_HIGH,
HMDTY_ALARM_LOW,
HMDTY_ALARM_HIGH_FILE_NAME,
HMDTY_ALARM_LOW_FILE_NAME,

WIFI_PASSWORD,
WIFI_SSID,
SCREEN_BRIGHTNESS,
WIFI_TIMEOUT,
AUDIBLE_ALARMS_ENABLED,
MAX_CONFIG_ELEMENT
} CONFIG_ELEMENT_NO;

#define CONFIG_FILE_NAME "/config.ini"

bool loadConfig(bool load_default=false);
bool saveConfig();
std::string get_config_element (int );
std::string generateConfigHTMLForm(char *next_page, char *next_page_desc, char *message);
void save_form_config_data(WebServer &) ;

extern CONFIG_ELEMENT config_elements[];

/*
typedef struct environment_reading_info
{
    const char *name;
    int val_lo;
    int val_hi;
    const char *low_alm_file;
    const char *high_alm_file;
} ENV_READING_INFO;

ENV_READING_INFO environment_readings_limits[]=
{
    {"temp",-999,1000,"temp_lo.wav","temp_hi.wav"},
    {"hmdty",-999,1000,"hmdty_lo.wav","hmdty_hi.wav"},
    {"pressure",-999,10000,"pressure_lo.wav","pressure_hi.wav"}
};

//This generates 3 values corresponding to indexes in the array of structures above.
typedef enum
{
  TEMP, HMDTY,PRESSURE
} ENV_DATA_TYPES;
*/
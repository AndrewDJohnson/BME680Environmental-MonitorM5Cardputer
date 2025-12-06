#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <unordered_map>
#include <cstdlib>


#include "FS.h"

#include <Arduino.h>
#include <Wire.h>
#include "SD.h"
#include "SPI.h"
#include "M5Cardputer.h"
#include <WebServer.h>
#include "config.h"

//This code generates the config page so that the user can configure the parameters in the config structure
//e.g. wifi ssid, password, max and min alarms etc

struct Config config;
//String check_key(int delay_val) ;
CONFIG_ELEMENT config_elements[]=
{
  {(void *)&config.temp_alm_hi,  "temp_alm_hi", 'f',"500"},
  {(void *)&config.temp_alm_lo,  "temp_alm_lo", 'f',"-100"},
  {(void *)&config.temp_hi_alm_file,"temp_hi_alm_file",'s',""},
  {(void *)&config.temp_lo_alm_file,"temp_lo_alm_file",'s',""},
  
  {(void *)&config.pres_alm_hi,  "pres_alm_hi", 'f',"5000"},
  {(void *)&config.pres_alm_lo,  "pres_alm_lo", 'f',"-5000"},
  {(void *)&config.pres_alm_hi_file,"pres_hi_alm_file",'s',""},
  {(void *)&config.pres_alm_lo_file,"pres_lo_alm_file",'s',""},
 
  {(void *)&config.hmdty_alm_hi,  "hmdty_alm_hi", 'f',"105"},
  {(void *)&config.hmdty_alm_lo,  "hmdty_alm_lo", 'f',"-1"},
  {(void *)&config.hmdty_alm_hi_file,"hmdty_hi_alm_file",'s',""},
  {(void *)&config.hmdty_alm_lo_file,"hmdty_lo_alm_file",'s',""},

	{(void *)&config.wifi_ssid,"wifi_ssid",'s',"ssid"},
	{(void *)&config.wifi_pass,"wifi_pass",'s',"password"},
  {(void *)&config.station_name,"station_name",'s',"M5 Cardputer EM"},
  {(void *)&config.screen_brightness,"scr_brightness",'i',"50"},
  {(void *)&config.wifi_timeout,"wifi_timeout",'i',"15"},
  {(void *)&config.audible_alarms,"audible_alarms",'i',"1"},
  {(void *)&config.repeat_alarms,"repeat_alarms",'i',"3"}, 
  {(void *)&config.alarm_response_time,"alarm_response_time",'i',"5"}, 
  {(void *)&config.speaker_volume,"speaker_volume",'i',"30"},

};

// Helper function to trim whitespace from strings
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    size_t last = str.find_last_not_of(" \t\n\r");
    return (first == std::string::npos || last == std::string::npos) ? "" : str.substr(first, last - first + 1);
}

void set_config_element (int element_no, std::string value)
{
  //Serial.printf("***Element no %d is type '%c' and is being set to value %s\n",element_no,config_elements[element_no].value_type,value.c_str());
  //delay(2);
  switch (config_elements[element_no].value_type)
  {
    case 'i':
      *((int *) config_elements[element_no].value_ptr) = std::stoi(value);
      break;
    case 'f':
      *((float *) config_elements[element_no].value_ptr) = std::stof(value);
      break;
    case 's':
      strcpy((char *)config_elements[element_no].value_ptr,value.c_str());
      //Serial.printf("Copied string config element: (%s) - (%s)\n",(char *)config_elements[element_no].value_name,(char *)config_elements[element_no].value_ptr);
      //delay (2);
      break;
  }
}

std::string get_config_element (int element_no)
{
  char config_value[100];
  std::string config_string;
  switch (config_elements[element_no].value_type)
  {
    case 'i':
      sprintf (config_value,"%s=%d",config_elements[element_no].value_name,*((int *) config_elements[element_no].value_ptr));
      break;
    case 'f':
      sprintf (config_value,"%s=%0.2f",config_elements[element_no].value_name,*((float *) config_elements[element_no].value_ptr));
      break;
    case 's':
      sprintf (config_value,"%s=%s",config_elements[element_no].value_name,(char *) config_elements[element_no].value_ptr);
      break;
  }  
  config_string=config_value;
  config_string+="\n";
  return (config_string);
}
// Function to parse the INI file and load configuration into the structure
bool loadConfig(bool load_default) {

    int config_element_counter,config_size;
    std::string value;
    
    config_size=sizeof(config_elements)/sizeof(CONFIG_ELEMENT);
    if (load_default)
    {
      //Serial.printf("Setting Default Config - Sizes %d and %d\n", sizeof(config_elements), );
      
      config_element_counter=0;
      //Loop through the config elements structure and set default values
      do
      {
        value = config_elements[config_element_counter].default_value;
        //Serial.printf("Setting config element\n");
        set_config_element(config_element_counter,value);
        //Serial.printf("%s=%s\n",config_elements[config_element_counter].value_name,value.c_str());
        //delay(2000);

      }
      while (config_element_counter++<config_size-1);
      Serial.printf("Set default conf\n");
      return true;

    }

    File file = SD.open(CONFIG_FILE_NAME);
    //check_key(2000);
    if(!file){
        Serial.println("Failed to open config file for reading\n");
        return false;
    }

    std::string line, section;
    String file_line;
    Serial.printf("Reading Config\n");
    

    while(file.available()){
        file_line=file.readStringUntil('\n'); 
        //Get rid of any extraneous spaces etc.
        char temp_buffer[256];
        sscanf(file_line.c_str(),"%s",temp_buffer);
        if (!*temp_buffer)
        {
          continue;
        }
        line = temp_buffer;
        
        // Ignore comments or empty lines
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        // Check if it's a section header (e.g., [section])
        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }

        // Process key-value pairs within a section
        size_t delimiterPos = line.find('=');
        if (delimiterPos != std::string::npos) {
            std::string key = trim(line.substr(0, delimiterPos));
            value = trim(line.substr(delimiterPos + 1));
            //Serial.printf ("Name is %s and value is %s\n",key.c_str(),value.c_str());
            config_element_counter=0;
            //Loop through the config elements structure so we can put the value read from the file in the correct part of the config structure in RAM etc
            do
            {
                std::string config_element= config_elements[config_element_counter].value_name;
                if (key == config_element)
                {
                  
                  set_config_element(config_element_counter,value);
                  //Serial.printf("%d:%s=%s - whole line was: %s\n", config_element_counter, config_element.c_str(),value.c_str(),line.c_str());

                }
                //delay (10);
            }
            while (config_element_counter++<config_size-1);
        }
    }
    file.close();
    return true;
}

bool saveConfig() 
{
    int config_size,i;
    File file = SD.open(CONFIG_FILE_NAME,FILE_WRITE);
    std::string value;
    config_size=sizeof(config_elements)/sizeof(CONFIG_ELEMENT);
    //Serial.printf("Config file is %d lines long:\n",config_size);
    for (i=0; i<config_size; i++)
    {
      //Serial.printf("%d=%s\n",i,get_config_element(i).c_str());
      if (!(file.print(get_config_element(i).c_str())))
      {
         //Serial.printf("Error saving congig file.");
         return false;
      }
    }
    file.close();
    return true;
}

// Function to generate HTML form from CONFIG_ELEMENT array
std::string generateConfigHTMLForm(char *next_page, char *next_page_desc, char *message) {
    std::stringstream html;
    int i,config_size;
    
    char * page_title="Cardputer Environment Monitor Config";
    html PROGMEM << R"rawliteral(<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" 
    "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd"><html xmlns="http://www.w3.org/1999/xhtml">
    <head><meta content="text/html; charset=utf-8" http-equiv="Content-Type" />)rawliteral";

    html<< "<title>" << page_title <<" </title></head><body><style>html * {font: normal 14px Verdana, Arial, sans-serif;}</style><center><h2>"<< page_title <<"</h2>";
    html << "<form action=\"/submit\" method=\"POST\">\n<table>";

    config_size=sizeof(config_elements)/sizeof(CONFIG_ELEMENT);
    for (i=0;i<config_size;i++)
    {
        html << "<tr><td>  <label for=\"" << config_elements[i].value_name << "\">" << config_elements[i].value_name << ":</label>\n</td><td>";
        
        switch (config_elements[i].value_type) {
            case 'i':  // Integer
                html << "<input type=\"number\" id=\"" << config_elements[i].value_name << "\" name=\"" 
                     << config_elements[i].value_name << "\" value=\"" 
                     << *(static_cast<int*>(config_elements[i].value_ptr)) 
                     << "\" step=\"1\">\n";
                break;
                
            case 'f':  // Float
                html << "  <input type=\"number\" id=\"" << config_elements[i].value_name << "\" name=\"" 
                     << config_elements[i].value_name << "\" value=\"" 
                     << *(static_cast<float*>(config_elements[i].value_ptr)) 
                     << "\" step=\"any\">\n";
                break;
                
            case 's':  // String
                html << "  <input type=\"text\" id=\"" << config_elements[i].value_name << "\" name=\"" 
                     << config_elements[i].value_name << "\" value=\"" 
                     << static_cast<char*>(config_elements[i].value_ptr) 
                     << "\">\n";
                break;
                
            default:
                html << "  <input type=\"text\" id=\"" << config_elements[i].value_name << "\" name=\"" 
                     << config_elements[i].value_name << "\" value=\"\">\n";
                break;
        }
        //html << "  <small>Default: " << config_elements[i].default_value << "</small>\n";
        //html << "  <br><br>\n";
        html << "</td></tr>";     
    }

    html << "</table><br><input type=\"submit\" value=\"Submit\"><br>"<<message<<"\n";
    if (*next_page)
    {
       html << "<a href=\"" << next_page << "\">" << next_page_desc << "</a>";
    }
    html << "</form></center></body></html>\n";
    //Serial.print(html.str().c_str());
    return html.str();
}

// Function to simulate form submission and update config values

void save_form_config_data(WebServer &server) {
        int i,config_size;
        config_size=sizeof(config_elements)/sizeof(CONFIG_ELEMENT);
        for (i=0;i<server.args();i++)
        {
        String paramName = config_elements[i].value_name;
        //Serial.printf("Saving %s = %s\n",server.argName(i).c_str(), server.arg(i).c_str());
        // Check if the form data contains the parameter
        if (server.argName(i) == paramName)  {
            String newValue = server.arg(i);
            
            switch (config_elements[i].value_type) {
                case 'i':  // Integer
                    *(static_cast<int*>(config_elements[i].value_ptr)) = newValue.toInt();
                    break;
                    
                case 'f':  // Float
                    *(static_cast<float*>(config_elements[i].value_ptr)) = newValue.toFloat();
                    break;
                    
                case 's':  // String
                    strcpy(static_cast<char*>(config_elements[i].value_ptr), newValue.c_str());
                    break;
                    
                default:
                    std::cerr << "Unknown type for parameter " << paramName << std::endl;
                    break;
            }
        }
        
    }
}

# BME680Environmental-MonitorM5Cardputer

See: https://www.youtube.com/watch?v=Ygyj-1DjHoc

This software is for the M5 Cardputer (original, but should work on the “advanced” model) and is an environmental monitoring system, saving data to an SD card and displaying it nicely on a webpage.

It uses the BME 680 sensor, connected via the I2C for remote environmental monitoring. The cardputer is set up as a Webserver so that current readings can be seen on a webpage.


This software project is unfinished, but all the main features work well and survive a 24 hour soak test. I hope others can build on it! The code is weakly commented, but I hope it is readable.

Readings of pressure, temp and humidity are taken every minute (frequency not currently configurable) and saved in memory and to a CSV file, stored on the SD card, if one is present. The file is named based on the current date and a new CSV file is created at midnight. 

On the home page, you can click the “Data and Charts” button which will allow you to view the last 24 hours of data and adjust the viewing start and end times for a nice graphical display.
 
Any CSV files saved on the SD card can be downloaded to your local machine.

Another page allows you to configure operational parameters for high and low alarms for temperature, pressure and humidity. An alarm sound is played from a WAV file through the cardputer’s speaker and an indication of the alarm is given on the home page.
 
The Cardputer’s display can show each of the environmental values in turn when you press any key, along with the device’s IP address on the network, once it is connected. The local time is determined by connecting to an NTP server.

The code is built from various examples given on the M5Stack website and forums as well as some other places.

The cardputer software uses 2 HTML files from the root folder of the SD card – “index.html” and “charts without data.html” These have placeholders for various bits of data such that the cardputer software fills them in with the relevant data. The chart data stored in memory is sent as a Javascript array so that the web page, when rendered, shows all the chart data. The chart display code was written by Chatgpt and modified a little to produce the relevant display, with start and end time zoom sliders!

I hope someone uses it to monitor their greenhouse or something!

Of course, you can set up port forwarding on your router and keep an eye on your greenhouse remotely!

Enjoy!

Andrew Johnson
Dec 2025

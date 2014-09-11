/*
Raspberry Pi Weather Station
See weather.gjmccarthy.co.uk for more info.

This code uses the following libraries and snippets and code:

BMP085 code and libraries:
https://www.john.geek.nz/2012/08/reading-data-from-a-bosch-bmp085-with-a-raspberry-pi/
https://learn.adafruit.com/bmp085/using-the-bmp085-api-v2

MCP3008 C Library:
http://gpcf.eu/projects/embedded/adc/

WiringPi:
http://wiringpi.com/

DHT22 Code:
https://github.com/technion/lol_dht22
*/


#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <math.h>
#include <getopt.h>
#include "BMP085/smbus.c"
#include "BMP085/smbus.h" 
#include "BMP085/getBMP085.c"
#include "lol_dht22/common.h"
#include "mcp3008/mcp3008.h"

//Prototype declaration
void readDHT22();

static const char * optString = "v";

static const struct option longOpts[] = {
	{ "version", no_argument, NULL, 'v' },
	{ NULL, no_argument,NULL,0}
};

int read_mcp3008(int channel)
{
	// Channel, Clock, Output, Input, CS 
  int value = mcp3008_value(channel, 11, 9, 10, 8);
  return value;
}

int debug(char *debug_info)
{
	FILE *fp;
        fp=fopen("/tmp/debug.log", "a");
        fprintf(fp, debug_info);
	fprintf(fp, "\n");
	fclose(fp);
	return 0;
}


float calculate_dew_point(float temp, float rel_humidity)
{
  float a = 17.27;
  float b = 237.7;
  float dp;
  float gamma = ((a * temp)/(b+temp)) + log(rel_humidity/100.0);
  if(a == gamma)
    dp = 0;
  else
    dp= (b*gamma)/(a-gamma);
  return dp;
}

float absolute_humidity(float temp, float rh)
{
  float abs_hum = ((6.112 * exp((17.67 * temp)/(temp + 243.5)) * 2.164 * rh) / (273.15 + temp));
  return abs_hum;
}

int main(int argc, char **argv)
{


        debug("Program startup");
	int opt = 0;
	int longIndex = 0;

	opt = getopt_long( argc, argv, optString, longOpts, &longIndex );
	while( opt != -1 ) {
		switch (opt) {
			case 'v':
				printf("Version 1.0\n");
				exit(0);
			default:
				exit(0);
		}
		opt = getopt_long( argc, argv, optString, longOpts, &longIndex );
	}

 	CURL *curl;
        CURLcode res;
        curl_global_init(CURL_GLOBAL_ALL);
	time_t start, stop, curtime;
	float dewpoint;
	float abs_humidity;

	char buffer[200];
        char tempbuffer[30];

	start = time(NULL);
	time(&curtime);
	debug(ctime(&curtime));
 	for(; /* some condition that takes forever to meet */;) {
     			// do stuff that apparently takes forever.
       		stop = time(NULL);
       		double diff = difftime(stop, start);

    		if (diff >= 300) {				// Upload data to internal server 5 mins
                	//printf("3 mins passed - send data...\n");
	                time(&curtime);
        	        debug(ctime(&curtime));

			debug("Write data to mysql");
                	start = time(NULL);

			debug("Reading mcp3008 -chan 0");
			int myval = read_mcp3008(0);			//Read UVI
			debug("done chan 0");
			float vout = myval/1023.0 * 3.3;
			float sensorVoltage = vout / 471.0;
			float millivolts = sensorVoltage * 1000.0;
			float uvi = millivolts * (5.25/20.0);

			debug("Reading mcp3008 - chan 3");
			myval = read_mcp3008(3);				//Read temt6000
			debug("done chan 3");
			vout = myval/1023.0 * 3.3;
			float microAmps = vout * 100.0;			// microamps = Vout/10k (resistor on temt6000) * 1000000
			float Light = microAmps * 2;				//Acording to datasheet graph

			debug("Reading dht22");
			readDHT22();				// Read DHT22 Sensor
			debug("done dht22");
			debug("Reading bmp085");
			bmp085_Calibration();
			debug("done bmp085");
        		temperature = bmp085_GetTemperature(bmp085_ReadUT());
			debug("got temp");
        		pressure = bmp085_GetPressure(bmp085_ReadUP());
			debug("got pressure");
			dewpoint = calculate_dew_point(t, h);
			debug("calc  dewpoint");
			abs_humidity = absolute_humidity(t, h);
			
			debug("init curl");
			curl = curl_easy_init();
	
			char f0[] = "http://suse.home/weather/upload-weather.php?key=qscwdv&";
 			char f1[] = "dewpoint=";
                        char f2[] = "&rel_hum=";
                        char f3[] = "&pressure=";
                        char f4[] = "&bmp085_temp=";
                        char f5[] = "&dht22_temp=";
                        char f6[] = "&abs_hum=";
                        char f7[] = "&rainfall=";
                        char f8[] = "&wind=";
			char f9[] = "&gust=";
                        char f10[] = "&uvi=";
			char f11[] = "&light=";

			if(curl) {
				debug("Creating buffer");

				float Windspeed = 10;
				float Rainfall = 1;

                                float WindGust = 45.12;
        			buffer[0] = '\0';								// Clear Buffers
				tempbuffer[0] = '\0';

				strcat(buffer,f0);

                		snprintf(tempbuffer, sizeof tempbuffer,  "%s%0.2f", f1, dewpoint);			//Dew Point
				strcat(buffer, tempbuffer);
				tempbuffer[0] = '\0';

				snprintf(tempbuffer, sizeof tempbuffer, "%s%0.2f", f2, h);			//Relative Humidity
                		strcat(buffer, tempbuffer);
				tempbuffer[0] = '\0';

                		snprintf(tempbuffer, sizeof tempbuffer, "%s%0.2f", f3, (double)pressure/100);	//Pressure               		strcat(buffer, t2buffer);
                                strcat(buffer, tempbuffer);
                                tempbuffer[0] = '\0';

                		snprintf(tempbuffer, sizeof tempbuffer, "%s%0.2f", f4, (double)temperature/10);	//BMP085 Temperature
				strcat(buffer, tempbuffer);
				tempbuffer[0] = '\0';

 				snprintf(tempbuffer, sizeof tempbuffer, "%s%0.2f", f5, t);			//DHT22 Temperature
                		strcat(buffer, tempbuffer);
				tempbuffer[0] = '\0';

 				snprintf(tempbuffer, sizeof tempbuffer, "%s%0.2f", f6, abs_humidity);		//Abs Humidity
                                strcat(buffer, tempbuffer);
				tempbuffer[0] = '\0';

 				snprintf(tempbuffer, sizeof tempbuffer, "%s%0.2f", f7, Rainfall);		//Rainfall
                                strcat(buffer, tempbuffer);
				tempbuffer[0] = '\0';

				snprintf(tempbuffer, sizeof tempbuffer, "%s%0.2f", f8, Windspeed);		//Windspeed
                                strcat(buffer, tempbuffer);
				tempbuffer[0] = '\0';

     			        snprintf(tempbuffer, sizeof tempbuffer, "%s%0.2f", f9, WindGust);              //Windspeed
                                strcat(buffer, tempbuffer);
                                tempbuffer[0] = '\0';

                                snprintf(tempbuffer, sizeof tempbuffer, "%s%0.2f", f10, uvi);              //UV
                                strcat(buffer, tempbuffer);
                                tempbuffer[0] = '\0';

                                snprintf(tempbuffer, sizeof tempbuffer, "%s%0.2f", f11, Light);       //Wind Gust
                                strcat(buffer, tempbuffer);
                                tempbuffer[0] = '\0';

				debug(buffer);
				
		                curl_easy_setopt(curl, CURLOPT_URL, buffer);
				debug("Open  curl dev null");
                		FILE *f = fopen("/dev/null", "wb");
				debug("send off data");
                		curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
                		res = curl_easy_perform(curl);
               		 	if(res != CURLE_OK)
                        		debug("curl_easy_strerror");

		        	fclose(f);
				debug("reset curl");
				curl_easy_reset(curl);

			}

	}
	}
	return 0;
}
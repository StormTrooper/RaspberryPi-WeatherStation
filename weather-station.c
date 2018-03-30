/*
Raspberry Pi Weather Station
Sends data to Openhab2 using MQTT

This code uses the following libraries and snippets and code:

BMP085 code and libraries:
https://www.john.geek.nz/2012/08/reading-data-from-a-bosch-bmp085-with-a-raspberry-pi/
https://learn.adafruit.com/bmp085/using-the-bmp085-api-v2

MCP3008 C Library:
http://gpcf.eu/projects/embedded/adc/

WiringPi:
http://wiringpi.com/

*/

#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <getopt.h>
#include "BMP085/smbus.c"
#include "BMP085/smbus.h"
#include "BMP085/getBMP085.c"
#include "mcp3008/mcp3008.h"
#include "MQTTClient.h"
#include <time.h>

#define RAIN_PIN 15
#define WIND_PIN 16
#define RAIN_CALIBRATION 0.2794              // 0.2794 mm per tip
#define WIND_CALIBRATION 2.4

#define ADDRESS     "tcp://openhab2.home:1883"
#define CLIENTID    "weatherstation"
#define QOS         1
#define TIMEOUT     10000L
#define TOPIC_temperature       "weather-station/temperature"
#define TOPIC_dewpoint		"weather-station/dewpoint"
#define TOPIC_light		"weather-station/light"
#define TOPIC_uvi          	"weather-station/uvi"
#define TOPIC_rain          	"weather-station/rain"
#define TOPIC_windspeed         "weather-station/windspeed"
#define TOPIC_abs_hum          	"weather-station/abs_hum"
#define TOPIC_pressure		"weather-station/pressure"

float rainCounter;      		// counter for rain guage clicks
float windCounter;
clock_t last_rain_interrupt_time = 0;
clock_t last_wind_interrupt_time = 0;

static const char * optString = "v";
char mystring[50]; 			//size of the number
static const struct option longOpts[] = {
	{ "version", no_argument, NULL, 'v' },
	{ NULL, no_argument,NULL,0}
};


// ======================================================================
// rainInterrupt:  called for rain gauge counter

void rainInterrupt(void) {
        clock_t rain_interrupt_time;
        rain_interrupt_time = clock();

        if (((((float)rain_interrupt_time - (float)last_rain_interrupt_time) / 1000000.0F ) * 1000) >  200)  {
                rainCounter++;
        }
        last_rain_interrupt_time = rain_interrupt_time;
}

// ======================================================================
// windInterrupt

void windInterrupt(void) {
	clock_t wind_interrupt_time;
        wind_interrupt_time = clock();

        if (((((float)wind_interrupt_time - (float)last_wind_interrupt_time) / 1000000.0F ) * 1000) >  5)  {
                windCounter++;
        }
        last_wind_interrupt_time = wind_interrupt_time;

}

//=======================================================================
int read_mcp3008(int channel)
{
	// Channel, Clock, Output, Input, CS 
  int value = mcp3008_value(channel, 11, 9, 10, 8);
  return value;
}

// ======================================================================

#ifdef DEBUG
int debug(char *debug_info)
{
	FILE *fp;
        fp=fopen("/tmp/debug.log", "a");
        fprintf(fp, debug_info);
	fprintf(fp, "\n");
	fclose(fp);
	return 0;
}
#endif

//=======================================================================
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

//=======================================================================
float absolute_humidity(float temp, float rh)
{
  float abs_hum = ((6.112 * exp((17.67 * temp)/(temp + 243.5)) * 2.164 * rh) / (273.15 + temp));
  return abs_hum;
}
//=======================================================================
int main(int argc, char **argv)
{

        FILE *ptr_file;			//File pointer for dht22.txt file
        char dht22_buf[20];
	float t;			//DHT22 temp
	float h;			//DHT22 rel humidity
	int result;			//wiringPi result
	float rainFall;
	float windSpeed;


        #ifdef DEBUG
	debug("=================");
	debug("Program Startup");
	debug("=================");
	#endif

	int opt = 0;
        int longIndex = 0;

        opt = getopt_long( argc, argv, optString, longOpts, &longIndex );
        while( opt != -1 ) {
                switch (opt) {
                        case 'v':
                                printf("Version 1.2\n");
                                exit(0);
                        default:
                                exit(0);
                }
                opt = getopt_long( argc, argv, optString, longOpts, &longIndex );
        }

        float start_time, end_time;

        #ifdef DEBUG
		time_t curtime;
                time(&curtime);
                debug(ctime(&curtime));
        #endif

	result = wiringPiSetup ();
        if (result == -1)
        {
                #ifdef DEBUG
			debug("Error on wiringPiSetup.  weatherstation quitting");
		#endif
                return 0;
        }

	//Setup MQTT
	MQTTClient client;
    	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    	MQTTClient_message pubmsg = MQTTClient_message_initializer;
    	MQTTClient_deliveryToken token;
    	int rc;
    	MQTTClient_create(&client, ADDRESS, CLIENTID,
        	MQTTCLIENT_PERSISTENCE_NONE, NULL);
    	conn_opts.keepAliveInterval = 20;
    	conn_opts.cleansession = 1;
    	if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    	{
            printf("Failed to connect, return code %d\n", rc);
            exit(EXIT_FAILURE);
    	}

	#ifdef DEBUG
               debug("Setup MQTT Complete");
        #endif

	//Setup interrupt
	if ( wiringPiISR (RAIN_PIN, INT_EDGE_BOTH, &rainInterrupt) < 0 ) {
                printf("Unable to setup ISR");
        }

	if ( wiringPiISR (WIND_PIN, INT_EDGE_FALLING, &windInterrupt) < 0 ) {
                printf("Unable to setup ISR");
	}


	float dewpoint;
	float abs_humidity;
	float pressure;

	start_time = time(NULL);

	for(; /* some condition that takes forever to meet */;) {
     			// do stuff that apparently takes forever.

		end_time = time(NULL);
       		double diff_time = difftime(end_time, start_time);


    		if (diff_time >= 60) {				// Upload data to internal server 5 mins
                	//printf("5 mins passed - send data...\n");

			start_time = time(NULL);
			#ifdef DEBUG
				time(&curtime);
				debug(ctime(&curtime));
				debug("Reading mcp3008 -chan 0");
			#endif

			int myval = read_mcp3008(0);			//Read UVI
			#ifdef DEBUG
				debug("done chan 0");
			#endif

			float vout = myval/1023.0 * 3.3;
			float sensorVoltage = vout / 471.0;
			float millivolts = sensorVoltage * 1000.0;
			float uvi = millivolts * (5.25/20.0);

			#ifdef DEBUG
				debug("Reading mcp3008 - chan 3");
			#endif

			myval = read_mcp3008(3);				//Read temt6000

			#ifdef DEBUG
				debug("done chan 3");
			#endif

			vout = myval/1023.0 * 3.3;
			float microAmps = vout * 100.0;			// microamps = Vout/10k (resistor on temt6000) * 1000000
			float Light = microAmps * 2;				//Acording to datasheet graph

			#ifdef DEBUG
				debug("Reading dht22");
			#endif

			// readDHT22();				// Read DHT22 Sensor

			//Call Adafruits python script to read dht22 sensor on pin 4
			//Saves result to test file
			system("/usr/bin/python /home/pi/RaspberryPi-WeatherStation/AdafruitDHT.py 22 4 > /tmp/dht22.txt");

			//Open file and get values
			ptr_file =fopen("/tmp/dht22.txt","r");
			while (fgets(dht22_buf,20, ptr_file)!=NULL)
	                       sscanf(dht22_buf, "%f,%f", &t, &h);
			fclose(ptr_file);

			#ifdef DEBUG
				debug("done dht22");
				debug("Reading bmp085");
			#endif
			bmp085_Calibration();

			#ifdef DEBUG
				debug("done bmp085");
        		#endif


			rainFall = rainCounter*RAIN_CALIBRATION;
			windSpeed = windCounter/4*WIND_CALIBRATION;   //4 pulses per rotation

			float temperature = bmp085_GetTemperature(bmp085_ReadUT());
			temperature = temperature / 10.0;

			#ifdef DEBUG
				debug("got temp");
        		#endif

			pressure = bmp085_GetPressure(bmp085_ReadUP());

			#ifdef DEBUG
				debug("got pressure");
			#endif

			dewpoint = calculate_dew_point(t, h);

			#ifdef DEBUG
				debug("calc  dewpoint");
			#endif

			abs_humidity = absolute_humidity(t, h);

			#ifdef DEBUG
				debug("send data to openhab");
			#endif

		        MQTTClient_create(&client, ADDRESS, CLIENTID,
       	 		        MQTTCLIENT_PERSISTENCE_NONE, NULL);
		        if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
        		{
   	        	 printf("Failed to connect, return code %d\n", rc);
 	       		    exit(EXIT_FAILURE);
  		         }

			sprintf(mystring, "%g", temperature);
			pubmsg.payload = mystring; 
    			pubmsg.payloadlen = strlen(mystring);
    			pubmsg.retained = 0;
    			MQTTClient_publishMessage(client, TOPIC_temperature, &pubmsg, &token);
    			rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
			#ifdef DEBUG
	    			debug("Message with delivery:temperature");
				debug(mystring);
			#endif

			sprintf(mystring, "%0.2g", dewpoint);
			pubmsg.payload = mystring; 
                        pubmsg.payloadlen = strlen(mystring);
			MQTTClient_publishMessage(client, TOPIC_dewpoint, &pubmsg, &token);
                        rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
			#ifdef DEBUG
				debug("Message with delivery:dewpoint");
				debug(mystring);
			#endif

			sprintf(mystring, "%g", pressure);
			pubmsg.payload = mystring; 
                        pubmsg.payloadlen = strlen(mystring);
                        MQTTClient_publishMessage(client, TOPIC_pressure, &pubmsg, &token);
                        rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
                        #ifdef DEBUG
				debug("Message with delivery:pressure");
				debug(mystring);
			#endif

 			sprintf(mystring, "%0.2g", Light);
                        pubmsg.payload = mystring; 
                        pubmsg.payloadlen = strlen(mystring);
                        MQTTClient_publishMessage(client, TOPIC_light, &pubmsg, &token);
                        rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
                        #ifdef DEBUG
				debug("Message with delivery:light");
				debug(mystring);
			#endif

			sprintf(mystring, "%0.2g", uvi);
                        pubmsg.payload = mystring; 
                        pubmsg.payloadlen = strlen(mystring);
                        MQTTClient_publishMessage(client, TOPIC_uvi, &pubmsg, &token);
                        rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
                        #ifdef DEBUG
				debug("Message with delivery:uvi");
				debug(mystring);
			#endif

			sprintf(mystring, "%0.3g", abs_humidity);
                        pubmsg.payload = mystring; 
                        pubmsg.payloadlen = strlen(mystring);
                        MQTTClient_publishMessage(client, TOPIC_abs_hum, &pubmsg, &token);
                        rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
                        #ifdef DEBUG
				debug("Message with delivery:abs_hum");
				debug(mystring);
			#endif

                        sprintf(mystring, "%g", windSpeed);
                        pubmsg.payload = mystring; 
                        pubmsg.payloadlen = strlen(mystring);
                        MQTTClient_publishMessage(client, TOPIC_windspeed, &pubmsg, &token);
                        rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
                        #ifdef DEBUG
				debug("Message with delivery:windspeed");
				debug(mystring);
			#endif


                        sprintf(mystring, "%g", rainFall);
                        pubmsg.payload = mystring; 
                        pubmsg.payloadlen = strlen(mystring);
                        MQTTClient_publishMessage(client, TOPIC_rain, &pubmsg, &token);
                        rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
                        #ifdef DEBUG
				debug("Message with delivery:rain");
				debug(mystring);
			#endif


    			MQTTClient_disconnect(client, 10000);
			MQTTClient_destroy(&client);
			}

	}
	return 0;
}


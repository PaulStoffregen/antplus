// (first draft)
// Testing antplus usb driver for teensy 3.6
//
//	Michael McElligott
//
// This example is in the public domain

#include "usbh_common.h"
#include "antdrv.h"
#include "libant.h"

USBHost myusb;
//USBHub hub1(myusb);


#define ENABLE_SERIALPRINTF		1
#if ENABLE_SERIALPRINTF
#undef printf
#define printf(...) Serial.printf(__VA_ARGS__); Serial.println()
#else
#undef printf
#define printf(...)    
#endif




static int doTests = 0;


static const char channelStatusStr[5][28] = {
	"STATUS UNASSIGNED CHANNEL",
	"STATUS ASSIGNED CHANNEL",
	"STATUS SEARCHING CHANNEL",
	"STATUS TRACKING_CHANNEL",
	"UNKNOWN STATUS STATE"
};


int antp_callback (uint32_t msg, intptr_t *value1, uint32_t value2)
{
	//printf("antp_callback %i %i %i", (int)msg, value1, value2);

	
	if (msg == USBD_MSG_DEVICEREADY){
		doTests = 1;
		return 1;
		
	}else if (msg == ANTP_MSG_CHANNELSTATUS){
		const int channel =(value2&0xF0)>>4;		// one profile per channel: Channel No. equates to Profile No.
		const int status  = value2&0x0F;			// first channel/profile is 0, 2'nd is 1, etc..
		
		printf("Channel %i status: %s", channel, channelStatusStr[status]);
		
		// or process via:
		switch (status){
		  case ANT_CHANNEL_STATUS_UNASSIGNED: break;
		  case ANT_CHANNEL_STATUS_SEARCHING: break;
		  case ANT_CHANNEL_STATUS_ASSIGNED: break;
		  case ANT_CHANNEL_STATUS_TRACKING: break;
		  default: break;	// unknown
		}
	}else if (msg == ANTP_MSG_PROFILE_SELECT){	// select which profiles to enable
		switch (value2){
		  case PROFILE_HRM:{
		  	uint32_t deviceId = 0;				// set your deviceId here. 0 = select nearest device of this type (ie; don't care)
		  	*value1 = (intptr_t)deviceId;
		  	return 1;							// yes, we want this processed
		  }
		  case PROFILE_SPDCAD:{
		  	uint32_t deviceId = 0;				// set your deviceId here. 0 = select nearest device of this type (ie; don't care)
		  	*value1 = (intptr_t)deviceId;
		  	return 1;							// yes, we want this processed also
		  }
		  case PROFILE_POWER:	return 0;
		  case PROFILE_STRIDE:	return 0;
		  case PROFILE_SPEED:	return 0;		// no, because I don't have these devices
		  case PROFILE_CADENCE:	return 0;
		}
		return 0;
		
	}else if (msg == ANTP_MSG_DEVICEID){
		TDEVICET *dev = (TDEVICET*)value1;
		printf("Device found on channel %i: deviceId:%i, deviceType:%i, transType:%i", value2, dev->deviceId, dev->deviceType, dev->transType);

	}else if (msg == ANTP_MSG_PROFILE_DATA){
		switch (value2){
		  case PROFILE_HRM:{
			payload_HRM_t *hrm = (payload_HRM_t*)value1;
			printf("HRM: sequence:%i, interval:%ims, bpm:%i", hrm->current.sequence, hrm->current.interval, hrm->current.bpm);
			break;
		  }
		  case PROFILE_SPDCAD:{
		  	payload_SPDCAD_t *spdcad = (payload_SPDCAD_t*)value1;
		  	// WHEEL_CIRCUMFERENCE should be adjusted as per your wheel size (payloadparser.h)
		  	// or
			//spdcad->wheelCircumference = 2122;   // set your wheel size here, in mm
		  	printf("SPDCAD: speed: %.2fkm/h, cadence: %irpm, total distance: %.2fkm", spdcad->current.speed/100.0f, spdcad->current.cadence, spdcad->current.distance/1000.0f);
			break;
		  }
		  case PROFILE_POWER:{
		  	payload_POWER_t *pwr = (payload_POWER_t*)value1;
		  	(void)pwr;
		  	break;
		  }
		  case PROFILE_STRIDE:{
		  	payload_STRIDE_t *stride = (payload_STRIDE_t*)value1;
		  	(void)stride;
		  	break;
		  }
		  case PROFILE_SPEED:{
		  	payload_SPEED_t *spd = (payload_SPEED_t*)value1;
		  	// WHEEL_CIRCUMFERENCE should be adjusted as per your wheel size (payloadparser.h)
		  	// or
			//spd->wheelCircumference = 2122;   // set your wheel size here, in mm
		  	printf("SPD: speed: %.2fkm/h, total distance: %.2fkm", spd->current.speed/100.0f, spd->current.distance/1000.0f);
		  	break;
		  }
		  case PROFILE_CADENCE:{
		  	payload_CADENCE_t *cad = (payload_CADENCE_t*)value1;
			printf("CAD: cadence: %irpm", cad->current.cadence);
		  	break;
		  }
		}
	}

	return -1;	// no error or no return value/response expected
}

void setup ()
{
	while (!Serial) ; // wait for Arduino Serial Monitor
	
	Serial.println("Ant+ USB");

	antplus_setCallbackFunc(antp_callback);
	antplus_init(KEY_ANTSPORT);
	
}

void loop ()
{
//	static uint32_t time0;
	
	antplus_task();
	
	if (doTests == 1){
		doTests = 2;
		antplus_start();	// we start here
		
#if 0
		time0 = millis();
	
	}else if (doTests == 2){
		if (millis() - time0 > 8000){
			time0 = millis();
			antplus_stop();		// stop all transmissions
			doTests = 3;
		}
	}else if (doTests == 3){
		if (millis() - time0 > 8000){
			doTests = 1;
		}
#endif
	}
}

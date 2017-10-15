
//#include <Arduino.h>
#include <stdint.h>

#include "libant.h"
#include "antdrv.h"
#include "payloadparser.h"



#define ENABLE_SERIALPRINTF		1


#if ENABLE_SERIALPRINTF
#undef printf
#define printf(...) Serial.printf(__VA_ARGS__); Serial.println()
#else
#undef printf
#define printf(...)    
#endif


typedef struct {
	payload_HRM_t		hrm;
	payload_SPDCAD_t	spdcad;
	
	payload_POWER_t		power;
	payload_STRIDE_t	stride;
	payload_SPEED_t		spd;
	payload_CADENCE_t	cad;
}payload_genstorage_t;
static payload_genstorage_t payload;



void payload_HRM (TDCONFIG *cfg, const uint8_t *data, const size_t dataLength, void *userPtr)
{
	payload_HRM_t *hrm = &payload.hrm;
	hrm->current.bpm = data[STREAM_RXBROADCAST_DEV120_HR];
	hrm->current.sequence = data[STREAM_RXBROADCAST_DEV120_SEQ];

	//const int page = data[1]&0x0F;
	if (hrm->previous.sequence != hrm->current.sequence || hrm->previous.bpm != hrm->current.bpm){
		if (hrm->current.bpm){
			hrm->current.time = (data[STREAM_RXBROADCAST_DEV120_BEATLO] + (data[STREAM_RXBROADCAST_DEV120_BEATHI] << 8));
			hrm->current.interval = hrm->current.time - hrm->previous.time;
			hrm->current.interval = (((int32_t)hrm->current.interval) * (int32_t)1000) / (int32_t)1024;

			//printf("page %i", page);
			antplus_sendMessage(ANTP_MSG_PROFILE_DATA, (intptr_t*)hrm, PROFILE_HRM);
	
			hrm->previous.time = hrm->current.time;
			hrm->previous.interval = hrm->current.interval;
			hrm->previous.sequence = hrm->current.sequence;
			hrm->previous.bpm = hrm->current.bpm;
		}
	}

	//int page = data[1]&0x0F;
	//printf("payload_HRM: page:%i, Sequence:%i, BPM:%i, %i %i", page, hrm->current.sequence, hrm->current.bpm, hrm->current.time, hrm->current.interval);
}

void payload_SPDCAD (TDCONFIG *cfg, const uint8_t *data, const size_t dataLength, void *userPtr)
{
	payload_SPDCAD_t *spdcad = &payload.spdcad;
	
	spdcad->current.cadenceTime = data[1];
	spdcad->current.cadenceTime |= (data[2] << 8);
	spdcad->current.cadenceCt = data[3];
	spdcad->current.cadenceCt |= (data[4] << 8);
	spdcad->current.speedTime = data[5];
	spdcad->current.speedTime |= (data[6] << 8);
	spdcad->current.speedCt = data[7];
	spdcad->current.speedCt |= (data[8] << 8);

	spdcad->cadChange = (spdcad->current.cadenceTime != spdcad->previous.cadenceTime || spdcad->current.cadenceCt != spdcad->previous.cadenceCt);
	spdcad->spdChange = (spdcad->current.speedTime != spdcad->previous.speedTime || spdcad->current.speedCt != spdcad->previous.speedCt);

	if (spdcad->cadChange || spdcad->spdChange){
		uint16_t cadence = (60 * (spdcad->current.cadenceCt - spdcad->previous.cadenceCt) * 1024) / (spdcad->current.cadenceTime - spdcad->previous.cadenceTime);
		spdcad->current.cadence = cadence;

		if (!spdcad->wheelCircumference) spdcad->wheelCircumference = WHEEL_CIRCUMFERENCE;
		uint32_t speedRotationDelta = spdcad->current.speedCt - spdcad->previous.speedCt;	// number wheel revolutions
		float speedTimeDelta = (float)(spdcad->current.speedTime - spdcad->previous.speedTime) / 1024.0f;	// time for above revolutions
		float distance = (speedRotationDelta * (float)spdcad->wheelCircumference) / 1000.0f;		// calculated distance (meters) travelled as per above
		float speed = (distance / (speedTimeDelta / 3600.0f)) / 1000.0f;		// its why we're here
		spdcad->current.speed = speed * 100;
		spdcad->current.distance += distance;
		
		antplus_sendMessage(ANTP_MSG_PROFILE_DATA, (intptr_t*)spdcad, PROFILE_SPDCAD);
		
		spdcad->previous.cadenceTime = spdcad->current.cadenceTime;
		spdcad->previous.cadence = spdcad->current.cadence;
		spdcad->previous.cadenceCt = spdcad->current.cadenceCt;

		spdcad->previous.speedTime = spdcad->current.speedTime;
		spdcad->previous.speedCt = spdcad->current.speedCt;
		spdcad->previous.speed = spdcad->current.speed;
		spdcad->previous.distance = spdcad->current.distance;
		
		//printf("payload_SPDCAD: speed: %.2f, cadence: %i, total distance: %.2f", spdcad->current.speed/100.0f, spdcad->current.cadence, spdcad->current.distance/1000.0f);
	}
}

void payload_POWER (TDCONFIG *cfg, const uint8_t *data, const size_t dataLength, void *userPtr)
{
	//printf("payload_POWER: len:%i", dataLength);
	payload_POWER_t *pwr = &payload.power;
/*
	uint8_t eventCount = data[2];
	uint8_t pedalPowerContribution = ((data[3] != 0xFF) && (data[3]&0x80)) ; // left/right is defined if NOT 0xFF (= no Pedal Power) AND BIT 7 is set
	uint8_t pedalPower = (data[3]&0x7F); // right pedalPower % - stored in bit 0-6
	uint8_t instantCadence = data[4];
	uint16_t sumPower = data[5] + (data[6]<<8);
	uint16_t instantPower = data[7] + (data[8]<<8);
        */
	antplus_sendMessage(ANTP_MSG_PROFILE_DATA, (intptr_t*)pwr, PROFILE_POWER);
}

void payload_STRIDE (TDCONFIG *cfg, const uint8_t *data, const size_t dataLength, void *userPtr)
{
	//printf("payload_STRIDE: len:%i", dataLength);
	payload_STRIDE_t *stride = &payload.stride;
	
	
	int page = data[1];
	if (page == 0){
		stride->current.strides = data[7];
		antplus_sendMessage(ANTP_MSG_PROFILE_DATA, (intptr_t*)stride, PROFILE_STRIDE);
		stride->previous.strides = stride->current.strides;

	}else if (page == 1){
		stride->current.speed = ((float)(data[4]&0x0f) + (float)(data[5]/256.0f));
		stride->current.cadence = ((float)data[3] + (float)((data[4] << 4) / 16.0f));
	
		antplus_sendMessage(ANTP_MSG_PROFILE_DATA, (intptr_t*)stride, PROFILE_STRIDE);

		stride->previous.speed = stride->current.speed;
		stride->previous.cadence = stride->current.cadence;
	}
	
}

void payload_SPEED (TDCONFIG *cfg, const uint8_t *data, const size_t dataLength, void *userPtr)
{
	//printf("payload_SPEED: len:%i", dataLength);
	payload_SPEED_t *spd = &payload.spd;
	
	spd->current.speedTime = data[5];
	spd->current.speedTime |= (data[6] << 8);
	spd->current.speedCt = data[7];
	spd->current.speedCt |= (data[8] << 8);
	
	spd->spdChange = (spd->current.speedTime != spd->previous.speedTime || spd->current.speedCt != spd->previous.speedCt);
	
	if (spd->spdChange){
		uint32_t speedRotationDelta = spd->current.speedCt - spd->previous.speedCt;	// number wheel revolutions
		float speedTimeDelta = (float)(spd->current.speedTime - spd->previous.speedTime) / 1024.0f;	// time for above revolutions
		if (!spd->wheelCircumference) spd->wheelCircumference = WHEEL_CIRCUMFERENCE;
		float distance = (speedRotationDelta * (float)spd->wheelCircumference) / 1000.0f;		// calculated distance (meters) travelled as per above
		float speed = (distance / (speedTimeDelta / 3600.0f)) / 1000.0f;		// its why we're here
		spd->current.speed = speed * 100;
		spd->current.distance += distance;
	
		antplus_sendMessage(ANTP_MSG_PROFILE_DATA, (intptr_t*)spd, PROFILE_SPEED);

		spd->previous.speedTime = spd->current.speedTime;
		spd->previous.speedCt = spd->current.speedCt;
		spd->previous.speed = spd->current.speed;
		spd->previous.distance = spd->current.distance;
	}
}

void payload_CADENCE (TDCONFIG *cfg, const uint8_t *data, const size_t dataLength, void *userPtr)
{
	//printf("payload_CADENCE: len:%i", dataLength);
	payload_CADENCE_t *cad = &payload.cad;
	

	cad->current.cadenceTime = data[5];
	cad->current.cadenceTime |= (data[6] << 8);
	cad->current.cadenceCt = data[7];
	cad->current.cadenceCt |= (data[8] << 8);
			
	cad->cadChange = (cad->current.cadenceTime != cad->previous.cadenceTime || cad->current.cadenceCt != cad->previous.cadenceCt);

	if (cad->cadChange){
		uint16_t cadence = (60 * (cad->current.cadenceCt - cad->previous.cadenceCt) * 1024) / (cad->current.cadenceTime - cad->previous.cadenceTime);
		cad->current.cadence = cadence;
	
		antplus_sendMessage(ANTP_MSG_PROFILE_DATA, (intptr_t*)cad, PROFILE_CADENCE);
	
		cad->previous.cadenceTime = cad->current.cadenceTime;
		cad->previous.cadence = cad->current.cadence;
		cad->previous.cadenceCt = cad->current.cadenceCt;
	}
}


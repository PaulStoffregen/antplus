

#include <stdint.h>

#include "usbh_common.h"
#include "libant.h"
#include "antdrv.h"
//#include "payloadparser.h"



#define ENABLE_SERIALPRINTF		1


#if ENABLE_SERIALPRINTF
#undef printf
#define printf(...) Serial.printf(__VA_ARGS__); Serial.println()
#else
#undef printf
#define printf(...)    
#endif


static TLIBANTPLUS _ant;
static TLIBANTPLUS *ant = &_ant;






#if 0
void dump_hexbytes (const uint8_t *buffer, const size_t len)
{
	for (size_t i = 0; i < len; i++)
		printf(" %X", buffer[i]);
	printf("\r\n");
}
#endif

static inline int antplus_dispatchMessage (TLIBANTPLUS *ant, const uint8_t *stream, const int len)
{
	return ant->eventCb[EVENTI_MESSAGE].cbPtr( stream[STREAM_CHANNEL], stream[STREAM_MESSAGE], &stream[STREAM_DATA], (size_t)stream[STREAM_LENGTH], ant->eventCb[EVENTI_MESSAGE].uPtr);
}

static inline void antplus_dispatchPayload (TLIBANTPLUS *ant, TDCONFIG *cfg, const uint8_t *payload, const int len)
{
	ant->payloadCb[cfg->channel].cbPtr(cfg, payload, (size_t)len, ant->payloadCb[cfg->channel].uPtr);
}

static inline const uint8_t *antplus_getAntKey (const uint8_t keyIdx)
{
	if (keyIdx < KEY_TOTAL)
		return antkeys[keyIdx];
	else
		return NULL;
}

static inline uint8_t antplus_calcMsgChecksum (const uint8_t *buffer, const uint8_t len)
{
	uint8_t checksum = 0x00;
	for (uint8_t i = 0; i < len; i++)
		checksum ^= buffer[i];
	return checksum;
}

static inline uint8_t *antplus_findStreamSync (uint8_t *stream, const size_t rlen, int *pos)
{
	// find and sync with input stream
	*pos = 0;
	while (*pos < (int)rlen /*&& *pos < INPUTBUFFERSIZE-3*/){
		if (stream[*pos] == MESG_TX_SYNC)
			return stream + *pos;
		(*pos)++;
	}
	return NULL;
}

static inline int antplus_msgCheckIntegrity (uint8_t *stream, const int len)
{
	// min message length is 5
	if (len < 5) return 0;

	int crc = stream[STREAM_SYNC];
	crc ^= stream[STREAM_LENGTH];
	crc ^= stream[STREAM_MESSAGE];
	int mlen = 0;
	
	do{
		crc ^= stream[STREAM_DATA+mlen];
	}while (++mlen < stream[STREAM_LENGTH]);
		
	//printf("crc == 0x%X: msg crc = 0x%X\n", crc, stream[stream[STREAM_LENGTH] + 3]);
	return (crc == stream[stream[STREAM_LENGTH] + 3]);
}

static inline int antplus_msgGetLength (uint8_t *stream)
{
	// eg; {A4 1 6F 20 EA} = {SYNC DATALEN MSGID DATA CRC}
	return stream[STREAM_LENGTH] + 4;
}

static inline int antplus_handleMessages (TLIBANTPLUS *ant, uint8_t *buffer, int tBytes)
{
	int syncOffset = 0;
	//uint8_t buffer[ANTPLUS_MAXPACKETSIZE];
	uint8_t *stream = buffer;
	
	//int tBytes = antplus_read(ant, buffer, ant->ioVar.readBufferSize);
	//if (tBytes <= 0) return tBytes;

	//int tBytes = ANTPLUS_MAXPACKETSIZE;

	while (tBytes > 0){
		stream = antplus_findStreamSync(stream, tBytes, &syncOffset);
		if (stream == NULL){
			//printf("stream sync not found {size:%i}\n", tBytes);
			return 0;
		}
		tBytes -= syncOffset;

		if (!antplus_msgCheckIntegrity(stream, tBytes)){
			//printf("stream integrity failed {size:%i}\n", tBytes);
			return 0;
		}

		//we have a valid message
		if (antplus_dispatchMessage(ant, stream, tBytes) == -1){
			//printf("quiting..\n");
			return 0;
		}

		int len = antplus_msgGetLength(stream);
		stream += len;
		tBytes -= len;

	}
	return 1;
}

static inline int antplus_registerEventCallback (TLIBANTPLUS *ant, const int which, void *eventFunc, void *userPtr)
{
	if (which < EVENTI_TOTAL){
		ant->eventCb[which].cbPtr = (int (*)(int, int, const uint8_t*, size_t, void*))eventFunc;
		ant->eventCb[which].uPtr = userPtr;
		return 1;
	}
	
	//printf("invalid callback id {%i}\n.", which);
	return 0;
}

static inline int antplus_registerPayloadCallback (TLIBANTPLUS *ant, const int profile, void *eventFunc, void *userPtr)
{
	if (profile < PROFILE_TOTAL){
		ant->payloadCb[profile].cbPtr = (void (*)(TDCONFIG*, const uint8_t*, size_t, void*))eventFunc;
		ant->payloadCb[profile].uPtr = userPtr;
		return 1;
	}
	
	//printf("invalid callback id {%i}\n.", profile);
	return 0;
}

static inline void antplus_sendMessageChannelStatus (TDCONFIG *cfg, const uint32_t channelStatus)
{
	cfg->flags.channelStatus = channelStatus;		
	if (cfg->flags.channelStatus != cfg->flags.channelStatusOld){
		uint32_t status = cfg->flags.channelStatus&0x0F;
		status |= ((cfg->channel&0x0F)<<4);

		antplus_sendMessage(ANTP_MSG_CHANNELSTATUS, NULL, status);
		cfg->flags.channelStatusOld = cfg->flags.channelStatus;
	}
}

void messageCb_channel (const int chan, const int eventId, const uint8_t *payload, const size_t dataLength, void *uPtr)
{
	//printf(" $ chan event: chan:%i, msgId:0x%.2X, payload:%p, dataLen:%i, uPtr:%p", chan, eventId, payload, (int)dataLength, uPtr);
	//dump_hexbytes(payload, dataLength);

	TLIBANTPLUS *ant = (TLIBANTPLUS*)uPtr;
	TDCONFIG *cfg = (TDCONFIG*)&ant->dcfg[chan];
		
	switch (eventId){
	  case EVENT_RX_SEARCH_TIMEOUT:
	  	printf(" $ event RX search timeout");
	  	break;
	  	
	  case EVENT_RX_FAIL:
	  	//printf(" $ event RX fail");
	  	break;
	  	
	  case EVENT_TX:
	  	//printf(" $ event TX");
	  	break;
	  	
	  case EVENT_RX_BROADCAST:
	  	//printf(" $ event RX broadcast ");
	  	if (!cfg->flags.chanIdOnce){
	  		cfg->flags.chanIdOnce = 1;
	  		libantplus_RequestMessage(cfg->channel, MESG_CHANNEL_ID_ID);
	  	}
		//dump_hexbytes(payload, dataLength);
		antplus_dispatchPayload(ant, cfg, payload, dataLength);
		break;
	 }	
}

void messageCb_response (const int chan, const int msgId, const uint8_t *payload, const size_t dataLength, void *uPtr)
{
	//printf(" # response event: msgId:0x%.2X, payload:%p, dataLen:%i, uPtr:%p", msgId, payload, dataLength, uPtr);

	TLIBANTPLUS *ant = (TLIBANTPLUS*)uPtr;
	TDCONFIG *cfg = (TDCONFIG*)&ant->dcfg[chan];
	
	switch (msgId){
	  case MESG_EVENT_ID:
	  	//printf(" * event");
	  	messageCb_channel(chan, payload[STREAM_EVENT_EVENTID], payload, dataLength, uPtr);
	  	break;
	  	
	  case MESG_NETWORK_KEY_ID: 
	  	printf("[%i] * network key accepted", chan);
	  	cfg->flags.keyAccepted = 1;
	  	if (cfg->transType == ANT_TRANSMISSION_MASTER)
	  		libantplus_AssignChannel(cfg->channel, PARAMETER_TX_NOT_RX, cfg->networkNumber);
	  	else
	  		libantplus_AssignChannel(cfg->channel, cfg->channelType, cfg->networkNumber);
	  	break;

	  case MESG_ASSIGN_CHANNEL_ID:
	  	printf("[%i]  * channel assign accepted", chan);
	  	libantplus_SetChannelPeriod(cfg->channel, cfg->channelPeriod);
	  	break; 
	  	
	  case MESG_CHANNEL_MESG_PERIOD_ID:
		printf("[%i]  * channel mesg period accepted", chan);
		libantplus_SetChannelSearchTimeout(cfg->channel, cfg->searchTimeout);
		break;
		
	  case MESG_CHANNEL_SEARCH_TIMEOUT_ID:
	  	printf("[%i]  * search timeout period accepted", chan);
	  	libantplus_SetChannelRFFreq(cfg->channel, cfg->RFFreq);
	  	break;
	  	
	  case MESG_CHANNEL_RADIO_FREQ_ID:
	  	printf("[%i]  * radio freq accepted", chan);
	  	libantplus_SetSearchWaveform(cfg->channel, cfg->searchWaveform);
	  	break;
	  	
	  case MESG_SEARCH_WAVEFORM_ID:
	  	printf("[%i]  * search waveform accepted", chan);
	  	libantplus_SetChannelId(cfg->channel, cfg->deviceNumber, cfg->deviceType, cfg->transType);
	  	break; 
	  	
	  case MESG_CHANNEL_ID_ID:
	  	printf("[%i]  * set channel id accepted", chan);
	  	libantplus_OpenChannel(cfg->channel);
	  	break; 
	  	
	  case MESG_OPEN_CHANNEL_ID:
	  	printf("[%i]  * open channel accepted", chan);
	  	//cfg->flags.channelStatus = 1;
	  	libantplus_RequestMessage(cfg->channel, MESG_CHANNEL_STATUS_ID);
	  	libantplus_RequestMessage(cfg->channel, MESG_CAPABILITIES_ID);
	  	libantplus_RequestMessage(cfg->channel, MESG_VERSION_ID);
	  	break;
	  	
 	  case MESG_UNASSIGN_CHANNEL_ID:
		printf("[%i]  * channel Unassigned", chan);
		break;

	  case MESG_CLOSE_CHANNEL_ID:
		printf("[%i]  * channel CLOSED", chan);
		cfg->flags.keyAccepted = 0;
		antplus_sendMessageChannelStatus(cfg, ANT_CHANNEL_STATUS_UNASSIGNED);

		break;
		
 	  case CHANNEL_IN_WRONG_STATE:          
		printf("[%i]  * channel in wrong state", chan);
		break;

 	  case CHANNEL_NOT_OPENED:              
		printf("[%i]  * channel not opened", chan);
		break;
		
 	  case CHANNEL_ID_NOT_SET: //??             
		printf("[%i]  * channel ID not set", chan);
		break;
		
 	  case CLOSE_ALL_CHANNELS: // Start RX Scan mode              
		printf("[%i]  * close all channels", chan);
		break;
		
 	  case TRANSFER_IN_PROGRESS: // TO ack message ID            
		printf("[%i]  * tranfer in progress", chan);
		break;
		
 	  case TRANSFER_SEQUENCE_NUMBER_ERROR:  
		printf("[%i]  * transfer sequence number error", chan);
		break;
		
 	  case TRANSFER_IN_ERROR:               
		printf("[%i]  * transfer in error", chan);
		break;
		
 	  case INVALID_MESSAGE:                 
		printf("[%i]  * invalid message", chan);
		break;
		
 	  case INVALID_NETWORK_NUMBER:          
		printf("[%i]  * invalid network number", chan);
		break;
		
 	  case INVALID_LIST_ID:                 
		printf("[%i]  * invalid list ID", chan);
		break;
		
 	  case INVALID_SCAN_TX_CHANNEL:         
		printf("[%i]  * invalid Scanning transmit channel", chan);
		break;
		
 	  case INVALID_PARAMETER_PROVIDED:      
		printf("[%i]  * invalid parameter provided", chan);
   		break;
   		
 	  case EVENT_QUE_OVERFLOW:              
		printf("[%i]  * queue overflow", chan);
		break;
		
	  default:
	  	printf("[i] #### unhandled response id %i", chan, msgId);
	};
}

void messageCb_event (const int channel, const int msgId, const uint8_t *payload, const size_t dataLength, void *uPtr)
{
	//printf(" @ msg event cb: Chan:%i, Id:0x%.2X, payload:%p, len:%i, ptr:%p", channel, msgId, payload, (int)dataLength, uPtr);
	//dump_hexbytes(payload, dataLength);

	TLIBANTPLUS *ant = (TLIBANTPLUS*)uPtr;
	uint8_t chan = 0;
	if (channel > 0 && channel < PROFILE_TOTAL);
		chan = channel;
	

	switch(msgId){
	  case MESG_BROADCAST_DATA_ID:
	  	//printf(" @ broadcast data \n");
		//dumpPayload(payload, dataLength);
		messageCb_channel(chan, EVENT_RX_BROADCAST, payload, dataLength, uPtr);
	  	break;


	  case MESG_STARTUP_MESG_ID: 
	  	// reason == ANT_STARTUP_RESET_xxxx
	  	printf(" @ start up mesg reason: 0x%X", payload[STREAM_STARTUP_REASON]);
	  	{
	  		TDCONFIG *cfg = (TDCONFIG*)&ant->dcfg[0];
			//libantplus_SetNetworkKey(cfg->networkNumber, antplus_getAntKey(cfg->keyIdx));
			libantplus_SetNetworkKey(cfg->networkNumber, antplus_getAntKey(ant->key));
		}
		break;
		
	  case MESG_RESPONSE_EVENT_ID:
	  	messageCb_response(payload[STREAM_EVENT_CHANNEL_ID], payload[STREAM_EVENT_RESPONSE_ID], payload, dataLength, uPtr);
	  	break;
	  	
	  case MESG_CHANNEL_STATUS_ID:
	  	//printf(" @ channel status for channel %i is %i", payload[STREAM_CHANNEL_ID], payload[STREAM_CHANNEL_STATUS]);
	  	{
	  		TDCONFIG *cfg = (TDCONFIG*)&ant->dcfg[payload[STREAM_CHANNEL_ID]];
	  		antplus_sendMessageChannelStatus(cfg, payload[STREAM_CHANNELSTATUS_STATUS]&ANT_CHANNEL_STATUS_MASK);
			//if (cfg->flags.channelStatus != STATUS_TRACKING_CHANNEL)
			//	printf("channel %i status: %s",payload[STREAM_CHANNEL_ID], channelStatusStr[cfg->flags.channelStatus]);
		}
	  	break;
	  	
	  case MESG_CAPABILITIES_ID:
		printf(" @ capabilities:");
		printf("   Max ANT Channels: %i",payload[STREAM_CAP_MAXCHANNELS]);
		printf("   Max ANT Networks: %i",payload[STREAM_CAP_MAXNETWORKS]);
		printf("   Std. option: 0x%X",payload[STREAM_CAP_STDOPTIONS]);
		printf("   Advanced: 0x%X",payload[STREAM_CAP_ADVANCED]);
		printf("   Advanced2: 0x%X",payload[STREAM_CAP_ADVANCED2]);
	  	break;

	case MESG_CHANNEL_ID_ID:
		{ 
			TDCONFIG *cfg = (TDCONFIG*)&ant->dcfg[chan];
        	cfg->dev.deviceId = payload[STREAM_CHANNELID_DEVNO_LO] | (payload[STREAM_CHANNELID_DEVNO_HI] << 8);
			cfg->dev.deviceType = payload[STREAM_CHANNELID_DEVTYPE];
			cfg->dev.transType = payload[STREAM_CHANNELID_TRANTYPE];
			//printf(" @ CHANNEL ID: channel %i, deviceId:%i, deviceType:%i, transType:%i)", chan, cfg->dev.deviceId, cfg->dev.deviceType, cfg->dev.transType);
			antplus_sendMessage(ANTP_MSG_DEVICEID, (intptr_t*)&cfg->dev, chan);
			
#if 0
			if (cfg->dev.scidDeviceType != cfg->deviceType){
				printf(" @ CHANNEL ID: this is not the device we're looking for");
				printf(" @ CHANNEL ID: expecting 0x%X but found 0x%X", cfg->deviceType, cfg->dev.scidDeviceType);
         		//libantplus_CloseChannel(cfg->channel);
         		return;
         	}
#endif
		}
		break;
		
	case MESG_VERSION_ID:
		printf(" @ version: '%s'", (char*)&payload[STREAM_VERSION_STRING]);
		break;
	};
}


/*
##############################################################################################################
##############################################################################################################
##############################################################################################################
##############################################################################################################
*/

int libantplus_SetPayloadHandler (const int profile, void *eventFunc, void *userPtr)
{
	return antplus_registerPayloadCallback(ant, profile, eventFunc, userPtr);
}

int libantplus_SetEventHandler (const int which, void *eventFunc, void *userPtr)
{
	return antplus_registerEventCallback(ant, which, eventFunc, userPtr);
}

int libantplus_HandleMessages (uint8_t *buffer, int tBytes)
{
	return antplus_handleMessages(ant, buffer, tBytes);
}

int libantplus_ResetSystem ()
{
	//Serial.println("libantplus_ResetSystem");
	
	uint8_t msg[5];

	msg[0] = MESG_TX_SYNC;			// sync
	msg[1] = 1;						// length
	msg[2] = MESG_SYSTEM_RESET_ID;	// msg id
	msg[3] = 0;						// nop
	msg[4] = antplus_calcMsgChecksum(msg, 4);	

	return antplus_write(ant, msg, 5);
}

int libantplus_RequestMessage (const int channel, const int message)
{
	//Serial.println("libantplus_RequestMessage");
	
	uint8_t msg[6];

	msg[0] = MESG_TX_SYNC;			// sync
	msg[1] = 2;						// length
	msg[2] = MESG_REQUEST_ID;		// msg id
	msg[3] = (uint8_t)channel;
	msg[4] = (uint8_t)message;
	msg[5] = antplus_calcMsgChecksum(msg, 5);	

	return antplus_write(ant, msg, 6);
}

int libantplus_SetNetworkKey (const int netNumber, const uint8_t *key)
{
	uint8_t msg[13];
	
	msg[0] = MESG_TX_SYNC;
	msg[1] = 9;
	msg[2] = MESG_NETWORK_KEY_ID;
	msg[3] = (uint8_t)netNumber;
	msg[4] = key[0];
	msg[5] = key[1];
	msg[6] = key[2];
	msg[7] = key[3];
	msg[8] = key[4];
	msg[9] = key[5];
	msg[10] = key[6];
	msg[11] = key[7];
	msg[12] = antplus_calcMsgChecksum(msg, 12); 			// xor checksum
	
	return antplus_write(ant, msg, 13);
}

int libantplus_SetChannelSearchTimeout (const int channel, const int searchTimeout)
{
	uint8_t msg[6];

	msg[0] = MESG_TX_SYNC;			// sync
	msg[1] = 2;						// length
	msg[2] = MESG_CHANNEL_SEARCH_TIMEOUT_ID;
	msg[3] = (uint8_t)channel;
	msg[4] = (uint8_t)searchTimeout;
	msg[5] = antplus_calcMsgChecksum(msg, 5);	

	// send the message
	return antplus_write(ant, msg, 6);	
}

int libantplus_SetChannelPeriod (const int channel, const int period)
{
	uint8_t msg[7];

	msg[0] = MESG_TX_SYNC;			// sync
	msg[1] = 3;						// length
	msg[2] = MESG_CHANNEL_MESG_PERIOD_ID;
	msg[3] = (uint8_t)channel;
	msg[4] = (uint8_t)(period & 0xFF);
	msg[5] = (uint8_t)(period >> 8);
	msg[6] = antplus_calcMsgChecksum(msg, 6);	

	// send the message
	return antplus_write(ant, msg, 7);
}

int libantplus_SetChannelRFFreq (const int channel, const int freq)
{
	uint8_t msg[6];

	msg[0] = MESG_TX_SYNC;			// sync
	msg[1] = 2;						// length
	msg[2] = MESG_CHANNEL_RADIO_FREQ_ID;
	msg[3] = (uint8_t)channel;
	msg[4] = (uint8_t)freq;
	msg[5] = antplus_calcMsgChecksum(msg, 5);	

	// send the message
	return antplus_write(ant, msg, 6);
}

int libantplus_SetSearchWaveform (const int channel, const int wave)
{
	uint8_t msg[7];

	msg[0] = MESG_TX_SYNC;			// sync
	msg[1] = 3;						// length
	msg[2] = MESG_SEARCH_WAVEFORM_ID;		// msg id
	msg[3] = (uint8_t)channel;
	msg[4] = (uint8_t)wave & 0xFF;
	msg[5] = (uint8_t)wave >> 8;
	msg[6] = antplus_calcMsgChecksum(msg, 6);	

	// send the message
	return antplus_write(ant, msg, 7);
}

int libantplus_OpenChannel (const int channel)
{
	uint8_t msg[5];

	msg[0] = MESG_TX_SYNC;			// sync
	msg[1] = 1;						// length
	msg[2] = MESG_OPEN_CHANNEL_ID;	// msg id
	msg[3] = (uint8_t)channel;
	msg[4] = antplus_calcMsgChecksum(msg, 4);	

	// send the message
	return antplus_write(ant, msg, 5);
}

int libantplus_CloseChannel (const int channel)
{
	uint8_t msg[5];

	msg[0] = MESG_TX_SYNC;			// sync
	msg[1] = 1;						// length
	msg[2] = MESG_CLOSE_CHANNEL_ID;	// msg id
	msg[3] = (uint8_t)channel;
	msg[4] = antplus_calcMsgChecksum(msg, 4);	

	// send the message
	return antplus_write(ant, msg, 5);
}

int libantplus_AssignChannel (const int channel, const int channelType, const int network)
{
	uint8_t msg[7];

	msg[0] = MESG_TX_SYNC;
	msg[1] = 3;
	msg[2] = MESG_ASSIGN_CHANNEL_ID;
	msg[3] = (uint8_t)channel;
	msg[4] = (uint8_t)channelType;
	msg[5] = (uint8_t)network;
	msg[6] = antplus_calcMsgChecksum(msg, 6);	

	// send the message
	return antplus_write(ant, msg, 7);
}

int libantplus_SetChannelId (const int channel, const int deviceNum, const int deviceType, const int transmissionType)
{
	uint8_t msg[9];

	msg[0] = MESG_TX_SYNC;
	msg[1] = 5;
	msg[2] = MESG_CHANNEL_ID_ID;
	msg[3] = (uint8_t)channel;
	msg[4] = (uint8_t)(deviceNum & 0xFF);
	msg[5] = (uint8_t)(deviceNum >> 8);
	msg[6] = (uint8_t)deviceType;
	msg[7] = (uint8_t)transmissionType;
	msg[8] = antplus_calcMsgChecksum(msg, 8);	

	return antplus_write(ant, msg, 9);
}

int libantplus_SendBurstTransferPacket (const int channelSeq, const uint8_t *data)
{
	uint8_t msg[13];
	
	msg[0] = MESG_TX_SYNC;
	msg[1] = 9;
	msg[2] = MESG_BURST_DATA_ID;
	msg[3] = (uint8_t)channelSeq;
	msg[4] = data[0];
	msg[5] = data[1];
	msg[6] = data[2];
	msg[7] = data[3];
	msg[8] = data[4];
	msg[9] = data[5];
	msg[10] = data[6];
	msg[11] = data[7];
	msg[12] = antplus_calcMsgChecksum(msg, 12); 			// xor checksum
	
	return antplus_write(ant, msg, 13);
}

int libantplus_SendBurstTransfer (const int channel, const uint8_t *data, const int nunPackets)
{
	int ret = 0;
	int seq = 0;
	
	for (int i = 0; i < nunPackets; i++){
		if (i == nunPackets-1) seq |= 0x04;
		ret = libantplus_SendBurstTransferPacket((seq<<5) | (channel&0x1F), data+(i<<3));
		seq = (seq+1)&0x03;
	}
	return ret;
}

int libantplus_SendBroadcastData (const int channel, const uint8_t *data)
{
	uint8_t msg[13];
	
	msg[0] = MESG_TX_SYNC;
	msg[1] = 9;
	msg[2] = MESG_BROADCAST_DATA_ID;
	msg[3] = (uint8_t)channel;
	msg[4] = data[0];
	msg[5] = data[1];
	msg[6] = data[2];
	msg[7] = data[3];
	msg[8] = data[4];
	msg[9] = data[5];
	msg[10] = data[6];
	msg[11] = data[7];
	msg[12] = antplus_calcMsgChecksum(msg, 12);
	
	return antplus_write(ant, msg, 13);
}

int libantplus_SendAcknowledgedData (const int channel, const uint8_t *data)
{
	uint8_t msg[13];
	
	msg[0] = MESG_TX_SYNC;
	msg[1] = 9;
	msg[2] = MESG_ACKNOWLEDGED_DATA_ID;
	msg[3] = (uint8_t)channel;
	msg[4] = data[0];
	msg[5] = data[1];
	msg[6] = data[2];
	msg[7] = data[3];
	msg[8] = data[4];
	msg[9] = data[5];
	msg[10] = data[6];
	msg[11] = data[7];
	msg[12] = antplus_calcMsgChecksum(msg, 12);
	
	return antplus_write(ant, msg, 13);
}

int libantplus_SendExtAcknowledgedData (const int channel, const int devNum, const int devType, const int TranType, const uint8_t *data)
{
	uint8_t msg[17];
	
	msg[0] = MESG_TX_SYNC;
	msg[1] = 13;
	msg[2] = MESG_EXT_ACKNOWLEDGED_DATA_ID;
	msg[3] = (uint8_t)channel;
	msg[4] = (uint8_t)(devNum & 0xFF);
	msg[5] = (uint8_t)(devNum >> 8);
	msg[6] = (uint8_t)devType;
	msg[7] = (uint8_t)TranType;
	msg[8] = data[0];
	msg[9] = data[1];
	msg[10] = data[2];
	msg[11] = data[3];
	msg[12] = data[4];
	msg[13] = data[5];
	msg[14] = data[6];
	msg[15] = data[7];
	msg[16] = antplus_calcMsgChecksum(msg, 16);
	
	return antplus_write(ant, msg, 17);
}

int libantplus_SendExtBroadcastData (const int channel, const int devNum, const int devType, const int TranType, const uint8_t *data)
{
	uint8_t msg[17];
	
	msg[0] = MESG_TX_SYNC;
	msg[1] = 13;
	msg[2] = MESG_EXT_BROADCAST_DATA_ID;
	msg[3] = (uint8_t)channel;
	msg[4] = (uint8_t)(devNum & 0xFF);
	msg[5] = (uint8_t)(devNum >> 8);
	msg[6] = (uint8_t)devType;
	msg[7] = (uint8_t)TranType;
	msg[8] = data[0];
	msg[9] = data[1];
	msg[10] = data[2];
	msg[11] = data[3];
	msg[12] = data[4];
	msg[13] = data[5];
	msg[14] = data[6];
	msg[15] = data[7];
	msg[16] = antplus_calcMsgChecksum(msg, 16);
	
	return antplus_write(ant, msg, 17);
}

int libantplus_SendExtBurstTransferPacket (const int chanSeq, const int devNum, const int devType, const int TranType, const uint8_t *data)
{
	uint8_t msg[17];
	
	msg[0] = MESG_TX_SYNC;
	msg[1] = 13;
	msg[2] = MESG_EXT_BROADCAST_DATA_ID;
	msg[3] = (uint8_t)chanSeq;
	msg[4] = (uint8_t)(devNum & 0xFF);
	msg[5] = (uint8_t)(devNum >> 8);
	msg[6] = (uint8_t)devType;
	msg[7] = (uint8_t)TranType;
	msg[8] = data[0];
	msg[9] = data[1];
	msg[10] = data[2];
	msg[11] = data[3];
	msg[12] = data[4];
	msg[13] = data[5];
	msg[14] = data[6];
	msg[15] = data[7];
	msg[16] = antplus_calcMsgChecksum(msg, 16);
	
	return antplus_write(ant, msg, 17);
}

int libantplus_SendExtBurstTransfer (const int channel, const int devNum, const int devType, const int tranType, const uint8_t *data, const int nunPackets)
{
	int ret = 0;
	int seq = 0;
	
	for (int i = 0; i < nunPackets; i++){
		if (i == nunPackets-1) seq |= 0x04;
		ret = libantplus_SendExtBurstTransferPacket((seq<<5) | (channel&0x1F), devNum, devType, tranType, data+(i<<3));
		seq = (seq+1)&0x03;
	}
	return ret;
}

const uint8_t *libantplus_GetNetworkKey (const uint8_t keyIdx)
{
	return antplus_getAntKey(keyIdx);
}

void profileSetup_HRM (TDCONFIG *cfg, const uint32_t deviceId)
{
	cfg->deviceNumber = deviceId;		// 0 
	cfg->deviceType = ANT_DEVICE_HRM;
	cfg->transType = ANT_TRANSMISSION_SLAVE;
	cfg->channelType = ANT_CHANNEL_TYPE_SLAVE;
	cfg->networkNumber = 0;
	cfg->channel = PROFILE_HRM;
	cfg->channelPeriod = ANT_PERIOD_HRM;
	cfg->RFFreq = ANT_FREQUENCY_SPORT;
	cfg->searchTimeout = 255;
	cfg->searchWaveform = 0x53;//316;//0x53;
	//cfg->keyIdx = KEY_ANTSPORT;

	cfg->flags.chanIdOnce = 0;
	cfg->flags.channelStatus = ANT_CHANNEL_STATUS_UNASSIGNED;
	cfg->flags.channelStatusOld = 0xFF;
	cfg->flags.keyAccepted = 0;
	cfg->flags.profileValid = 1;
}

void profileSetup_SPDCAD (TDCONFIG *cfg, const uint32_t deviceId)
{
	cfg->deviceNumber = deviceId;		// 0 
	cfg->deviceType = ANT_DEVICE_SPDCAD;
	cfg->transType = ANT_TRANSMISSION_SLAVE;		// 5
	cfg->channelType = ANT_CHANNEL_TYPE_SLAVE;
	cfg->networkNumber = 0;
	cfg->channel = PROFILE_SPDCAD;
	cfg->channelPeriod = ANT_PERIOD_SPDCAD; 
	cfg->RFFreq = ANT_FREQUENCY_SPORT;
	cfg->searchTimeout = 255;
	cfg->searchWaveform = 0x53;
	//cfg->keyIdx = KEY_ANTSPORT;

	cfg->flags.chanIdOnce = 0;
	cfg->flags.channelStatus = ANT_CHANNEL_STATUS_UNASSIGNED;
	cfg->flags.channelStatusOld = 0xFF;
	cfg->flags.keyAccepted = 0;
	cfg->flags.profileValid = 1;
}

void profileSetup_POWER (TDCONFIG *cfg, const uint32_t deviceId)
{
	cfg->deviceNumber = deviceId;		// 0 
	cfg->deviceType = ANT_DEVICE_POWER;
	cfg->transType = ANT_TRANSMISSION_SLAVE;		// 5
	cfg->channelType = ANT_CHANNEL_TYPE_SLAVE;
	cfg->networkNumber = 0;
	cfg->channel = PROFILE_POWER;
	cfg->channelPeriod = ANT_PERIOD_POWER; 
	cfg->RFFreq = ANT_FREQUENCY_SPORT;
	cfg->searchTimeout = 255;
	cfg->searchWaveform = 0x53;
	//cfg->keyIdx = KEY_ANTSPORT;

	cfg->flags.chanIdOnce = 0;
	cfg->flags.channelStatus = ANT_CHANNEL_STATUS_UNASSIGNED;
	cfg->flags.channelStatusOld = 0xFF;
	cfg->flags.keyAccepted = 0;
	cfg->flags.profileValid = 1;
}

void profileSetup_STRIDE (TDCONFIG *cfg, const uint32_t deviceId)
{
	cfg->deviceNumber = deviceId;		// 0 
	cfg->deviceType = ANT_DEVICE_STRIDE;
	cfg->transType = ANT_TRANSMISSION_SLAVE;		// 5
	cfg->channelType = ANT_CHANNEL_TYPE_SLAVE;
	cfg->networkNumber = 0;
	cfg->channel = PROFILE_STRIDE;
	cfg->channelPeriod = ANT_PERIOD_STRIDE; 
	cfg->RFFreq = ANT_FREQUENCY_STRIDE;
	cfg->searchTimeout = 255;
	cfg->searchWaveform = 0x53;
	//cfg->keyIdx = KEY_ANTSPORT;

	cfg->flags.chanIdOnce = 0;
	cfg->flags.channelStatus = ANT_CHANNEL_STATUS_UNASSIGNED;
	cfg->flags.channelStatusOld = 0xFF;
	cfg->flags.keyAccepted = 0;
	cfg->flags.profileValid = 1;
}

void profileSetup_SPEED (TDCONFIG *cfg, const uint32_t deviceId)
{
	cfg->deviceNumber = deviceId;		// 0 
	cfg->deviceType = ANT_DEVICE_SPEED;
	cfg->transType = ANT_TRANSMISSION_SLAVE;		// 5
	cfg->channelType = ANT_CHANNEL_TYPE_SLAVE;
	cfg->networkNumber = 0;
	cfg->channel = PROFILE_SPEED;
	cfg->channelPeriod = ANT_PERIOD_SPEED; 
	cfg->RFFreq = ANT_FREQUENCY_SPORT;
	cfg->searchTimeout = 255;
	cfg->searchWaveform = 0x53;
	//cfg->keyIdx = KEY_ANTSPORT;

	cfg->flags.chanIdOnce = 0;
	cfg->flags.channelStatus = ANT_CHANNEL_STATUS_UNASSIGNED;
	cfg->flags.channelStatusOld = 0xFF;
	cfg->flags.keyAccepted = 0;
	cfg->flags.profileValid = 1;
}

void profileSetup_CADENCE (TDCONFIG *cfg, const uint32_t deviceId)
{
	cfg->deviceNumber = deviceId;		// 0 
	cfg->deviceType = ANT_DEVICE_CADENCE;
	cfg->transType = ANT_TRANSMISSION_SLAVE;		// 5
	cfg->channelType = ANT_CHANNEL_TYPE_SLAVE;
	cfg->networkNumber = 0;
	cfg->channel = PROFILE_CADENCE;
	cfg->channelPeriod = ANT_PERIOD_CADENCE; 
	cfg->RFFreq = ANT_FREQUENCY_SPORT;
	cfg->searchTimeout = 255;
	cfg->searchWaveform = 0x53;
	//cfg->keyIdx = KEY_ANTSPORT;

	cfg->flags.chanIdOnce = 0;
	cfg->flags.channelStatus = ANT_CHANNEL_STATUS_UNASSIGNED;
	cfg->flags.channelStatusOld = 0xFF;
	cfg->flags.keyAccepted = 0;
	cfg->flags.profileValid = 1;
}

/*
uint64_t factory_passkey (uint64_t device_id, uint8_t *buffer)
{
	uint64_t n = (((uint64_t)device_id ^ 0x7d215abb) << 32) + ((uint64_t)device_id ^ 0x42b93f06);

	for (uint8_t i = 0; i < 8; i++)
		buffer[i] = n >> (8*i)&0xFF;

	return n;
}

*/
int libantplus_Start ()
{
#if 0
	uint8_t buffer[8];
	factory_passkey(3825666043, buffer);
	dump_hexbytes(buffer, 8);
#endif

	int ct = 0;
	uint32_t deviceId;
	
	for (int i = 0; i < PROFILE_TOTAL; i++){
		deviceId = 0;
		if (antplus_sendMessage(ANTP_MSG_PROFILE_SELECT, (intptr_t*)&deviceId, i) != 1)
			continue;
		
		ct++;	
		//printf("enabling profile %i", i);
		
		switch (i){
		  case PROFILE_HRM: 
			profileSetup_HRM(&ant->dcfg[PROFILE_HRM], deviceId);
			libantplus_SetPayloadHandler(PROFILE_HRM, (void*)payload_HRM, (void*)NULL);
			break;
		  case PROFILE_SPDCAD: 
			profileSetup_SPDCAD(&ant->dcfg[PROFILE_SPDCAD], deviceId);
			libantplus_SetPayloadHandler(PROFILE_SPDCAD, (void*)payload_SPDCAD, (void*)NULL);
			break;
		  case PROFILE_POWER: 
			profileSetup_POWER(&ant->dcfg[PROFILE_POWER], deviceId);
			libantplus_SetPayloadHandler(PROFILE_POWER, (void*)payload_POWER, (void*)NULL);
			break;
		  case PROFILE_STRIDE: 
			profileSetup_STRIDE(&ant->dcfg[PROFILE_STRIDE], deviceId);
			libantplus_SetPayloadHandler(PROFILE_STRIDE, (void*)payload_STRIDE, (void*)NULL);
			break;
		  case PROFILE_SPEED: 
			profileSetup_SPEED(&ant->dcfg[PROFILE_SPEED], deviceId);
			libantplus_SetPayloadHandler(PROFILE_SPEED, (void*)payload_SPEED, (void*)NULL);
			break;
		  case PROFILE_CADENCE: 
			profileSetup_CADENCE(&ant->dcfg[PROFILE_CADENCE], deviceId);
			libantplus_SetPayloadHandler(PROFILE_CADENCE, (void*)payload_CADENCE, (void*)NULL);
			break;
		}
	}
	
	return ct;
}


TLIBANTPLUS *libantplus_Init (const uint8_t networkKey)
{
	if (networkKey >= KEY_TOTAL){
		//printf("libantplus_Init(): invalid networkKey (%i)");
		//return NULL;
		ant->key = KEY_DEFAULT;
	}else{
		ant->key = networkKey;
	}

	libantplus_SetEventHandler(EVENTI_MESSAGE, (void*)messageCb_event, (void*)ant);
	return ant;
}
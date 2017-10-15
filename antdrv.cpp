

#include <Arduino.h>
#include "USBHost_t36.h"
//#include "usbh_common.h"
#include "antdrv.h"
#include "libant.h"
#include <TimerThree.h>






#define ENABLE_SERIALPRINTF		1
#if ENABLE_SERIALPRINTF
#undef printf
#define printf(...) Serial.printf(__VA_ARGS__); Serial.println()
#else
#undef printf
#define printf(...)    
#endif



extern USBHost myusb;
static ANTDRV antdrv(myusb);
TLIBANTPLUS *ant;



static uint8_t readBuffer[READ_BUFFERTOTAL][READ_BUFFERLEN];
static int readBufferIdx = 0;




void ANTDRV::Task ()
{
	//println("ANTDRV Task:  enum_state = ", device->enum_state);
	
	if (device->enum_state == USBH_ENUMSTATE_END){
		if (!driverReadySignalled){
			driverReadySignalled = millis();
		}else if (millis() - driverReadySignalled > 500){	// give device time to start
			device->enum_state++;
			driverReady();
		}
	}
}

void ANTDRV::init ()
{
	antdrv.state = 0;
	callbackFunc = NULL;
	contribute_Pipes(mypipes, sizeof(mypipes)/sizeof(Pipe_t));
	contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	
	driver_ready_for_device(this);
}

int antplus_sendMessage (uint32_t msg, intptr_t *value1, uint32_t value2)
{
	if (antdrv.callbackFunc)
		return (*antdrv.callbackFunc)(msg, value1, value2);
	return -1;
}

void ANTDRV::driverReady ()
{
	//println("ANTDRV driverReady  = ", (uint32_t)this, HEX);

	antplus_sendMessage(USBD_MSG_DEVICEREADY, NULL, 0);
}

bool ANTDRV::claim (Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{
	//println();
	//println("ANTDRV claim(): this = ", (uint32_t)this, HEX);
	//println("claimType = ", type);
	//println("idVendor = ", dev->idVendor, HEX);
	//println("idProduct = ", dev->idProduct, HEX);

	if (dev->idVendor != ANTPLUS_VID || (dev->idProduct != ANTPLUS_2_PID && dev->idProduct != ANTPLUS_M_PID)){
		//println("  device is not an ANTPLUS device");
		return false;
	}else{
		//println("  found an ANTPLUS stick");
	}
	
	const uint8_t *p = descriptors;
	const uint8_t *end = p + len;

	// http://www.beyondlogic.org/usbnutshell/usb5.shtml
	int descriptorLength = p[0];
	int descriptorType = p[1];
	//println("descriptorType = ", descriptorType, HEX);
	//println("descriptorLength = ", descriptorLength);

	if (!descriptorLength || descriptorType != USBH_DESCRIPTORTYPE_INTERFACE /*|| descriptorLength != 9*/)
		return false;

	descriptor_interface_t *interface = (descriptor_interface_t*)&p[0];
	//println("bInterfaceClass = ", interface->bInterfaceClass);
	//println("bInterfaceSubClass = ", interface->bInterfaceSubClass);
	if (interface->bInterfaceClass != USBH_DEVICECLASS_VENDOR || interface->bInterfaceSubClass != 0)
		return false;
	
	// only claim at interface level
	if (type != 1) return false;
	//println("  interface: ANTPLUS");
	
	p += descriptorLength;	// == sizeof(descriptor_interface_t)
	rx_ep = 0;
	tx_ep = 0;
	//int interfaceCt = 0;

	while (p < end){
		int interfaceLength = p[0];
		if (p + interfaceLength > end) return false; // reject if beyond end of data
		int interfaceType = p[1];

		//println(" ");		
		//println("interface number : ", interfaceCt++);
		//println("interfaceType = ", interfaceType, HEX);
		//println("interfaceLength = ", interfaceLength);
		
		if (interfaceType == USBH_DESCRIPTORTYPE_ENDPOINT){
			descriptor_endpoint_t *endpoint = (descriptor_endpoint_t*)&p[0];
			
			//println("bEndpointAddress = ", endpoint->bEndpointAddress, HEX);
			//println("bmAttributes = ", endpoint->bmAttributes, HEX);
			//println("wMaxPacketSize = ", endpoint->wMaxPacketSize);
			//println("  bInterval = ", endpoint->bInterval);

			uint8_t pipeType = endpoint->bmAttributes&0x03;
			uint8_t pipeDir = (endpoint->bEndpointAddress&0x80) >> 7;
			
			// type: 0 = Control, 1 = Isochronous, 2 = Bulk, 3 = Interrupt
			//println("  endpoint type : ", pipeType);
			//println("  endpoint dir  : ", pipeDir);
			//println("  endpoint number : ", (endpoint->bEndpointAddress&0x0F), HEX);

			if (endpoint->bEndpointAddress == ANTPLUS_EP_OUT){			// for data writes
				tx_ep = endpoint->bEndpointAddress&0x0F;
				tx_size = endpoint->wMaxPacketSize;

				txpipe = new_Pipe(dev, pipeType, tx_ep, pipeDir, tx_size, endpoint->bInterval);
				if (txpipe){
					txpipe->callback_function = NULL;
					device = dev;
					//println("txpipe device = ", (uint32_t)device, HEX);
				}
			}else if (endpoint->bEndpointAddress == ANTPLUS_EP_IN){		// for input (reading)
				rx_ep = endpoint->bEndpointAddress&0x0F;
				rx_size = endpoint->wMaxPacketSize;
				
				rxpipe = new_Pipe(dev, pipeType, rx_ep, pipeDir, rx_size, endpoint->bInterval);
				if (rxpipe){
					rxpipe->callback_function = rx_callback;
					device = dev;
					//println("rxpipe device = ", (uint32_t)device, HEX);
				}
			}
		}

		p += interfaceLength;
	}

	//println("@  endpoint txpipe : ", (uint32_t)txpipe, HEX);
	//println("@  endpoint rxpipe  : ", (uint32_t)rxpipe, HEX);
	
	// claim if either pipe created
	return (rxpipe || txpipe);
}

void dump_hexbytes (uint8_t *buffer, const size_t len)
{
	for (size_t i = 0; i < len; i++){
		//printf(" %X", buffer[i]);
		Serial.print(buffer[i], HEX);
		Serial.print(" ");
	}
	Serial.println();
}

void ANTDRV::rx_callback (const Transfer_t *transfer)
{
	const int length = transfer->length - QTD_LENGTH(transfer->qtd.token);
	if (length < 1) return;

	//printf("rx_callback(), len %i", length);
	//dump_hexbytes((uint8_t*)transfer->buffer, length);

	if (transfer->driver)
		libantplus_HandleMessages((uint8_t*)transfer->buffer, length);
}

void ANTDRV::control (const Transfer_t *transfer)
{
	//Serial.println("ANTDRV control()");
}

void ANTDRV::disconnect ()
{
	//Serial.println("ANTDRV disconnect()");
	
	// TODO: free resources
	device = NULL;
	driverReadySignalled = 0;
	writesMade = 0;
	lastRead = 0;
	state = 0;
}

int ANTDRV::writeData (const void *data, const size_t size)
{
	__disable_irq();
	int ret = usb_bulk_write(this, txpipe, data, size);
	__enable_irq();
	return ret;
}

int ANTDRV::readData (const void *data, const size_t size)
{
	__disable_irq();
	int ret = usb_bulk_read(this, rxpipe, data, size);
	__enable_irq();
	return ret;
}
	


/*
###############################################################################################################
###############################################################################################################
###############################################################################################################
###############################################################################################################
*/

void antplus_setCallbackFunc (int (*func)(uint32_t msg, intptr_t *value1, uint32_t value2))
{
	antdrv.callbackFunc = func;
}

void antplus_task ()
{
	myusb.Task();

	if (antdrv.writesMade){
		//printf("antplus_task() %i", antdrv.writesMade);
		
		memset(readBuffer[readBufferIdx], 0, antdrv.rx_size);
		antdrv.writesMade -= antplus_read(NULL, readBuffer[readBufferIdx], antdrv.rx_size);
		++readBufferIdx &= 0x0F;
		antdrv.lastRead = millis();
		
	}else{
		if (!antdrv.state) return;
		
		if ((millis() - antdrv.lastRead > 250) && antdrv.lastRead && ant){
			//printf("antplus_task() %i", antdrv.writesMade);
			//int foundEnabledValid = 0;
			
			for (int i = 0; i < PROFILE_TOTAL; i++){
				TDCONFIG *cfg = &ant->dcfg[i];
				//foundEnabledValid += cfg->flags.profileValid;
				
				if (cfg->flags.profileValid){
					//printf("#### %i %i: %i %i %i ####", i, cfg->channel, cfg->flags.channelStatus, cfg->flags.keyAccepted, cfg->flags.chanIdOnce);
					
					if (cfg->flags.channelStatus){
						libantplus_RequestMessage(cfg->channel, MESG_CHANNEL_STATUS_ID);
					}else{
						libantplus_AssignChannel(cfg->channel, cfg->channelType, cfg->networkNumber);
						libantplus_RequestMessage(cfg->channel, MESG_CHANNEL_STATUS_ID);
						
						if (!cfg->flags.keyAccepted && !cfg->flags.chanIdOnce)
							libantplus_SetNetworkKey(cfg->networkNumber, libantplus_GetNetworkKey(ant->key));
					}
				}
			}
			//if (!foundEnabledValid){
			//	// do something, like quit
			//}
		}
	}
}

// if available, send one message per 8ms
void writeCtrlInterrupt ()
{
	uint32_t timeDeltaLongest = 0;
	uint32_t transferIdx = 0xFF;
	uint32_t time1 = micros();
	
	for (uint32_t i = 0; i < ANT_TRANSFER_BUFFERS; i++){
		volatile aptrans_t *transfer = &antdrv.prendingWrites[i];
		if (transfer->ready){
			uint32_t timeDelta = time1 - transfer->time;
			if (timeDelta >= WRITE_WAITPERIOD && timeDelta >= timeDeltaLongest){
				timeDeltaLongest = timeDelta;
				transferIdx = i;
			}
		}
	}

	if (transferIdx != 0xFF){
		volatile aptrans_t *transfer = &antdrv.prendingWrites[transferIdx];
		antdrv.writeData((const void*)transfer->data, transfer->len);
		__disable_irq();
		transfer->ready = 0;
		__enable_irq();
		delayMicroseconds(25);
		antdrv.writesMade++;
	}
}

#if 1
// non blocking
// this works because we limit writes to once per PERIODIC_LIST_SIZE (8ms)
int antplus_write (void *unused, const uint8_t *buffer, const size_t len)
{
	//printf("antplus_write(): %i", len);
	
	if (len > ANT_TRANSFER_BUFFERSIZE){
		printf("antplus_write(): length too large %i", len);
		return 0;
	}

	for (uint32_t i = 0; i < ANT_TRANSFER_BUFFERS; i++){
		volatile aptrans_t *transfer = &antdrv.prendingWrites[i];
		if (!transfer->ready){
			transfer->time = micros();
			transfer->len = len;
			memcpy((void*)transfer->data, buffer, len);
			__disable_irq();
			transfer->ready = 1;
			__enable_irq();
			return 1;
		}
	}
	return 0;
}

#else
// blocking
int antplus_write (void *unused, const uint8_t *buffer, const size_t len)
{
	const int ret = antdrv.writeData(buffer, len);
	//Serial.printf("antplus_write(): ret %i, len %i", ret, (int)len);
	//Serial.println();
	delay(WRITE_WAITPERIOD);
	antdrv.writesMade++;
	return ret;
}
#endif

int antplus_read (void *unused, uint8_t *buffer, const size_t len)
{
	const int total = antdrv.readData(buffer, len);
	//if (total){
	//	printf("antplus_read(): ret %i, len %i", total, (int)len);
	//}
	return total;
}

int antplus_init (const uint8_t key)
{
	myusb.begin();
	Timer3.initialize(WRITE_WAITPERIOD>>1);
	return (ant=libantplus_Init(key)) != NULL;
}

int antplus_start ()
{
	antdrv.state = 1;
	
	Timer3.attachInterrupt(writeCtrlInterrupt);
	libantplus_ResetSystem();
	return libantplus_Start();
}

void antplus_stop ()
{
	for (int i = 0; i < PROFILE_TOTAL; i++){
		if (ant->dcfg[i].flags.channelStatus != ANT_CHANNEL_STATUS_UNASSIGNED)
			libantplus_CloseChannel(ant->dcfg[i].channel);
	}
	
	delay(50);	// proably isn't required
	Timer3.detachInterrupt();
	antdrv.state = 0;
}


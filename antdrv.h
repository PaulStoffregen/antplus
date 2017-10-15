

#ifndef _ANTDRV_H_
#define _ANTDRV_H_


#include "usbh_common.h"
#include "USBHost_t36.h"
#include "libant.h"



#define READ_BUFFERTOTAL			16
#define READ_BUFFERLEN				ANTPLUS_MAXPACKETSIZE		// must match maxpacketsize
#define WRITE_WAITPERIOD			PERIODIC_LIST_SIZE
#define ANT_TRANSFER_BUFFERSIZE		22		// length of each write buffer
#define ANT_TRANSFER_BUFFERS		16		// size of write queue


typedef struct{
	uint32_t time;		// time registered, send after (this time + WRITE_WAITPERIOD)
	uint8_t data[ANT_TRANSFER_BUFFERSIZE];
	uint8_t ready;		// ready to send
	uint8_t len;
}aptrans_t;


class ANTDRV : public USBDriver {
public:
	enum { SYSEX_MAX_LEN = 60 };
	
	ANTDRV (USBHost &host)
	{
		init();
	}
	ANTDRV (USBHost *host)
	{
		init();
	}

	int writeData (const void *data, const size_t size);
	int readData (const void *data, const size_t size);
	
	void setCallbackFunc (int (*func)(uint32_t msg, intptr_t *value1, uint32_t value2))
	{
		callbackFunc = func;
	}
	
	int (*callbackFunc) (uint32_t msg, intptr_t *value1, uint32_t value2);
	tx_descript_t tx_writeCtx;
	
	uint16_t rx_size;
	uint32_t lastRead;
	int writesMade = 0;
	int state = 0;

	volatile aptrans_t prendingWrites[ANT_TRANSFER_BUFFERS];
	
protected:

	virtual void driverReady ();

	virtual void Task ();
	virtual bool claim (Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
	virtual void control (const Transfer_t *transfer);
	virtual void disconnect ();
	
	static void tx_callback (const Transfer_t *transfer);	
	static void rx_callback (const Transfer_t *transfer);

	void init();
	
	

private:
	Device_t *device;
	Pipe_t *rxpipe;
	Pipe_t *txpipe;

	int driverReadySignalled;

	uint16_t tx_size;
	uint8_t tx_ep;
	uint8_t rx_ep;			

	Pipe_t mypipes[4] __attribute__ ((aligned(32)));
	Transfer_t mytransfers[32] __attribute__ ((aligned(32)));

};

int antplus_init (const uint8_t networkKey);
int antplus_start ();
void antplus_stop ();
void antplus_task ();
void antplus_setCallbackFunc (int (*func)(uint32_t msg, intptr_t *value1, uint32_t value2));
int antplus_sendMessage (uint32_t msg, intptr_t *value1, uint32_t value2);


int antplus_write (void *unused, const uint8_t *buffer, const size_t len);	
int antplus_read (void *unused, uint8_t *buffer, const size_t len);






#endif


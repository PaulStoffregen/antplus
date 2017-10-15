
#ifndef _PAYLOADPARSER_H_
#define _PAYLOADPARSER_H_


#define WHEEL_CIRCUMFERENCE			2122


typedef struct {
	struct {
		uint16_t time;
		uint16_t interval;
		uint8_t bpm;				// heart rate in beats per minute
		uint8_t sequence;
	}current;
	
	struct {
		uint8_t bpm;
		uint8_t sequence;
		uint16_t time;
		uint16_t interval;
	}previous;
}payload_HRM_t;

typedef struct {
	struct {
		uint16_t cadenceTime;
		uint16_t cadence;
		uint16_t cadenceCt;
		uint16_t speedTime;
		uint16_t speed;
		uint16_t speedCt;
		uint32_t distance;
	}current;

	struct {
		uint16_t cadenceTime;
		uint16_t cadence;
		uint16_t cadenceCt;
		uint16_t speedTime;
		uint16_t speed;
		uint16_t speedCt;
		uint32_t distance;
	}previous;

	uint16_t wheelCircumference;		// default is WHEEL_CIRCUMFERENCE (2122cm)
	uint8_t spdChange;
	uint8_t cadChange;
}payload_SPDCAD_t;



typedef struct {
	struct {
	uint8_t sequence;
	uint16_t pedalPowerContribution;
	uint8_t pedalPower;
	uint8_t instantCadence;
	uint16_t sumPower;
	uint16_t instantPower;
	}current;
	
	struct {
		uint16_t stub;
	}previous;
}payload_POWER_t;

typedef struct {
	struct {
		uint16_t speed;
		uint16_t cadence;
		uint8_t strides;
	}current;
	
	struct {
		uint8_t strides;
		uint16_t speed;
		uint16_t cadence;
	}previous;
}payload_STRIDE_t;

typedef struct {
	struct {
		uint16_t speedTime;
		uint16_t speed;
		uint16_t speedCt;
		uint32_t distance;
	}current;

	struct {
		uint16_t speedTime;
		uint16_t speed;
		uint16_t speedCt;
		uint32_t distance;
	}previous;

	uint16_t wheelCircumference;		// default is WHEEL_CIRCUMFERENCE (2122cm)
	uint8_t spdChange;
}payload_SPEED_t;

typedef struct {
	struct {
		uint16_t cadenceTime;
		uint16_t cadence;
		uint16_t cadenceCt;
	}current;

	struct {
		uint16_t cadenceTime;
		uint16_t cadence;
		uint16_t cadenceCt;
	}previous;

	uint8_t cadChange;
}payload_CADENCE_t;


void payload_HRM (TDCONFIG *cfg, const uint8_t *payLoad, const size_t dataLength, void *userPtr);
void payload_SPDCAD (TDCONFIG *cfg, const uint8_t *payLoad, const size_t dataLength, void *userPtr);

void payload_POWER (TDCONFIG *cfg, const uint8_t *payLoad, const size_t dataLength, void *userPtr);
void payload_STRIDE (TDCONFIG *cfg, const uint8_t *payLoad, const size_t dataLength, void *userPtr);
void payload_SPEED (TDCONFIG *cfg, const uint8_t *payLoad, const size_t dataLength, void *userPtr);
void payload_CADENCE (TDCONFIG *cfg, const uint8_t *payLoad, const size_t dataLength, void *userPtr);



// defined in https://github.com/GoldenCheetah/GoldenCheetah/blob/master/src/ANT/
//======================================================================
// ANT SPORT MESSAGE FORMATS
//======================================================================

/* The channel type the message was received upon generally indicates which kind of broadcast message
 * we are dealing with (4e is an ANT broadcast message). So in the ANTMessage constructor we also
 * pass the channel type to help decide how to decode. See interpret_xxx_broadcast() class members
 *
 * Channel       Message
 * type          Type                Message ID ...
 * ---------     -----------         --------------------------------------------------------------------------
 * heartrate     heart_rate          0x4e,channel,None,None,None,None,uint16_le_diff:measurement_time,
 *                                                                    uint8_diff:beats,uint8:instant_heart_rate
 *
 * speed         speed               0x4e,channel,None,None,None,None,uint16_le_diff:measurement_time,
 *                                                                    uint16_le_diff:wheel_revs
 *
 * cadence       cadence             0x4e,channel,None,None,None,None,uint16_le_diff:measurement_time,
 *                                                                    uint16_le_diff:crank_revs
 *
 * speed_cadence speed_cadence       0x4e,channel,uint16_le_diff:cadence_measurement_time,uint16_le_diff:crank_revs,
 *                                                uint16_le_diff:speed_measurement_time,uint16_le_diff:wheel_revs
 *
 * power         calibration_request None,channel,0x01,0xAA,None,None,None,None,None,None
 * power         srm_zero_response   None,channel,0x01,0x10,0x01,None,None,None,uint16_be:offset
 * power         calibration_pass    None,channel,0x01,0xAC,uint8:autozero_status,None,None,None,uint16_le:calibration_data
 * power         calibration_fail    None,channel,0x01,0xAF,uint8:autozero_status,None,None,None,uint16_le:calibration_data
 * power         torque_support      None,channel,0x01,0x12,uint8:sensor_configuration,sint16_le:raw_torque,
 *                                                     sint16_le:offset_torque,None
 * power         standard_power      0x4e,channel,0x10,uint8_diff:event_count,uint8:pedal_power,uint8:instant_cadence,
 *                                                     uint16_le_diff:sum_power,uint16_le:instant_power
 * power         wheel_torque        0x4e,channel,0x11,uint8_diff:event_count,uint8:wheel_rev,uint8:instant_cadence,
 *                                                     uint16_le_diff:wheel_period,uint16_le_diff:torque
 * power         crank_torque        0x4e,channel,0x12,uint8_diff:event_count,uint8:crank_rev,uint8:instant_cadence,
 *                                                     uint16_le_diff:crank_period,uint16_le_diff:torque
 * power         te_and_ps           0x4e,channel,0x13,uint8_diff:event_count,uint8:left_torque_effectivness, uint8:right_torque_effectiveness,
 *                                                     uint8:left_pedal_smoothness, uint8:right_pedal_smoothness
 *                                                     uint16_le_diff:crank_period,uint16_le_diff:torque
 * power         crank_SRM           0x4e,channel,0x20,uint8_diff:event_count,uint16_be:slope,uint16_be_diff:crank_period,
 *                                                     uint16_be_diff:torque
 *
 * any           manufacturer        0x4e,channel,0x50,None,None,hw_rev,uint16_le:manufacturer_id,uint16_le:model_number_id
 * any           product             0x4e,channel,0x51,None,None,sw_rev,uint16_le:serial_number_qpod,
 *                                                                      uint16_le:serial_number_spider
 * any           battery_voltage     0x4e,channel,0x52,None,None,operating_time_lsb,operating_time_1sb,
 *                                                operating_time_msb,voltage_lsb,descriptive_bits
*/

#endif


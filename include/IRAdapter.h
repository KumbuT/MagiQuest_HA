#ifndef IR_ADAPTER_H
#define IR_ADAPTER_H

#include "Globals.h"

#include <IRremote.hpp>

inline void IrInit() {
	IrReceiver.begin(IR_RECV_PIN, false);
}

inline bool IrDecodeAvailable() {
	return IrReceiver.decode();
}

inline bool IrIsMagiQuest() {
	return IrReceiver.decodedIRData.protocol == MAGIQUEST;
}

inline uint16_t IrGetMagnitude() {
	return IrReceiver.decodedIRData.command;
}

inline uint32_t IrGetWandID() {
	if (IrIsMagiQuest()) {
		return (uint32_t)IrReceiver.decodedIRData.decodedRawData;
	}
	return IrReceiver.decodedIRData.address;
}

inline void IrRememberLastSeenWand() {
	lastDetectedWandId = IrGetWandID();
	lastDetectedMagnitude = IrGetMagnitude();
	lastDetectedAtMs = millis();
	hasLastDetectedWand = true;
}

inline void IrResume() {
	IrReceiver.resume();
}

inline void printIrReceiver() {
	Serial.println("-----------------------------RECV START-----------------------------");
	Serial.print("Protocol = ");
	Serial.println(IrReceiver.decodedIRData.protocol);
	Serial.print("WandID = ");
	Serial.println(IrGetWandID());
	Serial.print("Magnitude = ");
	Serial.println(IrReceiver.decodedIRData.command, DEC);
	Serial.println("-----------------------------RECV END-----------------------------");
}

#endif // IR_ADAPTER_H

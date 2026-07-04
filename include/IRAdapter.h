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

inline uint16_t IrGetMagnitude() {
	return IrReceiver.decodedIRData.command;
}

inline uint16_t IrGetWandID() {
	return IrReceiver.decodedIRData.address;
}

inline void IrResume() {
	IrReceiver.resume();
}

inline void printIrReceiver() {
	Serial.println("-----------------------------RECV START-----------------------------");
	Serial.print("Protocol = ");
	Serial.println(IrReceiver.decodedIRData.protocol);
	Serial.print("WandID = ");
	Serial.println(IrReceiver.decodedIRData.address);
	Serial.print("Magnitude = ");
	Serial.println(IrReceiver.decodedIRData.command, DEC);
	Serial.println("-----------------------------RECV END-----------------------------");
}

#endif // IR_ADAPTER_H

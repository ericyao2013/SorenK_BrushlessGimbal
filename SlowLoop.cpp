#include "Globals.h"
#include "Definitions.h"
#include "Board.h"
#include "RCdecode.h"
#include "Mavlink.h"

uint8_t interfaceState;

// This is supposed to read the switch.
void updateGimbalState() {
	// if (switchPos < 0) balahblah .. implement switch to state logic here.
	// except when in autosetup, oops.
	if (interfaceState != INTERFACE_STATE_AUTOSETUP)
		gimbalState = PIDS_ARE_OUTPUT | MOTORS_POWERED;
}

inline bool checkMediumLoop() {
	uint8_t sreg = SREG;
	cli();
	bool result = mediumTaskHasRun;
	mediumTaskHasRun = false;
	SREG = sreg;
	return result;
}

extern void transientTask();
extern void oscillationTask();
extern void runAutosetup();
extern void debug();

void slowLoop() {
	static uint8_t humanDebugDivider;
	static uint8_t GUIDebugDivider;
	//static uint8_t RCDivider;
	//static uint8_t softstartDivider;
	static uint8_t LEDDivider;
	static uint8_t heartbeatDivider;
	//static uint8_t accMagDivider;
	static uint8_t oscDivider;
	static uint16_t transientsDivider;

	while (true) {
		bool ticked = checkMediumLoop();

		/* No need for this, it did more harm than good.
		 if (ticked && !accMagDivider) {
		 accMagDivider = ACCMAG_LATCH;
		 imu.updateAccMagnitude();
		 }
		 */
		if (ticked) {
			// Evaluate RC-Signals. Out of laziness, we ignore these if MAVLink is controlling.
			if (interfaceState != INTERFACE_STATE_MAVLINK) {
				evaluateRCControl();
				evaluateRCSwitch();
			}
			updateGimbalState();
		}

		if (interfaceState == INTERFACE_STATE_CONSOLE)
			sCmd.readSerial();
#ifdef SUPPORT_AUTOSETUP
		else if (interfaceState == INTERFACE_STATE_AUTOSETUP)
			runAutosetup();
#endif
		else if (interfaceState == INTERFACE_STATE_MAVLINK) {
			if (mavlink_parse()) {
				LEDEvent(LED_MAVLINK_RX);
			}
		}

		if (interfaceState == INTERFACE_STATE_CONSOLE && ticked && !humanDebugDivider) {
			humanDebugDivider = HUMAN_DEBUG_LATCH;
			debug();
		}

		if (interfaceState == INTERFACE_STATE_GUI && ticked && !GUIDebugDivider) {
			GUIDebugDivider = GUI_DEBUG_LATCH;
			GUIDebug();
		}

		if (ticked && !heartbeatDivider) {
			LEDEvent(LED_HEARTBEAT_MASK);
			heartbeatDivider = LED_LATCH*2;
		}

		if (ticked && !LEDDivider) {
			LEDDivider = LED_LATCH;
			if (LEDFlags & config.LEDMask) {
				LED_PORT |= (1<<LED_BIT);
				LEDFlags &= ~config.LEDMask;
			} else {
				LED_PORT &= ~(1<<LED_BIT);
			}
		}

		if (ticked && !oscDivider) {
			oscDivider = OSCILLATION_LATCH;
			oscillationTask();
		}

		if (ticked && !transientsDivider) {
			transientsDivider = MEDIUMLOOP_FREQ; // 1Hz
			transientTask();
		}
#ifdef STACKHEAPCHECK_ENABLE
		stackHeapEval(false);
#endif

		if (ticked) {
			// --accMagDivider;
			--humanDebugDivider;
			--GUIDebugDivider;
			//--softstartDivider;
			--LEDDivider;
			--heartbeatDivider;
			--oscDivider;
			--transientsDivider;
		}

		doubleFault = false; // If we have run the mainloop successfully, we reset the double WDT fault status.
	}
}

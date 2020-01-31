/*
Nintendo Switch Fightstick - Proof-of-Concept

Based on the LUFA library's Low-Level Joystick Demo
	(C) Dean Camera
Based on the HORI's Pokken Tournament Pro Pad design
	(C) HORI

This project implements a modified version of HORI's Pokken Tournament Pro Pad
USB descriptors to allow for the creation of custom controllers for the
Nintendo Switch. This also works to a limited degree on the PS3.

Since System Update v3.0.0, the Nintendo Switch recognizes the Pokken
Tournament Pro Pad as a Pro Controller. Physical design limitations prevent
the Pokken Controller from functioning at the same level as the Pro
Controller. However, by default most of the descriptors are there, with the
exception of Home and Capture. Descriptor modification allows us to unlock
these buttons for our use.
*/

/** \file
 *
 *  Main source file for the Joystick demo. This file contains the main tasks of the demo and
 *  is responsible for the initial application hardware configuration.
 */

#include "Joystick.h"

/*
The following ButtonMap variable defines all possible buttons within the
original 13 bits of space, along with attempting to investigate the remaining
3 bits that are 'unused'. This is what led to finding that the 'Capture'
button was operational on the stick.
*/
uint16_t ButtonMap[16] = {
	0x01,
	0x02,
	0x04,
	0x08,
	0x10,
	0x20,
	0x40,
	0x80,
	0x100,
	0x200,
	0x400,
	0x800,
	0x1000,
	0x2000,
	0x4000,
	0x8000,
};

/*** Debounce ****
The following is some -really bad- debounce code. I have a more robust library
that I've used in other personal projects that would be a much better use
here, especially considering that this is a stick indented for use with arcade
fighters.

This code exists solely to actually test on. This will eventually be replaced.
**** Debounce ***/
// Quick debounce hackery!
// We're going to capture each port separately and store the contents into a 32-bit value.
uint32_t pb_debounce = 0;
uint32_t pd_debounce = 0;

// We also need a port state capture. We'll use a 16-bit value for this.
uint16_t bd_state = 0;

// We'll also give us some useful macros here.
#define PINB_DEBOUNCED ((bd_state >> 0) & 0xFF)
#define PIND_DEBOUNCED ((bd_state >> 8) & 0xFF)

// So let's do some debounce! Lazily, and really poorly.
void debounce_ports(void) {
	// We'll shift the current value of the debounce down one set of 8 bits. We'll also read in the state of the pins.
	pb_debounce = (pb_debounce << 8) + PINB;
	pd_debounce = (pd_debounce << 8) + PIND;

	// We'll then iterate through a simple for loop.
	for (int i = 0; i < 8; i++) {
		if ((pb_debounce & (0x1010101 << i)) == (0x1010101 << i)) // wat
			bd_state |= (1 << i);
		else if ((pb_debounce & (0x1010101 << i)) == (0))
			bd_state &= ~(uint16_t)(1 << i);

		if ((pd_debounce & (0x1010101 << i)) == (0x1010101 << i))
			bd_state |= (1 << (8 + i));
		else if ((pd_debounce & (0x1010101 << i)) == (0))
			bd_state &= ~(uint16_t)(1 << (8 + i));
	}
}

#define WAIT 4
#define WAIT_PRESS 4
#define SHINY_FRAME 3486
int frame = 1;
int end_month = 0;

// Main entry point.
int main(void) {
	// We'll start by performing hardware and peripheral setup.
	SetupHardware();
	// We'll then enable global interrupts for our use.
	GlobalInterruptEnable();
	// Once that's done, we'll enter an infinite loop.
	for (;;)
	{
		if (frame > (SHINY_FRAME - 4)) {// 4 frames out
			for (;;) {} //when done enter infinite empty loop
		}
		// We need to run our task to process and deliver data for our IN and OUT endpoints.
		HID_Task();
		// We also need to run the main USB management task.
		USB_USBTask();
		// As part of this loop, we'll also run our bad debounce code.
		// Optimally, we should replace this with something that fires on a timer.
		debounce_ports();
	}
}

// Configures hardware and peripherals, such as the USB peripherals.
void SetupHardware(void) {
	// We need to disable watchdog if enabled by bootloader/fuses.
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	// We need to disable clock division before initializing the USB hardware.
	clock_prescale_set(clock_div_1);
	// We can then initialize our hardware and peripherals, including the USB stack.

	// Both PORTD and PORTB will be used for handling the buttons and stick.
	DDRD  &= ~0xFF;
	PORTD |=  0xFF;

	DDRB  &= ~0xFF;
	PORTB |=  0xFF;
	// The USB stack should be initialized last.
	USB_Init();
}

// Fired to indicate that the device is enumerating.
void EVENT_USB_Device_Connect(void) {
	// We can indicate that we're enumerating here (via status LEDs, sound, etc.).
}

// Fired to indicate that the device is no longer connected to a host.
void EVENT_USB_Device_Disconnect(void) {
	// We can indicate that our device is not ready (via status LEDs, sound, etc.).
}

// Fired when the host set the current configuration of the USB device after enumeration.
void EVENT_USB_Device_ConfigurationChanged(void) {
	bool ConfigSuccess = true;

	// We setup the HID report endpoints.
	ConfigSuccess &= Endpoint_ConfigureEndpoint(JOYSTICK_OUT_EPADDR, EP_TYPE_INTERRUPT, JOYSTICK_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(JOYSTICK_IN_EPADDR, EP_TYPE_INTERRUPT, JOYSTICK_EPSIZE, 1);

	// We can read ConfigSuccess to indicate a success or failure at this point.
}

// Process control requests sent to the device from the USB host.
void EVENT_USB_Device_ControlRequest(void) {
	// We can handle two control requests: a GetReport and a SetReport.
	switch (USB_ControlRequest.bRequest)
	{
		// GetReport is a request for data from the device.
		case HID_REQ_GetReport:
			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				// We'll create an empty report.
				USB_JoystickReport_Input_t JoystickInputData;
				// We'll then populate this report with what we want to send to the host.
				GetNextReport(&JoystickInputData);
				// Since this is a control endpoint, we need to clear up the SETUP packet on this endpoint.
				Endpoint_ClearSETUP();
				// Once populated, we can output this data to the host. We do this by first writing the data to the control stream.
				Endpoint_Write_Control_Stream_LE(&JoystickInputData, sizeof(JoystickInputData));
				// We then acknowledge an OUT packet on this endpoint.
				Endpoint_ClearOUT();
			}

			break;
		case HID_REQ_SetReport:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				// We'll create a place to store our data received from the host.
				USB_JoystickReport_Output_t JoystickOutputData;
				// Since this is a control endpoint, we need to clear up the SETUP packet on this endpoint.
				Endpoint_ClearSETUP();
				// With our report available, we read data from the control stream.
				Endpoint_Read_Control_Stream_LE(&JoystickOutputData, sizeof(JoystickOutputData));
				// We then send an IN packet on this endpoint.
				Endpoint_ClearIN();
			}

			break;
	}
}

// Process and deliver data from IN and OUT endpoints.
void HID_Task(void) {
	// If the device isn't connected and properly configured, we can't do anything here.
	if (USB_DeviceState != DEVICE_STATE_Configured)
	  return;

	// We'll start with the OUT endpoint.
	Endpoint_SelectEndpoint(JOYSTICK_OUT_EPADDR);
	// We'll check to see if we received something on the OUT endpoint.
	if (Endpoint_IsOUTReceived())
	{
		// If we did, and the packet has data, we'll react to it.
		if (Endpoint_IsReadWriteAllowed())
		{
			// We'll create a place to store our data received from the host.
			USB_JoystickReport_Output_t JoystickOutputData;
			// We'll then take in that data, setting it up in our storage.
			Endpoint_Read_Stream_LE(&JoystickOutputData, sizeof(JoystickOutputData), NULL);
			// At this point, we can react to this data.
			// However, since we're not doing anything with this data, we abandon it.
		}
		// Regardless of whether we reacted to the data, we acknowledge an OUT packet on this endpoint.
		Endpoint_ClearOUT();
	}

	// We'll then move on to the IN endpoint.
	Endpoint_SelectEndpoint(JOYSTICK_IN_EPADDR);
	// We first check to see if the host is ready to accept data.
	if (Endpoint_IsINReady())
	{
		// We'll create an empty report.
		USB_JoystickReport_Input_t JoystickInputData;
		// We'll then populate this report with what we want to send to the host.
		GetNextReport(&JoystickInputData);
		// Once populated, we can output this data to the host. We do this by first writing the data to the control stream.
		Endpoint_Write_Stream_LE(&JoystickInputData, sizeof(JoystickInputData), NULL);
		// We then send an IN packet on this endpoint.
		Endpoint_ClearIN();

		/* Clear the report data afterwards */
		// memset(&JoystickInputData, 0, sizeof(JoystickInputData));
	}
}

// script states
typedef enum {
	PRESS_A,
	MOVE_LEFT,
	MOVE_UP,
	PRESS_OK
} State_t;
State_t state = PRESS_A; // initial state
int release = 0;
// buffer report
USB_JoystickReport_Input_t last_report;
int wait_time = 0;
int count = 0;

// Prepare the next report for the host.
void GetNextReport(USB_JoystickReport_Input_t* const ReportData) {
	memset(ReportData, 0, sizeof(USB_JoystickReport_Input_t));
	ReportData->LX = 128;
	ReportData->LY = 128;
	ReportData->RX = 128;
	ReportData->RY = 128;
	ReportData->HAT = 0x08;

	// Resends the same report till wait time is over
	if (wait_time > 0)
	{
		memcpy(ReportData, &last_report, sizeof(USB_JoystickReport_Input_t));
		wait_time--;
		return;
	}

	// script
	switch(state)
	{
		case PRESS_A: // do 1x
			if (release == 0)
			{
				release = 1; // if button was not pressed before set to be pressed
			}
			else
			{
				release = 0; // release button
				break;
			}
			if (count > 0) // repeat button press 1 time
			{
				state = MOVE_LEFT; //move on to next state
				count = 0; // reset counter
			}
			else
			{
				ReportData->Button |= SWITCH_A; // press button
				count++; // update counter
			}
			break;

		case MOVE_LEFT: // do 5x
			if (release == 0)
			{
				release = 1;
			}
			else
			{
				release = 0;
				break;
			}
			if (count > 5)
			{
				state = MOVE_UP;
				count = 0;
			}
			else
			{
				ReportData->HAT = LEFT;
				count++;
			}
			break;

		case MOVE_UP: // do 1x
			ReportData->HAT = UP;
			state = PRESS_OK;
			break;

		case PRESS_OK: // do 6x
			if (release == 0)
			{
				release = 1;
			}
			else
			{
				release = 0;
				break;
			}
			if (count > 6)
			{
				if (end_month < 30) // accounts for end of month
				{
					frame++; // update frame skip amount
					end_month++;
				}
				else
				{
					end_month = 0;
				}
				state = PRESS_A;
				count = 0;
			}
			else
			{
				ReportData->Button |= SWITCH_A;
				count++;
			}
			break;
	}

	// Saves report data to buffer report
	memcpy(&last_report, ReportData, sizeof(USB_JoystickReport_Input_t));
	// Sets wait_time depending on what stage auto hoster is up to
	switch(state)
	{
		case PRESS_A:
			wait_time = WAIT_PRESS;
			break;
		case PRESS_OK:
			wait_time = WAIT_PRESS;
			break;
		default:
			wait_time = WAIT;
			break;
	}
}

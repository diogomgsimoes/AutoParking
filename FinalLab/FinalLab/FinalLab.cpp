// Trab1.cpp : This file contains the 'main' function. Program execution begins and ends there.

//Includes

#include <pch.h>
#include <conio.h>
#include <stdlib.h>
#include <windows.h>  //for Sleep function
#include <stdio.h>
#include <iostream>
#include <map>
#include <string>
#include <iterator>
#include <queue>

extern "C"
{
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <interface.h> 
}

//Standing Request Structure
//For simplicity, parking services always enter and leave the same day.
//Position Structure


//Position Struture
typedef struct
{
	int x;
	int y;
	int z;

} TPosition;

//Declaration of the global variable that has the current position of the platform
TPosition pos;

//Standing Request Structure
//For simplicity, parking services always enter and leave the same day.
//Position Structure
typedef struct
{
	int plateNumber;       //Plate Number
	int entryHour;         //Hour of the entry 
	int entryMinute;       //Minute of the entry
	int exitHour;          //Hour of the exit
	int exitMinute;        //Minute of the exit
	int durationHours;     //Duration Hours
	int durationMinutes;   //Duration minutes   
	float actualCost;      //Actual cost of the parking
	int day;
	int month;
	int year;
	int entrySeconds;
	int exitSeconds;
	int durationSeconds;
	TPosition pos;

} StandingRequest;

std::map<int, StandingRequest> RequestsList;

std::map<int, StandingRequest> PastRequests;

std::queue<StandingRequest> if_full;

std::map<int, StandingRequest>::iterator it;

//Declaration of the global variable that deals with time
SYSTEMTIME st;

//Global variable that helps entering and leaving the joystick sub-menu
bool joy_activated;

//Declaration of the global variable that counts the total cash earned from finished parking services

float total_money = 0;

//Declaration of mailboxes
xQueueHandle mbx_req;
xQueueHandle mbx_x;   // for goto_x
xQueueHandle mbx_y;   // for goto_y
xQueueHandle mbx_z;   // for goto_z
xQueueHandle mbx_xz;
xQueueHandle mbx_input;
xQueueHandle mbx_int;
xQueueHandle mbx_uplight;
xQueueHandle mbx_downlight;

//Declaration of semaphores
xSemaphoreHandle  Semkit;
xSemaphoreHandle  sem_x_done;
xSemaphoreHandle  sem_y_done;
xSemaphoreHandle  sem_z_done;

xTaskHandle goto_xz_task_handler;

//Show_Menu : Function that shows the initial menu 
void show_menu()
{
	printf("\n\n****** WELCOME *****");
	printf("\n\nWhat do you want to do?");
	printf("\n(a) Put a part in a specific x, z position");
	printf("\n(b) Retrieve a part from a specific x, z position");
	printf("\n(c) Enter joystickmode");
	printf("\n(d) Show all parked cars");
	printf("\n(e) Show the (x,z) coordinates and hour at wich a car was parked with plate number as input");
	printf("\n(f) Given a (x,z) coordinate with the keyboard, shows the corresponding car plate, hour, duration in the screen");
	printf("\n(g) Shows the list of all (finished) parking services (plate number, date, duration, cost)");
	printf("\n(h) Show the total earned in Euros from all (finished) parking services");
	printf("\n(t) End program");
}

//GetBitValue : Function that given a byte value, returns the value of bit n 
int getBitValue(uInt8 value, uInt8 n_bit)
{
	return(value & (1 << n_bit));
}

//SetBitValue : Function that given a byte value, sets the n bit to value
void setBitValue(uInt8  *variable, int n_bit, int new_value_bit)
{
	uInt8  mask_on = (uInt8)(1 << n_bit);
	uInt8  mask_off = ~mask_on;
	if (new_value_bit)  *variable |= mask_on;
	else                *variable &= mask_off;
}

//Left Up Light On: Turns on the up the left up light
void left_up_light_on()
{
	taskENTER_CRITICAL();
	uInt8 p = ReadDigitalU8(2);  //  read port 2
	setBitValue(&p, 0, 1);       //  set bit 0 to high level
	WriteDigitalU8(2, p);        //  update port 2
	taskEXIT_CRITICAL();
}

//Left Up Light Off: Turns off the left up light
void left_up_light_off()
{
	taskENTER_CRITICAL();
	uInt8 p = ReadDigitalU8(2);  //  read port 2
	setBitValue(&p, 0, 0);       //  set bit 0 to low level
	WriteDigitalU8(2, p);        //  update port 2
	taskEXIT_CRITICAL();
}

//Both Left Light On: Turns on the both left lights
void both_left_lights_on()
{
	taskENTER_CRITICAL();
	uInt8 p = ReadDigitalU8(2);  //  read port 2
	setBitValue(&p, 0, 1);       //  set bit 0 to high level
	setBitValue(&p, 1, 1);       //  set bit 1 to high level
	WriteDigitalU8(2, p);        //  update port 2
	taskEXIT_CRITICAL();
}

//Left down Light On: Turns on the left down light
void left_down_light_on()
{
	taskENTER_CRITICAL();
	uInt8 p = ReadDigitalU8(2);  //  read port 2
	setBitValue(&p, 1, 1);       //  set bit 1 to high level
	WriteDigitalU8(2, p);        //  update port 2
	taskEXIT_CRITICAL();
}

//Left Down Light Off: Turns off the left down light
void left_down_light_off()
{
	taskENTER_CRITICAL();
	uInt8 p = ReadDigitalU8(2);  //  read port 2
	setBitValue(&p, 1, 0);       //  set bit 1 to low level
	WriteDigitalU8(2, p);        //  update port 2
	taskEXIT_CRITICAL();
}

//Both Left Lights Off: Turns off the both left lights
void both_left_lights_off()
{
	taskENTER_CRITICAL();
	uInt8 p = ReadDigitalU8(2);  //  read port 2
	setBitValue(&p, 0, 0);       //  set bit 0 to low level
	setBitValue(&p, 1, 0);       //  set bit 1 to low level
	WriteDigitalU8(2, p);        //  update port 2
	taskEXIT_CRITICAL();
}

//ReadLeftUpButton : Returns true if the button is pressed, false if not
bool readLeftUpButton() {

	uInt8 p = ReadDigitalU8(1);
	if (getBitValue(p, 5))
		return true;
	else
		return false;
}

//ReadLeftDownButton : Returns true if the button is pressed, false if not
bool readLeftDownButton() {

	uInt8 p = ReadDigitalU8(1);
	if (getBitValue(p, 6))
		return true;
	else
		return false;
}

//Move Z Up : Function that moves the platform up
void move_z_up()
{
	taskENTER_CRITICAL();
	uInt8 p = ReadDigitalU8(2);  //  read port 2
	setBitValue(&p, 3, 1);       //  set bit 3 to high level
	setBitValue(&p, 2, 0);       //  set bit 2 to low level
	WriteDigitalU8(2, p);        //  update port 2
	taskEXIT_CRITICAL();
}

//Move Z Down : Function that moves the platform down
void move_z_down()
{
	taskENTER_CRITICAL();
	uInt8 p = ReadDigitalU8(2);  //  read port 2
	setBitValue(&p, 2, 1);       //  set bit 3 to down level
	setBitValue(&p, 3, 0);       //  set bit 2 to high level
	WriteDigitalU8(2, p);        //  update port 2
	taskEXIT_CRITICAL();
}

//Move X Left : Function that moves the platform left
void move_x_left()
{
	taskENTER_CRITICAL();
	uInt8 p = ReadDigitalU8(2);  //  read port 2
	setBitValue(&p, 6, 1);       //  set bit 6 to high level
	setBitValue(&p, 7, 0);       //  set bit 7 to low level
	WriteDigitalU8(2, p);        //  update port 2
	taskEXIT_CRITICAL();
}

//Move X Right : Function that moves the platform right
void move_x_right()
{
	taskENTER_CRITICAL();
	uInt8 p = ReadDigitalU8(2);  //  read port 2
	setBitValue(&p, 6, 0);       //  set bit 6 to  low level
	setBitValue(&p, 7, 1);       //  set bit 7 to high level
	WriteDigitalU8(2, p);        //  update port 2
	taskEXIT_CRITICAL();
}

//Move Y Inside : Function that moves the platform inside
void move_y_inside()
{
	taskENTER_CRITICAL();
	uInt8 p = ReadDigitalU8(2);   //  read port 2
	setBitValue(&p, 4, 0);        //  set bit 5 to  high level
	setBitValue(&p, 5, 1);        //  set bit 4 to low level
	WriteDigitalU8(2, p);         //  update port 2
	taskEXIT_CRITICAL();
}

//Move Y Outside : Function that moves the platform outside
void move_y_outside()
{
	taskENTER_CRITICAL();
	uInt8 p = ReadDigitalU8(2);   //  read port 2
	setBitValue(&p, 4, 1);        //  set bit 5 to low level
	setBitValue(&p, 5, 0);        //  set bit 4 to high level
	WriteDigitalU8(2, p);         //  update port 2
	taskEXIT_CRITICAL();
}

//Stop X : Function that stops the platform from moving left or right
void stop_x()
{
	taskENTER_CRITICAL();
	uInt8 p = ReadDigitalU8(2);  //  read port 2
	setBitValue(&p, 6, 0);       //  set bit 6 to  low level
	setBitValue(&p, 7, 0);       //  set bit 7 to low level
	WriteDigitalU8(2, p);        //  update port 2
	taskEXIT_CRITICAL();
}

//Stop Y : Function that stops the platform from moving inside or outside
void stop_y()
{
	taskENTER_CRITICAL();
	uInt8 p = ReadDigitalU8(2);  //  read port 2
	setBitValue(&p, 5, 0);       //  set bit 5 to  low level
	setBitValue(&p, 4, 0);       //  set bit 4 to low level
	WriteDigitalU8(2, p);        //  update port 2
	taskEXIT_CRITICAL();
}

//Stop Z : Function that stops the platform from moving up or down
void stop_z()
{
	taskENTER_CRITICAL();
	uInt8 p = ReadDigitalU8(2);   //  read port 2
	setBitValue(&p, 3, 0);        //  set bit 3 to  low level
	setBitValue(&p, 2, 0);        //  set bit 2 to low level
	WriteDigitalU8(2, p);         //  update port 2
	taskEXIT_CRITICAL();
}

//Is at Z up : Function that checks if the platform is at the higher Z position
bool is_at_z_up()
{
	int vp0 = ReadDigitalU8(0);
	int vp1 = ReadDigitalU8(1);
	if (!getBitValue(vp0, 6) || !getBitValue(vp1, 0) || !getBitValue(vp1, 2))
		return true;
	return false;
}

//Is at Z down : Function that checks if at the platform is at the lower Z position
bool is_at_z_down()
{
	int vp0 = ReadDigitalU8(0);
	int vp1 = ReadDigitalU8(1);
	if (!getBitValue(vp0, 7) || !getBitValue(vp1, 1) || !getBitValue(vp1, 3))
		return true;
	return false;
}

//Is at X sensor : For calibration purposes
bool is_at_x_sensor()
{
	uInt8 vp = ReadDigitalU8(0);
	if (!getBitValue(vp, 2) || !getBitValue(vp, 1) || !getBitValue(vp, 0))
		return true;
	return false;
}


//Is at y sensor : For calibration purposes
bool is_at_y_sensor()
{
	int vp = ReadDigitalU8(0);
	if (!getBitValue(vp, 5) || !getBitValue(vp, 4) || !getBitValue(vp, 3))
		return true;
	return false;
}


// Get Z Pos: Function that returns the Z position
int get_z_pos()
{
	uInt8 p0 = ReadDigitalU8(0);
	uInt8 p1 = ReadDigitalU8(1);
	if (!getBitValue(p1, 3))          // Down
		return 1;
	if (!getBitValue(p1, 1))          // Middle
		return 2;
	if (!getBitValue(p0, 7))          // Highest
		return 3;
	return(-1);
}

// Get X Pos: Function that returns the X position
int get_x_pos()
{
	uInt8 p = ReadDigitalU8(0);
	if (!getBitValue(p, 2))             // Left
		return 1;
	if (!getBitValue(p, 1))             // Middle
		return 2;
	if (!getBitValue(p, 0))             // Right
		return 3;
	return(-1);
}

// Get y Pos: Function that returns the y position
int get_y_pos()
{
	uInt8 p = ReadDigitalU8(0);
	if (!getBitValue(p, 5))
		return 1;                   // Outside
	if (!getBitValue(p, 4))
		return 2;                   // Middle
	if (!getBitValue(p, 3))
		return 3;                   // Inside
	return(-1);
}

// goto_x : moves the platform along the xx axis until it reaches a certain position
void goto_x(int x)
{
	if (get_x_pos() < x)
		move_x_right();
	else if (get_x_pos() > x)
		move_x_left();
	//   while position not reached
	while (get_x_pos() != x)
		Sleep(1);
	stop_x();
}

// goto_y : moves the platform along the yy axis until it reaches a certain position
void goto_y(int y)
{
	if (get_y_pos() < y)
		move_y_inside();
	else if (get_y_pos() > y)
		move_y_outside();
	//   while position not reached
	while (get_y_pos() != y)
		Sleep(1);
	stop_y();
}

// goto_z : moves the platform along the zz axis until it reaches a certain position
void goto_z(int z)
{
	if (get_z_pos() < z)
		move_z_up();
	else if (get_z_pos() > z)
		move_z_down();
	//   while position not reached
	while (get_z_pos() != z)
		Sleep(1);
	stop_z();
}

// goto_xz : go to xz using mailboxes (queue)
void goto_xz(int x, int z, bool _wait_till_done)
{
	TPosition pos;
	pos.x = x;
	pos.z = z;
	xQueueSend(mbx_xz, &pos, portMAX_DELAY);
	if (_wait_till_done) {
		while (get_x_pos() != x) { taskYIELD(); }
		while (get_z_pos() != z) { taskYIELD(); }
	}
}

// goto_xz : go to xz using mailboxes (queue)
void goto_y2(int y, bool _wait_till_done)
{
	TPosition pos;
	pos.y = y;
	xQueueSend(mbx_y, &pos, portMAX_DELAY);
	if (_wait_till_done) {
		while (get_y_pos() != y) { taskYIELD(); }
	}
}

// goto_z_up : moves the platform to the nearest up sensor
void goto_z_up()
{
	if (!is_at_z_up())
		move_z_up();
	while (!is_at_z_up())
		taskYIELD();
	stop_z();
}

// goto_z_dn : moves the platform to the nearest down sensor
void goto_z_down()
{
	if (!is_at_z_down())
		move_z_down();
	while (!is_at_z_down()  /* …. similar to goto_z_up  */)
		taskYIELD();
	stop_z();
}

//Level up : moves the platform to the nearest up sensor (level up)
void level_up()
{
	uInt8 p0 = ReadDigitalU8(0);
	uInt8 p1 = ReadDigitalU8(1);

	if (getBitValue(p0, 7) && getBitValue(p0, 6) && getBitValue(p1, 0) && getBitValue(p1, 1) && getBitValue(p1, 2) && getBitValue(p1, 3))
		return;
	else
	{
		move_z_up();
		while (getBitValue(p0, 6) && getBitValue(p1, 0) && getBitValue(p1, 2))
		{
			p0 = ReadDigitalU8(0);
			p1 = ReadDigitalU8(1);
			Sleep(10);
		}
		stop_z();
	}
}

//Level down : moves the platform to the nearest down sensor (level down)
void level_down()
{
	uInt8 p0 = ReadDigitalU8(0);
	uInt8 p1 = ReadDigitalU8(1);

	if (getBitValue(p0, 7) && getBitValue(p0, 6) && getBitValue(p1, 0) && getBitValue(p1, 1) && getBitValue(p1, 2) && getBitValue(p1, 3))
		return;
	else
	{
		move_z_down();
		while (getBitValue(p0, 7) && getBitValue(p1, 1) && getBitValue(p1, 3))
		{
			p0 = ReadDigitalU8(0);
			p1 = ReadDigitalU8(1);
			Sleep(10);
		}
		stop_z();
	}
}

// goto_x_right : moves the to the nearest right sensor
void goto_x_right()
{
	if (!is_at_x_sensor())
		move_x_right();
	while (!is_at_x_sensor()  /* …. similar to goto_z_up  */)
		taskYIELD();
	stop_x();
}

// goto_x_left : moves the to the nearest left sensor
void goto_x_left()
{
	if (!is_at_x_sensor())
		move_x_left();
	while (!is_at_x_sensor()  /* …. similar to goto_z_up  */)
		taskYIELD();
	stop_x();
}

// goto_y_inside : moves the platform to the nearest inside sensor
void goto_y_inside()
{
	if (!is_at_y_sensor())
		move_y_inside();
	while (!is_at_y_sensor()  /* …. similar to goto_z_up  */)
		taskYIELD();
	stop_x();
}

// goto_y_outside : moves the platform to the nearest outside sensor
void goto_y_outside()
{
	if (!is_at_y_sensor())
		move_y_outside();
	while (!is_at_y_sensor()  /* …. similar to goto_z_up  */)
		taskYIELD();
	stop_y();
}

// goto_x_task : go to x but with mailboxes (queue)
void goto_x_task(void *param)
{
	while (true)
	{
		int x;
		xQueueReceive(mbx_x, &x, portMAX_DELAY);
		goto_x(x);
		xSemaphoreGive(sem_x_done);
	}
}

// goto_y_task : go to x but with mailboxes (queue)
void goto_y_task(void *param)
{
	while (true)
	{
		int y;
		xQueueReceive(mbx_y, &y, portMAX_DELAY);
		goto_y(y);
		xSemaphoreGive(sem_y_done);
	}
}

// goto_z_task : go to z but with mailboxes (queue)
void goto_z_task(void *param)
{
	while (true)
	{
		int z;
		xQueueReceive(mbx_z, &z, portMAX_DELAY);
		goto_z(z);
		xSemaphoreGive(sem_z_done);
	}
}

// goto_xz_task : go to xz but with mailboxes (queue)
void goto_xz_task(void *param)
{
	while (true) {
		TPosition position;
		//wait until a goto request is received
		xQueueReceive(mbx_xz, &position, portMAX_DELAY);

		//send request for each goto_x_task and goto_z_task 
		xQueueSend(mbx_x, &position.x, portMAX_DELAY);
		xQueueSend(mbx_z, &position.z, portMAX_DELAY);

		//wait for goto_x completion
		xSemaphoreTake(sem_x_done, portMAX_DELAY);

		//wait for goto_z completion           
		xSemaphoreTake(sem_z_done, portMAX_DELAY);
	}
}

// Left Up Flash Task : Task that makes the left up light flash at a given flash period
void left_up_flash_task(void *param)
{
	int flash_period;

	while (true) {

		taskYIELD();
		xQueueReceive(mbx_uplight, &flash_period, portMAX_DELAY);

		while (flash_period != 0)
		{
			left_up_light_on();
			vTaskDelay(flash_period);
			left_up_light_off();
			vTaskDelay(flash_period);

			if (uxQueueMessagesWaiting(mbx_uplight))
				flash_period = 0;
			taskYIELD();
		}
	}
}

// Left Up Down Task : Task that makes the left up down flash at a given flash period
void left_down_flash_task(void *param)
{
	int flash_period;

	while (true) {

		taskYIELD();
		xQueueReceive(mbx_downlight, &flash_period, portMAX_DELAY);

		while (flash_period != 0)
		{
			left_down_light_on();
			vTaskDelay(flash_period);
			left_down_light_off();
			vTaskDelay(flash_period);

			if (uxQueueMessagesWaiting(mbx_downlight))
				flash_period = 0;
			taskYIELD();
		}
	}
}

// receive_instructions_task : receives the message and executes the required instruction
void receive_instructions_task(void *ignore) {
	int c = 0;
	while (true) {
		c = _getwch();  // this function is new.
		if (!joy_activated)
			putchar(c);
		xQueueSend(mbx_input, &c, portMAX_DELAY);
	}
}

// calibrate_platform : places the platform in x and z sensors to allow it to move towards a desired position
void calibrate_platform()
{
	goto_z_down();
	goto_x_right();
}

// Update Pos Task : Updates the structure that has the coordinates of the platform
void update_pos_task(void *param) {

	while (true) {

		pos.x = get_x_pos();
		pos.y = get_y_pos();
		pos.z = get_z_pos();

		taskYIELD();
		vTaskDelay(500);
	}
}

//Check Both Buttons Task : Cheks if both buttons are pressed (emergency stop)
void check_both_buttons_task(void *param)
{
	boolean stop = false;
	int flash_period;

	while (true)
	{
		taskYIELD();
		if (readLeftDownButton() && readLeftUpButton())
		{
			uInt8 p = ReadDigitalU8(2);  //  read port 2
			stop = true;
			while (stop)
			{
				taskYIELD();
				WriteDigitalU8(2, 0);    //Stops all
				flash_period = 50;
				xQueueSend(mbx_downlight, &flash_period, portMAX_DELAY);
				xQueueSend(mbx_uplight, &flash_period, portMAX_DELAY);
				Sleep(200);

				if (readLeftUpButton())
				{
					taskYIELD();
					WriteDigitalU8(2, p);    //Resumes
					stop = false;            //Exits the stop cycle
				}

				if (readLeftDownButton())
				{
					taskYIELD();
					stop = false;
					calibrate_platform();
					printf("sai");
				}
			}
			flash_period = 0;
			xQueueSend(mbx_downlight, &flash_period, portMAX_DELAY);
			xQueueSend(mbx_uplight, &flash_period, portMAX_DELAY);
		}
	}
}

// Read Plate
int read_plate(bool parking_or_retrieving)
{
	int plate = 0;
	if (parking_or_retrieving)
	{															//True  = Parking
		GetLocalTime(&st);
		int start_seconds = st.wSecond;
		int finish_seconds = st.wSecond;
		int difference = 0;

		while (difference < 10)
		{														//Waits 10 seconds for the plate
			if (readLeftUpButton()) {
				plate++;
				Sleep(150);
			}

			GetLocalTime(&st);
			finish_seconds = st.wSecond;
			difference = finish_seconds - start_seconds;
			if (difference < 0)
				difference = difference + 60;
			taskYIELD();
		}
	}
	else
	{															//False = Retrieving
		GetLocalTime(&st);
		int start_seconds = st.wSecond;
		int finish_seconds = st.wSecond;
		int difference = 0;

		while (difference < 10)
		{														//Waits 10 seconds for the plate
			if (readLeftDownButton()) {
				plate++;
				Sleep(150);
			}

			GetLocalTime(&st);
			finish_seconds = st.wSecond;
			difference = finish_seconds - start_seconds;
			if (difference < 0)
				difference = difference + 60;
			taskYIELD();
		}
	}
	return plate;
}

// PutInCell: leaves a car in a parking spot given by the user
void put_in_cell(int x, int z)
{
	int flash_period = 100;														//The light stays 100ms on and 100ms off (cycle)

	goto_xz(x, z, true);														//Goes to cell (x,z)
	xQueueSend(mbx_uplight, &flash_period, portMAX_DELAY);						//Request to activate up light flash
	level_up();																	//The platform goes a level up  
	goto_y(3);																    //The car goes inside
	level_down();																//The platform goes a level down, leaving the car in the cell    
	goto_y(2);																	//The platform goes to the original position
	flash_period = 0;															//The light goes permanently off
	xQueueSend(mbx_uplight, &flash_period, portMAX_DELAY);						//Request to desactivate up light flash
}

// GetFromCell: retrieves a car in a parking spot given by the user
void get_from_cell(int x, int z)
{
	int flash_period = 100;														//The light stays 100ms on and 100ms off (cycle)

	goto_xz(x, z, true);														//Goes to cell (x,z)
	xQueueSend(mbx_downlight, &flash_period, portMAX_DELAY);					//Request to activate down light flash
	level_down();																//The platform goes a level down  
	goto_y(3);																    //The platform reaches inside 
	level_up();																	//The platform goes a level up, retrieving the car
	goto_y(2);																	//The platform goes to the original position
	level_down();
	flash_period = 0;															//The light goes permanently off
	xQueueSend(mbx_downlight, &flash_period, portMAX_DELAY);					//Request to desactivate down light flash
}

// Joystick : Allows the user to move the platform using the keyboard, like a joystick
void joystick()
{
	char key;
	joy_activated = true;
	printf("\nOptions: w (up), s (down), a (left), d (right), i (inside), o (outside), p (stop all), l (stop and leave)");

	while (joy_activated)
	{
		xQueueReceive(mbx_input, &key, portMAX_DELAY);
		switch (key)
		{
		case 'w':
		{
			if (pos.y == 2)
			{
				move_z_up();
				break;
			}
			else
				break;
		}

		case 's':
		{
			if (pos.y == 2)
			{
				move_z_down();
				break;
			}
			else
				break;
		}

		case 'd':
		{
			if (pos.y == 2)
			{
				move_x_right();
				break;
			}
			else
				break;
		}

		case 'a':
		{
			if (pos.y == 2)
			{
				move_x_left();
				break;
			}
			else
				break;
		}

		case 'i':
		{
			stop_x();
			stop_z();
			move_y_inside();
			break;
		}

		case 'o':
		{
			stop_x();
			stop_z();
			move_y_outside();
			break;
		}

		case 'p':
		{
			stop_x();
			stop_y();
			stop_z();
			break;
		}

		case 'l':
		{
			joy_activated = false;
			stop_x();
			stop_z();
			stop_y();
			break;
		}
		}
	}
}

SYSTEMTIME get_system_time()
{
	GetSystemTime(&st);
	return st;
	//printf("Year: %d, Month: %d, Date: %d, Hour: %d, Min: %d, Second: %d", st.wYear, st.wMonth, st.wDay, (st.wHour + 1), st.wMinute, st.wSecond);
}

void calculate_fee(StandingRequest* a)
{
	int hours_spent;
	int minutes_spent;
	int seconds_spent;
	float price;

	hours_spent = a->exitHour - a->entryHour;

	if (a->exitMinute >= a->entryMinute)
		minutes_spent = a->exitMinute - a->entryMinute;
	else
	{
		minutes_spent = 60 - a->entryMinute + a->exitMinute;
		hours_spent--;
	}

	if (a->exitSeconds > a->entrySeconds)
		seconds_spent = a->exitSeconds - a->entrySeconds;
	else
	{
		seconds_spent = 60 - a->entrySeconds + a->exitSeconds;
		minutes_spent--;
	}

	price = seconds_spent + minutes_spent * 60 + hours_spent * 60 * 60;
	price = (price / 100);

	a->actualCost = price;
	a->durationHours = hours_spent;
	a->durationMinutes = minutes_spent;
	a->durationSeconds = seconds_spent;
}

//Task Storage Services : Manages the menu options
void task_storage_services(void *param)
{
	int cmd = -1;
	int plate;

	while (true)
	{
		show_menu();												// get selected option from keyboard											
		printf("\n\nEnter option=");
		xQueueReceive(mbx_input, &cmd, portMAX_DELAY);

		if (cmd == 'a')
		{
			TPosition pos;
			int x, z;
			StandingRequest a;

			printf("\nWhere do you want to park your car?");
			printf("\nX=");
			xQueueReceive(mbx_input, &x, portMAX_DELAY);
			x = x - '0'; //convert from ascii code to number

			printf("\nZ=");
			xQueueReceive(mbx_input, &z, portMAX_DELAY);
			z = z - '0'; //convert from ascii code to number

			if (x >= 1 && x <= 3 && z >= 1 && z <= 3)
			{
				a.pos.x = x;
				a.pos.z = z;

				printf("\nYou have 10 seconds to click sw1 to input plate number");
				plate = read_plate(true);
				printf("\nYour plate number is %d", plate);
				a.plateNumber = plate;

				bool available = true;

				if (RequestsList.size() >= 9)
					available = false;

				for (auto it = RequestsList.cbegin(); it != RequestsList.cend(); ++it)
				{
					StandingRequest b = it->second;
					if (b.pos.x == x && b.pos.z == z)
						available = false;
				}

				if (available) {

					get_system_time();
					a.entryHour = (st.wHour + 1);
					a.entryMinute = st.wMinute;
					a.entrySeconds = st.wSecond;
					a.day = st.wDay;
					a.month = st.wMonth;
					a.year = st.wYear;
					a.plateNumber = plate;

					put_in_cell(x, z);

					if (st.wMinute < 10)
						printf("\nEntry hour: %d:0%d:%d", a.entryHour, a.entryMinute, a.entrySeconds);
					else
						printf("\nEntry hour: %d:%d:%d", a.entryHour, a.entryMinute, a.entrySeconds);

					printf("\nCar parked in: X = %d and Z = %d", a.pos.x, a.pos.z);
					RequestsList.insert(std::pair <int, StandingRequest>(a.plateNumber, a));
				}
				else {

					printf("\nThe cell is full. Your car will be parked ASAP... when a cell is available.");
					if_full.push(a);
				}
			}

			else
				printf("\nWrong (x,z) coordinates, are you sleeping?... ");
		}

		if (cmd == 'b')
		{
			TPosition pos;
			int x, z;
			bool available = false;
			get_system_time();
			StandingRequest a;

			printf("\nWhere was your car parked?");
			printf("\nX=");
			xQueueReceive(mbx_input, &x, portMAX_DELAY);
			x = x - '0'; //convert from ascii code to number

			printf("\nZ=");
			xQueueReceive(mbx_input, &z, portMAX_DELAY);
			z = z - '0'; //convert from ascii code to number

			printf("\nYou have 10 seconds to click sw2 to input plate number");
			plate = read_plate(false);
			printf("\nYour plate number is %d", plate);

			if (x >= 1 && x <= 3 && z >= 1 && z <= 3)
			{

				for (auto it = RequestsList.cbegin(); it != RequestsList.cend(); ++it)
				{
					StandingRequest b = it->second;
					if (b.pos.x == x && b.pos.z == z && plate == b.plateNumber)
						available = true;
				}

				if (available)
				{

					it = RequestsList.find(plate);
					StandingRequest a = it->second;
					RequestsList.erase(it);

					a.pos.x = x;
					a.pos.z = z;
					a.exitHour = st.wHour;
					a.exitMinute = st.wMinute;
					a.exitSeconds = st.wSecond;
					calculate_fee(&a);
					total_money = total_money + a.actualCost;
					PastRequests.insert(std::pair <int, StandingRequest>(a.plateNumber, a));
					it = PastRequests.find(plate);
					StandingRequest b = it->second;

					get_from_cell(x, z);

					if (st.wMinute < 10)
						printf("\nExit hour: %d:0%d:%d", a.exitHour, a.exitMinute, a.exitSeconds, a.plateNumber);
					else
						printf("\nExit hour: %d:%d:%d", a.exitHour, a.exitMinute, a.exitSeconds, a.plateNumber);

					printf("\nYour stay: %d Hours, %d Minutes, and %d Seconds", a.durationHours, a.durationMinutes, a.durationSeconds);
					printf("\nPrice: %.2f Euros", a.actualCost);

					if (if_full.size())
					{
						StandingRequest z;
						z = if_full.front();
						if_full.pop();
						z.pos.x = b.pos.x;
						z.pos.z = b.pos.z;
						get_system_time();
						z.entryHour = (st.wHour + 1);
						z.entryMinute = st.wMinute;
						z.entrySeconds = st.wSecond;
						z.day = st.wDay;
						z.month = st.wMonth;
						z.year = st.wYear;
						z.plateNumber = plate;

						put_in_cell(z.pos.x, z.pos.z);

						if (st.wMinute < 10)
							printf("\nEntry hour: %d:0%d:%d", a.entryHour, a.entryMinute, a.entrySeconds);
						else
							printf("\nEntry hour: %d:%d:%d", a.entryHour, a.entryMinute, a.entrySeconds);

						printf("\nCar parked in: X = %d and Z = %d", z.pos.x, z.pos.z);
						RequestsList.insert(std::pair <int, StandingRequest>(z.plateNumber, z));
					}
				}

				else
					printf("\nThere's no car in that cell");
			}

			else
				printf("\nWrong (x,z) coordinates, are you sleeping?... ");
		}

		if (cmd == 'c')
		{
			joystick();
			calibrate_platform();
		}

		if (cmd == 'd')
		{
			printf("\nCars parked at the moment: %d", RequestsList.size());
			get_system_time();
			for (auto it = RequestsList.cbegin(); it != RequestsList.cend(); ++it)
			{
				get_system_time();
				StandingRequest a = it->second;
				a.exitHour = (st.wHour + 1);
				a.exitMinute = st.wMinute;
				a.exitSeconds = st.wSecond;
				calculate_fee(&a);
				printf("\nPlate: %d\nEntry: %d:%d:%d\nCoordinates: (%d, %d)\nCost for now: %.2f Euros", a.plateNumber, a.entryHour, a.entryMinute, a.entrySeconds, a.pos.x, a.pos.z, a.actualCost);
			}
		}

		if (cmd == 'e')
		{
			printf("\nEnter your car plate number: ");
			xQueueReceive(mbx_input, &plate, portMAX_DELAY);
			plate = plate - '0'; //convert from ascii code to number
			int has_plate = false;
			for (auto it = RequestsList.cbegin(); it != RequestsList.cend(); ++it)
			{
				StandingRequest a = it->second;
				if (a.plateNumber == plate) {
					printf("\nYour car was parked in (%d, %d) at %d:%d:%d", a.pos.x, a.pos.z, a.entryHour, a.entryMinute, a.entrySeconds);
					has_plate = true;
				}
			}
			if (!has_plate)
				printf("\nThere's no car with that plate.");
		}

		if (cmd == 'f')
		{
			int x, z;
			printf("\nWhere was your car parked?");
			printf("\nX=");
			xQueueReceive(mbx_input, &x, portMAX_DELAY);
			x = x - '0'; //convert from ascii code to number

			printf("\nZ=");
			xQueueReceive(mbx_input, &z, portMAX_DELAY);
			z = z - '0'; //convert from ascii code to number


			for (auto it = RequestsList.cbegin(); it != RequestsList.cend(); ++it)
			{
				StandingRequest a = it->second;
				if (a.pos.x == x && a.pos.z == z) {
					get_system_time();
					a.exitHour = (st.wHour + 1);
					a.exitMinute = st.wMinute;
					a.exitSeconds = st.wSecond;
					calculate_fee(&a);
					printf("\nThe corresponding car plate is: %d\nEntry hour: %d:%d:%d\nDuration - Hours: %d, Minutes: %d, Seconds: %d", a.plateNumber, a.entryHour, a.entryMinute, a.entrySeconds, a.durationHours, a.durationMinutes, a.durationSeconds);
				}
			}
		}
		else
			printf("\nThere are no cars parked in that cell");

		if (cmd == 'g')
		{
			printf("\nList of tickets: %d", PastRequests.size());
			for (auto it = PastRequests.cbegin(); it != PastRequests.cend(); ++it)
			{
				StandingRequest a = it->second;
				printf("\nPlate: %d\nDate: %d/%d/%d\nDuration - Hours: %d, Minutes: %d, Seconds: %d\nPrice: %.2f Euros", a.plateNumber, a.day, a.month, a.year, a.durationHours, a.durationMinutes, a.durationSeconds, a.actualCost);
			}
		}

		if (cmd == 'h')
			printf("\nThe total money earned from all the parked cars of the day: %.2f Euros.", total_money);

		if (cmd == 's') // hidden option  
			stop_x();

		if (cmd == 't') // hidden option
		{
			WriteDigitalU8(2, 0); //stop all motores;
			vTaskEndScheduler(); // terminates application
		}
	}   // end while
}

void main(int argc, char **argv) {

	create_DI_channel(0);
	create_DI_channel(1);
	create_DO_channel(2);

	calibrate_platform();

	printf("\nwaiting for hardware simulator...");
	WriteDigitalU8(2, 0);
	printf("\ngot access to simulator...");

	sem_x_done = xSemaphoreCreateCounting(1000, 0);
	sem_y_done = xSemaphoreCreateCounting(1000, 0);
	sem_z_done = xSemaphoreCreateCounting(1000, 0);

	mbx_req = xQueueCreate(10, sizeof(StandingRequest));
	mbx_int = xQueueCreate(10, sizeof(int));
	mbx_x = xQueueCreate(10, sizeof(int));
	mbx_y = xQueueCreate(10, sizeof(int));
	mbx_z = xQueueCreate(10, sizeof(int));
	mbx_xz = xQueueCreate(10, sizeof(TPosition));
	mbx_input = xQueueCreate(10, sizeof(int));
	mbx_uplight = xQueueCreate(10, sizeof(int));
	mbx_downlight = xQueueCreate(10, sizeof(int));

	xTaskCreate(task_storage_services, "task_storage_services", 100, NULL, 0, NULL);
	xTaskCreate(goto_x_task, "goto_x_task", 100, NULL, 0, NULL);
	xTaskCreate(goto_y_task, "goto_y_task", 100, NULL, 0, NULL);
	xTaskCreate(goto_z_task, "goto_z_task", 100, NULL, 0, NULL);
	xTaskCreate(goto_xz_task, "v_gotoxz_task", 100, NULL, 0, &goto_xz_task_handler);
	xTaskCreate(receive_instructions_task, "receive_instructions_task", 100, NULL, 0, NULL);
	xTaskCreate(left_up_flash_task, "left_up_flash_task", 100, NULL, 0, NULL);
	xTaskCreate(left_down_flash_task, "left_up_flash_task", 100, NULL, 0, NULL);
	xTaskCreate(update_pos_task, "update_tpos_task", 100, NULL, 0, NULL);
	xTaskCreate(check_both_buttons_task, "check_both_buttons_task", 100, NULL, 0, NULL);
	vTaskStartScheduler();
}
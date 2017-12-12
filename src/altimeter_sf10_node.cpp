#include <ros/ros.h>
#include <sensor_msgs/Range.h>

#include "CerealPort.h"
#include <chrono>
#include <thread>
#include <unistd.h>

#define REPLY_SIZE 20
#define TIMEOUT 1000

/*
static value for the altitude. Both the altimeter thread and the main function
have acces to it. Altimeter() writes a new value of the altitude each time a
new value is available. Main() read and outputs the value in altitude when needed.
 */
static double altitude;
static bool dev = true;
static std::string serial_port = "/dev/ttyUSB0";

boost::mutex m;

void altimeter() {

	unsigned int message_shift = 0; //is going to be varied by the synchronization later on
  unsigned int voltage_message_begin =message_shift+8; //default values on where in the sting the information stands
	unsigned int voltage_message_end =message_shift+15;
	unsigned int altitude_message_begin =message_shift;
	unsigned int altitude_message_end =message_shift+6;

	double voltage;

	std::chrono::high_resolution_clock cl;
	std::chrono::time_point<std::chrono::high_resolution_clock> t_begin, t_end;

	cereal::CerealPort device;
	char reply[REPLY_SIZE];

    //reading the string from serial port
	try{ device.open(serial_port.c_str() , 115200); }
	   catch(cereal::Exception& e){
	   	 ROS_ERROR("EXCEPTION: port %s not opened ", serial_port.c_str());
	   	 ROS_BREAK();
	   }

	while( dev ) {

		// continue acquiring altimetric measures
		t_begin = cl.now();

		std::stringstream ss1;
		std::stringstream ss2;


 		device.readBytes(reply, REPLY_SIZE, TIMEOUT);

		/*getting the altitude */
		for(unsigned int i=altitude_message_begin; i<=altitude_message_end; i++) {
			ss1<<reply[i];
		}

		/*getting the voltage */
		for(unsigned int j=voltage_message_begin; j<=voltage_message_end; j++) {
			ss2<<reply[j];
		}

		ss1 >> altitude;

		// Mutex lock to avoid data race between this_thread and main_thread
		m.lock();
			ss2 >> voltage;
		m.unlock();

		if(isdigit(reply[altitude_message_end])) {

		}
		else {

	    }

		/* for synchronisation */
		if(reply[message_shift+7]!='m') {

			message_shift++;

			voltage_message_begin =message_shift+10;
			voltage_message_end =message_shift+14;
			altitude_message_begin =message_shift;
			altitude_message_end =message_shift+5;

			if(message_shift>=20) message_shift=message_shift-20;
			if(voltage_message_begin>=20) voltage_message_begin=voltage_message_begin-20;
			if(voltage_message_end>=20) voltage_message_end=voltage_message_end-20;
			if(altitude_message_begin>=20) altitude_message_begin=altitude_message_begin-20;
			if(altitude_message_end>=20) altitude_message_end=altitude_message_end-20;


		}

		t_end = cl.now();
	}
	
	device.close();
}


int main(int argc, char** argv){

	// Init node and publisher
	ros::init(argc, argv, "altimeter_sf10_node");
	ros::NodeHandle nh;
	
	ros::Publisher alti_pub = nh.advertise<sensor_msgs::Range>("/alti", 10);
	ros::Rate loop_rate(20);
	
	// Read from launch file
	nh.getParam("serial_port_alti", serial_port);

	// Spawn thread to empty alti buffer to "altitude" double
	boost::thread t(altimeter);
	
	// Create range message
	sensor_msgs::Range range_msg;
	range_msg.header.frame_id = "alti";
	range_msg.field_of_view = 0.01;
	range_msg.min_range = 0.1;
	range_msg.max_range = 50;

	while(ros::ok()) {

		// Publish range measures
		range_msg.range = altitude;
		alti_pub.publish(range_msg);
		
		loop_rate.sleep();
		ros::spinOnce();

	}

	t.join();
	dev = false;
	ROS_INFO("Shutting down..");

	return 0;
}


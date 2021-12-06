#include "server.cpp"
#include "mobile.cpp"


#define CLOCK_PERIOD 10
#define BANDWIDTH int(512e3)
// #define BANDWIDTH int(10e6)
#define IMAGE_SIZE_X 1024			// image width
#define IMAGE_SIZE_Y 1024			// image height
#define PACKET_SIZE 20				// number of tuples in packet
#define NUM_DEVICES 3				// number of devices on network
#define NUM_IMAGES 5				// number of images
#define SERVER_PACKET_SIZE 200		// number of pixels in packet sent to mobile

#include "systemc.h"
#include <string>

using namespace std;

vector<vector<roi>> rois =
	{{{1, {50, 20}, {400, 320}},
	  {2, {50, 370}, {450, 1000}},
	  {3, {470, 20}, {600, 900}},
	  {4, {670, 40}, {950, 550}},
	  {5, {680, 700}, {1000, 1000}}},
	 {{1, {10, 10}, {150, 700}},
	  {2, {300, 10}, {980, 250}},
	  {3, {300, 270}, {980, 700}},
	  {4, {10, 740}, {950, 1000}}},
	 {{1, {10, 10}, {260, 900}},
	  {2, {270, 10}, {520, 1000}},
	  {3, {570, 20}, {700, 950}},
	  {4, {730, 10}, {950, 950}}},
	 {{1, {10, 10}, {160, 1000}},
	  {2, {170, 10}, {220, 1000}},
	  {3, {230, 10}, {380, 1000}},
	  {4, {380, 10}, {540, 1000}},
	  {5, {550, 10}, {700, 1000}},
	  {6, {710, 10}, {860, 1000}},
	  {7, {870, 10}, {1010, 1000}}},
	 {{1, {10, 10}, {1000, 160}},
	  {2, {10, 170}, {1000, 220}},
	  {3, {10, 230}, {1000, 380}},
	  {4, {100, 380}, {1000, 540}},
	  {5, {10, 550}, {1000, 700}},
	  {6, {10, 710}, {1000, 860}},
	  {7, {10, 870}, {1000, 1010}}}};


template <int program_size>
class stimulus : public sc_module
{
public:
	// PORTS
	sc_out<bool> clock;

	// CONSTRUCTOR
	SC_HAS_PROCESS(stimulus);

	stimulus(sc_module_name name) : sc_module(name)
	{
		SC_THREAD(main);
	}

	// PROCESS
	void main()
	{
		for (int i = 0; i < program_size * CLOCK_PERIOD; ++i)
		{
			clock = 1;
			wait(CLOCK_PERIOD/2, SC_MS);
			clock = 0;
			wait(CLOCK_PERIOD/2, SC_MS);
		}
	}

};

int sc_main(int argc, char *argv[])
{	
	// SIGNALS
	sc_signal<bool> clock;
	sc_signal<int> image_index;

	// NETWORK SIGNALS
	sc_signal<bool> m_network;
	sc_signal<bool> m_request[NUM_DEVICES], m_packet[NUM_DEVICES], m_response[NUM_DEVICES];

	sc_fifo<int> mobile_to_server[NUM_DEVICES];
	sc_fifo<int> server_to_mobile[NUM_DEVICES];

	// MODULES
	mobile<NUM_IMAGES, IMAGE_SIZE_X, IMAGE_SIZE_Y, PACKET_SIZE, SERVER_PACKET_SIZE, CLOCK_PERIOD, BANDWIDTH> mobile1("mobile1", "input1.txt", rois);
	mobile<NUM_IMAGES, IMAGE_SIZE_X, IMAGE_SIZE_Y, PACKET_SIZE, SERVER_PACKET_SIZE, CLOCK_PERIOD, BANDWIDTH> mobile2("mobile2", "input1.txt", rois);
	mobile<NUM_IMAGES, IMAGE_SIZE_X, IMAGE_SIZE_Y, PACKET_SIZE, SERVER_PACKET_SIZE, CLOCK_PERIOD, BANDWIDTH> mobile3("mobile3", "input1.txt", rois);

	mobile1.clock(clock);
	mobile1.image_index(image_index);
	mobile1.m_network(m_network);
	mobile1.m_request(m_request[0]);
	mobile1.m_packet(m_packet[0]);
	mobile1.m_response(m_response[0]);
	mobile1.data_in(server_to_mobile[0]);
	mobile1.data_out(mobile_to_server[0]);

	mobile2.clock(clock);
	mobile2.image_index(image_index);
	mobile2.m_network(m_network);
	mobile2.m_request(m_request[1]);
	mobile2.m_packet(m_packet[1]);
	mobile2.m_response(m_response[1]);
	mobile2.data_in(server_to_mobile[1]);
	mobile2.data_out(mobile_to_server[1]);

	mobile3.clock(clock);
	mobile3.image_index(image_index);
	mobile3.m_network(m_network);
	mobile3.m_request(m_request[2]);
	mobile3.m_packet(m_packet[2]);
	mobile3.m_response(m_response[2]);
	mobile3.data_in(server_to_mobile[2]);
	mobile3.data_out(mobile_to_server[2]);

	server<NUM_IMAGES, IMAGE_SIZE_X, IMAGE_SIZE_Y, PACKET_SIZE, SERVER_PACKET_SIZE, NUM_DEVICES, BANDWIDTH> server("server");
	server.clock(clock);
	server.image_index(image_index);
	server.m_network(m_network);
	for (int i = 0; i < NUM_DEVICES; ++i)
	{
		server.m_request[i](m_request[i]);
		server.m_packet[i](m_packet[i]);
		server.m_response[i](m_response[i]);
		server.data_in[i](mobile_to_server[i]);
		server.data_out[i](server_to_mobile[i]);
	}

	stimulus<40000> stimulus("stim");
	stimulus.clock(clock);

	// TRACES
	sc_trace_file *tf = sc_create_vcd_trace_file("sim_trace");
	sc_trace(tf, clock, "clock");
	sc_trace(tf, m_network, "m_network");
	for (int i = 0; i < NUM_DEVICES; ++i)
	{
		sc_trace(tf, m_request[i], "m_request");
		sc_trace(tf, m_response[i], "m_response");
		sc_trace(tf, m_packet[i], "m_packet");
	}

	// START SIM
	sc_start(400000, SC_MS);
	sc_close_vcd_trace_file(tf);

	cout << "Sim stopped at " << sc_time_stamp() << endl;

	return 0;
};

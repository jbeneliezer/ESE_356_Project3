#include "processing.cpp"
#include "server.cpp"
#include "mobile.cpp"

#include "systemc.h"
#include <string>

#define CLOCK_FREQUENCY 10
#define IMAGE_SIZE_X 1024
#define IMAGE_SIZE_Y 1024
#define PACKET_SIZE 20
#define FIFO_SIZE 16
#define NUM_DEVICES 3

using namespace std;

packet transfer_packet;

vector<vector<roi>> images =
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
		// SC_THREAD(prc_image);
	}

	// PROCESS
	void main()
	{
		for (int i = 0; i < program_size * 10; ++i)
		{
			// switch_image.notify(10, SC_SEC);
			clock = 1;
			wait(CLOCK_FREQUENCY/2, SC_MS);
			clock = 0;
			wait(CLOCK_FREQUENCY/2, SC_MS);
		}
	}

// 	void prc_image()
// 	{
// 		while (true)
// 		{
// 			for (int i = 0; i < images.size(); ++i)
// 			{
// 				img_ptr.write(&images[i]);
// 				wait(switch_image);
// 			}
// 		}
// 	}
// private:
// 	sc_event switch_image;
};

int sc_main(int argc, char *argv[])
{			

	// SIGNALS
	sc_signal<bool> clock;
	sc_signal<int> x[NUM_DEVICES], y[NUM_DEVICES]; // coordinates

	// NETWORK SIGNALS
	sc_signal<bool> m_network;
	sc_signal<bool> m_request[NUM_DEVICES], m_packet[NUM_DEVICES], m_response[NUM_DEVICES];

	// MODULES
	mobile mobile1("mobile1", "input2.txt");
	mobile mobile2("mobile2", "input2.txt");
	mobile mobile3("mobile3", "input2.txt");

	mobile1.clock(clock);
	mobile1.x(x[0]);
	mobile1.y(y[0]);

	mobile2.clock(clock);
	mobile2.x(x[1]);
	mobile2.y(y[1]);

	mobile3.clock(clock);
	mobile3.x(x[2]);
	mobile3.y(y[2]);

	processing<IMAGE_SIZE_X, IMAGE_SIZE_Y, PACKET_SIZE> processing1("processing1", &images[0]);
	processing<IMAGE_SIZE_X, IMAGE_SIZE_Y, PACKET_SIZE> processing2("processing2", &images[0]);
	processing<IMAGE_SIZE_X, IMAGE_SIZE_Y, PACKET_SIZE> processing3("processing3", &images[0]);

	processing1.clock(clock);
	processing1.x(x[0]);
	processing1.y(y[0]);
	processing1.m_request(m_request[0]);
	processing1.m_packet(m_packet[0]);
	processing1.m_response(m_response[0]);
	processing1.m_network(m_network);

	processing2.clock(clock);
	processing2.x(x[1]);
	processing2.y(y[1]);
	processing2.m_request(m_request[1]);
	processing2.m_packet(m_packet[1]);
	processing2.m_response(m_response[1]);
	processing2.m_network(m_network);

	processing3.clock(clock);
	processing3.x(x[2]);
	processing3.y(y[2]);
	processing3.m_request(m_request[2]);
	processing3.m_packet(m_packet[2]);
	processing3.m_response(m_response[2]);
	processing3.m_network(m_network);

	server<IMAGE_SIZE_X, IMAGE_SIZE_Y, PACKET_SIZE, NUM_DEVICES> server("server");
	server.clock(clock);
	server.m_network(m_network);
	for (int i = 0; i < NUM_DEVICES; ++i)
	{
		server.m_request[i](m_request[i]);
		server.m_packet[i](m_packet[i]);
		server.m_response[i](m_response[i]);
	}

	stimulus<2400> stimulus("stim");
	stimulus.clock(clock);

	// TRACES
	sc_trace_file *tf = sc_create_vcd_trace_file("sim_trace");
	sc_trace(tf, clock, "clock");
	sc_trace(tf, m_network, "m_network");
	for (int i = 0; i < NUM_DEVICES; ++i)
	{
		sc_trace(tf, m_request[i], "m_request " + to_string(i + 1));
		sc_trace(tf, m_response[i], "m_response " + to_string(i + 1));
		sc_trace(tf, m_packet[i], "m_packet " + to_string(i + 1));
	}

	// START SIM
	sc_start(24000, SC_MS);
	sc_close_vcd_trace_file(tf);
	cout << "Sim stopped at " << sc_time_stamp() << endl;

	return 0;
};

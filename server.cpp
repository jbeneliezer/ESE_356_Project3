#ifndef SERVER_H
#define SERVER_H

#define SCALE 1

// Case 1
#define DELTA 0.001
#define GAMMA 0

#define IMAGE_1 300
#define IMAGE_2 320
#define IMAGE_3 325
#define IMAGE_4 330
#define IMAGE_5 350

// Case 2
// #define DELTA 0.01
// #define GAMMA 0.001

// #define IMAGE_1 250
// #define IMAGE_2 275
// #define IMAGE_3 300
// #define IMAGE_4 325
// #define IMAGE_5 350

#include "systemc.h"
#include <vector>
#include <stdlib.h>		// for srand, rand
#include <time.h>		// for time

using namespace std;

typedef vector<tuple<int, int, int> > packet;

template <int num_images, int image_size_x, int image_size_y, int packet_size, int server_packet_size, int num_devices, int bandwidth>
class server : public sc_module
{
public:
	//PORTS
	sc_in<bool> clock{"clock"};
	sc_out<int> image_index{"image_index"};
	
	//NETWORK PORTS
	sc_vector<sc_in<bool> > m_request, m_packet;
	sc_out<bool> m_network{"m_network"};
	sc_vector<sc_out<bool> > m_response;

	// CHANNELS
	sc_fifo_in<int> data_in{"data_in"};
	sc_vector<sc_fifo_out<int> > data_out;

	//CONSTRUCTOR
	SC_HAS_PROCESS(server);

	server(sc_module_name name)
	: sc_module(name), m_request("m_request", num_devices), m_packet("m_packet", num_devices), m_response("m_response", num_devices),
		data_out("data_out", num_devices, sc_fifo_out<int>(server_packet_size))
	{
		SC_THREAD(prc_update)
		{
			for (int i = 0; i < num_devices; ++i)
			{
				sensitive << m_request[i].pos();
			}
		}

		SC_THREAD(prc_rx)
		{
			for (int i = 0; i < num_devices; ++i)
			{
				sensitive << m_packet[i].pos();
			}
		}

		SC_THREAD(prc_tx);

		SC_THREAD(prc_image);

		// initialize images with random data
		srand(time(0));
		for (auto& i: images)
			for (auto& j: i)
				for (auto& k: j)
					k = int(rand() * INT64_MAX);

		rx.open("rx.txt");

		// set up image sending
		it1 = images.begin();
		it2 = it1->begin();
		it3 = it2->begin();

	}

private:
	//LOCAL VAR
	vector<packet> packets_table[num_devices];
	ofstream rx;

	// IMAGE DATA
	vector<vector<vector<int>>> images(num_images, vector<vector<int>>(image_size_x, vector<int>(image_size_y)));
	vector<vector<vector<int>>>::iterator it1[num_devices];
	vector<vector<int>>::iterator it2[num_devices];
	vector<int>::iterator it3[num_devices];

	// TIMES
	vector<int> times = {IMAGE_1, IMAGE_2, IMAGE_3, IMAGE_4, IMAGE_5};

	// EVENTS
	sc_event mobile_time;

	void prc_update()
	{
		for(;;)
		{
			double min_time[num_images] = times;
			for (int i = 0; i < num_devices; ++i)
			{
				int index = distance(server_images.begin(), it1[i]);
				for (int j = index; j < num_images; ++j)
				{
					min_time[j] -= ((j - index) * image_size_x * image_size_y + 				// calculate min start time required to load each image
						image_size_x * distance(it2[i], it1[i]->end()) - image_size_x +
						distance(it3[i], it2[i]->end()) - 1) * (sizeof(int)/bandwidth);
				}
			}

			double k = 0.8;
			double weighted_average = 1;

			for (int i = 0; i < num_images; ++i)
			{
				if (sc_time_stamp().to_seconds() > times[i])
				{
					continue;
				} else if (sc_time_stamp().to_seconds() > (min_time[i] + (packet_size * 3 * sizeof(int))/bandwidth)){
					break;
				} else {
					weighted_average = weighted_average * k + ((min_time[i] - sc_time_stamp().to_seconds())/(times[i] - sc_time_stamp().to_seconds())) * (1 - k);
				}
			}

			m_network.write(true);
			wait((DELTA + (1 - weighted_average), SC_SEC));
			m_network.write(false);
			wait((DELTA + weighted_average), SC_SEC);
			
		}

	}

	void prc_rx()
	{
		m_network.write(false);
		for (;;)
		{
			wait();
			for (int i = 0; i < num_devices; i++)
			{
				if (m_request[i].read() == true)
				{
					m_network.write(false);
					m_response[i].write(true);							 //send ack bit
					rx << sc_time_stamp() << ": accepted packet from device " << (i + 1) << "." << endl;
					wait(m_packet.pos());
					packet transfer_packet;
					int j = 0;
					for (; (j < packet_size) && (data_in.num_available >= 3); ++j)
					{
						int id = data_in.read();
						int start = data_in.read();
						int end = data_in.read();
						transfer_packet.push_back(make_tuple(id, start, end));
					}
					packets_table[i].push_back(transfer_packet);		// read packet
					m_response[i].write(false);
					wait((((j + 1) * 3 * sizeof(int))/bandwidth, SC_SEC), m_packet.neg());
					break;
				}
			}
		}
	}

	void prc_tx()
	{
		for (;;)
		{
			for (int i = 0; i < num_devices; ++i)
			{
				if (m_network == false)
				{
					if (data_out.nb_write(*it3[i]) == true) ++it3[i];								// send data
					if (it3[i] == it2[i]->end())													// line is complete
					{
						if (it2[i] == it1[i]->end())												// image is complete
						{
							if (it1[i] == server_images.end())										// all images read
							{
								break;
							} else {
								++it1[i];
								it2[i] = it1[i]->begin();
								it3[i] = it2[i]->begin();
							}
						} else {
							++it2[i];																// next line
							it3[i] = it2[i]->begin();
						}
					}
					wait((sizeof(int))/bandwidth, SC_SEC);
				}
			}
			if (all_of(it1, it1 + num_devices, [it1](vector<vector<vector<int>>>::iterator x){ return x == server_images.end();}))
			{
				for (;;)
			}
		}


	}

	void prc_image()
	{
		wait(IMAGE_1, SC_SEC);
		image_index.write(0);
		wait(IMAGE_2, SC_SEC);
		image_index.write(1);
		wait(IMAGE_3, SC_SEC);
		image_index.write(2);
		wait(IMAGE_4, SC_SEC);
		image_index.write(3);
		wait(IMAGE_5, SC_SEC);
		image_index.write(4);
	}

};

#endif /* SERVER_H */
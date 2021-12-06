#ifndef SERVER_H
#define SERVER_H

// Case 1
#define _DELTA (0.001)
#define GAMMA 0.001

#define IMAGE_1 300
#define IMAGE_2 320
#define IMAGE_3 325
#define IMAGE_4 330
#define IMAGE_5 350

// Case 2
// #define _DELTA 0.01
// #define GAMMA 0.001

// #define IMAGE_1 250
// #define IMAGE_2 275
// #define IMAGE_3 300
// #define IMAGE_4 325
// #define IMAGE_5 350

#include "systemc.h"
#include <vector>
#include <stdlib.h> // for srand, rand
#include <time.h>   // for time

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
	sc_fifo_in<int> data_in[num_devices];
	sc_fifo_out<int> data_out[num_devices];

	//CONSTRUCTOR
	SC_HAS_PROCESS(server);

	server(sc_module_name name)
	: sc_module(name), m_request("m_request", num_devices), m_packet("m_packet", num_devices), m_response("m_response", num_devices)
	{

		SC_THREAD(prc_update);

		SC_THREAD(prc_rx)
		{
			for (int i = 0; i < num_devices; ++i)
			{
				sensitive << m_request[i].pos() << m_packet[i].pos();
			}
		}

		SC_THREAD(prc_tx);

		SC_THREAD(prc_image);

		// initialize image
		images = vector<vector<vector<int> > >(num_images, vector<vector<int> >(image_size_x, vector<int>(image_size_y)));

		// initialize images with random data
		srand(time(0));
		for (auto& i: images)
			for (auto& j: i)
				for (auto& k: j)
					k = int(rand());

		rx.open("rx.txt");

		// set up image sending
		for (int i = 0; i < num_devices; ++i)
		{
			it1[i] = images.begin();
			it2[i] = it1[i]->begin();
			it3[i] = it2[i]->begin();
		}

		// set up packets table
		packets_table = vector<vector<packet> >(num_devices, vector<packet>());

		mobile_time = false;
	}

private:
	//LOCAL VAR
	vector<vector<packet> > packets_table;
	ofstream rx;

	// IMAGE DATA
	vector<vector<vector<int> > > images;
	vector<vector<vector<int> > >::iterator it1[num_devices];
	vector<vector<int>>::iterator it2[num_devices];
	vector<int>::iterator it3[num_devices];

	// TIMES
	vector<int> times = {IMAGE_1, IMAGE_2, IMAGE_3, IMAGE_4, IMAGE_5};

	// EVENTS
	sc_event mobiles;
	bool mobile_time;

	void prc_update()
	{
		for(;;)
		{
			// double min_time[num_images];
			// //copy(times.begin(), times.end(), min_time);
			// for (int i = 0; i < num_devices; ++i)
			// {
			// 	min_time[i] = times[i];
			// 	int index = distance(images.begin(), it1[i]);
			// 	for (int j = index; j < num_images; ++j)
			// 	{
			// 		min_time[j] -= ((j - index) * image_size_x * image_size_y + 				// calculate min start time required to load each image
			// 			image_size_x * distance(it2[i], it1[i]->end()) - image_size_x +
			// 			distance(it3[i], it2[i]->end()) - 1) * (sizeof(int)/bandwidth);
			// 	}
			// }

			// double k = 0.5;
			// double weighted_average = 1;

			// for (auto& i: min_time)
			// {
			// 	cout << i << " ";
			// }
			// cout << endl;

			// for (int i = 0; i < num_images; ++i)
			// {
			// 	if (sc_time_stamp().to_seconds() > times[i])
			// 	{
			// 		continue;
			// 	} else if (sc_time_stamp().to_seconds() > (min_time[i] + (packet_size * 3 * sizeof(int))/bandwidth)){
			// 		break;
			// 	} else {
			// 		weighted_average = weighted_average * k + ((min_time[i] - sc_time_stamp().to_seconds())/(times[i] - sc_time_stamp().to_seconds())) * (1 - k);
			// 	}
			// }

			if (sc_time_stamp().to_seconds() > IMAGE_1)
			{
				// open network to mobile devices
				mobile_time = true;
				mobiles.notify();
				wait(10, SC_SEC);
			}

			// server time
			mobile_time = false;
			wait(10, SC_SEC);
			
		}

	}

	void prc_rx()
	{
		m_network.write(false);
		for (;;)
		{
			while (mobile_time)
			{
				m_network.write(true);
				wait();
				for (int i = 0; i < num_devices; i++)
				{
					if (m_request[i].read() == true)
					{
						m_network.write(false);
						m_response[i].write(true);							 //send ack bit
						rx << sc_time_stamp() << ": accepted packet from device " << (i + 1) << "." << endl;
						wait();
						packet transfer_packet;
						int j = 0;
						for (; (j < packet_size) && (data_in[i].num_available() >= 3); ++j)
						{
							int id = data_in[i].read();
							int start = data_in[i].read();
							int end = data_in[i].read();
							transfer_packet.push_back(make_tuple(id, start, end));
						}
						packets_table[i].push_back(transfer_packet);		// read packet
						m_response[i].write(false);
					}
				}
			}
			m_network.write(false);
			wait(mobiles);
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
					for (int k = 0; k < server_packet_size; ++k)
					{
						if (data_out[i].nb_write(*it3[i]) == true) ++it3[i];								// send data
						if (it3[i] == it2[i]->end())													// line is complete
						{
							if (it2[i] == it1[i]->end())												// image is complete
							{
								if (it1[i] == images.end())										// all images read
								{
									continue;
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
						wait(_DELTA, SC_SEC);
					}
				} else {
					wait(_DELTA, SC_SEC);
				}
			}
			bool finished = true;
			for (int i = 0; i < num_devices; ++i)
			{
				finished = (finished && (it1[i] == images.end()));
			}
			if (finished) for (;;);
		}

	}

	void prc_image()
	{
		wait(IMAGE_1, SC_SEC);
		image_index.write(0);
		wait(IMAGE_2 - IMAGE_1, SC_SEC);
		image_index.write(1);
		wait(IMAGE_3 - IMAGE_2, SC_SEC);
		image_index.write(2);
		wait(IMAGE_4 - IMAGE_3, SC_SEC);
		image_index.write(3);
		wait(IMAGE_5 - IMAGE_4, SC_SEC);
		image_index.write(4);
	}

};

#endif /* SERVER_H */

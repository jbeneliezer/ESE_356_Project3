#ifndef MOBILE_H
#define MOBILE_H

#define GAMMA 0
// #define GAMMA 0.001

#include "systemc.h"
#include <vector>
#include <sstream>
#include <fstream>
#include <stdlib.h>		// for srand, rand
#include <time.h>		

using namespace std;

typedef struct
{
	int id, start[2], end[2];
} roi;

typedef struct
{
	int index;
	int start_time;
	int num_of_roi;
	vector<roi> rois(num_of_roi);
	bool active;
} image;

typedef vector<tuple<int, int, int>> packet;

template <int num_images, int image_size_x, int image_size_y, int packet_size, int server_packet_size, int clock_period, int bandwidth>
class mobile : public sc_module
{
public:
	// PORTS
	sc_in<bool> clock{"clock"};
	sc_in<int> image_index{"image_index"};

	// NETWORK PORTS
	sc_in<bool> m_network{"m_network"}, m_response{"m_response"};
	sc_out<bool> m_request{"m_request"}, m_packet{"m_packet"};

	// CHANNELS
	sc_fifo_in<int> data_in;
	sc_fifo_out<int> data_out;

	// CONSTRUCTOR
	SC_HAS_PROCESS(mobile);

	mobile(sc_module_name name, string file_name, vector<vector<roi>> &roi_ref)
	: sc_module(name), data_in("data_in", (server_packet_size)), data_out("data_out", (packet_size * 3))
	{
		SC_METHOD(prc_update);
		sensitive << clock.pos();

		SC_THREAD(prc_tx);
		sensitive << m_network.pos() << m_response.pos();

		SC_METHOD(prc_input);
		sensitive << clock.pos();

		transmit_packet_counter = 0;
		tuple_counter = 0;
		time_index = 0;
		id = name;
		tx.open(this->id + ".txt");

		// set up gaze input
		fin.open(file_name);
		if (fin.is_open())
		{
			while (getline(fin, str))
			{
				stringstream ss(str);
				ss >> x >> y;
				coordinates.push_back(make_pair(x, y));
			}
		}
		else
		{
			cout << "error opening " << file_name << "." << endl;
			exit(1);
		}
		fin.close();
		input_ptr = coordinates.begin();

		// set up image table
		for (int i = 0; i < num_images, ++i)
		{
			image_table[i].start_time = -1;
			image_table[i].index = i + 1;
			image_table[i].num_of_roi = roi_ref[i].size();
			for (int j = 0; j < roi_ref[i].size(); ++j)
			{
				image_table[i].rois[j] = roi_ref[i][j];
			}
			image_table[i].active = false;
		}

		// set up server image read
		it1 = server_images.begin();
		it2 = it1->begin();
		it3 = it2->begin();

		srand(time(0));
	}

private:
	// LOCAL VAR
	// ROI HANDLING
	vector<image> image_table(num_images);

	// INPUT HANDLING
	ifstream fin;
	string str;
	int x, y;
	vector<pair<int, int>> coordinates = vector<pair<int, int>>(), *input_ptr;

	// NETWORK HANDLING
	int transmit_packet_counter, tuple_counter, time_index;
	sc_event packet_ready;
	vector<packet> packets = vector<packet>(1, packet(1));
	ofstream tx;

	// IMAGE HANDLING
	vector<vector<vector<int> > > server_images(num_images, vector<vector<int> >(image_size_x, vector<int>(image_size_y)));
	vector<vector<vector<int> > >::iterator it1;
	vector<vector<int> >::iterator it2;
	vector<int>::iterator it3;

	// PROCESSES
	// read data, update packets
	void prc_update()
	{
		while (image_table[image_index.read()].active == false)
		{
			//time_index += clock_period;
			return;
		}
		if (image_table[image_index.read()].start_time == -1) image_table[image_index.read()].start_time = time_index;		// set image start time
		// cout << sc_time_stamp() << ": " << this->id << ": (" << x.read() << ", " << y.read() << ")" << endl;
		for (auto &i : image_table[image_index.read()].rois) // iterate over rois
		{
			// cout << "(" << i.start[0] << ", " << i.start[1] << ") -> (" << i.end[0] << ", " << i.end[1] << ")" << endl;
			if (get<0>(*input_ptr) >= i.start[0] && get<0>(*input_ptr) <= i.end[0] && get<1>(*input_ptr) >= i.start[1] && get<1>(*input_ptr) <= i.end[1]) // (x, y) is in roi
			{
				if (get<0>(packets[transmit_packet_counter][tuple_counter]) == 0)
				{
					packets[transmit_packet_counter][tuple_counter] = make_tuple(i.id, (int) sc_time_stamp().to_double(), (int) sc_time_stamp().to_double());
				}
				else if ((get<0>(packets[transmit_packet_counter][tuple_counter])) == i.id) // still in same roi
				{
					get<2>((tuple<int, int, int> &)packets[transmit_packet_counter][tuple_counter]) = (int) sc_time_stamp().to_double();
				}
				else if (tuple_counter >= (packet_size - 1)) // new packet
				{
					packet p = packet();
					p.push_back(make_tuple(i.id, int(sc_time_stamp().to_double()), (int) sc_time_stamp().to_double()));
					packets.push_back(p);
					tuple_counter = 0;
					++transmit_packet_counter;
				}
				else // new roi
				{
					packets[transmit_packet_counter].push_back(make_tuple(i.id, (int)sc_time_stamp().to_double(), (int) sc_time_stamp().to_double()));
					++tuple_counter;
				}
				break;
			}
		}
		//time_index += clock_period;
		if (transmit_packet_counter > 0)
			packet_ready.notify();

		// cout << "Packets: " << endl;
		// for (auto& i: packets)
		// {
		// 	cout << ": ";
		// 	for (auto& j: i)
		// 		cout << "(" << get<0>(j) << ", " << get<1>(j) << ", " << get<2>(j) << ") ";
		// 	cout << endl;
		// }
	}

	// send data to server
	void prc_tx()
	{
		m_request.write(false);
		m_packet.write(false);
		while (true) // infinite loop
		{
			wait(packet_ready); // initiated by prc_update()
			tx << sc_time_stamp() << ": packet ready from " << this->id << endl;
			while (transmit_packet_counter > 0) // there are packets to transmit to server
			{
				if (m_network.read() != true) // check if network is free
				{
					wait(rand() * GAMMA, SC_SEC); // wait for some random time
				}
				else // if network is free
				{
					m_request.write(true);
					tx << sc_time_stamp() << ": request from " << this->id << " sent." << endl;
					wait((rand() * GAMMA, SC_SEC), m_response.pos());
					m_request.write(false);
					if (m_response.read() == 0)
					{
						tx << sc_time_stamp() << ": request from " << this->id << " denied." << endl;
						wait(rand() * GAMMA, SC_SEC);
					}
					else
					{
						tx << sc_time_stamp() << ": request from " << this->id << " accepted." << endl;
						m_packet.write(true);
						if (data_out.num_free() >= packet_size * 3)
						{
							for (auto& i: packets.front())
							{
								data_out.write(get<0>(i));
								data_out.write(get<1>(i));
								data_out.write(get<2>(i));
							}
							packets.erase(packets.begin());	   // remove first packet
							--transmit_packet_counter;
							wait((packet_size * 3 * sizeof(int)) / bandwidth, SC_SEC);
						}
						m_packet.write(false);
					}
				}
			}
		}
	}

	// receive data from server
	void prc_rx()
	{
		for (;;)
		{
			wait(data_in.data_written_event());
			while(data_in.num_available > 0)
			{
				*it3++ = data_in.read();
				if (it3 == it2->end())													// line is complete
				{
					if (it2 == it1->end())												// image is complete
					{
						image_table[distance(server_image.begin(), it1)].active = true;
						if (it1 == server_images.end())									// all images read
						{
							for(;;)
						} else {
							++it1;
							it2 = it1->begin();
							it3 = it2->begin();
						}
					} else {
						++it2;															// next line
						it3 = it2->begin();
					}
				}
			}
		
		}
	}

	// increment input pointer
	void prc_input()
	{
		if (input_ptr != prev(coordinates.end())) input_ptr = next(input_ptr);
	}

	// void print_stat()
	// {
	// }
};

#endif /* MOBILE_H */

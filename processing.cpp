#ifndef PROCESSING_H
#define PROCESSING_H

#include "systemc.h"
#include <iostream>
#include <vector>
#include <array>
#include <math.h>
#include <sstream>
#include <fstream>

#define CLOCK_FREQUENCY 10
#define GAMMA 0.001
#define NETWORK_SPEED (0.5e6)

using namespace std;

typedef struct
{
	int id, start[2], end[2];
} roi;

typedef vector<tuple<int, int, int> > packet;

template <int image_size_x, int image_size_y, int packet_size>
class processing : public sc_module
{
public:
	// PORTS
	sc_in<bool> clock{"clock"};
	sc_in<int> x{"x"}, y{"y"};

	// NETWORK PORTS
	sc_in<bool> m_network, m_response;
	sc_out<bool> m_request, m_packet;

	// CONSTRUCTOR
	SC_HAS_PROCESS(processing);

	processing(sc_module_name name, vector<roi> *img) : sc_module(name), image_ptr(img)
	{
		SC_METHOD(prc_update);
		sensitive << clock.pos();

		SC_THREAD(prc_tx);
		sensitive << m_network.pos() << m_response.pos();

		transmit_packet_counter = 0;
		tuple_counter = 0;
		time_index = 0;
		id = name;
		tx.open(this->id + ".txt");
	}

private:
	// LOCAL VAR
	vector<roi> *image_ptr; // pointer to image data
	int transmit_packet_counter, tuple_counter, time_index;
	sc_event packet_ready;
	vector<packet> packets = vector<packet>(1, packet(1));
	packet transfer_packet = packet(20);
	string id;
	ofstream tx;

	// PROCESS

	void prc_update()
	{
		// cout << sc_time_stamp() << ": " << this->id << ": (" << x.read() << ", " << y.read() << ")" << endl;
		for (auto &i : *image_ptr) // iterate over rois
		{
			// cout << "(" << i.start[0] << ", " << i.start[1] << ") -> (" << i.end[0] << ", " << i.end[1] << ")" << endl;
			if (x.read() >= i.start[0] && x.read() <= i.end[0] && y.read() >= i.start[1] && y.read() <= i.end[1]) // (x, y) is in roi
			{
				if (get<0>(packets[transmit_packet_counter][tuple_counter]) == 0)
				{
					packets[transmit_packet_counter][tuple_counter] = make_tuple(i.id, time_index, time_index);
				}
				else if ((get<0>(packets[transmit_packet_counter][tuple_counter])) == i.id) // still in same roi
				{
					get<2>((tuple<int, int, int> &)packets[transmit_packet_counter][tuple_counter]) = time_index;
				}
				else if (tuple_counter >= (packet_size - 1)) // new packet
				{
					packet p = packet();
					p.push_back(make_tuple(i.id, time_index, time_index));
					packets.push_back(p);
					tuple_counter = 0;
					++transmit_packet_counter;
				}
				else // new roi
				{
					packets[transmit_packet_counter].push_back(make_tuple(i.id, time_index, time_index));
					++tuple_counter;
				}
				break;
			}
		}
		time_index += CLOCK_FREQUENCY;
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

	void prc_tx()
	{
		m_request.write(false);
		m_packet.write(false);
		while (true) // infinite loop
		{
			wait(packet_ready);					// initiated by prc_update()
			tx << sc_time_stamp() << ": packet ready from " << this->id << endl;
			while (transmit_packet_counter > 0) // there are packets to transmit to server
			{
				if (m_network.read() != true) // check if network is free
				{
					wait(GAMMA, SC_MS); // wait for some random time
				}
				else // if network is free
				{
					m_request.write(true);
					tx << sc_time_stamp() << ": request from " << this->id << " sent." << endl;;
					wait();
					if (m_response.read() == 0)
					{
						tx << sc_time_stamp() << ": request from " << this->id << " denied." << endl;
						wait(GAMMA, SC_MS);
					}
					else
					{
						tx << sc_time_stamp() << ": request from " << this->id << " accepted." << endl;
						m_packet.write(true);
						transfer_packet = packets.front(); // write first packet
						packets.erase(packets.begin());	   // remove first packet
						--transmit_packet_counter;
						wait((packet_size * 3 * 64)/NETWORK_SPEED, SC_SEC);
						m_packet.write(false);
					}
					m_request.write(false);
				}
			}
		}
	}

	// void prc_rx()
	// {
	// }

	// void print_stat()
	// {
	// }
};

#endif /* PROCESSING_H */

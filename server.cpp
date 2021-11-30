#ifndef SERVER_H
#define SERVER_H

#include "systemc.h"
#include <array>
#include <vector>

using namespace std;

typedef vector<tuple<int, int, int> > packet;

template <int image_size_x, int image_size_y, int packet_size, int num_devices>
class server : public sc_module
{
public:
	//PORTS
	sc_in<bool> clock{"clock"};
	
	//NETWORK PORTS
	sc_in<bool> m_request[num_devices], m_packet[num_devices];
	sc_out<bool> m_network;
	sc_out<bool> m_response[num_devices];

	//CONSTRUCTOR
	SC_HAS_PROCESS(server);

	server(sc_module_name name) : sc_module(name)
	{
		SC_THREAD(prc_rx)
		{
			for (int i = 0; i < num_devices; ++i)
			{
				sensitive << m_request[i].pos() << m_packet[i].pos();
			}
		}
		rx.open("rx.txt");
	}

private:
	//LOCAL VAR
	vector<packet> packets_table[num_devices];
	ofstream rx;

	void prc_rx()
	{
		m_network.write(false);
		while (true)
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
					//packets_table[i].push_back(transfer_packet);		// read packet
					m_response[i].write(false);
				}
			}
		}
	}

};

#endif /* SERVER_H */
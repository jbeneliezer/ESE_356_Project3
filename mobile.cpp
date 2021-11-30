#ifndef MOBILE_H
#define MOBILE_H

#include "systemc.h"
#include <string>
#include <sstream>
#include <fstream>
#include <vector>

using namespace std;

class mobile : public sc_module
{
public:
    sc_in<bool> clock;
    sc_out<int> x, y;

    SC_HAS_PROCESS(mobile);

    mobile(sc_module_name name, string file_name) : sc_module(name)
    {
        SC_METHOD(send_data);
        sensitive << clock.pos();

        fin.open(file_name);

        if (fin.is_open())
        {
            while (getline(fin, str))
            {
                stringstream ss(str);
                ss >> _x >> _y;
                coordinates.push_back(make_pair(_x, _y));
            }
        } else {
            cout << "error opening " << file_name << "." << endl;
            exit(1);
        }

        fin.close();
    }

private:
    // LOCAL VAR
    ifstream fin;
    string str;
    int _x, _y;
    vector<pair<int, int> > coordinates = vector<pair<int, int> >();

    void send_data()
    {
        if (coordinates.empty())
        {
            cout << sc_time_stamp() << ": error, no data." << endl;
            return;
        }        
        x.write(get<0>(coordinates.front()));
        y.write(get<1>(coordinates.front()));
        coordinates.erase(coordinates.begin());
    }
};

#endif /* MOBILE_H */
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <string>
#include <cstdlib>
#define SENSOR "/sys/bus/w1/devices/28-0000055587df/w1_slave"



using namespace std;

int main()
{
	
	
	string line;
	string temp="";
	float temperature;

	
	
	while(1)
	{
		ifstream sensor(SENSOR);
		if (sensor.is_open())
		{
			
			while ( getline (sensor,line) )
			{
			  	//cout << line << '\n';
				temp+=line;
			}
			sensor.close();
			//cout << temp << '\n';
			int pos = temp.find("t=");
			//cout << pos << endl;
			if(pos>0) 
			{
				temp=temp.substr(pos+2);
				//cout << temp << endl;
				temperature = stof(temp);
				cout << temperature/1000 << "°C" << endl;
			}
			else cout << "Impossible de lire la temperature !" << endl;
		}
		else cout << "Lecture sensor impossible" << endl;
	}
}

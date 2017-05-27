#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

#include <vector>
#include <string>



/************************************************************************/
/*       SuperTerminal Sample                                           */
/************************************************************************/
#if 0
#define BOOST_REGEX_NO_LIB
#define BOOST_DATE_TIME_SOURCE
#define BOOST_SYSTEM_NO_LIB
#include "SuperTerminal.h"

int main()
{
	try
	{
		SuperTerminal sp("COM2");
		while (1)
		{
			sp.write_to_serial("serialPort");
			sp.read_from_serial();
			sp.call_handle();
			Sleep(1000);
		}
		return 0;
	}
	catch (std::exception &e)
	{
		cout << e.what();
		getchar();
	}
}
#endif

/************************************************************************/
/*       SimpleSerial Sample                                            */
/************************************************************************/
#if 0
#include <iostream>
#include "SimpleSerial.h"

using namespace std;
using namespace boost;

int main(int argc, char* argv[])
{
	try {

		SimpleSerial serial("COM2", 115200);

		serial.writeString("Hello world\n");

		cout << serial.readLine() << endl;

	}
	catch (boost::system::system_error& e)
	{
		cout << "Error: " << e.what() << endl;
		return 1;
	}
}
#endif


/************************************************************************/
/*       AsyncSerial Sample                                             */
/************************************************************************/
#if 0
#include "AsyncSerial.h"

#include <iostream>

#ifndef _WIN32
#	include <termios.h>
#endif

#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>

using namespace std;

void received(const char *data, unsigned int len)
{
	vector<char> v(data, data + len);
	for (unsigned int i = 0; i < v.size(); i++)
	{
		if (v[i] == '\n')
		{
			cout << endl;
		}
		else {
			if (v[i] < 32 || v[i] >= 0x7f) cout.put(' ');//Remove non-ascii char
			else cout.put(v[i]);
		}
	}
	cout.flush();//Flush screen buffer
}

int main(int argc, char* argv[])
{
	if (argc != 3)
	{
		cerr << "Usage: serial port baudrate" << endl <<
			"To quit type Ctrl-C x" << endl <<
			"To send Ctrl-C type Ctrl-C Ctrl-C" << endl;
		return 1;
	}

#ifndef _WIN32
	termios stored_settings;
	tcgetattr(0, &stored_settings);
	termios new_settings = stored_settings;
	new_settings.c_lflag &= (~ICANON);
	new_settings.c_lflag &= (~ISIG); // don't automatically handle control-C
	new_settings.c_lflag &= ~(ECHO); // no echo
	tcsetattr(0, TCSANOW, &new_settings);

	cout << "\e[2J\e[1;1H"; //Clear screen and put cursor to 1;1
#endif

	try {
		CallbackAsyncSerial serial(argv[1],
			boost::lexical_cast<unsigned int>(argv[2]));
		serial.setCallback(received);
		serial.writeString("Hello world\n");

		for (;;)
		{
			if (serial.errorStatus() || serial.isOpen() == false)
			{
				cerr << "Error: serial port unexpectedly closed" << endl;
				break;
			}
			char c;
			cin.get(c); //blocking wait for standard input
			if (c == 3) //if Ctrl-C
			{
				cin.get(c);
				switch (c)
				{
				case 3:
					serial.write(&c, 1);//Ctrl-C + Ctrl-C, send Ctrl-C
					break;
				case 'x': //fall-through
				case 'X':
					goto quit;//Ctrl-C + x, quit
				default:
					serial.write(&c, 1);//Ctrl-C + any other char, ignore
				}
			}
			else serial.write(&c, 1);
		}
	quit:
		serial.close();
	}
	catch (std::exception& e) {
		cerr << "Exception: " << e.what() << endl;
	}

#ifndef _WIN32
	tcsetattr(0, TCSANOW, &stored_settings);
#endif
}
#endif

/************************************************************************/
/*       minicomc-lient Sample                                          */
/************************************************************************/
#include "minicom_client .h"
int main(int argc, char* argv[])
{
	// on Unix POSIX based systems, turn off line buffering of input, so cin.get() returns after every keypress 
	// On other systems, you'll need to look for an equivalent 
#ifdef POSIX 
	termios stored_settings;
	tcgetattr(0, &stored_settings);
	termios new_settings = stored_settings;
	new_settings.c_lflag &= (~ICANON);
	new_settings.c_lflag &= (~ISIG); // don't automatically handle control-C 
	tcsetattr(0, TCSANOW, &new_settings);
#endif 
	try
	{
		if (argc != 3)
		{
			cerr << "Usage: minicom <device> <baud> \n";
			return 1;
		}
		boost::asio::io_service io_service;
		// define an instance of the main class of this program 
		minicom_client c(io_service, argv[1], boost::lexical_cast<unsigned int>(argv[2]));
		// run the IO service as a separate thread, so the main thread can block on standard input 
		boost::thread t(boost::bind(&boost::asio::io_service::run, &io_service));
		while (c.active()) // check the internal state of the connection to make sure it's still running 
		{
			char ch;
			cin.get(ch); // blocking wait for standard input 
			if (ch == 3) // ctrl-C to end program 
				break;
			c.write(ch);
		}
		c.close(); // close the minicom client connection 
		t.join(); // wait for the IO service thread to close 
	}
	catch (exception& e)
	{
		cerr << "Exception: " << e.what() << "\n";
	}
#ifdef POSIX // restore default buffering of standard input 
	tcsetattr(0, TCSANOW, &stored_settings);
#endif 
	return 0;
}
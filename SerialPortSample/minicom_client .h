#pragma once
/* minicom.cpp
A simple demonstration minicom client with Boost asio

Parameters:
baud rate
serial port (eg /dev/ttyS0 or COM1)

To end the application, send Ctrl-C on standard input
*/

#include <deque> 
#include <iostream> 
#include <boost/bind.hpp> 
#include <boost/asio.hpp> 
#include <boost/asio/serial_port.hpp> 
#include <boost/thread.hpp> 
#include <boost/lexical_cast.hpp> 
#include <boost/date_time/posix_time/posix_time_types.hpp> 

#ifdef POSIX 
#include <termios.h> 
#endif 

using namespace std;

class minicom_client
{
public:
	minicom_client(boost::asio::io_service& io_service, const string& device, unsigned int baud)
		: active_(true),
		io_service_(io_service),
		serialPort(io_service, device)
	{
		if (!serialPort.is_open())
		{
			cerr << "Failed to open serial port\n";
			return;
		}
		boost::asio::serial_port_base::baud_rate baud_option(baud);
		serialPort.set_option(baud_option); // set the baud rate after the port has been opened 
		read_start();
	}

	void write(const char msg) // pass the write data to the do_write function via the io service in the other thread 
	{
		io_service_.post(boost::bind(&minicom_client::do_write, this, msg));
	}

	void close() // call the do_close function via the io service in the other thread 
	{
		io_service_.post(boost::bind(&minicom_client::do_close, this, boost::system::error_code()));
	}

	bool active() // return true if the socket is still active 
	{
		return active_;
	}

private:

	static const int max_read_length = 512; // maximum amount of data to read in one operation 

	void read_start(void)
	{ // Start an asynchronous read and call read_complete when it completes or fails 
		serialPort.async_read_some(boost::asio::buffer(read_msg_, max_read_length),
			boost::bind(&minicom_client::read_complete,
				this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));
	}

	void read_complete(const boost::system::error_code& error, size_t bytes_transferred)
	{ // the asynchronous read operation has now completed or failed and returned an error 
		if (!error)
		{ // read completed, so process the data 
			cout.write(read_msg_, bytes_transferred); // echo to standard output 
			read_start(); // start waiting for another asynchronous read again 
		}
		else
			do_close(error);
	}

	void do_write(const char msg)
	{ // callback to handle write call from outside this class 
		bool write_in_progress = !write_msgs_.empty(); // is there anything currently being written? 
		write_msgs_.push_back(msg); // store in write buffer 
		if (!write_in_progress) // if nothing is currently being written, then start 
			write_start();
	}

	void write_start(void)
	{ // Start an asynchronous write and call write_complete when it completes or fails 
		boost::asio::async_write(serialPort,
			boost::asio::buffer(&write_msgs_.front(), 1),
			boost::bind(&minicom_client::write_complete,
				this,
				boost::asio::placeholders::error));
	}

	void write_complete(const boost::system::error_code& error)
	{ // the asynchronous read operation has now completed or failed and returned an error 
		if (!error)
		{ // write completed, so send next write data 
			write_msgs_.pop_front(); // remove the completed data 
			if (!write_msgs_.empty()) // if there is anthing left to be written 
				write_start(); // then start sending the next item in the buffer 
		}
		else
			do_close(error);
	}

	void do_close(const boost::system::error_code& error)
	{ // something has gone wrong, so close the socket & make this object inactive 
		if (error == boost::asio::error::operation_aborted) // if this call is the result of a timer cancel() 
			return; // ignore it because the connection cancelled the timer 
		if (error)
			cerr << "Error: " << error.message() << endl; // show the error message 
		else
			cout << "Error: Connection did not succeed.\n";
		cout << "Press Enter to exit\n";
		serialPort.close();
		active_ = false;
	}

private:
	bool active_; // remains true while this object is still operating 
	boost::asio::io_service& io_service_; // the main IO service that runs this connection 
	boost::asio::serial_port serialPort; // the serial port this instance is connected to 
	char read_msg_[max_read_length]; // data read from the socket 
	deque<char> write_msgs_; // buffered write data 
};
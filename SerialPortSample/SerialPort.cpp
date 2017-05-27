#include "StdAfx.h"

#include <Setupapi.h>
#pragma comment(lib, "Setupapi.lib")

#include "SerialPort.h"

SerialPort::SerialPort(void) : end_of_line_char_('\n')
{
}

SerialPort::~SerialPort(void)
{
	stop();
}

char SerialPort::end_of_line_char() const
{
	return this->end_of_line_char_;
}

void SerialPort::end_of_line_char(const char &c)
{
	this->end_of_line_char_ = c;
}

std::vector<std::string> SerialPort::get_port_names()
{
	std::vector<std::string> names;

	BOOL rv;
	DWORD size;
	GUID guid[1];
	HDEVINFO hdevinfo;
	DWORD idx = 0;
	SP_DEVINFO_DATA devinfo_data;
	devinfo_data.cbSize = sizeof(SP_DEVINFO_DATA);
	int count = 0;

	rv = SetupDiClassGuidsFromName("Ports", (LPGUID)&guid, 1, &size);
	if (!rv) {
		std::cout << "error : SetupDiClassGuidsFromName() failed..." << std::endl;
		return names;
	}

	hdevinfo = SetupDiGetClassDevs(&guid[0], NULL, NULL, DIGCF_PRESENT | DIGCF_PROFILE);
	if (hdevinfo == INVALID_HANDLE_VALUE) {
		std::cout << "error : SetupDiGetClassDevs() failed..." << std::endl;
		return names;
	}

	while (SetupDiEnumDeviceInfo(hdevinfo, idx++, &devinfo_data)) {
		char friendly_name[MAX_PATH];
		char port_name[MAX_PATH];
		DWORD prop_type;
		DWORD type = REG_SZ;
		HKEY hKey = NULL;

		rv = ::SetupDiGetDeviceRegistryProperty(hdevinfo, &devinfo_data, SPDRP_FRIENDLYNAME, &prop_type,
			(LPBYTE)friendly_name, sizeof(friendly_name), &size);
		if (!rv) {
			std::cout << "error : SetupDiGetDeviceRegistryProperty() failed..." << std::endl;
			continue;
		}

		hKey = ::SetupDiOpenDevRegKey(hdevinfo, &devinfo_data, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
		if (!hKey) continue;

		size = sizeof(port_name);
		rv = ::RegQueryValueEx(hKey, "PortName", 0, &type, (LPBYTE)&port_name, &size);
		::RegCloseKey(hKey);

		names.push_back(port_name);
	}

	SetupDiDestroyDeviceInfoList(hdevinfo);

	return names;
}

int SerialPort::get_port_number()
{
	std::vector<std::string> names = get_port_names();
	return names.size();
}

std::string SerialPort::get_port_name(const unsigned int &idx)
{
	std::vector<std::string> names = get_port_names();
	if (idx >= names.size()) return std::string();
	return names[idx];
}

void SerialPort::print_devices()
{
	std::cout << "SerialPort::print_devices()" << std::endl;
	int n = SerialPort::get_port_number();
	for (int i = 0; i < n; ++i) {
		std::string name = SerialPort::get_port_name(i);
		std::cout << "\t" << name.c_str() << std::endl;
	}
}

bool SerialPort::start(const char *com_port_name, int baud_rate)
{
	boost::system::error_code ec;

	if (port_) {
		std::cout << "error : port is already opened..." << std::endl;
		return false;
	}

	port_ = serial_port_ptr(new boost::asio::serial_port(io_service_));
	port_->open(com_port_name, ec);
	if (ec) {
		std::cout << "error : port_->open() failed...com_port_name="
			<< com_port_name << ", e=" << ec.message().c_str() << std::endl;
		return false;
	}

	// option settings...
	port_->set_option(boost::asio::serial_port_base::baud_rate(baud_rate));
	port_->set_option(boost::asio::serial_port_base::character_size(8));
	port_->set_option(boost::asio::serial_port_base::stop_bits(boost::asio::serial_port_base::stop_bits::one));
	port_->set_option(boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none));
	port_->set_option(boost::asio::serial_port_base::flow_control(boost::asio::serial_port_base::flow_control::none));

	boost::thread t(boost::bind(&boost::asio::io_service::run, &io_service_));

	async_read_some_();

	return true;
}

void SerialPort::stop()
{
	boost::mutex::scoped_lock look(mutex_);

	if (port_) {
		port_->cancel();
		port_->close();
		port_.reset();
	}
	io_service_.stop();
	io_service_.reset();
}

int SerialPort::write_some(const std::string &buf)
{
	return write_some(buf.c_str(), buf.size());
}

int SerialPort::write_some(const char *buf, const int &size)
{
	boost::system::error_code ec;

	if (!port_) return -1;
	if (size == 0) return 0;

	return port_->write_some(boost::asio::buffer(buf, size), ec);
}

void SerialPort::async_read_some_()
{
	if (port_.get() == NULL || !port_->is_open()) return;

	port_->async_read_some(
		boost::asio::buffer(read_buf_raw_, SERIAL_PORT_READ_BUF_SIZE),
		boost::bind(
			&SerialPort::on_receive_,
			this, boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
}

void SerialPort::on_receive_(const boost::system::error_code& ec, size_t bytes_transferred)
{
	boost::mutex::scoped_lock look(mutex_);

	if (port_.get() == NULL || !port_->is_open()) return;
	if (ec) {
		async_read_some_();
		return;
	}

	for (unsigned int i = 0; i < bytes_transferred; ++i) {
		char c = read_buf_raw_[i];
		if (c == end_of_line_char_) {
			this->on_receive_(read_buf_str_);
			read_buf_str_.clear();
		}
		else {
			read_buf_str_ += c;
		}
	}

	async_read_some_();
}

void SerialPort::on_receive_(const std::string &data)
{
	std::cout << "SerialPort::on_receive_() : " << data << std::endl;
}

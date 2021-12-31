#ifndef TCPSERVERPASSIVE_HPP
# define TCPSERVERPASSIVE_HPP

#include "TCPSocket.hpp"


class TCPSocketPassive : public TCPSocket {
	public:

		TCPSocketPassive(int port);
		~TCPSocketPassive();

		void start();
		void close_fd();
		int accept_connection();

		int get_socket_fd(void) const;

	protected:

	private:
		//TODO: Use _address_info !
		//struct addrinfo	_address_info;

};

#endif
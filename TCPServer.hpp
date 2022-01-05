#ifndef TCPSERVER_HPP
#define TCPSERVER_HPP

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <list>
#include <poll.h>
#include <queue>
#include <vector>


#include "TCPClient.hpp"
#include "TCPSocket.hpp"
#include "TCPSocketPassive.hpp"
#include "TCPMessage.hpp"


void handler_signal(int num);

class TCPServer {
	public:

		TCPServer(const std::string & port);
		~TCPServer();

		void start(bool run=false);
		void update();
		void stop();

		std::vector<int> new_clients;
		std::list<TCPMessage> messages_received;
		std::list<TCPMessage> messages_to_be_sent;

		class ErrorSignalException : public std::exception {
			public:
				virtual const char * what() const throw();
		};

		class ErrorPollException : public std::exception {
			public:
				virtual const char * what() const throw();
		};

	private:
		TCPSocketPassive _socket;
		std::vector<struct pollfd> _pollfds;
		std::map<int, TCPClient *> _clients;

		void _run();
		void _add_clients();
		void _add_client(int socket_fd);
		void _remove_client(int socket_fd);
		void _send_messages();
		void _send_message(TCPMessage & message);
		void _handle_reception(std::vector<struct pollfd>::iterator & it);

		TCPServer();
		TCPServer(TCPServer const & cpy);
		TCPServer & operator=(TCPServer const & rhs);
};

#endif

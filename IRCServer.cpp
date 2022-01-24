#include "IRCServer.hpp"


IRCServer::IRCServer(const std::string & port, const std::string & password) :
		_tcp_server(port), _password(password), _servername("IRC Server VTA !"), _version("42.42") {

	time_t raw_time;
	time(&raw_time);
	_server_creation_date = ctime(&raw_time);
	_server_creation_date.erase(--_server_creation_date.end());
	_operator_name = "root";
	_operator_password = "ircforlife";

	_commands["PASS"] = &IRCServer::_execute_pass;
	_commands["NICK"] = &IRCServer::_execute_nick;
	_commands["USER"] = &IRCServer::_execute_user;
	_commands["MODE"] = &IRCServer::_execute_mode;
	_commands["QUIT"] = &IRCServer::_execute_quit;
	_commands["JOIN"] = &IRCServer::_execute_join;
	_commands["PART"] = &IRCServer::_execute_part;
	_commands["PRIVMSG"] = &IRCServer::_execute_privmsg;
	_commands["NOTICE"] = &IRCServer::_execute_notice;
	_commands["TOPIC"] = &IRCServer::_execute_topic;
	_commands["NAMES"] = &IRCServer::_execute_names;
	_commands["LIST"] = &IRCServer::_execute_list;
	//_commands["WHOIS"] = &IRCServer::_execute_whois;
	_commands["PING"] = &IRCServer::_execute_ping;
	_commands["AWAY"] = &IRCServer::_execute_away;
	_commands["OPER"] = &IRCServer::_execute_oper;
	_commands["KILL"] = &IRCServer::_execute_kill;
	_commands["kill"] = &IRCServer::_execute_kill;
}

IRCServer::~IRCServer() {}

void IRCServer::start() {
	_tcp_server.start();
	_run();
}

void IRCServer::stop() {
	_tcp_server.stop();
	_commands.clear();
	if (!(_clients.empty())) {
		std::map<int, IRCClient *>::iterator it = _clients.begin();
		for (; it != _clients.end(); it++) {
			delete _clients[it->first];
		}
		_clients.clear();
	}
}

void IRCServer::_run() {
	while (true) {
		// Appel à update
		try {
			_tcp_server.update();
		} catch (TCPServer::ErrorSignalException & e) {
			throw e;
		}
		// Récupérer les nouveaux clients et supprimer les clients déconnectés
		_add_clients(_tcp_server.get_new_clients());
		_remove_clients(_tcp_server.get_disconnected_clients());

		// Récupérer les nouveaux messages reçus.
		std::list<TCPMessage>::const_iterator
				it_message = _tcp_server.get_messages_received().begin();
		for (; it_message != _tcp_server.get_messages_received().end(); it_message++) {
			IRCMessage const & irc_message = IRCMessage(*it_message);
			if (_is_connected(irc_message.get_sender())) {
				_execute_command(irc_message);
			}
		}
	}
}

void IRCServer::_add_clients(const std::vector<int> & new_clients) {
	std::vector<int>::const_iterator it = new_clients.begin();
	for (; it != new_clients.end(); it++) {
		IRCClient * new_client = new IRCClient(*it);
		_clients.insert(std::pair<int, IRCClient *>(*it, new_client));
	}
}

void IRCServer::_remove_clients(const std::vector<int> & disconnected_clients) {
	std::vector<int>::const_iterator it_client = disconnected_clients.begin();
	for (; it_client != disconnected_clients.end(); it_client++) {
		try {
			_clients.at(*it_client);
			_remove_client_from_all_channels(*it_client);
			delete _clients[*it_client];
			_clients.erase(*it_client);
		} catch (std::out_of_range & e) {
			// If the client doesn't exist, we ignore it.
		};
	}
}

void IRCServer::_remove_client_from_all_channels(int client_socketfd) {
	std::map<std::string, Channel *>::iterator it_channel = _channels.begin();
	for (; it_channel != _channels.end(); it_channel++) {
		it_channel->second->remove_client(client_socketfd);
	}
}

void IRCServer::_execute_command(IRCMessage const & message) {

	std::map<std::string, void (IRCServer::*)(IRCMessage const & message)>::iterator
			it = _commands.find(message.get_command());
	if (it != _commands.end()) {
		(this->*_commands[message.get_command()])(message);
	} else {
		std::cout << "Command not found: " << message.get_command() << std::endl;
	}
}

/**
 * @brief Executes a PASS command.
 * @param message The message containing the PASS command.
 */
void IRCServer::_execute_pass(IRCMessage const & message) {
	std::cout << "Executing PASS: " << message.get_command() << std::endl;
	IRCClient * client = _clients.at(message.get_sender());
	if (client->get_status() == UNREGISTERED) {
		if (_password == message.get_params().at(0)) {
			client->set_password(message.get_params().at(0));
			client->set_status(PASSWORD);
		} else {
			std::cout << "Wrong password" << std::endl; //DEBUG
			std::vector<int> disconnected_client(1u, client->get_fd());
			_tcp_server.add_client_to_disconnect(client->get_fd());
			_remove_clients(disconnected_client);
		}
	} else {
		_tcp_server.schedule_sent_message(make_reply_ERR_ALREADYREGISTRED(*client));
	}
}

/**
 * @brief Executes a NICK command.
 * @param message The message containing the NICK command.
 */
void IRCServer::_execute_nick(IRCMessage const & message) {
	std::cout << "Executing NICK: " << message.get_command() << std::endl;
	IRCClient * client = _clients.at(message.get_sender());
	std::string nick = message.get_params().at(0);

	std::map<int, IRCClient *>::const_iterator it_client;
	if (client->get_status() == UNREGISTERED) {
		std::cout << "PASS command not executed before NICK command" << std::endl; //DEBUG
		std::vector<int> disconnected_client(1u, client->get_fd());
		_tcp_server.add_client_to_disconnect(client->get_fd());
		_remove_clients(disconnected_client);
	} else if ((it_client = find_nickname(nick)) != _clients.end()) {
		_tcp_server.schedule_sent_message(make_reply_ERR_NICKNAMEINUSE(*client, nick));
	} else {
		if (client->get_status() == PASSWORD) {
			client->set_status(NICKNAME);
		}
		std::vector<int> receivers;
		receivers.push_back(client->get_fd());
		std::vector<Channel *> channels = _get_client_channels(client->get_fd());
		std::vector<Channel *>::const_iterator it_channel = channels.begin();
		for (; it_channel != channels.end(); it_channel++) {
			Channel * tmp = *it_channel;
			//TODO: There are 2 it_clients ! Which is which ? Shadowing variables is dangerous !
			std::vector<int>::const_iterator it_client = tmp->clients_begin();
			for (; it_client != tmp->clients_end(); it_client++) {
				if (find(receivers.begin(), receivers.end(), *it_client) == receivers.end()) {
					receivers.push_back(*it_client);
				}
			}
		}
		_tcp_server.schedule_sent_message(make_reply_NICK(*client, nick, receivers));
		client->set_nickname(message.get_params().at(0));
	}
}

/**
 * @brief Executes a USER command.
 * @param message The message containing the USER command.
 */
void IRCServer::_execute_user(IRCMessage const & message) {
	std::cout << "Executing USER: " << message.get_command() << std::endl;
	IRCClient * client = _clients.at(message.get_sender());
	int status = client->get_status();

	if (status == REGISTERED) {
		TCPMessage reply = make_reply_ERR_ALREADYREGISTRED(*client);
		_tcp_server.schedule_sent_message(reply);
	} else if (status == NICKNAME) {
		client->set_username(message.get_params().at(0));
		client->set_hostname(message.get_params().at(1)); // TODO: To change ?
		client->set_realname(message.get_params().at(3));
		client->set_status(REGISTERED);
		// TODO: To change ?
		std::string user_modes("USER_MODES"), channel_modes("CHANNEL_MODES");

		TCPMessage reply = make_reply_RPL_WELCOME(*client);
		_tcp_server.schedule_sent_message(reply);
		reply = make_reply_RPL_YOURHOST(*client, _servername, _version);
		_tcp_server.schedule_sent_message(reply);
		reply = make_reply_RPL_CREATED(*client, _server_creation_date);
		_tcp_server.schedule_sent_message(reply);
		reply = make_reply_RPL_MYINFO(*client, _servername, _version, user_modes, channel_modes);
		_tcp_server.schedule_sent_message(reply);
	}
}

/**
 * @brief Executes a MODE command for user.
 * @param message The message containing the MODE command for user.
 */
void IRCServer::_execute_mode_user(IRCMessage const & message) {
	std::cout << "Executing MODE USER: " << message.get_command() << std::endl;
	IRCClient * client = _clients.at(message.get_sender());
	std::vector<std::string> params = message.get_params();
	if (params[0] != client->get_nickname()) {
		_tcp_server.schedule_sent_message(make_reply_ERR_USERSDONTMATCH(*client));
	} else if (params[0] == client->get_nickname() && params.size() == 1) {
		_tcp_server.schedule_sent_message(make_reply_RPL_UMODEIS(*client));
	} else if (params[0] == client->get_nickname()) {
		//std::string	new_mode;
		std::string user_modes = USER_MODES();
		bool sign = true;
		for (size_t i = 0; i < params[1].size(); i++) {
			if (params[1][i] == '+') {
				sign = true;
			} else if (params[1][i] == '-') {
				sign = false;
			} else if (params[1][i] == 'o') {
				if (!sign) {
					client->set_mode('-', params[1][i]);
				}
			} else if (user_modes.find(params[1][i]) == std::string::npos) {
				_tcp_server.schedule_sent_message(make_reply_ERR_UMODEUNKNOWNFLAG(*client));
			}
		}
		_tcp_server.schedule_sent_message(make_reply_RPL_UMODEIS(*client));
	}
}

/**
 * @brief Executes a MODE command for channel.
 * @param message The message containing the MODE command for channel.
 */
void IRCServer::_execute_mode_channel(IRCMessage const & message) {
	std::cout << "Executing MODE CHANNEL: " << message.get_command() << std::endl;
	IRCClient * client = _clients.at(message.get_sender());
	std::vector<std::string> params = message.get_params();
	Channel * channel;
	try {
		channel = _channels.at(params[0]);
	} catch (std::out_of_range & e) {
		_tcp_server.schedule_sent_message(make_reply_ERR_NOSUCHCHANNEL(*client, params[0]));
		return;
	}
	if (find(channel->clients_begin(), channel->clients_end(), client->get_fd()) ==
		channel->clients_end()) {
		_tcp_server.schedule_sent_message(make_reply_ERR_NOTONCHANNEL(*client, params[0]));
		return;
	}
	if (params.size() == 1) {
		_tcp_server.schedule_sent_message(make_reply_RPL_CHANNELMODEIS(*client, *channel));
		return;
	}
	if (find(channel->channel_op_begin(), channel->channel_op_end(), client->get_fd()) ==
		channel->channel_op_end()) {
		_tcp_server.schedule_sent_message(make_reply_ERR_CHANOPRIVSNEEDED(*client, params[0]));
		return;
	}

	bool sign = true;
	int number = 2;
	bool ret = false;
	std::string channel_modes = CHANNEL_MODES();
	std::string channel_modes_without_params = CHANNEL_MODES_WITHOUT_PARAMS();
	std::string channel_modes_with_params = CHANNEL_MODES_WITH_PARAMS();
	for (size_t i = 0; i < params[1].size(); i++) {
		if (params[1][i] == '+') {
			sign = true;
		} else if (params[1][i] == '-') {
			sign = false;
		} else if (params[1][i] == 'o') {
			std::map<int, IRCClient *>::const_iterator it = find_nickname(params[number]);
			if (it == _clients.end() ||
				(find(channel->clients_begin(), channel->clients_end(), it->second->get_fd()) ==
				 channel->clients_end())) {
				_tcp_server.schedule_sent_message(
						make_reply_ERR_USERNOTINCHANNEL(*client, params[0], params[number]));
			} else if (sign) {
				ret = channel->add_client_to_channel_operator(it->first);
			} else {
				if (it->second->get_nickname() == client->get_nickname()) {
					ret = channel->remove_client_to_channel_operator(client->get_fd());
				}
			}
			number++;
		} else if (params[1][i] == 'k') {
			if (sign) {
				std::string mode = channel->get_mode();
				if (mode.find(params[1][i]) != std::string::npos) {
					_tcp_server.schedule_sent_message(make_reply_ERR_KEYSET(*client, params[0]));
				} else {
					channel->set_mode('+', 'k');
					channel->set_key(params[number]);
					ret = true;
				}
			} else {
				if (params[number] == channel->get_key()) {
					channel->set_mode('-', 'k');
					channel->set_key("");
					ret = true;
				}
			}
			number++;
		} else if (channel_modes_without_params.find(params[1][i]) != std::string::npos) {
			continue;
		} else if (channel_modes_with_params.find(params[1][i]) != std::string::npos) {
			number++;
		} else {
			_tcp_server.schedule_sent_message(
					make_reply_ERR_UNKNOWNMODE(*client, params[0], params[1][i]));
		}
		if (ret) {
			std::string channel_mode = "-";
			if (sign) {
				channel_mode = "+";
			}
			channel_mode.push_back(params[1][i]);
			_tcp_server.schedule_sent_message(
					make_reply_MODE(*client, *channel, channel_mode, params[number - 1]));
			ret = false;
		}
	}
}

/**
 * @brief Executes a MODE command.
 * @param message The message containing the MODE command.
 */
void IRCServer::_execute_mode(IRCMessage const & message) {
	std::cout << "Executing MODE: " << message.get_command() << std::endl;
	if (message.get_params().at(0).find('#') != std::string::npos) {
		_execute_mode_channel(message);
	} else {
		_execute_mode_user(message);
	}
}

/**
 * @brief Executes a QUIT command.
 * @param message The message containing the QUIT command.
 */
void IRCServer::_execute_quit(IRCMessage const & message) {

	std::cout << "Executing QUIT: " << message.get_command() << std::endl;
	IRCClient * client = _clients[message.get_sender()];
	std::string quit_message;
	if (message.get_params().size() == 1) {
		quit_message = message.get_params().at(0);
	}
	std::vector<Channel *> client_channels = _get_client_channels(client->get_fd());
	std::vector<Channel *>::iterator it_channel = client_channels.begin();
	for (; it_channel != client_channels.end(); it_channel++) {
		TCPMessage reply = make_reply_PART(*client, **it_channel, quit_message);
		_tcp_server.schedule_sent_message(reply);
		(*it_channel)->remove_client(message.get_sender());
		if ((*it_channel)->get_clients().empty()) {
			std::string channel_name = (*it_channel)->get_name();
			std::cout << "Channel " << channel_name << " deleted." << std::endl;
			delete *it_channel;
			_channels.erase(channel_name);
		}
	}
	_tcp_server.add_client_to_disconnect(client->get_fd());
	_tcp_server.schedule_sent_message(make_reply_ERROR(*client, quit_message));
}

void IRCServer::_join_channel(const IRCClient & client, Channel & channel) {
	channel.add_client(client.get_fd());
	_tcp_server.schedule_sent_message(make_reply_JOIN(client, channel));
	_tcp_server.schedule_sent_message(make_reply_RPL_TOPIC(client, channel));
	std::string users_list = _get_formatted_clients_from_channel(channel.get_name());
	_tcp_server.schedule_sent_message(make_reply_RPL_NAMREPLY(client, channel, users_list));
	_tcp_server.schedule_sent_message(make_reply_RPL_ENDOFNAMES(client, channel.get_name()));
}

void IRCServer::_leave_all_channels(const IRCClient & client) {
	std::vector<Channel *> channels = _get_client_channels(client.get_fd());
	std::vector<Channel *>::iterator it_channel = channels.begin();
	for (; it_channel != channels.end(); it_channel++)
	{
		Channel *channel = _channels.at((*it_channel)->get_name());
		_tcp_server.schedule_sent_message(make_reply_PART(client, *channel, ""));
		channel->remove_client(client.get_fd());
		if (channel->get_clients().empty()) {
			delete channel;
			_channels.erase((*it_channel)->get_name());
		}
	}
}

/**
 * @brief Executes a JOIN command.
 * @param message The message containing the JOIN command.
 */
void IRCServer::_execute_join(IRCMessage const & message) {
	std::cout << "Executing JOIN: " << message.get_command() << std::endl;
	IRCClient * client = _clients.at(message.get_sender());
	std::vector<std::string> channel_names = ft_split(message.get_params()[0], ",");
	if (*channel_names.begin() == "0")
	{
		_leave_all_channels(*client);
		return ;
	}
	bool new_channel = false;
	std::vector<std::string> channel_keys;
	std::vector<std::string>::const_iterator it_keys;
	std::vector<std::string>::const_iterator it_keys_end;
	if (message.get_params().size() > 1)
	{
		channel_keys = ft_split(message.get_params()[1], ",");
		it_keys = channel_keys.begin();
		it_keys_end = channel_keys.end();
	}
	std::vector<std::string>::const_iterator it_channel_name = channel_names.begin();
	for (; it_channel_name != channel_names.end(); it_channel_name++) {
	
		Channel * channel;
		try {
			channel = _channels.at(*it_channel_name);
			new_channel = false;
		} catch (std::out_of_range & e) {
			new_channel = true;
		}
		if (_get_client_channels(client->get_fd()).size() == MAX_CHANNELS)
			_tcp_server.schedule_sent_message(make_reply_ERR_TOOMANYCHANNELS(*client, *it_channel_name));
		else if (new_channel == false)
		{
			if (channel->get_mode().find('k') != std::string::npos)
			{
				if (it_keys == it_keys_end)
					_tcp_server.schedule_sent_message(make_reply_ERR_BADCHANNELKEY(*client, *it_channel_name));
				else
				{
					if (*it_keys != channel->get_key())
						_tcp_server.schedule_sent_message(make_reply_ERR_BADCHANNELKEY(*client, *it_channel_name));
					else
						_join_channel(*client, *channel);
					it_keys++;
				}
			}
			else
			{
				_join_channel(*client, *channel);
				if (it_keys != it_keys_end)
					it_keys++;
			}
		}
		else
		{
			channel = new Channel(*it_channel_name);
			channel->add_client_to_channel_operator(client->get_fd());
			_channels.insert(std::pair<std::string, Channel *>(*it_channel_name, channel));
			_join_channel(*client, *channel);
			//TODO: maybe delete
			if (it_keys != it_keys_end)
			{
				channel->set_key(*it_keys);
				channel->set_mode('+', 'k');
				std::string channel_mode = "+k";
				_tcp_server.schedule_sent_message(make_reply_MODE(*client, *channel, channel_mode, *it_keys));
				it_keys++;
			}
		}
	}
}


/**
 * @brief Executes a PART command.
 * @param message The message containing the PART command.
 */
void IRCServer::_execute_part(IRCMessage const & message) {
	std::cout << "Executing PART: " << message.get_command() << std::endl;
	IRCClient * client = _clients.at(message.get_sender());
	std::vector<std::string> channel_names = ft_split(message.get_params().at(0), ",");
	std::string part_message;
	if (message.get_params().size() == 2) {
		part_message = message.get_params().at(1);
	}
	std::vector<std::string>::const_iterator it_channel_name = channel_names.begin();
	for (; it_channel_name != channel_names.end(); it_channel_name++) {
		try {
			Channel * channel = _channels.at(*it_channel_name);
			if (!channel->has_client(message.get_sender())) {
				// If client isn't on the channel, send NOTONCHANNEL
				TCPMessage reply = make_reply_ERR_NOTONCHANNEL(*client, *it_channel_name);
				_tcp_server.schedule_sent_message(reply);
			} else {
				// Else, broadcast to channel's users that a new user joined the channel
				TCPMessage reply = make_reply_PART(*client, *channel, part_message);
				channel->remove_client(message.get_sender());
				_tcp_server.schedule_sent_message(reply);
				// And remove the channel if it's empty
				if (channel->get_clients().empty()) {
					std::cout << "Channel " << *it_channel_name << " deleted." << std::endl;
					delete channel;
					_channels.erase(*it_channel_name);
				}
			}
		} catch (std::out_of_range & e) {
			// If channel doesn't exist, send NOSUCHCHANNEL
			TCPMessage reply = make_reply_ERR_NOSUCHCHANNEL(*client, *it_channel_name);
			_tcp_server.schedule_sent_message(reply);
		}
	}
}

/**
 * @brief Executes a PRIVMSG command.
 * @param message The message containing the PRIVMSG command.
 */
void IRCServer::_execute_privmsg(IRCMessage const & message) {
	std::cout << "Executing PRIVMSG: " << message.get_command() << std::endl;
	IRCClient * client = _clients.at(message.get_sender());
	if (message.get_params().empty()) {
		// If no params, send ERR_NORECIPIENT
		TCPMessage reply = make_reply_ERR_NORECIPIENT(*client, message.get_command());
		_tcp_server.schedule_sent_message(reply);
		return;
	} else if (message.get_params().size() == 1) {
		// If 1 param, send ERR_NOTEXTTOSEND
		_tcp_server.schedule_sent_message(make_reply_ERR_NOTEXTTOSEND(*client));
		return;
	}
	std::string target = message.get_params().at(0);
	if (target[0] == '#') {
		Channel * channel;
		try {
			channel = _channels.at(target);
		} catch (std::out_of_range & e) {
			// If channel doesn't exist, send ERR_NOSUCHNICK
			_tcp_server.schedule_sent_message(make_reply_ERR_NOSUCHNICK(*client, target));
			return;
		}
		if (find(channel->clients_begin(), channel->clients_end(), client->get_fd()) ==
			channel->clients_end()) {
			// If not in channel, send ERR_CANNOTSENDTOCHAN
			TCPMessage reply = make_reply_ERR_CANNOTSENDTOCHAN(*client, channel->get_name());
			_tcp_server.schedule_sent_message(reply);
			return;
		}
		std::vector<int>::const_iterator it_client = channel->clients_begin();
		for (; it_client != channel->clients_end(); it_client++) {
			if (*it_client != client->get_fd()) {
				if (_clients[*it_client]->get_mode().find('a') != std::string::npos) {
					TCPMessage reply = make_reply_RPL_AWAY(*client, *(_clients[*it_client]));
					_tcp_server.schedule_sent_message(reply);
				}
			}
		}
		TCPMessage
				reply = make_reply_PRIVMSG_CHANNEL(*client, *channel, message.get_params().at(1));
		_tcp_server.schedule_sent_message(reply);
	} else {
		std::map<int, IRCClient *>::const_iterator it = find_nickname(target);
		if (it == _clients.end()) {
			TCPMessage reply = make_reply_ERR_NOSUCHNICK(*client, target);
			_tcp_server.schedule_sent_message(reply);
			return;
		}
		IRCClient * client_recipient = it->second;
		if (client_recipient->get_mode().find('a') != std::string::npos) {
			TCPMessage reply = make_reply_RPL_AWAY(*client, *client_recipient);
			_tcp_server.schedule_sent_message(reply);
		}
		TCPMessage reply = make_reply_PRIVMSG_USER(*client, *client_recipient, target,
												   message.get_params().at(1));
		_tcp_server.schedule_sent_message(reply);
	}
}

/**
 * @brief Executes a NOTICE command.
 * @param message The message containing the NOTICE command.
 */
void IRCServer::_execute_notice(IRCMessage const & message) {
	std::cout << "Executing NOTICE: " << message.get_command() << std::endl;
	IRCClient * client = _clients.at(message.get_sender());
	if (message.get_params().empty() || message.get_params().size() == 1) {
		return;
		//TODO: Shouldn't there be an error here ?
	}
	std::string target = message.get_params().at(0);
	if (target[0] == '#') {
		Channel * channel;
		try {
			channel = _channels.at(target);
		} catch (std::out_of_range & e) {
			return;
		}
		if (find(channel->clients_begin(), channel->clients_end(), client->get_fd()) ==
			channel->clients_end()) {
			return;
		}
		TCPMessage reply = make_reply_NOTICE_CHANNEL(*client, *channel, message.get_params().at(1));
		_tcp_server.schedule_sent_message(reply);
	} else {
		std::map<int, IRCClient *>::const_iterator it = find_nickname(target);
		if (it == _clients.end()) {
			return;
		}
		IRCClient * client_recipient = it->second;
		TCPMessage reply = make_reply_NOTICE_USER(*client, *client_recipient, target,
												  message.get_params().at(1));
		_tcp_server.schedule_sent_message(reply);
	}
}

/**
 * @brief Executes a TOPIC command.
 * @param message The message containing the TOPIC command.
 */
void IRCServer::_execute_topic(IRCMessage const & message) {
	std::cout << "Executing TOPIC: " << message.get_command() << std::endl;
	IRCClient * client = _clients.at(message.get_sender());
	std::string channel_name = message.get_params().at(0);

	std::map<std::string, Channel *>::const_iterator it = find_channel(channel_name);
	if (it == _channels.end()) {//TODO: change 403 ERR_NOSUHCHANNEL ??
		TCPMessage reply = make_reply_ERR_NOTONCHANNEL(*client, channel_name);
		_tcp_server.schedule_sent_message(reply);
		return;
	}
	Channel * channel_tmp = it->second;
	if (find(channel_tmp->clients_begin(), channel_tmp->clients_end(), message.get_sender()) ==
		channel_tmp->clients_end()) {
		TCPMessage reply = make_reply_ERR_NOTONCHANNEL(*client, channel_name);
		_tcp_server.schedule_sent_message(reply);
		return;
	}
	if (message.get_params().size() > 1) {
		if (find(channel_tmp->channel_op_begin(), channel_tmp->channel_op_end(),
				 message.get_sender()) == channel_tmp->channel_op_end()) {
			TCPMessage reply = make_reply_ERR_CHANOPRIVSNEEDED(*client, channel_name);
			_tcp_server.schedule_sent_message(TCPMessage(reply));
		} else {//TODO: if topic stored on several parameters ==> loop
			channel_tmp->set_topic(message.get_params().at(1));
			TCPMessage reply = make_reply_TOPIC(*client, *channel_tmp);
			_tcp_server.schedule_sent_message(reply);
		}
		return;
	} else {
		if (!channel_tmp->get_topic().empty()) {
			TCPMessage reply = make_reply_RPL_TOPIC(*client, *channel_tmp);
			_tcp_server.schedule_sent_message(reply);
		} else {
			TCPMessage reply = make_reply_RPL_NOTOPIC(*client, *channel_tmp);
			_tcp_server.schedule_sent_message(reply);
		}
	}
}

/**
 * @brief Executes a NAMES command.
 * @param message The message containing the NAMES command.
 */
void IRCServer::_execute_names(IRCMessage const & message) {
	std::cout << "Executing NAMES: " << message.get_command() << std::endl;
	IRCClient * client = _clients.at(message.get_sender());
	std::string users_list;
	if (message.get_params().empty()) {
		std::map<std::string, Channel *>::iterator it = _channels.begin();
		for (; it != _channels.end(); it++) {
			users_list = _get_formatted_clients_from_channel(it->second->get_name());
			TCPMessage reply = make_reply_RPL_NAMREPLY(*client, *(it->second), users_list);
			_tcp_server.schedule_sent_message(reply);
			reply = make_reply_RPL_ENDOFNAMES(*client, (*it->second).get_name());
			_tcp_server.schedule_sent_message(reply);
		}
		users_list = _get_formatted_clients_without_channel();
		if (!users_list.empty()) {
			std::string channel_name = "*";
			Channel tmp(channel_name);
			TCPMessage reply = make_reply_RPL_NAMREPLY(*client, tmp, users_list);
			_tcp_server.schedule_sent_message(reply);
			reply = make_reply_RPL_ENDOFNAMES(*client, channel_name);
			_tcp_server.schedule_sent_message(reply);
		}
	} else {
		for (size_t i = 0; i < message.get_params().size(); i++) {
			std::map<std::string, Channel *>::const_iterator
					it = find_channel(message.get_params()[i]);
			if (it != _channels.end()) {
				users_list = _get_formatted_clients_from_channel(it->second->get_name());
				TCPMessage reply = make_reply_RPL_NAMREPLY(*client, *(it->second), users_list);
				_tcp_server.schedule_sent_message(reply);
				reply = make_reply_RPL_ENDOFNAMES(*client, message.get_params()[i]);
				_tcp_server.schedule_sent_message(reply);
			}
		}
	}
}

/**
 * @brief Executes a LIST command.
 * @param message The message containing the LIST command.
 */
void IRCServer::_execute_list(IRCMessage const & message) {
	std::cout << "Executing LIST: " << message.get_command() << std::endl;
	IRCClient * client = _clients.at(message.get_sender());
	//TODO: see for the LIST command and the -YES parameter
	if (message.get_params().empty()) {
		std::map<std::string, Channel *>::iterator it = _channels.begin();
		for (; it != _channels.end(); it++) {
			TCPMessage reply = make_reply_RPL_LIST(*client, *(it->second));
			_tcp_server.schedule_sent_message(reply);
		}
	} else {
		for (size_t i = 0; i < message.get_params().size(); i++) {
			std::map<std::string, Channel *>::const_iterator
					c_it = find_channel(message.get_params()[i]);
			if (c_it != _channels.end()) {
				TCPMessage reply = make_reply_RPL_LIST(*client, *(c_it->second));
				_tcp_server.schedule_sent_message(reply);
			}
		}
	}
	TCPMessage reply = make_reply_RPL_LISTEND(*client);
	_tcp_server.schedule_sent_message(reply);
}

/**
 * @brief Executes a WHOIS command.
 * @param message The message containing the WHOIS command.
 */
/*void IRCServer::_execute_whois(IRCMessage const & message) {
	std::cout << "Executing WHOIS: " << message.get_command() << std::endl;
    IRCClient * client = _clients.at(message.get_sender());
	std::map<int, IRCClient *>::iterator it_clients = _clients.begin();
	for (;it_clients !=_clients.end(); it_clients++)
	{
		IRCClient * client_tmp = it_clients->second;
		if (client_tmp->get_nickname() == message.get_params().at(0))
		{
			TCPMessage reply = make_reply_RPL_WHOISUSER(*client,*client_tmp );
			_tcp_server.schedule_sent_message(reply);
			if (client_tmp->get_mode().find('a') != std::string::npos)
			{
				reply = make_reply_RPL_AWAY(*client, *client_tmp);
				_tcp_server.schedule_sent_message(reply);
			}
			if (client_tmp->get_mode().find('o') != std::string::npos)
			{
				reply = make_reply_RPL_WHOISOPERATOR(*client, *client_tmp);
				_tcp_server.schedule_sent_message(reply);
			}
			std::vector<std::string> client_channels = client_tmp->get_channels();
			std::string	channels_names;
			std::vector<std::string>::iterator it = client_channels.begin();
			for (; it != client_channels.end(); it++)
			{
				Channel *tmp = _channels[*it];
				if (find(tmp->channel_op_begin(), tmp->channel_op_end(), client_tmp->get_fd()) != tmp->channel_op_end())
					channels_names += ("@" + tmp->get_name() + " ");
				else if (client_tmp->is_visible())
					channels_names += (tmp->get_name() + " ");
			}
			if (!client_channels.empty())
			{
				reply = make_reply_RPL_WHOISCHANNELS(*client, *client_tmp, channels_names);
				_tcp_server.schedule_sent_message(reply);	
			}
			reply = make_reply_RPL_ENDOFWHOIS(*client);
			_tcp_server.schedule_sent_message(reply);
			return ;
		}
	}
	TCPMessage reply = make_reply_ERR_NOSUCHNICK(*client, message.get_params().at(0));
	_tcp_server.schedule_sent_message(reply);
	reply = make_reply_RPL_ENDOFWHOIS(*client);
	_tcp_server.schedule_sent_message(reply);
}*/

/**
 * @brief Executes a PING command.
 * @param message The message containing the PING command.
 */
void IRCServer::_execute_ping(IRCMessage const & message) {
	std::cout << "Executing PING: " << message.get_command() << std::endl;
	IRCClient * client = _clients.at(message.get_sender());
	if (message.get_params().empty()) {
		TCPMessage reply = make_reply_ERR_NOORIGIN(*client);
		_tcp_server.schedule_sent_message(reply);
	} else {
		TCPMessage reply = make_reply_PONG(*client, message.get_params().at(0));
		_tcp_server.schedule_sent_message(reply);
	}
}

/**
 * @brief Executes a AWAY command.
 * @param message The message containing the AWAY command.
 */
void IRCServer::_execute_away(IRCMessage const & message) {
	std::cout << "Executing AWAY: " << message.get_command() << std::endl;
	IRCClient * client = _clients.at(message.get_sender());

	if (message.get_params().empty()) {
		if (client->get_mode().find('a') != std::string::npos) {
			client->set_mode('-', 'a');
			client->set_away_message("");
			_tcp_server.schedule_sent_message(make_reply_RPL_UNAWAY(*client));
		}
	} else {
		if (client->get_mode().find('a') == std::string::npos) {
			client->set_mode('+', 'a');
		}
		client->set_away_message(message.get_params().at(0));
		_tcp_server.schedule_sent_message(make_reply_RPL_NOWAWAY(*client));
	}
}

void IRCServer::_execute_oper(const IRCMessage & message) {
	IRCClient * client = _clients.at(message.get_sender());
	//TODO: Sanity check -> ERR_NEEDMOREPARAMS
	if (message.get_params().at(0) != _operator_name ||
		message.get_params().at(1) != _operator_password) {
		// If operator name or password is incorrect, send ERR_PASSWDMISMATCH
		_tcp_server.schedule_sent_message(make_reply_ERR_PASSWDMISMATCH(*client));
	} else {
		// Else, add operator mode to user and send RPL_YOUREOPER
		client->set_mode('+', 'o');
		_tcp_server.schedule_sent_message(make_reply_RPL_YOUREOPER(*client));
	}
}

void IRCServer::_execute_kill(const IRCMessage & message) {
	IRCClient * killer = _clients.at(message.get_sender());
	//TODO: Sanity check -> ERR_NEEDMOREPARAMS
	//TODO: Pass as ref sur const !
	std::string nick_killed = message.get_params().at(0);
	std::string comment = message.get_params().at(1);
	if (!killer->is_mode('o')) {
		// If killer isn't oper -> ERR_NOPRIVILEGES
		_tcp_server.schedule_sent_message(make_reply_ERR_NOPRIVILEGES(*killer));
		return;
	} else if (find_nickname(nick_killed) == _clients.end()) {
		// If nick doesn't exist -> ERR_NOSUCHNICK
		_tcp_server.schedule_sent_message(make_reply_ERR_NOSUCHNICK(*killer, nick_killed));
		return;
	}
	// Else send them a kill message, broadcast a QUIT, send them an ERROR and finally remove them
	IRCClient * killed = find_nickname(nick_killed)->second;
	_tcp_server.schedule_sent_message(make_reply_KILL(*killer, *killed, comment));
	//TODO: broadcast a QUIT message to all uses sharing a channel with killed ?
	_tcp_server.add_client_to_disconnect(killed->get_fd()); //TODO: to check ?
	_tcp_server.schedule_sent_message(make_reply_ERROR(*killed, comment));
}

std::map<int, IRCClient *>::const_iterator IRCServer::find_nickname(
		const std::string & nickname) const {
	std::map<int, IRCClient *>::const_iterator it = _clients.begin();
	for (; it != _clients.end(); it++) {
		if (it->second->get_nickname() == nickname) {
			break;
		}
	}
	return it;
}

std::map<std::string, Channel *>::const_iterator IRCServer::find_channel(
		const std::string & channel_name) const {
	std::map<std::string, Channel *>::const_iterator it = _channels.begin();
	for (; it != _channels.end(); it++) {
		if (it->second->get_name() == channel_name) {
			break;
		}
	}
	return it;
}

std::vector<Channel *> IRCServer::_get_client_channels(int client_socketfd) {
	std::vector<Channel *> client_channels;
	std::map<std::string, Channel *>::const_iterator it_channel = _channels.begin();
	for (; it_channel != _channels.end(); it_channel++) {
		if (it_channel->second->has_client(client_socketfd)) {
			client_channels.push_back(it_channel->second);
		}
	}
	return client_channels;
}

std::string IRCServer::_get_formatted_clients_from_channel(const std::string & channel_name) {
	std::string users_list;
	Channel * channel = _channels.at(channel_name);
	std::vector<int>::const_iterator it_client = channel->clients_begin();
	for (; it_client != channel->clients_end(); it_client++) {
		if (find(channel->channel_op_begin(), channel->channel_op_end(), *it_client) !=
			channel->channel_op_end()) {
			users_list += "@";
		}
		users_list += _clients.at(*it_client)->get_nickname() + " ";
	}
	users_list.erase(--users_list.end());
	return users_list;
}

std::string IRCServer::_get_formatted_clients_without_channel() {
	std::string users_list;
	int find_client;
	std::map<int, IRCClient *>::const_iterator it_client = _clients.begin();
	for (; it_client != _clients.end(); it_client++) {
		find_client = 0;
		std::map<std::string, Channel *>::const_iterator it_channel = _channels.begin();
		for (; it_channel != _channels.end(); it_channel++) {
			Channel * tmp = it_channel->second;
			if (find(tmp->clients_begin(), tmp->clients_end(), it_client->first) !=
				tmp->clients_end()) {
				find_client = 1;
				break;
			}
		}
		if (find_client == 0) {
			users_list += it_client->second->get_nickname() + " ";
		}
	}
	if (!users_list.empty()) {
		users_list.erase(--users_list.end());
	}
	return users_list;
}

bool IRCServer::_is_connected(int client_socketfd) {
	try {
		_clients.at(client_socketfd);
		return true;
	} catch (std::out_of_range & e) {
		return false;
	}
}
#include "IRCMessage.hpp"
bool fmatch(std::string token, std::string format);

//IRCMessage::IRCMessage(TCPMessage &tcpmessage)
//{
//	_sender = tcpmessage.get_sender();
//	_receivers = tcpmessage.get_receivers();
//	try { _parse_line(tcpmessage.get_payload()); }
//	catch (Error_message_empty &e) { std::cerr << "Error: message is empty" << std::endl; return ; }
//	catch (Error_message_nocrlf &e) { std::cerr << "Error: message has no crlf" << std::endl; return ; }
//	catch (Error_message_invalid_prefix &e) { std::cerr << "Error: message has invalid prefix" << std::endl; return ; }
//}

IRCMessage::IRCMessage(std::string line)
{
	try { _parse_line(line); }
	catch (Error_message_empty &e) { std::cerr << "Error: message is empty" << std::endl; return ; }
	catch (Error_message_nocrlf &e) { std::cerr << "Error: message has no crlf" << std::endl; return ; }
	catch (Error_message_invalid_prefix &e) { std::cerr << "Error: message has invalid prefix" << std::endl; return ; }
}

//TCPMessage
//	IRCMessage::to_tcp_message()
//{
//	std::string payload;
//
//	if (has_prefix())
//		payload = ":" + _prefix + " ";
//	payload += _command;
//	for (size_t i = 0; i < _params.size(); ++i)
//	{
//		if (i == _params.size() + 1) //last param has : before it (see rfc2812 message format)
//			payload += " :" + _params[i].second;
//		else
//			payload += " " + _params[i].second;
//	}
//	return TCPMessage(_receivers, payload);
//}

IRCMessage::~IRCMessage() {}

bool
	IRCMessage::empty() const { return _command.empty(); }

bool
	IRCMessage::has_prefix() const { return !_prefix.empty(); }

std::string
	IRCMessage::get_prefix() const { return _prefix; }

std::string
	IRCMessage::get_command() const { return _command; }

std::vector<std::pair<int, std::string> >
	IRCMessage::get_params() const { return _params; }

int
	IRCMessage::get_sender() const { return _sender; }

std::vector<int>
	IRCMessage::get_receivers() const { return _receivers; }

void IRCMessage::set_command(std::string cmd) { _command = cmd; }

/**
 * @brief Debug function to display IRCMessage content after parsing
 */
std::ostream
	&operator<<(std::ostream &o, const IRCMessage &i)
{
	if (i.has_prefix())
		o << "prefix:\t|" << i.get_prefix() << '|' << std::endl;
	else
		o << "no prefix" << std::endl;
	o << "command:\t|" << i.get_command() << '|' << std::endl;
	if (i.get_params().empty())
		o << "no params";
	else
	{
		o << "params:\t";
		for (size_t j = 0; j < i.get_params().size(); ++j)
			o << '|' << i.get_params()[j].second << "| ";
	}
	o << std::endl;
	return (o);
}

const char*
	IRCMessage::Error_message_empty::what() const throw() { return ("Empty message"); }

const char*
	IRCMessage::Error_message_nocrlf::what() const throw() { return ("No crlf"); }

const char*
	IRCMessage::Error_message_invalid_prefix::what() const throw() { return ("Invalid prefix"); }

/**
 * @brief Parse a string into prefix, command and parameters according to rfc2812
 */
void
	IRCMessage::_parse_line(std::string line)
{
	size_t	pos1 = 0, pos2 = 0;

	try
	{
		if (line.empty())
			throw Error_message_empty();
		//here we get optional prefix
		if (line[0] == ':')
		{
			++pos1;
			if (line[pos1] == ' ' || line[pos1] == '\0')
				throw Error_message_invalid_prefix();
			pos2 = line.find(' ');
			//error if pos2 == line.npos?
			_prefix = line.substr(pos1, pos2 - pos1);
			pos1 = pos2 + 1;
		}
	}
	catch (Error_message_empty &e) { std::cerr << "Error: message is empty" << std::endl; return ; } //this should be ignored, but not here most likely
	catch (Error_message_nocrlf &e) { std::cerr << "Error: message has no crlf" << std::endl; return ; } //this is useless
	catch (Error_message_invalid_prefix &e) { std::cerr << "Error: message has invalid prefix" << std::endl; return ; }
	//here we get command
	if ((pos2 = line.find(' ', pos1)) == line.npos)
	{
		_command = line.substr(pos1);
		return ;
	}
	_command = line.substr(pos1, pos2 - pos1);
	pos1 = pos2 + 1;
	//here we get optional parameters
	while ((pos2 = line.find(' ', pos1 + 1)) != line.npos)
	{
		if (line[pos1] == ':')
		{
			_params.push_back(std::pair<int, std::string>(NONE, line.substr(pos1 + 1)));
			return ;
		}
		_params.push_back(std::pair<int, std::string>(NONE, line.substr(pos1, pos2 - pos1)));
		pos1 = pos2 + 1;
	}
	if (line[pos1] == ':')
		pos1++;
	_params.push_back(std::pair<int, std::string>(NONE, line.substr(pos1)));
}

/**
 * @brief Check IRCMessage prefix, command and parameters sanity and assigns type to its parameters
 */
void
	IRCMessage::_sanity_check()
{

}
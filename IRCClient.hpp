#ifndef IRCCLIENT_HPP
#define IRCCLIENT_HPP

#include <string>
#include <algorithm>
#include <vector>
#include <sys/socket.h>
#include <netdb.h>

#define USER_MODES() ("aiwroO")

enum e_user_status {
	UNREGISTERED, PASSWORD, NICKNAME, REGISTERED
};

class IRCClient {
	public:

		IRCClient(int fd);
		~IRCClient();

		int get_fd() const;
		std::string const & get_nickname(void) const;
		std::string const & get_username(void) const;
		std::string const & get_hostname(void) const;
		std::string const & get_realname(void) const;
		std::string const & get_mode(void) const;
		std::vector<std::string> get_channels(void) const;
		int get_status(void) const;
		std::string get_prefix() const;

		void set_nickname(std::string nickname);
		void set_username(std::string username);
		void set_hostname(std::string username);
		void set_realname(std::string realname);
		void set_password(std::string password);
		void set_mode(char sign, char mode);
		void set_status(int status);

		bool is_visible(void) const;


	private:

		int _fd;
		int _status;

		std::string _nickname;
		std::string _username;
		std::string _realname;
		std::string _password;
		std::string	_mode;
		//TODO: to be removed?
		std::string _hostname;

		IRCClient();
		IRCClient(IRCClient const & cpy);
		IRCClient & operator=(IRCClient const & rhs);

};

#endif
#ifndef MESSAGE_HPP
# define MESSAGE_HPP

//rfc2812 bnf formats for fmatch function

# define LETTER				"[a:z]+[A:Z]"
# define DIGIT				"[0:9]"
# define HEXDIGIT			DIGIT "+A+B+C+D+E+F"
# define SPECIAL			"[%0x5B:%0x60]+[%0x7B:%0x7D]"

# define NICKNAME			"%" LETTER "+" SPECIAL "%(:8)" LETTER "+" DIGIT "+" SPECIAL "+-"
# define USERNAME			"%(1:)[%0x01:%0x09]+[%0x0B:%0x0C]+[%0x0E:%0x1F]+[%0x21:%0x3F]+[%0x41:%0xFF]"
# define REALNAME			"%" LETTER "%(:)" LETTER "+ "
//I assume real name start with a letter and then any letter and spaces (like in the example)
# define USERMODE			"*((:)[%%0x2B+-%(:)i+w+o+O+r])"
# define CHANNELMODE		"*((:)[%%0x2B+-%(:)O+o+v+a+i+m+n+q+p+r+s+t+k+l+b+e+I])"
# define CHANSTRING			"%(1:)[%0x01:%0x09]+[%0x0B:%0x0C]+[%0x0E:%0x1F]+[%0x21:%0x2B]+[%0x2D:%0x39]+[%0x3B:%0xFF]"
//It's not clear in rfc2812 but I assume a CHANSTRING is a least one char long
# define CHANNELID			"%(5)[A:Z]+" DIGIT
# define CHANNEL			"*((1)[#+%0x2B+&]+(1)[!" CHANNELID "])" CHANSTRING "*((0:1)[:" CHANSTRING "])"
//I assume KEY cannot contain ',' else we can't parse KEY_LIST
# define KEY				"%(1:23)[%0x01:%0x05]+[%0x07:%0x08]+%0x0C+[%0x0E:%0x1F]+[%0x21:%0x2B]+[%0x2D:%0x7F]"
//Must SHORTNAME end with letter or digit (and not -)? bnf is not clear
# define SHORTNAME			"%" LETTER "+" DIGIT "%(:)" LETTER "+" DIGIT "+-" "%(:)" LETTER "+" DIGIT
# define HOSTNAME			SHORTNAME "*((:)[." SHORTNAME "])"
# define SERVERNAME			HOSTNAME
# define IP4ADDR			"%(1:3)" DIGIT "." "%(1:3)" DIGIT "." "%(1:3)" DIGIT "." "%(1:3)" DIGIT
# define IP6ADDR			"*((1)[%(1)" HEXDIGIT "*((7)[:" HEXDIGIT "])]+(1)[0:0:0:0:0:%0+(4)F:" IP4ADDR "])"
# define HOSTADDR			"*((1)[" IP4ADDR "]+(1)[" IP6ADDR "])"
# define HOST				"*((1)[" HOSTNAME "]+(1)[" HOSTADDR "])"
# define NICK_USER_HOST		NICKNAME "!" USERNAME "@" HOST
//# define MSGTO				"*((1)[" CHANNEL "]+(1)[" USERNAME "*((0:1)[%0x25" HOST "])" "@" SERVERNAME "\
//]+(1)[" USERNAME "%0x25" HOST "]+(1)[" NICKNAME "]+(1)[" NICKNAME "!" USERNAME "@" HOST "])"

# define MSGTARGET			MSGTO "*((:)[," MSGTO "])"
# define CHANNEL_LIST		CHANNEL "*((:)[," CHANNEL "])"
# define KEY_LIST			KEY "*((:)[," KEY "])"


#define ERR_NOORIGIN 409
#define ERR_NORECIPIENT 411
#define ERR_NOTEXTTOSEND 412
#define ERR_NONICKNAMEGIVEN 431
#define ERR_ERRONEUSNICKNAME 432
#define ERR_NEEDMOREPARAMS 461
#define ERR_BADCHANMASK 476
#define ERR_UMODEUNKNOWNFLAG 501
#define ERR_UNKNOWNMODE 472
#define ERR_ERROR 600

#define OK 0
#define KO 1

#endif
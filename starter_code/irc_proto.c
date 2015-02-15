#include "irc_proto.h"
#include "debug.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include "sircd.h"

#define MAX_COMMAND 16

/* You'll want to define the CMD_ARGS to match up with how you
 * keep track of clients.  Probably add a few args...
 * The command handler functions will look like
 * void cmd_nick(CMD_ARGS)
 * e.g., void cmd_nick(your_client_thingy *c, char *prefix, ...)
 * or however you set it up.
 */

#define CMD_ARGS char *prefix, char **params, int n_params
typedef void (*cmd_handler_t)(CMD_ARGS);
#define COMMAND(cmd_name) void cmd_name(CMD_ARGS)
    

struct dispatch {
    char cmd[MAX_COMMAND];
    int needreg; /* Must the user be registered to issue this cmd? */
    int minparams; /* send NEEDMOREPARAMS if < this many params */
    cmd_handler_t handler;
};


#define NELMS(array) (sizeof(array) / sizeof(array[0]))

/* Define the command handlers here.  This is just a quick macro
 * to make it easy to set things up */
COMMAND(cmd_nick);
COMMAND(cmd_user);
COMMAND(cmd_quit);
/* Fill in the blanks */

/* Dispatch table.  "reg" means "user must be registered in order
 * to call this function".  "#param" is the # of parameters that
 * the command requires.  It may take more optional parameters.
 */
struct dispatch cmds[] = {
    /* cmd,    reg  #parm  function */
    { "NICK",    0, 0, cmd_nick },
    { "USER",    0, 4, cmd_user },
    { "QUIT",    1, 0, cmd_quit },
    /* Fill in the blanks... */
};

/* Handle a command line.  NOTE:  You will probably want to
 * modify the way this function is called to pass in a client
 * pointer or a table pointer or something of that nature
 * so you know who to dispatch on...
 * Mostly, this is here to do the parsing and dispatching
 * for you
 *
 * This function takes a single line of text.  You MUST have
 * ensured that it's a complete line (i.e., don't just pass
 * it the result of calling read()).  
 * Strip the trailing newline off before calling this function.
 */

void
handle_line(char *line)
{
    char *prefix = NULL, *command, *pstart, *params[MAX_MSG_TOKENS];
    int n_params = 0;
    char *trailing = NULL;

    DPRINTF(DEBUG_INPUT, "Handling line: %s\n", line);
    command = line;
    if (*line == ':') {
	prefix = ++line;
	command = strchr(prefix, ' ');
    }
    if (!command || *command == '\0') {
	/* Send an unknown command error! */
        some_of_your_code_better_go_here();
	return;
    }
    
    while (*command == ' ') {
	*command++ = 0;
    }
    if (*command == '\0') {
        and_more_of_your_code_should_go_here();
	/* Send an unknown command error! */
	return;
    }
    pstart = strchr(command, ' ');
    if (pstart) {
	while (*pstart == ' ') {
	    *pstart++ = '\0';
	}
	if (*pstart == ':') {
	    trailing = pstart;
	} else {
	    trailing = strstr(pstart, " :");
	}
	if (trailing) {
	    while (*trailing == ' ')
		*trailing++ = 0;
	    if (*trailing == ':')
		*trailing++ = 0;
	}
	
	do {
	    if (*pstart != '\0') {
		params[n_params++] = pstart;
	    } else {
		break;
	    }
	    pstart = strchr(pstart, ' ');
	    if (pstart) {
		while (*pstart == ' ') {
		    *pstart++ = '\0';
		}
	    }
	} while (pstart != NULL && n_params < MAX_MSG_TOKENS);
    }

    if (trailing && n_params < MAX_MSG_TOKENS) {
	params[n_params++] = trailing;
    }
    
    DPRINTF(DEBUG_INPUT, "Prefix:  %s\nCommand: %s\nParams (%d):\n",
	    prefix ? prefix : "<none>", command, n_params);
    int i;
    for (i = 0; i < n_params; i++) {
	DPRINTF(DEBUG_INPUT, "   %s\n", params[i]);
    }
    DPRINTF(DEBUG_INPUT, "\n");

    for (i = 0; i < NELMS(cmds); i++) {
	if (!strcasecmp(cmds[i].cmd, command)) {
	    if (cmds[i].needreg && /* YOUR TEST HERE TO
                                      SEE IF CLIENT IS REGISTERED */) {
                youshouldputcodehere();
		/* ERROR - the client is not registered and they need
		 * to be in order to use this command! */
	    } else if (n_params < cmds[i].minparams) {
		/* ERROR - the client didn't specify enough parameters
		 * for this command! */
                and_you_should_put_code_here_too();
	    } else {
		/* Here's the call to the cmd_foo handler... modify
		 * to send it the right params per your program
		 * structure. */
		(*cmds[i].handler)(prefix, params, n_params);
	    }
	    break;
	}
    }
    if (i == NELMS(cmds)) {
	/* ERROR - unknown command! */
        yet_again_you_should_put_code_here();
    }
}



/* Command handlers */
/* MODIFY to take the arguments you specified above! */
void cmd_nick(char *prefix, char **params, int n_params)
{
    /* Do something */
}

void cmd_user(char *prefix, char **params, int n_params)
{
}

void cmd_quit(char *prefix, char **params, int n_params)
{
}

/* And so on */


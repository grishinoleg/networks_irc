#include "irc_proto.h"
#include "debug.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include "sircd.h"

#define MAX_COMMAND 16


/* Number of elements */

#define NELMS(array) (sizeof(array) / sizeof(array[0]))


/* You'll want to define the CMD_ARGS to match up with how you
 * keep track of clients.  Probably add a few args...
 * The command handler functions will look like
 * void cmd_nick(CMD_ARGS)
 * e.g., void cmd_nick(your_client_thingy *c, char *prefix, ...)
 * or however you set it up.
 */

#define CMD_ARGS char *prefix, char **params, int n_params

typedef void (*cmd_handler_t)(CMD_ARGS);

struct dispatch {
    char cmd[MAX_COMMAND];
    int needreg; /* Must the user be registered to issue this cmd? */
    int minparams; /* send NEEDMOREPARAMS if < this many params */
    cmd_handler_t handler;
};


/* Command handlers */

/* NICK – Give the user a nickname or change the previous one. Your server should report
an error message if a user attempts to use an already-taken nickname. */

void cmd_nick(CMD_ARGS) {
    /* Do something */
}


/* USER – Specify the username, hostname, and real name of a user. */

void cmd_user(CMD_ARGS) {
}


/* QUIT – End the client session. The server should announce the client’s departure to all
other users sharing the channel with the departing client. */

void cmd_quit(CMD_ARGS) {
}


/* Channel Commands
JOIN – Start listening to a specific channel. Although the standard IRC protocol allows a
client to join multiple channels simultaneously, your server should restrict a client to be a
member of at most one channel. Joining a new channel should implicitly cause the client
to leave the current channel. */

void cmd_join(CMD_ARGS) {
}


/* PART – Depart a specific channel. Though a user may only be in one channel at a time,
PART should still handle multiple arguments. If no such channel exists or it exists but the
user is not currently in that channel, send the appropriate error message. */

void cmd_part(CMD_ARGS) {
}


/* LIST – List all existing channels on the local server only. Your server should ignore
parameters and list all channels and the number of users on the local server in each channel.
Advanced Commands */

void cmd_list(CMD_ARGS) {
}


/* PRIVMSG – Send messages to users. The target can be either a nickname or a channel. If
the target is a channel, the message will be broadcast to every user on the specified channel,
except the message originator. If the target is a nickname, the message will be sent only to
that user. */

void cmd_privmsg(CMD_ARGS) {
}


/* WHO – Query information about clients or channels. In this project, your server only needs
to support querying channels on the local server. It should do an exact match on the channel
name and return the users on that channel. */

void cmd_who(CMD_ARGS) {
}


/* Dispatch table.  "reg" means "user must be registered in order
 * to call this function".  "#param" is the # of parameters that
 * the command requires.  It may take more optional parameters.
 */
struct dispatch cmds[] = {
    /* cmd,    reg  #parm  function */
    { "NICK",    0, 0, cmd_nick    },
    { "USER",    0, 4, cmd_user    },
    { "QUIT",    0, 0, cmd_quit    },
    { "JOIN",    1, 1, cmd_join    },
    { "PART",    1, 1, cmd_part    },
    { "LIST",    1, 0, cmd_list    },
    { "PRIVMSG", 1, 2, cmd_privmsg },
    { "WHO",     1, 0, cmd_who     },
};


/* Handle a command line.  NOTE:  You will probably want to
 * modify the way this function is called to pass in a client
 * pointer or a table pointer or something of that nature
 * so you know who to dispatch on...
 * Mostly, this is here to do the parsing and dispatching
 * for you
 *
 * This function takes a single line (i.e., don't just pass
 * it the result of calling readline of text.  You MUST have
 * ensured that it's a complete ()).
 * Strip the trailing newline off before calling this function.
 */

void handle_line(char *line) {
    char *prefix = NULL, *trailing = NULL;
    char *command, *pstart, *params[MAX_MSG_TOKENS];
    int n_params = 0;

    DPRINTF(DEBUG_INPUT, "Handling line: %s\n", line);

    command = line;
    if (*line == ':') {
        prefix = ++line;
        command = strchr(prefix, ' ');
    }

    if (!command || *command == '\0') {
        /* Send ERR_UNKNOWNCOMMAND */
        return;
    }

    while (*command == ' ') {
        *command++ = 0;
    }

    if (*command == '\0') {
        /* Send ERR_UNKNOWNCOMMAND */
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
    	    if (cmds[i].needreg /* && YOUR TEST HERE TO
                                          SEE IF CLIENT IS REGISTERED */) {
                // raise ERR_NOTREGISTERED
        		/* ERROR - the client is not registered and they need
        		 * to be in order to use this command! */
            } else if (n_params < cmds[i].minparams) {
    		/* ERROR - the client didn't specify enough parameters
    		 * for this command! */
                // raise ERR_NEEDMOREPARAMS
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
        // raise ERR_UNKNOWNCOMMAND
    }
}

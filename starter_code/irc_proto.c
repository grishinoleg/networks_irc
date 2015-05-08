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

#define CMD_ARGS client *client, char *prefix, char **params, int n_params

typedef void (*cmd_handler_t)(CMD_ARGS);

struct dispatch {
    char cmd[MAX_COMMAND];
    int needreg; /* Must the user be registered to issue this cmd? */
    int minparams; /* send NEEDMOREPARAMS if < this many params */
    cmd_handler_t handler;
};

/* Error message */
char err[MAX_MSG_LEN];
char hostname[MAX_HOSTNAME];


/* Command handlers */

/* NICK – Give the user a nickname or change the previous one. Your server should report
an error message if a user attempts to use an already-taken nickname. */

void cmd_nick(CMD_ARGS) {

    if (!is_valid_nick(params[0])) {

        snprintf(err, MAX_MSG_LEN, ":%s %d %s %s :Erroneus nickname\n",
            hostname, ERR_NICKNAMEINUSE, client->nick, params[0]);
        write_to_client(client->sock, err);
        return;
    }

    // silently do nothing if user changes nick to its own
    if ( !strcasecmp(client->nick, params[0]) ) {
        return;
    }

    if ( check_nick(client, params[0]) ) {
        // ERR_NICKNAMEINUSE
        snprintf(err, MAX_MSG_LEN, ":%s %d %s %s :Nickname is already in use\n",
            hostname, ERR_NICKNAMEINUSE, client->nick, params[0]);
        write_to_client(client->sock, err);
    } else {
        char *prev = strdup(client->nick);
        strcpy(client->nick, params[0]);
        client->registered_nick = 1;
        if (client->registered_user) {
            client->registered = 1;
            // Notify other users only if registered
            notify_nick_change(client, prev);
        }
    }

}


/* USER – Specify the username, hostname, and real name of a user. */

void cmd_user(CMD_ARGS) {

    if (client->registered) {
        // ERR_ALREADYREGISTRED
        snprintf(err, MAX_MSG_LEN, ":%s %d %s :You may not reregister\n",
            hostname, ERR_ALREADYREGISTRED, client->nick);
        write_to_client(client->sock, err);
        return;
    }
    strcpy(client->user, params[0]);
    strcpy(client->servername, params[2]);
    strcpy(client->realname, params[3]);
    client->registered_user = 1;
    if (client->registered_nick) {
        client->registered = 1;
    }

}


/* QUIT – End the client session. The server should announce the client’s departure to all
other users sharing the channel with the departing client. */

void cmd_quit(CMD_ARGS) {

    if (*client->channel) {
        notify_quit(client, params[0], n_params);
    }

    quit_client (client);

}


/* PART – Depart a specific channel. Though a user may only be in one channel at a time,
PART should still handle multiple arguments. If no such channel exists or it exists but the
user is not currently in that channel, send the appropriate error message. */

void cmd_part(CMD_ARGS) {

    for (int i = 0; i < n_params; i++) {
        if (strcasecmp(client->channel, params[i])) {
            // ERR_NOTONCHANNEL
            snprintf(err, MAX_MSG_LEN, ":%s %d %s %s :You're not on that channel\n",
                hostname, ERR_NOTONCHANNEL, client->nick, params[i]);
            write_to_client(client->sock, err);
        } else {
            snprintf(err, MAX_MSG_LEN, ":leaving %s", params[i]);
            notify_quit(client, err, 1);
        }
    }
}


/* Channel Commands
JOIN – Start listening to a specific channel. Although the standard IRC protocol allows a
client to join multiple channels simultaneously, your server should restrict a client to be a
member of at most one channel. Joining a new channel should implicitly cause the client
to leave the current channel. */

void cmd_join(CMD_ARGS) {

    if (!is_valid_channel(params[0])) {

        snprintf(err, MAX_MSG_LEN, ":%s %d %s %s :No such channel\n",
            hostname, ERR_NOSUCHCHANNEL, client->nick, params[0]);
        write_to_client(client->sock, err);
        return;
    }

    // we only care about the first channel, thus params[0]
    if ( strcasecmp(client->channel, params[0]) ) {

        char *ch = strdup(params[0]);
        if ( *client->channel) {
            // so we don't have to rewrite our function for part
            strcpy(params[0], client->channel);
            cmd_part(client, prefix, params, 1);
        }
        strcpy(client->channel, ch);
        notify_channel_join(client, ch);
        list_after_join(client, ch);
    }
}


/* LIST – List all existing channels on the local server only. Your server should ignore
parameters and list all channels and the number of users on the local server in each channel.
Advanced Commands */

void cmd_list(CMD_ARGS) {
    snprintf(err, MAX_MSG_LEN, ":%s Channel :Users Name\n", hostname);
    write_to_client(client->sock, err);
    list_all_channels(client);
    snprintf(err, MAX_MSG_LEN, ":%s :End of /LIST\n", hostname);
    write_to_client(client->sock, err);
}


/* PRIVMSG – Send messages to users. The target can be either a nickname or a channel. If
the target is a channel, the message will be broadcast to every user on the specified channel,
except the message originator. If the target is a nickname, the message will be sent only to
that user. */

void cmd_privmsg(CMD_ARGS) {
    char *token = strtok(params[0], ",");
    while (token != NULL) {
        if (channel_exists(token)) {
            send_to_channel(client, token, params[1]);
        } else {
            if (!send_to_user(client, token, params[1])) {
                snprintf(err, sizeof(err), ":%s %s :No such nick/channel\n",
                    hostname, token);
                write_to_client(client->sock, err);
            }
        }
        token = strtok(NULL, ",");
    }
}


/* WHO – Query information about clients or channels. In this project, your server only needs
to support querying channels on the local server. It should do an exact match on the channel
name and return the users on that channel. */

void cmd_who(CMD_ARGS) {

    if (n_params == 0) {
        if (channel_exists(client->channel)) {
            list_users_on(client, client->channel);
        }
    } else {
        char *token = strtok(params[0], ",");
        while (token != NULL) {
            if (channel_exists(token)) {
                list_users_on(client, token);
            }
            token = strtok(NULL, ",");
        }
    }

}


/* Dispatch table.  "reg" means "user must be registered in order
 * to call this function".  "#param" is the # of parameters that
 * the command requires.  It may take more optional parameters.
 */
struct dispatch cmds[] = {
    /* cmd,    reg  #parm  function */
    { "NICK",    0, 1, cmd_nick    },
    { "USER",    0, 4, cmd_user    },
    { "QUIT",    1, 0, cmd_quit    },
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

void handle_line(char *line, client *client) {
    char *prefix = NULL, *trailing = NULL;
    char *command, *pstart, *params[MAX_MSG_TOKENS];
    int n_params = 0;

    gethostname(hostname, MAX_HOSTNAME);

    DPRINTF(DEBUG_INPUT, "Handling line: %s\n", line);

    command = line;
    if (*line == ':') {
        prefix = ++line;
        command = strchr(prefix, ' ');
    }

    if (!command || *command == '\0') {
        // ERR_UNKNOWNCOMMAND
        snprintf(err, sizeof(err), "%s :Unknown command\n", command);
        write_to_client(client->sock, err);
        return;
    }

    while (*command == ' ') {
        *command++ = 0;
    }

    if (*command == '\0') {
        // ERR_UNKNOWNCOMMAND
        snprintf(err, sizeof(err), "%s :Unknown command\n", command);
        write_to_client(client->sock, err);
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
    	    if (cmds[i].needreg && !client->registered ) {
        		/* ERROR - the client is not registered and they need
        		 * to be in order to use this command! */
                // ERR_NOTREGISTERED
                snprintf(err, MAX_MSG_LEN, ":%s %d %s :You have not registered\n",
                    hostname, ERR_NOTREGISTERED, client->nick);
                write_to_client(client->sock, err);
            } else if (n_params < cmds[i].minparams) {
        		/* ERROR - the client didn't specify enough parameters
        		 * for this command! */

                if (!strcasecmp(command, "nick")) {
                    // ERR_NONICKNAMEGIVEN
                    snprintf(err, MAX_MSG_LEN, ":%s %d %s :No nickname given\n",
                        hostname, ERR_NONICKNAMEGIVEN, client->nick);
                } else if (!strcasecmp(command, "privmsg")) {
                    if ( n_params == 0 ) {
                        // ERR_NORECIPIENT
                        snprintf(err, MAX_MSG_LEN, ":%s %d %s :No recipient given (NICK)\n",
                            hostname, ERR_NORECIPIENT, client->nick);
                    } else if ( n_params == 1 ) {
                        // ERR_NOTEXTTOSEND
                        snprintf(err, MAX_MSG_LEN, ":%s %d %s :No text to send\n",
                            hostname, ERR_NORECIPIENT, client->nick);
                    }
                } else {
                    // ERR_NEEDMOREPARAMS
                    snprintf(err, MAX_MSG_LEN, ":%s %d %s %s :Not enough parameters\n",
                        hostname, ERR_NEEDMOREPARAMS, client->nick, command);
                }
                write_to_client(client->sock, err);
            } else {
        		/* Here's the call to the cmd_foo handler... modify
        		 * to send it the right params per your program
        		 * structure. */
                (*cmds[i].handler)(client, prefix, params, n_params);
            }
            break;
        }
    }

    if (i == NELMS(cmds)) {
        /* ERROR - unknown command! */
        // ERR_UNKNOWNCOMMAND
        snprintf(err, MAX_MSG_LEN, ":%s %d %s %s ::Unknown command\n",
            hostname, ERR_UNKNOWNCOMMAND, client->nick, command);
        write_to_client(client->sock, err);
    }
}

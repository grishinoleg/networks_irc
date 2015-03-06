#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <assert.h>
#include "debug.h"
#include "rtlib.h"
// #include "rtgrading.h"
#include "sircd.h"
#include "irc_proto.h"
// #include "csapp.h"

// client code is terrible so we need this:
#include <unistd.h>

u_long curr_nodeID;
rt_config_file_t   curr_node_config_file;  /* The config_file  for this node */
rt_config_entry_t *curr_node_config_entry; /* The config_entry for this node */

void init_node(char *nodeID, char *config_file);
void irc_server();

client clients[FD_SETSIZE];
chan channels[MAX_CLIENTS];
char msg[MAX_MSG_LEN];
fd_set active_fd_set, read_fd_set;
char hostname[MAX_HOSTNAME];

#define LETTERS "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define NUMBERS "0123456789"
#define SPECIALS "-[]\\`^{}"
#define INVALID_CHAN_CHARS " \7\0\13\10,"

void print_mem(void const *vp, size_t n)
{
    unsigned char const *p = vp;
    for (size_t i=0; i<n; i++)
        printf("%02x\n", p[i]);
    putchar('\n');
};


void usage() {
    fprintf(stderr, "sircd [-h] [-D debug_lvl] <nodeID> <config file>\n");
    exit(-1);
}



int main( int argc, char *argv[] ) {
    extern char *optarg;
    extern int optind;
    int ch;

    gethostname(hostname, MAX_HOSTNAME);

    while ((ch = getopt(argc, argv, "hD:")) != -1)
        switch (ch) {
        	case 'D':
        	    if (set_debug(optarg)) {
            		exit(0);
        	    }
        	    break;
            case 'h':
            default: /* FALLTHROUGH */
                usage();
        }

    argc -= optind;
    argv += optind;

    if (argc < 2) {
    	usage();
    }

    init_node(argv[0], argv[1]);

    printf( "I am node %lu and I listen on port %d for new users\n", curr_nodeID, curr_node_config_entry->irc_port );

    /* Start your engines here! */
    irc_server();

    return 0;
}

int make_socket (uint16_t port) {
    int sock;
    struct sockaddr_in name;

    /* Create the socket. */
    sock = socket (PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror ("socket");
        exit (EXIT_FAILURE);
    }

    /* Give the socket a name. */
    name.sin_family = AF_INET;
    name.sin_port = htons (port);
    name.sin_addr.s_addr = htonl (INADDR_ANY);
    if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0) {
        perror ("bind");
        exit (EXIT_FAILURE);
    }
    return sock;
}

int read_from_client (int filedes) {
    /* plus one to accomodate null pointer at the end */
    char buffer[MAX_MSG_LEN], *token, *message;
    int nbytes;

    /* copy our saved command into the buffer, so we don't have to implement
    different logic down the line */
    strcpy(buffer, clients[filedes].inbuf);
    clients[filedes].inbuf_size = strlen(clients[filedes].inbuf);

    *clients[filedes].inbuf = '\0';

    nbytes = read(filedes, buffer+clients[filedes].inbuf_size, MAX_MSG_LEN-
        clients[filedes].inbuf_size);

    buffer[clients[filedes].inbuf_size+nbytes] = '\0';

    if (nbytes < 0) {
        /* Read error. */
        perror ("read");
        exit (EXIT_FAILURE);
    } else if (nbytes <= 2)
        /* End-of-file. */
        return -1;
    else {
        /* Data read. */

        if ( (token = strtok(buffer, "\r\n")) && strlen(token) != MAX_MSG_LEN-
                clients[filedes].inbuf_size ) {

            if (clients[filedes].skip) {
                token = strtok(NULL, "\r\n");
                fprintf (stderr, "Server: the message is too long\n");
                clients[filedes].skip = 0;
            } else {

                /* copy token to message */
                message = strdup(token);

                /* every time while runs, we save the last token in message */
                while ( (token = strtok(NULL, "\r\n")) ) {

                    fprintf (stderr, "Server: got message: '%s'\n", message);
                    handle_line(message, &clients[filedes]);
                    message = strdup(token);
                }

                /* if message ends in \r\n, it's valid; if not, we need to copy it over */
                if (buffer[nbytes+clients[filedes].inbuf_size-1] == '\n') {
                    fprintf (stderr, "Server: got message: '%s'\n", message);
                    handle_line(message, &clients[filedes]);
                    free(message);
                } else {

                    strcpy(clients[filedes].inbuf, message);
                    free(message);
                }

            }

        } else {

            fprintf (stderr, "Server: Command too long\n");

            /* There is no \r\n, which means this is not a valid command and it's too long */
            clients[filedes].skip = 1;

        }

    }

    return 0;

}

void irc_server() {

    extern int make_socket(uint16_t port);
    int sock;
    int i;
    struct sockaddr_in clientname;
    size_t size;
    /* Create the socket and set it up to accept connections. */
    sock = make_socket (curr_node_config_entry->irc_port);
    if (listen (sock, 1) < 0) {
        perror ("listen");
        exit (EXIT_FAILURE);
    }

    /* Initialize the set of active sockets. */
    FD_ZERO (&active_fd_set);
    FD_SET (sock, &active_fd_set);

    while (1) {
        /* Block until input arrives on one or more active sockets. */
        read_fd_set = active_fd_set;
        if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0) {
            perror ("select");
            exit (EXIT_FAILURE);
        }

        /* Service all the sockets with input pending. */
        for (i = 0; i < FD_SETSIZE; ++i)
            if (FD_ISSET (i, &read_fd_set)) {

                if (i == sock) {
                    /* Connection request on original socket. */
                    int new;
                    size = sizeof (clientname);
                    new = accept (sock,
                        (struct sockaddr *) &clientname, &size);
                    if (new < 0) {
                        perror ("accept");
                        exit (EXIT_FAILURE);
                    }

                    fprintf (stderr,
                        "Server: connect from host %s, port %hd.\n",
                        inet_ntoa (clientname.sin_addr),
                        ntohs (clientname.sin_port));
                    FD_SET (new, &active_fd_set);
                } else {

                    // I don't know why it's so weird, but original sockets
                    // are all accepted at i == 3, so I have to do it here
                    if (!clients[i].sock) {

                        clients[i].sock = i;
                        clients[i].cliaddr = (struct sockaddr_in) clientname;
                        clear_client(&clients[i]);

                        /* since hostname is ignored */
                        char *temp = strdup(inet_ntoa(clientname.sin_addr));
                        strcpy(clients[i].hostname, temp);
                    }

                    /* Data arriving on an already-connected socket. */
                    if (read_from_client (i) < 0) {
                        close (i);
                        FD_CLR (i, &active_fd_set);
                        if (clients[i].sock) {
                            char close[19];
                            strcpy(close, ":Connection closed");
                            notify_quit(&clients[i], close, 1);
                            clear_client(&clients[i]);
                        }
                    }
                }
            }
    }

}



/*
 * void init_node( int argc, char *argv[] )
 *
 * Takes care of initializing a node for an IRC server
 * from the given command line arguments
 */
void init_node(char *nodeID, char *config_file) {
    int i;

    curr_nodeID = atol(nodeID);
    rt_parse_config_file("sircd", &curr_node_config_file, config_file );

    // Get config file for this node
    for( i = 0; i < curr_node_config_file.size; ++i )
        if( curr_node_config_file.entries[i].nodeID == curr_nodeID )
             curr_node_config_entry = &curr_node_config_file.entries[i];

    /* Check to see if nodeID is valid */
    if( !curr_node_config_entry ) {
        printf( "Invalid NodeID\n" );
        exit(1);
    }
}

void clear_client(client *client) {
    client->inbuf_size = 0;
    client->registered = 0;
    client->registered_nick = 0;
    client->registered_user = 0;
    *client->user = '\0';
    *client->nick = '*';
    *client->hostname = '\0';
    *client->servername = '\0';
    *client->realname = '\0';
    *client->channel = '\0';
    client->skip = 0;
}

#include <unistd.h>

int write_to_client (int filedes, char *buffer) {
    int nbytes;

    usleep(10);
    nbytes = write (filedes, buffer, MAX_MSG_LEN);
    if (nbytes < 0) {
        /* Read error. */
        perror ("write");
        exit (EXIT_FAILURE);
    } else if (nbytes == 0)
        /* End-of-file. */
        return -1;
    else {
        /* Data read. */
        return 0;
    }
}

/* functions used in irc_proto */

int check_nick(client *client, char *nick) {

    for (int i = 0; i < FD_SETSIZE; ++i) {
        if (clients[i].sock != client->sock
            && !strcasecmp(clients[i].nick, nick)) {
            return 1;
        }
    }
    return 0;
}

void notify_nick_change(client *client, char *nick) {

    snprintf(msg, MAX_MSG_LEN, ":%s NICK %s\n",
        nick, client->nick);

    for (int i = 0; i < FD_SETSIZE; ++i) {
        // Notify everyone except the client himself
        if ( clients[i].registered && *clients[i].channel
            && client->sock != clients[i].sock
            && !strcasecmp(clients[i].channel, client->channel) ) {
            write_to_client(i, msg);
        }
    }
}

void add_channel_count(char *channel, int count) {
    int i;

    for (i = 0; i < MAX_CLIENTS; i++) {

        // when we find name that is not taken, means we looked through
        // all allocated chanells, since we never empty a channel's name
        if (!*channels[i].name) {
            strcpy(channels[i].name, channel);
            channels[i].active = 1;
            channels[i].count += count;
            return;
        }

        if (!strcasecmp(channels[i].name, channel)) {
            channels[i].count += count;
            if (channels[i].count > 0) {
                channels[i].active = 1;
            } else {
                channels[i].active = 0;
            }
            return;
        }
    }

}

void notify_quit(client *client, char *quit_msg, int n_params) {

    if (n_params) {
        snprintf(msg, MAX_MSG_LEN, ":%s QUIT %s\n",
            client->nick, quit_msg);
    } else {
        snprintf(msg, MAX_MSG_LEN, ":%s QUIT\n",
            client->nick);
    }

    for (int i = 0; i < FD_SETSIZE; ++i) {
        if (clients[i].registered && *clients[i].channel
            && client->sock != clients[i].sock
            && !strcasecmp(clients[i].channel, client->channel) ) {
            write_to_client(i, msg);
        }
    }

    add_channel_count(client->channel, -1);
}

void notify_channel_join(client *client, char *channel) {

    snprintf(msg, MAX_MSG_LEN, ":%s JOIN %s\n",
        client->nick, channel);

    for (int i = 0; i < FD_SETSIZE; ++i) {
        if (*clients[i].channel && clients[i].sock != client->sock
            && clients[i].registered
            && !strcasecmp(clients[i].channel, channel) ) {
            write_to_client(i, msg);
        }
    }

    add_channel_count(client->channel, 1);

}

void list_all_channels(client *client) {

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!*channels[i].name) {
            return;
        }
        if (channels[i].active) {
            snprintf(msg, MAX_MSG_LEN, ":%s %s %d\n",
                hostname, channels[i].name, channels[i].count);
            write_to_client(client->sock, msg);
        }
    }
}

void who_user(client *client, char *nickname) {

    for (int i = 0; i < FD_SETSIZE; ++i) {
        if (!strcasecmp(nickname, clients[i].nick) ) {
            snprintf(msg, MAX_MSG_LEN, "%s %s %s %s %s H :%s\n",
                nickname, clients[i].user, clients[i].hostname,
                clients[i].servername, clients[i].nick,
                clients[i].realname);
            write_to_client(client->sock, msg);
            return;
        }
    }
}

void list_users_on(client *client, char *channel) {

    for (int i = 0; i < FD_SETSIZE; ++i) {
        if (!strcasecmp(channel, clients[i].channel) ) {
            snprintf(msg, MAX_MSG_LEN, "%s %s %s %s %s H :%s\n",
                channel, clients[i].user, clients[i].hostname,
                clients[i].servername, clients[i].nick,
                clients[i].realname);
            write_to_client(client->sock, msg);
        }
    }
    snprintf(msg, MAX_MSG_LEN, "%s :End of /WHO list\n", channel);
    write_to_client(client->sock, msg);
}

int channel_exists(char *channel) {


    for (int i = 0; i < MAX_CLIENTS; i++) {

        if ( channels[i].active && !strcasecmp(channels[i].name, channel) ) {
            return 1;
        }

        if (!*channels[i].name) {
            return 0;
        }
    }

    return 0;

}

void send_to_channel(client *client, char *channel, char *message) {

    for (int i = 0; i < FD_SETSIZE; ++i) {
        if (clients[i].registered &&
            !strcasecmp(channel, clients[i].channel) ) {
            snprintf(msg, sizeof(msg), ":%s PRIVMSG %s :%s\n",
                client->nick, channel, message);
            write_to_client(clients[i].sock, msg);
        }
    }
}

int send_to_user(client *client, char *nick, char *message) {

    char msg[MAX_MSG_LEN+MAX_USERNAME*2+10];

    for (int i = 0; i < FD_SETSIZE; ++i) {
        if (!strcasecmp(nick, clients[i].nick) ) {
            snprintf(msg, sizeof(msg), ":%s PRIVMSG %s :%s\n",
                client->nick, nick, message);
            write_to_client(clients[i].sock, msg);
            return 1;
        }
    }
    return 0;
}

void quit_client(client *client) {
    close (client->sock);
    FD_CLR (client->sock, &active_fd_set);
    clear_client(client);
}

int is_valid_nick(char *nickname) {
    int len = strlen(nickname);
    if ( len > MAX_NICKNAME || !strchr(LETTERS, *nickname) || strspn(nickname, LETTERS NUMBERS SPECIALS)!=len )
        return 0;

    return 1;
}

int is_valid_channel(char *channel) {
    int len = strlen(channel);
    if ( len > MAX_CHANNAME || !strchr("#&", *channel) || strcspn(channel, INVALID_CHAN_CHARS)!=len )
        return 0;

    return 1;
}

void list_after_join(client *client, char *channel) {
    snprintf(msg, MAX_MSG_LEN, "%s :", channel);

    for (int i = 0; i < FD_SETSIZE; ++i) {
        if ( !strcasecmp(channel, clients[i].channel) ) {
            strcat(msg, clients[i].nick);
            strcat(msg, " ");
        }
    }

    strcat(msg, "\n");

    write_to_client(client->sock, msg);

    snprintf(msg, MAX_MSG_LEN, "%s :End of /NAMES list", channel);

}
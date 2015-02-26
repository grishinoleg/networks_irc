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
#include "csapp.h"

u_long curr_nodeID;
rt_config_file_t   curr_node_config_file;  /* The config_file  for this node */
rt_config_entry_t *curr_node_config_entry; /* The config_entry for this node */

void init_node(char *nodeID, char *config_file);
void irc_server();


void usage() {
    fprintf(stderr, "sircd [-h] [-D debug_lvl] <nodeID> <config file>\n");
    exit(-1);
}



int main( int argc, char *argv[] ) {
    extern char *optarg;
    extern int optind;
    int ch;

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
//
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

int write_to_client (int filedes, char *buffer) {
    int nbytes;

    strcat(buffer, "\r\n");

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
        fprintf (stderr, "Server: sent message back: '%s'\n", buffer);
        return 0;
    }
}

int read_from_client (int filedes, char *saved, int *skip) {
    /* plus one to accomodate null pointer at the end */
    char buffer[MAX_MSG_LEN], *token, *message;
    int nbytes;

    /* copy our saved command into the buffer, so we don't have to implement
    different logic down the line */
    strcpy(buffer, saved);
    int saved_len = strlen(saved);

    // fprintf (stderr, "0: saved_len: %d. saved is \n'%s'\n", saved_len, saved);

    *saved = '\0';

    nbytes = read(filedes, buffer+saved_len, MAX_MSG_LEN-saved_len);

    buffer[saved_len+nbytes] = '\0';
    // fprintf (stderr, "0: got %d bytes. Buffer is \n'%s'\n", nbytes, buffer);

    if (nbytes < 0) {
        /* Read error. */
        perror ("read");
        exit (EXIT_FAILURE);
    } else if (nbytes <= 2)
        /* End-of-file. */
        return -1;
    else {
        /* Data read. */

        if ( (token = strtok(buffer, "\r\n")) && strlen(token) != MAX_MSG_LEN-saved_len ) {

            // fprintf (stderr, "1: '%s'\n", token);


            if (*skip) {
                token = strtok(NULL, "\r\n");
                fprintf (stderr, "Server: the message is too long\n");
                *skip = 0;
            } else {

                /* copy token to message */
                message = strdup(token);

                /* every time while runs, we save the last token in message */
                while ( (token = strtok(NULL, "\r\n")) ) {
                    // fprintf (stderr, "whiling\n");
                    fprintf (stderr, "Server: got message: '%s'\n", message);
                    write_to_client(filedes, message);
                    message = strdup(token);
                }

                /* if message ends in \r\n, it's valid; if not, we need to copy it over */
                if (buffer[nbytes+saved_len-1] == '\n') {
                    fprintf (stderr, "Server: got message: '%s'\n", message);
                    write_to_client(filedes, message);
                    free(message);
                } else {
                    fprintf(stderr, "Server: saving %s\n", message);
                    strcpy(saved, message);
                    free(message);
                }

            }

        } else {

            fprintf (stderr, "Server: Command too long\n");

            /* There is no \r\n, which means this is not a valid command and it's too long */
            *skip = 1;

        }

    }

    return 0;

}

void irc_server() {

    extern int make_socket(uint16_t port);
    int sock;
    fd_set active_fd_set, read_fd_set;
    int i;
    struct sockaddr_in clientname;
    size_t size;
    /* Create the socket and set it up to accept connections. */
    sock = make_socket (curr_node_config_entry->irc_port);
    if (listen (sock, 1) < 0) {
        perror ("listen");
        exit (EXIT_FAILURE);
    }

    /* Initialize array that stores commands that carry over,
    and remembers which inputs to skip */
    char saved[FD_SETSIZE][MAX_MSG_LEN];
    int skip[FD_SETSIZE];

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
                    /* Data arriving on an already-connected socket. */
                    if (read_from_client (i, saved[i], &skip[i]) < 0) {
                        close (i);
                        FD_CLR (i, &active_fd_set);
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
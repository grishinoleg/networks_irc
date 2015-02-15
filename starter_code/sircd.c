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
#include "rtgrading.h"
#include "sircd.h"
#include "irc_proto.h"

u_long curr_nodeID;
rt_config_file_t   curr_node_config_file;  /* The config_file  for this node */
rt_config_entry_t *curr_node_config_entry; /* The config_entry for this node */

void init_node(char *nodeID, char *config_file);
void irc_server();


void
usage() {
    fprintf(stderr, "sircd [-h] [-D debug_lvl] <nodeID> <config file>\n");
    exit(-1);
}



int main( int argc, char *argv[] )
{
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
    
    init_node(argv[0], argv[1]);
    
    printf( "I am node %d and I listen on port %d for new users\n", curr_nodeID, curr_node_config_entry->irc_port );

    /* Start your engines here! */
    
    return 0;
}

/*
 * void init_node( int argc, char *argv[] )
 *
 * Takes care of initializing a node for an IRC server
 * from the given command line arguments
 */
void
init_node(char *nodeID, char *config_file) 
{
    int i;

    curr_nodeID = atol(nodeID);
    rt_parse_config_file("sircd", &curr_node_config_file, config_file );

    /* Get config file for this node */
    for( i = 0; i < curr_node_config_file.size; ++i )
        if( curr_node_config_file.entries[i].nodeID == curr_nodeID )
             curr_node_config_entry = &curr_node_config_file.entries[i];

    /* Check to see if nodeID is valid */
    if( !curr_node_config_entry )
    {
        printf( "Invalid NodeID\n" );
        exit(1);
    }
}


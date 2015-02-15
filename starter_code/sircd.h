#ifndef _SIRCD_H_
#define _SIRCD_H_

#include <sys/types.h>
#include <netinet/in.h>

#define MAX_CLIENTS 512
#define MAX_MSG_TOKENS 10
#define MAX_MSG_LEN 512
#define MAX_USERNAME 32
#define MAX_HOSTNAME 512
#define MAX_SERVERNAME 512
#define MAX_REALNAME 512
#define MAX_CHANNAME 512

typedef struct {
    int sock;
    struct sockaddr_in cliaddr;
    unsigned inbuf_size;
    int registered;
    char hostname[MAX_HOSTNAME];
    char servername[MAX_SERVERNAME];
    char user[MAX_USERNAME];
    char nick[MAX_USERNAME];
    char realname[MAX_REALNAME];
    char inbuf[MAX_MSG_LEN+1];
    char channel[MAX_CHANNAME];
} client;

#endif /* _SIRCD_H_ */

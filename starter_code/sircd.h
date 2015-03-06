#ifndef _SIRCD_H_
    #define _SIRCD_H_

    #include <sys/types.h>
    #include <netinet/in.h>

    #define MAX_CLIENTS 512
    #define MAX_MSG_TOKENS 10
    #define MAX_MSG_LEN 512
    #define MAX_USERNAME 32
    #define MAX_NICKNAME 9
    #define MAX_HOSTNAME 512
    #define MAX_SERVERNAME 512
    #define MAX_REALNAME 512
    #define MAX_CHANNAME 9

    typedef struct {
        int sock;
        struct sockaddr_in cliaddr;
        unsigned inbuf_size;
        int registered_nick;
        int registered_user;
        int registered;
        char hostname[MAX_HOSTNAME];
        char servername[MAX_SERVERNAME];
        char user[MAX_USERNAME];
        char nick[MAX_USERNAME];
        char realname[MAX_REALNAME];
        char inbuf[MAX_MSG_LEN+1];
        char channel[MAX_CHANNAME];
        int skip;
        int quit;
    } client;

    typedef struct {
        int active;
        char name[MAX_CHANNAME];
        int count;
    } chan;

    void clear_client(client *client);
    int write_to_client (int filedes, char *buffer);
    int check_nick(client *client, char *nick);
    void notify_nick_change(client *client, char *nick);
    void notify_quit(client *client, char *quit_msg, int n_params);
    void notify_channel_join(client *client, char *channel);
    void list_all_channels(client *client);
    void list_users_on(client *client, char *channel);
    int channel_exists(char *channel);
    void send_to_channel(client *client, char *channel, char *msg);
    int send_to_user(client *client, char *nick, char *msg);
    void quit_client(client *client);
    void who_user(client *client, char *nickname);
    int is_valid_nick(char *nickname);
    int is_valid_channel(char *channel);
    void list_after_join(client *client, char *channel);

    void handle_line(char *line, client *client);

#endif /* _SIRCD_H_ */

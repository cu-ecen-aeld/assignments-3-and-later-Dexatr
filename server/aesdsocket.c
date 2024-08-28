#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "../aesd-char-driver/aesd_ioctl.h"

#define USE_AESD_CHAR_DEVICE 1

// Define the data file path
#if USE_AESD_CHAR_DEVICE
    #define DATA_FILE "/dev/aesdchar"
#else
    #define DATA_FILE "/var/tmp/aesdsocketdata"
#endif

static char is_exit = 0;
static const size_t buf_size = 256;
static const char file_path[] = DATA_FILE;
//static int file_descriptor;
int s_id;

static pthread_mutex_t file_mutex;
static pthread_mutex_t queue_mutex;

struct connect_item
{
    pthread_t thread;
    int completed;
    int cfd;
    struct connect_item *next;
};

static void signal_handler(int signo)
{
    syslog(LOG_INFO, "Ready to exit signal.");
    is_exit=1;
}

void* active_connection(void *element)
{
    struct connect_item *item = (struct connect_item *)element;
    int data_received = 0;

    uint32_t write_cmd;
    uint32_t write_cmd_offset;
    struct aesd_seekto seekto;

    int file_descriptor = open(file_path, O_CREAT | O_APPEND | O_WRONLY, 0666);

    for(;;) 
    {
        char buf[buf_size]; 


        int recv_len = recv(item->cfd, buf, sizeof(buf), 0);
        if (recv_len <= 0) 
        {
            if (data_received)
                break;
            else
                continue;
        }
        else
        {
            data_received = 1;
        }

        int i=0;
        int end = 0;
        for (i=0; i < recv_len; i++) {
            if (buf[i] == '\n') {
                i += 1;
                end = 1;
                break;
            }
        }
        syslog(LOG_INFO, "Buffer: %s", buf);


        int ret = sscanf(buf, "AESDCHAR_IOCSEEKTO:%u,%u\n", &write_cmd, &write_cmd_offset);
        if (ret == 2)
        {
            seekto.write_cmd = write_cmd;
            seekto.write_cmd_offset = write_cmd_offset;
        }
        else
        {
            pthread_mutex_lock(&file_mutex);
            write(file_descriptor, buf, i);
            pthread_mutex_unlock(&file_mutex);
        }
        
        
        if (end==1) {
            pthread_mutex_lock(&file_mutex);
            int rfd = open(file_path, O_RDONLY, 0);
            if (ret == 2)
                ioctl(rfd, AESDCHAR_IOCSEEKTO, &seekto);
            while (1) {
                int read_len = read(rfd, buf, sizeof(buf));
                if (read_len == 0) {
                    pthread_mutex_unlock(&file_mutex);
                    break;
                }

                send(item->cfd, buf, read_len, 0);
            }
            close(rfd);
        }
        
    }

    close(file_descriptor);

    item->completed = 1;
    return NULL;
}

static void alarm_handler(union sigval sigval)
{
	/*char outstr[200];

	time_t t = time(NULL);
	struct tm *tmp = localtime(&t);
	strftime(outstr, sizeof(outstr), "%a, %d %b %Y %T %z", tmp);

	pthread_mutex_lock(&file_mutex);
	write(file_descriptor, "timestamp:", strlen("timestamp:"));
	write(file_descriptor, outstr, strlen(outstr));
	write(file_descriptor, "\n", strlen("\n"));
	pthread_mutex_unlock(&file_mutex);*/
    
}

int main(int argc, char** argv)
{
    //timer_t timerid;
    struct sigevent sev;
    //struct itimerspec its;

    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = signal_handler;

    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGPIPE, &action, NULL);

    openlog(NULL, 0, LOG_USER);

    struct connect_item *head=NULL, *last=NULL;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));

    struct addrinfo *result;

    struct sockaddr_in  peer_addr = {};
    socklen_t peer_addr_size;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    s_id = socket(PF_INET, SOCK_STREAM, 0);
    if (s_id == -1)
    {
        fprintf(stderr, "Cannot open socket.\n");
        exit(EXIT_FAILURE);        
    }

    int s = getaddrinfo(NULL, "9000", &hints, &result);
    if (s != 0) 
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }
    int bind_res = bind(s_id, result->ai_addr, result->ai_addrlen);
    if (bind_res==-1)
    {
        fprintf(stderr, "Error in binding.\n");
        close(s_id);
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(result);

    if ((argc > 1) && (strcmp(argv[1],"-d") == 0)) 
    {
        int pid = fork();
        if (pid!=0)
        {
            exit(EXIT_SUCCESS);
        }
    }

    if (listen(s_id, 1)==-1)
    {
            fprintf(stderr, "Error in listening.\n");
            close(s_id);
            exit(EXIT_FAILURE);
    }

    printf("Listening\n");

    pthread_mutex_init(&queue_mutex, NULL);
    pthread_mutex_init(&file_mutex, NULL);

    //file_descriptor = open(file_path, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU  | S_IRGRP | S_IROTH);

    /*int clock_id = CLOCK_MONOTONIC;
    memset(&sev, 0, sizeof(struct sigevent));
    
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_value.sival_ptr = &timerid;
    sev.sigev_notify_function = alarm_handler;
    sev.sigev_notify_attributes = NULL;
    if (timer_create(clock_id, &sev, &timerid) == -1)
    {
        printf("create timer failed\n");
        timer_delete(timerid);
        return -1;
    }

    its.it_value.tv_sec = 10;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;

    if (timer_settime(timerid, 0, &its, NULL) == -1)
    {
        printf("set timer failed\n");
        timer_delete(timerid);
        return -1;
    }*/

    while(is_exit==0) 
    {
        peer_addr_size = sizeof(peer_addr);
        int cfd = accept(s_id, (struct sockaddr*)&peer_addr, &peer_addr_size);
        if (cfd==-1)
        {
            fprintf(stderr, "Error in accepting.\n");
            break;
        }
        char addrs[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &peer_addr.sin_addr, addrs, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", addrs);

        if (head==NULL)
        {
            head = malloc(sizeof(struct connect_item));
            last = head;
        }
        else
        {
            last->next = malloc(sizeof(struct connect_item));
            last = last->next;
        }

        last->completed = 0;
        last->cfd = cfd;
        last->next = NULL;
        if (pthread_create(&(last->thread), NULL, &active_connection, last) != 0)
		{
			perror("pthread_create");
			exit(EXIT_FAILURE);
		}
        syslog(LOG_INFO, "Thread %d has been created", (int)(last->thread));

        struct connect_item *plist_elem=head;
		while (plist_elem!=NULL)
		{
			if(plist_elem->completed==1)
			{
                syslog(LOG_INFO, "Joining thread %d", (int)(plist_elem->thread));
				pthread_join(plist_elem->thread, NULL);
                plist_elem->completed=-1;
                close(plist_elem->cfd);
				syslog(LOG_INFO, "Thread %d has been completed", (int)(plist_elem->thread));
			}
            plist_elem = plist_elem->next;
		}
    }

    syslog(LOG_INFO, "Exiting block");
    struct connect_item *plist_elem_c=head, *tmp_el;
    pthread_mutex_lock(&queue_mutex);
	while (plist_elem_c!=NULL)
	{
		if (plist_elem_c->completed!=-1)
        {
            syslog(LOG_INFO, "Joining thread %d", (int)(plist_elem_c->thread));
            pthread_join(plist_elem_c->thread, NULL);
		    close(plist_elem_c->cfd);
            syslog(LOG_INFO, "Thread %d has been completed", (int)(plist_elem_c->thread));
        }
        tmp_el=plist_elem_c;
        plist_elem_c=plist_elem_c->next;
        syslog(LOG_INFO, "Element for thread %d has been deleted", (int)(tmp_el->thread));
		free(tmp_el);
	}
    pthread_mutex_unlock(&queue_mutex);

    syslog(LOG_INFO, "Caught signal, exiting");
    closelog();
    //timer_delete(timerid);
    //close(file_descriptor);
    pthread_mutex_destroy(&file_mutex);
    pthread_mutex_destroy(&queue_mutex);
    close(s_id);
    //unlink(file_path);

    return 0;
}
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>

#include "library.h"


#define HASH_LIMIT 1000
#define READ_BYTES 15
#define SEND_BYTES 15
#define MAX_FETCH 100
#define TRUE 20
#define FALSE 0
#define LIST_COMMAND_ID 1
#define FETCH_COMMAND_ID 2
#define HISTORY_COMMAND_ID 3
#define QUIT_COMMAND_ID 4

// -------------------- Global Variables/Structures & Functions -------------------- //
int timer_limit; 
int port; 
pthread_mutex_t mutex;

typedef struct information
{
    char *url;
    char *content;
    time_t timer;
    struct information *next;
    struct information *previous;
}information;
information *hash_table[HASH_LIMIT];

void hash_initialize();
unsigned int hash_function(char *url);
void print_list();
int update_content(char *url, information *new_information);
int insert_and_update(char *url);
void fetch (int consocket);
void list (int consocket);
void remove_expired_information();

void print_list_sync();
void fetch_sync(int consocket);
void list_sync(int consocket);
void remove_expired_information_sync(int signum);

void* process_request(void *arguments);
void* waiting_for_connections(void *arguments);


void hash_initialize()
{
    for (int i = 0; i < HASH_LIMIT; ++i)
        hash_table[i] = NULL;
}

unsigned int hash_function(char *url)
{ 
    unsigned int index = 0;
    for (int i = 0; i < strlen(url); ++i)
        index += (int) url[i];

    return index % 1000;    
}

void print_list()
{
    information *temp = NULL;
    printf("--------------- CURRENT PROXY CONTENT ---------------\n");
    for (int i = 0; i < HASH_LIMIT; ++i)
    {
        temp = hash_table[i];
        while (temp != NULL)
        {
            printf("HASH INDEX:%d\n", i);
            printf("URL:\n%s\n", temp->url);
            printf("CONTENT:\n%s\n", temp->content);
            time_t now = time(NULL);
            time_t old = temp->timer;
            printf("TIMER:\n%.2f\n\n", difftime(now, old));
            temp = temp->next;
        }
    }
    printf("-----------------------------------------------------\n");
}
// ------------------------------------------------------------ FETCH ------------------------------------------------------------ //

int update_content(char *url, information *new_information) // Connection With the Internet through HTTP 
{
    // Defining only alpha-numeric characters in the file's name //
    char *file_name = (char *)calloc(100, sizeof(char));
    int len = 0;
    for (int i = 0; i < strlen(url); ++i)
    {
        if ((url[i] >= '0' && url[i] <= '9') || (url[i] >= 'a' && url[i] <= 'z') || (url[i] >= 'A' && url[i] <= 'z'))
        {
            file_name[len] = url[i];
            ++len;
        }
    }
    file_name[len] = '\0';
    strcat(file_name, ".html");

    // Defining the url's name //
    char* host = (char*)calloc(10000, sizeof(char));
    int counter = 0;
    int index = 0;
    for (int i = 0; i < strlen(url); ++i)
    {
        if (url[i] == '/')
            ++counter;

        if (counter == 2)
        {
            int j = i + 1;
            while (url[j] != '/' && url[j] != '\0')
            {
                host[index] = url[j];
                ++j;
                ++index;
            }
            host[index] = '\0';
            break;
        }    
    }

    // ---------- Connection ---------- //

    // 1) Get Addr Info
    struct addrinfo* urlInformation = NULL;
    struct addrinfo* auxiliarPointer = NULL;
    struct sockaddr* destinationSocket = NULL;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // We only want IPv4 addresses
    hints.ai_protocol = SOCK_STREAM; // TCP FTW 

    int error;
    // Resolve the domain name into a List of Addresses //
    error = getaddrinfo( host, NULL, &hints, &urlInformation );
    if (error != 0)
        return 0;
    
    destinationSocket = urlInformation->ai_addr;

    // 2) Socket Connection
    int socketFd = socket( AF_INET, SOCK_STREAM, 0 );
    ((struct sockaddr_in*) destinationSocket)->sin_port = htons( 80 ); //HTTP Port

    int result = connect( socketFd, destinationSocket, sizeof( struct sockaddr_in ) );
    if( result != 0 )
        return 0;

    char* pedido = (char*)calloc(500, sizeof(char));
    char* resposta = (char*) calloc(READ_BYTES + 5, sizeof(char ));    

    //3) Request/Reply and Assembling the Content file //
    char * data_dir = "./data/";   
    char file_path[100];   
    strcpy(file_path, "");
    strcat(file_path, data_dir);
    strcat(file_path, file_name);

    FILE * content = fopen(file_path, "w");
    if (content == NULL)
    {
        printf("Error: File cannot be opened.\n");
        exit(-1);
    }

    FILE * aux_content = fopen("./data/aux_content", "w+");
    if (aux_content == NULL)
    {
        printf("Error: File cannot be opened.\n");
        exit(-1);
    }

    sprintf(pedido, "GET %s HTTP/1.0\r\nAccept: text/plain, text/html, text/*\r\n\r\n", url);   
    send( socketFd, pedido, strlen(pedido) * sizeof(char), 0 );
    while (recv(socketFd, resposta, READ_BYTES * sizeof(char), 0) != 0)
    {
        fprintf(aux_content, "%s", resposta);
        memset(resposta, 0, (READ_BYTES + 5) * sizeof(char));
    }
    // Removing the Header
    int flag = 0;
    char ch;
    fseek(aux_content, 0, SEEK_SET);
    while((ch = fgetc(aux_content)) != EOF)
    {
        if (flag != 1 && ch == '<')
            flag = 1;
        if (flag == 1)
            fputc(ch, content);
    }
    
    close(socketFd);
    fclose(content);

    strcpy(new_information->content, file_name);

    return 1;
}

int insert_and_update(char *url)
{
    unsigned int index = hash_function(url);
    time_t now;
    if (hash_table[index] == NULL)
    {
        information *new_information = (information *)calloc(1, sizeof(information));
        new_information->url = (char *)calloc(strlen(url) + 1, sizeof(char));
        new_information->content = (char *)calloc(strlen(url) + 1, sizeof(char));

        strcpy(new_information->url, url);
        // Here we should update the content of the current url//        
        int state = update_content(url, new_information);
        
        if (state == 0)
            return 0;

        new_information->next = NULL;
        new_information->previous = NULL;
        // Update the Timer //
        now = time(NULL);
        new_information->timer = now;

        hash_table[index] = new_information;

        return 1;
    }

    else
    {   
        information *temp = hash_table[index];
        information *current = temp;

        while (temp != NULL)
        {
            if (strcmp(url, temp->url) == 0)
            {
                // Here we should update the content of the current url //
                int state = update_content(url, temp);
                if (state == 0)
                {
                    return 0;
                }
                // Update the Timer //
                now = time(NULL);
                temp->timer = now;

                return 1;
            }
            current = temp;
            temp = temp->next;                        
        }

        information *new_information = (information *)calloc(1, sizeof(information));   
        new_information->url = (char *)calloc(strlen(url) + 1, sizeof(char));
        new_information->content = (char *)calloc(strlen(url) + 1, sizeof(char));

        strcpy(new_information->url, url);   
        // Here we should update the content of the current url //
        int state = update_content(url, new_information);

        if (state == 0)
        {
            return 0;
        }
        // Update the Timer //
        now = time(NULL);
        new_information->timer = now;

        new_information->next = NULL;
        new_information->previous = current;

        current->next = new_information;        
        return 1;
    }    
}


void fetch (int consocket)
{    
    // Receiving the Url. //
    char *url = rec_string(consocket);
    
    // Inserting into the Hash Table and Updating the Content Files
    int state = insert_and_update(url);

    // If the content is nonexistent //
    if (state == 0)
    {
        send_int(-1, consocket);
        return;
    }   
        
    unsigned int index = hash_function(url);
    information *temp = hash_table[index];

    char * data_dir = "./data/";   
    char file_path[100];       
    while (temp != NULL)
    {
        if (strcmp(temp->url, url) == 0)        
        {
            strcpy(file_path, "");
            strcat(file_path, data_dir);
            strcat(file_path, temp->content);
            send_string(temp->content, consocket);
                
            FILE *content = fopen(file_path, "rb");
            if (content == NULL)
            {
                printf("Error: File cannot be opened.\n");
                exit(-1);
            }

            char *buffer = (char*)calloc(SEND_BYTES + 1, sizeof(char));
            int read = 1;
            while ((read = fread(buffer, sizeof(char), SEND_BYTES, content)) > 0) 
            {
                buffer[read] = '\0';
                send_string(buffer, consocket); 
                memset(buffer, 0, (SEND_BYTES + 1) * sizeof(char));
            }
            send_int(-1, consocket); 

            fclose(content); 
            break;
        }                
        temp = temp->next;
    }    
}

// ------------------------------------------------------------ LIST ------------------------------------------------------------ //
void list (int consocket)
{
    information *temp = NULL;               
    for (int i = 0; i < HASH_LIMIT; ++i)
    {
        temp = hash_table[i];
        while (temp != NULL)
        {
            send_string(temp->url, consocket);
            send_string(temp->content, consocket);           
            temp = temp->next;
        }
    }
    send_int(-1, consocket);
}

// ------------------------------------------------------------ REMOVE ------------------------------------------------------------ //
void remove_expired_information(int signum)
{
    information *temp = NULL;               
    information *aux = NULL;
    information *pre = NULL;
    information *nex = NULL;

    char * data_dir = "./data/";    
    char file_path[100];   

    char cmd[1000];
    for (int i = 0; i < 1000; ++i)
    {
        temp = hash_table[i];        
        while (temp != NULL)
        {
            pre = temp->previous;
            nex = temp->next;            
            time_t now = time(NULL);
            time_t old = temp->timer;
            double time_diff = difftime(now, old );
            
            strcpy(file_path, "");
            strcat(file_path, data_dir);
            strcat(file_path, temp->content);
            if (time_diff >= timer_limit)
            {
                if (pre == NULL && nex == NULL)
                {
                    hash_table[i] = NULL;
                    
                    sprintf(cmd, "rm %s", file_path);
                    system(cmd);                    
                    free(temp);
                }
                
                else if (pre == NULL)
                {                   
                    nex->previous = NULL;                    
                    hash_table[i] = nex;
                                     
                    sprintf(cmd, "rm %s", file_path);
                    system(cmd);                    
                    free(temp); 
                }

                else if (nex == NULL)
                {
                    pre->next = NULL;
                    
                    sprintf(cmd, "rm %s", file_path);
                    system(cmd);                    
                    free(temp);
                }

                else
                {
                    pre->next = nex;
                    nex->previous = pre;
                    
                    sprintf(cmd, "rm %s", file_path);
                    system(cmd);                    
                    free(temp);
                }
            }
            temp = nex;
        }
    }
    alarm(timer_limit);
}
// Synchronized Functions //
void print_list_sync()
{
    pthread_mutex_lock(&mutex);
    print_list();
    pthread_mutex_unlock(&mutex);
}

void fetch_sync(int consocket)
{
    pthread_mutex_lock(&mutex);
    fetch(consocket);
    pthread_mutex_unlock(&mutex);
}

void list_sync(int consocket)
{
    pthread_mutex_lock(&mutex);
    list(consocket);
    pthread_mutex_unlock(&mutex);
}

void remove_expired_information_sync(int signum)
{
    pthread_mutex_lock(&mutex);
    remove_expired_information(signum);
    pthread_mutex_unlock(&mutex);
}

void* process_request(void *arguments)
{
    int consocket = *((int *)arguments);     
   
    int id = rec_int(consocket);  

    switch(id)
    {
        case LIST_COMMAND_ID:
            printf("LISTING...\n");
            list_sync(consocket);
            print_list_sync();                 
            break;

        case FETCH_COMMAND_ID:
            printf("FETCHING...\n");           
            fetch_sync(consocket);
            print_list_sync();                             
            break;

        default:
            break;
     }    

    close(consocket);
    return NULL;
}

void* waiting_for_connections(void *arguments)
{    
    struct sockaddr_in dest;     // Socket info about the machine connecting to us 
    struct sockaddr_in serv;     // socket info about our server 
    int clientsocket;            // Socket used to listen for incoming connections
    socklen_t socksize = sizeof(struct sockaddr_in);

    memset(&serv, 0, sizeof(serv));             
    serv.sin_family = AF_INET;                  // Set the type of connection to TCP/IP 
    serv.sin_addr.s_addr = htonl(INADDR_ANY);   // Set our address to any interface 
    serv.sin_port = htons(port); // Set the server port number 

    clientsocket = socket(AF_INET, SOCK_STREAM, 0);

    // Server <-> Socket
    bind(clientsocket, (struct sockaddr *)&serv, sizeof(struct sockaddr));

    // Listen Connection
    listen(clientsocket, 1);
    printf("Connection Port: %d\n", port);
    
    int consocket;
    int *aux;
    pthread_t *thread;       
    while (TRUE)
    { 
        aux = (int *)calloc(1, sizeof(int));
        thread = (pthread_t *)calloc(1, sizeof(pthread_t));

        consocket = accept(clientsocket, (struct sockaddr*)&dest, &socksize);  
        *aux = consocket;     
        
      
        if (pthread_create(thread, NULL, &process_request, aux) != 0)
        {
            printf("Error Creating the Thread.\n");
        }     
    }

    return NULL;       
}

int main(int argc, char *argv[]) 
{
    // Defining the Server Port //
    if (argc != 3)
    { 
        printf("Attention: ./proxy [timer_limit] [port]\n");
        return -1; 
    }
    timer_limit = atoi(argv[1]);
    port = atoi(argv[2]);

    pthread_mutex_init(&mutex, NULL);
    pthread_t main_thread;
    hash_initialize();

    signal(SIGALRM, remove_expired_information_sync);
    alarm(timer_limit);

    if (pthread_create(&main_thread, NULL, &waiting_for_connections, NULL) != 0)
    {
        printf("Error Creating the Thread.\n");
    }

    if (pthread_join(main_thread, NULL) != 0)
    {
        printf("Error Joining the Thread.\n");
    }

    return 0;
}

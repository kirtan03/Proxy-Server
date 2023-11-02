#include <stdio.h>
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
#include <pthread.h>

#include "library.h"

#define TRUE 1
#define FALSE 0
#define MAXRCVLEN 500
#define MAX_LINE_LENGTH 10000
#define MAX_FETCH 100
#define LIST_COMMAND "list"
#define FETCH_COMMAND "fetch"
#define HISTORY_COMMAND "history"
#define QUIT_COMMAND "quit"
#define LIST_COMMAND_ID 1
#define FETCH_COMMAND_ID 2
#define HISTORY_COMMAND_ID 3
#define QUIT_COMMAND_ID 4


// -------------------- Global Variables/Structures & Functions -------------------- //
int port; // Setting the port as Global, in order to use afterward. //
pthread_mutex_t mutex;

// List With the Command and the Arguments. //
typedef struct list_word
{
   char word[MAX_LINE_LENGTH];
   struct list_word* next;

}list_word;

typedef struct list_command
{
   struct list_word* start;
   struct list_word* end;
} list_command;
list_command list;

// History of commands. //
typedef struct command_of_history
{
   char cmd[MAX_LINE_LENGTH];
   struct command_of_history* next;

}command_of_history;

typedef struct history_command
{
   struct command_of_history* start;
   struct command_of_history* end;
}history_command;
history_command history;

// List of Replies. //
typedef struct reply_word
{
   char* url;
   char* content;
   struct reply_word *next;
}reply_word;

typedef struct reply_list
{
   struct reply_word *start;
   struct reply_word *end;
}reply_list;
reply_list reply;

// General Functions //
list_command* transform_to_list(char* command);
void build_history(char* command);
int id_command();
void* file_receiver(void *arguments);
void _fetch();
void _list();
void _history();
void _quit();

// Building the List of Commands. //
list_command* transform_to_list(char* command)
{  
      list_word* current;
      char* token = strtok(command, " ");    
      while(token != NULL)
      {
         list_word* new_word = (list_word*)calloc(1, sizeof(list_word));            
         if (list.start == NULL)
         {           
            strcpy(new_word->word, token);
            new_word->next = NULL;
            list.start = new_word;           
         }
         else
         {           
            strcpy(new_word->word, token);
            new_word->next = NULL;
            current->next = new_word;              
         }
         current = new_word;
         token = strtok(NULL, " ");
      }
      list.end = current;
      list.end->next = NULL;
}

// Building the History. //
void build_history(char* command)
{  
   command_of_history* new_command = (command_of_history*)calloc(1, sizeof(command_of_history));   
   strcat(command, "\n");
   if (history.start == NULL)
   {
      strcpy(new_command->cmd, command);  
      new_command->next = NULL;
      history.start = new_command;
      history.end = new_command;
   }
   else
   {
      strcpy(new_command->cmd, command);
      new_command->next = NULL;
      history.end->next = new_command;
      history.end = new_command;            
   }
}

// Taking the command's id //
int id_command()
{
   if(strcmp(list.start->word, LIST_COMMAND) == 0)
      return LIST_COMMAND_ID;

   if(strcmp(list.start->word, FETCH_COMMAND) == 0)
      return FETCH_COMMAND_ID;

   if(strcmp(list.start->word, HISTORY_COMMAND) == 0)
      return HISTORY_COMMAND_ID;

   if(strcmp(list.start->word, QUIT_COMMAND) == 0)
      return QUIT_COMMAND_ID;

   return -1;
}

// Commands Functions //

void* file_receiver(void *arguments)
{
   char *url = (char *)arguments;

   // ---------- Connection ---------- //
   int serversocket;
   struct sockaddr_in dest;

   serversocket = socket(AF_INET, SOCK_STREAM, 0);

   memset(&dest, 0, sizeof(dest));                 // zero the struct
   dest.sin_family = AF_INET;
   dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // set destination IP number - localhost, 127.0.0.1
   dest.sin_port = htons(port);                    // set destination port number

   int connectResult = connect(serversocket, (struct sockaddr *)&dest, sizeof(struct sockaddr_in));
   if (connectResult == -1)
   {
         printf("Client Connection Error: %s\n", strerror(errno));
         return NULL;
   }
   send_int(FETCH_COMMAND_ID, serversocket);

   // Sending the Url. //
   send_string(url, serversocket);

   // If the URL is valid and the Content exist //
   char * data_dir = "./data/";
   char * file_name = rec_string(serversocket);
   char file_path[100];   
   strcpy(file_path, "");
   strcat(file_path, data_dir);
   strcat(file_path, file_name);
   FILE* content;
   if (file_name != NULL)
   {
      // Receiving the Content. Updating the content on the client's side //            
      content = fopen(file_path, "w");
      if (content == NULL)
      {
        perror("Error");
        exit(-1);
      }           
     
      char *buffer;
      while (TRUE)
      {
         buffer = rec_string(serversocket);
         if (buffer == NULL)
            break;         
         fprintf(content, "%s", buffer);
      }
      fclose(content);         
   }

   // Making the reply list in order to print //     
   reply_word *new_word = (reply_word *)calloc(1, sizeof(reply_word));
   new_word->url = (char *)calloc(1000, sizeof(char));
   new_word->content = (char *)calloc(1000, sizeof(char));
   
   if (reply.start == NULL)
   {
      strcpy(new_word->url, url);      
      if (file_name != NULL)
         strcpy(new_word->content, file_name);
      else
         strcpy(new_word->content, "~Invalid URL~");
      new_word->next = NULL; 
      reply.start = new_word;
      reply.end = reply.start;
   }
   
   else
   {           
      strcpy(new_word->url, url);
      if (file_name != NULL)
         strcpy(new_word->content, file_name);
      else
         strcpy(new_word->content, "~Invalid URL~");
      new_word->next = NULL;  
      reply.end->next = new_word;
      reply.end = new_word;            
   }  

   close(serversocket);

   return NULL;
}

void _fetch()
{  
   list_word *url = list.start;
   url = url->next;   

   pthread_t file_thread[MAX_FETCH];
   int thread_id = 0;
   while (url != NULL)
   {
      if (pthread_create(&file_thread[thread_id], NULL, &file_receiver, &url->word) != 0)
      {
         printf("Error Creating the Thread.\n");
      }
      ++thread_id;
      url = url->next;      
   }   

   for (int i = 0; i < thread_id; ++i)
   {
      if (pthread_join(file_thread[i], NULL) != 0)
      {
         printf("Error Joining the Thread\n");
      }
   }

   // Printing //
   printf("--------------- RESULTS OF FETCHING ---------------\n");
   reply_word *temp = reply.start;   
   int counter = 1;
   while (temp != NULL)
   {
      printf("(%d)\n", counter);      
      printf("URL:\n%s\n", temp->url);      
      printf("CONTENT:\n%s\n\n", temp->content);

      temp = temp->next;
      ++counter;
   }
   printf("---------------------------------------------------\n");  

   // Here, we need to open all the files on the reply list using the file_name stored in the content field (word->content)//      
   temp = reply.start;
   while (temp != NULL)
   {
      char cmd[MAX_LINE_LENGTH];
      if (strcmp(temp->content, "~Invalid URL~") != 0)
      {
         sprintf(cmd, "<PUT YOUR COMMAND HERE> %s", temp->content);
         // system(cmd);   
      }
      temp = temp->next;
   }   

   // Releasing All Memory From the Reply List //
   temp = reply.start;
   while (temp != NULL)
   {
      reply_word *aux = temp;
      temp = temp->next;
      free(aux);
   }
}

void _list()
{
   // ---------- Connection  ---------- //  
   int serversocket;
   struct sockaddr_in dest;

   serversocket = socket(AF_INET, SOCK_STREAM, 0);

   memset(&dest, 0, sizeof(dest));                 
   dest.sin_family = AF_INET;
   dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // set destination IP number - localhost, 127.0.0.1
   dest.sin_port = htons(port);                    // set destination port number

   int connectResult = connect(serversocket, (struct sockaddr *)&dest, sizeof(struct sockaddr_in));
   if (connectResult == -1)
   {
         printf("Client Connection Error: %s\n", strerror(errno));
         return;
   }

	send_int(LIST_COMMAND_ID, serversocket);

   reply_word *current = (reply_word *)calloc(1, sizeof(reply_word));
   while (TRUE)
   {
      // Receiving the Url. //
      char *url = rec_string(serversocket);
      if (url == NULL)
         break;      
      // Receiving the content's file name stored on the proxy's side. //
      char *file_name = rec_string(serversocket);

      // Making the reply list. //
      reply_word *new_word = (reply_word *)calloc(1, sizeof(reply_word));
      new_word->url = (char*)calloc(strlen(url) + 1, sizeof(char));
      new_word->content = (char*)calloc(strlen(file_name) + 1, sizeof(char));

      if (reply.start == NULL)
      {
         strcpy(new_word->url, url);
         strcpy(new_word->content, file_name);
         new_word->next = NULL; 
         reply.start = new_word;
      }

      else
      {
         strcpy(new_word->url, url);
         strcpy(new_word->content, file_name);
         new_word->next = NULL;
         current->next = new_word;
      }

      current = new_word; 
   }
   reply.end = current;
   reply.end->next = NULL;
   
   // Printing //
   printf("--------------- RESULTS OF LISTING ---------------\n");
   reply_word *temp = reply.start;
   int counter = 1;
   while (temp != NULL)
   {         
      printf("(%d)\n", counter);
      printf("URL:\n%s\n", temp->url);
      printf("CONTENT:\n%s\n\n", temp->content);

      temp = temp->next;
      ++counter;
   }
   printf("--------------------------------------------------\n");


   // Releasing All Memory From the Reply List //
   temp = reply.start;
   while (temp != NULL)
   {
      reply_word *aux = temp;
      temp = temp->next;
      free(aux);
   }   

   close(serversocket);
}


void _history()
{
   command_of_history* temp = history.start;
   printf("---------- HISTORY ----------\n");
   while (temp != NULL)
   {
      printf("%s", temp->cmd);
      temp = temp->next;
   }
   printf("-----------------------------\n");
}

void _quit()
{
   printf("Bye! :)\n");
   exit(0);
}

int main(int argc, char* argv[])
{  
   // Defining the Server Port //
   if (argc != 2)
   { 
        printf("Just Give the Server Port Number.\n");
        return -1; 
   }
   port = atoi(argv[1]);

   // Program //
   char command[MAX_LINE_LENGTH];
   char command_aux[MAX_LINE_LENGTH];
   history.start = NULL;
   history.end = NULL;
   while (TRUE)
   {
      list.start = NULL;
      list.end = NULL;
      reply.start = NULL;
      reply.end = NULL;

      scanf(" %[^\n]", command);
      strcpy(command_aux, command);
      transform_to_list(command_aux);                  
   
      int id = id_command();
      switch(id)
      {
         case LIST_COMMAND_ID:
            build_history(command);
            _list();          
            break;

         case FETCH_COMMAND_ID:     
            build_history(command);
            _fetch();                              
            break;

         case HISTORY_COMMAND_ID:            
            build_history(command);
            _history();          
            break;

         case QUIT_COMMAND_ID:            
            _quit();
            break;

         default:
            printf("~Invalid Command~\n");
      }
   }

    return 0;
}
/* Group 55
Jeffrey Chien
chienj5

Arkar Chan
arkarc
*/


#include "server.h"

// global variable
char buffer[BUFFER_SIZE];
static pthread_mutex_t buffer_lock = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t auc_lock = PTHREAD_MUTEX_INITIALIZER;
int listen_fd;

// data strcutures
List_t *user_info = NULL;       // user packet
List_t* auction_list = NULL;    // Auctions
int auctionID = 1;              // AuctionIDs
sbuf_t* jobqueue = NULL;        // Job Queue

void sigint_handler(int sig)
{
    printf("shutting down server\n");
    close(listen_fd);
    exit(0);
}

int server_init(int server_port)
{
    int sockfd;
    struct sockaddr_in servaddr;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) 
    {
        printf("socket creation failed...\n");
        exit(EXIT_FAILURE);
    }
    else
        printf("Socket successfully created\n");

    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(server_port);

    int opt = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (char *)&opt, sizeof(opt))<0)
    {
        perror("setsockopt");exit(EXIT_FAILURE);
    }

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) 
    {
        printf("socket bind failed\n");
        exit(EXIT_FAILURE);
    }
    else
        printf("Socket successfully binded\n");

    // Now server is ready to listen and verification
    if ((listen(sockfd, 1)) != 0) 
    {
        printf("Listen failed\n");
        exit(EXIT_FAILURE);
    }
    else
        printf("Server listening on port: %d.. Waiting for connection\n", server_port);

    return sockfd;
}


void *client_thread(void* clientfd_ptr)
{
    int client_fd = *(int *)clientfd_ptr;
    //free(clientfd_ptr);
    int received_size;
    fd_set read_fds;

    petr_header *msg = malloc(sizeof(petr_header));
    job_t *job = malloc(sizeof(job_t));

    job->msg_type = 0;
    job->msg_len = 0;
    job->client_fd = 0;

    for (int i = 0; i < 30; i++)
    {
        job->msg_body[i] = 0;
    }
    

    char client_msg[50];

    int retval = 0;
    
    while(1)
    {

        //pthread_mutex_lock(&buffer_lock); 

        bzero(buffer, BUFFER_SIZE);
 
        //receive message header from client
        //0 = success
        if (rd_msgheader(client_fd, msg) == 0)
        {
            // FOR DEBUG
            // printf("msg_type = %d\n", msg->msg_type);
            // printf("msg_len = %i\n", msg->msg_len);

            //receive the user info and store into user_msg
            //then identify username and password from the user info and store into a struct
            received_size = read(client_fd, client_msg, msg->msg_len);


            if(received_size < 0)
            {
                printf("Receiving failed\n");
                break;
            }
            // else if(received_size == 0)
            // {
            //     continue;
            // }

            job->msg_type = msg->msg_type;
            job->msg_len = msg->msg_len;
            job->client_fd = client_fd;
            // printf("job->msg_type = %i\n", job->msg_type);
            // printf("job->msg_len = %i\n", job->msg_len);

            

            
        }
        if ((msg->msg_type == ANCREATE) || (msg->msg_type == ANWATCH) || (msg->msg_type == ANBID))
        {
            // will only print messages for createauction and watchauction
            //printf("messages: %s\n", client_msg);
            strcpy(job->msg_body, client_msg);
            // printf("job->msg_body: %s\n", job->msg_body);

        }
        if ((msg->msg_type == LOGOUT))
        {
            node_t* user_search;
            user_search = user_info->head;
            if (user_search == NULL)
            {
                printf("The list is empty\n");
            }

            while (user_search != NULL)
            {
                if (((packet_user_info*)user_search->value)->client_fd == client_fd)
                {
                    ((packet_user_info*)user_search->value)->login_status = 0;
                }
                user_search = user_search->next;
            }

            msg->msg_type = OK;
            msg->msg_len = 0;

            // write OK protocol
            // 0 = success
            wr_msg(client_fd, msg, client_msg);
        }
        else
        {
            //store the msg into the job queue
            sbuf_insert(jobqueue, *job);
        }
        
        
        
        

        //pthread_mutex_unlock(&buffer_lock);
  
    }
    // Close the socket at the end
    printf("Close current client connection\n");
    close(client_fd);

    return 0;
}


// the main thread
// it's the main function that will run and start the server
// it will read the message passed in from the client
// and store it into a struct with username and password.
void main_thread(int server_port)
{
    listen_fd = server_init(server_port); // Initiate server and start listening on specified port
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    //user_info = (List_t*)malloc(sizeof(List_t));
    //List_t *user_info = (List_t*)malloc(sizeof(List_t));
    user_info = (List_t*)malloc(sizeof(List_t));
    user_info->head = NULL;
    user_info->length = 0;
    
    // auction_list initialization

    // job queue initialization
    sbuf_init(jobqueue,10);

    // job threads
    pthread_t tid_job;
    pthread_create(&tid_job, NULL, job_thread, NULL); 

    while(1)
    {
        // Wait and Accept the connection from client
        printf("Wait for new client connection\n");
        int* client_fd = malloc(sizeof(int));
        *client_fd = accept(listen_fd, (SA*)&client_addr, &client_addr_len);
        if (*client_fd < 0) 
        {
            printf("server acccept failed\n");
            exit(EXIT_FAILURE);
        }
        else
        {
            printf("Client connetion accepted\n");

            // when client connected
            int temp_client_fd = *client_fd;
            free(client_fd);
            int received_size;
            fd_set read_fds;

            petr_header *msg = malloc(sizeof(petr_header));
            msg->msg_type = 0;
            msg->msg_len = 0;

            int is_password = 0;
            char user_msg[50];
            int j = 0;

            //struct packet_user_info *user_packet = malloc(sizeof(user_packet));

            int retval;
            // while(1)
            // {
                FD_ZERO(&read_fds);
                FD_SET(temp_client_fd, &read_fds);
                retval = select(temp_client_fd + 1, &read_fds, NULL, NULL, NULL);
                if (retval!=1 && !FD_ISSET(temp_client_fd, &read_fds))
                {
                    printf("Error with select() function\n");
                    break;
                }

                bzero(buffer, BUFFER_SIZE);

                // receive first message from client
                // 0 = success
                if (rd_msgheader(temp_client_fd, msg) == 0)
                {
                    // FOR DEBUG
                    // printf("msg_type = %d\n", msg->msg_type);
                    // printf("msg_len = %i\n", msg->msg_len);

                    // receive the user info and store into user_msg
                    // then identify username and password from the user info and store into a struct
                    received_size = read(temp_client_fd, user_msg, msg->msg_len);
                    if(received_size < 0)
                    {
                        printf("Receiving failed\n");
                        break;
                    }
                    else if(received_size == 0)
                    {
                        continue;
                    }
            
                    //printf("user info: %s\n", user_msg);
                }


                //printf("msg: %s\n", user_msg);

                int username_size = 0;
                int password_size = 0;

                // determine size for username and password
                for (int i = 0; i < strlen(user_msg); i++)
                {
                    if (user_msg[i] != 10 && is_password == 0)
                    {
                        username_size++;
                    }
                    else if (user_msg[i] == 10 && is_password == 0)
                    {
                        is_password = 1;
                    }
                    else if (is_password == 1)
                    {
                        password_size++;
                        j++;
                    }
                    else if (user_msg[i] == 10 && is_password == 1)
                    {
                        // break out of loop
                        i = i + 20;
                    }
                }
                is_password = 0;
                j = 0;
                password_size++;

                // printf("size of username: %i\n", username_size);
                // printf("size of password: %i\n", password_size);

                packet_user_info *user_packet = malloc(sizeof(packet_user_info));
                // user_packet->username = (char*) malloc(sizeof(char) * username_size);
                // user_packet->password = (char*) malloc(sizeof(char) * password_size);
                user_packet->login_status = 0;
                user_packet->client_fd = 0;

                char temp_username[20];
                char temp_password[20];

                for (int i = 0; i < 20; i++)
                {
                    user_packet->username[i] = 0;
                    user_packet->password[i] = 0;
                    temp_username[i] = 0;
                    temp_password[i] = 0;
                }

                // // create a user_packet for storing username and password
                // struct packet_user_info *user_packet = malloc(sizeof(user_packet));
                
                // store the username and password from msg into a user_info struct
                for (int i = 0; i < strlen(user_msg); i++)
                {
                    if (user_msg[i] != 10 && is_password == 0)
                    {
                        temp_username[i] = user_msg[i];
                    }
                    else if (user_msg[i] == 10 && is_password == 0)
                    {
                        //user_packet->username[i] = '\0';
                        is_password = 1;
                    }
                    else if (user_msg[i] != 10 && is_password == 1)
                    {
                        temp_password[j] = user_msg[i];
                        j++;
                    }
                    else if (user_msg[i] == 10 && is_password == 1)
                    {
                        //user_packet->password[j] = '\0';
                        // break out of loop
                        i = i + 20;
                    }

                }

                strcpy(user_packet->username, temp_username);
                strcpy(user_packet->password, temp_password);

                // printf("username: %s\n", user_packet->username);
                // printf("password: %s\n", user_packet->password);

                //free(user_packet);


                // check if user_info is empty
                // empty means no user registered yet
                node_t *temp = malloc(sizeof(node_t));

                temp->value = NULL;
                temp->next = NULL;


                int unique_username = 0;

                pthread_t tid;

                temp = user_info->head;
                if (temp == NULL)
                {
                    // login status = 1
                    user_packet->login_status = 1;
                    // store the client fd
                    user_packet->client_fd = temp_client_fd;
                    // store the user info into the linkedlist
                    insertFront(user_info, user_packet);


                    // first user stored in the server
                    // responds with OK

                    // OK protocol
                    msg->msg_type = 0x00;
                    msg->msg_len = 0;

                    //printf("size of list: %i\n", user_info->length);

                    // write OK protocol
                    // 0 = success
                    if (wr_msg(temp_client_fd, msg, user_msg) == 0)
                    {

                        // if (ret < 0){
                        // printf("Sending failed\n");
                        // break;
                        // }
                        // login success
                        // spawn client thread
                        pthread_create(&tid, NULL, client_thread, (void *)&temp_client_fd);

                    }
                }
                else
                {
                    // when the list is not empty
                    // check for username and password
                    node_t* username_search = malloc(sizeof(node_t));

                    username_search->value = NULL;
                    username_search->next = NULL;

                    username_search = user_info->head;

                    //printf("size of list: %i\n", user_info->length);

                    while (username_search != NULL)
                    {
                        // if username and password both match
                        if ((strcmp((user_packet->username), ((packet_user_info*)username_search->value)->username) == 0) 
                        && (strcmp((user_packet->password), ((packet_user_info*)username_search->value)->password) == 0))
                        {
                            // check to see if the user is logged in
                            if (((packet_user_info*)username_search->value)->login_status == 1)
                            {
                                // user already logged in
                                // user currently online
                                msg->msg_type = 0x1A;
                                msg->msg_len = 0;

                                // user logged in protocol
                                if (wr_msg(temp_client_fd, msg, user_msg) == 0)
                                {
                                    // login failed
                                }
                            }
                            else
                            {
                                // account match
                                // not logged in
                                // responds with OK
                                // OK protocol
                                msg->msg_type = 0x00;
                                msg->msg_len = 0;

                                // write OK protocol
                                // 0 = success
                                if (wr_msg(temp_client_fd, msg, user_msg) == 0)
                                {
                                    // login success
                                    // spawn client thread
                                    pthread_create(&tid, NULL, client_thread, (void *)&temp_client_fd);
                                }
                            }


                        }
                        // if username match and password doesn't match
                        else if ((strcmp((user_packet->username), ((packet_user_info*)username_search->value)->username) == 0) 
                            && (strcmp((user_packet->password), ((packet_user_info*)username_search->value)->password) != 0))
                        {
                            // check to see if the user is logged in
                            if (((packet_user_info*)username_search->value)->login_status == 1)
                            {
                                // user already logged in
                                // user currently online
                                msg->msg_type = 0x1A;
                                msg->msg_len = 0;

                                // user logged in protocol
                                if (wr_msg(temp_client_fd, msg, user_msg) == 0)
                                {
                                    // login failed
                                }
                            }
                            else
                            {
                                // wrong password
                                msg->msg_type = 0x1B;
                                msg->msg_len = 0;

                                // write OK protocol
                                // 0 = success
                                if (wr_msg(temp_client_fd, msg, user_msg) == 0)
                                {
                                    // login failed. Wrong password

                                }
                            }


                        }
                        // if username doesn't match
                        else if (strcmp((user_packet->username), ((packet_user_info*)username_search->value)->username) != 0)
                        {
                            unique_username++;
                        }

                        username_search = username_search->next;
                    }
                    free(username_search);

                    //create new user info
                    if (unique_username == user_info->length)
                    {
                        // login status = 1
                        user_packet->login_status = 1;
                        // store the client fd
                        user_packet->client_fd = temp_client_fd;
                        // store the user info into the linkedlist
                        insertFront(user_info, user_packet);

                        // new user responds with OK
                        // OK protocol
                        msg->msg_type = 0x00;
                        msg->msg_len = 0;

                        // write OK protocol
                        // 0 = success
                        if (wr_msg(temp_client_fd, msg, user_msg) == 0)
                        {
                            // login success
                            // spawn client thread
                            pthread_create(&tid, NULL, client_thread, (void *)&temp_client_fd); 

                        }
                    }
                    

                }
                //free(temp);

                // printf("the head->username: %s\n", ((struct packet_user_info*)user_info->head->value)->username);
                // printf("the head->password: %s\n", ((struct packet_user_info*)user_info->head->value)->password);

                // // FOR DEBUG
                // // print all the username stored in linked list
                // node_t* node_for_printing;

                // node_for_printing = user_info->head;
                // if (node_for_printing == NULL)
                // {
                //     printf("The list is empty\n");
                // }
                // printf("\n");
                // while (node_for_printing != NULL)
                // {
                    
                //     printf("%s\n", ((packet_user_info*)node_for_printing->value)->username);
                    
                //     node_for_printing = node_for_printing->next;
                // }
                // printf("\n");

        }
    }
    bzero(buffer, BUFFER_SIZE);
    close(listen_fd);
    return;
}


// the main function for getting the arguments entered by the user to the server
int main(int argc, char* argv[]) 
{
    int opt;
    int port = 0;
    FILE* file = NULL;
    char* filename = NULL;
    int numJob = 2;
    int numTick = -999;
    char* tick;

    // INTIALIZATION
    auction_list = (List_t*)malloc(sizeof(List_t));
    auction_list->head = NULL;
    auction_list->length = 0;
    
    while((opt = getopt(argc, argv, "hj:t:")) != -1)
    {
        switch (opt)
        {
        case 'h':
            // display help menu and return EXIT_SUCCESS
            printf("./bin/zbid_server [-h] [-j N] [-t M] PORT_NUMBER AUCTION_FILENAME\n");
            printf("-h                  Display this help menu, and returns EXIT_SUCCESS;\n");
            printf("-j N                Number of job threads. If option not specified, default to 2.\n");
            printf("-t M                M seconds between time ticks. If option not specified, default is to wait on\n");
            printf("                    input from stdin to indicate a tick.\n");
            printf("PORT_NUMBER         Port number to listen on.\n");
            printf("ACUTION_FILENAME    File to read auction item information from at the start of the server.\n");
            return EXIT_SUCCESS;
        case 'j':
            numJob = atoi(optarg);
            break;
        case 't':
            numTick = atoi(optarg);
            break;
        
        default:
            exit(EXIT_FAILURE);
        }
    }

    // if (numTick == -999)
    // {
    //     printf("Please indicate tick intervals: ");
    //     numTick = fgetc(stdin);
    // }
    port = atoi(argv[argc-2]);
    filename = argv[argc-1];
    jobqueue = malloc(sizeof(sbuf_t));

    /* read auctions from file*/
    file = fopen(filename, "r");
    int turn = 0;
    char string[20];
    auction_t* auction_item = malloc(sizeof(auction_t));
    auction_t* ptr_item = auction_item;
    ptr_item->item_name = malloc(sizeof(20));
    while(fgets(string, 20, file) != NULL)
    {
        if (turn == 0)
        {
            ptr_item->id = auctionID;
            auctionID++;
            ptr_item->highest_bid = 0;
            ptr_item->num_watcher = 0;
            strncpy(ptr_item->item_name, string, strlen(string)-2);
            printf("item name: %s\n", ptr_item->item_name); 
            turn++;
        }
        else if (turn == 1)
        {
            ptr_item->duration = atoi(string);
            printf("duration: %d\n", ptr_item->duration);
            turn++;
        }
        else if (turn == 2)
        {
            ptr_item->max_bid = atoi(string);
            printf("max bid price: %d\n", ptr_item->max_bid);
            turn++;
            insertRear(auction_list, ptr_item);
        }
        else
        {
            ptr_item = malloc(sizeof(auction_t));
            ptr_item->item_name = malloc(sizeof(20));
            turn = 0;
        }
    }
    fclose(file); 
    

    // printf("number of job threads: %d\n", numJob);
    // printf("number of ticks: %d\n", numTick);

    // if (port == 0){
    //     fprintf(stderr, "ERROR: Port number for server to listen is not given\n");
    //     fprintf(stderr, "Server Application Usage: %s -p <port_number>\n",
    //                 argv[0]);
    //     exit(EXIT_FAILURE);
    // }
    
    main_thread(port);

    return 0;
}

/* Create an empty, bounded, shared FIFO buffer with n slots */
void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = calloc(n, sizeof(job_t)); 
    sp->n = n;                       /* Buffer holds max of n items */
    sp->front = sp->rear = 0;        /* Empty buffer iff front == rear */
    sem_init(&sp->mutex, 0, 1);      /* Binary semaphore for locking */
    sem_init(&sp->slots, 0, n);      /* Initially, buf has n empty slots */
    sem_init(&sp->items, 0, 0);      /* Initially, buf has zero data items */
}

/* Clean up buffer sp */
void sbuf_deinit(sbuf_t *sp)
{
    free(sp->buf);
}

/* Insert item onto the rear of shared buffer sp */
void sbuf_insert(sbuf_t *sp, job_t item)
{
    sem_wait(&sp->slots);                          /* Wait for available slot */
    sem_wait(&sp->mutex);                          /* Lock the buffer */
    sp->buf[(++sp->rear)%(sp->n)] = item;          /* Insert the item */
    sem_post(&sp->mutex);                          /* Unlock the buffer */
    sem_post(&sp->items);                          /* Announce available item */
}

/* Remove and return the first item from buffer sp */
job_t sbuf_remove(sbuf_t *sp)
{
    job_t item;
    sem_wait(&sp->items);                          /* Wait for available item */
    sem_wait(&sp->mutex);                          /* Lock the buffer */
    item = sp->buf[(++sp->front)%(sp->n)];         /* Remove the item */
    sem_post(&sp->mutex);                          /* Unlock the buffer */
    sem_post(&sp->slots);                          /* Announce available slot */
    return item;
}

void* job_thread()
{

    // main thread will creates N thread to handle job request from client threads
    // what kinds of jobs are there?
    // ANCREATE - create an auction with <item_name>, <duration>, and <max_bid_price>
    // ANLIST - give a list of currently running auctions, ordered by <auction_id>
    // ANWATCH - watch a currently running aunction
    // ANLEAVE - stop watching a running auction
    // ANBID - bid on an auction using <bid> and <auction_id>
    // ...

    while(1)
    {
        job_t item = sbuf_remove(jobqueue);
        int jobtype = item.msg_type;
        
        // printf("job type: %x\n", item.msg_type);
        //printf("job body: %s\n", item.msg_body);

        if (jobtype == ANCREATE)
        {
        
            char* item_name = malloc(20);
            char duration_string[2];
            char max_bid_string[10];
            char* current_char = item.msg_body;
            
            // printf("MESSAGE BODY: %s\n", item.msg_body);
            int i = 0; 
            while (current_char[i] != '\r' && current_char[i+1] != '\n') 
            {
                item_name[i] = current_char[i];
                // strncpy(item_name, current_char, i+1);
                ++i;
            }
            i = i + 2;          // skip '\r' and '\n'
            int j = 0;
            while (current_char[i] != '\r' && current_char[i+1] != '\n')
            {
                duration_string[j] = current_char[i];
                ++j;            // increment to move forward in duration
                ++i;            // increment to move forward in message body
            }
            i = i + 2;          // skip '\r' and '\n'
            j = 0;
            while(current_char[i] != '\0')
            {
                max_bid_string[j] = current_char[i];
                ++j;
                ++i;
            }
            printf("item name: %s\n", item_name);
            int duration = atoi(duration_string);
            printf("duration: %d\n", duration);
            int max_bid = atoi(max_bid_string);
            printf("max_bid: %d\n", max_bid);
            // invalid argument case
            if (duration < 1 || max_bid < 0 || (strcmp(item_name, "") == 0))
            {
                petr_header* message = malloc(sizeof(petr_header));
                message->msg_type = EINVALIDARG;
                message->msg_len = 0;
                wr_msg(item.client_fd,message,NULL);
                continue;
            }

            auction_t* auction = malloc(sizeof(auction_t));
            auction->id = auctionID;
            auction->item_name = item_name;
            auction->highest_bid_user = NULL;
            // strcpy(auction->item_name,item_name);
            auction->duration = duration;
            auction->max_bid = max_bid;
            auction->highest_bid = 0;
            auction->num_watcher = 0;

            // storing creator
            node_t* search_fd;
            search_fd = user_info->head;
            if (search_fd == NULL)
            {
                printf("The list is empty\n");
            }
            while (search_fd != NULL)
            {
                if (((packet_user_info*)search_fd->value)->client_fd == item.client_fd)
                {
                    auction->creator = ((packet_user_info*)search_fd->value)->username;
                }
                search_fd = search_fd->next;
            }

            insertRear(auction_list, (void*)auction);
            pthread_mutex_lock(&auc_lock);
            auctionID++;
            pthread_mutex_unlock(&auc_lock);

            petr_header* message = malloc(sizeof(petr_header));
            message->msg_type = ANCREATE;
            message->msg_len = 2;
            char buf[2];
            sprintf(buf, "%d", auction->id);
            wr_msg(item.client_fd, message, buf);
        }
        else if (jobtype == ANLIST)
        {
            int client_fd = item.client_fd;

            petr_header* msg_header = malloc(sizeof(petr_header*));
            msg_header->msg_type = ANLIST;
            msg_header->msg_len = 0;
            char* message = malloc(100);
            //char* ptr_message = message;

            node_t* current = auction_list->head;
            while(current != NULL)
            {
                char its[2];
                auction_t* temp = ((auction_t*)current->value);
                sprintf(its, "%d", temp->id);
                strcat(message, its);
                strcat(message, ";");
                strcat(message, temp->item_name);
                strcat(message, ";");
                sprintf(its, "%d", temp->max_bid);
                strcat(message, its);
                strcat(message, ";");
                sprintf(its, "%d", temp->num_watcher);
                strcat(message, its);
                strcat(message, ";");
                sprintf(its, "%d", temp->highest_bid);
                strcat(message, its);
                strcat(message, ";");
                sprintf(its, "%d", temp->duration);
                strcat(message, its);
                strcat(message, "\n");
                current = current->next;
            }
            // strcat(message, '\0');
            msg_header->msg_len = strlen(message) + 1;
            
            wr_msg(client_fd, msg_header, message);
        }
        else if (jobtype == ANWATCH)
        {
            printf("In anwatch\n");
            int temp_client_fd = item.client_fd;
            // client will enter the auction id to watch
            // get the id from the client
            printf("client msg in anwatch: %s\n", item.msg_body);
            int auction_id = atoi(item.msg_body);
            int item_name_size = 0;
            int total_id_not_found = 0;
            petr_header* temp = malloc(sizeof(petr_header));
            temp->msg_type = 0;
            temp->msg_len = 0;

            //printf("auction id: %i\n", auction_id);

            node_t* search_id;
            search_id = auction_list->head;
            if (search_id == NULL)
            {
                printf("The list is empty\n");
            }
            while (search_id != NULL)
            {
                // if auction id is found
                if (((auction_t*)search_id->value)->id == auction_id)
                {   
                    // if auction reached maximum of 5 users, responds with EANFULL
                    if ((((auction_t*)search_id->value)->num_watcher < 5))
                    {
                        char* message = malloc(30);
                        char price[10];
                        

                        strcat(message, ((auction_t*)search_id->value)->item_name);
                        strcat(message, "\r");
                        strcat(message, "\n");
                        sprintf(price, "%d", (((auction_t*)search_id->value)->max_bid));
                        strcat(message, price);
                        strcat(message, "\0");

                        // increase user watching
                        ((auction_t*)search_id->value)->num_watcher++;
                        // writing back to client
                        temp->msg_type = ANWATCH;
                        temp->msg_len = strlen(message) + 1;

                        wr_msg(temp_client_fd, temp, message);
                    }
                    else
                    {
                        // writing back to client
                        temp->msg_type = EANFULL;
                        temp->msg_len = 0;
                        char buffer[1];

                        wr_msg(temp_client_fd, temp, buffer);
                    }
                    
                }
                else
                {
                    total_id_not_found++;
                }

                search_id = search_id->next;
                
            }
            // if auction id doesn't exist, responds with EANNOTFOUND
            if (total_id_not_found == auction_list->length)
            {
                temp->msg_type = EANNOTFOUND;
                temp->msg_len = 0;
                char buffer[1];

                wr_msg(temp_client_fd, temp, buffer);
            }
 

        }
        else if (jobtype == ANLEAVE)
        {
            int temp_client_fd = item.client_fd;
            // client will enter the auction id to watch
            // get the id from the client
            int auction_id = atoi(item.msg_body);
            int total_id_not_found = 0;
            petr_header* temp = malloc(sizeof(petr_header));
            temp->msg_type = 0;
            temp->msg_len = 0;
            char buffer[1];

            //printf("auction id: %i\n", auction_id);

            node_t* search_id;
            search_id = auction_list->head;
            if (search_id == NULL)
            {
                printf("The list is empty\n");
            }
            while (search_id != NULL)
            {
                // if auction id is found
                if (((auction_t*)search_id->value)->id == auction_id)
                {   
                    // decrease user watching
                    ((auction_t*)search_id->value)->num_watcher--;
                    // writing back to client
                    temp->msg_type = OK;
                    temp->msg_len = 0;

                    wr_msg(temp_client_fd, temp, buffer);
                }
                else
                {
                    total_id_not_found++;
                }

                search_id = search_id->next;
                
            }
            // if auction id doesn't exist, responds with EANNOTFOUND
            if (total_id_not_found == auction_list->length)
            {
                temp->msg_type = EANNOTFOUND;
                temp->msg_len = 0;
                char buffer[1];

                wr_msg(temp_client_fd, temp, buffer);
            }



        }
        else if (jobtype == ANBID)
        {
            printf("In anbid\n");
            int temp_client_fd = item.client_fd;
            petr_header* temp = malloc(sizeof(petr_header));
            temp->msg_type = 0;
            temp->msg_len = 0;

            printf("client msg in anbid: %s\n", item.msg_body);
            // make a bid in auction with auction id
            int total_id_not_found = 0;
            

            char buffer[10];
            char buffer2[10];


            int is_bid_price = 0;
            int counter = 0;

            // get the bid price from the client message
            while (item.msg_body[counter] != '\r')
            {
                buffer2[counter] = item.msg_body[counter];
                counter++;
            }
            buffer2[counter] = '\0';
            counter++;
    
            int i = 0;
            while (item.msg_body[counter] != '\0')
            {
                buffer[i] = item.msg_body[counter];
                // printf("buffer2[%i] = %c\n", i, item.msg_body[counter]);
                i++;
                counter++;
            }
            buffer[i] = '\0';

            int bid_price = atoi(buffer);
            int auction_id = atoi(buffer2);

            // // get the bid price from the client message
            // for (int i = 0; i < strlen(item.msg_body); i++)
            // {
            //     if (item.msg_body[i] != '\r' && is_bid_price == 0)
            //     {
            //         buffer2[i] = item.msg_body[i];
            //     }
            //     else if (item.msg_body[i] == '\n')
            //     {
            //         is_bid_price = 1;
            //     }
            //     else if (is_bid_price == 1)
            //     {
            //         buffer[counter] = item.msg_body[i];
            //         counter++;
            //     }
            // }

            // int bid_price = atoi(buffer);
            // int auction_id = atoi(buffer2);



            printf("bid price: %i\n", bid_price);
            printf("auction_id: %i\n", auction_id);

            node_t* search_id;
            search_id = auction_list->head;
            if (search_id == NULL)
            {
                printf("The list is empty\n");
            }

            while (search_id != NULL)
            {
                // if auction id is found
                if (((auction_t*)search_id->value)->id == auction_id)
                {   
                    // check for creator of the auction
                    if (((packet_user_info*)search_id->value)->client_fd == item.client_fd)
                    {
                        // if auction id exist, but the user created the auction, responds with EADENIED
                        if (((auction_t*)search_id->value)->creator == ((packet_user_info*)search_id->value)->username)
                        {
                            // responds with EADENIED
                            // writing back to client
                            temp->msg_type = EANDENIED;
                            temp->msg_len = 0;

                            wr_msg(temp_client_fd, temp, buffer);
                        }
                    }
                    else
                    {
                        // bid allow
                        // if bid price is lower than current highesh bid, responds with EBIDLOW
                        if (bid_price < (((auction_t*)search_id->value)->highest_bid))
                        {
                            // responds with EBIDLOW
                            // writing back to client
                            temp->msg_type = EBIDLOW;
                            temp->msg_len = 0;

                            wr_msg(temp_client_fd, temp, buffer);
                        }
                        else
                        {
                            // bid success
                            // update the highest bid user
                            ((auction_t*)search_id->value)->highest_bid = bid_price;
                            ((auction_t*)search_id->value)->highest_bid_user = ((packet_user_info*)search_id->value)->username;

                            
                            // writing back to client
                            temp->msg_type = OK;
                            temp->msg_len = 0;

                            wr_msg(temp_client_fd, temp, buffer);

                            petr_header* temp2 = malloc(sizeof(petr_header*));
                            char buffer2[] = "1\r\nbroken statue\r\nniteowl2\r\n150\x00";
                            temp2->msg_type = ANUPDATE;
                            temp2->msg_len = 32;

                            wr_msg(temp_client_fd, temp2, buffer2);

                        }
                        
                    }
                    
                }
                else
                {
                    total_id_not_found++;
                }

                search_id = search_id->next;
            }
            // if auction id doesn't exist, responds with EANNOTFOUND
            if (total_id_not_found == auction_list->length)
            {
                temp->msg_type = EANNOTFOUND;
                temp->msg_len = 0;

                printf("here\n");

                wr_msg(temp_client_fd, temp, buffer);
            }
        }
        // else if (jobtype == ANUPDATE)
        // {
        //     int temp_client_fd = item.client_fd;
        //     petr_header* temp = malloc(sizeof(petr_header));
        //     temp->msg_type = ANUPDATE;
        //     temp->msg_len = 0;
        //     char msg[1];


        //     wr_msg(temp_client_fd, temp, msg);

        // }
        else if (jobtype == USRLIST)
        {
            int temp_client_fd = item.client_fd;

            petr_header* temp = malloc(sizeof(petr_header));
            temp->msg_type = USRLIST;
            temp->msg_len = 0;
            int msg_size = 0;

            // counting the length of msg
            node_t* msg_len_counter;
            msg_len_counter = user_info->head;
            if (msg_len_counter == NULL)
            {
                printf("The list is empty\n");
            }

            while (msg_len_counter != NULL)
            {
                if ((((packet_user_info*)msg_len_counter->value)->login_status == 1) 
                && (((packet_user_info*)msg_len_counter->value)->client_fd != temp_client_fd) )
                {
                    for (int i = 0; i < strlen(((packet_user_info*)msg_len_counter->value)->username); i++)
                    {
                    
                        msg_size++;

                    }
                }
                
                msg_len_counter = msg_len_counter->next;
            }
            msg_size++;
            temp->msg_len = msg_size;

            // printf("total username: %i\n", total_username);
            // printf("msg length: %i\n", msg_size);
            // storing all the usernames into the buffer;
            char msg[temp->msg_len];
            for (int i = 0; i < temp->msg_len; i++)
            {
                msg[i] = 0;
            }
            node_t* store_msg;
            store_msg = user_info->head;
            int counter = 0;

            while (store_msg != NULL)
            {
                if ((((packet_user_info*)store_msg->value)->login_status == 1) 
                && (((packet_user_info*)store_msg->value)->client_fd != temp_client_fd))
                {
                    for (int i = 0; i < strlen(((packet_user_info*)store_msg->value)->username); i++)
                    {
                        if (((packet_user_info*)store_msg->value)->username[i] 
                        == ((packet_user_info*)store_msg->value)->username[strlen(((packet_user_info*)store_msg->value)->username) - 1])
                        {
                            msg[counter] = '\n';
                        }
                        else
                        {
                            msg[counter] = ((packet_user_info*)store_msg->value)->username[i];
                        }
                        counter++;
                    }
                }

                store_msg = store_msg->next;
            }
            msg[counter] = 0;

            wr_msg(temp_client_fd, temp, msg);
            
        }
    }
    return NULL;
}
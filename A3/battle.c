/*
 * CSC209: Software Tools and Systems Programming
 * A3: Sockets and Communication - Multiplayer Game
 * 
 * Group Members:
 * Dhvani Patel, Hia Aggrawal, Kanupreet Arora
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

#define PORT 51687
#define MAX_BUF 256
#define MAX_NAME 64

// Structure to store client information
struct client {
    int fd; // File descriptor
    struct in_addr ipaddr; // IP address
    struct client *next; // Pointer to the next client in the list
    struct client *last_battled;
    char name[MAX_NAME];
    int hitpoints;
    int powermoves;
    int inmatch;
    int turn;
    int win_streak;
};

// Function prototypes
int handleclient(struct client *p, struct client *top);
static struct client *addclient(struct client *top, int fd, struct in_addr addr);
int setupclient(struct client *p, struct client *top);
struct client *find_match(struct client *player, struct client *top);
void engageincombat(struct client *player1, struct client *player2);
int check_combat_done (struct client *player1, struct client *player2);
void saywait(struct client *p);
void displayactions(struct client *player1);
void displaystats(struct client *player1, struct client *player2);
int taketurn(char *action, struct client *player1, struct client *player2);
void move(struct client **top, struct client *p);
int bindandlisten(void);
static struct client *removeclient(struct client *top, int fd);
static void broadcast(struct client *top, char *s, int size);

// GLOBAL VARIABLE
struct client *head = NULL;

int main(void) {
    int clientfd, maxfd, nready;
    struct client *p;
    socklen_t len;
    struct sockaddr_in q;
    fd_set allset;
    fd_set rset;
    int i;

    int listenfd = bindandlisten(); // Create and bind a socket for listening
    FD_ZERO(&allset); 
    FD_SET(listenfd, &allset); 
    maxfd = listenfd;

    while (1) {
        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (nready == 0) {
            continue;
        }
        if (nready == -1) {
            perror("select");
            continue;
        }
        if (FD_ISSET(listenfd, &rset)) { // Check if a new client is connecting
            printf("a new client is connecting\n");
            len = sizeof(q);
            if ((clientfd = accept(listenfd, (struct sockaddr *)&q, &len)) < 0) {
                perror("accept");
                exit(1);
            }
            FD_SET(clientfd, &allset); // Add the new client to the set
            if (clientfd > maxfd) {
                maxfd = clientfd; // Update the maximum file descriptor
            }
            printf("connection from %s\n", inet_ntoa(q.sin_addr));
            head = addclient(head, clientfd, q.sin_addr); // Add the new client to the client list
        }
        for(i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &rset)) { // Check if there is activity on a client socket
                for (p = head; p != NULL; p = p->next) {
                    if (p->fd == i) {
                        int result = handleclient(p, head); // Handle client activity
                        if (result == -1) {
                            int tmp_fd = p->fd;
                            head = removeclient(head, p->fd); // Remove the client from the list
                            FD_CLR(tmp_fd, &allset); // Clear the file descriptor from the set
                            close(tmp_fd); // Close the socket
                        }
                        break;
                    }
                }
            }
        }
    }
    return 0;
}

/* function to handles client activity 
 * reads user input input if it is their turn to make a move
* ignores user input otherwise */ 
int handleclient(struct client *p, struct client *top) {
    char buf[MAX_NAME];
    char outbuf[512];
    fflush(stdout);
    int len = read(p->fd, buf, sizeof(buf) - 1); 
    if (len > 0) {
        if (p->inmatch == 1 && p->turn == 1) {
            taketurn(buf, p, p->last_battled);
        }
        return 0;
    } else if (len <= 0) {
        if (p->inmatch == 1) {
            struct client *opponent = p->last_battled;
            if (opponent != NULL) {
                char outbuf[512];
                // Notify the opponent that they win and are awaiting a new opponent
                sprintf(outbuf, "Your opponent, %s, has left the game. You win!\n", opponent->name);
                write(opponent->fd, outbuf, strlen(outbuf));
                opponent->inmatch = 0;
            }
        }
        // Broadcast the disconnection message to other clients
        sprintf(outbuf, "\n***%s has left the game***\n", p->name); // Display the dropping client's name
        broadcast(top, outbuf, strlen(outbuf)); 
        return -1;
    } return -1;
}

/* adds client to the list of players */ 
static struct client *addclient(struct client *top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client)); // Allocate memory for the new client
    if (!p) {
        perror("malloc");
        exit(1);
    }
    p->fd = fd; // Set the file descriptor
    p->ipaddr = addr; // Set the IP address
    printf("Adding client %s\n", inet_ntoa(p->ipaddr));

    char outbuf[512];
    char *namemsg = "What is your name? ";
    write(p->fd, namemsg, strlen(namemsg));
    fflush(stdout);
    if (read(p->fd, p->name, MAX_NAME) <= 0){
        perror("Couldn't read client name.");
        exit(1);
    } p->name[strlen(p->name) - 1] = '\0';
    char greeting[100];
    sprintf(greeting, "\nHello, %s\n", p->name);
    write(p->fd, greeting, strlen(greeting));

    char *intro = "\nWelcome to our multiplayer combat game! \nGet ready to engage in a thrilling battle of wits \nand strategy against another player. \n\nYou have three moves at your disposal:\n(a) Attack: Launch a direct attack against your opponent. \n(p) Power Hit: Unleash a powerful strike to deal extra damage \n(s) Speak: Send a message to your opponent. \n\nStrategize wisely, each move can turn the tide of the game. \nPlay it safe, think ahead, and emerge victorious! \nGood luck, and may the best player win!\n\n"; 
    write(p->fd, intro, strlen(intro));

    // tell everyone a new player has entered the arena
    printf("%s has entered the game. \n", inet_ntoa((p)->ipaddr)); // Print message to server saying client arrived
    sprintf(outbuf, "**%s enters the arena** \n", p->name);
    broadcast(top, outbuf, strlen(outbuf)); // Broadcast the message to other clients
    p->win_streak = 0;
    p->inmatch = 0;
    p->next = top;
    top = p;
    setupclient(p, top);
    return top;
}

/* finds an opponent for the player to battle
 * if there is no opponent available, informs the player to wait */ 
int setupclient(struct client *p, struct client *top) {
    if (p->inmatch == 0) {
        struct client *opponent = find_match(p, top);
        if (!opponent) {
            char *wait =  "Awaiting opponent... \n";
            write(p->fd, wait, strlen(wait));
        } else {
            engageincombat(p, opponent);
        }
    } return 0;
}

/* iterates through the list of players to find a suitable opponent */ 
struct client *find_match(struct client *player, struct client *top) {
    struct client *candidate = NULL; // Keep track of a potential candidate for fallback
    for (struct client *p = top; p != NULL; p = p->next) {
        // Skip if it's the same player or if the potential match is already in a match.
        if (strcmp(player->name, p->name) == 0 || p->inmatch) {
            continue;
        }
        // If the player has not battled anyone yet, or this is not the last battled opponent.
        if (player->last_battled == NULL || strcmp(p->name, player->last_battled->name) != 0) {
            return p; // Found a suitable match.
        } else if (player->last_battled != NULL && strcmp(p->name, player->last_battled->name) == 0) {
            // Keep a reference to the last battled opponent in case no other match is found.
            candidate = p;
        }
    }
    // If no suitable match other than the last battled opponent is found, return the last battled opponent.
    // This is a fallback and will return the last battled opponent only if no other opponents are available.
    return candidate;
}

/* sets up two clients for combat */ 
void engageincombat(struct client *player1, struct client *player2) {
    char outbuf[MAX_BUF];
    player1->last_battled = player2;
    player2->last_battled = player1;
    player1->inmatch = 1;
    player2->inmatch = 1;
    player1->turn = 1;
    player2->turn = 0;
    player1->hitpoints = (rand()%11) + 20;
    player1->powermoves = (rand()%3) + 1;
    player2->hitpoints = (rand()%11) + 20;
    player2->powermoves = (rand()%3) + 1;

    if (player1->win_streak >= 3) {
        player1->powermoves = 4;
    } if (player2->win_streak >= 3) {
        player2->powermoves = 4;
    }

    // Notify both players they are engaging
    sprintf(outbuf, "You engage %s! \n", player2->name);
    write(player1->fd, outbuf, strlen(outbuf));
    sprintf(outbuf, "\n%s has a win streak of %d \n", player2->name, player2->win_streak);
    write(player1->fd, outbuf, strlen(outbuf));
    sprintf(outbuf, "You engage %s! \n", player1->name);
    write(player2->fd, outbuf, strlen(outbuf));
    sprintf(outbuf, "\n%s has a win streak of %d \n", player1->name, player1->win_streak);
    write(player2->fd, outbuf, strlen(outbuf));

    displaystats(player1, player2);
    displayactions(player1);
    saywait(player2);
}

/* displays user stats to both players in a combat */ 
void displaystats(struct client *player1, struct client *player2) {
    char outbuf[MAX_BUF];
    // Display player 1 info
    sprintf(outbuf, "Your hitpoints: %d \n", player1->hitpoints);
    write(player1->fd, outbuf, strlen(outbuf));
    sprintf(outbuf, "Your powermoves: %d \n", player1->powermoves);
    write(player1->fd, outbuf, strlen(outbuf));
    sprintf(outbuf, "\n%s's hitpoints: %d \n", player2->name, player2->hitpoints);
    write(player1->fd, outbuf, strlen(outbuf));
    // Display player 2 info
    sprintf(outbuf, "Your hitpoints: %d \n", player2->hitpoints);
    write(player2->fd, outbuf, strlen(outbuf));
    sprintf(outbuf, "Your powermoves: %d \n", player2->powermoves);
    write(player2->fd, outbuf, strlen(outbuf));
    sprintf(outbuf, "\n%s's hitpoints: %d \n", player1->name, player1->hitpoints);
    write(player2->fd, outbuf, strlen(outbuf));
}

/* displays possible actions a player can take */ 
void displayactions(struct client *player1) {
    // Display options to player 1
    if (player1->powermoves > 0) {
        char *options = "\n(a)attack \n(p)owermove \n(s)speak something \n";
        write(player1->fd, options, strlen(options));
    } else {
        char *options = "\n(a)attack \n(s)speak something \n";
        write(player1->fd, options, strlen(options));
    }
}

/* informs a client to wait for an opponent to take their turn */ 
void saywait(struct client *p) {
    char outbuf[MAX_BUF];
    // Tell player 2 to wait for player 1
    struct client *opponent = p->last_battled;
    sprintf(outbuf, "Waiting for %s to strike... \n\n", opponent->name);
    write(p->fd, outbuf, strlen(outbuf));
}

/* parses the player's input and executes the corresponding action */ 
int taketurn(char *action, struct client *player1, struct client *player2) {
    char outbuf[MAX_BUF];
    if (action[0] == 'a') {
        player1->turn = 0;
        player2->turn = 1;
        int damage = (rand()%5) + 2; 
        player2->hitpoints -= damage;
        sprintf(outbuf, "\nYou hit %s for %d damage! \n", player2->name, damage);
        write(player1->fd, outbuf, strlen(outbuf));
        sprintf(outbuf, "\n%s hits you for %d damage! \n", player1->name, damage);
        write(player2->fd, outbuf, strlen(outbuf));
        if (check_combat_done(player1, player2) == 0) {
            displaystats(player1, player2);
            displayactions(player2);
            saywait(player1);
        }
    } else if (action[0] == 'p') {
        if (player1->powermoves > 0) {
            int luck = (rand()%2);
            player1->turn = 0;
            player2->turn = 1;
            if (luck == 0) {
                int damage = ((rand()% 5) + 2) * 3;
                player1->powermoves -= 1;
                player2->hitpoints -= damage;
                sprintf(outbuf, "\nYou powermove %s for %d damage! \n", player2->name, damage);
                write(player1->fd, outbuf, strlen(outbuf));
                sprintf(outbuf, "\n%s powermoves you for %d damage! \n", player1->name, damage);
                write(player2->fd, outbuf, strlen(outbuf));
                if (check_combat_done(player1, player2) == 0) {
                    displaystats(player1, player2);
                    displayactions(player2);
                    saywait(player1);
                }
            }
            else {
                sprintf(outbuf, "\nYou missed %s! \n", player2->name);
                write(player1->fd, outbuf, strlen(outbuf));
                sprintf(outbuf, "\n%s missed you! \n", player1->name);
                write(player2->fd, outbuf, strlen(outbuf));
                displaystats(player1, player2);
                displayactions(player2);
                saywait(player1);
            }
            
        } else {
            char *options = "\nYou don't have any powermoves left.\n";
            write(player1->fd, options, strlen(options));
            displayactions(player1);
        }
    } else if (action[0] == 's') {
        player1->turn = 1;
        player2->turn = 0;
        // Player 1 speaks
        char *speak = "\nSpeak: ";
        write(player1->fd, speak, strlen(speak));
        char speech[150];
        fflush(stdout);
        read(player1->fd, speech, 150);
        sprintf(outbuf, "\nYou speak: %s\n", speech);
        write(player1->fd, outbuf, strlen(outbuf));
        // Player 2 listens
        sprintf(outbuf, "\n%s takes a break to tells you: \n %s \n", player1->name, speech);
        write(player2->fd, outbuf, strlen(outbuf));
        displayactions(player1);
    } else {
        player1->turn = 1;
        player2->turn = 0;
        char *outputbuf = "\nPlease enter a valid move \n";
        write(player1->fd, outputbuf, strlen(outputbuf));
        displayactions(player1);
    }

    return 0;
}

/* check if a combat has ended by checking both players' hitpoints
 * if it has ended, both players are notified and and their attributes 
 * are updates */ 
int check_combat_done (struct client *player1, struct client *player2) {
    char outbuf[MAX_BUF];
    // Check if the combat has ended
    if (player2->hitpoints < 0) {
        sprintf(outbuf, "\n%s gives up. You win!\n", player2->name);
        write(player1->fd, outbuf, strlen(outbuf));
        sprintf(outbuf, "\nYou are no match for %s. You scurry away... \n", player1->name);
        write(player2->fd, outbuf, strlen(outbuf));
        player1->win_streak += 1;
        player2->win_streak = 0;
        player1->inmatch = 0;
        player2->inmatch = 0;
        player1->turn = 0;
        player2->turn = 0;
        setupclient(player1, head);
        setupclient(player2, head);
        return 1;
    } else if (player1->hitpoints < 0) {
        sprintf(outbuf, "\n%s gives up. You win!\n", player1->name);
        write(player2->fd, outbuf, strlen(outbuf));
        sprintf(outbuf, "\nYou are no match for %s. You scurry away... \n", player2->name);
        write(player1->fd, outbuf, strlen(outbuf));
        player1->win_streak = 0;
        player2->win_streak += 1;
        player1->inmatch = 0;
        player2->inmatch = 0;
        player1->turn = 0;
        player2->turn = 0;
        setupclient(player1, head);
        setupclient(player2, head);
        return 1;
    }
    return 0;
}

/* moves client p to the end of the list */ 
void move(struct client **top, struct client *p) {
    if (*top == p) {
        *top = p->next;
    } else {
        struct client *prev = *top;
        while (prev->next != p) {
            prev = prev->next;
        }
        prev->next = p->next;
    }
    // Moving p to the end
    p->next = NULL;
    struct client *curr = *top;
    while (curr->next != NULL) {
        curr = curr->next;
    }
    curr->next = p;
}

/* bind and listen, abort on error
 * returns FD of listening socket
 */
int bindandlisten(void) {
    // create socket
    int listenfd;
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) { // Create a socket
        perror("socket");
        exit(1);
    }
    // set socket to allow multiple connections
    int yes = 1;
    if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }
    // initialise server address
    struct sockaddr_in server;
    memset(&server, '\0', sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);
    // Bind the socket
    if (bind(listenfd, (struct sockaddr *)&server, sizeof server)) {
        perror("bind");
        exit(1);
    }
    // Listen for incoming connections
    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
    return listenfd;
}

// Function to remove a client from the list
static struct client *removeclient(struct client *top, int fd) {
    struct client **p;

    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        free(*p); // Free memory allocated for the client
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
    return top;
}

// Function to broadcast a message to all clients
static void broadcast(struct client *top, char *s, int size) {
    struct client *p;
    for (p = top; p; p = p->next) {
        write(p->fd, s, size); 
    }
    /* should probably check write() return value and perhaps remove client */
}
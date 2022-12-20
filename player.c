#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include "player.h"

int udpfd,errcode, tcpfd, num_errors, num_letters, trial, errorsMade = 0;
socklen_t addrlen;
struct addrinfo hints,*res, *tcpRes;
struct sockaddr_in addr;
char buffer[MAX_STR], gsport[MAX_GSPORT_SIZE], gsip[MAX_GSIP_SIZE], hostname[MAX_HOSTNAME_SIZE], plid[MAX_PLID_SIZE], wordUnderscores[MAX_WORD_SIZE*2];


//test comment

int main (int argc, char **argv) {
    char command[COMMAND_SIZE];
    CLEAN_BUFFER();
    init(argc, argv);

    udpfd=socket(AF_INET,SOCK_DGRAM,0); //TCP socket
    if (udpfd==-1) exit(1); //error

    memset(&hints,0,sizeof hints);
    hints.ai_family=AF_INET; //IPv4
    hints.ai_socktype=SOCK_DGRAM;

    errcode = getaddrinfo(HOSTNAME, PORT, &hints, &res);
    if(errcode!=0)/*error*/exit(1);

    while(1) {
        scanf("%s", command);
        switch(hash(command))
        {
            case(COMMAND_START):
            case(COMMAND_SG):
                start_game();
                break;
            case(COMMAND_PLAY):
            case(COMMAND_PL):
                play();
                break;
            case(COMMAND_GUESS):
            case(COMMAND_GW):
                guess_word();
                break;
            case(COMMAND_REV):
                reveal();
                break;
            case(COMMAND_SCOREBOARD):
            case(COMMAND_SB):
                scoreboard(); 
                break;      
            case(COMMAND_HINT):
            case(COMMAND_H):
                hint(); 
                break;      
            case(COMMAND_STATE):
            case(COMMAND_ST):
                state(); 
                break;      
            case(COMMAND_QUIT):
                quit_game();
                break;
            case(COMMAND_EXIT):
                if(quit_game() == 0) return 0;
                else break;
            default:
                printf("Error!\n");
                break;
        }
    }
}

void start_game() {
    char message[MAX_STR];
    CLEAN_BUFFER();

    scanf("%s", plid);
    
    sprintf(message, "SNG %s\n", plid);

    SEND(message, strlen(message));

    RECEIVE(buffer, MAX_STR);

    trial = 1;
    char status[2];
    sscanf(buffer, "%*s %s", status);
    if (strcmp(status, "NOK") == 0)
    {
        printf("Game already in progress.\n");
    }
    else
    {
        sscanf(buffer, "%*s %*s %d %d", &num_errors, &num_letters);
        for(int i = 0; i < num_letters*2; i+=2) { wordUnderscores[i] = '_'; wordUnderscores[i+1] = ' '; }
        wordUnderscores[num_letters*2-1] = '\0';
        printf("New game started. Guess %d letter word: %s\n", num_letters, wordUnderscores);
    }
}

void play() {
    char letter, message[MAX_STR], *token, *status;
    CLEAN_BUFFER();

    scanf(" %c", &letter);

    sprintf(message, "PLG %s %c %d\n", plid, letter, trial);

    SEND(message, strlen(message));

    RECEIVE(buffer, MAX_STR);
    token = strtok(buffer, " ");
    // fetch status
    status = strtok(NULL, " ");
    enum status stat = getStatus(status);
    switch(stat) {
        case(OK):
        case(WIN):
            correctGuess(token, letter);
            trial++;
            break;
        case(DUP):
            printf("Duplicate guess!\n");
            break; 
        case(NOK):
            printf("Incorrect guess.\n%d/%d errors made.\n", ++errorsMade, num_errors);
            trial++;
            break; 
        case(INV):
            printf("Invalid trial numbers!\n");
            break; 
        case(OVR):
            printf("Incorrect guess.\nGame over!\n");
            break;
        default:
            printf("The server encountered an error. Please verify that you have an ongoing game (type sg <plid> to start one), you entered a valid PLID, or that you typed this command correctly (pl <letter>).");
    }
}

void scoreboard() {
    char message[MAX_STR];

    sprintf(message, "GSB\n");

    tcpfd=socket(AF_INET,SOCK_STREAM,0); //TCP socket
    if (tcpfd==-1) exit(1); //error

    memset(&hints,0,sizeof hints);
    hints.ai_family=AF_INET; //IPv4
    hints.ai_socktype=SOCK_STREAM;

    errcode = getaddrinfo(HOSTNAME, PORT, &hints, &tcpRes);
    if(errcode!=0)/*error*/exit(1);

    connect(tcpfd, tcpRes->ai_addr, tcpRes->ai_addrlen);

    write(tcpfd, message, strlen(message));

    printf("Waiting for read...\n");
    memset(buffer, '\0', MAX_STR);
    read(tcpfd, buffer, MAX_STR);
    printf("%s\n", buffer);
    printf("Read!\n");
    
    close(tcpfd);
}

void hint() {
    return;
}

void state() {
    return;
}

void correctGuess(char *token, char letter) {
    int positions[MAX_WORD_SIZE];
    for(int i = 0; i < MAX_WORD_SIZE; i++) positions[i] = 0;
    // Skip trial
    strtok(NULL, " ");
    // fetch positions
    token = strtok(NULL, " ");
    int i = 0;
    while(token != NULL) {
        positions[atoi(token)*2] = 1;

        token = strtok(NULL, " ");
    }
    for(int i = 0; i < num_letters*2; i+=2) {
        if (positions[i] == 1)
            wordUnderscores[i] = letter;
        wordUnderscores[i+1] = ' '; 
    }
    wordUnderscores[num_letters*2-1] = '\0';
    for(int i = 0; i < MAX_WORD_SIZE; i++) { 
        if (wordUnderscores[i] == '_') {
            printf("Correct guess. Word: %s\n", wordUnderscores);
            return;
        }
    }
    printf("Word: %s!\nYou win!\n", wordUnderscores);
}

void guess_word() {
    char word[MAX_WORD_SIZE], message[MAX_STR];
    CLEAN_BUFFER();

    scanf(" %s", word);

    sprintf(message, "PWG %s %s %d\n", plid, word, trial);

    SEND(message, MAX_STR);

    RECEIVE(buffer, MAX_STR);

    char status[COMMAND_SIZE];
    sscanf(buffer, "RWG %s %*s", status);

    switch(getStatus(status)) {
        case(WIN):
            printf("You win!\n");
            break;
        case(NOK):
            printf("Incorrect guess.\n%d/%d errors made.\n", ++errorsMade, num_errors);
            break;
        case(INV):
            printf("Invalid trial numbers!\n");
            break; 
        case(OVR):
            printf("Incorrect guess.\nGame over!\n");
            break;
        case(ERR):
            printf("The server encountered an error. Please verify that you have an ongoing game (type sg <plid> to start one), that you entered a valid PLID, or that you typed this command correctly (gw <word>).");
    }
}


void reveal() {
    char message[MAX_STR];
    
    sprintf(message, "REV %s\n", plid);

    SEND(message, MAX_STR);

    RECEIVE(buffer, MAX_STR);

    char status[COMMAND_SIZE], word[MAX_WORD_SIZE], *token;
    
    token = strtok(buffer+COMMAND_SIZE, "/");
    strcpy(word, token);
    sscanf(strtok(NULL, "/"), "%s\n", status);

    switch(getStatus(status)) {
        case(OK):
            printf("Reveal requested.\nWord: %s.\nGame over!\n", word);
            break;
        default:
            printf("The server encountered an error. Are you sure you have an ongoing game?\n");
            break;
    }
}

// Returns an integer indicating wether the game was successfully quit
int quit_game() {
    char message[MAX_STR];
    
    sprintf(message, "QUT %s\n", plid);

    SEND(message, MAX_STR);

    RECEIVE(buffer, MAX_STR);

    char status[COMMAND_SIZE];
    sscanf(buffer, "RQT %s\n", status);

    switch(getStatus(status)) {
        case(OK):
            printf("User quit!\nGame over!\n");
            return 0;
        default:
            printf("The server encountered an error. Are you sure you have an ongoing game?\n");
            return 1;
    }
}


enum status getStatus(char* status) {
    if (strcmp(status, "NOK") == 0) { return NOK; }  
    if (strcmp(status, "OK") == 0) { return OK; }  
    if (strcmp(status, "ERR") == 0) { return ERR; }  
    if (strcmp(status, "WIN") == 0) { return WIN; }  
    if (strcmp(status, "DUP") == 0) { return DUP; }  
    if (strcmp(status, "OVR") == 0) { return OVR; }  
    if (strcmp(status, "INV") == 0) { return INV; }  
}

// djb2 hashing method for simplicity and switch case
int hash(char* arg) {
    int hash = 5381;
    int c;

    while (c = *arg++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

void init(int argc, char **argv) {
    // Initialize GSport to hostname in case one wasn't provided
    strcpy(gsport, PORT);
    gethostname(hostname, MAX_HOSTNAME_SIZE);

    if (argc < 3)
        return;
    strcpy(gsip, argv[2]);
    if (argc < 5)
        return;
    strcpy(gsport, argv[4]);
}

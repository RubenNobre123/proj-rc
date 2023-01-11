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

int main (int argc, char **argv) {
    char command[COMMAND_SIZE];
    CLEAN_BUFFER();
    init(argc, argv);

    udpfd=socket(AF_INET,SOCK_DGRAM,0);
    if (udpfd==-1) exit(1);

    memset(&hints,0,sizeof hints);
    hints.ai_family=AF_INET;
    hints.ai_socktype=SOCK_DGRAM;

    errcode = getaddrinfo(gsip, gsport, &hints, &res);
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
                quit_game();
                return 0;
            default:
                printf("Error!\n");
                break;
        }
    }
}

void start_game() {
    char message[MAX_STR];
    int offset;
    CLEAN_BUFFER();

    scanf("%s", plid);
    
    offset = sprintf(message, "SNG %s\n", plid);

    SEND(message, offset);

    if(receiveUDP(buffer) == -1)
        return;

    trial = 1;
    char status[2], code[4];
    sscanf(buffer, "%s %s%n", code, status, &offset);

    if(getStatus(code)==ERR) {
        printf("Invalid PLID!\n");
        return;
    }


    if (strcmp(status, "NOK") == 0)
    {
        printf("Game already in progress.\n");
    }
    else
    {
        sscanf(buffer+offset, " %d %d", &num_letters, &num_errors);
        for(int i = 0; i < num_letters*2; i+=2) { wordUnderscores[i] = '_'; wordUnderscores[i+1] = ' '; }
        wordUnderscores[num_letters*2-1] = '\0';
        printf("New game started. Guess %d letter word: %s\n", num_letters, wordUnderscores);
    }
}

void play() {
    char letter, message[MAX_STR], *token, *status;
    int offset;
    CLEAN_BUFFER();

    scanf(" %c", &letter);

    offset = sprintf(message, "PLG %s %c %d\n", plid, letter, trial);

    SEND(message, offset);

    memset(buffer, '\0', MAX_STR);
    if(receiveUDP(buffer) == -1)
        return;


    token = strtok(buffer, " ");
    status = strtok(NULL, " ");
    if(status[strlen(status)-1] == '\n')
        status[strlen(status)-1] = '\0';

    enum status stat = getStatus(status);

    switch(stat) {
        case(OK):
        case(WIN):
            correctGuess(token, letter, stat);
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
            printf("The server encountered an error. Please verify that you have an ongoing game (type sg <plid> to start one), you entered a valid PLID, or that you typed this command correctly (pl <letter>).\n");
    }
}

void scoreboard() {
    char message[MAX_STR], status[10];
    int offset;

    offset = sprintf(message, "GSB\n");

    tcpfd=socket(AF_INET,SOCK_STREAM,0); //TCP socket
    if (tcpfd==-1) exit(1); //error

    memset(&hints,0,sizeof hints);
    hints.ai_family=AF_INET; //IPv4
    hints.ai_socktype=SOCK_STREAM;

    errcode = getaddrinfo(gsip, gsport, &hints, &tcpRes);
    if(errcode!=0)/*error*/exit(1);

    connect(tcpfd, tcpRes->ai_addr, tcpRes->ai_addrlen);

    write(tcpfd, message, offset);

    if(receiveTCP(9, NULL) == -1)
        return;

    sscanf(buffer, "RSB %s", status);

    switch(getStatus(status)) {
        case(EMPTY):
            printf("No games have been played! No scoreboard available.\n");
            break;
        case(OK):
            receiveScoreboard();
            break;
        default:
            printf("Unknown error ocurred.");
            break;
    }

    close(tcpfd);
}

void receiveScoreboard() {
    char sbName[MAX_FILENAME_SIZE], line[MAX_STR];
    int sbSize, bytesRead, offset;
    
    if(receiveTCP(MAX_STR, &bytesRead)==-1)
        return;
    sscanf(buffer, "%s %d\n%n", sbName, &sbSize, &offset);
    
    int toWrite = sbSize;
    FILE* sb = fopen(sbName, "w");

    toWrite-=fwrite(buffer+offset, 1, bytesRead-offset, sb);
    while(toWrite > 0) {
        if((bytesRead = read(tcpfd, buffer, MAX_STR)) > 0) {
            toWrite -= fwrite(buffer, 1, bytesRead, sb);
            memset(buffer, '\0', MAX_STR);
        }
    }
    
    printf("File %s created with size %d\n", sbName, sbSize);

    fclose(sb);
}

void hint() {
    char message[MAX_STR], status[10];
    int offset;

    offset = sprintf(message, "GHL %s\n", plid);

    tcpfd=socket(AF_INET,SOCK_STREAM,0); //TCP socket
    if (tcpfd==-1) exit(1); //error

    memset(&hints,0,sizeof hints);
    hints.ai_family=AF_INET; //IPv4
    hints.ai_socktype=SOCK_STREAM;

    errcode = getaddrinfo(gsip, gsport, &hints, &tcpRes);
    if(errcode!=0)/*error*/exit(1);

    connect(tcpfd, tcpRes->ai_addr, tcpRes->ai_addrlen);

    write(tcpfd, message, offset);

    if(receiveTCP(8, NULL) == -1)
        return;

    sscanf(buffer, "RHL %s ", status);

    switch(getStatus(status)) {
        case(NOK):
            printf("No file available for this word!\n");
            break;
        case(OK):
            receiveFile();
            break;
        default:
            printf("Unknown error ocurred.");
            break;
    }

    close(tcpfd);
}

void receiveFile() {
    char fileName[MAX_FILENAME_SIZE], line[MAX_STR];
    int fileSize, bytesRead, offset;
    
    if(receiveTCP(MAX_STR, &bytesRead)==-1)
        return;

    sscanf(buffer, "%s %d\n%n", fileName, &fileSize, &offset);

    int toWrite = fileSize;
    FILE* sb = fopen(fileName, "wb");

    toWrite-=fwrite(buffer+offset, 1, bytesRead-offset, sb);

    while(toWrite > 0) {
        if((bytesRead = read(tcpfd, buffer, MAX_STR)) > 0) {
            toWrite -= fwrite(buffer, 1, bytesRead, sb);
            memset(buffer, '\0', MAX_STR);
        }
    }

    printf("Hint %s created with size %d!\n", fileName, fileSize);

    fclose(sb);
}

void state() {
    char message[MAX_STR], status[10];
    int offset;

    offset = sprintf(message, "STA %s\n", plid);

    tcpfd=socket(AF_INET,SOCK_STREAM,0); //TCP socket
    if (tcpfd==-1) exit(1); //error

    memset(&hints,0,sizeof hints);
    hints.ai_family=AF_INET; //IPv4
    hints.ai_socktype=SOCK_STREAM;

    errcode = getaddrinfo(gsip, gsport, &hints, &tcpRes);
    if(errcode!=0)/*error*/exit(1);

    connect(tcpfd, tcpRes->ai_addr, tcpRes->ai_addrlen);

    write(tcpfd, message, offset);

    memset(buffer, '\0', MAX_STR);
    read(tcpfd, buffer, 8);

    sscanf(buffer, "RST %s", status);

    switch(getStatus(status)) {
        case(FIN):
        case(ACT):
            receiveAndDisplay();
            break;
        case(NOK):
            printf("No games associated with your PLID were found!\n");
            break;
        default:
            printf("Unknown error ocurred.");
            break;
    }

    close(tcpfd);
}

void receiveAndDisplay() {
    char word[MAX_WORD_SIZE], status[10], fileName[MAX_FILENAME_SIZE];
    int fileSize, offset, readBytes;

    if(receiveTCP(MAX_STR, &readBytes)==-1)
        return;
    sscanf(buffer, "%s %d%n", fileName, &fileSize, &offset);

    FILE* f = fopen(fileName, "w");

    fwrite(buffer+offset+1, 1, readBytes-offset-1, f);
    printf("%.*s", readBytes-offset-1, buffer+offset+1);
    while((readBytes = read(tcpfd, buffer, MAX_STR)) > 0) {
        fwrite(buffer, 1, readBytes, f);
        printf("%.*s", readBytes, buffer);
    }

    printf("File %s created with size %d!\n", fileName, fileSize);

    fclose(f);
}

void correctGuess(char *token, char letter, enum status stat) {
    int positions[MAX_WORD_SIZE];
    for(int i = 0; i < MAX_WORD_SIZE; i++) positions[i] = 0;
    // Skip trial
    strtok(NULL, " ");
    // skip num of positions found
    strtok(NULL, " ");
    token = strtok(NULL, " ");
    int i = 0;
    while(token != NULL) {
        wordUnderscores[(atoi(token)-1)*2] = letter;

        token = strtok(NULL, " ");
    }
    wordUnderscores[num_letters*2-1] = '\0';
    if(stat == OK) {
        printf("Correct guess. Word: %s\n", wordUnderscores);
        return;
    }
    if(stat == WIN) {
        for(int i = 0; i < strlen(wordUnderscores); i++) { if (wordUnderscores[i] == '_') wordUnderscores[i] = letter; }
        printf("Word: %s!\nYou win!\n", wordUnderscores);
        return;
    }
}

void guess_word() {
    char word[MAX_WORD_SIZE], message[MAX_STR];
    int offset;
    CLEAN_BUFFER();

    scanf(" %s", word);

    offset = sprintf(message, "PWG %s %s %d\n", plid, word, trial);

    SEND(message, offset);

    if(receiveUDP(buffer) == -1)
        return;
    
    char status[COMMAND_SIZE];
    sscanf(buffer, "RWG %s %*s", status);

    switch(getStatus(status)) {
        case(WIN):
            printf("You win!\n");
            break;
        case(NOK):
            printf("Incorrect guess. %d/%d errors made.\n", ++errorsMade, num_errors);
            trial++;
            break;
        case(INV):
            printf("Invalid trial numbers!\n");
            break; 
        case(OVR):
            printf("Incorrect guess.Game over!\n");
            break;
        case(ERR):
            printf("The server encountered an error. Please verify that you have an ongoing game (type sg <plid> to start one), that you entered a valid PLID, or that you typed this command correctly (gw <word>).");
            break;
    }
}


void reveal() {
    char message[MAX_STR];
    int offset;
    
    offset = sprintf(message, "REV %s\n", plid);

    SEND(message, offset);

    if(receiveUDP(buffer) == -1)
        return;

    char status[COMMAND_SIZE], word[MAX_WORD_SIZE], *token, code[MAX_WORD_SIZE];
    
    sscanf(buffer, "%s%n", code, &offset);
    if(strcmp(code, "RRV") != 0) {
        printf("The server encountered an error. Are you sure you have an ongoing game?\n");
        return;
    }
    sscanf(buffer+offset, " %s", word);

    printf("Reveal requested.\nWord: %s.\nGame over!\n", word);
}

// Returns an integer indicating wether the game was successfully quit
void quit_game() {
    char message[MAX_STR];
    int offset;

    offset = sprintf(message, "QUT %s\n", plid);

    SEND(message, offset);

    if(receiveUDP(buffer) == -1)
        return;

    char status[COMMAND_SIZE];
    sscanf(buffer, "RQT %s\n", status);

    if(getStatus(status) == OK)
            printf("User quit! Game over!\n");
    return;
}


enum status getStatus(char* status) {
    if (strcmp(status, "NOK") == 0) { return NOK; }  
    if (strcmp(status, "OK") == 0) { return OK; }  
    if (strcmp(status, "ERR") == 0) { return ERR; }  
    if (strcmp(status, "WIN") == 0) { return WIN; }  
    if (strcmp(status, "DUP") == 0) { return DUP; }  
    if (strcmp(status, "OVR") == 0) { return OVR; }  
    if (strcmp(status, "INV") == 0) { return INV; }  
    if (strcmp(status, "QUT") == 0) { return QUT; }
    if (strcmp(status, "EMPTY") == 0) { return EMPTY; }
    if (strcmp(status, "ACT") == 0) { return ACT; }
    if (strcmp(status, "FIN") == 0) { return FIN; }
}

int receiveUDP() {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(udpfd, &readfds);
    struct timeval tv;
    if(TIMEOUT_ENABLED) {
        tv.tv_sec = TIMEOUT_SEC;
        select(udpfd+1, &readfds, NULL, NULL, &tv);
    }
    else
        select(udpfd+1, &readfds, NULL, NULL, NULL);
    if(TIMEOUT_ENABLED && tv.tv_sec == 0) {
        printf("No response was received from the server. Please try again later.\n");
        return -1;
    }
    addrlen = sizeof(addr); 
    int n = recvfrom(udpfd, buffer, MAX_STR, 0, (struct sockaddr*) &addr, &addrlen); 
    if (n == -1) exit(1);
    return 0;
}

int receiveTCP(int amount, int *amountRead) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(tcpfd, &readfds);
    struct timeval tv;
    if(TIMEOUT_ENABLED) {
        tv.tv_sec = TIMEOUT_SEC;
        select(tcpfd+1, &readfds, NULL, NULL, &tv);
    }
    else
        select(tcpfd+1, &readfds, NULL, NULL, NULL);
    if(TIMEOUT_ENABLED && tv.tv_sec == 0) {
        printf("No response was received from the server. Please try again later.\n");
        return -1;
    }
    if(amountRead == NULL)
        read(tcpfd, buffer, amount);
    else
        *amountRead = read(tcpfd, buffer, amount);
    return 0;
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
    strcpy(gsport, DEFAULT_PORT);
    gethostname(gsip, MAX_HOSTNAME_SIZE);

    if (argc < 3)
        return;
    strcpy(gsip, argv[2]);
    if (argc < 5)
        return;
    strcpy(gsport, argv[4]);
}

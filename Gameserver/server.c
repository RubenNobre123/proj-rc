#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include "server.h"

int udpfd, tcpfd, connfd, errcode;
fd_set fdreads;
ssize_t n, lineNumber = 0;
socklen_t addrlen;
struct addrinfo hints, *res;
struct sockaddr_in addr, cliaddr;
char words_file[128];

int main(int argc, char** argv){
    char request[REQUEST_SIZE];
    init(argc, argv);

    memset(&hints, 0, sizeof hints);
    hints.ai_family=AF_INET;//IPv4
    hints.ai_socktype=SOCK_STREAM;//TCP socket
    hints.ai_flags=AI_PASSIVE;

    n = getaddrinfo(NULL, PORT, &hints, &res);

    /* create listening TCP socket */
    tcpfd = socket(AF_INET, SOCK_STREAM, 0);
    const int enable = 1;
    setsockopt(udpfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    n = bind(tcpfd, res->ai_addr,res->ai_addrlen);
    if(n==-1) exit(1);

    listen(tcpfd, 20);

    memset(&hints, 0, sizeof hints);
    hints.ai_family=AF_INET;//IPv4
    hints.ai_socktype=SOCK_DGRAM;//UDP socket
    hints.ai_flags=AI_PASSIVE;

    n = getaddrinfo("localhost", PORT, &hints, &res);
    if(n != 0) exit(1);

    /* create UDP socket */
    udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(udpfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    n = bind(udpfd, res->ai_addr,res->ai_addrlen);
    if(n==-1) exit(1);

    FD_ZERO(&fdreads);
    FD_SET(udpfd, &fdreads);
    FD_SET(tcpfd, &fdreads);
    int maxfd = udpfd > tcpfd ? udpfd + 1 : tcpfd + 1;

    while(1) {

        fd_set copy = fdreads;

        select(maxfd, &copy, NULL, NULL, NULL);

        if(FD_ISSET(udpfd, &copy)) {
            char command[COMMAND_SIZE];
            char request[REQUEST_SIZE];

            RECEIVE(request, REQUEST_SIZE);

            strncpy(command, request, COMMAND_SIZE);
            command[COMMAND_SIZE-1] = '\0';

            stringToUpper(command);
            switch(hash(command)) {
                case(REQUEST_SNG): sng(request+COMMAND_SIZE); break;
                case(REQUEST_PLG): plg(request+COMMAND_SIZE); break; 
                case(REQUEST_PWG): pwg(request+COMMAND_SIZE); break; 
                case(REQUEST_QUT): qut(request+COMMAND_SIZE); break; 
                case(REQUEST_REV): rev(request+COMMAND_SIZE); break;
                default: UNKOWN_MESSAGE(command); break;
            }
        }
        else if (FD_ISSET(tcpfd, &copy)) {
            pid_t childpid;
            char command[COMMAND_SIZE];
            char request[REQUEST_SIZE];
            
            socklen_t addr_size = sizeof(addr);
            connfd = accept(tcpfd, (struct sockaddr *) &addr, &addr_size);
            read(connfd, request, REQUEST_SIZE);

            strncpy(command, request, COMMAND_SIZE);
            command[COMMAND_SIZE-1] = '\0';

            childpid = fork();
            if (childpid == 0) {
                switch(hash(command)) {
                    case(REQUEST_GSB): gsb(); break;
                    case(REQUEST_GHL): ghl(request+COMMAND_SIZE); break;
                    case(REQUEST_STA): sta(request+COMMAND_SIZE); break;
                }
                close(connfd);
                exit(0);
            }
        }
    }
}

void sng(char* args) { 
    char plid[MAX_PLID_SIZE], path[MAX_STR] = GAMES_DIRECTORY, content[MAX_STR], reply[MAX_STR];
    enum status stat;

    // Falta verificar se o plid é válido
    strncpy(plid, args, MAX_PLID_SIZE-1);
    plid[MAX_PLID_SIZE-1] = '\0';
    sprintf(path, "./GAMES/GAME_%s.txt", plid);

    if(fileExists(path))
    {
        // Already exists
        stat = NOK;
    }
    else
    {
        char word[MAX_WORD_SIZE], image[MAX_WORD_SIZE], content[MAX_STR];
        char line[256];
        void *ptr = NULL;

        stat = OK;
        FILE* words = fopen(words_file, "r");
        int i = 0, num_errors, n_letters;

        while((ptr = fgets(line, sizeof(line), words)))
        {
            if(i == lineNumber)
            {
                sscanf(line, "%s %s\n", word, image);
                num_errors = NUM_ERRORS(word);
                n_letters = strlen(word);
                sprintf(content, "%s %s\n", word, image);

                // Write in file
                writeToUserFile(path, content);

                lineNumber++;
                break;
            }
            else i++;
        }
        // Reached EOF, should still choose a word
        if(ptr == NULL) {
            lineNumber = 0;
            stat = NOK;
        }
        
        if(stat == NOK)
            sprintf(reply, "RSG %s\n", statusToString(stat));
        else
            sprintf(reply, "RSG %s %d %d\n", statusToString(stat), num_errors, n_letters);

        SEND(reply);
    }
} 

void plg(char* args) {
    char plid[MAX_PLID_SIZE], path[MAX_STR] = GAMES_DIRECTORY, content[MAX_STR], reply[MAX_STR], letter;
    int trial, positions[MAX_WORD_SIZE], found;
    enum status stat;

    // Falta verificar se o plid é válido
    strncpy(plid, args, MAX_PLID_SIZE-1);
    plid[MAX_PLID_SIZE-1] = '\0';
    sprintf(path, "./GAMES/GAME_%s.txt", plid);

    sscanf(args, "%s %c %d", plid, &letter, &trial);

    if(fileExists(path)) {

        found = playLetter(path, letter, trial, content, (int*) &stat, positions);

        // Check if game is over
        if (!(stat == INV || stat == DUP))
            writeToUserFile(path, content);
        if(stat == WIN || stat == OVR)
            endGame(path, (int*) &stat);
    }
    else
        stat = ERR;
    
    if(stat == ERR)
        sprintf(reply, "RLG %s\n", statusToString(stat));
    else
        sprintf(reply, "RLG %s %d\n", statusToString(stat), trial);
    int i = 0;
    while(i < found) {
        sprintf(reply+strlen(reply), " %d", positions[i++]);
    }
    SEND(reply);
}

void pwg(char* args) {
    char plid[MAX_PLID_SIZE], path[MAX_STR] = GAMES_DIRECTORY, content[MAX_STR], reply[MAX_STR], word[MAX_STR];
    int trial;
    enum status stat;

    // Falta verificar se o plid é válido
    strncpy(plid, args, MAX_PLID_SIZE-1);
    plid[MAX_PLID_SIZE-1] = '\0';
    sprintf(path, "./GAMES/GAME_%s.txt", plid);

    sscanf(args, "%s %s %d", plid, word, &trial);

    if(fileExists(path)) {
        
        guessWord(path, word, trial, content, (int*) &stat);

        // Check if game is over
        if (!(stat == INV || stat == DUP))
            writeToUserFile(path, content);
        if(stat == WIN || stat == OVR)
            endGame(path, (int*) &stat);
    }
    else
        stat = ERR;
    
    if(stat == ERR)
        sprintf(reply, "RWG %s\n", statusToString(stat));
    else
        sprintf(reply, "RWG %s %d\n", statusToString(stat), trial);

    SEND(reply);
}

void qut(char* args) {
    char plid[MAX_PLID_SIZE], path[MAX_STR] = GAMES_DIRECTORY, reply[MAX_STR];
    enum status stat;

    // Falta verificar se o plid é válido
    strncpy(plid, args, MAX_PLID_SIZE-1);
    plid[MAX_PLID_SIZE-1] = '\0';
    sprintf(path, "./GAMES/GAME_%s.txt", plid);
    stat = QUT;

    if(fileExists(path)) {
        endGame(path, (int*) &stat);
        stat = OK;
    }
    else stat = ERR;

    sprintf(reply, "RQT %s\n", statusToString(stat));

    SEND(reply);
}

void rev(char *args) {
    char path[MAX_STR], plid[MAX_PLID_SIZE], reply[MAX_STR], word[MAX_WORD_SIZE];
    enum status stat;
    stat = QUT;


    sprintf(word, "##");

    // Falta verificar se o plid é válido
    strncpy(plid, args, MAX_PLID_SIZE-1);
    plid[MAX_PLID_SIZE-1] = '\0';
    sprintf(path, "./GAMES/GAME_%s.txt", plid);

    if(fileExists(path)) {
        FILE* game = fopen(path, "r");
        fscanf(game, "%s %*s\n", word);

        endGame(path, (int*) &stat);

        stat = OK;
    }
    else stat = ERR;

    sprintf(reply, "RRV %s/%s\n", word, statusToString(stat));

    SEND(reply);
}

void gsb() {
    char buffer[MAX_STR], line[MAX_STR], SCOREBOARD_PATH[MAX_STR];
    
    sprintf(SCOREBOARD_PATH, "%s/scoreboard.txt", GAMES_DIRECTORY);

    // Create scoreboard file
    FILE* scoreboard = fopen(SCOREBOARD_PATH, "w");    

    write(connfd, buffer, MAX_STR);
}

void ghl(char *args) {
    return;
}

void sta(char *args) {
    return;
}

void guessWord(char* gamePath, char* wordGuessed, int userTrial, char* content, int* stat) {
    int fileTrial = 1, fileErrors = 0;
    void *ptr = NULL;
    char line[MAX_STR], word[MAX_WORD_SIZE];

    FILE* game = fopen(gamePath, "r");
    fscanf(game, "%s %*s\n", word);
    while((ptr = fgets(line, sizeof(line), game))) {

        // Line reflects a letter-guess
        if(line[0] == 'T') {
            int guessed;
            sscanf(line, "T %*c %d\n", &guessed);
            if (guessed == 0) fileErrors++;
        }
        else if(line[0] == 'G') {
            char fileGuess[MAX_STR];
            sscanf(line, "G %s\n", fileGuess);
            if(strcasecmp(fileGuess, wordGuessed) == 0) {
                *stat = DUP;
                return;
            }
            else fileErrors++;
        }
        fileTrial++;
    }
    if(fileTrial != userTrial) {
        *stat = INV;
        return;
    }

    *stat = strcmp(word, wordGuessed) == 0 ? WIN : NOK;

    if (*stat == NOK) {
        int num_errors = NUM_ERRORS(word);
        if(fileErrors + 1 == num_errors)
            *stat = OVR;
    }

    sprintf(content, "G %s\n", word);

    fclose(game);

    return;
}

// Handles the play letter command; returns number of positions found
int playLetter(char* gamePath, char letter, int userTrial, char* content, int* stat, int* positions) {
    int fileTrial = 1, totalPosGuessed = 0, fileErrors=0;
    void* ptr = NULL;
    char line[MAX_STR], word[MAX_WORD_SIZE];

    FILE* game = fopen(gamePath, "r");
    fscanf(game, "%s %*s\n", word);
    while((ptr = fgets(line, sizeof(line), game))) {

        // For each play, check if it was successful
        int posGuessed;
        char guess;
        if(line[0] == 'T') {
            sscanf(line, "T %c %d\n", &guess, &posGuessed);
            if(posGuessed == 0)
                fileErrors++;
            // Falta ignorar case
            if (guess == letter) { 
                *stat = DUP; 
                return 0;
            }

            totalPosGuessed += posGuessed;
        }
        // "Guess word" play
        else fileErrors++;
        fileTrial++;
    }
    if(fileTrial != userTrial) {
        *stat = INV;
        return 0;
    }

    char* cursor = word;
    int found = 0, cursorCount = 0;
    while(cursorCount < strlen(word)) { 
        if (*cursor == letter) {
            positions[found++] = cursorCount;
        }

        cursor+=sizeof(char);
        cursorCount++;
    }
    if (found == 0) {
        *stat = NOK;
        int num_errors = NUM_ERRORS(word);
        if(fileErrors + 1 == num_errors)
            *stat = OVR;
    }
    else { 
        *stat = OK;
        if (totalPosGuessed + found == strlen(word))
            *stat = WIN;
    }

    sprintf(content, "T %c %d\n", letter, found);

    fclose(game);

    return found;
}

void endGame(char* gamePath, int *stat) {
    char line[MAX_STR], mode[5];

    if(*stat == WIN)
        strcpy(mode, "WIN");
    else if(*stat == OVR)
        strcpy(mode, "FAIL");
    else if (*stat == QUT)
        strcpy(mode, "QUIT");

    sprintf(line, "%s\n", mode);

    writeToUserFile(gamePath, line);

    char dest[MAX_STR*2], dir[MAX_STR], plid[MAX_PLID_SIZE], code;

    sscanf(gamePath, "./GAMES/GAME_%s.txt", plid);

    plid[MAX_PLID_SIZE-1] = '\0';

    sprintf(dir, "./GAMES/%s/", plid);
    DIR* d;
    if((d = opendir(dir))) closedir(d);
    else mkdir(dir, 0777);
    
    time_t rawtime = time(NULL);
    struct tm *t;
    
    t = localtime(&rawtime);

    sprintf(dest, "%s/%d%02d%02d_%02d%02d%02d_%c.txt", dir, 1900+t->tm_year, 1+t->tm_mon, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, mode[0]);

    if(*stat == WIN)
        createScore(gamePath, plid);

    rename(gamePath, dest);
}

void createScore(char* gamePath, char* plid) {
    char line[MAX_STR], word[MAX_WORD_SIZE], wordGuessed[MAX_WORD_SIZE];
    int userTrial = 1, fileTrial = 1, fileErrors = 0;
    void* ptr = NULL;

    FILE* game = fopen(gamePath, "r");
    fscanf(game, "%s %s\n", word, wordGuessed);
    while((ptr = fgets(line, sizeof(line), game))) {
        if(line[0] == 'T') {
            int guessed;
            sscanf(line, "T %*c %d\n", &guessed);
            if (guessed == 0) fileErrors++;
            fileTrial++;
        }
        else if(line[0] == 'G') {
            fileErrors++;
            fileTrial++;
        }
    }
    fclose(game);

    int score = 100*(fileTrial - fileErrors) / fileTrial;

    char content[MAX_STR], scorePath[MAX_STR];

    sprintf(content, "%d %s %d %d %s\n", score, plid, (fileTrial - fileErrors)-1, fileTrial-1, word);


    time_t rawtime = time(NULL);
    struct tm *t;
    
    t = localtime(&rawtime);

    sprintf(scorePath, "./SCORES/%03d_%s_%d%02d%02d_%02d%02d%02d.txt", score, plid, 1900+t->tm_year, 1+t->tm_mon, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

    FILE* scoreFile = fopen(scorePath, "w");

    fprintf(scoreFile, "%s", content);

    fclose(scoreFile);
}

void stringToUpper(char* arg) {
    for(int i = 0; i < COMMAND_SIZE; i++) {
        arg[i] = toupper(arg[i]);
    }
}

void writeToUserFile(char* path, char* content) {
    FILE* userFile = fopen(path, "a");

    fprintf(userFile, "%s", content);

    fclose(userFile);
}

// djb2 hashing method for simplicity and switch case
int hash(char* arg) {
    int hash = 5381;
    int c;

    while (c = *arg++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

void init(int argc, char** argv) {

    if (argc < 2)
        ERRORMSG();
    sprintf(words_file, "./%s", argv[1]);
}

char* statusToString(enum status s) {
    char *strings[] = { "NOK", "OK", "ERR", "WIN", "DUP", "OVR", "INV", "QUT" };

    return strings[s];
}

int fileExists(char* path) {
    FILE* f = fopen(path, "r");
    if(f)
        fclose(f);
    return f != NULL;
}

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
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include "server.h"

int udpfd, tcpfd, connfd, errcode, verbose = 0, childpid;
fd_set fdreads;
ssize_t n, lineNumber = 0;
socklen_t addrlen;
struct addrinfo hints, *res;
struct sockaddr_in addr, cliaddr;
char words_file[128], gsport[MAX_GSPORT_SIZE];

int main(int argc, char** argv){
    char request[REQUEST_SIZE];
    init(argc, argv);

    memset(&hints, 0, sizeof hints);
    hints.ai_family=AF_INET;//IPv4
    hints.ai_socktype=SOCK_STREAM;//TCP socket
    hints.ai_flags=AI_PASSIVE;

    n = getaddrinfo(NULL, gsport, &hints, &res);

    /* create listening TCP socket */
    tcpfd = socket(AF_INET, SOCK_STREAM, 0);
    const int enable = 1;
    setsockopt(tcpfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    n = bind(tcpfd, res->ai_addr,res->ai_addrlen);
    if(n==-1) {
        printf("%s\n", strerror(errno));
        exit(1);
    }

    listen(tcpfd, 20);

    memset(&hints, 0, sizeof hints);
    hints.ai_family=AF_INET;//IPv4
    hints.ai_socktype=SOCK_DGRAM;//UDP socket
    hints.ai_flags=AI_PASSIVE;

    n = getaddrinfo(NULL, gsport, &hints, &res);
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
                    default: UNKOWN_MESSAGE(command); break;
                }
                close(connfd);
                close(tcpfd);
                close(udpfd);
                exit(0);
            }
            close(connfd);
        }
    }
}

void sng(char* args) { 
    char plid[MAX_PLID_SIZE], path[MAX_STR] = GAMES_DIRECTORY, content[MAX_STR], reply[MAX_STR];
    enum status stat;
    int i = 0, num_errors, n_letters, chosen = 0;

    if(getArguments(args, plid, path) == -1) {
        sprintf(reply, "ERR\n");
        SEND(reply);
        return;
    }

    if(verbose) {
        printf("Received %s request from IP %s and port %s with PLID %s.\n", "SNG", inet_ntoa(addr.sin_addr), gsport, plid); 
    }

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

        while(!chosen) {
            FILE* words = fopen(words_file, "r");   
            lineNumber = 0;
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
                    chosen = 1;
                    break;
                }
                else i++;
            }
            fclose(words);
        }
    }
    if(stat == NOK)
        sprintf(reply, "RSG %s\n", statusToString(stat));
    else
        sprintf(reply, "RSG %s %d %d\n", statusToString(stat), n_letters, num_errors);

    SEND(reply);
} 

void plg(char* args) {
    char plid[MAX_PLID_SIZE], path[MAX_STR] = GAMES_DIRECTORY, content[MAX_STR], reply[MAX_STR], letter;
    int trial, positions[MAX_WORD_SIZE], found;
    enum status stat;

    if(getArguments(args, plid, path) == -1) {
        sprintf(reply, "ERR\n");
        SEND(reply);
        return;
    }

    if(verbose) {
        printf("Received %s request from IP %s and port %s with PLID %s.\n", "PLG", inet_ntoa(addr.sin_addr), gsport, plid); 
    }

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
    
    if(stat != OK)
        sprintf(reply, "RLG %s\n", statusToString(stat));
    else {
        sprintf(reply, "RLG %s %d %d ", statusToString(stat), trial, found);
        int i = 0;
        while(i < found) {
            if(i+1 == found)
                sprintf(reply+strlen(reply), "%d\n", positions[i++]);
            else
                sprintf(reply+strlen(reply), "%d ", positions[i++]);
        }
    }
    SEND(reply);
}

void pwg(char* args) {
    char plid[MAX_PLID_SIZE], path[MAX_STR] = GAMES_DIRECTORY, content[MAX_STR], reply[MAX_STR], word[MAX_STR];
    int trial;
    enum status stat;

    if(getArguments(args, plid, path) == -1) {
        sprintf(reply, "ERR\n");
        SEND(reply);
        return;
    }

    if(verbose) {
        printf("Received %s request from IP %s and port %s with PLID %s.\n", "PWG", inet_ntoa(addr.sin_addr), gsport, plid); 
    }

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

    if(getArguments(args, plid, path) == -1) {
        sprintf(reply, "ERR\n");
        SEND(reply);
        return;
    }

    if(verbose) {
        printf("Received %s request from IP %s and port %s with PLID %s.\n", "QUT", inet_ntoa(addr.sin_addr), gsport, plid); 
    }

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

    sprintf(word, "##");

    if(getArguments(args, plid, path) == -1) {
        sprintf(reply, "ERR\n");
        SEND(reply);
        return;
    }

    if(verbose) {
        printf("Received %s request from IP %s and port %s with PLID %s.\n", "REV", inet_ntoa(addr.sin_addr), gsport, plid); 
    }

    if(fileExists(path)) {
        FILE* game = fopen(path, "r");
        fscanf(game, "%s %*s\n", word);

        remove(path);

        stat = OK;
    }
    else stat = ERR;

    sprintf(reply, "RRV %s/%s\n", word, statusToString(stat));

    SEND(reply);
}

void gsb() {
    char buffer[MAX_STR], scoreboardPath[MAX_STR], line[MAX_STR], scoresDir[MAX_STR], reply[MAX_STR*2];
    enum status stat;

    if(verbose) {
        printf("Received %s request from IP %s and port %s with PLID %s.\n", "SNG", inet_ntoa(addr.sin_addr), gsport, "N/A");
    }

    DIR* dir = opendir(GAMES_DIRECTORY);
    // Start reading scoreDir and count how many files are in it
    struct dirent* entry;
    int nFiles = 0;
    while((entry = readdir(dir))) {
        if(entry->d_type == DT_REG) {
            nFiles++;
        }
    }
    closedir(dir);

    stat = nFiles == 0 ? EMPTY : OK;

    sprintf(reply, "RSB %s\n", statusToString(stat));
    write(connfd, reply, MAX_STR);

    if(stat==OK) {
        long bytes = createScoreboard(scoreboardPath, nFiles);
        FILE* sb = fopen(scoreboardPath, "r");
        int offset, readBytes, bytesRead;

        sprintf(reply, "%s %ld\n%n", "scoreboard.txt", bytes, &offset);
        write(connfd, reply, offset);

        while(bytes > 0) {
            bytesRead = fread(line, 1, MAX_STR, sb);
            bytes -= write(connfd, line, bytesRead);
        }

        fclose(sb);
    }
    else {
        strcat(reply, "\n");
        write(connfd, reply, MAX_STR);
    }
}

int createScoreboard(char* scoreboardPath, int nFiles) {
    char fname[MAX_STR*2];
    int written = 0;
    struct dirent **entries;
    FILE* fp;
    int n_entries;

    sprintf(scoreboardPath, "%s/scoreboard.txt", GAMES_DIRECTORY);

    // Create scoreboard file
    FILE* scoreboard = fopen(scoreboardPath, "w");

    n_entries = scandir("./SCORES/", &entries, NULL, alphasort);
    if(n_entries < 0) return 0;
    while (n_entries--) {
        if(entries[n_entries]->d_name[0] != '.') {
            sprintf(fname, "SCORES/%s", entries[n_entries]->d_name);
            fp = fopen(fname, "r");
            if(fp) {
                char plid[MAX_PLID_SIZE], word[MAX_WORD_SIZE];
                int score, rights, total;
                fscanf(fp, "%d %s %d %d %s\n", &score, plid, &rights, &total, word);
                written += fprintf(scoreboard, "%d %s %d %d %s\n", score, plid, rights, total, word);
            }
            fclose(fp);
        }
    }
    fclose(scoreboard);
    return written;
}

void ghl(char *args) {
    char buffer[MAX_STR], path[MAX_STR], image[MAX_FILENAME_SIZE], line[50], scoresDir[MAX_STR], reply[MAX_STR*2], plid[MAX_PLID_SIZE];
    enum status stat;

    if(getArguments(args, plid, path) == -1) {
        sprintf(reply, "ERR\n");
        SEND(reply);
        return;
    }

    if(verbose) {
        printf("Received %s request from IP %s and port %s with PLID %s.\n", "GHL", inet_ntoa(addr.sin_addr), gsport, plid); 
    }

    if(fileExists(path)) {
        FILE* game = fopen(path, "r");

        fscanf(game, "%*s %s\n", image);
        fclose(game);

        stat = OK;
    }
    else stat = NOK;

    sprintf(reply, "RHL %s ", statusToString(stat));
    write(connfd, reply, 8);

    if(stat==OK) {
        char imagePath[MAX_STR] = "./images/";
        strcat(imagePath, image);
        FILE* img = fopen(imagePath, "rb");
        int bytesRead, offset;
        long fileSize = getFileSize(imagePath);

        sprintf(reply, "%s %ld\n%n", image, fileSize, &offset);
        write(connfd, reply, offset);

        while(fileSize > 0) {
            bytesRead = fread(line, 1, MAX_STR, img);
            fileSize -= write(connfd, line, bytesRead);
        }

        fclose(img);
    }
    else {
        strcat(reply, "\n");
        write(connfd, reply, MAX_STR);
    }
}

void sta(char *args) {
    char buffer[MAX_STR], path[MAX_STR], fname[MAX_FILENAME_SIZE], image[MAX_FILENAME_SIZE], line[50], scoresDir[MAX_STR], reply[MAX_STR*2], plid[MAX_PLID_SIZE];
    enum status stat;

    if(getArguments(args, plid, path) == -1) {
        sprintf(reply, "ERR\n");
        SEND(reply);
        return;
    }

    if(verbose) {
        printf("Received %s request from IP %s and port %s with PLID %s.\n", "STA", inet_ntoa(addr.sin_addr), gsport, plid); 
    }

    if(fileExists(path)) {
        FILE* game = fopen(path, "r");

        fscanf(game, "%*s %s\n", image);
        fclose(game);

        stat = ACT;
    }
    else {
        if(findLastGame(plid, fname)) stat = FIN;
        else stat = NOK;
    }
    sprintf(reply, "RST %s ", statusToString(stat));
    write(connfd, reply, 8);

    if(stat==ACT || stat==FIN) {
        char statePath[MAX_STR];
        long bytes;
        if(stat==ACT) 
            bytes = createStateFile(path, statePath, plid, buffer, stat);
        else
            bytes = createStateFile(fname, statePath, plid, buffer, stat);
        FILE* sb = fopen(statePath, "r");
        int offset, bytesRead;

        char fileName[MAX_FILENAME_SIZE];
        sscanf(statePath, "./%s", fileName);

        bytesRead = sprintf(reply, "%s %ld ", fileName, bytes);
        write(connfd, reply, bytesRead);

        while(bytes > 0) {
            bytesRead = fread(line, 1, MAX_STR, sb);
            bytes -= write(connfd, line, bytesRead);
        }

        fclose(sb);
    }
    else {
        strcat(reply, "\n");
        write(connfd, reply, MAX_STR);
    }
}

int findLastGame(char* PLID, char* fname)
{
    struct dirent **filelist;
    int n_entries, found;
    char dirname[20];
    sprintf(dirname, "GAMES/%s/", PLID);
    n_entries=scandir(dirname, &filelist,0,alphasort);
    found=0;
    if(n_entries<=0)
        return 0;
    else {
        while(n_entries--) {
            if(filelist[n_entries]->d_name[0]!='.') {
                sprintf(fname,"GAMES/%s/%s",PLID,filelist[n_entries]->d_name);
                found=1;
            }
            free(filelist[n_entries]);
            if(found)
                break;
        }
        free(filelist);
    }
    return found;
}

long createStateFile(char* path, char* statePath, char* plid, char* buffer, enum status stat) {
    FILE* game = fopen(path, "r");
    long bytes = 0;
    char word[MAX_WORD_SIZE], image[MAX_FILENAME_SIZE];
    sprintf(statePath, "./STATE_%s.txt", plid);
    
    FILE* stateFile = fopen(statePath, "w");

    fscanf(game, "%s %s\n", word, image);
    if(stat == ACT)
        bytes += fprintf(stateFile, "Active game for player %s found:\n", plid);
    else if(stat == FIN) {
        bytes += fprintf(stateFile, "Last game for player %s found. Word was %s and hint was %s\n", plid, word, image);
    }

    bytes += fprintf(stateFile, "--------------------------------\n");
    int i = 0;

    while(fgets(buffer, MAX_STR, game)) {
        if(buffer[0] == 'T') {
            char letter;
            int found;

            sscanf(buffer, "T %c %d\n", &letter, &found);
            bytes += fprintf(stateFile, "\t%2d - \tLETTER:\t%c\tFOUND:\t%d\t\n", i++, letter, found);
        }
        else if(buffer[0] == 'G') {
            char guessedWord[MAX_WORD_SIZE], success[8];
            sscanf(buffer, "G %s\n", guessedWord);

            if(strcasecmp(word, guessedWord) == 0)
                sprintf(success, "SUCCESS");
            else
                sprintf(success, "FAIL");

            bytes += fprintf(stateFile, "\t%2d - \tWORD:\t%s\t%s\t\n", i++, guessedWord, success);
        }
    }
    bytes += fprintf(stateFile, "--------------------------------\n");
    fclose(stateFile);    
    fclose(game);
    return bytes;
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

    *stat = strcasecmp(word, wordGuessed) == 0 ? WIN : NOK;

    if (*stat == NOK) {
        int num_errors = NUM_ERRORS(word);
        if(fileErrors + 1 == num_errors)
            *stat = OVR;
    }

    sprintf(content, "G %s\n", wordGuessed);

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
            if (guess == letter || guess == letter + 32) { 
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
            positions[found++] = cursorCount+1;
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

    sprintf(dir, "%s%s/", GAMES_DIRECTORY, plid);
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
    int wasGuess = 1;
    while((ptr = fgets(line, sizeof(line), game))) {

        if(line[0] == 'T') {
            int guessed;
            sscanf(line, "T %*c %d\n", &guessed);
            if (guessed == 0) fileErrors++;
            wasGuess = 0;
            fileTrial++;
        }
        else if(line[0] == 'G') {
            wasGuess = 1;
            fileErrors++;
            fileTrial++;
        }
    }
    if(wasGuess) fileErrors--;
    fclose(game);

    int score = 100*(fileTrial - fileErrors) / fileTrial;

    char content[MAX_STR], scorePath[MAX_STR];

    sprintf(content, "%d %s %d %d %s\n", score, plid, (fileTrial - fileErrors)-1, fileTrial-1, word);

    time_t rawtime = time(NULL);
    struct tm *t;
    
    t = localtime(&rawtime);

    sprintf(scorePath, "%s/%03d_%s_%d%02d%02d_%02d%02d%02d.txt", SCORES_DIRECTORY, score, plid, 1900+t->tm_year, 1+t->tm_mon, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

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

long getFileSize(char* file) {
    FILE* fp = fopen(file, "rb");
    fseek(fp, 0L, SEEK_END);
    long size = ftell(fp);
    fclose(fp);
    return size;
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

    mkdir(GAMES_DIRECTORY, 0777);
    mkdir(SCORES_DIRECTORY, 0777);

    struct sigaction act;

    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;

    if(sigaction(SIGPIPE, &act, NULL) == -1) {
        exit(-1);
    }
    signal(SIGKILL, sigHandler);


    if(argc < 2) {
        printf("Usage: %s <words_file> [-p <port>] [-v]\n", argv[0]);
        exit(-1);
    }

    strcpy(words_file, argv[1]);

    for (int i = 2; i < argc; i += 2)
    {

        if (argv[i][0] != '-')
            exit(1);

        switch (argv[i][1])
        {

        case 'v':
            verbose = 1;
            break;

        case 'p':
            strcpy(gsport, argv[i + 1]);
            break;
        }
    }
}

void sigHandler() {
    killpg(childpid, SIGKILL);
}

char* statusToString(enum status s) {
    char *strings[] = { "NOK", "OK", "ERR", "WIN", "DUP", "OVR", "INV", "QUT", "EMPTY", "ACT", "FIN" };

    return strings[s];
}

int ongoingGame(char* path) {
    if(fileExists(path)) {
        char line[MAX_STR];
        FILE* game = fopen(path, "r");
        fgets(line, sizeof(line), game);
        fgets(line, sizeof(line), game);
        return (line[0] == 'G' || line[0] == 'T');
    }
}

int fileExists(char* path) {
    FILE* f = fopen(path, "r");
    if(f)
        fclose(f);
    return f != NULL;
}

int getArguments( char* args, char* plid, char *path) {

    snprintf(plid,  MAX_PLID_SIZE, "%s", args);
    plid[MAX_PLID_SIZE-1] = '\0';

    int n = atoi(plid); 
    if(strlen(plid) < 6 || n > 200000) {
        return -1;
    }

    sprintf(path, "%sGAME_%s.txt", GAMES_DIRECTORY, plid);

    return 0;
}
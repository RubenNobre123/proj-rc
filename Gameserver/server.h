#define PORT "58001"
#define MAX_STR 256

// Relevant application-related constants
#define MAX_GSPORT_SIZE 6
#define MAX_GSIP_SIZE 16
#define COMMAND_SIZE 4
#define REQUEST_SIZE 50
#define MAX_PLID_SIZE 7
#define MAX_WORD_SIZE 31
#define MAX_HOSTNAME_SIZE 100

// Request hashes
#define REQUEST_SNG 193470029
#define REQUEST_RSG 193469105
#define REQUEST_PLG 193466696
#define REQUEST_RLG 193468874
#define REQUEST_PWG 193467059
#define REQUEST_RWG 193469237
#define REQUEST_GSB 193457121
#define REQUEST_GHL 193492704
#define REQUEST_STA 193506157
#define REQUEST_QUT 193468095
#define REQUEST_RQT 193469052
#define REQUEST_RRV 193469087
#define REQUEST_REV 193468658

#define GAMES_DIRECTORY "./GAMES/"

#define ERRORMSG() { printf("Usage: ./GS word_file [-p GSport] [-v]\n"); exit(1); }
#define UNKOWN_MESSAGE(msg) { printf("Unknown protocol message received: %s\n", msg);}
#define NUM_ERRORS(word) strlen(word) <= 6 ? 7 : (strlen(word) >= 11 ? 9 : 8);
#define RECEIVE(buffer, size) { addrlen = sizeof(addr); n = recvfrom (udpfd, buffer, size, 0, (struct sockaddr*) &addr, &addrlen); if (n == -1) exit(1);}
#define SEND(msg) { n = sendto(udpfd, msg, strlen(msg), 0, (struct sockaddr*) &addr, addrlen); if (n == -1) exit(1);}

enum status {
    NOK,
    OK,
    ERR,
    WIN,
    DUP,
    OVR,
    INV,
    QUT
};

void init(int argc, char** argv);
int hash(char* arg);
void writeToUserFile(char* path, char* content);
void stringToUpper(char* arg);
char* statusToString(enum status s);
int fileExists(char* path);
void endGame(char* gamePath, int *stat);
int playLetter(char* gamePath, char letter, int userTrial, char* content, int* stat, int* positions) ;
void guessWord(char* gamePath, char* wordGuessed, int userTrial, char* content, int* stat);
void createScore(char* gamePath, char* plid);
void sng(char* plid); 
void plg(char* args); 
void pwg(char* args);
void qut(char* args);
void rev(char *args);
void gsb();
void ghl(char* args);
void sta(char *args);
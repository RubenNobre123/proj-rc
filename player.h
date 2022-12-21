#define PORT "58011"
#define HOSTNAME "tejo.tecnico.ulisboa.pt"
#define MAX_STR 256

// Relevant application-related constants
#define MAX_GSPORT_SIZE 6
#define MAX_GSIP_SIZE 16
#define COMMAND_SIZE 4
#define REQUEST_SIZE 50
#define MAX_PLID_SIZE 7
#define MAX_HOSTNAME_SIZE 100
#define MAX_FILENAME_SIZE 25
#define MAX_WORD_SIZE 31

// Command hashes
#define COMMAND_START 274811347
#define COMMAND_SG 5863807
#define COMMAND_PLAY 2090619483
#define COMMAND_PL 5863713
#define COMMAND_GUESS 260620620
#define COMMAND_GW 5863427
#define COMMAND_SCOREBOARD -1119087959
#define COMMAND_SB 5863802
#define COMMAND_HINT 2090329144
#define COMMAND_H 177677
#define COMMAND_STATE 274811398
#define COMMAND_ST 5863820
#define COMMAND_QUIT 2090665480
#define COMMAND_EXIT 2090237503

#define COMMAND_REV 193504594

// Macros
#define CLEAN_BUFFER() { for(int i = 0; i < MAX_STR; i++) buffer[i] = '\0'; }
#define SEND(message, size) { int n = sendto(udpfd, message, size, 0, res->ai_addr, res->ai_addrlen); if (n == -1) exit(1); }
#define RECEIVE(buffer, size) { addrlen = sizeof(addr); int n = recvfrom(udpfd, buffer, size, 0, (struct sockaddr*) &addr, &addrlen); if (n == -1) exit(1); }

enum status {
    NOK,
    OK,
    ERR,
    WIN,
    DUP,
    OVR,
    INV,
    QUT,
    EMPTY
};

// Auxiliary functions
void correctGuess(char *token, char letter);
enum status getStatus(char* status);
void init(int argc, char **argv);
int hash(char* arg);
void receiveScoreboard();
void receiveFile();

// Command-handling functions
void start_game();
void play();
void guess_word();
int quit_game();
void exit_game();
void reveal();
void scoreboard();
void hint();
void state();
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <memory.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE 200

//Timing Stuff
#include <time.h>
#include <sys/time.h>
#define TIME_S(tv_s) {\
                gettimeofday(&tv_s,NULL);\
            }
    
#define TIME_F(tv_s,tv_f) {\
                gettimeofday(&tv_f,NULL);\
                long seconds = tv_f.tv_sec - tv_s.tv_sec;\
                long microseconds = tv_f.tv_usec - tv_s.tv_usec;\
                double elapsed = seconds + microseconds*1e-6;\
                printf("Elapsed: %.6f\n", elapsed);\
                }

struct command_node_t {
    float burst_us;
    float spacing_us;
    int repeats;
    float frequency;
    float dead_frequency;

    struct command_node_t* next;
};

static int socket_handle = 0;
static int tcp_socket_connection = 0;
static int websocket = 0;

typedef struct pi_options {
    int verbose;
    int udp;
    int tcp;
    int port;
} pi_options_t;

void get_args(const int argc, char* argv[], pi_options_t *options);

static void print_usage(const char* program);
// static void udelay(int us);
static void terminate(int unused);
static void fatal(char* fmt, ...);
static int fill_buffer(
    const struct command_node_t* command,
    const struct command_node_t* next_command,
    int bytes_count
    // struct control_data_s* ctl_,
    // const volatile uint32_t* dma_reg_
);
// static int frequency_to_control(float frequency);
static int parse_json(
    const char* json,
    struct command_node_t** new_command,
    int* request_response
);
static void free_command(struct command_node_t* command);
#define BYTE unsigned char
#define WORD unsigned int
struct SHA1_CTX {
    BYTE data[64];
    WORD datalen;
    unsigned long long bitlen;
    WORD state[5];
    WORD k[4];
};
void sha1_init(struct SHA1_CTX *ctx);
void sha1_update(struct SHA1_CTX *ctx, const BYTE data[], size_t len);
void sha1_final(struct SHA1_CTX *ctx, BYTE hash[]);
void sha1_transform(struct SHA1_CTX *ctx, const BYTE data[]);
int base64_encode(unsigned char *in, int inlen, char *out);
const char* websocket_handshake_response(const char* const handshake);
void decode_websocket_message(char message[]);


int main(int argc, char** argv) {

    struct  timeval t1,t2;
    TIME_S(t1)

    struct pi_options options;
    get_args(argc, argv, &options); 

    int i;

    float frequency = 49.830;

    printf(" debug line:%d\n", __LINE__);

    struct command_node_t* command = malloc(32); 

    printf("command pointer addr: %p\n", command);

    command->burst_us = 100.0f; 
    command->spacing_us = 100.0f; 
    command->repeats = 1; 
    command->frequency = frequency; 
    command->dead_frequency = frequency; 
    command->next = NULL; 

    struct command_node_t* new_command = NULL; 

    struct sockaddr_in server_address; 
    struct sockaddr_in client_address; 
    socklen_t client_length = sizeof(client_address); 

    printf(" debug line:%d", __LINE__);

    bzero(&client_address, sizeof(client_address));
    server_address.sin_family = AF_INET; 
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); 
    server_address.sin_port = htons(options.port); 

    char json_buffer[BUFFER_SIZE];

    int count = 0;
    unsigned long start, end;

    printf(" debug line:%d", __LINE__);

    while (count++ < 100) {
        /* This is nonblocking because we set it as such as above */
        int bytes_count = 0;

        fgets(json_buffer, sizeof(json_buffer) / sizeof(json_buffer[0]), stdin);
            // bytes_count = recvfrom(
            //     socket_handle,
            //     json_buffer,
            //     sizeof(json_buffer) / sizeof(json_buffer[0]),
            //     0,
            //     (struct sockaddr*)&client_address,
            //     &client_length);
        bytes_count = strlen(json_buffer);
        printf("bytes_count: %d\n", bytes_count);
    
        /**
         * If the received message is too long, then it will be truncated.
         * All valid messages should always fit in the buffer, so just ignore it
         * if it's too long.
         */
        if (bytes_count > 0) {
            json_buffer[bytes_count] = '\0';
            struct command_node_t* parsed_command = NULL;
            int request_response = 0;
            /**
             * WebSocket handshake messages have a plain GET request, so we
             * respond with handshake message
             */
            if (strncmp("GET ", json_buffer, 4) == 0) {
                const char* const response = websocket_handshake_response(json_buffer);
                //send(tcp_socket_connection, response, strlen(response), 0);
                printf("%s\n", response);
                websocket = 1;
                continue;
            }
            if (websocket) {
                decode_websocket_message(json_buffer);
            }

            /**
             * I tried to do terminate json_buffer in decode_websocket_message,
             * but sometimes it just doesn't work and I don't know why.
             */
            char* close_bracket = strstr(json_buffer, "]");
            if (close_bracket) {
                *(close_bracket + 1) = '\0';
            }

            printf("%s\n", json_buffer);

            const int parse_status = parse_json(json_buffer, &parsed_command, &request_response);

            if (request_response) {
                const int length = snprintf(
                    json_buffer,
                    sizeof(json_buffer) / sizeof(json_buffer[0]),
                    "{\"time\": %d}",
                    (int)time(NULL));
                /**
                 * The client should be listening for responses on the port one
                 * above the port that it sent to us
                 */
                client_address.sin_port = htons(ntohs(server_address.sin_port) + 1);
                /**
                 * TODO: This probably needs to be updated for TCP,
                 * although, TCP guarantees delivery anyway, so maybe it's
                 * not necessary.
                 */
                printf("send:%s\n", json_buffer);
                // sendto(
                //     socket_handle,
                //     json_buffer,
                //     length,
                //     0,
                //     (struct sockaddr*)&client_address,
                //     client_length);
            }

            if (parse_status == 0) {
                /**
                 * Command bursts come after synchronization bursts, and
                 * they're the interesting part, so print those
                 */
                const struct command_node_t* const print_command = (
                    parsed_command->next == NULL
                    ? parsed_command
                    : parsed_command->next);
                if (options.verbose) {
                    printf(
                        "Sending command %d %d:%d bursts @ %4.3f (%4.3f)\n",
                        print_command->repeats,
                        (int)print_command->burst_us,
                        (int)print_command->spacing_us,
                        print_command->frequency,
                        print_command->dead_frequency);
                }

                if (new_command == NULL) {
                    new_command = parsed_command;
                } else {
                    free_command(command);
                    command = new_command;
                    new_command = parsed_command;
                }
            } else {
                /* The error message will be printed in the JSON parser */
                free_command(parsed_command);
            }
        }

        // const int used = fill_buffer(command, new_command, ctl, dma_reg);
        const int used = fill_buffer(command, new_command, bytes_count);
        if (used > 0) {
            if (new_command != NULL) {
                free_command(command);
                command = new_command;
                new_command = NULL;
            }
        }

        usleep(100000);
    }

    TIME_F(t1,t2)

    terminate(0);

    return 0;
}

static void terminate(const int signal_) {
    printf("Terminating with signal %d\n", signal_);

    if (tcp_socket_connection != 0) {
        close(tcp_socket_connection);
    }
    if (socket_handle != 0) {
        close(socket_handle);
    }
    exit(1);
}


static void fatal(char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    terminate(0);
}

int fill_buffer(
    const struct command_node_t* const command,
    const struct command_node_t* const next_command,
    int bytes_count
) {
    assert(command != NULL || next_command != NULL);

    int nodes_processed = 0;

    if (!bytes_count)
	return 0;

    if (command) {
	if (command->repeats == 1 || command->repeats == 20) {
		//printf("idle\n");
	} else
         	printf("Sending cmd: %f\t%f\t%d\t%f\t%f\n",command->burst_us,command->spacing_us,command->repeats,command->frequency,command->dead_frequency);
        nodes_processed ++; 
    }
   
    return nodes_processed;
}

static int parse_json(
    const char* const json,
    struct command_node_t** command,
    int* const request_response
) {

    return -1;
}


static void free_command(struct command_node_t* command) {
    while (command) {
        struct command_node_t* const previous = command;
        command = command->next;
        free(previous);
    }
}


void get_args(const int argc, char* argv[], pi_options_t *options) {
    const struct option long_options[] = { // scrittura
        {"help", no_argument, NULL, 'h'},
        {"verbose", no_argument, NULL, 'v'},
        {"tcp", no_argument, NULL, 't'},
        {"udp", no_argument, NULL, 'u'},
        {"port", required_argument, NULL, 'p'},
        {0, 0, 0, 0}
    };

    options->port = 12345; //scrittura
    options->verbose = 0; //scrittura
    options->udp = 0; //scrittura
    options->tcp = 0; //scrittura
    int opt;
    int long_index = 0;
    while (1) {
        opt = getopt_long(argc, argv, "hvp:tu", long_options, &long_index);
        if (opt == -1) {
            break;
        }
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
                break;
            case 'v':
                options->verbose = 1;
                break;
            case 'p':
                options->port = atoi(optarg);
                break;
            case 't':
                options->tcp = 1;
                break;
            case 'u':
                options->udp = 1;
                break;
            default:
                fprintf(stderr, "Unknown option\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    if (options->tcp && options->udp) {
        fprintf(stderr, "Only one of --tcp and --udp may be specified\n");
        exit(EXIT_FAILURE);
    }
    if (!options->tcp && !options->udp) {
        /* Default to UDP */
        options->udp = 1; //scrittura
    }
}


static void print_usage(const char* program) {
    printf("Usage: %s [OPTIONS]\n", program);
    printf("-p, --port     The port to listen for messags on.\n");
    printf("-h, --help     Print this help message.\n");
    printf("-v, --verbose  Print more debugging information.\n");
    printf("-t, --tcp      Listen for messages on a TCP connection.\n");
    printf("-u, --udp      Listen for messages on a UDP connection.\n");
}


const char* websocket_handshake_response(const char* const handshake) {
    const char* const key_header = "Sec-WebSocket-Key: ";
    const char* const start = strstr(handshake, key_header);
    
    static char buffer[512];
    char hash[20];
    char key[64] = "FAKE-258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    // const int key_length = strstr(start, "\n") - start - strlen(key_header) - 1;
    
    // const char* const start_key = start + strlen(key_header);
    // char key[64];
    // /**
    //  * The response needs to include base64(SHA-1(key +
    //  * "258EAFA5-E914-47DA-95CA-C5AB0DC85B11))
    //  */
    
    // strncpy(key, start_key, key_length);
    // key[key_length] = '\0';
    // strncpy(buffer, key, key_length);
    // strcpy(buffer + key_length, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    
    // struct SHA1_CTX sha1_context;
    // sha1_init(&sha1_context);
    // sha1_update(&sha1_context, (BYTE*)buffer, strlen(buffer));
    // sha1_final(&sha1_context, (BYTE*)hash);
    
    // const int length = base64_encode((BYTE*)hash, 20, key);
    // key[length] = '\0';
    
    snprintf(
        buffer,
        sizeof(buffer) / sizeof(buffer[0]),
        "HTTP/1.1 101 Web Sockt Protocol Handshake\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: %s\r\n"
            "Acces-Control-Allow-Headers: x-websocket-protocol\r\n\r\n",
        key);
    return buffer;
}


void decode_websocket_message(char message[]) {
    printf("------decode_websocket_message------\n");
    char key[24];
    char buffer[BUFFER_SIZE];
    
    printf("Enter decode key:");
    scanf("%s", key);

    message = buffer; 
}


/*********************************************************************
* Filename:   sha1.c
* Author:     Brad Conte (brad AT bradconte.com)
* Copyright:
* Disclaimer: This code is presented "as is" without any guarantees.
* Details:    Implementation of the SHA1 hashing algorithm.
              Algorithm specification can be found here:
               * http://csrc.nist.gov/publications/fips/fips180-2/fips180-2withchangenotice.pdf
              This implementation uses little endian byte order.
*********************************************************************/


/****************************** MACROS ******************************/
#define ROTLEFT(a, b) ((a << b) | (a >> (32 - b)))

/*********************** FUNCTION DEFINITIONS ***********************/
void sha1_transform(struct SHA1_CTX *ctx, const BYTE data[])
{
    WORD a, b, c, d, e, i, j, t, m[80];

    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = (data[j] << 24) + (data[j + 1] << 16) + (data[j + 2] << 8) + (data[j + 3]);
    for ( ; i < 80; ++i) {
        m[i] = (m[i - 3] ^ m[i - 8] ^ m[i - 14] ^ m[i - 16]);
        m[i] = (m[i] << 1) | (m[i] >> 31);
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];

    for (i = 0; i < 20; ++i) {
        t = ROTLEFT(a, 5) + ((b & c) ^ (~b & d)) + e + ctx->k[0] + m[i];
        e = d;
        d = c;
        c = ROTLEFT(b, 30);
        b = a;
        a = t;
    }
    for ( ; i < 40; ++i) {
        t = ROTLEFT(a, 5) + (b ^ c ^ d) + e + ctx->k[1] + m[i];
        e = d;
        d = c;
        c = ROTLEFT(b, 30);
        b = a;
        a = t;
    }
    for ( ; i < 60; ++i) {
        t = ROTLEFT(a, 5) + ((b & c) ^ (b & d) ^ (c & d))  + e + ctx->k[2] + m[i];
        e = d;
        d = c;
        c = ROTLEFT(b, 30);
        b = a;
        a = t;
    }
    for ( ; i < 80; ++i) {
        t = ROTLEFT(a, 5) + (b ^ c ^ d) + e + ctx->k[3] + m[i];
        e = d;
        d = c;
        c = ROTLEFT(b, 30);
        b = a;
        a = t;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
}

void sha1_init(struct SHA1_CTX *ctx)
{
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xc3d2e1f0;
    ctx->k[0] = 0x5a827999;
    ctx->k[1] = 0x6ed9eba1;
    ctx->k[2] = 0x8f1bbcdc;
    ctx->k[3] = 0xca62c1d6;
}

void sha1_update(struct SHA1_CTX *ctx, const BYTE data[], size_t len)
{
    size_t i;

    for (i = 0; i < len; ++i) {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == 64) {
            sha1_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

void sha1_final(struct SHA1_CTX *ctx, BYTE hash[])
{
    WORD i;

    i = ctx->datalen;

    /* Pad whatever data is left in the buffer. */
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56)
            ctx->data[i++] = 0x00;
    }
    else {
        ctx->data[i++] = 0x80;
        while (i < 64)
            ctx->data[i++] = 0x00;
        sha1_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }

    /**
     * Append to the padding the total message's length in bits and transform.
     */
    ctx->bitlen += ctx->datalen * 8;
    ctx->data[63] = ctx->bitlen;
    ctx->data[62] = ctx->bitlen >> 8;
    ctx->data[61] = ctx->bitlen >> 16;
    ctx->data[60] = ctx->bitlen >> 24;
    ctx->data[59] = ctx->bitlen >> 32;
    ctx->data[58] = ctx->bitlen >> 40;
    ctx->data[57] = ctx->bitlen >> 48;
    ctx->data[56] = ctx->bitlen >> 56;
    sha1_transform(ctx, ctx->data);

    /**
     * Since this implementation uses little endian byte ordering and MD uses big endian,
     * reverse all the bytes when copying the final state to the output hash.
     */
    for (i = 0; i < 4; ++i) {
        hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
    }
}


/* This is a public domain base64 implementation written by WEI Zhicheng. */
/* BASE 64 encode table */
static char base64en[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/',
};

#define BASE64_PAD  '='


#define BASE64DE_FIRST  '+'
#define BASE64DE_LAST   'z'

int
base64_encode(unsigned char *in, int inlen, char *out)
{
    int i, j;

    for (i = j = 0; i < inlen; i++) {
        int s = i % 3;          /* from 6/gcd(6, 8) */

        switch (s) {
        case 0:
            out[j++] = base64en[(in[i] >> 2) & 0x3F];
            continue;
        case 1:
            out[j++] = base64en[((in[i-1] & 0x3) << 4) + ((in[i] >> 4) & 0xF)];
            continue;
        case 2:
            out[j++] = base64en[((in[i-1] & 0xF) << 2) + ((in[i] >> 6) & 0x3)];
            out[j++] = base64en[in[i] & 0x3F];
        default:;
        }
    }

    /* move back */
    i -= 1;

    /* check the last and add padding */
    if ((i % 3) == 0) {
        out[j++] = base64en[(in[i] & 0x3) << 4];
        out[j++] = BASE64_PAD;
        out[j++] = BASE64_PAD;
    } else if ((i % 3) == 1) {
        out[j++] = base64en[(in[i] & 0xF) << 2];
        out[j++] = BASE64_PAD;
    }

    return j;
}

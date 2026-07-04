#pragma once
// Cross-platform TCP socket compatibility layer.
// Supported: Windows (Winsock2) and Linux (BSD sockets). No macOS support.

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef SOCKET socket_t;
  #define CLOSESOCK closesocket
  #define SOCK_INVALID INVALID_SOCKET
#elif defined(__linux__)
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <netdb.h>
  typedef int socket_t;
  #define CLOSESOCK close
  #define SOCK_INVALID (-1)
#else
  #error "esp32-ssh only supports Windows and Linux."
#endif

inline bool socketInit() {
#ifdef _WIN32
  WSADATA wsa;
  return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#else
  return true;
#endif
}

inline void socketCleanup() {
#ifdef _WIN32
  WSACleanup();
#endif
}

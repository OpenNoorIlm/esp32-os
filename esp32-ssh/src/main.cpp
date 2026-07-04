// esp32-ssh -- cross-platform (Windows + Linux) command-line client for
// NoorShell, the authenticated shell running on the ESP32.
//
// Usage:
//   esp32-ssh <host> [--port 2222] [--pass <password>]
//
// If the ESP32 has no password set yet, this client walks you through the
// SETPASS flow automatically. Otherwise it performs the challenge/response
// login (your password is hashed locally and never sent in the clear) and
// drops you into an interactive shell.

#include <iostream>
#include <string>
#include <cstring>
#include "socket_compat.h"
#include "sha256.h"

#ifdef _WIN32
  #include <conio.h>
#else
  #include <termios.h>
  #include <unistd.h>
#endif

static std::string readHiddenPassword(const std::string& prompt) {
  std::cout << prompt;
  std::cout.flush();
  std::string pw;
#ifdef _WIN32
  int ch;
  while ((ch = _getch()) != '\r' && ch != '\n') {
    if (ch == '\b') {
      if (!pw.empty()) { pw.pop_back(); std::cout << "\b \b"; }
    } else {
      pw += (char)ch;
      std::cout << '*';
    }
  }
#else
  termios oldt{}, newt{};
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  std::getline(std::cin, pw);
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif
  std::cout << std::endl;
  return pw;
}

// Small buffered line reader over a raw socket, since NoorShell's protocol
// is now fully newline-delimited (including "PROMPT ..." lines).
class BufferedSocket {
public:
  explicit BufferedSocket(socket_t sock) : sock_(sock) {}

  bool readLine(std::string& out) {
    out.clear();
    while (true) {
      size_t pos = buf_.find('\n');
      if (pos != std::string::npos) {
        out = buf_.substr(0, pos);
        if (!out.empty() && out.back() == '\r') out.pop_back();
        buf_.erase(0, pos + 1);
        return true;
      }
      char tmp[512];
      int n = recv(sock_, tmp, sizeof(tmp), 0);
      if (n <= 0) return false;
      buf_.append(tmp, n);
    }
  }

  bool sendLine(const std::string& line) {
    std::string data = line + "\n";
    return send(sock_, data.c_str(), (int)data.size(), 0) == (int)data.size();
  }

private:
  socket_t sock_;
  std::string buf_;
};

static void printUsage() {
  std::cerr << "Usage: esp32-ssh <host> [--port 2222] [--pass <password>]\n";
}

int main(int argc, char** argv) {
  std::string host;
  int port = 2222;
  std::string passArg;

  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
    else if (a == "--pass" && i + 1 < argc) passArg = argv[++i];
    else if (a == "--host" && i + 1 < argc) host = argv[++i];
    else if (host.empty() && !a.empty() && a[0] != '-') host = a;
  }

  if (host.empty()) { printUsage(); return 1; }

  if (!socketInit()) {
    std::cerr << "Failed to initialize networking.\n";
    return 1;
  }

  socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == SOCK_INVALID) {
    std::cerr << "Failed to create socket.\n";
    socketCleanup();
    return 1;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons((uint16_t)port);
  if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
    std::cerr << "Invalid host IP: " << host << " (use a raw IP address)\n";
    CLOSESOCK(sock);
    socketCleanup();
    return 1;
  }

  if (connect(sock, (sockaddr*)&addr, sizeof(addr)) != 0) {
    std::cerr << "Could not connect to " << host << ":" << port << "\n";
    CLOSESOCK(sock);
    socketCleanup();
    return 1;
  }

  BufferedSocket conn(sock);
  std::string line;

  if (!conn.readLine(line)) {
    std::cerr << "Connection closed unexpectedly.\n";
    CLOSESOCK(sock); socketCleanup(); return 1;
  }
  std::cout << line << "\n"; // "NOOR-SHELL v0.1"

  if (!conn.readLine(line)) {
    std::cerr << "Connection closed unexpectedly.\n";
    CLOSESOCK(sock); socketCleanup(); return 1;
  }

  // First-time setup: no password configured on the ESP32 yet.
  if (line.rfind("AUTH_REQUIRED", 0) == 0) {
    std::cout << line << "\n";
    conn.readLine(line); // "Send: SETPASS <newpassword>"
    std::string newPass = readHiddenPassword("Set a new NoorShell password: ");
    conn.sendLine("SETPASS " + newPass);
    conn.readLine(line);
    std::cout << line << "\n";
    std::cout << "Run esp32-ssh again to log in.\n";
    CLOSESOCK(sock); socketCleanup(); return 0;
  }

  // Normal login: challenge/response, password never sent in the clear.
  if (line.rfind("AUTH CHALLENGE ", 0) == 0) {
    std::string nonce = line.substr(strlen("AUTH CHALLENGE "));
    std::string password = passArg.empty() ? readHiddenPassword("NoorShell password: ") : passArg;
    std::string passHashHex = SHA256::hashHex(password);
    std::vector<uint8_t> key(passHashHex.begin(), passHashHex.end());
    std::string responseHex = SHA256::hex(hmacSha256(key, nonce));

    conn.sendLine("AUTH RESPONSE " + responseHex);
    conn.readLine(line);
    if (line != "AUTH OK") {
      std::cout << line << "\n";
      CLOSESOCK(sock); socketCleanup(); return 1;
    }
    std::cout << "LOGGED IN SUCCESSFULLY\n";
  } else {
    std::cerr << "Unexpected server greeting: " << line << "\n";
    CLOSESOCK(sock); socketCleanup(); return 1;
  }

  // Interactive shell loop: ESP32 drives the prompt via "PROMPT <text>"
  // lines; everything else is command output printed as-is.
  while (true) {
    if (!conn.readLine(line)) {
      std::cout << "\nConnection closed by ESP32.\n";
      break;
    }

    if (line.rfind("PROMPT ", 0) == 0) {
      std::cout << line.substr(strlen("PROMPT "));
      std::cout.flush();

      std::string cmd;
      if (!std::getline(std::cin, cmd)) break;
      if (!conn.sendLine(cmd)) { std::cout << "\nConnection lost.\n"; break; }

      if (cmd == "exit" || cmd == "quit") {
        conn.readLine(line); // "bye!"
        std::cout << line << "\n";
        break;
      }
    } else if (line == "Send: SETPASS <newpassword>") {
      std::string newPass = readHiddenPassword("New NoorShell password: ");
      conn.sendLine("SETPASS " + newPass);
    } else if (line.rfind("ASK ", 0) == 0) {
      // Generic one-line question/answer: print the question (no newline),
      // read one line of plain-text input, send it back. Used for things
      // like the storage backend choice and the untrusted-source [Y/n] gate.
      std::cout << line.substr(strlen("ASK "));
      std::cout.flush();
      std::string answer;
      if (!std::getline(std::cin, answer)) break;
      if (!conn.sendLine(answer)) { std::cout << "\nConnection lost.\n"; break; }
    } else {
      std::cout << line << "\n";
    }
  }

  CLOSESOCK(sock);
  socketCleanup();
  return 0;
}

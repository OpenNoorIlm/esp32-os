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
#include <fstream>
#include <vector>
#include "socket_compat.h"
#include "sha256.h"
#include "config_store.h"
#include "python_finder.h"
#include "process_launch.h"
#include "exe_path.h"
#include "app_config.h"
#include "dependency_installer.h"

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
  std::cerr <<
    "Usage:\n"
    "  esp32-ssh <host> [--port 2222] [--pass <password>]\n"
    "  esp32-ssh <host> --pass <password> --command \"<shell command>\"\n"
    "  esp32-ssh --app <path/to/app/main.py or app folder>\n"
    "  esp32-ssh --python-change   (lists every Python found, lets you pick one)\n"
    "\n"
    "  --host/<host> and --pass are remembered after the first successful\n"
    "  login (saved to a local config file), so later --command/--app runs\n"
    "  don't need them repeated.\n";
}

// Lists every Python interpreter found on this system and lets the user
// pick which one to use for CLIENT GUI apps (--app), saving that exact
// choice so it isn't asked again on every launch. Doesn't need a network
// connection at all -- this is purely local bookkeeping.
static int handlePythonChange() {
  std::vector<std::string> chosen = PythonFinder::choose();
  if (chosen.empty()) return 1;
  NoorConfig cfg = ConfigStore::load();
  cfg.pythonCmd = ConfigStore::joinPythonCmd(chosen);
  ConfigStore::save(cfg);
  std::string shown;
  for (auto& p : chosen) shown += p + " ";
  std::cout << "Python interpreter for CLIENT GUI apps set to: " << shown
            << "\nThis will be used for every 'esp32-ssh --app ...' launch "
               "until changed again (run 'esp32-ssh --python-change' to pick "
               "a different one).\n";
  return 0;
}

// Launches a CLIENT GUI app's entrypoint under the configured Python
// interpreter, with packages/lua, packages/device, packages/oled (etc.)
// each added individually to PYTHONPATH so "import lua" / "import device" /
// "import oled" resolve. Doesn't connect to the ESP32 itself -- the app's
// own lua.run()/device.run()/oled.run() calls do that by shelling back out
// to this same esp32-ssh binary with --command.
static int handleApp(const std::string& appPath, std::string host, std::string passArg) {
  NoorConfig cfg = ConfigStore::load();

  // The app itself can't prompt for a password (it's a GUI, no terminal),
  // so credentials have to come from either this invocation's args or an
  // already-saved config. If neither exists, fail clearly instead of
  // silently hanging on a hidden-password prompt nobody can see.
  if (host.empty())    host = cfg.host;
  if (passArg.empty()) passArg = cfg.password;
  if (host.empty() || passArg.empty()) {
    std::cerr << "error: no saved ESP32 host/password found, and none given.\n"
                 "Run 'esp32-ssh <host> --pass <password>' once first (or pass "
                 "--host/--pass alongside --app) so credentials get saved.\n";
    return 1;
  }
  // Persist whatever we resolved so the app's own lua.run()/device.run()/
  // oled.run() calls -- which shell back out to esp32-ssh --command with no
  // --host/--pass of their own -- pick up the exact same credentials.
  cfg.host = host;
  cfg.password = passArg;
  ConfigStore::save(cfg);

  // No interpreter chosen yet (first --app run ever) -- ask once, right
  // here, and remember it. Later runs skip straight to the saved choice.
  std::vector<std::string> pythonCmd = ConfigStore::splitPythonCmd(cfg.pythonCmd);
  if (pythonCmd.empty()) {
    std::cout << "No Python interpreter selected yet for CLIENT GUI apps.\n";
    pythonCmd = PythonFinder::choose();
    if (pythonCmd.empty()) {
      std::cerr << "error: no interpreter selected -- aborting launch. Run "
                   "'esp32-ssh --python-change' any time to pick one.\n";
      return 1;
    }
    cfg.pythonCmd = ConfigStore::joinPythonCmd(pythonCmd);
    ConfigStore::save(cfg);
  }

  // Resolve the actual script to run: if appPath is a directory, assume
  // main.py inside it (matches config.json's documented "entry": "main.py"
  // convention); if it's a .py file directly, use it as-is.
  std::string scriptPath = appPath;
  std::string appDir = appPath;
  {
    std::ifstream asFile(appPath);
    bool looksLikeFile = appPath.size() > 3 &&
        appPath.substr(appPath.size() - 3) == ".py";
    if (!looksLikeFile) {
      char sepc =
#ifdef _WIN32
        '\\';
#else
        '/';
#endif
      if (!appPath.empty() && appPath.back() != sepc) scriptPath += sepc;
      scriptPath += "main.py";
      appDir = appPath;
    } else {
      size_t pos = appPath.find_last_of("\\/");
      appDir = pos == std::string::npos ? "." : appPath.substr(0, pos);
    }
  }

  std::string packagesRoot = ExePath::exeDir();
  {
    char sepc =
#ifdef _WIN32
      '\\';
#else
      '/';
#endif
    packagesRoot += sepc;
    packagesRoot += "..";
    packagesRoot += sepc;
    packagesRoot += "packages";
  }
  std::vector<std::string> subdirNames = ExePath::listSubdirs(packagesRoot);
  if (subdirNames.empty()) {
    std::cerr << "warning: no packages found under " << packagesRoot
              << " -- \"import lua\"/\"import device\"/\"import oled\" will "
                 "fail in the app.\n";
  }
  std::vector<std::string> packageDirs;
  for (auto& name : subdirNames) {
    packageDirs.push_back(packagesRoot +
#ifdef _WIN32
      "\\" +
#else
      "/" +
#endif
      name);
  }

  // Auto-install whatever this app declares in config.json's
  // "requirements" (e.g. esp32-app needs PyQt5) under this exact
  // interpreter, before ever trying to launch it -- this is what makes
  // "esp32-ssh handles dependencies automatically" actually true instead
  // of just documentation.
  std::vector<std::string> requirements = AppConfig::readRequirements(appDir);
  if (!requirements.empty()) {
    if (!DependencyInstaller::ensureInstalled(pythonCmd, requirements)) {
      std::cerr << "error: aborting launch -- one or more required "
                   "packages could not be installed.\n";
      return 1;
    }
  }

  std::string shownCmd;
  for (auto& p : pythonCmd) shownCmd += p + " ";
  std::cout << "Launching " << scriptPath << " with " << shownCmd << "...\n";
  return ProcessLaunch::run(pythonCmd, scriptPath, appDir, packageDirs);
}

int main(int argc, char** argv) {
  std::string host;
  int port = 2222;
  std::string passArg;
  std::string commandArg;
  std::string appArg;
  bool pythonChangeFlag = false;

  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
    else if ((a == "--pass" || a == "--password") && i + 1 < argc) passArg = argv[++i];
    else if (a == "--host" && i + 1 < argc) host = argv[++i];
    else if (a == "--command" && i + 1 < argc) commandArg = argv[++i];
    else if (a == "--app" && i + 1 < argc) appArg = argv[++i];
    else if (a == "--python-change") {
      pythonChangeFlag = true;
      // Backward-compat: a lone version arg like "3.11" used to follow
      // this flag. It no longer selects anything (the picker lists real,
      // installed interpreters instead of trusting a typed version
      // number), so just skip over it if someone's muscle memory adds one.
      if (i + 1 < argc && !std::string(argv[i + 1]).empty() && argv[i + 1][0] != '-') i++;
    }
    else if (host.empty() && !a.empty() && a[0] != '-') host = a;
  }

  if (pythonChangeFlag) return handlePythonChange();
  if (!appArg.empty()) return handleApp(appArg, host, passArg);

  // Fall back to a saved host/password if none was given on this
  // invocation -- matches the "credentials remembered after first login"
  // behavior documented in printUsage(). This matters most for
  // --command: it's what shell.run() (used by every CLIENT GUI app) calls
  // under the hood with no --pass of its own, and with no fallback here
  // it would sit at a hidden password prompt nobody's watching until the
  // caller's subprocess timeout kills it.
  {
    NoorConfig cfg = ConfigStore::load();
    if (host.empty())    host = cfg.host;
    if (passArg.empty()) passArg = cfg.password;
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

    // Cache the credentials that just worked -- lets later --command/--app
    // runs (and the Python lua/device/oled packages, which shell back out
    // to this same binary) skip re-entering host/password every time.
    NoorConfig cfg = ConfigStore::load();
    cfg.host = host;
    cfg.password = password;
    ConfigStore::save(cfg);
  } else {
    std::cerr << "Unexpected server greeting: " << line << "\n";
    CLOSESOCK(sock); socketCleanup(); return 1;
  }

  // One-shot mode: send exactly one command, print its output, then exit --
  // this is what the Python lua/device/oled packages use under the hood
  // (e.g. lua.run("print(1+1)") shells out to
  // `esp32-ssh <host> --command "lua print(1+1)"`), and what --command
  // scripting from a terminal uses directly.
  if (!commandArg.empty()) {
    // Consume the very first PROMPT (the one shown right after login)
    // before sending the command -- the shell only accepts input once it's
    // actually prompting.
    while (true) {
      if (!conn.readLine(line)) {
        std::cout << "\nConnection closed by ESP32.\n";
        CLOSESOCK(sock); socketCleanup(); return 1;
      }
      if (line.rfind("PROMPT ", 0) == 0) break;
      // Anything else this early (e.g. a stray ASK) -- just surface it.
      std::cout << line << "\n";
    }

    if (!conn.sendLine(commandArg)) {
      std::cout << "\nConnection lost.\n";
      CLOSESOCK(sock); socketCleanup(); return 1;
    }

    // Read everything the command produces, up to (not including) the
    // NEXT prompt, which marks the command as finished. ASK-type follow-up
    // questions (e.g. "storage --change") are answered from stdin same as
    // interactive mode -- works fine from a real terminal; a script piping
    // input can still supply an answer, but there's nothing to answer with
    // if stdin isn't connected to anything, so multi-step commands aren't
    // a great fit for fully unattended --command use.
    while (true) {
      if (!conn.readLine(line)) {
        std::cout << "\nConnection closed by ESP32.\n";
        break;
      }
      if (line.rfind("PROMPT ", 0) == 0) break;
      if (line.rfind("ASK ", 0) == 0) {
        std::cout << line.substr(strlen("ASK "));
        std::cout.flush();
        std::string answer;
        if (!std::getline(std::cin, answer)) break;
        conn.sendLine(answer);
      } else if (line == "Send: SETPASS <newpassword>") {
        std::string newPass = readHiddenPassword("New NoorShell password: ");
        conn.sendLine("SETPASS " + newPass);
      } else {
        std::cout << line << "\n";
      }
    }

    conn.sendLine("exit");
    conn.readLine(line); // "bye!" -- discard, one-shot mode doesn't echo it
    CLOSESOCK(sock);
    socketCleanup();
    return 0;
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

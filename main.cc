/**
 * Provides a simple implementation of an http proxy and web cache.  
 * Save for the parsing of the command line, all bucks are passed
 * to an HTTPProxy proxy instance, which repeatedly loops and waits 
 * for proxied requests to come through.
 */

#include <climits>  // for USHRT_MAX
#include <iostream> // for cerr
#include <sstream>  // for ostringstream
#include <csignal>  // for sigaction, SIG_IGN
#include <cstdlib>  // for getenv

#include "proxy.h"
#include "proxy-exception.h"
#include "ostreamlock.h"

using namespace std;

/**
 * Function: alertOfBrokenPipe
 * ---------------------------
 * Simple fault handler that occasionally gets invoked because
 * some pipe is broken.  We actually just print a short message but
 * otherwise allow execution to continue.
 */
static void alertOfBrokenPipe(int unused) {
  cerr << oslock << "Client closed socket.... aborting response." 
       << endl << osunlock;
}

/**
 * Function: killProxyServer
 * -------------------------
 * Simple fault handler that ends the program.
 */
static void killProxyServer(int unused) {
  cout << endl << "Shutting down http-proxy." << endl;
  exit(0);
}

/**
 * Function: handleBrokenPipes
 * ---------------------------
 * Configures the entire system to trivially handle all broken
 * pipes.  The alternative is to let a single broken pipe bring
 * down the entire proxy.
 */
static void handleBrokenPipes() {
  struct sigaction act;
  act.sa_handler = alertOfBrokenPipe;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  sigaction(SIGPIPE, &act, NULL);
}

/**
 * Function: handleKillRequests
 * ----------------------------
 * Configures the entire system to quit on 
 * ctrl-c and ctrl-z.
 */
static void handleKillRequests() {
  struct sigaction act;
  act.sa_handler = killProxyServer;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGTSTP, &act, NULL);
}

/**
 * Function: assertCorrectArgCount
 * -------------------------------
 * Confirms that two arguments were passed to main.  If not
 * an HTTPProxyException is thrown.
 */

static void assertCorrectArgCount(int argCount, const char *executableName) {
  if (argCount > 2) {
    ostringstream oss;
    oss << "Executable should be invoked as \"" 
	<< executableName << " [port]\".";
    throw HTTPProxyException(oss.str());
  }
}

/**
 * Function: computeDefaultPortForUser
 * -----------------------------------
 * Uses the loggedinuser ID to generate a port number between 1024
 * and USHRT_MAX inclusive.  The idea to hash the loggedinuser ID to
 * a number is in place so that users can launch the proxy to listen to
 * a port that, with very high probability, no other user is likely to
 * generate.
 */

static const unsigned short kLowestOpenPortNumber = 1024;
static unsigned short computeDefaultPortForUser() {
  string username = getenv("USER");
  size_t hashValue = hash<string>()(username);
  return hashValue % (USHRT_MAX - kLowestOpenPortNumber) + kLowestOpenPortNumber;
}

/**
 * Function: extractPortNumber
 * ---------------------------
 * Accepts the string form of the port number and converts it
 * to the unsigned short form and returns it.  If there are any
 * problems with the argument (e.g. it's not a number, or it's
 * too big or too small to every work), then an HTTPProxyException
 * is thrown.
 */
static unsigned short extractPortNumber(const char *portArgument) {
  char *endptr;
  long rawPort = strtol(portArgument, &endptr, 0);
  if (*endptr != '\0') {
    ostringstream oss;
    oss << "The supplied port number of \"" << portArgument << "\" is malformed.";
    throw HTTPProxyException(oss.str());
  }
  
  if (rawPort < 1 || rawPort > USHRT_MAX) {
    ostringstream oss;
    oss << "Port number must be between 1 and " << USHRT_MAX << ".";
    throw HTTPProxyException(oss.str());
  }
  
  return rawPort;
}

/**
 * Function: main
 * --------------
 * Defines the simple entry point to the entire proxy
 * application, which save for the configuration issues
 * passes the buck to an instance of the HTTPProxy class.
 */
int main(int argc, const char *argv[]) {
  handleKillRequests();
  handleBrokenPipes();
  try {
    assertCorrectArgCount(argc, argv[0]);
    unsigned short portNumber = 
      argc == 2 ? extractPortNumber(argv[1]) : computeDefaultPortForUser();
    HTTPProxy proxy(portNumber);
    cout << "Listening for all incoming traffic on port " << portNumber << "." << endl;
    while (true) {
      proxy.acceptAndProxyRequest();
    }
  } catch (const HTTPProxyException& hpe) {
    cerr << "Fatal Error: " << hpe.what() << endl;
    cerr << "Exiting..... " << endl;
    return 1;
  }
  
  return 0; // never gets here, but it feels wrong to not type it
}

/**
 * File: http-proxy.cc
 * -------------------
 * Presents the implementation of all HTTPProxy class methods as
 * defined in http-proxy.h.
 */

#include "proxy.h"

#include <cstring>
#include <sstream>
#include <string>

#include <sys/socket.h>
#include <sys/types.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "proxy-exception.h"
using namespace std;

/** Public constructor and methods **/

/**
 * Constructs the HTTPProxy so that it listens
 * for all traffic on the specified port number.
 * If there are any problems at all (e.g. listen
 * socket cannot be created, or it can't be bound
 * to the specified port number), then an HTTPProxyException
 * is thrown.
 */
static const int kUnitializedSocket = -1;
HTTPProxy::HTTPProxy(unsigned short portNumber) throw (HTTPProxyException) {
  this->portNumber = portNumber;
  listenfd = kUnitializedSocket;
  try {
    createServerSocket();
    configureServerSocket();
  } catch (const HTTPProxyException& hpe) {
    if (listenfd != kUnitializedSocket) {
      close(listenfd);
    }
    throw;
  }
}

/**
 * Method: acceptAndProxyRequest
 * -----------------------------
 * General umbrella method that blocks until a request
 * is detected.  When a request is detected, the IP address
 * of the requesting host is extracted, and the request is
 * passed on to the origin server (once the application has been
 * fully implemented, of course).
 */
void HTTPProxy::acceptAndProxyRequest() throw() {
  struct sockaddr_in clientAddr;
  socklen_t clientAddrSize = sizeof(clientAddr);
  int connectionfd = accept(listenfd, (struct sockaddr *) &clientAddr, &clientAddrSize);
  if (connectionfd < 0) {
    // connectionfd isn't open, so we're not orphaning any resources
    throw HTTPProxyException
      ("Call to accept failed to return a valid client socket.");
  }
  
  string clientIPAddress = getClientIPAddress(&clientAddr);
  scheduler.scheduleRequest(connectionfd, clientIPAddress);
}

/** Private methods **/

/**
 * Creates a server socket and configures it to
 * be closed more or less immediately if the surrounding
 * application dies or is killed.
 */
void HTTPProxy::createServerSocket() {
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd < 0) {
    throw HTTPProxyException
      ("Failed to open a primary socket to poll for connections.");
  }
  
  // the following configures the socket to be auto-closed within a
  // second if the surrounding process dies.  Otherwise, the socket might
  // not be available for up to a minute, and that makes iterative development
  // more difficult to manage if the easier way to kill the proxy server is
  // to just type Ctrl-C.
  const int optval = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval , sizeof(int));
}

/**
 * Further configures the server socket created via
 * createServerSocket so that it listens for activity from
 * any host whatsoever on the port number passed in to the
 * HTTPProxy constructor.  ::bind actually ties the socket
 * to the provided port number, and listen clarifies how many
 * pending connections can be queued up before the proxy starts
 * refusing connections.
 */
void HTTPProxy::configureServerSocket() const {
  struct sockaddr_in serverAddr;
  bzero(&serverAddr, sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serverAddr.sin_port = htons(portNumber);
  struct sockaddr *sa = (struct sockaddr *) &serverAddr;

  // ::bind needed to be clear that global bind in the C libs is 
  // being called instead of the std::bind from C++'s STL.
  if (::bind(listenfd, sa, sizeof(serverAddr)) < 0) {
    ostringstream oss;
    oss << "Failed to associate listening socket with port " << portNumber << ".";
    throw HTTPProxyException(oss.str());
  }
  
  const size_t kMaxQueuedRequests = 128; // 128 is the largest allowed value
  if (listen(listenfd, kMaxQueuedRequests) < 0) {
    throw HTTPProxyException
      ("Failed to set listening socket to accept connection requests");
  }
}

/**
 * Converts the IP addressed embedded within the supplied sockaddr
 * to C string form.
 */
const char *HTTPProxy::getClientIPAddress(const struct sockaddr_in *clientAddr) const {
  const char *clientIPAddr = inet_ntoa(clientAddr->sin_addr);
  if (clientIPAddr == NULL) {
    throw HTTPProxyException
      ("Failed to extract an IP address from the client connection.");
  }
  
  return clientIPAddr;
}

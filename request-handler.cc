/**
 * File: request-handler.cc
 * ------------------------
 * Provides the implementation for the HTTPRequestHandler class.
*/

#include "blacklist.h"
#include "request-handler.h"
#include "request.h"
#include "response.h"
#include "proxy-exception.h"
#include "cache.h"

#include <memory>
#include <mutex>
#include <netdb.h>
#include <iostream>              // for flush
#include <string>                // for string
#include "socket++/sockstream.h" // for sockbuf, iosockstream

#define kClientSocketError -1
#define BLACKLIST 403
using namespace std;

/* Allocates a socket for the connection */ 
int HTTPRequestHandler::createSocket(const string& host, unsigned short port) {
	int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) return kClientSocketError;

	struct hostent *he = gethostbyname(host.c_str());
	if (he == NULL) {
	  close(s);
		return kClientSocketError;
	}

	struct sockaddr_in serverAddress;
  memset(&serverAddress, 0, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(port);
	serverAddress.sin_addr.s_addr = ((struct in_addr *)he->h_addr)->s_addr;

	if (connect(s, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) != 0) {
		close(s);
		return kClientSocketError;
	}

	return s;
}

/*  sends appropriate messages for blacklist error */
void handleBlacklist(iosockstream& ss, HTTPRequest& request) {
	HTTPResponse response;
	response.setProtocol(request.getMethod());
	response.setResponseCode(BLACKLIST);
	response.setPayload("Forbidden Content"); 
	
	ss << response << flush;
}


/**
 * Services a request by posting a placeholder payload.  The implementation
 * of this method will change dramatically as you work toward your final milestone.
 */
void HTTPRequestHandler::serviceRequest(const pair<int, string>& connection) throw() {
	//Get the client request stream
	sockbuf clientSb(connection.first); 
	iosockstream clientSs(&clientSb);

	try {
		// process stream into request
		HTTPRequest request; 
		request.ingestRequestLine(clientSs);
		request.ingestHeader(clientSs, connection.second);
		request.ingestPayload(clientSs); 

		mutex& cacheLock = cache.getMutex(request);
	//	cout << request.getMethod() << " " << request.getURL() << endl;
		
		//block if server is on blacklist 
		if (!blacklist.serverIsAllowed(request.getServer())) {
			handleBlacklist(clientSs, request); 
			return;
		}

		// checks for cache
		HTTPResponse response;
		cacheLock.lock();
		if(cache.containsCacheEntry(request, response)) {
			cout << "loading cache" << endl;
			clientSs << response << flush;
			cacheLock.unlock();
			return;
		}

//		cout << "got here" << endl;
		// Establish a connection with the server
		int serverSocket = createSocket(request.getServer(), request.getPort()); 
		if (serverSocket == kClientSocketError) {
			cacheLock.unlock();
			throw HTTPRequestException("Failed to create Socket");
		}  
	
		// sends request to server
		sockbuf serverSb(serverSocket);
		iosockstream serverSs(&serverSb);
		serverSs << request << flush;

		// receive the response from server
		response.ingestResponseHeader(serverSs);
		response.ingestPayload(serverSs);

		// cache the entry if needed
		if (cache.shouldCache(request, response)) cache.cacheEntry(request, response);
		clientSs << response << flush;
		cacheLock.unlock();
	} catch (const HTTPBadRequestException& hpe) {
		cout << "Oops Bad request" << endl;
	} catch (const HTTPProxyException& hpe) {
		cout << "Oops" << endl;
	} catch (...) {
		cout << "unexpected error" << endl;
	}
}

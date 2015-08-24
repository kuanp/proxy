/**
 * File: scheduler.cc
 * ------------------
 * Presents the implementation of the HTTPProxyScheduler class.
 */

#include "scheduler.h"
#include <utility>      // for make_pair
#include <string>       // for string
using namespace std;

void HTTPProxyScheduler::worker(int workerID) {
	while(true) {
		workerSema[workerID].wait();

		workListLock.lock();
		pair<int, string> work = workList.front();
		workList.pop();
		workListLock.unlock();

		handler.serviceRequest(work);
		
		//worker is done. So inserts himself back and lets dispatcher know. 
		workerListLock.lock();
		availableWorkers.push(workerID);
		workerListLock.unlock();

		moreWorkerLeft.signal();
	}
}

void HTTPProxyScheduler::dispatcher() {
	while(true) {
		moreWorkLeft.wait();
		moreWorkerLeft.wait();

		workerListLock.lock();
		unsigned short freeWorkerID = availableWorkers.front();
		availableWorkers.pop();
		workerListLock.unlock();

		workerSema[freeWorkerID].signal();
	}
}

void HTTPProxyScheduler::scheduleRequest(int connectionfd, const string& clientIPAddress) {
	pair<int, string> work = make_pair(connectionfd, clientIPAddress);
	workList.push(work);
	moreWorkLeft.signal();
}

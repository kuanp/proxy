/**
 * File: http-proxy-scheduler.h
 * ----------------------------
 * This class defines the scheduler class, which takes all
 * proxied requests off of the main thread and schedules them to 
 * be handled by a constant number of child threads.
 */

#ifndef _http_proxy_scheduler_
#define _http_proxy_scheduler_

#include "request-handler.h"
#include "semaphore.h"
#include <string>
#include <mutex>
#include <queue>
#include <vector>
#include <utility>
#include <thread>
class HTTPProxyScheduler {
	public:
	HTTPProxyScheduler() : handler(), workList(), availableWorkers(), workerSema(16), moreWorkLeft(), moreWorkerLeft(), workListLock(), workerListLock() {
		// spawns dispatcher;
		std::thread dt([this]() -> void {
				this->dispatcher();
				});
		dt.detach();

		//spawns workers;
		for (unsigned short workerID = 0; workerID < 1; workerID++) {
			std::thread wt([this](unsigned short workerID) -> void {
					this->worker(workerID);
					}, workerID);
			wt.detach();
			moreWorkerLeft.signal();
			availableWorkers.push(workerID);
		}
	}

		void scheduleRequest(int connectionfd, const std::string& clientIPAddress);

	private:
		HTTPRequestHandler handler;

		std::queue<std::pair<int, std::string> > workList; 
		std::queue<unsigned short> availableWorkers;
		
		std::vector<semaphore> workerSema;

		semaphore moreWorkLeft;
		semaphore moreWorkerLeft;
		
		std::mutex workListLock;
		std::mutex workerListLock;


		void dispatcher();
		void worker(int workerID); 
		// some locks
};

#endif

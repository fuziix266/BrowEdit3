#pragma once

#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <json.hpp>

class BrowEdit;

class IpcServer
{
public:
	struct Request
	{
		SOCKET clientSocket;
		std::string rawJson;
	};

private:
	std::atomic<bool> running{ false };
	std::thread serverThread;
	SOCKET listenSocket = INVALID_SOCKET;
	int listenPort = 38200;

	std::mutex queueMutex;
	std::queue<Request> requestQueue;

	IpcServer();
	~IpcServer();

	void runServer();
	void handleClient(SOCKET client);

	nlohmann::json dispatchCommand(BrowEdit* browEdit, const std::string& method, const nlohmann::json& params);
	void sendResponse(SOCKET client, const nlohmann::json& response);

public:
	static IpcServer& instance();

	bool start(int port = 38200);
	void stop();

	// Process queued requests on the BrowEdit main OpenGL thread
	void processRequests(BrowEdit* browEdit);
};

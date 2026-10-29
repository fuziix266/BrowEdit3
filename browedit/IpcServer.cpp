#include "IpcServer.h"
#include <browedit/BrowEdit.h>
#include <browedit/Map.h>
#include <browedit/Node.h>
#include <browedit/MapView.h>
#include <browedit/actions/Action.h>
#include <browedit/actions/GroupAction.h>
#include <browedit/actions/NewObjectAction.h>
#include <browedit/actions/DeleteObjectAction.h>
#include <browedit/actions/ObjectChangeAction.h>
#include <browedit/actions/CubeHeightChangeAction.h>
#include <browedit/actions/GatTileChangeAction.h>
#include <browedit/actions/CubeTileChangeAction.h>
#include <browedit/components/Rsw.h>
#include <browedit/components/Gnd.h>
#include <browedit/components/Gat.h>
#include <browedit/components/Rsm.h>
#include <browedit/components/RsmRenderer.h>
#include <browedit/components/Collider.h>
#include <browedit/components/BillboardRenderer.h>
#include <browedit/components/GndRenderer.h>
#include <browedit/components/GatRenderer.h>
#include <browedit/components/WaterRenderer.h>
#include <browedit/util/ResourceManager.h>
#include <browedit/util/FileIO.h>
#include <browedit/util/Util.h>

#include <iostream>
#include <sstream>
#include <algorithm>

IpcServer::IpcServer()
{
}

IpcServer::~IpcServer()
{
	stop();
}

IpcServer& IpcServer::instance()
{
	static IpcServer server;
	return server;
}

bool IpcServer::start(int port)
{
	if (running)
		return true;

	listenPort = port;
	running = true;

	WSADATA wsaData;
	int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (res != 0)
	{
		std::cerr << "[IpcServer] WSAStartup failed: " << res << std::endl;
		running = false;
		return false;
	}

	listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSocket == INVALID_SOCKET)
	{
		std::cerr << "[IpcServer] socket failed: " << WSAGetLastError() << std::endl;
		WSACleanup();
		running = false;
		return false;
	}

	BOOL opt = TRUE;
	setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

	sockaddr_in service;
	service.sin_family = AF_INET;
	InetPtonA(AF_INET, "127.0.0.1", &service.sin_addr);
	service.sin_port = htons((u_short)listenPort);

	if (bind(listenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR)
	{
		std::cerr << "[IpcServer] bind failed on port " << listenPort << ": " << WSAGetLastError() << std::endl;
		closesocket(listenSocket);
		WSACleanup();
		running = false;
		return false;
	}

	if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		std::cerr << "[IpcServer] listen failed: " << WSAGetLastError() << std::endl;
		closesocket(listenSocket);
		WSACleanup();
		running = false;
		return false;
	}

	std::cout << "[IpcServer] BrowEdit3 MCP Bridge listening on 127.0.0.1:" << listenPort << std::endl;

	serverThread = std::thread(&IpcServer::runServer, this);
	return true;
}

void IpcServer::stop()
{
	if (!running)
		return;

	running = false;
	if (listenSocket != INVALID_SOCKET)
	{
		closesocket(listenSocket);
		listenSocket = INVALID_SOCKET;
	}

	if (serverThread.joinable())
		serverThread.join();

	WSACleanup();
	std::cout << "[IpcServer] BrowEdit3 MCP Bridge stopped." << std::endl;
}

void IpcServer::runServer()
{
	while (running)
	{
		sockaddr_in clientAddr;
		int clientAddrLen = sizeof(clientAddr);
		SOCKET client = accept(listenSocket, (SOCKADDR*)&clientAddr, &clientAddrLen);
		if (client == INVALID_SOCKET)
		{
			if (!running)
				break;
			continue;
		}

		std::thread clientThread(&IpcServer::handleClient, this, client);
		clientThread.detach();
	}
}

void IpcServer::handleClient(SOCKET client)
{
	std::string buffer;
	char chunk[4096];

	while (running)
	{
		int bytesRead = recv(client, chunk, sizeof(chunk), 0);
		if (bytesRead <= 0)
			break;

		buffer.append(chunk, bytesRead);

		size_t pos = 0;
		while ((pos = buffer.find('\n')) != std::string::npos)
		{
			std::string line = buffer.substr(0, pos);
			buffer.erase(0, pos + 1);

			// Trim carriage return if present
			if (!line.empty() && line.back() == '\r')
				line.pop_back();

			if (line.empty())
				continue;

			{
				std::lock_guard<std::mutex> lock(queueMutex);
				requestQueue.push(Request{ client, line });
			}
		}
	}

	closesocket(client);
}

void IpcServer::sendResponse(SOCKET client, const nlohmann::json& response)
{
	try
	{
		std::string str = response.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace) + "\n";
		send(client, str.c_str(), (int)str.length(), 0);
	}
	catch (const std::exception& e)
	{
		std::string errStr = "{\"id\":0,\"error\":\"Serialization error: " + std::string(e.what()) + "\"}\n";
		send(client, errStr.c_str(), (int)errStr.length(), 0);
	}
	catch (...)
	{
		std::string errStr = "{\"id\":0,\"error\":\"Unknown serialization error\"}\n";
		send(client, errStr.c_str(), (int)errStr.length(), 0);
	}
}

static Node* findNodeRecursive(Node* current, const std::string& name)
{
	if (!current)
		return nullptr;
	if (current->name == name)
		return current;
	for (auto child : current->children)
	{
		Node* found = findNodeRecursive(child, name);
		if (found)
			return found;
	}
	return nullptr;
}

void IpcServer::processRequests(BrowEdit* browEdit)
{
	std::vector<Request> requests;
	{
		std::lock_guard<std::mutex> lock(queueMutex);
		while (!requestQueue.empty())
		{
			requests.push_back(requestQueue.front());
			requestQueue.pop();
		}
	}

	for (const auto& req : requests)
	{
		nlohmann::json response;
		int id = 0;

		try
		{
			auto jReq = nlohmann::json::parse(req.rawJson);
			if (jReq.contains("id") && jReq["id"].is_number())
				id = jReq["id"].get<int>();

			std::string method = jReq.value("method", "");
			nlohmann::json params = jReq.value("params", nlohmann::json::object());

			nlohmann::json result = dispatchCommand(browEdit, method, params);
			response["id"] = id;
			response["result"] = result;
		}
		catch (const std::exception& e)
		{
			response["id"] = id;
			response["error"] = e.what();
		}
		catch (...)
		{
			response["id"] = id;
			response["error"] = "Unknown internal exception in BrowEdit3.";
		}

		sendResponse(req.clientSocket, response);
	}
}

nlohmann::json IpcServer::dispatchCommand(BrowEdit* browEdit, const std::string& method, const nlohmann::json& params)
{
	nlohmann::json res;

	// 1. PING & STATUS
	if (method == "ping")
	{
		res["status"] = "pong";
		res["app"] = "BrowEdit3";
		res["version"] = "3.0-mcp";
		return res;
	}

	if (method == "get_status")
	{
		res["mapCount"] = browEdit->maps.size();
		res["activeMap"] = browEdit->activeMapView && browEdit->activeMapView->map ? browEdit->activeMapView->map->name : "";
		nlohmann::json mapsArr = nlohmann::json::array();
		for (auto m : browEdit->maps)
			mapsArr.push_back(m->name);
		res["maps"] = mapsArr;
		return res;
	}

	// 2. MAP MANAGEMENT
	if (method == "open_map")
	{
		std::string file = params.value("file", "");
		if (file.empty())
			throw std::runtime_error("Param 'file' is required.");

		browEdit->loadMap(file);
		if (!browEdit->mapViews.empty())
			browEdit->activeMapView = &browEdit->mapViews.back();
		res["status"] = "ok";
		res["message"] = "Opening map: " + file;
		return res;
	}

	if (method == "save_map")
	{
		if (!browEdit->activeMapView || !browEdit->activeMapView->map)
			throw std::runtime_error("No active map open to save.");

		browEdit->saveMap(browEdit->activeMapView->map);
		res["status"] = "ok";
		res["message"] = "Saved active map: " + browEdit->activeMapView->map->name;
		return res;
	}

	if (method == "new_map")
	{
		std::string name = params.value("name", "new_map");
		int width = params.value("width", 50);
		int height = params.value("height", 50);

		std::string rswName = name;
		if (rswName.length() < 4 || rswName.substr(rswName.length() - 4) != ".rsw")
			rswName += ".rsw";
		if (rswName.substr(0, 5) != "data\\")
			rswName = "data\\" + rswName;

		auto map = new Map(rswName, width, height, browEdit);
		browEdit->maps.push_back(map);
		browEdit->mapViews.push_back(MapView(map, rswName + "#0"));
		browEdit->activeMapView = &browEdit->mapViews.back();

		res["status"] = "ok";
		res["name"] = name;
		res["width"] = width;
		res["height"] = height;
		return res;
	}

	// Active map validation for subsequent commands
	Map* map = browEdit->activeMapView ? browEdit->activeMapView->map : nullptr;
	if (!map && !browEdit->maps.empty())
	{
		map = browEdit->maps.back();
		if (!browEdit->mapViews.empty())
			browEdit->activeMapView = &browEdit->mapViews.back();
	}

	if (!map)
		throw std::runtime_error("No map is currently loaded in BrowEdit3.");

	// 3. MAP INFO
	if (method == "get_map_info")
	{
		res["name"] = map->name;

		auto gnd = map->rootNode->getComponent<Gnd>();
		auto gat = map->rootNode->getComponent<Gat>();
		auto rsw = map->rootNode->getComponent<Rsw>();

		if (gnd)
		{
			res["width"] = gnd->width;
			res["height"] = gnd->height;
			res["textureCount"] = gnd->textures.size();
			nlohmann::json textures = nlohmann::json::array();
			for (const auto& t : gnd->textures)
				if (t) textures.push_back(t->file);
			res["textures"] = textures;
		}

		if (gat)
		{
			res["gatWidth"] = gat->width;
			res["gatHeight"] = gat->height;
		}

		if (rsw && !rsw->water.zones.empty() && !rsw->water.zones[0].empty())
		{
			res["waterHeight"] = rsw->water.zones[0][0].height;
			res["waterType"] = rsw->water.zones[0][0].type;
		}

		// Count objects in hierarchy
		int totalObjects = 0;
		int modelsCount = 0;
		int lightsCount = 0;
		int effectsCount = 0;
		int soundsCount = 0;

		map->rootNode->traverse([&](Node* n)
		{
			if (n == map->rootNode) return;
			totalObjects++;
			if (n->getComponent<RswModel>()) modelsCount++;
			if (n->getComponent<RswLight>()) lightsCount++;
			if (n->getComponent<RswEffect>()) effectsCount++;
			if (n->getComponent<RswSound>()) soundsCount++;
		});

		res["objectsTotal"] = totalObjects;
		res["modelsCount"] = modelsCount;
		res["lightsCount"] = lightsCount;
		res["effectsCount"] = effectsCount;
		res["soundsCount"] = soundsCount;

		return res;
	}

	// 4. CAMERA
	if (method == "get_camera")
	{
		if (!browEdit->activeMapView)
			throw std::runtime_error("No active MapView.");

		auto mv = browEdit->activeMapView;
		res["center"] = { mv->cameraCenter.x, mv->cameraCenter.y, mv->cameraCenter.z };
		res["rotation"] = { mv->cameraRot.x, mv->cameraRot.y };
		res["distance"] = mv->cameraDistance;
		return res;
	}

	if (method == "set_camera")
	{
		if (!browEdit->activeMapView)
			throw std::runtime_error("No active MapView.");

		auto mv = browEdit->activeMapView;
		if (params.contains("center") && params["center"].is_array() && params["center"].size() == 3)
			mv->cameraCenter = glm::vec3(params["center"][0].get<float>(), params["center"][1].get<float>(), params["center"][2].get<float>());

		if (params.contains("rotation") && params["rotation"].is_array() && params["rotation"].size() == 2)
			mv->cameraRot = glm::vec2(params["rotation"][0].get<float>(), params["rotation"][1].get<float>());

		if (params.contains("distance") && params["distance"].is_number())
			mv->cameraDistance = params["distance"].get<float>();

		res["status"] = "ok";
		return res;
	}

	// 5. OBJECTS INSPECTION
	if (method == "list_objects")
	{
		std::string filterType = params.value("type", "all");
		int limit = params.value("limit", 100);
		int offset = params.value("offset", 0);
		std::string search = params.value("search", "");

		nlohmann::json objList = nlohmann::json::array();
		int currentIndex = 0;

		map->rootNode->traverse([&](Node* n)
		{
			if (n == map->rootNode) return;

			auto rswObj = n->getComponent<RswObject>();
			auto rswModel = n->getComponent<RswModel>();
			auto rswLight = n->getComponent<RswLight>();
			auto rswEffect = n->getComponent<RswEffect>();
			auto rswSound = n->getComponent<RswSound>();

			std::string type = "unknown";
			if (rswModel) type = "model";
			else if (rswLight) type = "light";
			else if (rswEffect) type = "effect";
			else if (rswSound) type = "sound";

			if (filterType != "all" && filterType != type)
				return;

			if (!search.empty() && n->name.find(search) == std::string::npos)
			{
				if (rswModel && rswModel->fileName.find(search) == std::string::npos)
					return;
			}

			if (currentIndex >= offset && (int)objList.size() < limit)
			{
				nlohmann::json obj;
				obj["name"] = n->name;
				obj["type"] = type;

				if (rswObj)
				{
					obj["position"] = { rswObj->position.x, rswObj->position.y, rswObj->position.z };
					obj["rotation"] = { rswObj->rotation.x, rswObj->rotation.y, rswObj->rotation.z };
					obj["scale"] = { rswObj->scale.x, rswObj->scale.y, rswObj->scale.z };
				}

				if (rswModel)
					obj["modelFile"] = rswModel->fileName;

				if (rswLight)
				{
					obj["color"] = { rswLight->color.r, rswLight->color.g, rswLight->color.b };
					obj["range"] = rswLight->range;
					obj["intensity"] = rswLight->intensity;
				}

				if (rswEffect)
					obj["effectId"] = rswEffect->id;

				if (rswSound)
				{
					obj["soundFile"] = rswSound->fileName;
					obj["volume"] = rswSound->vol;
					obj["range"] = rswSound->range;
				}

				objList.push_back(obj);
			}

			currentIndex++;
		});

		res["totalFound"] = currentIndex;
		res["objects"] = objList;
		return res;
	}

	if (method == "get_object")
	{
		std::string name = params.value("name", "");
		if (name.empty())
			throw std::runtime_error("Param 'name' is required.");

		Node* node = findNodeRecursive(map->rootNode, name);
		if (!node)
			throw std::runtime_error("Object not found: " + name);

		res["name"] = node->name;
		auto rswObj = node->getComponent<RswObject>();
		if (rswObj)
		{
			res["position"] = { rswObj->position.x, rswObj->position.y, rswObj->position.z };
			res["rotation"] = { rswObj->rotation.x, rswObj->rotation.y, rswObj->rotation.z };
			res["scale"] = { rswObj->scale.x, rswObj->scale.y, rswObj->scale.z };
		}

		auto rswModel = node->getComponent<RswModel>();
		if (rswModel)
		{
			res["type"] = "model";
			res["modelFile"] = rswModel->fileName;
		}

		auto rswLight = node->getComponent<RswLight>();
		if (rswLight)
		{
			res["type"] = "light";
			res["color"] = { rswLight->color.r, rswLight->color.g, rswLight->color.b };
			res["range"] = rswLight->range;
			res["intensity"] = rswLight->intensity;
		}

		auto rswEffect = node->getComponent<RswEffect>();
		if (rswEffect)
		{
			res["type"] = "effect";
			res["effectId"] = rswEffect->id;
		}

		auto rswSound = node->getComponent<RswSound>();
		if (rswSound)
		{
			res["type"] = "sound";
			res["soundFile"] = rswSound->fileName;
			res["volume"] = rswSound->vol;
			res["range"] = rswSound->range;
		}

		return res;
	}

	// 6. OBJECT CREATION & MUTATION
	if (method == "spawn_model")
	{
		std::string modelFile = params.value("model", "");
		if (modelFile.empty())
			throw std::runtime_error("Param 'model' is required (e.g. 'data\\model\\prontera\\tree.rsm').");

		std::string name = params.value("name", "");
		if (name.empty())
		{
			std::string base = modelFile;
			if (base.find_last_of("\\/") != std::string::npos)
				base = base.substr(base.find_last_of("\\/") + 1);
			name = base;
		}

		glm::vec3 pos(0.0f);
		if (params.contains("position") && params["position"].is_array() && params["position"].size() == 3)
			pos = glm::vec3(params["position"][0].get<float>(), params["position"][1].get<float>(), params["position"][2].get<float>());

		glm::vec3 rot(0.0f);
		if (params.contains("rotation") && params["rotation"].is_array() && params["rotation"].size() == 3)
			rot = glm::vec3(params["rotation"][0].get<float>(), params["rotation"][1].get<float>(), params["rotation"][2].get<float>());

		glm::vec3 scale(1.0f);
		if (params.contains("scale") && params["scale"].is_array() && params["scale"].size() == 3)
			scale = glm::vec3(params["scale"][0].get<float>(), params["scale"][1].get<float>(), params["scale"][2].get<float>());

		Node* newNode = new Node(name);

		auto rswObj = new RswObject();
		rswObj->position = pos;
		rswObj->rotation = rot;
		rswObj->scale = scale;
		newNode->addComponent(rswObj);

		auto rswModel = new RswModel();
		rswModel->fileName = modelFile;
		newNode->addComponent(rswModel);

		std::string fullModelPath = modelFile;
		if (fullModelPath.substr(0, 11) != "data\\model\\")
			fullModelPath = "data\\model\\" + fullModelPath;

		newNode->addComponent(util::ResourceManager<Rsm>::load(util::utf8_to_iso_8859_1(fullModelPath)));
		newNode->addComponent(new RsmRenderer());
		newNode->addComponent(new RswModelCollider());

		newNode->setParent(map->findAndBuildNode(""));
		newNode->makeNameUnique(map->rootNode);

		map->doAction(new NewObjectAction(newNode), browEdit);

		res["status"] = "ok";
		res["name"] = newNode->name;
		res["position"] = { pos.x, pos.y, pos.z };
		return res;
	}

	if (method == "spawn_light")
	{
		std::string name = params.value("name", "Light");
		glm::vec3 pos(0.0f);
		if (params.contains("position") && params["position"].is_array() && params["position"].size() == 3)
			pos = glm::vec3(params["position"][0].get<float>(), params["position"][1].get<float>(), params["position"][2].get<float>());

		glm::vec3 color(1.0f, 1.0f, 1.0f);
		if (params.contains("color") && params["color"].is_array() && params["color"].size() == 3)
			color = glm::vec3(params["color"][0].get<float>(), params["color"][1].get<float>(), params["color"][2].get<float>());

		float range = params.value("range", 50.0f);
		float intensity = params.value("intensity", 1.0f);

		Node* newNode = new Node(name);

		auto rswObj = new RswObject();
		rswObj->position = pos;
		newNode->addComponent(rswObj);

		auto rswLight = new RswLight();
		rswLight->color = color;
		rswLight->range = range;
		rswLight->intensity = intensity;
		newNode->addComponent(rswLight);
		newNode->addComponent(new BillboardRenderer("data\\light.png", "data\\light_selected.png"));
		newNode->addComponent(new CubeCollider(5));

		newNode->setParent(map->findAndBuildNode(""));
		newNode->makeNameUnique(map->rootNode);

		map->doAction(new NewObjectAction(newNode), browEdit);

		res["status"] = "ok";
		res["name"] = newNode->name;
		return res;
	}

	if (method == "spawn_effect")
	{
		std::string name = params.value("name", "Effect");
		int effectId = params.value("effectId", 0);
		glm::vec3 pos(0.0f);
		if (params.contains("position") && params["position"].is_array() && params["position"].size() == 3)
			pos = glm::vec3(params["position"][0].get<float>(), params["position"][1].get<float>(), params["position"][2].get<float>());

		Node* newNode = new Node(name);

		auto rswObj = new RswObject();
		rswObj->position = pos;
		newNode->addComponent(rswObj);

		auto rswEffect = new RswEffect();
		rswEffect->id = effectId;
		newNode->addComponent(rswEffect);
		newNode->addComponent(new BillboardRenderer("data\\effect.png", "data\\effect_selected.png"));
		newNode->addComponent(new CubeCollider(5));

		newNode->setParent(map->findAndBuildNode(""));
		newNode->makeNameUnique(map->rootNode);

		map->doAction(new NewObjectAction(newNode), browEdit);

		res["status"] = "ok";
		res["name"] = newNode->name;
		return res;
	}

	if (method == "spawn_sound")
	{
		std::string name = params.value("name", "Sound");
		std::string file = params.value("file", "");
		if (file.empty())
			throw std::runtime_error("Param 'file' is required (e.g. 'data\\wav\\water.wav').");

		glm::vec3 pos(0.0f);
		if (params.contains("position") && params["position"].is_array() && params["position"].size() == 3)
			pos = glm::vec3(params["position"][0].get<float>(), params["position"][1].get<float>(), params["position"][2].get<float>());

		float vol = params.value("volume", 0.7f);
		float range = params.value("range", 100.0f);
		float cycle = params.value("cycle", 4.0f);

		Node* newNode = new Node(name);

		auto rswObj = new RswObject();
		rswObj->position = pos;
		newNode->addComponent(rswObj);

		auto rswSound = new RswSound();
		rswSound->fileName = file;
		rswSound->vol = vol;
		rswSound->range = range;
		rswSound->cycle = cycle;
		newNode->addComponent(rswSound);
		newNode->addComponent(new BillboardRenderer("data\\sound.png", "data\\sound_selected.png"));
		newNode->addComponent(new CubeCollider(5));

		newNode->setParent(map->findAndBuildNode(""));
		newNode->makeNameUnique(map->rootNode);

		map->doAction(new NewObjectAction(newNode), browEdit);

		res["status"] = "ok";
		res["name"] = newNode->name;
		return res;
	}

	if (method == "delete_object")
	{
		std::string name = params.value("name", "");
		if (name.empty())
			throw std::runtime_error("Param 'name' is required.");

		Node* node = findNodeRecursive(map->rootNode, name);
		if (!node)
			throw std::runtime_error("Object not found: " + name);

		map->doAction(new DeleteObjectAction(node), browEdit);
		res["status"] = "ok";
		res["message"] = "Deleted object: " + name;
		return res;
	}

	if (method == "transform_object")
	{
		std::string name = params.value("name", "");
		if (name.empty())
			throw std::runtime_error("Param 'name' is required.");

		Node* node = findNodeRecursive(map->rootNode, name);
		if (!node)
			throw std::runtime_error("Object not found: " + name);

		auto rswObj = node->getComponent<RswObject>();
		if (!rswObj)
			throw std::runtime_error("Object does not have spatial transform: " + name);

		auto ga = new GroupAction("Transform " + name);

		if (params.contains("position") && params["position"].is_array() && params["position"].size() == 3)
		{
			glm::vec3 newPos(params["position"][0].get<float>(), params["position"][1].get<float>(), params["position"][2].get<float>());
			glm::vec3 oldPos = rswObj->position;
			rswObj->position = newPos;
			ga->addAction(new ObjectChangeAction<glm::vec3>(node, &rswObj->position, oldPos, "Move"));
		}

		if (params.contains("rotation") && params["rotation"].is_array() && params["rotation"].size() == 3)
		{
			glm::vec3 newRot(params["rotation"][0].get<float>(), params["rotation"][1].get<float>(), params["rotation"][2].get<float>());
			glm::vec3 oldRot = rswObj->rotation;
			rswObj->rotation = newRot;
			ga->addAction(new ObjectChangeAction<glm::vec3>(node, &rswObj->rotation, oldRot, "Rotate"));
		}

		if (params.contains("scale") && params["scale"].is_array() && params["scale"].size() == 3)
		{
			glm::vec3 newScale(params["scale"][0].get<float>(), params["scale"][1].get<float>(), params["scale"][2].get<float>());
			glm::vec3 oldScale = rswObj->scale;
			rswObj->scale = newScale;
			ga->addAction(new ObjectChangeAction<glm::vec3>(node, &rswObj->scale, oldScale, "Scale"));
		}

		map->doAction(ga, browEdit);
		res["status"] = "ok";
		return res;
	}

	if (method == "batch_spawn_models")
	{
		if (!params.contains("models") || !params["models"].is_array())
			throw std::runtime_error("Param 'models' array is required.");

		auto ga = new GroupAction("MCP: Batch Spawn Models");
		int count = 0;

		for (const auto& item : params["models"])
		{
			std::string modelFile = item.value("model", "");
			if (modelFile.empty()) continue;

			std::string name = item.value("name", "");
			if (name.empty())
			{
				std::string base = modelFile;
				if (base.find_last_of("\\/") != std::string::npos)
					base = base.substr(base.find_last_of("\\/") + 1);
				name = base;
			}

			glm::vec3 pos(0.0f);
			if (item.contains("position") && item["position"].is_array() && item["position"].size() == 3)
				pos = glm::vec3(item["position"][0].get<float>(), item["position"][1].get<float>(), item["position"][2].get<float>());

			glm::vec3 rot(0.0f);
			if (item.contains("rotation") && item["rotation"].is_array() && item["rotation"].size() == 3)
				rot = glm::vec3(item["rotation"][0].get<float>(), item["rotation"][1].get<float>(), item["rotation"][2].get<float>());

			glm::vec3 scale(1.0f);
			if (item.contains("scale") && item["scale"].is_array() && item["scale"].size() == 3)
				scale = glm::vec3(item["scale"][0].get<float>(), item["scale"][1].get<float>(), item["scale"][2].get<float>());

			Node* newNode = new Node(name);

			auto rswObj = new RswObject();
			rswObj->position = pos;
			rswObj->rotation = rot;
			rswObj->scale = scale;
			newNode->addComponent(rswObj);

			auto rswModel = new RswModel();
			rswModel->fileName = modelFile;
			newNode->addComponent(rswModel);

			std::string fullModelPath = modelFile;
			if (fullModelPath.substr(0, 11) != "data\\model\\")
				fullModelPath = "data\\model\\" + fullModelPath;

			newNode->addComponent(util::ResourceManager<Rsm>::load(util::utf8_to_iso_8859_1(fullModelPath)));
			newNode->addComponent(new RsmRenderer());
			newNode->addComponent(new RswModelCollider());

			newNode->setParent(map->findAndBuildNode(""));
			newNode->makeNameUnique(map->rootNode);

			ga->addAction(new NewObjectAction(newNode));
			count++;
		}

		map->doAction(ga, browEdit);
		res["status"] = "ok";
		res["count"] = count;
		return res;
	}

	// 7. TERRAIN / HEIGHTMAP (GND)
	auto gnd = map->rootNode->getComponent<Gnd>();
	auto gndRenderer = map->rootNode->getComponent<GndRenderer>();

	if (method == "get_tile")
	{
		if (!gnd)
			throw std::runtime_error("Map has no GND component.");

		int x = params.value("x", -1);
		int y = params.value("y", -1);

		if (!gnd->inMap(glm::ivec2(x, y)))
			throw std::runtime_error("Coordinates out of map bounds.");

		auto cube = gnd->cubes[x][y];
		res["x"] = x;
		res["y"] = y;
		res["heights"] = { cube->h1, cube->h2, cube->h3, cube->h4 };
		res["tileUp"] = cube->tileUp;
		res["tileFront"] = cube->tileFront;
		res["tileSide"] = cube->tileSide;

		auto gat = map->rootNode->getComponent<Gat>();
		if (gat && x * 2 < gat->width && y * 2 < gat->height)
		{
			// RO GAT has 2x2 cells per GND cube
			nlohmann::json gatCells = nlohmann::json::array();
			for (int gy = y * 2; gy < y * 2 + 2 && gy < gat->height; gy++)
			{
				for (int gx = x * 2; gx < x * 2 + 2 && gx < gat->width; gx++)
				{
					auto gc = gat->cubes[gx][gy];
					nlohmann::json gCell;
					gCell["gx"] = gx;
					gCell["gy"] = gy;
					gCell["type"] = gc->gatType;
					gCell["heights"] = { gc->h1, gc->h2, gc->h3, gc->h4 };
					gatCells.push_back(gCell);
				}
			}
			res["gatCells"] = gatCells;
		}

		return res;
	}

	if (method == "set_tile_height")
	{
		if (!gnd)
			throw std::runtime_error("Map has no GND component.");

		int x = params.value("x", -1);
		int y = params.value("y", -1);

		if (!gnd->inMap(glm::ivec2(x, y)))
			throw std::runtime_error("Coordinates out of map bounds.");

		auto cube = gnd->cubes[x][y];

		std::map<Gnd::Cube*, float[4]> oldVals;
		std::map<Gnd::Cube*, float[4]> newVals;

		for (int i = 0; i < 4; i++)
			oldVals[cube][i] = cube->heights[i];

		if (params.contains("height") && params["height"].is_number())
		{
			float h = params["height"].get<float>();
			for (int i = 0; i < 4; i++)
				newVals[cube][i] = h;
		}
		else if (params.contains("heights") && params["heights"].is_array() && params["heights"].size() == 4)
		{
			for (int i = 0; i < 4; i++)
				newVals[cube][i] = params["heights"][i].get<float>();
		}
		else
		{
			throw std::runtime_error("Provide 'height' (float) or 'heights' (array of 4 floats).");
		}

		std::vector<glm::ivec2> selection = { glm::ivec2(x, y) };
		auto action = new CubeHeightChangeAction<Gnd, Gnd::Cube>(oldVals, newVals, selection);
		map->doAction(action, browEdit);

		if (gndRenderer)
		{
			gndRenderer->setChunkDirty(x, y);
			gndRenderer->setChunksDirty();
		}

		res["status"] = "ok";
		return res;
	}

	if (method == "set_area_height")
	{
		if (!gnd)
			throw std::runtime_error("Map has no GND component.");

		int minX = params.value("minX", 0);
		int minY = params.value("minY", 0);
		int maxX = params.value("maxX", 0);
		int maxY = params.value("maxY", 0);
		std::string mode = params.value("mode", "flatten"); // "flatten", "offset", "smooth"
		float value = params.value("value", 0.0f);

		minX = std::max(0, std::min(minX, gnd->width - 1));
		minY = std::max(0, std::min(minY, gnd->height - 1));
		maxX = std::max(0, std::min(maxX, gnd->width - 1));
		maxY = std::max(0, std::min(maxY, gnd->height - 1));

		if (minX > maxX) std::swap(minX, maxX);
		if (minY > maxY) std::swap(minY, maxY);

		std::map<Gnd::Cube*, float[4]> oldVals;
		std::map<Gnd::Cube*, float[4]> newVals;
		std::vector<glm::ivec2> selection;

		for (int cy = minY; cy <= maxY; cy++)
		{
			for (int cx = minX; cx <= maxX; cx++)
			{
				auto cube = gnd->cubes[cx][cy];
				selection.push_back(glm::ivec2(cx, cy));
				for (int i = 0; i < 4; i++)
				{
					oldVals[cube][i] = cube->heights[i];
					if (mode == "flatten")
						newVals[cube][i] = value;
					else if (mode == "offset")
						newVals[cube][i] = cube->heights[i] + value;
					else
						newVals[cube][i] = cube->heights[i];
				}
			}
		}

		auto action = new CubeHeightChangeAction<Gnd, Gnd::Cube>(oldVals, newVals, selection);
		map->doAction(action, browEdit);

		if (gndRenderer)
			gndRenderer->setChunksDirty();

		res["status"] = "ok";
		res["tilesModified"] = selection.size();
		return res;
	}

	// 8. GAT WALKABILITY
	auto gat = map->rootNode->getComponent<Gat>();
	auto gatRenderer = map->rootNode->getComponent<GatRenderer>();

	if (method == "get_gat")
	{
		if (!gat)
			throw std::runtime_error("Map has no GAT component.");

		int x = params.value("x", -1);
		int y = params.value("y", -1);

		if (!gat->inMap(glm::ivec2(x, y)))
			throw std::runtime_error("Coordinates out of GAT bounds.");

		auto cube = gat->cubes[x][y];
		res["x"] = x;
		res["y"] = y;
		res["gatType"] = cube->gatType;
		res["heights"] = { cube->h1, cube->h2, cube->h3, cube->h4 };
		return res;
	}

	if (method == "set_gat")
	{
		if (!gat)
			throw std::runtime_error("Map has no GAT component.");

		int type = params.value("type", 0);

		if (params.contains("minX") && params.contains("minY") && params.contains("maxX") && params.contains("maxY"))
		{
			int minX = params["minX"].get<int>();
			int minY = params["minY"].get<int>();
			int maxX = params["maxX"].get<int>();
			int maxY = params["maxY"].get<int>();

			minX = std::max(0, std::min(minX, gat->width - 1));
			minY = std::max(0, std::min(minY, gat->height - 1));
			maxX = std::max(0, std::min(maxX, gat->width - 1));
			maxY = std::max(0, std::min(maxY, gat->height - 1));

			if (minX > maxX) std::swap(minX, maxX);
			if (minY > maxY) std::swap(minY, maxY);

			auto ga = new GroupAction("MCP: Set GAT Area");
			int count = 0;

			for (int gy = minY; gy <= maxY; gy++)
			{
				for (int gx = minX; gx <= maxX; gx++)
				{
					auto cube = gat->cubes[gx][gy];
					if (cube->gatType != type)
					{
						ga->addAction(new GatTileChangeAction(glm::ivec2(gx, gy), cube, type));
						count++;
					}
				}
			}

			map->doAction(ga, browEdit);
			if (gatRenderer)
				gatRenderer->setChunksDirty();

			res["status"] = "ok";
			res["cellsModified"] = count;
			return res;
		}
		else
		{
			int x = params.value("x", -1);
			int y = params.value("y", -1);

			if (!gat->inMap(glm::ivec2(x, y)))
				throw std::runtime_error("Coordinates out of GAT bounds.");

			auto cube = gat->cubes[x][y];
			map->doAction(new GatTileChangeAction(glm::ivec2(x, y), cube, type), browEdit);

			if (gatRenderer)
				gatRenderer->setChunksDirty();

			res["status"] = "ok";
			return res;
		}
	}

	if (method == "auto_gat")
	{
		if (!gat || !gnd)
			throw std::runtime_error("Map must have both GND and GAT components.");

		float maxSlope = params.value("maxSlope", 20.0f); // Height diff threshold
		bool waterBlocked = params.value("waterBlocked", true);

		auto rsw = map->rootNode->getComponent<Rsw>();
		float waterHeight = (rsw && !rsw->water.zones.empty() && !rsw->water.zones[0].empty()) ? rsw->water.zones[0][0].height : 0.0f;

		auto ga = new GroupAction("MCP: Auto GAT Calculation");
		int modified = 0;

		for (int gy = 0; gy < gat->height; gy++)
		{
			for (int gx = 0; gx < gat->width; gx++)
			{
				auto cube = gat->cubes[gx][gy];
				int targetType = 0; // Walkable

				// Check height variation across corners
				float minH = std::min({ cube->h1, cube->h2, cube->h3, cube->h4 });
				float maxH = std::max({ cube->h1, cube->h2, cube->h3, cube->h4 });
				float diff = maxH - minH;

				if (diff > maxSlope)
				{
					targetType = 1; // Non-walkable cliff
				}
				else if (waterBlocked && maxH > waterHeight) // In RO, positive Y is down in some coords or water level comparison
				{
					// If underwater
					if (rsw && cube->h1 >= waterHeight)
						targetType = 2; // Non-walkable water
				}

				if (cube->gatType != targetType)
				{
					ga->addAction(new GatTileChangeAction(glm::ivec2(gx, gy), cube, targetType));
					modified++;
				}
			}
		}

		map->doAction(ga, browEdit);
		if (gatRenderer)
			gatRenderer->setChunksDirty();

		res["status"] = "ok";
		res["cellsModified"] = modified;
		return res;
	}

	if (method == "rotate_area")
	{
		int gMinX = params.value("gndMinX", 36);
		int gMaxX = params.value("gndMaxX", 99);
		int gMinY = params.value("gndMinY", 95);
		int gMaxY = params.value("gndMaxY", 158);

		float rswCx = params.value("rswCenterX", -105.0f);
		float rswCz = params.value("rswCenterZ", 295.0f);

		int objectsRotated = 0;
		int tilesRotated = 0;
		int gatCellsRotated = 0;

		// 1. ROTATE RSW OBJECTS (models, lights, etc.)
		if (params.value("rotateObjects", true))
		{
			float minRswX = params.value("minRswX", -450.0f);
			float maxRswX = params.value("maxRswX", 250.0f);
			float minRswZ = params.value("minRswZ", -50.0f);
			float maxRswZ = params.value("maxRswZ", 650.0f);

			map->rootNode->traverse([&](Node* n)
			{
				if (n == map->rootNode) return;
				auto rswObj = n->getComponent<RswObject>();
				if (!rswObj) return;

				if (rswObj->position.x >= minRswX && rswObj->position.x <= maxRswX &&
					rswObj->position.z >= minRswZ && rswObj->position.z <= maxRswZ)
				{
					// Rotate 180 degrees around (rswCx, rswCz)
					rswObj->position.x = 2.0f * rswCx - rswObj->position.x;
					rswObj->position.z = 2.0f * rswCz - rswObj->position.z;
					rswObj->rotation.y = std::fmod(rswObj->rotation.y + 180.0f, 360.0f);
					if (rswObj->rotation.y < 0) rswObj->rotation.y += 360.0f;

					auto rsmRend = n->getComponent<RsmRenderer>();
					if (rsmRend)
						rsmRend->setDirty();

					objectsRotated++;
				}
			});
		}

		// 2. ROTATE GND CUBES & TILES
		if (params.value("rotateGnd", true) && gnd)
		{
			int w = gMaxX - gMinX + 1;
			int h = gMaxY - gMinY + 1;
			std::vector<std::vector<Gnd::Cube>> srcCubes(w, std::vector<Gnd::Cube>(h));
			for (int y = gMinY; y <= gMaxY; y++)
				for (int x = gMinX; x <= gMaxX; x++)
					srcCubes[x - gMinX][y - gMinY] = *gnd->cubes[x][y];

			for (int y = gMinY; y <= gMaxY; y++)
			{
				for (int x = gMinX; x <= gMaxX; x++)
				{
					int destX = gMinX + gMaxX - x;
					int destY = gMinY + gMaxY - y;
					const auto& src = srcCubes[x - gMinX][y - gMinY];
					auto dest = gnd->cubes[destX][destY];

					// Corner heights: 1->4, 2->3, 3->2, 4->1
					dest->h1 = src.h4;
					dest->h2 = src.h3;
					dest->h3 = src.h2;
					dest->h4 = src.h1;

					// Rotate top tile
					if (src.tileUp != -1 && src.tileUp < (int)gnd->tiles.size())
					{
						auto srcTile = gnd->tiles[src.tileUp];
						auto newTile = new Gnd::Tile(*srcTile);
						newTile->v1 = srcTile->v4;
						newTile->v2 = srcTile->v3;
						newTile->v3 = srcTile->v2;
						newTile->v4 = srcTile->v1;
						gnd->tiles.push_back(newTile);
						dest->tileUp = (int)gnd->tiles.size() - 1;
					}
					else
					{
						dest->tileUp = src.tileUp;
					}

					dest->calcNormal();
					tilesRotated++;
				}
			}

			if (gndRenderer)
				gndRenderer->setChunksDirty();
		}

		// 3. ROTATE GAT CELLS
		if (params.value("rotateGat", true) && gat)
		{
			int gatMinX = gMinX * 2;
			int gatMaxX = gMaxX * 2 + 1;
			int gatMinY = gMinY * 2;
			int gatMaxY = gMaxY * 2 + 1;

			int gw = gatMaxX - gatMinX + 1;
			int gh = gatMaxY - gatMinY + 1;
			std::vector<std::vector<Gat::Cube>> srcGat(gw, std::vector<Gat::Cube>(gh));
			for (int y = gatMinY; y <= gatMaxY; y++)
				for (int x = gatMinX; x <= gatMaxX; x++)
					srcGat[x - gatMinX][y - gatMinY] = *gat->cubes[x][y];

			for (int y = gatMinY; y <= gatMaxY; y++)
			{
				for (int x = gatMinX; x <= gatMaxX; x++)
				{
					int destGx = gatMinX + gatMaxX - x;
					int destGy = gatMinY + gatMaxY - y;
					const auto& src = srcGat[x - gatMinX][y - gatMinY];
					auto dest = gat->cubes[destGx][destGy];

					dest->gatType = src.gatType;
					dest->h1 = src.h4;
					dest->h2 = src.h3;
					dest->h3 = src.h2;
					dest->h4 = src.h1;
					dest->calcNormal();
					gatCellsRotated++;
				}
			}

			if (gatRenderer)
				gatRenderer->setChunksDirty();
		}

		map->changed = true;
		if (params.value("autoSave", false))
			browEdit->saveMap(map);

		res["status"] = "ok";
		res["objectsRotated"] = objectsRotated;
		res["tilesRotated"] = tilesRotated;
		res["gatCellsRotated"] = gatCellsRotated;
		return res;
	}

	// 9. TEXTURES
	if (method == "list_textures")
	{
		if (!gnd)
			throw std::runtime_error("Map has no GND component.");

		nlohmann::json texArr = nlohmann::json::array();
		for (size_t i = 0; i < gnd->textures.size(); i++)
		{
			if (!gnd->textures[i]) continue;
			nlohmann::json t;
			t["index"] = i;
			t["name"] = gnd->textures[i]->name;
			t["file"] = gnd->textures[i]->file;
			texArr.push_back(t);
		}
		res["textures"] = texArr;
		return res;
	}

	// 10. WATER
	auto rsw = map->rootNode->getComponent<Rsw>();
	if (method == "get_water")
	{
		if (!rsw)
			throw std::runtime_error("Map has no RSW component.");

		if (!rsw->water.zones.empty() && !rsw->water.zones[0].empty())
		{
			const auto& w = rsw->water.zones[0][0];
			res["height"] = w.height;
			res["type"] = w.type;
			res["amplitude"] = w.amplitude;
			res["waveSpeed"] = w.waveSpeed;
			res["wavePitch"] = w.wavePitch;
			res["textureAnimSpeed"] = w.textureAnimSpeed;
		}
		res["splitWidth"] = rsw->water.splitWidth;
		res["splitHeight"] = rsw->water.splitHeight;
		return res;
	}

	if (method == "set_water")
	{
		if (!rsw)
			throw std::runtime_error("Map has no RSW component.");

		for (auto& row : rsw->water.zones)
		{
			for (auto& w : row)
			{
				if (params.contains("height") && params["height"].is_number())
					w.height = params["height"].get<float>();
				if (params.contains("type") && params["type"].is_number())
					w.type = params["type"].get<int>();
			}
		}

		auto waterRenderer = map->rootNode->getComponent<WaterRenderer>();
		if (waterRenderer)
			waterRenderer->setDirty();

		res["status"] = "ok";
		return res;
	}

	// 11. UNDO / REDO
	if (method == "undo")
	{
		map->undo(browEdit);
		res["status"] = "ok";
		return res;
	}

	if (method == "redo")
	{
		map->redo(browEdit);
		res["status"] = "ok";
		return res;
	}

	// 12. EXPORT MINIMAP
	if (method == "export_minimap")
	{
		map->exportMinimapWalkable(browEdit);
		res["status"] = "ok";
		res["message"] = "Walkable minimap exported.";
		return res;
	}

	throw std::runtime_error("Unknown method: " + method);
}

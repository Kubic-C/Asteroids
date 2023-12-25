#include "states.hpp"

void ClientOnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo) {
	switch (pInfo->m_info.m_eState)
	{
	case k_ESteamNetworkingConnectionState_None:
		break;

	case k_ESteamNetworkingConnectionState_ClosedByPeer:
	case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
		game.sockets->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
		game.TransitionState<connectionFailed_t>();
		break;

	case k_ESteamNetworkingConnectionState_Connecting:
		game.TransitionState<connecting_t>();
		break;

	case k_ESteamNetworkingConnectionState_Connected:
		game.TransitionState<connected_t>();
		break;

	default:
		break;
	}
}

void ServerOnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo) {
	switch (pInfo->m_info.m_eState)
	{
	case k_ESteamNetworkingConnectionState_None:
		break;
	case k_ESteamNetworkingConnectionState_ClosedByPeer:
	case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
		game.sockets->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
		game.TransitionState<connectionFailed_t>();
		break;
	case k_ESteamNetworkingConnectionState_Connecting:
		if (game.connection != k_HSteamNetConnection_Invalid) {
			// We already have a peer connected. 
			break;
		}

		if (game.sockets->AcceptConnection(pInfo->m_hConn) != k_EResultOK) {
			game.sockets->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
			game.TransitionState<connectionFailed_t>();
			break;
		} 
			
		game.connection = pInfo->m_hConn;
		game.TransitionState<connecting_t>();
		break;
	case k_ESteamNetworkingConnectionState_Connected:
		game.TransitionState<connected_t>();
		break;
	default:
		break;
	}
}

int main() {
	if (!game.LoadResources()) {
		std::cout << "Failed to load resources.\n";
		return 1;
	}

	bool isUserHost = false;
	do {
		std::string in;
		std::cout << "Are you hosting(y/n): ";
		std::cin >> in;
		if (in.empty() || in.size() > 1) {
			std::cout << "Enter a valid response.\n";
			continue;
		}
		if(in[0] == 'y') {
			isUserHost = true;
			break;
		}
		else if (in[0] == 'n') {
			isUserHost = false;
			break;
		} else {
			std::cout << "Enter a valid response.\n";
			continue;
		}

	} while (true);

	// to prevent freezing, we initialize all data after the neccessary terminal input
	// is entered

	SteamNetworkingErrMsg msg;
	if (!GameNetworkingSockets_Init(nullptr, msg)) {
		std::cout << msg << std::endl;
		return 1;
	}
	game.sockets = SteamNetworkingSockets();
	game.utils = SteamNetworkingUtils();
	game.listen = k_HSteamListenSocket_Invalid;
	game.connection = k_HSteamNetConnection_Invalid;

	if (isUserHost) {
		SteamNetworkingIPAddr listenAddr;
		listenAddr.SetIPv4(0, defaultHostPort);
		SteamNetworkingConfigValue_t opt;
		opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)ServerOnSteamNetConnectionStatusChanged);
		game.listen = game.sockets->CreateListenSocketIP(listenAddr, 1, &opt);
	} else {
		SteamNetworkingIPAddr connect_addr;
		SteamNetworkingConfigValue_t opt;
		opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)ClientOnSteamNetConnectionStatusChanged);

		do {
			std::string ipv4;
			std::cout << "What IP Address would you like to connect to?: ";
			std::cin >> ipv4;

			connect_addr.ParseString(ipv4.c_str());
			connect_addr.m_port = defaultHostPort;

			std::cout << "Attempting to connect. This may take some time.\n";
			game.connection = game.sockets->ConnectByIPAddress(connect_addr, 1, &opt);
			if (game.connection == k_HSteamNetConnection_Invalid) {
				std::cout << "Failed to connect.\n";
				continue;
			} else {
				break;
			}
		} while (true);
	}

	std::string additionalInfo = "";
	if (isUserHost) {
		game.world.SetIsHost(true);
		additionalInfo += "Host";
	}
	else {
		game.world.SetIsHost(false);
		additionalInfo += "Client";
	}

	game.exit = false;
	game.CreateWindow(600, 400, "Asteroids " + additionalInfo);
	game.TransitionState<connecting_t>();

	while (game.window->isOpen() && !game.exit) {
		game.Update();
	}

	return 0;
}
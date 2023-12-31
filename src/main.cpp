#include "states.hpp"

void ClientOnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo) {
	switch (pInfo->m_info.m_eState)
	{
	case k_ESteamNetworkingConnectionState_None:
		break;

	case k_ESteamNetworkingConnectionState_ClosedByPeer:
	case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
		game->sockets->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
		game->TransitionState<connectionFailed_t>();
		break;

	case k_ESteamNetworkingConnectionState_Connecting:
		game->TransitionState<connecting_t>();
		break;

	case k_ESteamNetworkingConnectionState_Connected:
		game->TransitionState<connected_t>();
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
		game->sockets->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
		game->TransitionState<connectionFailed_t>();
		break;
	case k_ESteamNetworkingConnectionState_Connecting:
		if (game->sockets->AcceptConnection(pInfo->m_hConn) != k_EResultOK) {
			game->sockets->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
			game->TransitionState<connectionFailed_t>();
			break;
		} 
			
		game->TransitionState<connecting_t>();
		break;
	case k_ESteamNetworkingConnectionState_Connected:
		game->AddNewClient(pInfo->m_hConn);
		game->TransitionState<connected_t>();
		break;
	default:
		break;
	}
}

int main() {
	game = new global_t();

	if (!game->LoadResources()) {
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

	game->FinishNetworkInit();

	if (isUserHost) {
		SteamNetworkingIPAddr listenAddr;
		listenAddr.SetIPv4(0, defaultHostPort);
		SteamNetworkingConfigValue_t opt;
		opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)ServerOnSteamNetConnectionStatusChanged);
		game->SetListen(game->sockets->CreateListenSocketIP(listenAddr, 1, &opt));
		game->MarkHost();
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
			HSteamNetConnection connection = game->sockets->ConnectByIPAddress(connect_addr, 1, &opt);
			if(connection == k_HSteamNetConnection_Invalid) {
				continue;
			} else {
				game->AddNewClient(connection);
				break;
			}
		} while (true);
	}

	std::string additionalInfo = "";
	if (isUserHost) {
		additionalInfo += "Host";
	}
	else {
		additionalInfo += "Client";
	}

	game->CreateWindow(600, 400, "Asteroids " + additionalInfo);
	game->TransitionState<connecting_t>();

	while (!game->ShouldAppClose()) {
		game->Update();
	}

	delete game;

	return 0;
}
// SPDX-License-Identifier: GPL-3.0-or-later
#include "prosperoeden/eden_services.h"
#include "assets_dir.h"

Eden::Network::Status EdenServices::network_shares() { return Eden::Network::Current(); }
void EdenServices::reconnect_network_shares() {
    if (Eden::FilesystemAccess() && !launch_committed_)
        Eden::Network::Configure(Eden::ConfigFile("network-shares.json"));
}
void EdenServices::disconnect_network_shares() { Eden::Network::Disconnect(); }

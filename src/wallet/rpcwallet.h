// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_RPCWALLET_H
#define BITCOIN_WALLET_RPCWALLET_H

using namespace std;
class CRPCTable;

void RegisterWalletRPCCommands(CRPCTable &t);

// ;;;;;;
//std::string getnewaddress(const std::string& name);
//std::string dumpprivkey(const std::string& addr);
//std::string signmessage(const string& strAddress, const string& strMessage);
//std::string signmessagewithprivkey(const string& strPrivkey, const string& strMessage);
//bool verifymessage(const string& strAddress, const string& strSign, const string& strMessage);


#endif //BITCOIN_WALLET_RPCWALLET_H

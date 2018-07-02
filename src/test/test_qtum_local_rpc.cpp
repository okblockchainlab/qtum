// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define BOOST_TEST_MODULE Bitcoin Test Suite

#include "test/test_qtum_local_rpc.h"

#include "chainparams.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "key.h"
#include "validation.h"
#include "miner.h"
#include "net_processing.h"
#include "pubkey.h"
#include "random.h"
#include "txdb.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "rpc/server.h"
#include "rpc/register.h"
#include "script/sigcache.h"
#include "base58.h"

#include <memory>
#include "com_okcoin_vault_jni_qtum_Qtumj.h"

#include <boost/algorithm/string.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <stdio.h>

using namespace std;
extern void RegisterWalletRPCCommands(CRPCTable &tableRPC);
extern void RegisterRawTransactionRPCCommands(CRPCTable &tableRPC);
extern void SelectParams(const std::string& network);
extern UniValue RPCConvertValues(const std::string& strMethod, const std::vector<std::string>& strParams);
char* jstring2char(JNIEnv* env, jstring jstr);
jobjectArray stringList2jobjectArray(JNIEnv* env, const list<string>& stringList);
jstring char2jstring(JNIEnv* env, const char* pat);
std::list<std::string> invokeRpc(std::string args);
boost::filesystem::path GetTempPath();
//-I${JAVA_HOME}/include -I${JAVA_HOME}/include/darwin

std::unique_ptr<CConnman> g_connman2;
FastRandomContext insecure_rand_ctx(true);

extern bool fPrintToConsole;
extern void noui_connect();



BasicTestingSetup::BasicTestingSetup(const std::string& chainName)
{
    ECC_Start();
    SetupEnvironment();
    SetupNetworking();
    InitSignatureCache();
    fPrintToDebugLog = false; // don't want to write to debug.log file
    fCheckBlockIndex = true;
    SelectParams(chainName);
    noui_connect();
}

BasicTestingSetup::~BasicTestingSetup()
{
    ECC_Stop();
    g_connman2.reset();
}

TestingSetup::TestingSetup(const std::string& chainName) : BasicTestingSetup(chainName)
{
//    printf("blocked\n");
//    char ch = getchar();
//    while (ch != '\n')
//    {
//        ch = getchar();
//    }

    const CChainParams& chainparams = Params();
    // Ideally we'd move all the RPC tests to the functional testing framework
    // instead of unit tests, but for now we need these here.

    static bool init = false;
    if (!init) {
        RegisterWalletRPCCommands(tableRPC);
        RegisterBlockchainRPCCommands(tableRPC);
        RegisterNetRPCCommands(tableRPC);
        RegisterMiscRPCCommands(tableRPC);
        RegisterMiningRPCCommands(tableRPC);
        RegisterRawTransactionRPCCommands(tableRPC);
        init = true;
    }
    ClearDatadirCache();
    pathTemp = GetTempPath() / strprintf("test_bitcoin_%lu_%i", (unsigned long) GetTime(), (int) (GetRand(100000)));
    boost::filesystem::create_directories(pathTemp);
    ForceSetArg("-datadir", pathTemp.string());
    mempool.setSanityCheck(1.0);
    pblocktree = new CBlockTreeDB(1 << 20, true);
    pcoinsdbview = new CCoinsViewDB(1 << 23, true);
    pcoinsTip = new CCoinsViewCache(pcoinsdbview);

////////////////////////////////////////////////////////////// qtum
    dev::eth::Ethash::init();
    boost::filesystem::path pathTemp =
            GetTempPath() / strprintf("test_bitcoin_%lu_%i", (unsigned long) GetTime(), (int) (GetRand(100000)));
    boost::filesystem::create_directories(pathTemp);

    printf("pathTemp: %s\n", pathTemp.string().c_str());
    const dev::h256 hashDB(dev::sha3(dev::rlp("")));

    globalState = std::unique_ptr<QtumState>(
            new QtumState(dev::u256(0),
                          QtumState::openDB(pathTemp.string(), hashDB, dev::WithExisting::Trust),
                          pathTemp.string(), dev::eth::BaseState::Empty));

    dev::eth::ChainParams cp((dev::eth::genesisInfo(dev::eth::Network::qtumTestNetwork)));
    globalSealEngine = std::unique_ptr<dev::eth::SealEngineFace>(cp.createSealEngine());
    globalState->populateFrom(cp.genesisState);
    globalState->setRootUTXO(uintToh256(chainparams.GenesisBlock().hashUTXORoot));
    globalState->db().commit();
    globalState->dbUtxo().commit();
    pstorageresult = new StorageResults(pathTemp.string());
//////////////////////////////////////////////////////////////

    InitBlockIndex(chainparams);
    {
        CValidationState state;
//        bool ok = ActivateBestChain(state, chainparams);
        ActivateBestChain(state, chainparams);
//        BOOST_CHECK(ok);
    }
    nScriptCheckThreads = 3;
    for (int i = 0; i < nScriptCheckThreads - 1; i++)
        threadGroup.create_thread(&ThreadScriptCheck);
    g_connman2 = std::unique_ptr<CConnman>(new CConnman(0x1337, 0x1337)); // Deterministic randomness for tests.
    connman = g_connman2.get();
    RegisterNodeSignals(GetNodeSignals());
}

TestingSetup::~TestingSetup()
{
    UnregisterNodeSignals(GetNodeSignals());
    threadGroup.interrupt_all();
    threadGroup.join_all();
    UnloadBlockIndex();
    delete pcoinsTip;
    delete pcoinsdbview;
    delete pblocktree;

/////////////////////////////////////////////// // qtum
    delete globalState.release();
    globalSealEngine.reset();
///////////////////////////////////////////////

    boost::filesystem::remove_all(pathTemp);
}


JNIEXPORT jobjectArray JNICALL
Java_com_okcoin_vault_jni_qtum_Qtumj_execute(JNIEnv *env, jclass, jstring networkType, jstring command) {

    TestingSetup ts(jstring2char(env, networkType));
    std::list<std::string> resultList = invokeRpc(jstring2char(env, command));
    return stringList2jobjectArray(env, resultList);
}

std::list<std::string> invokeRpc(std::string args)
{
    cout << endl;
    cout << "Invoke Local Rpc: " << endl;
    cout << "===================================================================" << endl;
    cout << "Rpc Request: " << args << endl;

    try {
        std::list <std::string> resultList;
        UniValue result;
        std::vector <std::string> vArgs;
        boost::split(vArgs, args, boost::is_any_of(" \t"));

        std::string strMethod = vArgs[0];
        vArgs.erase(vArgs.begin());
        JSONRPCRequest request;
        request.strMethod = strMethod;
        request.params = RPCConvertValues(strMethod, vArgs);
        request.fHelp = false;

        if (tableRPC[strMethod] == NULL) {

            resultList.push_back("Error");
            resultList.push_back("No such a Jni Api " + strMethod);
            return resultList;
        }

        string res;
        rpcfn_type method = tableRPC[strMethod]->actor;
        result = (*method)(request);
        result.feedStringList(resultList);
    }
    catch (const UniValue &objError) {
        result = objError;

        resultList.push_back("Error");
        resultList.push_back(find_value(result.get_obj(), "message").get_str());
    }
    catch(...) {

        resultList.push_back("Error");
        resultList.push_back("Unknown exception!");
    }
    return resultList;
}

void invokeLocalRpc() {

    printf("blocked\n");
    char ch = getchar();
    while (ch != '\n')
    {
        ch = getchar();
    }

    TestingSetup ts("main");


    // addr: Qb9XDawkh7o5cXXPa5YsxBY4p1TJs4FZsb
    // pkey: L43NUb18bYnsZr8SaudQtLczX2SJz6KQG6tnXWJTM46po7m4m49h
    CBitcoinSecret BitcoinSecret;
    BitcoinSecret.SetString("L43NUb18bYnsZr8SaudQtLczX2SJz6KQG6tnXWJTM46po7m4m49h");

    CKey key = BitcoinSecret.GetKey();
    CPubKey pubkey = key.GetPubKey();
    CKeyID keyId = pubkey.GetID();
    CBitcoinAddress BitcoinAddress;
    BitcoinAddress.Set(keyId);
    string addr = BitcoinAddress.ToString();
    cout << "Address: " << addr << endl;

    cout << "------------------------------------------" << endl;

    std::string cmd = "signrawtransaction 0200000001b2fa59b9b4d3858e5d560407d68b9336420490043a2daabd31a83f0449807fdd0000000000ffffffff01c0175302000000001976a914f1b5be282a20fb932a648fc4c9decb253399979488ac00000000 [{\"txid\":\"dd7f8049043fa831bdaa2d3a0490044236938bd60704565d8e85d3b4b959fab2\",\"vout\":0,\"scriptPubKey\":\"a91404a477f8bfe145d24fdef8b219e84b3f6b09c75687\",\"redeemScript\":\"522102cc37dcc588eb08484d0d74bee9c7e7f6f818129e49abc04b25e842c6247be56c2102cf0215383cf70e8318ab09e5ec88235e878c1a4e54d6fe8e37f9da21928856eb210212360211f166a465c0e346f8538512784e9c0d5b0b9a15b465a33e6af1c1500253ae\"}] [\"L2S6o32XWJFo3xZ1hdgTh98XYtM565veYERXZZamWLUqCBADLYXs\"]";
    std::list<std::string> results = invokeRpc(cmd);

    for (auto const& r: results) {
        cout << r << endl;
    }
    cout << "------------------------------------------" << endl;

    std::string createTx = "createrawtransaction [{\"txid\":\"b4cc287e58f87cdae59417329f710f3ecd75a4ee1d2872b7248f50977c8493f3\",\"vout\":1,\"scriptPubKey\":\"a914b10c9df5f7edf436c697f02f1efdba4cf399615187\",\"redeemScript\":\"512103debedc17b3df2badbcdd86d5feb4562b86fe182e5998abd8bcd4f122c6155b1b21027e940bb73ab8732bfdf7f9216ecefca5b94d6df834e77e108f68e66f126044c052ae\"}] {\"MQ3Jx2krKJbDgAcxJrXvL73KXH4zVf8mWg\":11}";
    results = invokeRpc(createTx);

    for (auto const& r: results) {
        cout << r << endl;
    }
    cout << "------------------------------------------" << endl;

    std::string signTx = "signrawtransaction 0200000001f393847c97508f24b772281deea475cd3e0f719f321794e5da7cf8587e28ccb40100000000ffffffff0100ab90410000000017a914b10c9df5f7edf436c697f02f1efdba4cf39961518700000000 [{\"txid\":\"b4cc287e58f87cdae59417329f710f3ecd75a4ee1d2872b7248f50977c8493f3\",\"vout\":1,\"scriptPubKey\":\"a914b10c9df5f7edf436c697f02f1efdba4cf399615187\",\"redeemScript\":\"512103debedc17b3df2badbcdd86d5feb4562b86fe182e5998abd8bcd4f122c6155b1b21027e940bb73ab8732bfdf7f9216ecefca5b94d6df834e77e108f68e66f126044c052ae\"}] [\"KzsXybp9jX64P5ekX1KUxRQ79Jht9uzW7LorgwE65i5rWACL6LQe\",\"Kyhdf5LuKTRx4ge69ybABsiUAWjVRK4XGxAKk2FQLp2HjGMy87Z4\"]";
    results = invokeRpc(signTx);

    for (auto const& r: results) {
        cout << r << endl;
    }
}

jstring char2jstring(JNIEnv* env, const char* pat)
{
    jclass strClass = env->FindClass("Ljava/lang/String;");
    jmethodID ctorID = env->GetMethodID(strClass, "<init>", "([BLjava/lang/String;)V");

    jbyteArray bytes = env->NewByteArray(strlen(pat));
    env->SetByteArrayRegion(bytes, 0, strlen(pat), (jbyte*)pat);
    jstring encoding = env->NewStringUTF("utf-8");
    return (jstring)env->NewObject(strClass, ctorID, bytes, encoding);
}

jobjectArray stringList2jobjectArray(JNIEnv* env, const list<string>& stringList)
{
    size_t size = stringList.size();
    jclass objClass = env->FindClass("java/lang/String");
    jobjectArray resArray = env->NewObjectArray(size, objClass, 0);
    jsize i = 0;
    for (auto &s: stringList) {
        env->SetObjectArrayElement(resArray, i++, char2jstring(env, s.c_str()));
    }
    return resArray;
}

char* jstring2char(JNIEnv* env, jstring jstr)
{
    jclass clsstring = env->FindClass("java/lang/String");
    jstring strencode = env->NewStringUTF("utf-8");
    jmethodID mid = env->GetMethodID(clsstring, "getBytes", "(Ljava/lang/String;)[B");
    jbyteArray barr= (jbyteArray)env->CallObjectMethod(jstr, mid, strencode);
    jsize alen = env->GetArrayLength(barr);
    jbyte* ba = env->GetByteArrayElements(barr, JNI_FALSE);

    char* rtn = NULL;
    if (alen > 0)
    {
        rtn = (char*)malloc(alen + 1);
        memcpy(rtn, ba, alen);
        rtn[alen] = 0;
    }
    env->ReleaseByteArrayElements(barr, ba, 0);


    return rtn;
}


boost::filesystem::path GetTempPath() {
#if BOOST_FILESYSTEM_VERSION == 3
    return boost::filesystem::temp_directory_path();
#else
    // TODO: remove when we don't support filesystem v2 anymore
    boost::filesystem::path path;
#ifdef WIN32
    char pszPath[MAX_PATH] = "";

    if (GetTempPathA(MAX_PATH, pszPath))
        path = boost::filesystem::path(pszPath);
#else
    path = boost::filesystem::path("/tmp");
#endif
    if (path.empty() || !boost::filesystem::is_directory(path)) {
        LogPrintf("GetTempPath(): failed to find temp path\n");
        return boost::filesystem::path("");
    }
    return path;
#endif
}

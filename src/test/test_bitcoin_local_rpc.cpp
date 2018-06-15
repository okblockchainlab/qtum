// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define BOOST_TEST_MODULE Bitcoin Test Suite

#include "test/test_bitcoin_local_rpc.h"

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
//-I${JAVA_HOME}/include -I${JAVA_HOME}/include/darwin

std::unique_ptr<CConnman> g_connman2;
FastRandomContext insecure_rand_ctx(true);

extern bool fPrintToConsole;
extern void noui_connect();


extern void RegisterRawTransactionRPCCommands(CRPCTable &tableRPC);
extern void SelectParams(const std::string& network);
extern UniValue RPCConvertValues(const std::string& strMethod, const std::vector<std::string>& strParams);

char* jstring2char(JNIEnv* env, jstring jstr);
jobjectArray stringList2jobjectArray(JNIEnv* env, const list<string>& stringList);
jstring char2jstring(JNIEnv* env, const char* pat);
std::list<std::string> invokeRpc(std::string args);


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

TestChain100Setup::TestChain100Setup() : TestingSetup(CBaseChainParams::REGTEST)
{
    // Generate a 100-block chain:
    coinbaseKey.MakeNewKey(true);
    CScript scriptPubKey = CScript() <<  ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
    for (int i = 0; i < COINBASE_MATURITY; i++)
    {
        std::vector<CMutableTransaction> noTxns;
        CBlock b = CreateAndProcessBlock(noTxns, scriptPubKey);
        coinbaseTxns.push_back(*b.vtx[0]);
    }
}

//
// Create a new block with just given transactions, coinbase paying to
// scriptPubKey, and try to add it to the current chain.
//
CBlock
TestChain100Setup::CreateAndProcessBlock(const std::vector<CMutableTransaction>& txns, const CScript& scriptPubKey)
{
    const CChainParams& chainparams = Params();
    std::unique_ptr<CBlockTemplate> pblocktemplate = BlockAssembler(chainparams).CreateNewBlock(scriptPubKey);
    CBlock& block = pblocktemplate->block;

    // Replace mempool-selected txns with just coinbase plus passed-in txns:
    block.vtx.resize(1);
    BOOST_FOREACH(const CMutableTransaction& tx, txns)
        block.vtx.push_back(MakeTransactionRef(tx));
    block.nTime = chainActive.Tip()->GetBlockTime() + 1;
    // IncrementExtraNonce creates a valid coinbase and merkleRoot
    unsigned int extraNonce = 0;
    IncrementExtraNonce(&block, chainActive.Tip(), extraNonce);

    while (!CheckProofOfWork(block.GetHash(), block.nBits, chainparams.GetConsensus())) ++block.nNonce;

    std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(block);
    ProcessNewBlock(chainparams, shared_pblock, true, NULL);

    CBlock result = block;
    return result;
}

TestChain100Setup::~TestChain100Setup()
{
}


CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(const CMutableTransaction &tx, CTxMemPool *pool) {
    CTransaction txn(tx);
    return FromTx(txn, pool);
}

CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(const CTransaction &txn, CTxMemPool *pool) {
    // Hack to assume either it's completely dependent on other mempool txs or not at all
    CAmount inChainValue = pool && pool->HasNoInputsOf(txn) ? txn.GetValueOut() : 0;

    return CTxMemPoolEntry(MakeTransactionRef(txn), nFee, nTime, dPriority, nHeight,
                           inChainValue, spendsCoinbase, sigOpCost, lp);
}

//void Shutdown(void* parg)
//{
//  exit(0);
//}
//
//void StartShutdown()
//{
//  exit(0);
//}
//
//bool ShutdownRequested()
//{
//  return false;
//}

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
    cout << args << endl;

    std::list<std::string> resultList;
    UniValue result;
    std::vector<std::string> vArgs;
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
    try {
        result = (*method)(request);

        if (result.size() == 0) {

            res = result.get_str();

            cout << endl;
            cout << strMethod << " result:" << endl;
            cout << "====================================================" << endl;
            cout << res << endl << endl;

            resultList.push_back(strMethod + " result");
            resultList.push_back(result.get_str());

        } else if (result.size() >= 2) {

            if (find_value(result.get_obj(), "complete").get_bool()) {

                res = find_value(result.get_obj(), "hex").get_str();

                cout << endl;
                cout << strMethod << " result:" << endl;
                cout << "====================================================" << endl;
                cout << res << endl;

                resultList.push_back(strMethod + " result");
                resultList.push_back(res);
            }

        }
    }
    catch (const UniValue &objError) {
        result = objError;

        resultList.push_back("Error");
        resultList.push_back(find_value(result.get_obj(), "message").get_str());
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

    CBitcoinSecret BitcoinSecret;
    BitcoinSecret.SetString("L43NUb18bYnsZr8SaudQtLczX2SJz6KQG6tnXWJTM46po7m4m49h");

    CKey key = BitcoinSecret.GetKey();
    CPubKey pubkey = key.GetPubKey();

    CKeyID keyId = pubkey.GetID();
    CBitcoinAddress BitcoinAddress;
    BitcoinAddress.Set(keyId);
    string addr = BitcoinAddress.ToString();

    cout << "Address: " << addr << endl;

    std::string createTx = "createrawtransaction [{\"txid\":\"b4cc287e58f87cdae59417329f710f3ecd75a4ee1d2872b7248f50977c8493f3\",\"vout\":1,\"scriptPubKey\":\"a914b10c9df5f7edf436c697f02f1efdba4cf399615187\",\"redeemScript\":\"512103debedc17b3df2badbcdd86d5feb4562b86fe182e5998abd8bcd4f122c6155b1b21027e940bb73ab8732bfdf7f9216ecefca5b94d6df834e77e108f68e66f126044c052ae\"}] {\"MQ3Jx2krKJbDgAcxJrXvL73KXH4zVf8mWg\":11}";
    invokeRpc(createTx);

    std::string signTx = "signrawtransaction 0200000001f393847c97508f24b772281deea475cd3e0f719f321794e5da7cf8587e28ccb40100000000ffffffff0100ab90410000000017a914b10c9df5f7edf436c697f02f1efdba4cf39961518700000000 [{\"txid\":\"b4cc287e58f87cdae59417329f710f3ecd75a4ee1d2872b7248f50977c8493f3\",\"vout\":1,\"scriptPubKey\":\"a914b10c9df5f7edf436c697f02f1efdba4cf399615187\",\"redeemScript\":\"512103debedc17b3df2badbcdd86d5feb4562b86fe182e5998abd8bcd4f122c6155b1b21027e940bb73ab8732bfdf7f9216ecefca5b94d6df834e77e108f68e66f126044c052ae\"}] [\"KzsXybp9jX64P5ekX1KUxRQ79Jht9uzW7LorgwE65i5rWACL6LQe\",\"Kyhdf5LuKTRx4ge69ybABsiUAWjVRK4XGxAKk2FQLp2HjGMy87Z4\"]";
    invokeRpc(signTx);
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


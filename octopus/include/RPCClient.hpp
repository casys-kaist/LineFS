#ifndef RPCCLINET_HREADER
#define RPCCLINET_HREADER
#include <thread>
#include <mutex>
#include "RdmaSocket.hpp"
#include "Configuration.hpp"
#include "mempool.hpp"
#include "global.h"

using namespace std;

extern std::mutex g_send_lock;

class RPCClient {
private:
	Configuration *conf;
	RdmaSocket *socket;
	MemoryManager *mem;
	bool isServer;
	uint32_t taskID;
public:
	uint64_t mm;
	RPCClient(Configuration *conf, RdmaSocket *socket, MemoryManager *mem, uint64_t mm);
	RPCClient();
	~RPCClient();
	RdmaSocket* getRdmaSocketInstance();
	Configuration* getConfInstance();
	bool RdmaCall(uint16_t DesNodeID, char *bufferSend, uint64_t lengthSend, char *bufferReceive, uint64_t lengthReceive);
	bool RemoteRead(uint64_t bufferSend, uint16_t NodeID, uint64_t bufferReceive, uint64_t size);
	bool RemoteWrite(uint64_t bufferSend, uint16_t NodeID, uint64_t bufferReceive, uint64_t size);
	uint64_t ContractSendBuffer(GeneralSendBuffer *send);
};

#endif

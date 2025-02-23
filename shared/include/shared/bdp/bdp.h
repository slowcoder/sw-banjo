#pragma once

#include <stdint.h>
#include <list>
#include "shared/bdp/bdp-protocol.h"

class BDP {
public:
	BDP(int bIsServer = 0);
	~BDP();

	// Deleting the copy constructor to prevent copies
	BDP(const BDP& obj) = delete;

	// Static method to get the BDP instance
	static BDP *getInstance(int bIsServer = 0) {
		if( instancePtr == nullptr ) {
			if( instancePtr == nullptr ) {
				instancePtr = new BDP(bIsServer);
			}
		}
		return instancePtr;
	}

  typedef enum {
		Invalid = 0,
		URIPlayer,
		SPSink,
		FFRF,
	} eServices;

	class Peer;

	class Service {
	public:
		eServices type;
		uint16_t  rawsvc;
		uint16_t  port;

		Peer *pPeer;
	};

  class Peer {
  public:
		char     name[64];
		uint64_t uuid;
		char     address[128];
		std::list<Service *> services;
	};

	int advertiseService(eServices svc,uint16_t port);

	void clearPeers(void);
	std::list<Peer *> getPeers();
	Peer *getPeer(uint64_t uuid);

	std::list<Service *> getServices();

	struct udppriv *pUDPPriv;
	int bIsServer;

	std::list<Peer *> peers;
	std::list<Service *> myservices;

private:
	// Static pointer to the SDP instance
	static BDP* instancePtr;
};

#if 0
class SDP {
private:
	uint64_t myUUID;

	class UDP {
	private:
		void *pPrivate;
		int   port;
		int   isblocking;
	public:
		UDP(int port,int bindport = 0,int blocking = 1);
		~UDP();

		class Packet {
			friend class UDP;
		private:
			int   bHasData;
			void *data;
			int   datalen;

			void *pPrivate;
		public:
			Packet(int port = 0);
			~Packet();

			int getData(void *data,int *datasize);
			int setData(void *data,int  datasize);

			int getSenderASCII(char *pzAddr);

			int setDestination(void); // Broadcast
			int setDestination(char *addr);
		};

		int recv(Packet *pPacket);
		int send(Packet *pPacket);
	};

public:
	SDP(int bIsServer);

	// Deleting the copy constructor to prevent copies
	SDP(const SDP& obj) = delete;

	// Static method to get the SDP instance
	static SDP *getInstance(int bIsServer = 0) {
		if( instancePtr == nullptr ) {
			if( instancePtr == nullptr ) {
				instancePtr = new SDP(bIsServer);
			}
		}
		return instancePtr;
	}

  typedef enum {
		Invalid = 0,
		URIPlayer,
		AudioSink,
	} eServices;

	class Service {
	public:
		eServices type;
		uint16_t  rawsvc;
		uint16_t  port;
	};

  class Peer {
  public:
		char     name[64];
		uint64_t uuid;
		char     address[128];
		std::list<Service *> services;
	};

	int addService(eServices serv, uint16_t port);

	void clearPeers(void);
	std::list<Peer *> getPeers();

	// Shouldn't be called from outside SDP, but
	// can't be made private, because C..
	void tryReceive(void);
	void advertise(void);

private:
	// Static pointer to the SDP instance
	static SDP* instancePtr;

	std::list<Peer *> peers;
	std::list<Service *> myservices;

	UDP *pReqport,*pRespport;
	UDP *pAdvPort;
};
#endif

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

//#define LOGLEVEL LOGLEVEL_DEBUG
#include "shared/log.h"
#include "shared/util.h"

#include "shared/bdp/bdp.h"

#define MAX_PKTSIZE  1500
#define REQUEST_PORT 9090
#define REPLY_PORT   9091

typedef struct udppriv {
	int s;
	struct sockaddr_in sa;
	int isServer;
} udppriv_t;

static udppriv_t *udp_server(void);
static udppriv_t *udp_client(void);
static int udp_send(udppriv_t *pCtx,void *data,int datalen);
//static int udp_recv(udppriv_t *pCtx,void **data,int *len,uint32_t timeoutms);
static int udp_recvfrom(udppriv_t *pCtx,void **data,int *len,char *pzSrc,uint32_t timeoutms);

BDP* BDP::instancePtr = nullptr;
static pthread_t threadhandle;
static pthread_mutex_t mtx;

static void buildAdvertisementPacket(bdppkt_t *pPktAdv,BDP *pBDP) {
	// Build our advertisement packet header
	memset(pPktAdv,0,MAX_PKTSIZE);
	pPktAdv->type = BDPP_TYPE_ADVERTISE;
	pPktAdv->version = BDPP_VERSION;
	pPktAdv->uuid    = util_getUUID();
	util_gethostname(pPktAdv->name,63);

	// Populate our advertised services
	pthread_mutex_lock(&mtx);
	int i = 0;
	std::list<BDP::Service *> myservices = pBDP->getServices();
	std::list<BDP::Service *>::iterator it = myservices.begin();
	for (; it != myservices.end(); ++it) {
		LOGD("Populating %i service. Port=%u",i,(*it)->port);
		pPktAdv->svc[i].svc = (*it)->rawsvc;
		pPktAdv->svc[i].port = (*it)->port;
		i++;
	}
	pthread_mutex_unlock(&mtx);	
}

static void updatePeerList(BDP *pBDP,bdppkt_t *pPkt,int pktlen,char *srcaddr) {
	BDP::Peer *pPeer = NULL;
	int bNewPeer = 0;

	pPeer = pBDP->getPeer(pPkt->uuid);
	if( pPeer == NULL ) {
		pPeer = new BDP::Peer();
		bNewPeer = 1;
	}

	pPeer->uuid = pPkt->uuid;
	strncpy(pPeer->name,pPkt->name,63);
	strncpy(pPeer->address,srcaddr,63);

	int i = 0;
	while(pPkt->svc[i].port != 0) {
		LOGD("  Found service %i @ %u",pPkt->svc[i].svc,pPkt->svc[i].port);

		BDP::Service *pSvc = new BDP::Service();
		pSvc->rawsvc = pPkt->svc[i].svc;
		pSvc->port   = pPkt->svc[i].port;
		pSvc->pPeer  = pPeer;

		int bIsValidService = 1;
		switch(pSvc->rawsvc&0xFF00) {
		case BDPP_SVC_URIPLAYER: pSvc->type = BDP::URIPlayer; break;
		case BDPP_SVC_SPSINK: pSvc->type = BDP::SPSink; break;
		case BDPP_SVC_FFRF: pSvc->type = BDP::FFRF; break;
		default:
			bIsValidService = 0;
			LOGE("Invalid service received..");
			break;
		}

		if( bIsValidService && bNewPeer )
			pPeer->services.push_back(pSvc);

		i++;
	}

	if( bNewPeer ) {
		pBDP->peers.push_back(pPeer);
	}
}

static void *workerthread(void *pArg) {
	BDP *pBDP = (BDP*)pArg;
	bdppkt_t *pPktAdv,*pPktReply;
	int len;
	char srcaddr[20];
	int  loops = 0;

	// Allocate memory for our tx/rx packets
	pPktAdv   = (bdppkt_t*)calloc(1,MAX_PKTSIZE);
	pPktReply = (bdppkt_t*)calloc(1,MAX_PKTSIZE);

	while(1) {

		if( !pBDP->bIsServer && ((loops%10) == 0) ) {
			buildAdvertisementPacket(pPktAdv,pBDP);
	//		LOG("Sending %i bytes (%i + %i*4)",sizeof(bdppkt_t) + pBDP->getServices().size() * 4,sizeof(bdppkt_t),pBDP->getServices().size());
			udp_send(pBDP->pUDPPriv,pPktAdv,sizeof(bdppkt_t) + pBDP->getServices().size() * 4);
//			usleep(200 * 1000);
		}

		if( udp_recvfrom(pBDP->pUDPPriv,(void**)&pPktReply,&len,srcaddr,200) == 0 ) {
			LOGD("Got packet!");

			updatePeerList(pBDP,pPktReply,len,srcaddr);

			if( pBDP->bIsServer && (pBDP->getServices().size() > 0) ) { // If we have services, announce them as a reply
				buildAdvertisementPacket(pPktAdv,pBDP);
				LOGD("Sending %i bytes (%i + %i*4)",sizeof(bdppkt_t) + pBDP->getServices().size() * 4,sizeof(bdppkt_t),pBDP->getServices().size());
				udp_send(pBDP->pUDPPriv,pPktAdv,sizeof(bdppkt_t) + pBDP->getServices().size() * 4);
			}

			free(pPktReply);
		}
		loops++;
	}

	return NULL;
}

BDP::BDP(int bIsServer) {
	pthread_mutex_init(&mtx,NULL);

	if( bIsServer ) {
		this->bIsServer = 1;
		pUDPPriv = udp_server();
	} else {
		this->bIsServer = 0;
		pUDPPriv = udp_client();		
	}

	pthread_create(&threadhandle,NULL,workerthread,this);
}

BDP::~BDP() {
}

std::list<BDP::Service *> BDP::getServices() {
	return myservices;
}

BDP::Peer *BDP::getPeer(uint64_t uuid) {
	Peer *pRet = NULL;

	pthread_mutex_lock(&mtx);

	std::list<Peer *>::iterator it = peers.begin();
	
	for (; it != peers.end(); ++it) {
		if( (*it)->uuid == uuid ) {
			pRet = (*it);
			break;
		}
	}

	pthread_mutex_unlock(&mtx);

	return pRet;
}

int BDP::advertiseService(eServices svc,uint16_t port) {
	BDP::Service *pS;

	pS = new BDP::Service();

	pS->type = svc;
	pS->port = port;
	switch(svc) {
	case BDP::URIPlayer: pS->rawsvc = BDPP_SVC_URIPLAYER; break;
	case BDP::SPSink: pS->rawsvc = BDPP_SVC_SPSINK; break;
	case BDP::FFRF: pS->rawsvc = BDPP_SVC_FFRF; break;
	default:
		LOGE("Invalid service %i",svc);
	}
//	pS->rawsvc = 0;

	myservices.push_back(pS);

	return 0;
}

void BDP::clearPeers(void) {
	std::list<Peer *>::iterator it = peers.begin();
	
	LOG("Cleaning peers list");

	for (; it != peers.end(); ++it) {
		delete (*it);
	}
	peers.clear();
}

std::list<BDP::Peer *> BDP::getPeers() {
	return peers;
}

static udppriv_t *udp_server(void) {
	udppriv_t *pCtx;
  int broadcast = 1;
  int yes = 1;

	pCtx = (udppriv_t*)calloc(1,sizeof(udppriv_t));
	if( pCtx == NULL ) return NULL;

  pCtx->s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if( pCtx->s == -1 ) {
    LOGE("socket(AF_INET): %s", strerror(errno));
    goto err_exit;
  }

  if( setsockopt(pCtx->s, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) == -1 ) {
    LOGE("setsockopt(SO_BROADCAST): %s", strerror(errno));
    goto err_exit;
  }

  int rc;
  rc = setsockopt(pCtx->s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  if (rc == -1) {
    LOGE("setsockopt(SO_REUSEADDR): %s", strerror(errno));
    goto err_exit;
  }

  pCtx->sa.sin_family      = AF_INET;
  pCtx->sa.sin_port        = htons(REQUEST_PORT); // Sets source&dest port
//  sa.sin_addr.s_addr = htonl(INADDR_ANY);
  pCtx->sa.sin_addr.s_addr = htonl(INADDR_BROADCAST);

  if( bind(pCtx->s,(const struct sockaddr*)&pCtx->sa, sizeof(pCtx->sa)) == -1 ) {
    LOGE("bind(INADDR_ANY): %s", strerror(errno));
    goto err_exit;
  }

  pCtx->isServer = 1;

  return pCtx;
err_exit:
	if( pCtx != NULL ) {
		memset(pCtx,0,sizeof(udppriv_t));
		free(pCtx);
	}
	return NULL;
}

static udppriv_t *udp_client(void) {
	udppriv_t *pCtx;
  int broadcast = 1;
  int yes = 1;

	pCtx = (udppriv_t*)calloc(1,sizeof(udppriv_t));
	if( pCtx == NULL ) return NULL;

  pCtx->s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if( pCtx->s == -1 ) {
    LOGE("socket(AF_INET): %s", strerror(errno));
    goto err_exit;
  }

  if( setsockopt(pCtx->s, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) == -1 ) {
    LOGE("setsockopt(SO_BROADCAST): %s", strerror(errno));
    goto err_exit;
  }

  int rc;
  rc = setsockopt(pCtx->s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  if (rc == -1) {
    LOGE("setsockopt(SO_REUSEADDR): %s", strerror(errno));
    goto err_exit;
  }

  pCtx->sa.sin_family      = AF_INET;
  pCtx->sa.sin_port        = htons(REPLY_PORT); // bind() sets source port
//  sa.sin_addr.s_addr = htonl(INADDR_ANY);
  pCtx->sa.sin_addr.s_addr = htonl(INADDR_BROADCAST);

  if( bind(pCtx->s,(const struct sockaddr*)&pCtx->sa, sizeof(pCtx->sa)) == -1 ) {
    LOGE("bind(INADDR_ANY): %s", strerror(errno));
    goto err_exit;
  }

  pCtx->isServer = 0;

  return pCtx;
err_exit:
	if( pCtx != NULL ) {
		memset(pCtx,0,sizeof(udppriv_t));
		free(pCtx);
	}
	return NULL;
}

static int udp_send(udppriv_t *pCtx,void *data,int datalen) {
  socklen_t slen = sizeof(struct sockaddr_in);

  if( pCtx->isServer )
	  pCtx->sa.sin_port = htons(REPLY_PORT); // Set the dest port
	else
	  pCtx->sa.sin_port = htons(REQUEST_PORT); // Set the dest port

  if( sendto(pCtx->s,data,datalen,0,
	     (const struct sockaddr*)&pCtx->sa,slen) == -1 ) {
    LOGE("sendto: %s", strerror(errno));
    return -1;
  }

  return 0;
}

#if 0
static int udp_recv(udppriv_t *pCtx,void **data,int *len,uint32_t timeoutms) {
  socklen_t slen = sizeof(struct sockaddr_in);
  struct sockaddr_in rxsa;
  int r;
  void *d;
 	struct timeval tv;
	fd_set rset;

	if( timeoutms > 0 ) {
		FD_ZERO(&rset);
		FD_SET(pCtx->s, &rset);

		tv.tv_sec  = timeoutms / 1000;
		tv.tv_usec = (timeoutms % 1000) * 1000;

		r = select(pCtx->s + 1, &rset, NULL, NULL, &tv);
		if( r <= 0 ) return -1; // No FDs set, return
		if( !FD_ISSET(pCtx->s, &rset) ) return -2; // Not our FD, return (should never happen)
	}

  d = calloc(1,MAX_PKTSIZE);

  r = recvfrom(pCtx->s, d, MAX_PKTSIZE,
	       0,
	       (struct sockaddr *)&rxsa,
	       &slen);


  if( r == -1 ) {
    if (errno != EAGAIN)
      LOGE("recvfrom: %s", strerror(errno));
    return -1;
  }

  LOG("Got packet from: %s",inet_ntoa(rxsa.sin_addr));

  if( len != NULL ) *len = r;

  if( data != NULL ) *data = d;
  else free(d);
  
  return 0;
}
#endif
static int udp_recvfrom(udppriv_t *pCtx,void **data,int *len,char *pzSrc,uint32_t timeoutms) {
  socklen_t slen = sizeof(struct sockaddr_in);
  struct sockaddr_in rxsa;
  int r;
  void *d;
 	struct timeval tv;
	fd_set rset;

	if( timeoutms > 0 ) {
		FD_ZERO(&rset);
		FD_SET(pCtx->s, &rset);

		tv.tv_sec  = timeoutms / 1000;
		tv.tv_usec = (timeoutms % 1000) * 1000;

		r = select(pCtx->s + 1, &rset, NULL, NULL, &tv);
		if( r <= 0 ) return -1; // No FDs set, return
		if( !FD_ISSET(pCtx->s, &rset) ) return -2; // Not our FD, return (should never happen)
	}

  d = calloc(1,MAX_PKTSIZE);

  r = recvfrom(pCtx->s, d, MAX_PKTSIZE,
	       0,
	       (struct sockaddr *)&rxsa,
	       &slen);


  if( r == -1 ) {
    if (errno != EAGAIN)
      LOGE("recvfrom: %s", strerror(errno));
    return -1;
  }

  LOGD("Got packet from: %s",inet_ntoa(rxsa.sin_addr));
  if( pzSrc != NULL ) strcpy(pzSrc,inet_ntoa(rxsa.sin_addr));

  if( len != NULL ) *len = r;

  if( data != NULL ) *data = d;
  else free(d);
  
  return 0;
}

#if 0

SDP* SDP::instancePtr = nullptr;

static pthread_t threadhandle;
static void *workerthread(void *pArg) {
	SDP *pSDP = (SDP*)pArg;

	while(1) {
		LOG("Tick..");
		pSDP->advertise();
		usleep(500 * 1000);
	}
	return NULL;
}

SDP::SDP(int bIsServer) {
	LOG("Me!");

	pAdvPort  = new UDP(9090,bIsServer,0);

	pthread_create(&threadhandle,NULL,workerthread,this);
}

void SDP::tryReceive(void) {
	UDP::Packet *pReq = new UDP::Packet();

	while( pAdvPort->recv(pReq) ) {
		LOG("Got packet!");
	}

	delete pReq;
}

void SDP::advertise(void) {
  sdppkt_t rawpkt;

  UDP::Packet *pAdvPacket = new UDP::Packet(9090);

	// Only advertise if we have services to offer
	if( myservices.size() <= 0 ) {
		return;
	}

	LOG("Advertising %i services",myservices.size());

  memset(&rawpkt,0,sizeof(sdppkt_t));
  rawpkt.version = SDPP_VERSION;
  rawpkt.type    = SDPP_TYPE_SERVICEREQ | 0x80;
  rawpkt.uuid    = cpu_to_net64(myUUID);
  pAdvPacket->setData((void*)&rawpkt,sizeof(sdppkt_t));
  pAdvPacket->setDestination(); // Broadcast
  pAdvPort->send(pAdvPacket);
}


//SDP::~SDP() {
//}

int SDP::addService(SDP::eServices serv, uint16_t port) {
	SDP::Service *pS;

	pS = new SDP::Service();

	pS->type = serv;
	pS->port = port;
	pS->rawsvc = 0;

	myservices.push_back(pS);

	return 0;
}

void SDP::clearPeers(void) {
	std::list<Peer *>::iterator it = peers.begin();
	
	LOG("Cleaning peers list");

	for (; it != peers.end(); ++it) {
		delete (*it);
	}
	peers.clear();
}

std::list<SDP::Peer *> SDP::getPeers() {
	return peers;
}
#endif
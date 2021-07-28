/*****************************************************************************
Copyright (c) 2001 - 2011, The Board of Trustees of the University of Illinois.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the
  above copyright notice, this list of conditions
  and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the University of Illinois
  nor the names of its contributors may be used to
  endorse or promote products derived from this
  software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu, last updated 02/28/2012
*****************************************************************************/

#ifndef WIN32
   #include <unistd.h>
   #include <netdb.h>
   #include <arpa/inet.h>
   #include <cerrno>
   #include <cstring>
   #include <cstdlib>
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #ifdef LEGACY_WIN32
      #include <wspiapi.h>
   #endif
#endif
#include <cmath>
#include <cassert>
#include <sstream>
#include <stdexcept>
#include "queue.h"
#include "core.h"
#include <string>
#include <chrono>

using namespace std::chrono;
using namespace std;


CUDTUnited CUDT::s_UDTUnited;

const UDTSOCKET CUDT::INVALID_SOCK = -1;
const int CUDT::ERROR = -1;

const UDTSOCKET UDT::INVALID_SOCK = CUDT::INVALID_SOCK;
const int UDT::ERROR = CUDT::ERROR;

const int32_t CSeqNo::m_iSeqNoTH = 0x3FFFFFFF;
const int32_t CSeqNo::m_iMaxSeqNo = 0x7FFFFFFF;
const int32_t CAckNo::m_iMaxAckSeqNo = 0x7FFFFFFF;
const int32_t CMsgNo::m_iMsgNoTH = 0xFFFFFFF;
const int32_t CMsgNo::m_iMaxMsgNo = 0x1FFFFFFF;

const uint32_t MASK_MSGNO = 0x1FFFFFFF;
const uint32_t MASK_BITTHRESH = 0x00FFFFFF;

const int CUDT::m_iVersion = 4;
const int CUDT::m_iSYNInterval = 10000;
const int CUDT::m_iSelfClockInterval = 64;


CUDT::CUDT()
{
   m_pSndBuffer = NULL;
   m_pRcvBuffer = NULL;
   //m_pSndLossList = NULL;
   //m_pRcvLossList = NULL;
   //m_pACKWindow = NULL;
   m_pScoreBoard = NULL;
   m_pRateSample = NULL;
   m_pSndTimeWindow = NULL;
   m_pRcvTimeWindow = NULL;

   m_pSndQueue = NULL;
   m_pRcvQueue = NULL;
   m_pPeerAddr = NULL;
   m_pSNode = NULL;
   m_pRNode = NULL;

   // Initilize mutex and condition variables
   initSynch();

   // Default UDT configurations
   m_iMSS = 1500;
   m_bSynSending = true;
   m_bSynRecving = true;
   m_iFlightFlagSize = 25600;
   m_iSndBufSize = 8192;
   m_iRcvBufSize = 8192; //Rcv buffer MUST NOT be bigger than Flight Flag size
   m_Linger.l_onoff = 1;
   m_Linger.l_linger = 180;
   m_iUDPSndBufSize = 65536;
   m_iUDPRcvBufSize = m_iRcvBufSize * m_iMSS;
   m_iSockType = UDT_STREAM;
   m_iIPversion = AF_INET;
   m_bRendezvous = false;
   m_iSndTimeOut = -1;
   m_iRcvTimeOut = -1;
   m_bReuseAddr = true;
   m_llMaxBW = -1;

   m_pCCFactory = new CCCFactory<CUDTCC>;
   m_pCC = NULL;
   m_pCache = NULL;

   // Initial status
   m_bOpened = false;
   m_bListening = false;
   m_bConnecting = false;
   m_bConnected = false;
   m_bClosing = false;
   m_bShutdown = false;
   m_bBroken = false;
   m_bPeerHealth = true;
   m_ullLingerExpiration = 0;
}

CUDT::CUDT(const CUDT& ancestor)
{
   m_pSndBuffer = NULL;
   m_pRcvBuffer = NULL;
   //m_pSndLossList = NULL;
   //m_pRcvLossList = NULL;
   //m_pACKWindow = NULL;
   m_pScoreBoard = NULL;
   m_pRateSample = NULL;
   m_pSndTimeWindow = NULL;
   m_pRcvTimeWindow = NULL;

   m_pSndQueue = NULL;
   m_pRcvQueue = NULL;
   m_pPeerAddr = NULL;
   m_pSNode = NULL;
   m_pRNode = NULL;

   // Initilize mutex and condition variables
   initSynch();

   // Default UDT configurations
   m_iMSS = ancestor.m_iMSS;
   m_bSynSending = ancestor.m_bSynSending;
   m_bSynRecving = ancestor.m_bSynRecving;
   m_iFlightFlagSize = ancestor.m_iFlightFlagSize;
   m_iSndBufSize = ancestor.m_iSndBufSize;
   m_iRcvBufSize = ancestor.m_iRcvBufSize;
   m_Linger = ancestor.m_Linger;
   m_iUDPSndBufSize = ancestor.m_iUDPSndBufSize;
   m_iUDPRcvBufSize = ancestor.m_iUDPRcvBufSize;
   m_iSockType = ancestor.m_iSockType;
   m_iIPversion = ancestor.m_iIPversion;
   m_bRendezvous = ancestor.m_bRendezvous;
   m_iSndTimeOut = ancestor.m_iSndTimeOut;
   m_iRcvTimeOut = ancestor.m_iRcvTimeOut;
   m_bReuseAddr = true;	// this must be true, because all accepted sockets shared the same port with the listener
   m_llMaxBW = ancestor.m_llMaxBW;

   m_pCCFactory = ancestor.m_pCCFactory->clone();
   m_pCC = NULL;
   m_pCache = ancestor.m_pCache;

   // Initial status
   m_bOpened = false;
   m_bListening = false;
   m_bConnecting = false;
   m_bConnected = false;
   m_bClosing = false;
   m_bShutdown = false;
   m_bBroken = false;
   m_bPeerHealth = true;
   m_ullLingerExpiration = 0;
}

CUDT::~CUDT()
{
   // release mutex/condtion variables
   destroySynch();

   // destroy the data structures
   delete m_pSndBuffer;
   delete m_pRcvBuffer;
   //delete m_pSndLossList;
   //delete m_pRcvLossList;
   //delete m_pACKWindow;
   delete m_pScoreBoard;
   delete m_pRateSample;
   delete m_pSndTimeWindow;
   delete m_pRcvTimeWindow;
   delete m_pCCFactory;
   delete m_pCC;
   delete m_pPeerAddr;
   delete m_pSNode;
   delete m_pRNode;

   delete m_pRcvSeen;
}

void CUDT::setOpt(UDTOpt optName, const void* optval, int)
{
   if (m_bBroken || m_bClosing)
      throw CUDTException(2, 1, 0);

   CGuard cg(m_ConnectionLock);
   CGuard sendguard(m_SendLock);
   CGuard recvguard(m_RecvLock);

   switch (optName)
   {
   case UDT_MSS:
      if (m_bOpened)
         throw CUDTException(5, 1, 0);

      if (*(int*)optval < int(28 + CHandShake::m_iContentSize))
         throw CUDTException(5, 3, 0);

      m_iMSS = *(int*)optval;

      // Packet size cannot be greater than UDP buffer size
      if (m_iMSS > m_iUDPSndBufSize)
         m_iMSS = m_iUDPSndBufSize;
      if (m_iMSS > m_iUDPRcvBufSize)
         m_iMSS = m_iUDPRcvBufSize;

      break;

   case UDT_SNDSYN:
      m_bSynSending = *(bool *)optval;
      break;

   case UDT_RCVSYN:
      m_bSynRecving = *(bool *)optval;
      break;

   case UDT_CC:
      if (m_bConnecting || m_bConnected)
         throw CUDTException(5, 1, 0);
      if (NULL != m_pCCFactory)
         delete m_pCCFactory;
      m_pCCFactory = ((CCCVirtualFactory *)optval)->clone();

      break;

   case UDT_FC:
      if (m_bConnecting || m_bConnected)
         throw CUDTException(5, 2, 0);

      if (*(int*)optval < 1)
         throw CUDTException(5, 3);

      // Mimimum recv flight flag size is 32 packets
      if (*(int*)optval > 32)
         m_iFlightFlagSize = *(int*)optval;
      else
         m_iFlightFlagSize = 32;

      break;

   case UDT_SNDBUF:
      if (m_bOpened)
         throw CUDTException(5, 1, 0);

      if (*(int*)optval <= 0)
         throw CUDTException(5, 3, 0);

      m_iSndBufSize = *(int*)optval / (m_iMSS - 28);

      break;

   case UDT_RCVBUF:
      if (m_bOpened)
         throw CUDTException(5, 1, 0);

      if (*(int*)optval <= 0)
         throw CUDTException(5, 3, 0);

      // Mimimum recv buffer size is 32 packets
      if (*(int*)optval > (m_iMSS - 28) * 32)
         m_iRcvBufSize = *(int*)optval / (m_iMSS - 28);
      else
         m_iRcvBufSize = 32;

      // recv buffer MUST not be greater than FC size
      if (m_iRcvBufSize > m_iFlightFlagSize)
         m_iRcvBufSize = m_iFlightFlagSize;

      break;

   case UDT_LINGER:
      m_Linger = *(linger*)optval;
      break;

   case UDP_SNDBUF:
      if (m_bOpened)
         throw CUDTException(5, 1, 0);

      m_iUDPSndBufSize = *(int*)optval;

      if (m_iUDPSndBufSize < m_iMSS)
         m_iUDPSndBufSize = m_iMSS;

      break;

   case UDP_RCVBUF:
      if (m_bOpened)
         throw CUDTException(5, 1, 0);

      m_iUDPRcvBufSize = *(int*)optval;

      if (m_iUDPRcvBufSize < m_iMSS)
         m_iUDPRcvBufSize = m_iMSS;

      break;

   case UDT_RENDEZVOUS:
      if (m_bConnecting || m_bConnected)
         throw CUDTException(5, 1, 0);
      m_bRendezvous = *(bool *)optval;
      break;

   case UDT_SNDTIMEO: 
      m_iSndTimeOut = *(int*)optval; 
      break; 
    
   case UDT_RCVTIMEO: 
      m_iRcvTimeOut = *(int*)optval; 
      break; 

   case UDT_REUSEADDR:
      if (m_bOpened)
         throw CUDTException(5, 1, 0);
      m_bReuseAddr = *(bool*)optval;
      break;

   case UDT_MAXBW:
      m_llMaxBW = *(int64_t*)optval;
      break;
    
   default:
      throw CUDTException(5, 0, 0);
   }
}

void CUDT::getOpt(UDTOpt optName, void* optval, int& optlen)
{
   CGuard cg(m_ConnectionLock);

   switch (optName)
   {
   case UDT_MSS:
      *(int*)optval = m_iMSS;
      optlen = sizeof(int);
      break;

   case UDT_SNDSYN:
      *(bool*)optval = m_bSynSending;
      optlen = sizeof(bool);
      break;

   case UDT_RCVSYN:
      *(bool*)optval = m_bSynRecving;
      optlen = sizeof(bool);
      break;

   case UDT_CC:
      if (!m_bOpened)
         throw CUDTException(5, 5, 0);
      *(CCC**)optval = m_pCC;
      optlen = sizeof(CCC*);

      break;

   case UDT_FC:
      *(int*)optval = m_iFlightFlagSize;
      optlen = sizeof(int);
      break;

   case UDT_SNDBUF:
      *(int*)optval = m_iSndBufSize * (m_iMSS - 28);
      optlen = sizeof(int);
      break;

   case UDT_RCVBUF:
      *(int*)optval = m_iRcvBufSize * (m_iMSS - 28);
      optlen = sizeof(int);
      break;

   case UDT_LINGER:
      if (optlen < (int)(sizeof(linger)))
         throw CUDTException(5, 3, 0);

      *(linger*)optval = m_Linger;
      optlen = sizeof(linger);
      break;

   case UDP_SNDBUF:
      *(int*)optval = m_iUDPSndBufSize;
      optlen = sizeof(int);
      break;

   case UDP_RCVBUF:
      *(int*)optval = m_iUDPRcvBufSize;
      optlen = sizeof(int);
      break;

   case UDT_RENDEZVOUS:
      *(bool *)optval = m_bRendezvous;
      optlen = sizeof(bool);
      break;

   case UDT_SNDTIMEO: 
      *(int*)optval = m_iSndTimeOut; 
      optlen = sizeof(int); 
      break; 
    
   case UDT_RCVTIMEO: 
      *(int*)optval = m_iRcvTimeOut; 
      optlen = sizeof(int); 
      break; 

   case UDT_REUSEADDR:
      *(bool *)optval = m_bReuseAddr;
      optlen = sizeof(bool);
      break;

   case UDT_MAXBW:
      *(int64_t*)optval = m_llMaxBW;
      optlen = sizeof(int64_t);
      break;

   case UDT_STATE:
      *(int32_t*)optval = s_UDTUnited.getStatus(m_SocketID);
      optlen = sizeof(int32_t);
      break;

   case UDT_EVENT:
   {
      int32_t event = 0;
      if (m_bBroken)
         event |= UDT_EPOLL_ERR;
      else
      {
         if (m_pRcvBuffer && (m_pRcvBuffer->getRcvDataSize() > 0))
            event |= UDT_EPOLL_IN;
         if (m_pSndBuffer && (m_iSndBufSize > m_pSndBuffer->getCurrBufSize()))
            event |= UDT_EPOLL_OUT;
      }
      *(int32_t*)optval = event;
      optlen = sizeof(int32_t);
      break;
   }

   case UDT_SNDDATA:
      if (m_pSndBuffer)
         *(int32_t*)optval = m_pSndBuffer->getCurrBufSize();
      else
         *(int32_t*)optval = 0;
      optlen = sizeof(int32_t);
      break;

   case UDT_RCVDATA:
      if (m_pRcvBuffer)
         *(int32_t*)optval = m_pRcvBuffer->getRcvDataSize();
      else
         *(int32_t*)optval = 0;
      optlen = sizeof(int32_t);
      break;

   default:
      throw CUDTException(5, 0, 0);
   }
}

void CUDT::open()
{
   CGuard cg(m_ConnectionLock);

   // Initial sequence number, loss, acknowledgement, etc.
   m_iPktSize = m_iMSS - 28;
   m_iPayloadSize = m_iPktSize - CPacket::m_iPktHdrSize;

   m_iEXPCount = 1;
   m_iBandwidth = 1;
   m_iDeliveryRate = 16;
   m_ullLastAckTime = 0;

   // trace information
   m_StartTime = CTimer::getTime();
   m_llSentTotal = m_llRecvTotal = m_iSndLossTotal = m_iRcvLossTotal = m_iRetransTotal = m_iSentACKTotal = m_iRecvACKTotal = m_iSentNAKTotal = m_iRecvNAKTotal = 0;
   m_LastSampleTime = CTimer::getTime();
   m_llTraceSent = m_llTraceRecv = m_iTraceSndLoss = m_iTraceRcvLoss = m_iTraceRetrans = m_iSentACK = m_iRecvACK = m_iSentNAK = m_iRecvNAK = 0;
   m_llSndDuration = m_llSndDurationTotal = 0;

   // structures for queue
   if (NULL == m_pSNode)
      m_pSNode = new CSNode;
   m_pSNode->m_pUDT = this;
   m_pSNode->m_llTimeStamp = 1;
   m_pSNode->m_iHeapLoc = -1;

   if (NULL == m_pRNode)
      m_pRNode = new CRNode;
   m_pRNode->m_pUDT = this;
   m_pRNode->m_llTimeStamp = 1;
   m_pRNode->m_pPrev = m_pRNode->m_pNext = NULL;
   m_pRNode->m_bOnList = false;

   m_iRTT = 10 * m_iSYNInterval;
   m_iRTTVar = m_iRTT >> 1;
   m_ullCPUFrequency = CTimer::getCPUFrequency();

   // set up the timers
   m_ullSYNInt = m_iSYNInterval * m_ullCPUFrequency;
  
   // set minimum NAK and EXP timeout to 100ms
   m_ullMinNakInt = 300000 * m_ullCPUFrequency;
   m_ullMinExpInt = 300000 * m_ullCPUFrequency;

   m_ullACKInt = m_ullSYNInt;
   m_ullNAKInt = m_ullMinNakInt;

   uint64_t currtime;
   CTimer::rdtsc(currtime);
   m_ullLastRspTime = currtime;
   m_ullNextACKTime = currtime + m_ullSYNInt;
   m_ullNextNAKTime = currtime + m_ullNAKInt;

   m_iPktCount = 0;
   m_iLightACKCount = 1;

   m_ullTargetTime = 0;
   m_ullTimeDiff = 0;

   // Now UDT is opened.
   m_bOpened = true;
}

void CUDT::listen()
{
   CGuard cg(m_ConnectionLock);

   if (!m_bOpened)
      throw CUDTException(5, 0, 0);

   if (m_bConnecting || m_bConnected)
      throw CUDTException(5, 2, 0);

   // listen can be called more than once
   if (m_bListening)
      return;

   // if there is already another socket listening on the same port
   if (m_pRcvQueue->setListener(this) < 0)
      throw CUDTException(5, 11, 0);

   m_bListening = true;
}

void CUDT::connect(const sockaddr* serv_addr)
{
    fprintf(stderr, "connect serv_addr\n");

   CGuard cg(m_ConnectionLock);

   if (!m_bOpened)
      throw CUDTException(5, 0, 0);

   if (m_bListening)
      throw CUDTException(5, 2, 0);

   if (m_bConnecting || m_bConnected)
      throw CUDTException(5, 2, 0);

   // record peer/server address
   delete m_pPeerAddr;
   m_pPeerAddr = (AF_INET == m_iIPversion) ? (sockaddr*)new sockaddr_in : (sockaddr*)new sockaddr_in6;
   memcpy(m_pPeerAddr, serv_addr, (AF_INET == m_iIPversion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6));

   // register this socket in the rendezvous queue
   // RendezevousQueue is used to temporarily store incoming handshake, non-rendezvous connections also require this function
   uint64_t ttl = 3000000;
   if (m_bRendezvous)
      ttl *= 10;
   ttl += CTimer::getTime();
   m_pRcvQueue->registerConnector(m_SocketID, this, m_iIPversion, serv_addr, ttl);

   // This is my current configurations
   m_ConnReq.m_iVersion = m_iVersion;
   m_ConnReq.m_iType = m_iSockType;
   m_ConnReq.m_iMSS = m_iMSS;
   m_ConnReq.m_iFlightFlagSize = (m_iRcvBufSize < m_iFlightFlagSize)? m_iRcvBufSize : m_iFlightFlagSize;
   m_ConnReq.m_iReqType = (!m_bRendezvous) ? 1 : 0;
   m_ConnReq.m_iID = m_SocketID;
   CIPAddress::ntop(serv_addr, m_ConnReq.m_piPeerIP, m_iIPversion);

   // Random Initial Sequence Number
   srand((unsigned int)CTimer::getTime());
   //m_iISN = m_ConnReq.m_iISN = (int32_t)(CSeqNo::m_iMaxSeqNo * (double(rand()) / RAND_MAX));
   m_iISN = m_ConnReq.m_iISN = 10;

   m_iLastDecSeq = m_iISN - 1;
   m_iSndLastAck = m_iISN;
   m_iSndLastDataAck = m_iISN;
   m_iSndForward = m_iISN;
   m_iSndCurrSeqNo = m_iISN - 1;
   m_iSndCurrMsgNo = -1;
   m_iSndDropMsgNo = -1;
   m_iSndHighSeqNo = m_iISN - 1;
   m_ullSndLastAck2Time = CTimer::getTime();
   m_bLostRecovery = false;

   // Inform the server my configurations.
   CPacket request;
   char* reqdata = new char [m_iPayloadSize];
   request.pack(0, NULL, reqdata, m_iPayloadSize);
   // ID = 0, connection request
   request.m_iID = 0;

   int hs_size = m_iPayloadSize;
   m_ConnReq.serialize(reqdata, hs_size);
   request.setLength(hs_size);
   m_pSndQueue->sendto(serv_addr, request);
   m_llLastReqTime = CTimer::getTime();

   m_bConnecting = true;

   // asynchronous connect, return immediately
   if (!m_bSynRecving)
   {
      delete [] reqdata;
      return;
   }

   // Wait for the negotiated configurations from the peer side.
   CPacket response;
   char* resdata = new char [m_iPayloadSize];
   response.pack(0, NULL, resdata, m_iPayloadSize);

   CUDTException e(0, 0);

   while (!m_bClosing)
   {
      // avoid sending too many requests, at most 1 request per 250ms
      if (CTimer::getTime() - m_llLastReqTime > 250000)
      {
         m_ConnReq.serialize(reqdata, hs_size);
         request.setLength(hs_size);
         if (m_bRendezvous)
            request.m_iID = m_ConnRes.m_iID;
         m_pSndQueue->sendto(serv_addr, request);
         m_llLastReqTime = CTimer::getTime();
      }

      response.setLength(m_iPayloadSize);
      if (m_pRcvQueue->recvfrom(m_SocketID, response) > 0)
      {
         if (connect(response) <= 0)
            break;

         // new request/response should be sent out immediately on receving a response
         m_llLastReqTime = 0;
      }

      if (CTimer::getTime() > ttl)
      {
         // timeout
         e = CUDTException(1, 1, 0);
         break;
      }
   }

   delete [] reqdata;
   delete [] resdata;

   if (e.getErrorCode() == 0)
   {
      if (m_bClosing)                                                 // if the socket is closed before connection...
         e = CUDTException(1);
      else if (1002 == m_ConnRes.m_iReqType)                          // connection request rejected
         e = CUDTException(1, 2, 0);
      else if ((!m_bRendezvous) && (m_iISN != m_ConnRes.m_iISN))      // secuity check
         e = CUDTException(1, 4, 0);
   }

   if (e.getErrorCode() != 0)
      throw e;
}

int CUDT::connect(const CPacket& response) throw ()
{
   // this is the 2nd half of a connection request. If the connection is setup successfully this returns 0.
   // returning -1 means there is an error.
   // returning 1 or 2 means the connection is in process and needs more handshake
   fprintf( stderr, "connect response\n" );

   if (!m_bConnecting)
      return -1;

   if (m_bRendezvous && ((0 == response.getFlag()) || (1 == response.getType())) && (0 != m_ConnRes.m_iType))
   {
      //a data packet or a keep-alive packet comes, which means the peer side is already connected
      // in this situation, the previously recorded response will be used
      goto POST_CONNECT;
   }

   if ((1 != response.getFlag()) || (0 != response.getType()))
      return -1;

   m_ConnRes.deserialize(response.m_pcData, response.getLength());

   if (m_bRendezvous)
   {
      // regular connect should NOT communicate with rendezvous connect
      // rendezvous connect require 3-way handshake
      if (1 == m_ConnRes.m_iReqType)
         return -1;

      if ((0 == m_ConnReq.m_iReqType) || (0 == m_ConnRes.m_iReqType))
      {
         m_ConnReq.m_iReqType = -1;
         // the request time must be updated so that the next handshake can be sent out immediately.
         m_llLastReqTime = 0;
         return 1;
      }
   }
   else
   {
      // set cookie
      if (1 == m_ConnRes.m_iReqType)
      {
         m_ConnReq.m_iReqType = -1;
         m_ConnReq.m_iCookie = m_ConnRes.m_iCookie;
         m_llLastReqTime = 0;
         return 1;
      }
   }

POST_CONNECT:
   // Remove from rendezvous queue
   m_pRcvQueue->removeConnector(m_SocketID);

   // Re-configure according to the negotiated values.
   m_iMSS = m_ConnRes.m_iMSS;
   m_iFlowWindowSize = m_ConnRes.m_iFlightFlagSize;
   m_iPktSize = m_iMSS - 28;
   m_iPayloadSize = m_iPktSize - CPacket::m_iPktHdrSize;
   m_iPeerISN = m_ConnRes.m_iISN;
   m_iRcvLastAck = m_ConnRes.m_iISN;

   m_iRcvCurrSeqNo  = m_ConnRes.m_iISN - 1;
   m_iRcvHighSeqNo  = m_ConnRes.m_iISN - 1;
   m_iRcvWndMask    = 0xffff;
   m_iRcvWndSize    = m_iRcvWndMask + 1;
   m_pRcvSeen       = new int32_t[m_iRcvWndSize];
   memset( m_pRcvSeen, 0, sizeof(int32_t) * m_iRcvWndSize );

   m_PeerID = m_ConnRes.m_iID;
   memcpy(m_piSelfIP, m_ConnRes.m_piPeerIP, 16);

   // Prepare all data structures
   try
   {
      m_pSndBuffer = new CSndBuffer(32, m_iPayloadSize);
      m_pRcvBuffer = new CRcvBuffer(&(m_pRcvQueue->m_UnitQueue), m_iRcvBufSize);
      // after introducing lite ACK, the sndlosslist may not be cleared in time, so it requires twice space.
      //m_pSndLossList = new CSndLossList(m_iFlowWindowSize * 2);
      //m_pRcvLossList = new CRcvLossList(m_iFlightFlagSize);
      //m_pACKWindow = new CACKWindow(1024);
      m_pScoreBoard = new ScoreBoard(m_iPeerISN);
      m_pRateSample = new RateSample();
      m_pRcvTimeWindow = new CPktTimeWindow(16, 64);
      m_pSndTimeWindow = new CPktTimeWindow();
   }
   catch (...)
   {
      throw CUDTException(3, 2, 0);
   }

   CInfoBlock ib;
   ib.m_iIPversion = m_iIPversion;
   CInfoBlock::convert(m_pPeerAddr, m_iIPversion, ib.m_piIP);
   if (m_pCache->lookup(&ib) >= 0)
   {
      m_iRTT = ib.m_iRTT;
      m_iBandwidth = ib.m_iBandwidth;
   }

   m_pCC = m_pCCFactory->create();
   m_pCC->m_UDT = m_SocketID;
   m_pCC->setMSS(m_iMSS);
   m_pCC->setMaxCWndSize(m_iFlowWindowSize);
   m_pCC->setSndCurrSeqNo(m_iSndCurrSeqNo);
   m_pCC->setRcvRate(m_iDeliveryRate);
   m_pCC->setRTT(m_iRTT);
   m_pCC->setBandwidth(m_iBandwidth);
   m_pCC->init();

   m_ullInterval = (uint64_t)(m_pCC->m_dPktSndPeriod * m_ullCPUFrequency);
   m_dCongestionWindow = m_pCC->m_dCWndSize;

   // And, I am connected too.
   m_bConnecting = false;
   m_bConnected = true;

   // register this socket for receiving data packets
   m_pRNode->m_bOnList = true;
   m_pRcvQueue->setNewEntry(this);

   // acknowledge the management module.
   s_UDTUnited.connect_complete(m_SocketID);

   // acknowledde any waiting epolls to write
   s_UDTUnited.m_EPoll.update_events(m_SocketID, m_sPollID, UDT_EPOLL_OUT, true);

   return 0;
}

void CUDT::connect(const sockaddr* peer, CHandShake* hs)
{
    fprintf( stderr, "connect peer, hs\n" );
   CGuard cg(m_ConnectionLock);

   // Uses the smaller MSS between the peers        
   if (hs->m_iMSS > m_iMSS)
      hs->m_iMSS = m_iMSS;
   else
      m_iMSS = hs->m_iMSS;

   // exchange info for maximum flow window size
   m_iFlowWindowSize = hs->m_iFlightFlagSize;
   hs->m_iFlightFlagSize = (m_iRcvBufSize < m_iFlightFlagSize)? m_iRcvBufSize : m_iFlightFlagSize;

   m_iPeerISN = hs->m_iISN;

   m_iRcvLastAck = hs->m_iISN;
   m_iRcvCurrSeqNo = hs->m_iISN - 1;
   m_iRcvHighSeqNo = hs->m_iISN - 1;
   m_iRcvWndMask    = 0xffff;
   m_iRcvWndSize    = m_iRcvWndMask + 1;
   m_pRcvSeen       = new int32_t[m_iRcvWndSize];
   memset( m_pRcvSeen, 0, sizeof(int32_t) * m_iRcvWndSize );

   m_PeerID = hs->m_iID;
   hs->m_iID = m_SocketID;

   // use peer's ISN and send it back for security check
   m_iISN = hs->m_iISN;

   m_iLastDecSeq = m_iISN - 1;
   m_iSndLastAck = m_iISN;
   m_iSndLastDataAck = m_iISN;
   m_iSndForward = m_iISN;
   m_iSndCurrSeqNo = m_iISN - 1;
   m_iSndCurrMsgNo = -1;
   m_iSndDropMsgNo = -1;
   m_iSndHighSeqNo = m_iISN - 1;
   m_ullSndLastAck2Time = CTimer::getTime();
   m_bLostRecovery = false;

   // this is a reponse handshake
   hs->m_iReqType = -1;

   // get local IP address and send the peer its IP address (because UDP cannot get local IP address)
   memcpy(m_piSelfIP, hs->m_piPeerIP, 16);
   CIPAddress::ntop(peer, hs->m_piPeerIP, m_iIPversion);
  
   m_iPktSize = m_iMSS - 28;
   m_iPayloadSize = m_iPktSize - CPacket::m_iPktHdrSize;

   // Prepare all structures
   try
   {
      m_pSndBuffer = new CSndBuffer(32, m_iPayloadSize);
      m_pRcvBuffer = new CRcvBuffer(&(m_pRcvQueue->m_UnitQueue), m_iRcvBufSize);
      //m_pSndLossList = new CSndLossList(m_iFlowWindowSize * 2);
      //m_pRcvLossList = new CRcvLossList(m_iFlightFlagSize);
      //m_pACKWindow = new CACKWindow(1024);
      m_pScoreBoard = new ScoreBoard(m_iISN);
      m_pRateSample = new RateSample();
      m_pRcvTimeWindow = new CPktTimeWindow(16, 64);
      m_pSndTimeWindow = new CPktTimeWindow();
   }
   catch (...)
   {
      throw CUDTException(3, 2, 0);
   }

   CInfoBlock ib;
   ib.m_iIPversion = m_iIPversion;
   CInfoBlock::convert(peer, m_iIPversion, ib.m_piIP);
   if (m_pCache->lookup(&ib) >= 0)
   {
      m_iRTT = ib.m_iRTT;
      m_iBandwidth = ib.m_iBandwidth;
   }

   m_pCC = m_pCCFactory->create();
   m_pCC->m_UDT = m_SocketID;
   m_pCC->setMSS(m_iMSS);
   m_pCC->setMaxCWndSize(m_iFlowWindowSize);
   m_pCC->setSndCurrSeqNo(m_iSndCurrSeqNo);
   m_pCC->setRcvRate(m_iDeliveryRate);
   m_pCC->setRTT(m_iRTT);
   m_pCC->setBandwidth(m_iBandwidth);
   m_pCC->init();

   m_ullInterval = (uint64_t)(m_pCC->m_dPktSndPeriod * m_ullCPUFrequency);
   m_dCongestionWindow = m_pCC->m_dCWndSize;

   m_pPeerAddr = (AF_INET == m_iIPversion) ? (sockaddr*)new sockaddr_in : (sockaddr*)new sockaddr_in6;
   memcpy(m_pPeerAddr, peer, (AF_INET == m_iIPversion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6));

   // And of course, it is connected.
   m_bConnected = true;

   // register this socket for receiving data packets
   m_pRNode->m_bOnList = true;
   m_pRcvQueue->setNewEntry(this);

   //send the response to the peer, see listen() for more discussions about this
   CPacket response;
   int size = CHandShake::m_iContentSize;
   char* buffer = new char[size];
   hs->serialize(buffer, size);
   response.pack(0, NULL, buffer, size);
   response.m_iID = m_PeerID;
   m_pSndQueue->sendto(peer, response);
   delete [] buffer;
}

void CUDT::close()
{
   if (!m_bOpened)
      return;

   if (0 != m_Linger.l_onoff)
   {
      uint64_t entertime = CTimer::getTime();

      while (!m_bBroken && m_bConnected && (m_pSndBuffer->getCurrBufSize() > 0) && (CTimer::getTime() - entertime < m_Linger.l_linger * 1000000ULL))
      {
         // linger has been checked by previous close() call and has expired
         if (m_ullLingerExpiration >= entertime)
            break;

         if (!m_bSynSending)
         {
            // if this socket enables asynchronous sending, return immediately and let GC to close it later
            if (0 == m_ullLingerExpiration)
               m_ullLingerExpiration = entertime + m_Linger.l_linger * 1000000ULL;

            return;
         }

         #ifndef WIN32
            timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = 1000000;
            nanosleep(&ts, NULL);
         #else
            Sleep(1);
         #endif
      }
   }

   // remove this socket from the snd queue
   if (m_bConnected)
      m_pSndQueue->m_pSndUList->remove(this);

   // trigger any pending IO events.
   s_UDTUnited.m_EPoll.update_events(m_SocketID, m_sPollID, UDT_EPOLL_ERR, true);
   // then remove itself from all epoll monitoring
   try
   {
      for (set<int>::iterator i = m_sPollID.begin(); i != m_sPollID.end(); ++ i)
         s_UDTUnited.m_EPoll.remove_usock(*i, m_SocketID);
   }
   catch (...)
   {
   }

   if (!m_bOpened)
      return;

   // Inform the threads handler to stop.
   m_bClosing = true;

   CGuard cg(m_ConnectionLock);

   // Signal the sender and recver if they are waiting for data.
   releaseSynch();

   if (m_bListening)
   {
      m_bListening = false;
      m_pRcvQueue->removeListener(this);
   }
   else if (m_bConnecting)
   {
      m_pRcvQueue->removeConnector(m_SocketID);
   }

   if (m_bConnected)
   {
      if (!m_bShutdown)
         sendCtrl(5);

      m_pCC->close();

      // Store current connection information.
      CInfoBlock ib;
      ib.m_iIPversion = m_iIPversion;
      CInfoBlock::convert(m_pPeerAddr, m_iIPversion, ib.m_piIP);
      ib.m_iRTT = m_iRTT;
      ib.m_iBandwidth = m_iBandwidth;
      m_pCache->update(&ib);

      m_bConnected = false;
   }

   // waiting all send and recv calls to stop
   CGuard sendguard(m_SendLock);
   CGuard recvguard(m_RecvLock);

   // CLOSED.
   m_bOpened = false;
}

int CUDT::send(const char* data, int len)
{
   if (UDT_DGRAM == m_iSockType)
      throw CUDTException(5, 10, 0);

   // throw an exception if not connected
   if (m_bBroken || m_bClosing)
      throw CUDTException(2, 1, 0);
   else if (!m_bConnected)
      throw CUDTException(2, 2, 0);

   if (len <= 0)
      return 0;

   CGuard sendguard(m_SendLock);

   if (m_pSndBuffer->getCurrBufSize() == 0)
   {
      // delay the EXP timer to avoid mis-fired timeout
      uint64_t currtime;
      CTimer::rdtsc(currtime);
      m_ullLastRspTime = currtime;
   }

   if (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize())
   {
      if (!m_bSynSending)
         throw CUDTException(6, 1, 0);
      else
      {
         // wait here during a blocking sending
         #ifndef WIN32
            pthread_mutex_lock(&m_SendBlockLock);
            if (m_iSndTimeOut < 0) 
            { 
               while (!m_bBroken && m_bConnected && !m_bClosing && (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize()) && m_bPeerHealth)
                  pthread_cond_wait(&m_SendBlockCond, &m_SendBlockLock);
            }
            else
            {
               uint64_t exptime = CTimer::getTime() + m_iSndTimeOut * 1000ULL;
               timespec locktime; 
    
               locktime.tv_sec = exptime / 1000000;
               locktime.tv_nsec = (exptime % 1000000) * 1000;

               while (!m_bBroken && m_bConnected && !m_bClosing && (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize()) && m_bPeerHealth && (CTimer::getTime() < exptime))
                  pthread_cond_timedwait(&m_SendBlockCond, &m_SendBlockLock, &locktime);
            }
            pthread_mutex_unlock(&m_SendBlockLock);
         #else
            if (m_iSndTimeOut < 0)
            {
               while (!m_bBroken && m_bConnected && !m_bClosing && (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize()) && m_bPeerHealth)
                  WaitForSingleObject(m_SendBlockCond, INFINITE);
            }
            else 
            {
               uint64_t exptime = CTimer::getTime() + m_iSndTimeOut * 1000ULL;

               while (!m_bBroken && m_bConnected && !m_bClosing && (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize()) && m_bPeerHealth && (CTimer::getTime() < exptime))
                  WaitForSingleObject(m_SendBlockCond, DWORD((exptime - CTimer::getTime()) / 1000)); 
            }
         #endif

         // check the connection status
         if (m_bBroken || m_bClosing)
            throw CUDTException(2, 1, 0);
         else if (!m_bConnected)
            throw CUDTException(2, 2, 0);
         else if (!m_bPeerHealth)
         {
            m_bPeerHealth = true;
            throw CUDTException(7);
         }
      }
   }

   if (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize())
   {
      if (m_iSndTimeOut >= 0)
         throw CUDTException(6, 3, 0); 

      return 0;
   }

   int size = (m_iSndBufSize - m_pSndBuffer->getCurrBufSize()) * m_iPayloadSize;
   if (size > len)
      size = len;

   // record total time used for sending
   if (0 == m_pSndBuffer->getCurrBufSize())
      m_llSndDurationCounter = CTimer::getTime();

   // insert the user buffer into the sening list
   m_pSndBuffer->addBuffer(data, size);

   // insert this socket to snd list if it is not on the list yet
   m_pSndQueue->m_pSndUList->update(this, false);

   if (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize())
   {
      // write is not available any more
      s_UDTUnited.m_EPoll.update_events(m_SocketID, m_sPollID, UDT_EPOLL_OUT, false);
   }

   return size;
}

int CUDT::recv(char* data, int len)
{
   if (UDT_DGRAM == m_iSockType)
      throw CUDTException(5, 10, 0);

   // throw an exception if not connected
   if (!m_bConnected)
      throw CUDTException(2, 2, 0);
   else if ((m_bBroken || m_bClosing) && (0 == m_pRcvBuffer->getRcvDataSize()))
      throw CUDTException(2, 1, 0);

   if (len <= 0)
      return 0;

   CGuard recvguard(m_RecvLock);

   if (0 == m_pRcvBuffer->getRcvDataSize())
   {
      if (!m_bSynRecving)
         throw CUDTException(6, 2, 0);
      else
      {
         #ifndef WIN32
            pthread_mutex_lock(&m_RecvDataLock);
            if (m_iRcvTimeOut < 0) 
            { 
               while (!m_bBroken && m_bConnected && !m_bClosing && (0 == m_pRcvBuffer->getRcvDataSize()))
                  pthread_cond_wait(&m_RecvDataCond, &m_RecvDataLock);
            }
            else
            {
               uint64_t exptime = CTimer::getTime() + m_iRcvTimeOut * 1000ULL; 
               timespec locktime; 
    
               locktime.tv_sec = exptime / 1000000;
               locktime.tv_nsec = (exptime % 1000000) * 1000;

               while (!m_bBroken && m_bConnected && !m_bClosing && (0 == m_pRcvBuffer->getRcvDataSize()))
               {
                  pthread_cond_timedwait(&m_RecvDataCond, &m_RecvDataLock, &locktime); 
                  if (CTimer::getTime() >= exptime)
                     break;
               }
            }
            pthread_mutex_unlock(&m_RecvDataLock);
         #else
            if (m_iRcvTimeOut < 0)
            {
               while (!m_bBroken && m_bConnected && !m_bClosing && (0 == m_pRcvBuffer->getRcvDataSize()))
                  WaitForSingleObject(m_RecvDataCond, INFINITE);
            }
            else
            {
               uint64_t enter_time = CTimer::getTime();

               while (!m_bBroken && m_bConnected && !m_bClosing && (0 == m_pRcvBuffer->getRcvDataSize()))
               {
                  int diff = int(CTimer::getTime() - enter_time) / 1000;
                  if (diff >= m_iRcvTimeOut)
                      break;
                  WaitForSingleObject(m_RecvDataCond, DWORD(m_iRcvTimeOut - diff ));
               }
            }
         #endif
      }
   }

   // throw an exception if not connected
   if (!m_bConnected)
      throw CUDTException(2, 2, 0);
   else if ((m_bBroken || m_bClosing) && (0 == m_pRcvBuffer->getRcvDataSize()))
      throw CUDTException(2, 1, 0);

   int res = m_pRcvBuffer->readBuffer(data, len);

   if (m_pRcvBuffer->getRcvDataSize() <= 0)
   {
      // read is not available any more
      s_UDTUnited.m_EPoll.update_events(m_SocketID, m_sPollID, UDT_EPOLL_IN, false);
   }

   if ((res <= 0) && (m_iRcvTimeOut >= 0))
      throw CUDTException(6, 3, 0);

   return res;
}

int CUDT::sendmsg(const char* data, int len, int msttl, bool inorder, uint32_t extra_field)
{
   if (UDT_STREAM == m_iSockType)
      throw CUDTException(5, 9, 0);

   // throw an exception if not connected
   if (m_bBroken || m_bClosing)
      throw CUDTException(2, 1, 0);
   else if (!m_bConnected)
      throw CUDTException(2, 2, 0);

   if (len <= 0)
      return 0;

   if (len > m_iSndBufSize * m_iPayloadSize)
      throw CUDTException(5, 12, 0);

   CGuard sendguard(m_SendLock);

   if (m_pSndBuffer->getCurrBufSize() == 0)
   {
      // delay the EXP timer to avoid mis-fired timeout
      uint64_t currtime;
      CTimer::rdtsc(currtime);
      m_ullLastRspTime = currtime;
   }

   if ((m_iSndBufSize - m_pSndBuffer->getCurrBufSize()) * m_iPayloadSize < len)
   {
      if (!m_bSynSending)
         throw CUDTException(6, 1, 0);
      else
      {
         // wait here during a blocking sending
         #ifndef WIN32
            pthread_mutex_lock(&m_SendBlockLock);
            if (m_iSndTimeOut < 0)
            {
               while (!m_bBroken && m_bConnected && !m_bClosing && ((m_iSndBufSize - m_pSndBuffer->getCurrBufSize()) * m_iPayloadSize < len))
                  pthread_cond_wait(&m_SendBlockCond, &m_SendBlockLock);
            }
            else
            {
               uint64_t exptime = CTimer::getTime() + m_iSndTimeOut * 1000ULL;
               timespec locktime;

               locktime.tv_sec = exptime / 1000000;
               locktime.tv_nsec = (exptime % 1000000) * 1000;

               while (!m_bBroken && m_bConnected && !m_bClosing && ((m_iSndBufSize - m_pSndBuffer->getCurrBufSize()) * m_iPayloadSize < len) && (CTimer::getTime() < exptime))
                  pthread_cond_timedwait(&m_SendBlockCond, &m_SendBlockLock, &locktime);
            }
            pthread_mutex_unlock(&m_SendBlockLock);
         #else
            if (m_iSndTimeOut < 0)
            {
               while (!m_bBroken && m_bConnected && !m_bClosing && ((m_iSndBufSize - m_pSndBuffer->getCurrBufSize()) * m_iPayloadSize < len))
                  WaitForSingleObject(m_SendBlockCond, INFINITE);
            }
            else
            {
               uint64_t exptime = CTimer::getTime() + m_iSndTimeOut * 1000ULL;

               while (!m_bBroken && m_bConnected && !m_bClosing && ((m_iSndBufSize - m_pSndBuffer->getCurrBufSize()) * m_iPayloadSize < len) && (CTimer::getTime() < exptime))
                  WaitForSingleObject(m_SendBlockCond, DWORD((exptime - CTimer::getTime()) / 1000));
            }
         #endif

         // check the connection status
         if (m_bBroken || m_bClosing)
            throw CUDTException(2, 1, 0);
         else if (!m_bConnected)
            throw CUDTException(2, 2, 0);
      }
   }

   if ((m_iSndBufSize - m_pSndBuffer->getCurrBufSize()) * m_iPayloadSize < len)
   {
      if (m_iSndTimeOut >= 0)
         throw CUDTException(6, 3, 0);

      return 0;
   }

   // record total time used for sending
   if (0 == m_pSndBuffer->getCurrBufSize())
      m_llSndDurationCounter = CTimer::getTime();

   checkAppLimited();

   // insert the user buffer into the sending list
   m_iSndCurrMsgNo = m_pSndBuffer->addBuffer(data, len, 0, inorder, extra_field);

   // insert this socket to the snd list if it is not on the list yet
   m_pSndQueue->m_pSndUList->update(this, false);

   if (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize())
   {
      // write is not available any more
      s_UDTUnited.m_EPoll.update_events(m_SocketID, m_sPollID, UDT_EPOLL_OUT, false);
   }

   return len;   
}

int CUDT::recvmsg(char* data, int len)
{
   if (UDT_STREAM == m_iSockType)
      throw CUDTException(5, 9, 0);

   // throw an exception if not connected
   if (!m_bConnected)
      throw CUDTException(2, 2, 0);

   if (len <= 0)
      return 0;

   CGuard recvguard(m_RecvLock);

   if (m_bBroken || m_bClosing)
   {
      int res = m_pRcvBuffer->readMsg(data, len);

      if (m_pRcvBuffer->getRcvMsgNum() <= 0)
      {
         // read is not available any more
         s_UDTUnited.m_EPoll.update_events(m_SocketID, m_sPollID, UDT_EPOLL_IN, false);
      }

      if (0 == res)
         throw CUDTException(2, 1, 0);
      else
         return res;
   }

   if (!m_bSynRecving)
   {
      int res = m_pRcvBuffer->readMsg(data, len);
      if (0 == res)
         throw CUDTException(6, 2, 0);
      else
         return res;
   }

   int res = 0;
   bool timeout = false;

   do
   {
      #ifndef WIN32
         pthread_mutex_lock(&m_RecvDataLock);

         if (m_iRcvTimeOut < 0)
         {
            while (!m_bBroken && m_bConnected && !m_bClosing && (0 == (res = m_pRcvBuffer->readMsg(data, len))))
               pthread_cond_wait(&m_RecvDataCond, &m_RecvDataLock);
         }
         else
         {
            uint64_t exptime = CTimer::getTime() + m_iRcvTimeOut * 1000ULL;
            timespec locktime;

            locktime.tv_sec = exptime / 1000000;
            locktime.tv_nsec = (exptime % 1000000) * 1000;

            if (pthread_cond_timedwait(&m_RecvDataCond, &m_RecvDataLock, &locktime) == ETIMEDOUT)
               timeout = true;

            res = m_pRcvBuffer->readMsg(data, len);           
         }
         pthread_mutex_unlock(&m_RecvDataLock);
      #else
         if (m_iRcvTimeOut < 0)
         {
            while (!m_bBroken && m_bConnected && !m_bClosing && (0 == (res = m_pRcvBuffer->readMsg(data, len))))
               WaitForSingleObject(m_RecvDataCond, INFINITE);
         }
         else
         {
            if (WaitForSingleObject(m_RecvDataCond, DWORD(m_iRcvTimeOut)) == WAIT_TIMEOUT)
               timeout = true;

            res = m_pRcvBuffer->readMsg(data, len);
         }
      #endif

      if (m_bBroken || m_bClosing)
         throw CUDTException(2, 1, 0);
      else if (!m_bConnected)
         throw CUDTException(2, 2, 0);
   } while ((0 == res) && !timeout);

   if (m_pRcvBuffer->getRcvMsgNum() <= 0)
   {
      // read is not available any more
      s_UDTUnited.m_EPoll.update_events(m_SocketID, m_sPollID, UDT_EPOLL_IN, false);
   }

   if ((res <= 0) && (m_iRcvTimeOut >= 0))
      throw CUDTException(6, 3, 0);

   return res;
}

int64_t CUDT::sendfile(fstream& ifs, int64_t& offset, int64_t size, int block)
{
   if (UDT_DGRAM == m_iSockType)
      throw CUDTException(5, 10, 0);

   if (m_bBroken || m_bClosing)
      throw CUDTException(2, 1, 0);
   else if (!m_bConnected)
      throw CUDTException(2, 2, 0);

   if (size <= 0)
      return 0;

   CGuard sendguard(m_SendLock);

   if (m_pSndBuffer->getCurrBufSize() == 0)
   {
      // delay the EXP timer to avoid mis-fired timeout
      uint64_t currtime;
      CTimer::rdtsc(currtime);
      m_ullLastRspTime = currtime;
   }

   int64_t tosend = size;
   int unitsize;

   // positioning...
   try
   {
      ifs.seekg((streamoff)offset);
   }
   catch (...)
   {
      throw CUDTException(4, 1);
   }

   // sending block by block
   while (tosend > 0)
   {
      if (ifs.fail())
         throw CUDTException(4, 4);

      if (ifs.eof())
         break;

      unitsize = int((tosend >= block) ? block : tosend);

      #ifndef WIN32
         pthread_mutex_lock(&m_SendBlockLock);
         while (!m_bBroken && m_bConnected && !m_bClosing && (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize()) && m_bPeerHealth)
            pthread_cond_wait(&m_SendBlockCond, &m_SendBlockLock);
         pthread_mutex_unlock(&m_SendBlockLock);
      #else
         while (!m_bBroken && m_bConnected && !m_bClosing && (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize()) && m_bPeerHealth)
            WaitForSingleObject(m_SendBlockCond, INFINITE);
      #endif

      if (m_bBroken || m_bClosing)
         throw CUDTException(2, 1, 0);
      else if (!m_bConnected)
         throw CUDTException(2, 2, 0);
      else if (!m_bPeerHealth)
      {
         // reset peer health status, once this error returns, the app should handle the situation at the peer side
         m_bPeerHealth = true;
         throw CUDTException(7);
      }

      // record total time used for sending
      if (0 == m_pSndBuffer->getCurrBufSize())
         m_llSndDurationCounter = CTimer::getTime();

      int64_t sentsize = m_pSndBuffer->addBufferFromFile(ifs, unitsize);

      if (sentsize > 0)
      {
         tosend -= sentsize;
         offset += sentsize;
      }

      // insert this socket to snd list if it is not on the list yet
      m_pSndQueue->m_pSndUList->update(this, false);
   }

   if (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize())
   {
      // write is not available any more
      s_UDTUnited.m_EPoll.update_events(m_SocketID, m_sPollID, UDT_EPOLL_OUT, false);
   }

   return size - tosend;
}

int64_t CUDT::recvfile(fstream& ofs, int64_t& offset, int64_t size, int block)
{
   if (UDT_DGRAM == m_iSockType)
      throw CUDTException(5, 10, 0);

   if (!m_bConnected)
      throw CUDTException(2, 2, 0);
   else if ((m_bBroken || m_bClosing) && (0 == m_pRcvBuffer->getRcvDataSize()))
      throw CUDTException(2, 1, 0);

   if (size <= 0)
      return 0;

   CGuard recvguard(m_RecvLock);

   int64_t torecv = size;
   int unitsize = block;
   int recvsize;

   // positioning...
   try
   {
      ofs.seekp((streamoff)offset);
   }
   catch (...)
   {
      throw CUDTException(4, 3);
   }

   // receiving... "recvfile" is always blocking
   while (torecv > 0)
   {
      if (ofs.fail())
      {
         // send the sender a signal so it will not be blocked forever
         int32_t err_code = CUDTException::EFILE;
         sendCtrl(8, &err_code);

         throw CUDTException(4, 4);
      }

      #ifndef WIN32
         pthread_mutex_lock(&m_RecvDataLock);
         while (!m_bBroken && m_bConnected && !m_bClosing && (0 == m_pRcvBuffer->getRcvDataSize()))
            pthread_cond_wait(&m_RecvDataCond, &m_RecvDataLock);
         pthread_mutex_unlock(&m_RecvDataLock);
      #else
         while (!m_bBroken && m_bConnected && !m_bClosing && (0 == m_pRcvBuffer->getRcvDataSize()))
            WaitForSingleObject(m_RecvDataCond, INFINITE);
      #endif

      if (!m_bConnected)
         throw CUDTException(2, 2, 0);
      else if ((m_bBroken || m_bClosing) && (0 == m_pRcvBuffer->getRcvDataSize()))
         throw CUDTException(2, 1, 0);

      unitsize = int((torecv >= block) ? block : torecv);
      recvsize = m_pRcvBuffer->readBufferToFile(ofs, unitsize);

      if (recvsize > 0)
      {
         torecv -= recvsize;
         offset += recvsize;
      }
   }

   if (m_pRcvBuffer->getRcvDataSize() <= 0)
   {
      // read is not available any more
      s_UDTUnited.m_EPoll.update_events(m_SocketID, m_sPollID, UDT_EPOLL_IN, false);
   }

   return size - torecv;
}

void CUDT::sample(CPerfMon* perf, bool clear)
{
   if (!m_bConnected)
      throw CUDTException(2, 2, 0);
   if (m_bBroken || m_bClosing)
      throw CUDTException(2, 1, 0);

   uint64_t currtime = CTimer::getTime();
   perf->msTimeStamp = (currtime - m_StartTime) / 1000;

   perf->pktSent = m_llTraceSent;
   perf->pktRecv = m_llTraceRecv;
   perf->pktSndLoss = m_iTraceSndLoss;
   perf->pktRcvLoss = m_iTraceRcvLoss;
   perf->pktRetrans = m_iTraceRetrans;
   perf->pktSentACK = m_iSentACK;
   perf->pktRecvACK = m_iRecvACK;
   perf->pktSentNAK = m_iSentNAK;
   perf->pktRecvNAK = m_iRecvNAK;
   perf->usSndDuration = m_llSndDuration;

   perf->pktSentTotal = m_llSentTotal;
   perf->pktRecvTotal = m_llRecvTotal;
   perf->pktSndLossTotal = m_iSndLossTotal;
   perf->pktRcvLossTotal = m_iRcvLossTotal;
   perf->pktRetransTotal = m_iRetransTotal;
   perf->pktSentACKTotal = m_iSentACKTotal;
   perf->pktRecvACKTotal = m_iRecvACKTotal;
   perf->pktSentNAKTotal = m_iSentNAKTotal;
   perf->pktRecvNAKTotal = m_iRecvNAKTotal;
   perf->usSndDurationTotal = m_llSndDurationTotal;

   double interval = double(currtime - m_LastSampleTime);

   perf->mbpsSendRate = double(m_llTraceSent) * m_iPayloadSize * 8.0 / interval;
   perf->mbpsRecvRate = double(m_llTraceRecv) * m_iPayloadSize * 8.0 / interval;

   perf->pktFlowWindow = m_iFlowWindowSize;
   perf->pktCongestionWindow = (int)m_dCongestionWindow;
   perf->pktFlightSize = CSeqNo::seqlen(m_iSndLastAck, CSeqNo::incseq(m_iSndCurrSeqNo)) - 1;
   perf->msRTT = m_iRTT/1000.0;
   perf->mbpsBandwidth = m_iBandwidth * m_iPayloadSize * 8.0 / 1000000.0;

   perf->pacingRate = (int)(m_dVideoRate * 1024);

   #ifndef WIN32
      if (0 == pthread_mutex_trylock(&m_ConnectionLock))
   #else
      if (WAIT_OBJECT_0 == WaitForSingleObject(m_ConnectionLock, 0))
   #endif
   {
      perf->byteAvailSndBuf = (NULL == m_pSndBuffer) ? 0 : (m_iSndBufSize - m_pSndBuffer->getCurrBufSize()) * m_iMSS;
      perf->byteAvailRcvBuf = (NULL == m_pRcvBuffer) ? 0 : m_pRcvBuffer->getAvailBufSize() * m_iMSS;

      #ifndef WIN32
         pthread_mutex_unlock(&m_ConnectionLock);
      #else
         ReleaseMutex(m_ConnectionLock);
      #endif
   }
   else
   {
      perf->byteAvailSndBuf = 0;
      perf->byteAvailRcvBuf = 0;
   }

   if (clear)
   {
      m_llTraceSent = m_llTraceRecv = m_iTraceSndLoss = m_iTraceRcvLoss = m_iTraceRetrans = m_iSentACK = m_iRecvACK = m_iSentNAK = m_iRecvNAK = 0;
      m_llSndDuration = 0;
      m_LastSampleTime = currtime;
   }
}

void CUDT::CCUpdate()
{
    assert( m_ullCPUFrequency == 1);
   m_ullInterval = (uint64_t)(m_pCC->m_dPktSndPeriod * m_ullCPUFrequency);
   m_dCongestionWindow = m_pCC->m_dCWndSize;

   m_dPacingRate = 12000 / m_pCC->m_dPktSndPeriod;
   m_dBtlBw = m_pCC->m_dBtlBw;
   m_dVideoRate = m_pCC->m_dVideoRate;

   /*
   if (m_llMaxBW <= 0)
      return;
   const double minSP = 1000000.0 / (double(m_llMaxBW) / m_iMSS) * m_ullCPUFrequency;
   if (m_ullInterval < minSP)
       m_ullInterval = minSP;
       */
}

void CUDT::initSynch()
{
   #ifndef WIN32
      pthread_mutex_init(&m_SendBlockLock, NULL);
      pthread_cond_init(&m_SendBlockCond, NULL);
      pthread_mutex_init(&m_RecvDataLock, NULL);
      pthread_cond_init(&m_RecvDataCond, NULL);
      pthread_mutex_init(&m_SendLock, NULL);
      pthread_mutex_init(&m_RecvLock, NULL);
      pthread_mutex_init(&m_AckLock, NULL);
      pthread_mutex_init(&m_ConnectionLock, NULL);
   #else
      m_SendBlockLock = CreateMutex(NULL, false, NULL);
      m_SendBlockCond = CreateEvent(NULL, false, false, NULL);
      m_RecvDataLock = CreateMutex(NULL, false, NULL);
      m_RecvDataCond = CreateEvent(NULL, false, false, NULL);
      m_SendLock = CreateMutex(NULL, false, NULL);
      m_RecvLock = CreateMutex(NULL, false, NULL);
      m_AckLock = CreateMutex(NULL, false, NULL);
      m_ConnectionLock = CreateMutex(NULL, false, NULL);
   #endif
}

void CUDT::destroySynch()
{
   #ifndef WIN32
      pthread_mutex_destroy(&m_SendBlockLock);
      pthread_cond_destroy(&m_SendBlockCond);
      pthread_mutex_destroy(&m_RecvDataLock);
      pthread_cond_destroy(&m_RecvDataCond);
      pthread_mutex_destroy(&m_SendLock);
      pthread_mutex_destroy(&m_RecvLock);
      pthread_mutex_destroy(&m_AckLock);
      pthread_mutex_destroy(&m_ConnectionLock);
   #else
      CloseHandle(m_SendBlockLock);
      CloseHandle(m_SendBlockCond);
      CloseHandle(m_RecvDataLock);
      CloseHandle(m_RecvDataCond);
      CloseHandle(m_SendLock);
      CloseHandle(m_RecvLock);
      CloseHandle(m_AckLock);
      CloseHandle(m_ConnectionLock);
   #endif
}

void CUDT::releaseSynch()
{
   #ifndef WIN32
      // wake up user calls
      pthread_mutex_lock(&m_SendBlockLock);
      pthread_cond_signal(&m_SendBlockCond);
      pthread_mutex_unlock(&m_SendBlockLock);

      pthread_mutex_lock(&m_SendLock);
      pthread_mutex_unlock(&m_SendLock);

      pthread_mutex_lock(&m_RecvDataLock);
      pthread_cond_signal(&m_RecvDataCond);
      pthread_mutex_unlock(&m_RecvDataLock);

      pthread_mutex_lock(&m_RecvLock);
      pthread_mutex_unlock(&m_RecvLock);
   #else
      SetEvent(m_SendBlockCond);
      WaitForSingleObject(m_SendLock, INFINITE);
      ReleaseMutex(m_SendLock);
      SetEvent(m_RecvDataCond);
      WaitForSingleObject(m_RecvLock, INFINITE);
      ReleaseMutex(m_RecvLock);
   #endif
}

void CUDT::sendCtrl(int pkttype, void* lparam, void* rparam, int size)
{
   CPacket ctrlpkt;

   switch (pkttype)
   {
   case 2: //010 - Acknowledgement
       {
           int32_t ack;
           ack = CSeqNo::incseq(m_iRcvCurrSeqNo);

           // There are new received packets to acknowledge, update related information.
           if (CSeqNo::seqcmp(ack, m_iRcvLastAck) > 0)
           {
               int acksize = CSeqNo::seqoff(m_iRcvLastAck, ack);
               m_iRcvLastAck = ack;
               m_pRcvBuffer->ackData(acksize);
               // signal a waiting "recv" call if there is any data available
               /*
               pthread_mutex_lock(&m_RecvDataLock);
               if (m_bSynRecving)
                   pthread_cond_signal(&m_RecvDataCond);
               pthread_mutex_unlock(&m_RecvDataLock);
               */

               // acknowledge any waiting epolls to read
               s_UDTUnited.m_EPoll.update_events(m_SocketID, m_sPollID, UDT_EPOLL_IN, true);
           }

           int32_t data[3];
           data[0] = ack;
           data[1] = m_pRcvBuffer->getAvailBufSize();
           data[2] = *(int32_t *)lparam; 
           // a minimum flow window of 2 is used, even if buffer is full, to break potential deadlock
           if (data[1] < 2)
               data[1] = 2;

           ctrlpkt.pack(pkttype, data, NULL, 0);

           ctrlpkt.m_iID = m_PeerID;
           m_pSndQueue->sendto(m_pPeerAddr, ctrlpkt);


           ++ m_iSentACK;
           ++ m_iSentACKTotal;

           break;
       }
       /*{{{
      {
      int32_t ack;

      // If there is no loss, the ACK is the current largest sequence number plus 1;
      // Otherwise it is the smallest sequence number in the receiver loss list.
      if (0 == m_pRcvLossList->getLossLength())
         ack = CSeqNo::incseq(m_iRcvCurrSeqNo);
      else
         ack = m_pRcvLossList->getFirstLostSeq();

      if (ack == m_iRcvLastAckAck)
         break;

      // send out a lite ACK
      // to save time on buffer processing and bandwidth/AS measurement, a lite ACK only feeds back an ACK number
      if (4 == size)
      {
         ctrlpkt.pack(pkttype, NULL, &ack, size);
         ctrlpkt.m_iID = m_PeerID;
         m_pSndQueue->sendto(m_pPeerAddr, ctrlpkt);

         break;
      }

      uint64_t currtime;
      CTimer::rdtsc(currtime);

      // There are new received packets to acknowledge, update related information.
      if (CSeqNo::seqcmp(ack, m_iRcvLastAck) > 0)
      {
         int acksize = CSeqNo::seqoff(m_iRcvLastAck, ack);

         m_iRcvLastAck = ack;

         m_pRcvBuffer->ackData(acksize);

         // signal a waiting "recv" call if there is any data available
         #ifndef WIN32
            pthread_mutex_lock(&m_RecvDataLock);
            if (m_bSynRecving)
               pthread_cond_signal(&m_RecvDataCond);
            pthread_mutex_unlock(&m_RecvDataLock);
         #else
            if (m_bSynRecving)
               SetEvent(m_RecvDataCond);
         #endif

         // acknowledge any waiting epolls to read
         s_UDTUnited.m_EPoll.update_events(m_SocketID, m_sPollID, UDT_EPOLL_IN, true);
      }
      else if (ack == m_iRcvLastAck)
      {
         if ((currtime - m_ullLastAckTime) < ((m_iRTT + 4 * m_iRTTVar) * m_ullCPUFrequency))
            break;
      }
      else
         break;

      // Send out the ACK only if has not been received by the sender before
      if (CSeqNo::seqcmp(m_iRcvLastAck, m_iRcvLastAckAck) > 0)
      {
         int32_t data[6];

         m_iAckSeqNo = CAckNo::incack(m_iAckSeqNo);
         data[0] = m_iRcvLastAck;
         data[1] = m_iRTT;
         data[2] = m_iRTTVar;
         data[3] = m_pRcvBuffer->getAvailBufSize();
         // a minimum flow window of 2 is used, even if buffer is full, to break potential deadlock
         if (data[3] < 2)
            data[3] = 2;

         if (currtime - m_ullLastAckTime > m_ullSYNInt)
         {
            data[4] = m_pRcvTimeWindow->getPktRcvSpeed();
            data[5] = m_pRcvTimeWindow->getBandwidth();
            ctrlpkt.pack(pkttype, &m_iAckSeqNo, data, 24);

            CTimer::rdtsc(m_ullLastAckTime);
         }
         else
         {
            ctrlpkt.pack(pkttype, &m_iAckSeqNo, data, 16);
         }

         ctrlpkt.m_iID = m_PeerID;
         m_pSndQueue->sendto(m_pPeerAddr, ctrlpkt);

         m_pACKWindow->store(m_iAckSeqNo, m_iRcvLastAck);

         ++ m_iSentACK;
         ++ m_iSentACKTotal;
      }

      break;
      }
   *//*}}}*/

      /*
   case 6: //110 - Acknowledgement of Acknowledgement
      ctrlpkt.pack(pkttype, lparam);
      ctrlpkt.m_iID = m_PeerID;
      m_pSndQueue->sendto(m_pPeerAddr, ctrlpkt);

      break;
      */

   case 3: //011 - Sack 
   {
       int32_t ack;
       ack = CSeqNo::incseq(m_iRcvCurrSeqNo);

       if ( CSeqNo::seqcmp(ack, m_iRcvLastAck) > 0 )
       {
           int acksize = CSeqNo::seqoff(m_iRcvLastAck, ack);
           m_iRcvLastAck = ack;
           m_pRcvBuffer->ackData(acksize);
           s_UDTUnited.m_EPoll.update_events(m_SocketID, m_sPollID, UDT_EPOLL_IN, true);
       }
       int32_t data[3];
       data[0] = ack;
       data[1] = m_pRcvBuffer->getAvailBufSize();
       data[2] = *(int32_t *)lparam;  // it carrys the received seq that triggers this sack
       // a minimum flow window of 2 is used, even if buffer is full, to break potential deadlock
       if (data[1] < 2)
           data[1] = 2;

       ctrlpkt.pack(pkttype, &data, rparam, size);

       ctrlpkt.m_iID = m_PeerID;
       m_pSndQueue->sendto(m_pPeerAddr, ctrlpkt);

       break;
   }
       /*
      {
      if (NULL != rparam)
      {
         if (1 == size)
         {
            // only 1 loss packet
            ctrlpkt.pack(pkttype, NULL, (int32_t *)rparam + 1, 4);
         }
         else
         {
            // more than 1 loss packets
            ctrlpkt.pack(pkttype, NULL, rparam, 8);
         }

         ctrlpkt.m_iID = m_PeerID;
         m_pSndQueue->sendto(m_pPeerAddr, ctrlpkt);

         ++ m_iSentNAK;
         ++ m_iSentNAKTotal;
      }
      else if (m_pRcvLossList->getLossLength() > 0)
      {
         // this is periodically NAK report; make sure NAK cannot be sent back too often

         // read loss list from the local receiver loss list
         int32_t* data = new int32_t[m_iPayloadSize / 4];
         int losslen;
         m_pRcvLossList->getLossArray(data, losslen, m_iPayloadSize / 4);

         if (0 < losslen)
         {
            ctrlpkt.pack(pkttype, NULL, data, losslen * 4);
            ctrlpkt.m_iID = m_PeerID;
            m_pSndQueue->sendto(m_pPeerAddr, ctrlpkt);

            ++ m_iSentNAK;
            ++ m_iSentNAKTotal;
         }

         delete [] data;
      }

      // update next NAK time, which should wait enough time for the retansmission, but not too long
      m_ullNAKInt = (m_iRTT + 4 * m_iRTTVar) * m_ullCPUFrequency;
      int rcv_speed = m_pRcvTimeWindow->getPktRcvSpeed();
      if (rcv_speed > 0)
         m_ullNAKInt += (m_pRcvLossList->getLossLength() * 1000000ULL / rcv_speed) * m_ullCPUFrequency;
      if (m_ullNAKInt < m_ullMinNakInt)
         m_ullNAKInt = m_ullMinNakInt;

      break;
      }
      */

   case 4: //100 - Congestion Warning
      ctrlpkt.pack(pkttype);
      ctrlpkt.m_iID = m_PeerID;
      m_pSndQueue->sendto(m_pPeerAddr, ctrlpkt);

      CTimer::rdtsc(m_ullLastWarningTime);

      break;

   case 1: //001 - Keep-alive
      ctrlpkt.pack(pkttype);
      ctrlpkt.m_iID = m_PeerID;
      m_pSndQueue->sendto(m_pPeerAddr, ctrlpkt);
 
      break;

   case 0: //000 - Handshake
      ctrlpkt.pack(pkttype, NULL, rparam, sizeof(CHandShake));
      ctrlpkt.m_iID = m_PeerID;
      m_pSndQueue->sendto(m_pPeerAddr, ctrlpkt);

      break;

   case 5: //101 - Shutdown
      ctrlpkt.pack(pkttype);
      ctrlpkt.m_iID = m_PeerID;
      m_pSndQueue->sendto(m_pPeerAddr, ctrlpkt);

      break;

   case 7: //111 - Msg drop request
      ctrlpkt.pack(pkttype, lparam, rparam, 8);
      ctrlpkt.m_iID = m_PeerID;
      m_pSndQueue->sendto(m_pPeerAddr, ctrlpkt);

      break;

   case 8: //1000 - acknowledge the peer side a special error
      ctrlpkt.pack(pkttype, lparam);
      ctrlpkt.m_iID = m_PeerID;
      m_pSndQueue->sendto(m_pPeerAddr, ctrlpkt);

      break;

   case 32767: //0x7FFF - Resevered for future use
      break;

   default:
      break;
   }
}

#define SACK_LEFT( i ) (i * 2 + 1)
#define SACK_RIGHT( i ) (i * 2 + 2)

void CUDT::processCtrl(CPacket& ctrlpkt)
{
   // Just heard from the peer, reset the expiration count.
   m_iEXPCount = 1;
   uint64_t currtime;
   CTimer::rdtsc(currtime);

   switch (ctrlpkt.getType())
   {
   case 2: //010 - Acknowledgement
       {
           m_ullLastRspTime = currtime;
           int32_t ack = ctrlpkt.getRcvAck();
           m_iSndLastAck = m_iSndLastDataAck;

           fprintf(stderr, "recv_ack ack: %d SndLastDataAck: %d SndCurrSeq: %d rwnd: %d ts: %ldms\n",
                   ack,
                   m_iSndLastDataAck,
                   m_iSndCurrSeqNo,
                   ctrlpkt.getRcvWnd(),
                   CTimer::getTime() / 1000);

           // check the validation of the ack
           if (CSeqNo::seqcmp(ack, CSeqNo::incseq(m_iSndHighSeqNo)) > 0)
           {
               //this should not happen: attack or bug
               m_bBroken = true;
               m_iBrokenCounter = 0;
               break;
           }
           if ( CSeqNo::seqcmp(ack, m_iSndLastAck) >= 0 )
           {
               // Update Flow Window Size, must update before and together with m_iSndLastAck
               m_iFlowWindowSize = ctrlpkt.getRcvWnd();
               m_iSndLastDataAck = ack;
           }

           if ( CSeqNo::seqoff( m_iSndLastAck, ack ) <= 0 ) {
               // discard it if it is a repeated ACK
               break;
           }

           m_bLostRecovery = false;

           if (CSeqNo::seqcmp(m_iSndLastDataAck, m_iSndCurrSeqNo) > 0)
               m_iSndCurrSeqNo = m_iSndLastDataAck - 1;

           Block* block = m_pSndBuffer->readData( 0, m_iSndLastDataAck - 1 );
           m_pRateSample->onAckSacked( block, 1 );
           m_pRateSample->setPacketLost( false );
           m_pCC->onAck( block, m_pRateSample );

           // record total time used for sending
           m_llSndDuration += currtime - m_llSndDurationCounter;
           m_llSndDurationTotal += currtime - m_llSndDurationCounter;
           m_llSndDurationCounter = currtime;

           // acknowledge the sending buffer
           m_pSndBuffer->ackData( m_iSndLastDataAck );

           // protect scoreboard 
           CGuard::enterCS(m_AckLock);
           m_pScoreBoard->update( m_iSndLastDataAck, NULL );
           checkPktForward();
           CGuard::leaveCS(m_AckLock);

           pthread_mutex_lock(&m_SendBlockLock);
           if (m_bSynSending)
               pthread_cond_signal(&m_SendBlockCond);
           pthread_mutex_unlock(&m_SendBlockLock);
           // acknowledde any waiting epolls to write
           s_UDTUnited.m_EPoll.update_events(m_SocketID, m_sPollID, UDT_EPOLL_OUT, true);

           // insert this socket to snd list if it is not on the list yet
           m_pSndQueue->m_pSndUList->update(this, false);

           m_pCC->onACK( m_iSndLastDataAck );
           CCUpdate();

           ++ m_iRecvACK;
           ++ m_iRecvACKTotal;

           break;
       }
       /*{{{
      {
      int32_t ack;

      // process a lite ACK
      if (4 == ctrlpkt.getLength())
      {
         ack = *(int32_t *)ctrlpkt.m_pcData;
         if (CSeqNo::seqcmp(ack, m_iSndLastAck) >= 0)
         {
            m_iFlowWindowSize -= CSeqNo::seqoff(m_iSndLastAck, ack);
            m_iSndLastAck = ack;
         }

         break;
      }

       // read ACK seq. no.
      ack = ctrlpkt.getAckSeqNo();

      // send ACK acknowledgement
      // number of ACK2 can be much less than number of ACK
      uint64_t now = CTimer::getTime();
      if ((currtime - m_ullSndLastAck2Time > (uint64_t)m_iSYNInterval) || (ack == m_iSndLastAck2))
      {
         sendCtrl(6, &ack);
         m_iSndLastAck2 = ack;
         m_ullSndLastAck2Time = now;
      }

      // Got data ACK
      ack = *(int32_t *)ctrlpkt.m_pcData;

      // check the validation of the ack
      if (CSeqNo::seqcmp(ack, CSeqNo::incseq(m_iSndCurrSeqNo)) > 0)
      {
         //this should not happen: attack or bug
         m_bBroken = true;
         m_iBrokenCounter = 0;
         break;
      }

      if (CSeqNo::seqcmp(ack, m_iSndLastAck) >= 0)
      {
         // Update Flow Window Size, must update before and together with m_iSndLastAck
         m_iFlowWindowSize = *((int32_t *)ctrlpkt.m_pcData + 3);
         m_iSndLastAck = ack;
      }

      // protect packet retransmission
      CGuard::enterCS(m_AckLock);

      int offset = CSeqNo::seqoff(m_iSndLastDataAck, ack);
      if (offset <= 0)
      {
         // discard it if it is a repeated ACK
         CGuard::leaveCS(m_AckLock);
         break;
      }

      // acknowledge the sending buffer
      m_pSndBuffer->ackData(offset);

      // record total time used for sending
      m_llSndDuration += currtime - m_llSndDurationCounter;
      m_llSndDurationTotal += currtime - m_llSndDurationCounter;
      m_llSndDurationCounter = currtime;

      // update sending variables
      m_iSndLastDataAck = ack;
      m_pSndLossList->remove(CSeqNo::decseq(m_iSndLastDataAck));

      CGuard::leaveCS(m_AckLock);

      #ifndef WIN32
         pthread_mutex_lock(&m_SendBlockLock);
         if (m_bSynSending)
            pthread_cond_signal(&m_SendBlockCond);
         pthread_mutex_unlock(&m_SendBlockLock);
      #else
         if (m_bSynSending)
            SetEvent(m_SendBlockCond);
      #endif

      // acknowledde any waiting epolls to write
      s_UDTUnited.m_EPoll.update_events(m_SocketID, m_sPollID, UDT_EPOLL_OUT, true);

      // insert this socket to snd list if it is not on the list yet
      m_pSndQueue->m_pSndUList->update(this, false);

      // Update RTT
      //m_iRTT = *((int32_t *)ctrlpkt.m_pcData + 1);
      //m_iRTTVar = *((int32_t *)ctrlpkt.m_pcData + 2);
      int rtt = *((int32_t *)ctrlpkt.m_pcData + 1);
      m_iRTTVar = (m_iRTTVar * 3 + abs(rtt - m_iRTT)) >> 2;
      m_iRTT = (m_iRTT * 7 + rtt) >> 3;

      m_pCC->setRTT(m_iRTT);

      if (ctrlpkt.getLength() > 16)
      {
         // Update Estimated Bandwidth and packet delivery rate
         if (*((int32_t *)ctrlpkt.m_pcData + 4) > 0)
            m_iDeliveryRate = (m_iDeliveryRate * 7 + *((int32_t *)ctrlpkt.m_pcData + 4)) >> 3;

         if (*((int32_t *)ctrlpkt.m_pcData + 5) > 0)
            m_iBandwidth = (m_iBandwidth * 7 + *((int32_t *)ctrlpkt.m_pcData + 5)) >> 3;

         m_pCC->setRcvRate(m_iDeliveryRate);
         m_pCC->setBandwidth(m_iBandwidth);
      }

      m_pCC->onACK(ack);
      CCUpdate();

      ++ m_iRecvACK;
      ++ m_iRecvACKTotal;

      break;
      }
   *//*}}}*/

       case 3: //011 - SACK
   {
       int32_t* sack_array = (int32_t *)(ctrlpkt.m_pcData);
       // modify the sack area and ack based on the SndForward
       // sack area [left, right)
       if (ctrlpkt.getRcvAck() < m_iSndForward) {
           int32_t sack_num = sack_array[0];
           int32_t sack_area[ m_iSackBlkNum * 2 + 1];
           for (int sack_index = 0; sack_index < sack_num; ++sack_index) {
               sack_area[SACK_LEFT(sack_index)] = sack_array[SACK_LEFT(sack_index)];
               sack_area[SACK_RIGHT(sack_index)] = sack_array[SACK_RIGHT(sack_index)];
           }
           int32_t new_sack_num = 0;
           for (int sack_index = 0; sack_index < sack_num; ++sack_index) {
               if ( sack_area[SACK_RIGHT(sack_index)] < m_iSndForward ) {
                   continue;
               }
               else if ( m_iSndForward >= sack_area[SACK_LEFT(sack_index)]
                       && m_iSndForward <= sack_area[SACK_RIGHT(sack_index)] ) {
                   m_iSndForward = sack_array[SACK_RIGHT(sack_index)];
               }
               else {
                   sack_array[SACK_LEFT(new_sack_num)] = sack_area[SACK_LEFT(sack_index)];
                   sack_array[SACK_RIGHT(new_sack_num)] = sack_area[SACK_RIGHT(sack_index)];
                   new_sack_num += 1;
               }
           }
           ctrlpkt.m_nHeader[1] = m_iSndForward;
           if (new_sack_num == 0) {
               ctrlpkt.m_nHeader[0] = 0x80000000 | (2 << 16);// ack now
               return processCtrl(ctrlpkt);
           }
           else {
               sack_array[0] = new_sack_num;
           }
       }
       m_bLostRecovery = true;
       m_iSndLastAck = m_iSndLastDataAck;

       // protect packet retransmission
       CGuard::enterCS(m_AckLock);
       m_pScoreBoard->update( ctrlpkt.getRcvAck(), sack_array );
       fprintf( stderr, "recv_sack, " );
       m_pScoreBoard->dumpBoard();
       fprintf( stderr, ", ts:%ld\n", CTimer::getTime() );
       checkPktForward();
       CGuard::leaveCS(m_AckLock);

       // call onAck for both acked and sacked pkt
       if ( ctrlpkt.getTriggerSeq() >= m_iSndLastDataAck ) {
           Block* block = m_pSndBuffer->readData( 0, ctrlpkt.getTriggerSeq() );
           m_pRateSample->onAckSacked( block, 2 );
           m_pCC->onAck( block, m_pRateSample );
       }

       int32_t ack = ctrlpkt.getRcvAck();
       if ( CSeqNo::seqcmp( ack, m_iSndLastDataAck ) >= 0 ) {
           m_iFlowWindowSize = ctrlpkt.getRcvWnd();
           m_iSndLastDataAck = ack;
       }

       int offset = CSeqNo::seqoff( m_iSndLastAck, m_iSndLastDataAck );

       if (offset > 0)
       {
           // only call onAck for m_pCC and m_pRateSample when new ack arrives
           if (CSeqNo::seqcmp(m_iSndLastDataAck, m_iSndCurrSeqNo) > 0)
               m_iSndCurrSeqNo = m_iSndLastDataAck - 1;
           m_pSndBuffer->ackData(m_iSndLastDataAck);
           m_ullLastRspTime = currtime;     //reset RTO timer
       }

       // this will wake up the m_pSndUList from sleep (sleep results from no pkt/congestion)
       // the lost packet (retransmission) should be sent out immediately
       m_pSndQueue->m_pSndUList->update(this, false);

       CCUpdate();
       break;
   }
   /*{{{
      {
      int32_t* losslist = (int32_t *)(ctrlpkt.m_pcData);

      m_pCC->onLoss(losslist, ctrlpkt.getLength() / 4);
      CCUpdate();

      bool secure = true;

      // decode loss list message and insert loss into the sender loss list
      for (int i = 0, n = (int)(ctrlpkt.getLength() / 4); i < n; ++ i)
      {
         if (0 != (losslist[i] & 0x80000000))
         {
            if ((CSeqNo::seqcmp(losslist[i] & 0x7FFFFFFF, losslist[i + 1]) > 0) || (CSeqNo::seqcmp(losslist[i + 1], m_iSndCurrSeqNo) > 0))
            {
               // seq_a must not be greater than seq_b; seq_b must not be greater than the most recent sent seq
               secure = false;
               break;
            }

            int num = 0;
            if (CSeqNo::seqcmp(losslist[i] & 0x7FFFFFFF, m_iSndLastAck) >= 0)
               num = m_pSndLossList->insert(losslist[i] & 0x7FFFFFFF, losslist[i + 1]);
            else if (CSeqNo::seqcmp(losslist[i + 1], m_iSndLastAck) >= 0)
               num = m_pSndLossList->insert(m_iSndLastAck, losslist[i + 1]);

            m_iTraceSndLoss += num;
            m_iSndLossTotal += num;

            ++ i;
         }
         else if (CSeqNo::seqcmp(losslist[i], m_iSndLastAck) >= 0)
         {
            if (CSeqNo::seqcmp(losslist[i], m_iSndCurrSeqNo) > 0)
            {
               //seq_a must not be greater than the most recent sent seq
               secure = false;
               break;
            }

            int num = m_pSndLossList->insert(losslist[i], losslist[i]);

            m_iTraceSndLoss += num;
            m_iSndLossTotal += num;
         }
      }

      if (!secure)
      {
         //this should not happen: attack or bug
         m_bBroken = true;
         m_iBrokenCounter = 0;
         break;
      }

      // the lost packet (retransmission) should be sent out immediately
      m_pSndQueue->m_pSndUList->update(this);

      ++ m_iRecvNAK;
      ++ m_iRecvNAKTotal;

      break;
      }
      *//*}}}*/

   case 4: //100 - Delay Warning
      // One way packet delay is increasing, so decrease the sending rate
      m_ullLastRspTime = currtime;
      m_ullInterval = (uint64_t)ceil(m_ullInterval * 1.125);
      m_iLastDecSeq = m_iSndCurrSeqNo;

      break;

   case 1: //001 - Keep-alive
      // The only purpose of keep-alive packet is to tell that the peer is still alive
      // nothing needs to be done.
      m_ullLastRspTime = currtime;

      break;

   case 0: //000 - Handshake
      {     
          m_ullLastRspTime = currtime;
          CHandShake req;
          req.deserialize(ctrlpkt.m_pcData, ctrlpkt.getLength());
          if ((req.m_iReqType > 0) || (m_bRendezvous && (req.m_iReqType != -2)))
          {
              // The peer side has not received the handshake message, so it keeps querying
              // resend the handshake packet

              CHandShake initdata;
              initdata.m_iISN = m_iISN;
              initdata.m_iMSS = m_iMSS;
              initdata.m_iFlightFlagSize = m_iFlightFlagSize;
              initdata.m_iReqType = (!m_bRendezvous) ? -1 : -2;
              initdata.m_iID = m_SocketID;

              char* hs = new char [m_iPayloadSize];
              int hs_size = m_iPayloadSize;
              initdata.serialize(hs, hs_size);
              sendCtrl(0, NULL, hs, hs_size);
              delete [] hs;
          }

          break;
      }

   case 5: //101 - Shutdown
      m_ullLastRspTime = currtime;
      m_bShutdown = true;
      m_bClosing = true;
      m_bBroken = true;
      m_iBrokenCounter = 60;

      // Signal the sender and recver if they are waiting for data.
      releaseSynch();

      CTimer::triggerEvent();

      break;

   case 8: // 1000 - An error has happened to the peer side
      //int err_type = packet.getAddInfo();

      // currently only this error is signalled from the peer side
      // if recvfile() failes (e.g., due to disk fail), blcoked sendfile/send should return immediately
      // giving the app a chance to fix the issue

      m_ullLastRspTime = currtime;
      m_bPeerHealth = false;

      break;

   case 32767: //0x7FFF - reserved and user defined messages
      m_ullLastRspTime = currtime;
      m_pCC->processCustomMsg(&ctrlpkt);
      CCUpdate();

      break;

   default:
      break;
   }
}

void CUDT::checkPktForward()
{
    uint64_t currtime;
    CTimer::rdtsc(currtime);
    if (m_pScoreBoard->empty())
        return;
    int seq = -1, offset;
    Block* block;
    while( -1 != (seq = m_pScoreBoard->getNextRetran()) ) {
        offset = CSeqNo::seqoff( m_iSndLastDataAck, seq ); 
        assert (offset >= 0);
        block = m_pSndBuffer->readData( offset, seq ); 
        if ( !block->is_reliable() ) {
            m_pScoreBoard->markRetran( seq );
            m_pScoreBoard->update( seq, NULL );
            m_pRateSample->onAckSacked( block, 3 );
            m_iSndForward = seq + 1;
        }
        else
            break;
    }
    if ( m_iSndLastDataAck < m_iSndForward ) {
        fprintf( stderr, "forward_ack: %d->%d\n", m_iSndLastDataAck, m_iSndForward );
        m_iSndLastDataAck = m_iSndForward;
        m_ullLastRspTime = currtime;     //reset RTO timer
    }
}

void CUDT::checkAppLimited()
{
    int cwnd = (m_iFlowWindowSize < (int)m_dCongestionWindow) ? m_iFlowWindowSize : (int)m_dCongestionWindow;
    if ( m_pSndBuffer->isEmpty() &&
            cwnd >= m_pRateSample->pktsInFlight() ) {
        m_pRateSample->setAppLimited( true );
    }
    else
        m_pRateSample->setAppLimited( false );
}

int CUDT::packData(CPacket& packet, uint64_t& ts)
{
   int payload = 0;

   uint64_t entertime = CTimer::getTime();


   CCUpdate();

   string send_pkt("send_pkt");

   Block* block = NULL;

   // Loss retransmission always has higher priority.
   if ((packet.m_iSeqNo = m_pScoreBoard->getNextRetran()) >= 0)
   {
      // protect m_iSndLastDataAck from updating by ACK processing
      CGuard ackguard(m_AckLock);

      int offset = CSeqNo::seqoff(m_iSndLastDataAck, packet.m_iSeqNo);
      if (offset < 0)
         return 0;

      if (CSeqNo::seqcmp( packet.m_iSeqNo, m_iSndHighSeqNo ) > 0 )
          throw std::runtime_error("resend seq bigger than SndHighSeqNo!\n");

      block = m_pSndBuffer->readData( offset, packet.m_iSeqNo );

      packet.m_pcData = block->m_pcData;
      packet.m_iMsgNo = block->m_iMsgNo;
      packet.m_iExtra = block->m_iExtra;
      packet.m_iForward = m_iSndForward;
      payload = block->m_iLength;

      m_pScoreBoard->markRetran( packet.m_iSeqNo );
      ++ m_iTraceRetrans;
      ++ m_iRetransTotal;
      send_pkt = string("resend_pkt");
      block->is_retrans_ = true;
   }
   else
   {
      // If no loss, pack a new packet.
      // check congestion/flow window limet
      int cwnd = (m_iFlowWindowSize < (int)m_dCongestionWindow) ? m_iFlowWindowSize : (int)m_dCongestionWindow;

      if (cwnd >= CSeqNo::seqlen(m_iSndLastDataAck, CSeqNo::incseq(m_iSndCurrSeqNo))
              || ( m_bLostRecovery && m_pRateSample->pktsInFlight() <= cwnd ) )
      {
         if (m_iSndHighSeqNo == m_iSndCurrSeqNo) {
             /*
             while ( (block = m_pSndBuffer->readCurrData()) != NULL ) {
                if ( !block->m_Drop )
                    break;
             }
             */

             /*
             double send_rate = 99999999.0;
             if ( m_dequeueTrace.size() >= 5 )
                 send_rate = ( m_dequeueTrace.size() - 1 ) * 12000.0 / ( m_dequeueTrace.back() - m_dequeueTrace.front() );
                 */
             
             double send_rate = m_dVideoRate;
             while ( ( block = m_pSndBuffer->readCurrData() ) != NULL ) {
                 if ( block->m_Drop )
                     continue;
                 else if ( (block->m_iMsgNo & 0xc0000000) == 0x80000000 &&
                         (block->m_iExtra & MASK_BITTHRESH) > (uint32_t)(send_rate * 1000) &&
                         (int32_t)(block->m_iMsgNo & MASK_MSGNO) != m_iSndCurrMsgNo ) {
                     m_iSndDropMsgNo = block->m_iMsgNo & MASK_MSGNO;
                     fprintf( stdout, "drop_msg msg_no: %u pace_rate: %u extra: %x\n",
                             block->m_iMsgNo & MASK_MSGNO,
                             (uint32_t)(send_rate * 1000),
                             block->m_iExtra
                             );
                 }
                 if ( (int32_t)(block->m_iMsgNo & MASK_MSGNO) == m_iSndDropMsgNo )
                     continue;
                 else
                     break;
             }

             if ( block ) {
                block->seq_ = CSeqNo::incseq(m_iSndCurrSeqNo);
                //m_pRateSample->onPktSent(block, ts);
             }
             else {
                 m_dequeueTrace.clear();
             }
         }
         else {
            block = m_pSndBuffer->readData( 
                    CSeqNo::seqoff( m_iSndLastDataAck, CSeqNo::incseq(m_iSndCurrSeqNo) ),
                    CSeqNo::incseq(m_iSndCurrSeqNo)
                    );
            send_pkt = string("resend_pkt");
            block->is_retrans_ = true;
         }
         if (block)
         {
            m_iSndCurrSeqNo = CSeqNo::incseq(m_iSndCurrSeqNo);
            m_pCC->setSndCurrSeqNo(m_iSndCurrSeqNo);

            packet.m_iSeqNo = block->seq_;
            packet.m_pcData = block->m_pcData;
            packet.m_iMsgNo = block->m_iMsgNo;
            packet.m_iExtra = block->m_iExtra;
            packet.m_iForward = m_iSndForward;
            payload = block->m_iLength;

            if (packet.m_iSeqNo > m_iSndHighSeqNo)
                m_iSndHighSeqNo = packet.m_iSeqNo;
         }
         else
         {
             fprintf( stderr, "SndBuffer empty!\n" );
             m_ullTargetTime = 0;
             m_ullTimeDiff = 0;
             ts = 0;
             return 0;
         }
      }
      else
      {
         m_ullTargetTime = 0;
         m_ullTimeDiff = 0;
         ts = 0;
         return 0;
      }
   }

   m_dequeueTrace.push_back( ts ); 
   // first 100,000 us
   while( m_dequeueTrace.size() > 5 &&
           m_dequeueTrace.front() < ( ts - 100000 ) ) {
       m_dequeueTrace.pop_front();
   }

   packet.m_iID = m_PeerID;
   packet.setLength(payload);

   uint64_t deliver_interval = (uint64_t) m_ullInterval * (packet.getLength() + 52) / 1500.0; 
   if ((0 != m_ullTargetTime) && (entertime > m_ullTargetTime))
      m_ullTimeDiff += entertime - m_ullTargetTime;

   if (m_ullTimeDiff >= deliver_interval)
   {
       ts = entertime;
       m_ullTimeDiff -= deliver_interval;
   }
   else
   {
       ts = entertime + deliver_interval - m_ullTimeDiff;
       m_ullTimeDiff = 0;
   }

   if ( block && !block->is_retrans_ ) {
       m_pRateSample->onPktSent(block, ts);
   }


   ++ m_llTraceSent;
   ++ m_llSentTotal;

   m_pCC->onPktSent(&packet);

   fprintf( stderr, "%s seq: %d msg_no: %d wildcard: %x size: %d SndCumuAck: %d SndCurrSeq: %d send_ts: %.2fms ullInterval: %ldms\n",
           send_pkt.c_str(),
           packet.m_iSeqNo,
           packet.getMsgNo(),
           packet.m_nHeader[2],
           //packet.getLength() + CPacket::m_iPktHdrSize,
           packet.getLength() + 52,
           m_iSndLastDataAck,
           m_iSndCurrSeqNo,
           ts / 1000.0,
           m_ullInterval
           );

   m_ullTargetTime = ts;

   return payload;
}

void CUDT::updateRcvWnd( int32_t seq )
{
    while( seq + 1 - m_iRcvCurrSeqNo >= (m_iRcvWndSize - 1) ) {
        throw std::runtime_error("Recv window overflow!\n");
    }
    // duplicate pkt
    if ( seq <= m_iRcvCurrSeqNo ) {
        return;
    }
    if ( seq > m_iRcvCurrSeqNo &&
            seq < m_iRcvHighSeqNo &&
            m_pRcvSeen[ seq & m_iRcvWndMask ] ) {
        return;
    }
    // pkt.seq is gt max_seen
    if ( seq > m_iRcvHighSeqNo ) {
        for( int i = m_iRcvHighSeqNo + 1; i < seq; ++i ) {
            m_pRcvSeen[ i & m_iRcvWndMask ] = 0;
        }
        m_iRcvHighSeqNo = seq;
        m_pRcvSeen[ m_iRcvHighSeqNo & m_iRcvWndMask ] = 1;
        m_pRcvSeen[ (m_iRcvHighSeqNo + 1) & m_iRcvWndMask ] = 0;
    }
    int recvCurrSeq = m_iRcvCurrSeqNo;
    if ( seq > m_iRcvCurrSeqNo ) {
        m_pRcvSeen[ seq & m_iRcvWndMask ] = 1;
        while( m_pRcvSeen[ (recvCurrSeq + 1) & m_iRcvWndMask ] != 0 ) {
            recvCurrSeq += 1;
        }
        m_iRcvCurrSeqNo = recvCurrSeq;
    }
}

// [left, right), left to right-1 are all received
/*
void CUDT::getSackArray( int32_t* sack_array, int* sack_num) {
    int sack_n = 0;
    int32_t left = CSeqNo::incseq(m_iRcvCurrSeqNo);
    for (; left <= m_iRcvHighSeqNo; left++) {
        if( m_pRcvSeen[ left & m_iRcvWndMask ] == 0 )
            continue;
        for (int32_t right = left; ; right++) {
            if (m_pRcvSeen[ right & m_iRcvWndMask ] == 0) {
                sack_array[SACK_LEFT(sack_n)] = left;
                sack_array[SACK_RIGHT(sack_n)] = right;
                sack_n += 1;
                if  (sack_n == m_iSackBlkNum)
                    goto EXIT;
                left = right;
                break;
            }
        }
    }

EXIT:
    *sack_num = sack_n;
}
*/

void CUDT::getSackArray( int32_t* sack_array, int* sack_num) {
    int sack_n = 0;
    int32_t right = m_iRcvHighSeqNo;
    for(; right > (m_iRcvCurrSeqNo + 1); right--) {
        if ( m_pRcvSeen[ right & m_iRcvWndMask ] == 0 )
            continue;
        for( int32_t left = right; ; left-- ) {
            if ( left <= (m_iRcvCurrSeqNo + 1) || 
                    m_pRcvSeen[ left & m_iRcvWndMask ] == 0 ) {
                sack_array[SACK_LEFT(sack_n)] = left + 1;
                sack_array[SACK_RIGHT(sack_n)] = right + 1;
                sack_n += 1;
                if (sack_n == m_iSackBlkNum)
                    goto EXIT;
                right = left + 1;
                break;
            }
        }
    }
EXIT:
    *sack_num = sack_n;
}

int CUDT::processData(CUnit* unit)
{
   CPacket& packet = unit->m_Packet;

   // Just heard from the peer, reset the expiration count.
   m_iEXPCount = 1;
   uint64_t currtime;
   CTimer::rdtsc(currtime);
   m_ullLastRspTime = currtime;

   m_pCC->onPktReceived(&packet);
   ++ m_iPktCount;
   // update time information
   m_pRcvTimeWindow->onPktArrival();

   // check if it is probing packet pair
   if (0 == (packet.m_iSeqNo & 0xF))
      m_pRcvTimeWindow->probe1Arrival();
   else if (1 == (packet.m_iSeqNo & 0xF))
      m_pRcvTimeWindow->probe2Arrival();

   ++ m_llTraceRecv;
   ++ m_llRecvTotal;

   fprintf(stderr, "recv_pkt seq: %d msg_no: %d RcvCurrSeq: %d RcvHighSeq: %d recv_buf: %d recv_ts: %ldms\n", 
           packet.m_iSeqNo, 
           packet.getMsgNo(),
           m_iRcvCurrSeqNo, 
           m_iRcvHighSeqNo, 
           m_pRcvBuffer->getAvailBufSize(),
           duration_cast< milliseconds >( system_clock::now().time_since_epoch() ).count()
           );

   // 1. save the data unit in recv_buffer
   // 2. update recv window to get latest ack
   int32_t offset = CSeqNo::seqoff(m_iRcvLastAck, packet.m_iSeqNo);
   if ( offset >= m_pRcvBuffer->getAvailBufSize() ) {
       char error_str[100];
       sprintf( error_str, "Recv buffer overflow, last_ack: %u, seq: %u, bufsize: %u\n",
               m_iRcvLastAck, packet.m_iSeqNo, m_pRcvBuffer->getAvailBufSize() );
       throw std::runtime_error(error_str);
   }
   if ( offset >= 0 ) {
       // it's possible the received data is received earlier
       m_pRcvBuffer->addData(unit, offset);
       if (m_bSynRecving) {
           pthread_mutex_lock(&m_RecvDataLock);
           pthread_cond_signal(&m_RecvDataCond);
           pthread_mutex_unlock(&m_RecvDataLock);
       }
   }

   updateRcvWnd(packet.m_iSeqNo);
   // forward the cumu.ack from forward
   offset = CSeqNo::seqoff(m_iRcvCurrSeqNo, packet.m_iForward);
   if ( offset > 1 ) {
       for ( int32_t seq = m_iRcvCurrSeqNo + 1; seq < packet.m_iForward; ++seq ) {
           updateRcvWnd( seq );
       }
   }

   // ack anyway
   int32_t sack_num = 0;
   int32_t sack_array [m_iSackBlkNum * 2 + 1];

   if ( m_iRcvCurrSeqNo < m_iRcvHighSeqNo ) {
       getSackArray( sack_array, &sack_num );
       sack_array[0] = sack_num;
   }

   if (sack_num > 0) {
       // it carries the received seq that trigger this sack
       sendCtrl(3, &packet.m_iSeqNo, sack_array, 4 * (2 * sack_num + 1) );
       fprintf(stderr, "send_sack, ack: %d RcvHighSeq: %d RecvSeq: %d", 
               m_iRcvCurrSeqNo + 1,
               m_iRcvHighSeqNo,
               packet.m_iSeqNo);
       for(int i = 0; i < sack_num; i++) {
           fprintf(stderr, "[%d, %d]\t", 
                   sack_array[ SACK_LEFT(i) ],
                   sack_array[ SACK_RIGHT(i) ]);
       }
       fprintf(stderr, "ts: %ldms\n", CTimer::getTime() / 1000);
   }
   else {
       sendCtrl(2, &packet.m_iSeqNo);
       fprintf(stderr, "send_ack, ack: %d RcvHighSeq: %d RecvSeq: %d ts: %ldms\n", 
               m_iRcvCurrSeqNo+1,
               m_iRcvHighSeqNo,
               packet.m_iSeqNo,
               CTimer::getTime() / 1000);
   }

   return 0;
   /*
   // Loss detection.
   if (CSeqNo::seqcmp(packet.m_iSeqNo, CSeqNo::incseq(m_iRcvCurrSeqNo)) > 0)
   {
      // If loss found, insert them to the receiver loss list
      m_pRcvLossList->insert(CSeqNo::incseq(m_iRcvCurrSeqNo), CSeqNo::decseq(packet.m_iSeqNo));

      // pack loss list for NAK
      int32_t lossdata[2];
      lossdata[0] = CSeqNo::incseq(m_iRcvCurrSeqNo) | 0x80000000;
      lossdata[1] = CSeqNo::decseq(packet.m_iSeqNo);

      // Generate loss report immediately.
      sendCtrl(3, NULL, lossdata, (CSeqNo::incseq(m_iRcvCurrSeqNo) == CSeqNo::decseq(packet.m_iSeqNo)) ? 1 : 2);

      int loss = CSeqNo::seqlen(m_iRcvCurrSeqNo, packet.m_iSeqNo) - 2;
      m_iTraceRcvLoss += loss;
      m_iRcvLossTotal += loss;
   }

   // This is not a regular fixed size packet...   
   //an irregular sized packet usually indicates the end of a message, so send an ACK immediately   
   if (packet.getLength() != m_iPayloadSize)   
      CTimer::rdtsc(m_ullNextACKTime); 

   // Update the current largest sequence number that has been received.
   // Or it is a retransmitted packet, remove it from receiver loss list.
   if (CSeqNo::seqcmp(packet.m_iSeqNo, m_iRcvCurrSeqNo) > 0)
      m_iRcvCurrSeqNo = packet.m_iSeqNo;
   else
      m_pRcvLossList->remove(packet.m_iSeqNo);
   return 0;
   */
}

int CUDT::listen(sockaddr* addr, CPacket& packet)
{
   if (m_bClosing)
      return 1002;

   if (packet.getLength() != CHandShake::m_iContentSize)
      return 1004;

   CHandShake hs;
   hs.deserialize(packet.m_pcData, packet.getLength());

   // SYN cookie
   char clienthost[NI_MAXHOST];
   char clientport[NI_MAXSERV];
   getnameinfo(addr, (AF_INET == m_iVersion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6), clienthost, sizeof(clienthost), clientport, sizeof(clientport), NI_NUMERICHOST|NI_NUMERICSERV);
   int64_t timestamp = (CTimer::getTime() - m_StartTime) / 60000000; // secret changes every one minute
   stringstream cookiestr;
   cookiestr << clienthost << ":" << clientport << ":" << timestamp;
   unsigned char cookie[16];
   CMD5::compute(cookiestr.str().c_str(), cookie);

   if (1 == hs.m_iReqType)
   {
      hs.m_iCookie = *(int*)cookie;
      packet.m_iID = hs.m_iID;
      int size = packet.getLength();
      hs.serialize(packet.m_pcData, size);
      m_pSndQueue->sendto(addr, packet);
      return 0;
   }
   else
   {
      if (hs.m_iCookie != *(int*)cookie)
      {
         timestamp --;
         cookiestr << clienthost << ":" << clientport << ":" << timestamp;
         CMD5::compute(cookiestr.str().c_str(), cookie);

         if (hs.m_iCookie != *(int*)cookie)
            return -1;
      }
   }

   int32_t id = hs.m_iID;

   // When a peer side connects in...
   if ((1 == packet.getFlag()) && (0 == packet.getType()))
   {
      if ((hs.m_iVersion != m_iVersion) || (hs.m_iType != m_iSockType))
      {
         // mismatch, reject the request
         hs.m_iReqType = 1002;
         int size = CHandShake::m_iContentSize;
         hs.serialize(packet.m_pcData, size);
         packet.m_iID = id;
         m_pSndQueue->sendto(addr, packet);
      }
      else
      {
         int result = s_UDTUnited.newConnection(m_SocketID, addr, &hs);
         if (result == -1)
            hs.m_iReqType = 1002;

         // send back a response if connection failed or connection already existed
         // new connection response should be sent in connect()
         if (result != 1)
         {
            int size = CHandShake::m_iContentSize;
            hs.serialize(packet.m_pcData, size);
            packet.m_iID = id;
            m_pSndQueue->sendto(addr, packet);
         }
         else
         {
            // a new connection has been created, enable epoll for write 
            s_UDTUnited.m_EPoll.update_events(m_SocketID, m_sPollID, UDT_EPOLL_OUT, true);
         }
      }
   }

   return hs.m_iReqType;
}

void CUDT::checkTimers()
{
   // update CC parameters
   CCUpdate();

   uint64_t currtime;
   CTimer::rdtsc(currtime);

   uint64_t next_exp_time;
   // temporarily set timeout to 400ms(3RTT)
   //next_exp_time = m_ullLastRspTime + m_pCC->m_iRTO * m_ullCPUFrequency;
   next_exp_time = m_ullLastRspTime + 5000000 * m_ullCPUFrequency;

   if (currtime > next_exp_time)
   {
      // Haven't receive any information from the peer, is it dead?!
      // timeout: at least 16 expirations and must be greater than 10 seconds
      //if ( m_iEXPCount > 16 )
      if ( m_iEXPCount > 5 )
      {
         //
         // Connection is broken. 
         // UDT does not signal any information about this instead of to stop quietly.
         // Application will detect this when it calls any UDT methods next time.
         //
         m_bClosing = true;
         m_bBroken = true;
         m_iBrokenCounter = 30;

         // update snd U list to remove this socket
         m_pSndQueue->m_pSndUList->update(this);

         releaseSynch();

         // app can call any UDT API to learn the connection_broken error
         s_UDTUnited.m_EPoll.update_events(m_SocketID, m_sPollID, UDT_EPOLL_IN | UDT_EPOLL_OUT | UDT_EPOLL_ERR, true);

         CTimer::triggerEvent();

         return;
      }

      // sender: Insert all the packets sent after last received acknowledgement into the sender loss list.
      if (m_pSndBuffer->getCurrBufSize() > 0)
      {
         for ( int32_t seq = m_iSndLastDataAck ; seq <= m_iSndCurrSeqNo; ++seq ) {
            int offset = CSeqNo::seqoff( m_iSndLastDataAck, seq ); 
            Block* block = m_pSndBuffer->readData( offset, seq ); 
            if ( block->is_reliable() )
                m_iSndCurrSeqNo = seq;
         }
         m_iSndLastDataAck = m_iSndCurrSeqNo + 1;
         m_iSndForward = m_iSndLastDataAck + 1;

         m_pCC->onTimeout();
         m_pRateSample->onTimeout( m_iSndLastDataAck );
         CCUpdate();

         fprintf(stderr, "Timeout! RTO:%ldus Timeout:%ldus\n",
                 (next_exp_time - m_ullLastRspTime) / m_ullCPUFrequency,
                 (currtime - m_ullLastRspTime) / m_ullCPUFrequency);
         m_pScoreBoard->clear();

         // immediately restart transmission
         m_pSndQueue->m_pSndUList->update(this, false);
      }

      ++ m_iEXPCount;
      // Reset last response time since we just sent a heart-beat.
      m_ullLastRspTime = currtime;
   }
}

void CUDT::addEPoll(const int eid)
{
   CGuard::enterCS(s_UDTUnited.m_EPoll.m_EPollLock);
   m_sPollID.insert(eid);
   CGuard::leaveCS(s_UDTUnited.m_EPoll.m_EPollLock);

   if (!m_bConnected || m_bBroken || m_bClosing)
      return;

   if (((UDT_STREAM == m_iSockType) && (m_pRcvBuffer->getRcvDataSize() > 0)) ||
      ((UDT_DGRAM == m_iSockType) && (m_pRcvBuffer->getRcvMsgNum() > 0)))
   {
      s_UDTUnited.m_EPoll.update_events(m_SocketID, m_sPollID, UDT_EPOLL_IN, true);
   }
   if (m_iSndBufSize > m_pSndBuffer->getCurrBufSize())
   {
      s_UDTUnited.m_EPoll.update_events(m_SocketID, m_sPollID, UDT_EPOLL_OUT, true);
   }
}

void CUDT::removeEPoll(const int eid)
{
   // clear IO events notifications;
   // since this happens after the epoll ID has been removed, they cannot be set again
   set<int> remove;
   remove.insert(eid);
   s_UDTUnited.m_EPoll.update_events(m_SocketID, remove, UDT_EPOLL_IN | UDT_EPOLL_OUT, false);

   CGuard::enterCS(s_UDTUnited.m_EPoll.m_EPollLock);
   m_sPollID.erase(eid);
   CGuard::leaveCS(s_UDTUnited.m_EPoll.m_EPollLock);
}

//
// Copyright (C) 2005 Michael Tuexen
// Copyright (C) 2008 Irene Ruengeler
// Copyright (C) 2009 Thomas Dreibholz
// Copyright (C) 2009 Thomas Reschka
// Copyright (C) 2011 Zoltan Bojthe
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//


#include <errno.h>

#include "PcapRecorder.h"

#include "IPProtocolId_m.h"

#ifdef WITH_UDP
#include "UDPPacket_m.h"
#endif

#ifdef WITH_SCTP
#include "SCTPMessage.h"
#include "SCTPAssociation.h"
#endif

#ifdef WITH_TCP_BASE
#include "TCPSegment.h"
#endif

#ifdef WITH_IPv4
#include "ICMPMessage.h"
#include "IPAddress.h"
#include "IPControlInfo_m.h"
#include "IPDatagram.h"
#include "IPSerializer.h"
#endif

#ifdef WITH_IPv6
#include "IPv6Datagram.h"
#endif

#if !defined(_WIN32) && !defined(__CYGWIN__) && !defined(_WIN64)
#include <netinet/in.h>  // htonl, ntohl, ...
#endif

#define MAXBUFLENGTH 65536

#define PCAP_MAGIC           0xa1b2c3d4

/* "libpcap" file header (minus magic number). */
struct pcap_hdr {
     uint32 magic;      /* magic */
     uint16 version_major;   /* major version number */
     uint16 version_minor;   /* minor version number */
     uint32 thiszone;   /* GMT to local correction */
     uint32 sigfigs;        /* accuracy of timestamps */
     uint32 snaplen;        /* max length of captured packets, in octets */
     uint32 network;        /* data link type */
};

/* "libpcap" record header. */
struct pcaprec_hdr {
     int32  ts_sec;     /* timestamp seconds */
     uint32 ts_usec;        /* timestamp microseconds */
     uint32 incl_len;   /* number of octets of packet saved in file */
     uint32 orig_len;   /* actual length of packet */
};

typedef struct {
     uint8  dest_addr[6];
     uint8  src_addr[6];
     uint16 l3pid;
} hdr_ethernet_t;



PcapDumper::PcapDumper(std::ostream& out)
{
     outp = &out;
}

PcapDumper::~PcapDumper()
{
}

void PcapDumper::ipDump(const char *label, IPDatagram *dgram, const char *comment)
{
#ifdef WITH_SCTP
     if (dynamic_cast<SCTPMessage *>(dgram->getEncapsulatedPacket()))
     {
          SCTPMessage *sctpmsg = check_and_cast<SCTPMessage *>(dgram->getEncapsulatedPacket());
          if (dgram->hasBitError())
                sctpmsg->setBitError(true);
          sctpDump(label, sctpmsg, dgram->getSrcAddress().str(), dgram->getDestAddress().str(), comment);
     }
     else
#endif
     ;
}

void PcapDumper::sctpDump(const char *label, SCTPMessage *sctpmsg, const std::string& srcAddr, const std::string& destAddr, const char *comment)
{
#ifdef WITH_SCTP
     std::ostream& out = *outp;
     uint32 numberOfChunks;
     SCTPChunk* chunk;
     uint8 type;
     // seq and time (not part of the tcpdump format)
     char buf[30];
     sprintf(buf,"[%.3f%s] ", simulation.getSimTime().dbl(), label);
     out << buf;

     // src/dest
     out << srcAddr  << "." << sctpmsg->getSrcPort()  << " > ";

     out << destAddr << "." << sctpmsg->getDestPort() << ": ";

     if (sctpmsg->hasBitError())
     {
          sctpmsg->setChecksumOk(false);
     }

     numberOfChunks = sctpmsg->getChunksArraySize();
     out << "numberOfChunks="<<numberOfChunks<<" VTag="<<sctpmsg->getTag()<<"\n";

     if (sctpmsg->hasBitError())
          out << "Packet has bit error!!\n";

     for (uint32 i=0; i<numberOfChunks; i++)
     {
          chunk = (SCTPChunk*)sctpmsg->getChunks(i);
          type  = chunk->getChunkType();

          // FIXME create a getChunkTypeName(SCTPChunkType x) function in SCTP code and use it!
          switch (type)
          {
                case INIT:
                     out << "INIT ";
                     break;
                case INIT_ACK:
                     out << "INIT_ACK ";
                     break;
                case COOKIE_ECHO:
                     out << "COOKIE_ECHO ";
                     break;
                case COOKIE_ACK:
                     out << "COOKIE_ACK ";
                     break;
                case DATA:
                     out << "DATA ";
                     break;
                case SACK:
                     out << "SACK ";
                     break;
                case HEARTBEAT:
                     out << "HEARTBEAT ";
                     break;
                case HEARTBEAT_ACK:
                     out << "HEARTBEAT_ACK ";
                     break;
                case ABORT:
                     out << "ABORT ";
                     break;
                case SHUTDOWN:
                     out << "SHUTDOWN ";
                     break;
                case SHUTDOWN_ACK:
                     out << "SHUTDOWN_ACK ";
                     break;
                case SHUTDOWN_COMPLETE:
                     out << "SHUTDOWN_COMPLETE ";
                     break;
                case ERRORTYPE:
                     out << "ERROR";
                     break;
          }
     }

     if (verbosity >= 1)
     {
          out << endl;

          for (uint32 i = 0; i < numberOfChunks; i++)
          {
                chunk = (SCTPChunk*)sctpmsg->getChunks(i);
                type = chunk->getChunkType();

                sprintf(buf,  "   %3u: ", i + 1);
                out << buf;

                switch (type)
                {
                     case INIT:
                     {
                          SCTPInitChunk* initChunk;
                          initChunk = check_and_cast<SCTPInitChunk *>(chunk);
                          out << "INIT[InitiateTag=";
                          out << initChunk->getInitTag();
                          out << "; a_rwnd=";
                          out << initChunk->getA_rwnd();
                          out << "; OS=";
                          out << initChunk->getNoOutStreams();
                          out << "; IS=";
                          out << initChunk->getNoInStreams();
                          out << "; InitialTSN=";
                          out << initChunk->getInitTSN();

                          if (initChunk->getAddressesArraySize() > 0)
                          {
                                out <<"; Addresses=";

                                for (uint32 i = 0; i < initChunk->getAddressesArraySize(); i++)
                                {
                                     if (i > 0)
                                          out << ",";

                                     if (initChunk->getAddresses(i).isIPv6())
                                          out << initChunk->getAddresses(i).str();
                                     else
                                          out << initChunk->getAddresses(i);
                                }
                          }

                          out <<"]";
                          break;
                     }

                     case INIT_ACK:
                     {
                          SCTPInitAckChunk* initackChunk;
                          initackChunk = check_and_cast<SCTPInitAckChunk *>(chunk);
                          out << "INIT_ACK[InitiateTag=";
                          out << initackChunk->getInitTag();
                          out << "; a_rwnd=";
                          out << initackChunk->getA_rwnd();
                          out << "; OS=";
                          out << initackChunk->getNoOutStreams();
                          out << "; IS=";
                          out << initackChunk->getNoInStreams();
                          out << "; InitialTSN=";
                          out << initackChunk->getInitTSN();
                          out << "; CookieLength=";
                          out << initackChunk->getCookieArraySize();

                          if (initackChunk->getAddressesArraySize() > 0)
                          {
                                out <<"; Addresses=";

                                for (uint32 i = 0; i < initackChunk->getAddressesArraySize(); i++)
                                {
                                     if (i > 0)
                                          out << ",";

                                     out << initackChunk->getAddresses(i);
                                }
                          }

                          out <<"]";
                          break;
                     }

                     case COOKIE_ECHO:
                          out << "COOKIE_ECHO[CookieLength=";
                          out <<     chunk->getBitLength()/8 - 4;
                          out <<"]";
                          break;

                     case COOKIE_ACK:
                          out << "COOKIE_ACK ";
                          break;

                     case DATA:
                     {
                          SCTPDataChunk* dataChunk;
                          dataChunk = check_and_cast<SCTPDataChunk *>(chunk);
                          out << "DATA[TSN=";
                          out << dataChunk->getTsn();
                          out << "; SID=";
                          out << dataChunk->getSid();
                          out << "; SSN=";
                          out << dataChunk->getSsn();
                          out << "; PPID=";
                          out << dataChunk->getPpid();
                          out << "; PayloadLength=";
                          out << dataChunk->getBitLength()/8 - 16;
                          out <<"]";
                          break;
                     }

                     case SACK:
                     {
                          SCTPSackChunk* sackChunk;
                          sackChunk = check_and_cast<SCTPSackChunk *>(chunk);
                          out << "SACK[CumTSNAck=";
                          out << sackChunk->getCumTsnAck();
                          out << "; a_rwnd=";
                          out << sackChunk->getA_rwnd();

                          if (sackChunk->getGapStartArraySize() > 0)
                          {
                                out <<"; Gaps=";
                                for (uint32 i = 0; i < sackChunk->getGapStartArraySize(); i++)
                                {
                                     if (i > 0)
                                          out << ", ";
                                     out << sackChunk->getGapStart(i) << "-" << sackChunk->getGapStop(i);
                                }
                          }

                          if (sackChunk->getDupTsnsArraySize() > 0)
                          {
                                out <<"; Dups=";

                                for (uint32 i = 0; i < sackChunk->getDupTsnsArraySize(); i++)
                                {
                                     if (i > 0)
                                          out << ", ";

                                     out << sackChunk->getDupTsns(i);
                                }
                          }

                          out <<"]";
                          break;
                     }

                     case HEARTBEAT:
                          SCTPHeartbeatChunk* heartbeatChunk;
                          heartbeatChunk = check_and_cast<SCTPHeartbeatChunk *>(chunk);
                          out << "HEARTBEAT[InfoLength=";
                          out <<     chunk->getBitLength()/8 - 4;
                          out << "; time=";
                          out << heartbeatChunk->getTimeField();
                          out <<"]";
                          break;

                     case HEARTBEAT_ACK:
                          out << "HEARTBEAT_ACK[InfoLength=";
                          out <<     chunk->getBitLength()/8 - 4;
                          out <<"]";
                          break;

                     case ABORT:
                          SCTPAbortChunk* abortChunk;
                          abortChunk = check_and_cast<SCTPAbortChunk *>(chunk);
                          out << "ABORT[T-Bit=";
                          out << abortChunk->getT_Bit();
                          out << "]";
                          break;

                     case SHUTDOWN:
                          SCTPShutdownChunk* shutdown;
                          shutdown = check_and_cast<SCTPShutdownChunk *>(chunk);
                          out << "SHUTDOWN[CumTSNAck=";
                          out << shutdown->getCumTsnAck();
                          out << "]";
                          break;

                     case SHUTDOWN_ACK:
                          out << "SHUTDOWN_ACK ";
                          break;

                     case SHUTDOWN_COMPLETE:
                          out << "SHUTDOWN_COMPLETE ";
                          break;

                     case ERRORTYPE:
                     {
                          SCTPErrorChunk* errorChunk;
                          errorChunk = check_and_cast<SCTPErrorChunk*>(chunk);
                          uint32 numberOfParameters = errorChunk->getParametersArraySize();
                          uint32 parameterType;

                          for (uint32 i=0; i<numberOfParameters; i++)
                          {
                                SCTPParameter* param = (SCTPParameter*)errorChunk->getParameters(i);
                                parameterType   = param->getParameterType();
                          }

                          break;
                     }
                }
                out << endl;
          }
     }

     // comment
     if (comment)
          out << "# " << comment;

     out << endl;
#endif
}

PcapRecorder::~PcapRecorder()
{
}

void PcapDumper::dump(const char *label, const char *msg)
{
     std::ostream& out = *outp;

     // seq and time (not part of the tcpdump format)
     char buf[30];
     sprintf(buf,"[%.3f%s] ", simulation.getSimTime().dbl(), label);
     out << buf;

     out << msg << "\n";
}

//----

Define_Module(PcapRecorder);

PcapRecorder::PcapRecorder() : cSimpleModule(), pcapDumper(ev.getOStream())
{
}

void PcapDumper::udpDump(bool l2r, const char *label, IPDatagram *dgram, const char *comment)
{
#if defined(WITH_UDP) && defined(WITH_IPv4)
    cMessage *encapmsg = dgram->getEncapsulatedPacket();

    if (dynamic_cast<UDPPacket *>(encapmsg))
    {
        std::ostream& out = *outp;

        // seq and time (not part of the tcpdump format)
        char buf[30];
        sprintf(buf,"[%.3f%s] ", simulation.getSimTime().dbl(), label);
        out << buf;
        UDPPacket* udppkt = check_and_cast<UDPPacket*>(encapmsg);

        // src/dest
        if (l2r)
        {
            out << dgram->getSrcAddress().str() << "." << udppkt->getSourcePort() << " > ";
            out << dgram->getDestAddress().str() << "." << udppkt->getDestinationPort() << ": ";
        }
        else
        {
            out << dgram->getDestAddress().str() << "." << udppkt->getDestinationPort() << " < ";
            out << dgram->getSrcAddress().str() << "." << udppkt->getSourcePort() << ": ";
        }

        //out << endl;
        out << "UDP: Payload length=" << udppkt->getByteLength()-8 << endl;

#ifdef WITH_SCTP
        if (udppkt->getSourcePort() == 9899 || udppkt->getDestinationPort() == 9899)
        {
            if (dynamic_cast<SCTPMessage *>(udppkt->getEncapsulatedPacket()))
                sctpDump("", (SCTPMessage *)(udppkt->getEncapsulatedPacket()), std::string(l2r?"A":"B"),std::string(l2r?"B":"A"));
        }
#endif
    }
#endif
}

void PcapDumper::dumpIPv4(bool l2r, const char *label, IPDatagram *dgram, const char *comment)
{
#ifdef WITH_IPv4
    cMessage *encapmsg = dgram->getEncapsulatedPacket();

#ifdef WITH_TCP_BASE
    if (dynamic_cast<TCPSegment *>(encapmsg))
    {
         // if TCP, dump as TCP
         tcpDump(l2r, label, (TCPSegment *)encapmsg, dgram->getSrcAddress().str(), dgram->getDestAddress().str(), comment);
    }
    else
#endif
    {
         // some other packet, dump what we can
         std::ostream& out = *outp;

         // seq and time (not part of the tcpdump format)
         char buf[30];
         sprintf(buf,"[%.3f%s] ", SIMTIME_DBL(simTime()), label);
         out << buf;

         // packet class and name
         out << "? " << encapmsg->getClassName() << " \"" << encapmsg->getName() << "\"\n";
    }
#else
    throw cRuntimeError("INET compiled without IPv4 features!");
#endif
}

void PcapDumper::dumpIPv6(bool l2r, const char *label, IPv6Datagram *dgram, const char *comment)
{
#ifdef WITH_IPv6
    cMessage *encapmsg = dgram->getEncapsulatedPacket();

#ifdef WITH_TCP_BASE
    if (dynamic_cast<TCPSegment *>(encapmsg))
    {
         // if TCP, dump as TCP
         tcpDump(l2r, label, (TCPSegment *)encapmsg, dgram->getSrcAddress().str(), dgram->getDestAddress().str(), comment);
    }
    else
#endif
    {
         // some other packet, dump what we can
         std::ostream& out = *outp;

         // seq and time (not part of the tcpdump format)
         char buf[30];
         sprintf(buf,"[%.3f%s] ", SIMTIME_DBL(simTime()), label);
         out << buf;

         // packet class and name
         out << "? " << encapmsg->getClassName() << " \"" << encapmsg->getName() << "\"\n";
    }
#else
    throw cRuntimeError("INET compiled without IPv6 features!");
#endif
}

void PcapDumper::tcpDump(bool l2r, const char *label, TCPSegment *tcpseg, const std::string& srcAddr, const std::string& destAddr, const char *comment)
{
#ifdef WITH_TCP_BASE
     std::ostream& out = *outp;

    // seq and time (not part of the tcpdump format)
    char buf[30];
    sprintf(buf,"[%.3f%s] ", SIMTIME_DBL(simTime()), label);
    out << buf;

    // src/dest ports
    if (l2r)
    {
        out << srcAddr << "." << tcpseg->getSrcPort() << " > ";
        out << destAddr << "." << tcpseg->getDestPort() << ": ";
    }
    else
    {
        out << destAddr << "." << tcpseg->getDestPort() << " < ";
        out << srcAddr << "." << tcpseg->getSrcPort() << ": ";
    }

    // flags
    bool flags = false;
    if (tcpseg->getUrgBit()) {flags=true; out << "U ";}
    if (tcpseg->getAckBit()) {flags=true; out << "A ";}
    if (tcpseg->getPshBit()) {flags=true; out << "P ";}
    if (tcpseg->getRstBit()) {flags=true; out << "R ";}
    if (tcpseg->getSynBit()) {flags=true; out << "S ";}
    if (tcpseg->getFinBit()) {flags=true; out << "F ";}
    if (!flags) {out << ". ";}

    // data-seqno
    if (tcpseg->getPayloadLength()>0 || tcpseg->getSynBit())
    {
        out << tcpseg->getSequenceNo() << ":" << tcpseg->getSequenceNo()+tcpseg->getPayloadLength();
        out << "(" << tcpseg->getPayloadLength() << ") ";
    }

    // ack
    if (tcpseg->getAckBit())
        out << "ack " << tcpseg->getAckNo() << " ";

    // window
    out << "win " << tcpseg->getWindow() << " ";

    // urgent
    if (tcpseg->getUrgBit())
        out << "urg " << tcpseg->getUrgentPointer() << " ";

    // options present?
    if (tcpseg->getHeaderLength() > 20)
    {
        std::string direction = "sent";

        if (l2r) // change direction
            {direction = "received";}

        unsigned short numOptions = tcpseg->getOptionsArraySize();
        out << "\nTCP Header Option(s) " << direction << ":\n";

        for (int i=0; i<numOptions; i++)
        {
            TCPOption option = tcpseg->getOptions(i);
            unsigned short kind = option.getKind();
            unsigned short length = option.getLength();
            out << (i+1) << ". option kind=" << kind << " length=" << length << "\n";
        }
    }

    // comment
    if (comment)
        out << "# " << comment;

     out << endl;
#else
    throw cRuntimeError("INET compiled without any TCP features!");
#endif
}

void PcapRecorder::initialize()
{
    struct pcap_hdr fh;
    const char* file = par("pcapFile");
    snaplen = this->par("snaplen");
    pcapDumper.setVerbosity(par("verbosity"));

    signalList.clear();

    {
        cStringTokenizer signalTokenizer(par("sendingSignalNames"));

        while (signalTokenizer.hasMoreTokens())
            signalList[registerSignal(signalTokenizer.nextToken())] = true;
    }

    {
        cStringTokenizer signalTokenizer(par("receivingSignalNames"));

        while (signalTokenizer.hasMoreTokens())
            signalList[registerSignal(signalTokenizer.nextToken())] = false;
    }

    const char* moduleNames = par("moduleNamePatterns");
    cStringTokenizer moduleTokenizer(moduleNames);

    while (moduleTokenizer.hasMoreTokens())
    {
        bool found = false;
        std::string mname(moduleTokenizer.nextToken());
        bool isAllIndex = (mname.length() > 3) && mname.rfind("[*]") == mname.length() - 3;

        if (isAllIndex)
            mname.replace(mname.length() - 3, 3, "");

        for (cModule::SubmoduleIterator i(getParentModule()); !i.end(); i++)
        {
            cModule *submod = i();
            if (0 == strcmp(isAllIndex ? submod->getName() : submod->getFullName(), mname.c_str()))
            {
                found = true;

                for (SignalList::iterator s = signalList.begin(); s != signalList.end(); s++)
                {
                    if (!submod->isSubscribed(s->first, this))
                    {
                        submod->subscribe(s->first, this);
                        EV << "PcapRecorder " << getFullPath() << " subscribed to "
                           << submod->getFullPath() << ":" << getSignalName(s->first) << endl;
                    }
                }
            }
        }

        if (!found)
        {
            EV << "The module " << mname << (isAllIndex ? "[*]" : "")
                    << " not found for PcapRecorder " << getFullPath() << endl;
        }
    }

    if (0 != strcmp(file, ""))
    {
        pcapDumper.dumpfile = fopen(file, "wb");

        if (!pcapDumper.dumpfile)
        {
            fprintf(stderr, "Cannot open file [%s] for writing: %s\n", file, strerror(errno));
            exit(-1);
        }

        fh.magic = PCAP_MAGIC;
        fh.version_major = 2;
        fh.version_minor = 4;
        fh.thiszone = 0;
        fh.sigfigs = 0;
        fh.snaplen = snaplen;
        fh.network = 0;
        fwrite(&fh, sizeof(fh), 1, pcapDumper.dumpfile);
    }
    else
        pcapDumper.dumpfile = NULL;
}

void PcapRecorder::handleMessage(cMessage *msg)
{
    throw cRuntimeError("This module does not handle messages");
}

void PcapRecorder::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj)
{
    cPacket *packet = dynamic_cast<cPacket *>(obj);

    if (packet)
    {
        SignalList::const_iterator i = signalList.find(signalID);
        bool l2r = (i != signalList.end()) ? i->second : true;
        recordPacket(packet, l2r);
    }
}

void PcapRecorder::recordPacket(cPacket *msg, bool l2r)
{
    if (!ev.disable_tracing)
    {
        EV << "PcapRecorder::recordPacket(" << msg->getFullPath() << ", " << l2r << ")\n";
#ifdef WITH_IPv4
        if (dynamic_cast<IPDatagram *>(msg))
        {
            if (((IPDatagram *)msg)->getTransportProtocol() == IP_PROT_SCTP)
            {
                pcapDumper.ipDump("", (IPDatagram *)msg);
            }
            else
            {
                if (msg->hasBitError())
                {
                    return; //FIXME is it correct?
                }

                if (((IPDatagram *)msg)->getTransportProtocol() == IP_PROT_TCP)
                {
                    pcapDumper.dumpIPv4(l2r, "", (IPDatagram *)msg, "");
                }
                else if (((IPDatagram *)msg)->getTransportProtocol() == IP_PROT_UDP)
                    pcapDumper.udpDump(l2r, "", (IPDatagram *)msg, "");
            }
        }
        else
#endif
#ifdef WITH_SCTP
        if (dynamic_cast<SCTPMessage *>(msg))
        {
            pcapDumper.sctpDump("", (SCTPMessage *)msg, std::string(l2r?"A":"B"),std::string(l2r?"B":"A"));
        }
        else
#endif
#ifdef WITH_TCP_BASE
        if (dynamic_cast<TCPSegment *>(msg))
        {
            if (PK(msg)->hasBitError())
            {
                return; //FIXME is it correct?
            }

            pcapDumper.tcpDump(l2r, "", (TCPSegment *)msg, std::string(l2r?"A":"B"),std::string(l2r?"B":"A"));
        }
        else
#endif
#ifdef WITH_IPv4
        if (dynamic_cast<ICMPMessage *>(msg))
        {
            if (msg->hasBitError())
            {
                return; // FIXME is it correct?
            }

            std::cout<<"ICMPMessage\n";
        }
        else
#endif
        {
            // search for encapsulated IP[v6]Datagram in it
            cPacket *encapmsg = msg;

            while (encapmsg
#ifdef WITH_IPv4
                    && dynamic_cast<IPDatagram *>(encapmsg) == NULL
#endif
#ifdef WITH_IPv6
                    && dynamic_cast<IPv6Datagram_Base *>(encapmsg) == NULL
#endif
                )
            {
                encapmsg = encapmsg->getEncapsulatedPacket();
            }

            if (!encapmsg)
            {
                //We do not want this to end in an error if EtherAutoconf messages
                //are passed, so just print a warning. -WEI
                EV << "CANNOT DECODE: packet " << msg->getName() << " doesn't contain either IP or IPv6 Datagram\n";
            }
            else
            {
                msg = encapmsg;

#ifdef WITH_IPv4
                if (dynamic_cast<IPDatagram *>(msg))
                    pcapDumper.dumpIPv4(l2r, "", (IPDatagram *)msg);
                else
#endif
#ifdef WITH_IPv6
                if (dynamic_cast<IPv6Datagram *>(msg))
                    pcapDumper.dumpIPv6(l2r, "", (IPv6Datagram *)msg);
                else
#endif
                    ASSERT(0); // cannot get here
            }
        }
    }

#ifdef WITH_IPv4
    if (pcapDumper.dumpfile == NULL)
        return;

    while (msg && NULL == dynamic_cast<IPDatagram *>(msg))
    {
        msg = msg->getEncapsulatedPacket();
    }

    if (dynamic_cast<IPDatagram *>(msg))
    {
        uint8 buf[MAXBUFLENGTH];
        memset((void*)&buf, 0, sizeof(buf));

        const simtime_t stime = simulation.getSimTime();
        // Write PCap header

        struct pcaprec_hdr ph;
        ph.ts_sec = (int32)stime.dbl();
        ph.ts_usec = (uint32)((stime.dbl() - ph.ts_sec) * 1000000);
         // Write Ethernet header
        uint32 hdr = 2; //AF_INET
                     //We do not want this to end in an error if EtherAutoconf messages
        IPDatagram *ipPacket = check_and_cast<IPDatagram *>(msg);
        int32 serialized_ip = IPSerializer().serialize(ipPacket, buf, sizeof(buf), true);
        ph.incl_len = serialized_ip + sizeof(uint32);

        ph.orig_len = ph.incl_len;
        fwrite(&ph, sizeof(ph), 1, pcapDumper.dumpfile);
        fwrite(&hdr, sizeof(uint32), 1, pcapDumper.dumpfile);
        fwrite(buf, serialized_ip, 1, pcapDumper.dumpfile);
    }
#endif
}

void PcapRecorder::finish()
{
     pcapDumper.dump("", "pcapRecorder finished");

     if (0 != strcmp(this->par("pcapFile"),""))
          fclose(pcapDumper.dumpfile);
}

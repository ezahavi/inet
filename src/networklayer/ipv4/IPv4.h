//
// Copyright (C) 2004 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#ifndef __INET_IP_H
#define __INET_IP_H

#include "INETDefs.h"

#include "ICMPAccess.h"
#include "IPv4FragBuf.h"
#include "ProtocolMap.h"
#include "QueueBase.h"


class ARPPacket;
class ICMPMessage;
class IInterfaceTable;
class IPv4Datagram;
class IRoutingTable;

// ICMP type 2, code 4: fragmentation needed, but don't-fragment bit set
const int ICMP_FRAGMENTATION_ERROR_CODE = 4;


/**
 * Implements the IPv4 protocol.
 */
class INET_API IPv4 : public QueueBase
{
  public:
    /**
     * Implements a Netfilter-like datagram hook
     */
    class Hook {
      public:
        enum Result {
            ACCEPT, /**< allow datagram to pass to next hook */
            DROP, /**< do not allow datagram to pass to next hook, delete it */
            QUEUE, /**< queue datagram for later re-injection */
            STOLEN /**< do not allow datagram to pass to next hook, but do not delete it */
        };

        virtual ~Hook() {};

        /**
        * called before a packet arriving from the network is routed
        */
        virtual Result datagramPreRoutingHook(IPv4Datagram* datagram, InterfaceEntry* inIE) = 0;

        /**
        * called before a packet arriving from the network is delivered locally
        */
        virtual Result datagramLocalInHook(IPv4Datagram* datagram, InterfaceEntry* inIE) = 0;

        /**
        * called before a packet arriving from the network is delivered via the network
        */
        virtual Result datagramForwardHook(IPv4Datagram* datagram, InterfaceEntry* inIE, InterfaceEntry*& outIE, IPv4Address& nextHopAddr) = 0;

        /**
        * called before a packet is delivered via the network
        */
        virtual Result datagramPostRoutingHook(IPv4Datagram* datagram, InterfaceEntry* inIE, InterfaceEntry*& outIE, IPv4Address& nextHopAddr) = 0;

        /**
        * called before a packet arriving locally is delivered
        */
        virtual Result datagramLocalOutHook(IPv4Datagram* datagram, InterfaceEntry*& outIE) = 0;
    };

    /**
     * Represents an IPv4Datagram, queued by a Hook
     */
    class QueuedDatagramForHook {
      public:
        enum Hook {
          PREROUTING,
          LOCALIN,
          FORWARD,
          POSTROUTING,
          LOCALOUT
        };

        QueuedDatagramForHook(IPv4Datagram* datagram, InterfaceEntry* inIE, InterfaceEntry* outIE, const IPv4Address& nextHopAddr, Hook hook) :
                datagram(datagram), inIE(inIE), outIE(outIE), nextHopAddr(nextHopAddr), hook(hook) {}
        virtual ~QueuedDatagramForHook() {}

        IPv4Datagram* datagram;
        InterfaceEntry* inIE;
        InterfaceEntry* outIE;
        IPv4Address nextHopAddr;
        const Hook hook;
    };

  protected:
    IRoutingTable *rt;
    IInterfaceTable *ift;
    ICMPAccess icmpAccess;
    cGate *queueOutGate; // the most frequently used output gate

    // config
    int defaultTimeToLive;
    int defaultMCTimeToLive;
    simtime_t fragmentTimeoutTime;
    bool forceBroadcast;

    // working vars
    long curFragmentId; // counter, used to assign unique fragmentIds to datagrams
    IPv4FragBuf fragbuf;  // fragmentation reassembly buffer
    simtime_t lastCheckTime; // when fragbuf was last checked for state fragments
    ProtocolMapping mapping; // where to send packets after decapsulation

    // statistics
    int numMulticast;
    int numLocalDeliver;
    int numDropped;  // forwarding off, no outgoing interface, too large but "don't fragment" is set, TTL exceeded, etc
    int numUnroutable;
    int numForwarded;

    // hooks
    typedef std::multimap<int, Hook*> HookList;
    HookList hooks;
    typedef std::list<QueuedDatagramForHook> DatagramQueueForHooks;
    DatagramQueueForHooks queuedDatagramsForHooks;

  protected:
    // utility: look up interface from getArrivalGate()
    virtual InterfaceEntry *getSourceInterfaceFrom(cPacket *msg);

    // utility: look up route to the source of the datagram and return its interface
    virtual InterfaceEntry *getShortestPathInterfaceToSource(IPv4Datagram *datagram);

    // utility: show current statistics above the icon
    virtual void updateDisplayString();

    /**
     * Encapsulate packet coming from higher layers into IPv4Datagram, using
     * the given control info. Override if you subclassed controlInfo and/or
     * want to add options etc to the datagram.
     */
    virtual IPv4Datagram *encapsulate(cPacket *transportPacket, IPv4ControlInfo *controlInfo);

    /**
     * Creates a blank IPv4 datagram. Override when subclassing IPv4Datagram is needed
     */
    virtual IPv4Datagram *createIPv4Datagram(const char *name);

    /**
     * Handle IPv4Datagram messages arriving from lower layer.
     * Decrements TTL, then invokes routePacket().
     */
    virtual void handlePacketFromNetwork(IPv4Datagram *datagram, InterfaceEntry *fromIE);

    // called after PREROUTING Hook (used for reinject, too)
    virtual void preroutingFinish(IPv4Datagram *datagram, InterfaceEntry *fromIE);

    /**
     * Handle messages (typically packets to be send in IPv4) from transport or ICMP.
     * Invokes encapsulate(), then routePacket().
     */
    virtual void handleMessageFromHL(cPacket *msg);

    /**
     * Routes and sends datagram received from higher layers.
     * Invokes datagramLocalOutHook(), then routePacket().
     */
    virtual void datagramLocalOut(IPv4Datagram* datagram, InterfaceEntry* destIE);

    /**
     * Handle incoming ARP packets by sending them over "queueOut" to ARP.
     */
    virtual void handleARP(ARPPacket *msg);

    /**
     * Handle incoming ICMP messages.
     */
    virtual void handleReceivedICMP(ICMPMessage *msg);

    /**
     * Performs unicast routing. Based on the routing decision, it sends the
     * datagram through the outgoing interface.
     */
    virtual void routeUnicastPacket(IPv4Datagram *datagram, InterfaceEntry *fromIE, InterfaceEntry *destIE, IPv4Address nextHopAddr);

    /**
     * Broadcasts the datagram on the specified interface.
     * When destIE is NULL, the datagram is broadcasted on each interface.
     */
    virtual void routeLocalBroadcastPacket(IPv4Datagram *datagram, InterfaceEntry *destIE);

    /**
     * Determines the output interface for the given multicast datagram.
     */
    virtual InterfaceEntry *determineOutgoingInterfaceForMulticastDatagram(IPv4Datagram *datagram, InterfaceEntry *multicastIFOption);

    /**
     * Forwards packets to all multicast destinations, using fragmentAndSend().
     */
    virtual void forwardMulticastPacket(IPv4Datagram *datagram, InterfaceEntry *fromIE);

    /**
     * Perform reassembly of fragmented datagrams, then send them up to the
     * higher layers using sendToHL().
     */
    virtual void reassembleAndDeliver(IPv4Datagram *datagram);

    // called after LOCAL_IN Hook (used for reinject, too)
    virtual void reassembleAndDeliverFinish(IPv4Datagram *datagram);

    /**
     * Decapsulate and return encapsulated packet after attaching IPv4ControlInfo.
     */
    virtual cPacket *decapsulate(IPv4Datagram *datagram);

    /**
     * Call PostRouting Hook and continue with fragmentAndSend() if accepted
     */
    virtual void fragmentPostRouting(IPv4Datagram *datagram, InterfaceEntry *ie, IPv4Address nextHopAddr);

    /**
     * Fragment packet if needed, then send it to the selected interface using
     * sendDatagramToOutput().
     */
    virtual void fragmentAndSend(IPv4Datagram *datagram, InterfaceEntry *ie, IPv4Address nextHopAddr);

    /**
     * Last TTL check, then send datagram on the given interface.
     */
    virtual void sendDatagramToOutput(IPv4Datagram *datagram, InterfaceEntry *ie, IPv4Address nextHopAddr);

  public:
    IPv4() { rt = NULL; ift = NULL; queueOutGate = NULL; }

  protected:
    /**
     * Initialization
     */
    virtual void initialize();

    /**
     * Processing of IPv4 datagrams. Called when a datagram reaches the front
     * of the queue.
     */
    virtual void endService(cPacket *msg);

    // NetFilter functions:
  protected:
    /**
     * called before a packet arriving from the network is routed
     */
    Hook::Result datagramPreRoutingHook(IPv4Datagram* datagram, InterfaceEntry* inIE);

    /**
     * called before a packet arriving from the network is delivered locally
     */
    Hook::Result datagramLocalInHook(IPv4Datagram* datagram, InterfaceEntry* inIE);

    /**
     * called before a packet arriving from the network is delivered via the network
     */
    Hook::Result datagramForwardHook(IPv4Datagram* datagram, InterfaceEntry* inIE, InterfaceEntry*& outIE, IPv4Address& nextHopAddr);

    /**
     * called before a packet is delivered via the network
     */
    Hook::Result datagramPostRoutingHook(IPv4Datagram* datagram, InterfaceEntry* inIE, InterfaceEntry*& outIE, IPv4Address& nextHopAddr);

    /**
     * called before a packet arriving locally is delivered
     */
    Hook::Result datagramLocalOutHook(IPv4Datagram* datagram, InterfaceEntry*& outIE);

  public:
    /**
     * registers a Hook to be executed during datagram processing
     */
    void registerHook(int priority, IPv4::Hook* hook);

    /**
     * unregisters a Hook to be executed during datagram processing
     */
    void unregisterHook(int priority, IPv4::Hook* hook);

    /**
     * get queued infos for a previously queued datagram
     *
     * you must use it, if you want to change output interface, next hop address, etc.
     * before use reinjectDatagram()
     */
    QueuedDatagramForHook& getQueuedDatagramForHook(const IPv4Datagram* datagram);

    /**
     * drop a previously queued datagram
     */
    void dropQueuedDatagram(const IPv4Datagram* datagram);

    /**
     * re-injects a previously queued datagram
     */
    void reinjectDatagram(const IPv4Datagram* datagram);

    IRoutingTable* getRoutingTable() const { return rt; };
    IInterfaceTable* getInterfaceTable() const { return ift; };
};

#endif


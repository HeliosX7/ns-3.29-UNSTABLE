/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 IITP RAS
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Based on 
 *      NS-2 AOMDV model developed by the CMU/MONARCH group and optimized and
 *      tuned by Samir Das and Mahesh Marina, University of Cincinnati;
 * 
 *      AOMDV-UU implementation by Erik Nordström of Uppsala University
 *      http://core.it.uu.se/core/index.php/AOMDV-UU
 *
 * Authors: Elena Buchatskaia <borovkovaes@iitp.ru>
 *          Pavel Boyko <boyko@iitp.ru>
 */
#ifndef AOMDVROUTINGPROTOCOL_H
#define AOMDVROUTINGPROTOCOL_H

#include "aomdv-rtable.h"
#include "aomdv-rqueue.h"
#include "aomdv-packet.h"
#include "aomdv-neighbor.h"
#include "aomdv-dpd.h"
#include "ns3/node.h"
#include "ns3/random-variable-stream.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-interface.h"
#include "ns3/ipv4-l3-protocol.h"
#include <map>

// AOMDV code
#define AOMDV_PACKET_SALVAGING
#define AOMDV_MAX_SALVAGE_COUNT  10
//#define AOMDV_LINK_DISJOINT_PATHS
#define AOMDV_NODE_DISJOINT_PATHS
#define NODE_TRAVERSAL_TIME     0.03             // 30 ms
#define LOCAL_REPAIR_WAIT_TIME  0.15 //sec
//#define INFINITY 0xff
#define AOMDV_MAX_PATHS 5
#define AOMDV_PRIM_ALT_PATH_LENGTH_DIFF 3

namespace ns3
{
namespace aomdv
{
/**
 * \ingroup aomdv
 * 
 * \brief AOMDV routing protocol
 */
class RoutingProtocol : public Ipv4RoutingProtocol
{
public:
  static TypeId GetTypeId (void);
  static const uint32_t AOMDV_PORT;

  /// c-tor
  RoutingProtocol ();
  virtual ~RoutingProtocol();
  virtual void DoDispose ();

  // Inherited from Ipv4RoutingProtocol
  Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);
  bool RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                   UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                   LocalDeliverCallback lcb, ErrorCallback ecb);
  virtual void NotifyInterfaceUp (uint32_t interface);
  virtual void NotifyInterfaceDown (uint32_t interface);
  virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void SetIpv4 (Ptr<Ipv4> ipv4);
  virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const;

  // Handle protocol parameters
  Time GetMaxQueueTime () const { return m_maxQueueTime; }
  void SetMaxQueueTime (Time t);
  uint32_t GetMaxQueueLen () const { return m_maxQueueLen; }
  void SetMaxQueueLen (uint32_t len);
  bool GetDesinationOnlyFlag () const { return m_destinationOnly; }
  void SetDesinationOnlyFlag (bool f) { m_destinationOnly = f; }
  bool GetGratuitousReplyFlag () const { return m_gratuitousReply; }
  void SetGratuitousReplyFlag (bool f) { m_gratuitousReply = f; }
  void SetHelloEnable (bool f) { m_enableHello = f; }
  bool GetHelloEnable () const { return m_enableHello; }
  void SetBroadcastEnable (bool f) { m_enableBroadcast = f; }
  bool GetBroadcastEnable () const { return m_enableBroadcast; }

 /**
  * Assign a fixed random variable stream number to the random variables
  * used by this model.  Return the number of streams (possibly zero) that
  * have been assigned.
  *
  * \param stream first stream index to use
  * \return the number of stream indices assigned by this model
  */
  int64_t AssignStreams (int64_t stream);

protected:
  virtual void DoInitialize (void);
private:
  
  // Protocol parameters.
  uint32_t m_rreqRetries;             ///< Maximum number of retransmissions of RREQ with TTL = NetDiameter to discover a route
  uint16_t m_ttlStart;                ///< Initial TTL value for RREQ.
  uint16_t m_ttlIncrement;            ///< TTL increment for each attempt using the expanding ring search for RREQ dissemination.
  uint16_t m_ttlThreshold;            ///< Maximum TTL value for expanding ring search, TTL = NetDiameter is used beyond this value.
  uint16_t m_timeoutBuffer;           ///< Provide a buffer for the timeout.
  uint16_t m_rreqRateLimit;           ///< Maximum number of RREQ per second.
  uint16_t m_rerrRateLimit;           ///< Maximum number of REER per second.
  Time m_activeRouteTimeout;          ///< Period of time during which the route is considered to be valid.
  uint32_t m_netDiameter;             ///< Net diameter measures the maximum possible number of hops between two nodes in the network
  /**
   *  NodeTraversalTime is a conservative estimate of the average one hop traversal time for packets
   *  and should include queuing delays, interrupt processing times and transfer times.
   */
  Time m_nodeTraversalTime;
  Time m_netTraversalTime;             ///< Estimate of the average net traversal time.
  Time m_pathDiscoveryTime;            ///< Estimate of maximum time needed to find route in network.
  Time m_myRouteTimeout;               ///< Value of lifetime field in RREP generating by this node.
  /**
   * Every HelloInterval the node checks whether it has sent a broadcast  within the last HelloInterval.
   * If it has not, it MAY broadcast a  Hello message
   */
  Time m_helloInterval;
  uint32_t m_allowedHelloLoss;         ///< Number of hello messages which may be loss for valid link
  /**
   * DeletePeriod is intended to provide an upper bound on the time for which an upstream node A
   * can have a neighbor B as an active next hop for destination D, while B has invalidated the route to D.
   */
  Time m_deletePeriod;
  Time m_nextHopWait;                  ///< Period of our waiting for the neighbour's RREP_ACK
  Time m_blackListTimeout;             ///< Time for which the node is put into the blacklist
  uint32_t m_maxQueueLen;              ///< The maximum number of packets that we allow a routing protocol to buffer.
  Time m_maxQueueTime;                 ///< The maximum period of time that a routing protocol is allowed to buffer a packet for.
  bool m_destinationOnly;              ///< Indicates only the destination may respond to this RREQ.
  bool m_gratuitousReply;              ///< Indicates whether a gratuitous RREP should be unicast to the node originated route discovery.
  bool m_enableHello;                  ///< Indicates whether a hello messages enable
  bool m_enableBroadcast;              ///< Indicates whether a a broadcast data packets forwarding enable
  //\}

  /// IP protocol
  Ptr<Ipv4> m_ipv4;
  /// Raw unicast socket per each IP interface, map socket -> iface address (IP + mask)
  std::map< Ptr<Socket>, Ipv4InterfaceAddress > m_socketAddresses;
  /// Raw subnet directed broadcast socket per each IP interface, map socket -> iface address (IP + mask)
  std::map< Ptr<Socket>, Ipv4InterfaceAddress > m_socketSubnetBroadcastAddresses;
  /// Loopback device used to defer RREQ until packet will be fully formed
  Ptr<NetDevice> m_lo; 

  /// Routing table
  RoutingTable m_routingTable;
  /// A "drop-front" queue used by the routing layer to buffer packets to which it does not have a route.
  RequestQueue m_queue;
  /// Broadcast ID
  uint32_t m_requestId;
  /// Request sequence number
  uint32_t m_seqNo;
  /// Handle duplicated RREQ
  IdCache m_rreqIdCache;
  /// Handle duplicated broadcast/multicast packets
  DuplicatePacketDetection m_dpd;
  /// Handle neighbors
  Neighbors m_nb;
  /// Number of RREQs used for RREQ rate control
  uint16_t m_rreqCount;
  /// Number of RERRs used for RERR rate control
  uint16_t m_rerrCount;

private:
  /// Start protocol operation
  void Start ();
  /// Queue packet and send route request
  void DeferredRouteOutput (Ptr<const Packet> p, const Ipv4Header & header, UnicastForwardCallback ucb, ErrorCallback ecb);
  /// If route exists and valid, forward packet.
  bool Forwarding (Ptr<const Packet> p, const Ipv4Header & header, UnicastForwardCallback ucb, ErrorCallback ecb);
  /**
  * To reduce congestion in a network, repeated attempts by a source node at route discovery
  * for a single destination MUST utilize a binary exponential backoff.
  */
  void ScheduleRreqRetry (Ipv4Address dst);
  /**
   * Set lifetime field in routing table entry to the maximum of existing lifetime and lt, if the entry exists
   * \param addr - destination address
   * \param lt - proposed time for lifetime field in routing table entry for destination with address addr.
   * \return true if route to destination address addr exist
   */
  bool UpdateRouteLifeTime (Ipv4Address addr, Time lt);
  /**
   * Set lifetime field in routing table entry to the maximum of existing lifetime and lt, if the entry exists
   * and update the expire time of all paths of that entry
   * \param addr - destination address
   * \param lt - proposed time for lifetime field in routing table entry for destination with address addr.
   * \return true if route to destination address addr exist
   */
  bool UpdatePathsLifeTime (Ipv4Address addr, Time lt);
  /**
   * Update neighbor record.
   * \param receiver is supposed to be my interface
   * \param sender is supposed to be IP address of my neighbor.
   */
  void UpdateRouteToNeighbor (Ipv4Address sender, Ipv4Address receiver);
  /// Check that packet is send from own interface
  bool IsMyOwnAddress (Ipv4Address src);
  /// Find unicast socket with local interface address iface
  Ptr<Socket> FindSocketWithInterfaceAddress (Ipv4InterfaceAddress iface) const;
  /// Find subnet directed broadcast socket with local interface address iface
  Ptr<Socket> FindSubnetBroadcastSocketWithInterfaceAddress (Ipv4InterfaceAddress iface) const;
  /// Process hello message
  void ProcessHello (RrepHeader const & rrepHeader, Ipv4Address receiverIfaceAddr);
  /// Create loopback route for given header
  Ptr<Ipv4Route> LoopbackRoute (const Ipv4Header & header, Ptr<NetDevice> oif) const;

  ///\name Receive control packets
  //\{
  /// Receive and process control packet
  void RecvAomdv (Ptr<Socket> socket);
  /// Receive RREQ
  void RecvRequest (Ptr<Packet> p, Ipv4Address receiver, Ipv4Address src);
  /// Receive RREP
  void RecvReply (Ptr<Packet> p, Ipv4Address my,Ipv4Address src);
  /// Receive RREP_ACK
  void RecvReplyAck (Ipv4Address neighbor);
  /// Receive RERR from node with address src
  void RecvError (Ptr<Packet> p, Ipv4Address src);
  //\}

  ///\name Send
  //\{
  /// Forward packet from route request queue
  void SendPacketFromQueue (Ipv4Address dst, Ptr<Ipv4Route> route);
  /// Send hello
  void SendHello ();
  /// Send RREQ
  void SendRequest (Ipv4Address dst);
  /// Send RREP
  void SendReply (RreqHeader const & rreqHeader, RoutingTableEntry & toOrigin, Ipv4Address firstHop);
  /** Send RREP by intermediate node
   * \param toDst routing table entry to destination
   * \param toOrigin routing table entry to originator
   * \param gratRep indicates whether a gratuitous RREP should be unicast to destination
   */
  void SendReplyByIntermediateNode (RoutingTableEntry & toDst, RoutingTableEntry & toOrigin, bool gratRep, Time expire,
                                    Ipv4Address firstHop, uint32_t requestId);
  /// Send RREP_ACK
  void SendReplyAck (Ipv4Address neighbor);
  /// Initiate RERR
  void SendRerrWhenBreaksLinkToNextHop (Ipv4Address nextHop);
  /// Forward RERR
  void SendRerrMessage (Ptr<Packet> packet,  std::vector<Ipv4Address> precursors);
  /**
   * Send RERR message when no route to forward input packet. Unicast if there is reverse route to originating node, broadcast otherwise.
   * \param dst - destination node IP address
   * \param dstSeqNo - destination node sequence number
   * \param origin - originating node IP address
   */
  void SendRerrWhenNoRouteToForward (Ipv4Address dst, uint32_t dstSeqNo, Ipv4Address origin);
  /// @}

  void SendTo (Ptr<Socket> socket, Ptr<Packet> packet, Ipv4Address destination);

  /// Hello timer
  Timer m_htimer;
  /// Schedule next send of hello message
  void HelloTimerExpire ();
  /// RREQ rate limit timer
  Timer m_rreqRateLimitTimer;
  /// Reset RREQ count and schedule RREQ rate limit timer with delay 1 sec.
  void RreqRateLimitTimerExpire ();
  /// RERR rate limit timer
  Timer m_rerrRateLimitTimer;
  /// Reset RERR count and schedule RERR rate limit timer with delay 1 sec.
  void RerrRateLimitTimerExpire ();
  /// Map IP address + RREQ timer.
  std::map<Ipv4Address, Timer> m_addressReqTimer;
  /// Handle route discovery process
  void RouteRequestTimerExpire (Ipv4Address dst);
  /// Mark link to neighbor node as unidirectional for blacklistTimeout
  void AckTimerExpire (Ipv4Address neighbor,  Time blacklistTimeout);

  /// Provides uniform random variables.
  Ptr<UniformRandomVariable> m_uniformRandomVariable;  
  /// Keep track of the last bcast time
  Time m_lastBcastTime;
  /// AOMDV Code
  int32_t m_MaxPaths;
  int32_t m_PrimAltPathLengthDiff;

};

}
}
#endif /* AOMDVROUTINGPROTOCOL_H */

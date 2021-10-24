/*
 * An LDoS attack consists of sending periodic short-lived and high-pulsed 
 * UDP packets through the network (bursts).
 * LDoS make use of the TCP congestion control's Retransmission TimeOut (RTO) 
 * functionality to degrade the quality of TCP links, targetting the bottleneck link.
 * With this type of attack we 'time' these bursts such that they congest the network 
 * right when the TCP sender retransmits when there is a packet loss.
 * 
 * One way to launch a very successful attack is to time the attacker bursts 
 * at the exact time the sender is about to send some TCP packets.
 */

#include <iostream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LDoSAttack");

#define ATTACK 1 // Run simulation with/out attacker

int
main (int argc, char *argv[])
{
  // Set up some default values for the simulation.
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
  // RTO = 1s default value (Retransmission Timeout)
  Config::SetDefault ("ns3::RedQueueDisc::ARED",
                      BooleanValue (true)); // RED queue management algorithm

  CommandLine cmd (__FILE__);

  bool verbose = false, tracing = true;
  double global_start_time = 0.0;
  double global_stop_time = 605.0;
  double sink_start_time = global_start_time;
  double sink_stop_time = global_stop_time + 20.0;
  double client_start_time = global_start_time + 1.0;
  double client_stop_time = global_stop_time;
  uint32_t TCPn = 10;
  uint32_t bTCPn = 5;
  uint32_t BUn = 5;
  std::string std_linkDataRate = "100Mbps";
  std::string std_linkDelay = "30ms";
  std::string bottleneck_linkDataRate = "10Mbps";
  std::string bottleneck_linkDelay = "15ms";

  cmd.AddValue ("verbose", "Explicit debugging", verbose);
  cmd.AddValue ("tracing", "Save tracing information", tracing);
  cmd.AddValue ("global_start_time", "when the simulation starts (s)", global_start_time);
  cmd.AddValue ("global_stop_time", "when the simulation ends (s)", global_stop_time);
  cmd.AddValue ("TCPn", "Number of nodes sending/receiving normal TCP traffic", TCPn);
  cmd.AddValue ("bTCPn", "Number of nodes sending/receiving background TCP traffic", bTCPn);
  cmd.AddValue ("BUn", "Number of nodes sending background UDP traffic", BUn);

#if (ATTACK)
  // * LDoS Attack parameters
  double attack_start_time = global_start_time + 5.0; // s
  double attack_stop_time = global_stop_time; // s
  double attack_T = 1.0; // cycle (s)
  double attack_t = 0.1; // duration (s)
  double R = 20; // intensity (Mbps)

  cmd.AddValue ("attack_T", "LDoS attack period (s)", attack_T);
  cmd.AddValue ("attack_t", "LDoS attack duration (s)", attack_t);
  cmd.AddValue ("attack_R", "LDoS attack strength (Mbps)", R);

  // cost of attack (bottleneck bandwidth is 10Mbps)
  double attack_cost = (attack_t / attack_T) * (R / 10);

  // Time attacker is not sending UDP packets.
  // Also used to start sending TCP packets
  // (to sync with the attacker for a more successfull attack.
  //  This is because the UDP OnOff application is initially in the Off state)
  double attack_offTime = attack_T - attack_t;

  std::string attack_R = std::to_string (R) + "Mbps";
#endif

  cmd.Parse (argc, argv);

  // logging
  LogComponentEnable ("LDoSAttack", LOG_LEVEL_INFO);

  NS_LOG_INFO ("Create nodes.");
  NodeContainer T_nodes;
  T_nodes.Create (TCPn);
  NodeContainer S_nodes;
  S_nodes.Create (TCPn);
  NodeContainer BT_nodes;
  BT_nodes.Create (bTCPn);
  NodeContainer BS_nodes;
  BS_nodes.Create (bTCPn);
  NodeContainer BU_nodes;
  BU_nodes.Create (BUn);
  NodeContainer router_nodes;
  router_nodes.Create (3); // three routers
#if (ATTACK)
  NodeContainer A_nodes;
  A_nodes.Create (1); // 1 attacker
#endif

  NS_LOG_INFO ("Install internet stack on all nodes.");
  InternetStackHelper internet;
  internet.InstallAll ();

  NS_LOG_INFO ("Crete point-to-point channels.");
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (std_linkDataRate));
  p2p.SetChannelAttribute ("Delay", StringValue (std_linkDelay));

  std::vector<NetDeviceContainer> T_dev (TCPn), S_dev (TCPn), BT_dev (bTCPn), BS_dev (bTCPn),
      BU_dev (BUn);
  for (uint32_t i = 0; i < TCPn; i++)
    {
      T_dev[i] = p2p.Install (T_nodes.Get (i), router_nodes.Get (0));
      S_dev[i] = p2p.Install (S_nodes.Get (i), router_nodes.Get (2));
    }
  for (uint32_t i = 0; i < bTCPn; i++)
    {
      BT_dev[i] = p2p.Install (BT_nodes.Get (i), router_nodes.Get (0));
      BS_dev[i] = p2p.Install (BS_nodes.Get (i), router_nodes.Get (1));
    }
  for (uint32_t i = 0; i < BUn; i++)
    {
      BU_dev[i] = p2p.Install (BU_nodes.Get (i), router_nodes.Get (0));
    }

#if (ATTACK)
  // connect Attacker node to r1
  NetDeviceContainer A_dev = p2p.Install (A_nodes.Get (0), router_nodes.Get (0));
#endif

  // routers with RED queue management algorithm
  TrafficControlHelper tch_r1r2, tch_r2r3;
  tch_r1r2.SetRootQueueDisc ("ns3::RedQueueDisc", "LinkBandwidth", StringValue (std_linkDataRate),
                             "LinkDelay", StringValue (std_linkDelay));
  tch_r2r3.SetRootQueueDisc ("ns3::RedQueueDisc", "LinkBandwidth",
                             StringValue (bottleneck_linkDataRate), "LinkDelay",
                             StringValue (bottleneck_linkDelay));
  PointToPointHelper p2pRed;

  // connect r1 and r2
  p2pRed.SetDeviceAttribute ("DataRate", StringValue (std_linkDataRate));
  p2pRed.SetChannelAttribute ("Delay", StringValue (std_linkDelay));
  NetDeviceContainer r1r2 = p2pRed.Install (router_nodes.Get (0), router_nodes.Get (1));
  tch_r1r2.Install (r1r2);

  // * bottleneck
  // connect r2 and r3
  p2pRed.SetDeviceAttribute ("DataRate", StringValue (bottleneck_linkDataRate));
  p2pRed.SetChannelAttribute ("Delay", StringValue (bottleneck_linkDelay));
  NetDeviceContainer r2r3 = p2pRed.Install (router_nodes.Get (1), router_nodes.Get (2));
  tch_r2r3.Install (r2r3);

  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;

  std::vector<Ipv4InterfaceContainer> T_int (TCPn), S_int (TCPn), BT_int (bTCPn), BS_int (bTCPn),
      BU_int (BUn);

  // normal TCP flows, nodes T and S
  for (uint32_t i = 0; i < TCPn; i++)
    {
      std::ostringstream T_subnet, S_subnet;
      T_subnet << "80.1." << i + 1 << ".0";
      ipv4.SetBase (T_subnet.str ().c_str (), "255.255.255.252");
      T_int[i] = ipv4.Assign (T_dev[i]);

      S_subnet << "80.2." << i + 1 << ".0";
      ipv4.SetBase (S_subnet.str ().c_str (), "255.255.255.252");
      S_int[i] = ipv4.Assign (S_dev[i]);
    }

  // background TCP flows, nodes BT and BS
  for (uint32_t i = 0; i < bTCPn; i++)
    {
      std::ostringstream BT_subnet, BS_subnet;
      BT_subnet << "90.1." << i + 1 << ".0";
      ipv4.SetBase (BT_subnet.str ().c_str (), "255.255.255.252");
      BT_int[i] = ipv4.Assign (BT_dev[i]);

      BS_subnet << "90.2." << i + 1 << ".0";
      ipv4.SetBase (BS_subnet.str ().c_str (), "255.255.255.252");
      BS_int[i] = ipv4.Assign (BS_dev[i]);
    }

  // background UDP flows, nodes BU
  for (uint32_t i = 0; i < BUn; i++)
    {
      std::ostringstream subnet;
      subnet << "100.1." << i + 1 << ".0";
      ipv4.SetBase (subnet.str ().c_str (), "255.255.255.252");
      BU_int[i] = ipv4.Assign (BU_dev[i]);
    }

  // routers
  ipv4.SetBase ("192.168.0.0", "255.255.255.252");
  Ipv4InterfaceContainer r1r2_int = ipv4.Assign (r1r2);

  ipv4.SetBase ("192.168.1.0", "255.255.255.252");
  Ipv4InterfaceContainer r2r3_int = ipv4.Assign (r2r3);

#if (ATTACK)
  ipv4.SetBase ("60.1.0.0", "255.255.255.0");
  Ipv4InterfaceContainer A_int = ipv4.Assign (A_dev);
#endif

  NS_LOG_INFO ("Enable static global routing.");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  NS_LOG_INFO ("Create Applications.");

  uint16_t tcp_port = 50000;
  uint16_t udp_port = 9;

  /* Setup background and normal TCP flows */

  // Create a TCP sink to receive normal and background TCP packets on BS_nodes and S_nodes
  ApplicationContainer tcp_sinkApps;
  Address tcp_sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), tcp_port));
  PacketSinkHelper tcp_sink ("ns3::TcpSocketFactory", tcp_sinkLocalAddress);

  for (uint32_t i = 0; i < bTCPn; i++)
    {
      tcp_sinkApps.Add (tcp_sink.Install (BS_nodes.Get (i)));
    }
  for (uint32_t i = 0; i < TCPn; i++)
    {
      tcp_sinkApps.Add (tcp_sink.Install (S_nodes.Get (i)));
    }

  tcp_sinkApps.Start (Seconds (sink_start_time));
  tcp_sinkApps.Stop (Seconds (sink_stop_time));

  // Create a Bulk Send Application to send TCP packets
  BulkSendHelper tcp_client ("ns3::TcpSocketFactory", Address ());

  // setup background TCP flow (BT_nodes -> BS_nodes)
  ApplicationContainer bTCP_clientApps;
  for (uint32_t i = 0; i < bTCPn; i++)
    {
      AddressValue remoteAddress (InetSocketAddress (BS_int[i].GetAddress (0), tcp_port));
      tcp_client.SetAttribute ("Remote", remoteAddress);
      bTCP_clientApps.Add (tcp_client.Install (BT_nodes.Get (i)));
    }

  bTCP_clientApps.Start (Seconds (client_start_time));
  bTCP_clientApps.Stop (Seconds (client_stop_time));

  // setup normal TCP flow (T_nodes -> S_nodes)
  ApplicationContainer TCP_clientApps;
  for (uint32_t i = 0; i < TCPn; i++)
    {
      AddressValue remoteAddress (InetSocketAddress (S_int[i].GetAddress (0), tcp_port));
      tcp_client.SetAttribute ("Remote", remoteAddress);
      TCP_clientApps.Add (tcp_client.Install (T_nodes.Get (i)));
    }

  TCP_clientApps.Start (Seconds (client_start_time));
  TCP_clientApps.Stop (Seconds (client_stop_time));

  /* create UDP sink on receiver R3 */

  PacketSinkHelper udp_sink ("ns3::UdpSocketFactory",
                             Address (InetSocketAddress (Ipv4Address::GetAny (), udp_port)));

  ApplicationContainer udp_sinkApp = udp_sink.Install (router_nodes.Get (2));
  udp_sinkApp.Start (Seconds (sink_start_time));
  udp_sinkApp.Stop (Seconds (sink_stop_time));

  /* Send background UDP traffic from BU_nodes to R3 */

  OnOffHelper udp_client ("ns3::UdpSocketFactory",
                          Address (InetSocketAddress (r2r3_int.GetAddress (1), udp_port)));
  udp_client.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  udp_client.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer udp_clientApps = udp_client.Install (BU_nodes);
  udp_clientApps.Start (Seconds (client_start_time));
  udp_clientApps.Stop (Seconds (client_stop_time));

  /* LDoS attack */
#if (ATTACK)
  NS_LOG_INFO ("Cost of attack A = " << attack_cost);

  // UDP On-Off Application used by the attacker to create low-rate periodic bursts
  // targeting the bottleneck R2-R3, send UDP bursts to R3
  OnOffHelper LDoSudp_client ("ns3::UdpSocketFactory",
                              Address (InetSocketAddress (r2r3_int.GetAddress (1), udp_port)));
  LDoSudp_client.SetConstantRate (DataRate (attack_R));

  LDoSudp_client.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" +
                                                      std::to_string (attack_t) + "]"));
  LDoSudp_client.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" +
                                                       std::to_string (attack_offTime) + "]"));

  ApplicationContainer LDoSudp_clientApp = LDoSudp_client.Install (A_nodes.Get (0));
  LDoSudp_clientApp.Start (Seconds (attack_start_time));
  LDoSudp_clientApp.Stop (Seconds (attack_stop_time));
#else
  NS_LOG_INFO ("LDoS attack disabled.");
#endif

  if (tracing)
    {
      AsciiTraceHelper ascii;
      // pcap files: "prefix-nodeID-deviceID.pcap"
#if (ATTACK)
      // p2p.EnableAsciiAll (ascii.CreateFileStream ("LDoS_A" + std::to_string (attack_cost) + ".tr"));
      p2p.EnablePcap ("LDoS_A" + std::to_string (attack_cost), r1r2);
      p2p.EnablePcap ("LDoS_A" + std::to_string (attack_cost), r2r3);
      p2p.EnablePcap ("LDoS_A" + std::to_string (attack_cost), A_dev);
#else
      // p2p.EnableAsciiAll (ascii.CreateFileStream ("LDoS.tr"));
      p2p.EnablePcap ("LDoS", r1r2);
      p2p.EnablePcap ("LDoS", r2r3);
#endif
    }

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}
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
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("simpleLDoS");

#define ATTACK 1 // Run simulation with/out attacker

int
main (int argc, char *argv[])
{
  // Set up some default values for the simulation.
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
  // RTO = 1s default value (Retransmission Timeout)

  CommandLine cmd (__FILE__);

  bool tracing = true;
  double maxSimulationTime = 10.0;
  uint32_t TCPn = 10;

  cmd.AddValue ("tracing", "Save tracing information", tracing);
  cmd.AddValue ("maxSimulationTime", "starting at 0.0s, when the simulation ends (s)",
                maxSimulationTime);
  cmd.AddValue ("TCPn", "Number of nodes sending/receiving normal TCP traffic", TCPn);

  double attack_offTime = 0.0;

#if (ATTACK)
  // * LDoS Attack parameters
  double attack_start = 3.0; // s
  double attack_stop = maxSimulationTime; // s
  double attack_T = 1.0; // cycle (s)
  double attack_t = 0.1; // duration (s)
  double R = 20; // intensity (Mbps)

  cmd.AddValue ("attack_start", "when the LDoS attack starts (s)", attack_start);
  cmd.AddValue ("attack_stop", "when the LDoS attack stops (s)", attack_stop);
  cmd.AddValue ("attack_T", "LDoS attack period (s)", attack_T);
  cmd.AddValue ("attack_t", "LDoS attack duration (s)", attack_t);
  cmd.AddValue ("attack_R", "LDoS attack strength (Mbps)", R);

  // cost of attack (bottleneck bandwidth is 10Mbps)
  double attack_cost = (attack_t / attack_T) * (R / 10);

  // Time attacker is not sending UDP packets.
  // Also used to start sending TCP packets
  // (to sync with the attacker for a more successfull attack.
  //  This is because the UDP OnOff application is initially in the Off state)
  attack_offTime = attack_T - attack_t;

  std::string attack_R = std::to_string (R) + "Mbps";
#endif

  cmd.Parse (argc, argv);

  // logging
  LogComponentEnable ("simpleLDoS", LOG_LEVEL_INFO);

  NS_LOG_INFO ("Create nodes.");
  NodeContainer T_nodes;
  T_nodes.Create (TCPn);
  NodeContainer S_nodes;
  S_nodes.Create (TCPn);
  NodeContainer router_nodes;
  router_nodes.Create (3); // three routers
#if (ATTACK)
  NodeContainer A_nodes;
  A_nodes.Create (1); // 1 attacker
#endif

  NS_LOG_INFO ("Crete channels.");
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("15ms"));

  std::vector<NetDeviceContainer> T_devices (TCPn), S_devices (TCPn);
  for (uint32_t i = 0; i < TCPn; i++)
    {
      T_devices[i] = p2p.Install (T_nodes.Get (i), router_nodes.Get (0));
      S_devices[i] = p2p.Install (S_nodes.Get (i), router_nodes.Get (2));
    }

#if (ATTACK)
  // connect Attacker node to r1
  NetDeviceContainer A_devices = p2p.Install (A_nodes.Get (0), router_nodes.Get (0));
#endif
  // connect r1 and r2
  NetDeviceContainer r1r2 = p2p.Install (router_nodes.Get (0), router_nodes.Get (1));

  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("30ms"));
  // connect r2 and r3 (bottleneck)
  NetDeviceContainer r2r3 = p2p.Install (router_nodes.Get (1), router_nodes.Get (2));

  NS_LOG_INFO ("Install internet stack on all nodes.");
  InternetStackHelper internet;
  internet.InstallAll ();

  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  // for T nodes
  std::vector<Ipv4InterfaceContainer> T_interfaces (TCPn), S_interfaces (TCPn);
  for (uint32_t i = 0; i < T_interfaces.size (); i++)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i + 1 << ".0";
      ipv4.SetBase (subnet.str ().c_str (), "255.255.255.0");
      T_interfaces[i] = ipv4.Assign (T_devices[i]);
    }
  // for S nodes
  for (uint32_t i = 0; i < S_interfaces.size (); i++)
    {
      std::ostringstream subnet;
      subnet << "20.1." << i + 1 << ".0";
      ipv4.SetBase (subnet.str ().c_str (), "255.255.255.0");
      S_interfaces[i] = ipv4.Assign (S_devices[i]);
    }

  // for p2p (routers)
  ipv4.SetBase ("192.168.0.0", "255.255.255.252");
  Ipv4InterfaceContainer r1r2_interfaces = ipv4.Assign (r1r2);

  ipv4.SetBase ("192.168.1.0", "255.255.255.252");
  Ipv4InterfaceContainer r2r3_interfaces = ipv4.Assign (r2r3);

#if (ATTACK)
  ipv4.SetBase ("60.1.100.0", "255.255.255.0");
  Ipv4InterfaceContainer A_interfaces = ipv4.Assign (A_devices);
#endif

  NS_LOG_INFO ("Enable static global routing.");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  NS_LOG_INFO ("Create Applications.");

  uint16_t tcp_port = 50000;

  /* Setup  normal TCP flows */

  // Create a TCP sink to receive normal TCP packets on S_nodes
  ApplicationContainer tcp_sinkApps;
  Address tcp_sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), tcp_port));
  PacketSinkHelper tcp_sink ("ns3::TcpSocketFactory", tcp_sinkLocalAddress);

  for (uint32_t i = 0; i < TCPn; i++)
    {
      tcp_sinkApps.Add (tcp_sink.Install (S_nodes.Get (i)));
    }

  tcp_sinkApps.Start (Seconds (0.0));
  tcp_sinkApps.Stop (Seconds (maxSimulationTime + 100));

  // Create a Bulk Send Application to send TCP packets
  BulkSendHelper tcp_client ("ns3::TcpSocketFactory", Address ());

  // setup normal TCP flow (T_nodes -> S_nodes)
  ApplicationContainer tcp_clientApps;
  for (uint32_t i = 0; i < TCPn; i++)
    {
      AddressValue remoteAddress (InetSocketAddress (S_interfaces[i].GetAddress (0), tcp_port));
      tcp_client.SetAttribute ("Remote", remoteAddress);
      tcp_clientApps.Add (tcp_client.Install (T_nodes.Get (i)));
    }

  tcp_clientApps.Start (Seconds (attack_offTime));
  tcp_clientApps.Stop (Seconds (maxSimulationTime));

  /* LDoS attack */
#if (ATTACK)
  NS_LOG_INFO ("Cost of attack A = " << attack_cost);

  // UDP On-Off Application used by the attacker to create low-rate periodic bursts
  // targeting the bottleneck R2-R3, send UDP bursts to R3
  uint16_t udp_port = 9;

  OnOffHelper LDoSudp_client (
      "ns3::UdpSocketFactory",
      Address (InetSocketAddress (r2r3_interfaces.GetAddress (1), udp_port)));
  LDoSudp_client.SetConstantRate (DataRate (attack_R));
  LDoSudp_client.SetAttribute ("PacketSize", UintegerValue (1024));

  LDoSudp_client.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" +
                                                      std::to_string (attack_t) + "]"));
  LDoSudp_client.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" +
                                                       std::to_string (attack_offTime) + "]"));

  ApplicationContainer LDoSudp_clientApp = LDoSudp_client.Install (A_nodes.Get (0));
  LDoSudp_clientApp.Start (Seconds (attack_start));
  LDoSudp_clientApp.Stop (Seconds (attack_stop));

  /* create UDP sink on receiver R3 */

  PacketSinkHelper udp_sink ("ns3::UdpSocketFactory",
                             Address (InetSocketAddress (Ipv4Address::GetAny (), udp_port)));

  ApplicationContainer udp_sinkApp = udp_sink.Install (router_nodes.Get (2));
  udp_sinkApp.Start (Seconds (0.0));
  udp_sinkApp.Stop (Seconds (maxSimulationTime + 100));
#else
  NS_LOG_INFO ("LDoS attack disabled.");
#endif

  if (tracing)
    {
      AsciiTraceHelper ascii;
#if (ATTACK)
      p2p.EnableAsciiAll (
          ascii.CreateFileStream ("simple-LDoS_A" + std::to_string (attack_cost) + ".tr"));
      p2p.EnablePcapAll ("simple-LDoS_A" + std::to_string (attack_cost));
#else
      p2p.EnableAsciiAll (ascii.CreateFileStream ("simple-LDoS.tr"));
      p2p.EnablePcapAll ("simple-LDoS"); // prefix followed by node id and device id.
#endif
    }

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}
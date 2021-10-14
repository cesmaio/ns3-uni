/*
 * An LDoS attack consists of sending periodic short-lived and high-pulsed 
 * UDP packets through the net. The role of the data packet 
 * that is sent by the attacker is to cause congestion on the bottleneck link,
 * which results in packet loss in normal TCP flows.
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

NS_LOG_COMPONENT_DEFINE ("LDoSAttack");

#define ATTACK 0 // Run simulation with/out attacker

int
main (int argc, char *argv[])
{
  // Set up some default values for the simulation.
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (250));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("15kb/s"));

  CommandLine cmd (__FILE__);

  bool verbose = false, tracing = true;
  uint32_t BUn = 5;
  uint32_t TCPn = 10;
  uint32_t bTCPn = 5;

  cmd.AddValue ("verbose", "Explicit debugging", verbose);
  cmd.AddValue ("tracing", "Save tracing information", tracing);
  cmd.AddValue ("BUn", "Number of nodes sending background UDP traffic", BUn);
  cmd.AddValue ("bTCPn", "Number of nodes sending/receiving background TCP traffic", bTCPn);
  cmd.AddValue ("TCPn", "Number of nodes sending/receiving normal TCP traffic", TCPn);

  cmd.Parse (argc, argv);

  // logging
  LogComponentEnable ("LDoSAttack", LOG_LEVEL_INFO);
  if (verbose)
    {

      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_ALL);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_ALL);
    }

  NS_LOG_INFO ("Create nodes.");
  NodeContainer BU_nodes;
  BU_nodes.Create (BUn);
  NodeContainer T_nodes;
  T_nodes.Create (TCPn);
  NodeContainer S_nodes;
  S_nodes.Create (TCPn);
  NodeContainer BT_nodes;
  BT_nodes.Create (bTCPn);
  NodeContainer BS_nodes;
  BS_nodes.Create (bTCPn);
  NodeContainer router_nodes;
  router_nodes.Create (3); // three routers
#if (ATTACK)
  NodeContainer A_nodes;
  A_nodes.Create (1); // 1 attacker
#endif

  NS_LOG_INFO ("Connect nodes to routers.");
  T_nodes.Add (router_nodes.Get (0));
  BT_nodes.Add (router_nodes.Get (0));
  BU_nodes.Add (router_nodes.Get (0));
  BS_nodes.Add (router_nodes.Get (1));
  S_nodes.Add (router_nodes.Get (2));
#if (ATTACK)
  A_nodes.Add (router_nodes.Get (0));
#endif

  NS_LOG_INFO ("Crete channels.");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("15ms"));

  NetDeviceContainer T_devices = csma.Install (T_nodes);
  NetDeviceContainer BT_devices = csma.Install (BT_nodes);
  NetDeviceContainer BU_devices = csma.Install (BU_nodes);
  NetDeviceContainer BS_devices = csma.Install (BS_nodes);
  NetDeviceContainer S_devices = csma.Install (S_nodes);
#if (ATTACK)
  NetDeviceContainer A_devices = csma.Install (A_nodes);
#endif

  PointToPointHelper p2p;

  p2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("15ms"));
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
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer T_interfaces = ipv4.Assign (T_devices);
  // for BT nodes
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer BT_interfaces = ipv4.Assign (BT_devices);
  // for BU nodes
  ipv4.SetBase ("10.2.1.0", "255.255.255.0");
  Ipv4InterfaceContainer BU_interfaces = ipv4.Assign (BU_devices);
  // for BS nodes
  ipv4.SetBase ("11.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer BS_interfaces = ipv4.Assign (BS_devices);
  // for S nodes
  ipv4.SetBase ("11.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer S_interfaces = ipv4.Assign (S_devices);

  // for p2p (routers)
  ipv4.SetBase ("192.168.0.0", "255.255.255.252");
  Ipv4InterfaceContainer r1r2_interfaces = ipv4.Assign (r1r2);

  ipv4.SetBase ("192.168.1.0", "255.255.255.252");
  Ipv4InterfaceContainer r2r3_interfaces = ipv4.Assign (r2r3);

#if (ATTACK)
  ipv4.SetBase ("12.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer A_interfaces = ipv4.Assign (A_devices);
#endif

  NS_LOG_INFO ("Enable static global routing.");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  NS_LOG_INFO ("Create Applications.");

  /* Setup background and normal TCP flows */

  uint16_t tcp_port = 50000;
  // Create a packet sink to receive normal and background TCP packets on BS_nodes and S_nodes
  ApplicationContainer sinkApps;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), tcp_port));
  PacketSinkHelper sink ("ns3::TcpSocketFactory", sinkLocalAddress);

  for (uint32_t i = 0; i < bTCPn; i++)
    {
      sinkApps.Add (sink.Install (BS_nodes.Get (i)));
    }
  for (uint32_t i = 0; i < TCPn; i++)
    {
      sinkApps.Add (sink.Install (S_nodes.Get (i)));
    }

  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (20.0));

  // Create the OnOff applications to send TCP packets
  OnOffHelper tcp_client ("ns3::TcpSocketFactory", Address ());
  tcp_client.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  tcp_client.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  // setup background TCP flow (BT_nodes -> BS_nodes)
  ApplicationContainer bTCP_clientApps;
  for (uint32_t i = 0; i < bTCPn; i++)
    {
      AddressValue remoteAddress (InetSocketAddress (BS_interfaces.GetAddress (i), tcp_port));
      tcp_client.SetAttribute ("Remote", remoteAddress);
      bTCP_clientApps.Add (tcp_client.Install (BT_nodes.Get (i)));
    }

  bTCP_clientApps.Start (Seconds (2.0));
  bTCP_clientApps.Stop (Seconds (15.0));

  // setup normal TCP flow (T_nodes -> S_nodes)
  ApplicationContainer tcp_clientApps;
  for (uint32_t i = 0; i < TCPn; i++)
    {
      AddressValue remoteAddress (InetSocketAddress (S_interfaces.GetAddress (i), tcp_port));
      tcp_client.SetAttribute ("Remote", remoteAddress);
      tcp_clientApps.Add (tcp_client.Install (T_nodes.Get (i)));
    }

  tcp_clientApps.Start (Seconds (5.0));
  tcp_clientApps.Stop (Seconds (10.0));

  /* Send background UDP traffic from BU_nodes to R3 */

  uint16_t udp_port = 9;
  // Create a UdpServer application on R3
  UdpEchoServerHelper udp_server (udp_port);
  ApplicationContainer serverApps = udp_server.Install (router_nodes.Get (2));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (20.0));

  // Create one UdpClient application to send UDP datagrams
  uint32_t packetSize = 1024;
  uint32_t maxPacketCount = 10;
  Time interPacketInterval = Seconds (1);

  UdpEchoClientHelper udp_client (r2r3_interfaces.GetAddress (1), udp_port);
  udp_client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  udp_client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  udp_client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer udp_clientApps = udp_client.Install (BU_nodes);
  udp_clientApps.Start (Seconds (1.0));
  udp_clientApps.Stop (Seconds (20.0));

  /* LDoS attack */
#if (ATTACK)
#endif

  if (tracing)
    {
      AsciiTraceHelper ascii;
      p2p.EnableAsciiAll (ascii.CreateFileStream ("three-routers.tr"));
      p2p.EnablePcapAll ("three-routers"); // prefix followed by node id and device id.
      csma.EnablePcapAll ("three-routers");
    }

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}
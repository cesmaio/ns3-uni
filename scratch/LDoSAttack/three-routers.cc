
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LDoSAttack");

int
main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);

// turn on explicit debugging
#if 0
  LogComponentEnable ("LDoSAttack", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_ALL);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_ALL);
#endif

  uint32_t Tn = 10;
  uint32_t BTn = 5;
  uint32_t BUn = 5;
  uint32_t BSn = 5;
  uint32_t Sn = 10;

  cmd.AddValue ("Tn", "Number of nodes sending normal TCP traffic", Tn);
  cmd.AddValue ("BTn", "Number of nodes sending background TCP traffic", BTn);
  cmd.AddValue ("BUn", "Number of nodes sending background UDP traffic", BUn);
  cmd.AddValue ("BSn", "Number of nodes receiving background TCP traffic", BUn);
  cmd.AddValue ("Sn", "Number of nodes receiving normal TCP traffic", Sn);

  cmd.Parse (argc, argv);

  NS_LOG_INFO ("Create nodes.");
  NodeContainer T_nodes;
  T_nodes.Create (Tn);
  NodeContainer BT_nodes;
  BT_nodes.Create (BTn);
  NodeContainer BU_nodes;
  BU_nodes.Create (BUn);
  NodeContainer BS_nodes;
  BS_nodes.Create (BSn);
  NodeContainer S_nodes;
  S_nodes.Create (Sn);
  NodeContainer A_nodes;
  A_nodes.Create (1); // 1 attacker
  NodeContainer router_nodes;
  router_nodes.Create (3); // three routers

  // connect nodes to routers
  T_nodes.Add (router_nodes.Get (0));
  BT_nodes.Add (router_nodes.Get (0));
  BU_nodes.Add (router_nodes.Get (0));
  A_nodes.Add (router_nodes.Get (0));
  BS_nodes.Add (router_nodes.Get (1));
  S_nodes.Add (router_nodes.Get (2));

  NS_LOG_INFO ("Crete channels.");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("15ms"));

  NetDeviceContainer T_devices = csma.Install (T_nodes);
  NetDeviceContainer BT_devices = csma.Install (BT_nodes);
  NetDeviceContainer BU_devices = csma.Install (BU_nodes);
  NetDeviceContainer BS_devices = csma.Install (BS_nodes);
  NetDeviceContainer S_devices = csma.Install (S_nodes);
  NetDeviceContainer A_devices = csma.Install (A_nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("15ms"));
  // connect r1 and r2
  NetDeviceContainer r1r2 = p2p.Install (router_nodes.Get (0), router_nodes.Get (1));

  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("30ms"));
  // connect r2 and r3
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
  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer BU_interfaces = ipv4.Assign (BU_devices);
  // for A nodes
  ipv4.SetBase ("10.1.100.0", "255.255.255.0");
  Ipv4InterfaceContainer A_interfaces = ipv4.Assign (A_devices);
  // for BS nodes
  ipv4.SetBase ("11.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer BS_interfaces = ipv4.Assign (BS_devices);
  // for S nodes
  ipv4.SetBase ("11.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer S_interfaces = ipv4.Assign (S_devices);

  // for p2p
  ipv4.SetBase ("192.168.0.0", "255.255.255.0");
  Ipv4InterfaceContainer r1r2_interfaces = ipv4.Assign (r1r2);

  ipv4.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer r2r3_interfaces = ipv4.Assign (r2r3);

  NS_LOG_INFO ("Enable static global routing.");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  NS_LOG_INFO ("Create Applications.");

  // Send TCP packets from BT_nodes to S_nodes
  // Create a packet sink on the star "hub" to receive these packets
  uint16_t tcp_port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), tcp_port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (router_nodes.Get (0));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (10.0));

  // Create the OnOff applications to send TCP to the server
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", Address ());
  clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  //normally wouldn't need a loop here but the server IP address is different
  //on each p2p subnet
  ApplicationContainer tcp_clientApps;
  AddressValue remoteAddress (InetSocketAddress (BT_interfaces.GetAddress (0), tcp_port));
  clientHelper.SetAttribute ("Remote", remoteAddress);
  tcp_clientApps.Add (clientHelper.Install (BT_nodes.Get (0)));
  tcp_clientApps.Start (Seconds (1.0));
  tcp_clientApps.Stop (Seconds (10.0));

  // // Create a UdpServer application on r3
  // //
  // uint16_t udp_port = 9;
  // UdpEchoServerHelper server (udp_port);
  // ApplicationContainer serverApps = server.Install (router_nodes.Get (2));
  // serverApps.Start (Seconds (1.0));
  // serverApps.Stop (Seconds (10.0));

  // // Create one UdpClient application to send UDP datagrams from node Attacker to r3
  // uint32_t packetSize = 1024;
  // uint32_t maxPacketCount = 10;
  // Time interPacketInterval = Seconds (1);
  // UdpEchoClientHelper client (r2r3_interfaces.GetAddress (0), udp_port);
  // client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  // client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  // client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  // ApplicationContainer udp_clientApps = client.Install (A_nodes);
  // udp_clientApps.Start (Seconds (2.0));
  // udp_clientApps.Stop (Seconds (10.0));

  // configure tracing
  p2p.EnablePcapAll ("three-routers"); // prefix followed by node id and device id.
  csma.EnablePcapAll ("three-routers");

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();

  NS_LOG_INFO ("Done.");

  return 0;
}
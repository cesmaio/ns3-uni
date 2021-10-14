#include <iostream>
#include <fstream>
#include <string>
#include <cassert>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("tcpTest");

int
main (int argc, char *argv[])
{
  // Set up some default values for the simulation.
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (250));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("15kb/s"));

  CommandLine cmd (__FILE__);

  uint32_t N = 3;

  cmd.AddValue ("N", "Number of nodes sending/receiving TCP traffic", N);

  cmd.Parse (argc, argv);

  NS_LOG_INFO ("Create nodes.");

  NodeContainer r_nodes; //routers
  r_nodes.Create (3);
  NodeContainer S_nodes; //Tx
  S_nodes.Create (N);
  NodeContainer D_nodes; //Rx
  D_nodes.Create (N);

  NS_LOG_INFO ("Connect nodes to routers.");
  S_nodes.Add (r_nodes.Get (0));
  D_nodes.Add (r_nodes.Get (2));

  NS_LOG_INFO ("Crete channels.");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("15ms"));
  // connect LANs
  NetDeviceContainer S_devices = csma.Install (S_nodes);
  NetDeviceContainer D_devices = csma.Install (D_nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("5ms"));
  // connect routers
  NetDeviceContainer r0r1 = p2p.Install (r_nodes.Get (0), r_nodes.Get (1));
  NetDeviceContainer r1r2 = p2p.Install (r_nodes.Get (1), r_nodes.Get (2));

  NS_LOG_INFO ("Install internet stack on all nodes.");
  InternetStackHelper internet;
  internet.Install (S_nodes);
  internet.Install (D_nodes);
  internet.Install (r_nodes.Get (1));

  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  // S nodes
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer S_interfaces = ipv4.Assign (S_devices);
  // D nodes
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer D_interfaces = ipv4.Assign (D_devices);
  // routers
  ipv4.SetBase ("192.168.0.0", "255.255.255.0");
  Ipv4InterfaceContainer r0r1_interfaces = ipv4.Assign (r0r1);
  ipv4.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer r1r2_interfaces = ipv4.Assign (r1r2);

  NS_LOG_INFO ("Enable static global routing.");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  NS_LOG_INFO ("Create Applications.");

  // Create a packet sink to receive TCP packets on server nodes (D_nodes)
  uint16_t port = 50000;

  ApplicationContainer sinkApps;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sink ("ns3::TcpSocketFactory", sinkLocalAddress);

  for (uint32_t i = 0; i < N; i++)
    {
      sinkApps.Add (sink.Install (D_nodes.Get (i)));
    }

  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (5.0));

  // Create the OnOff applications to send TCP packets
  OnOffHelper client ("ns3::TcpSocketFactory", Address ());
  client.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  client.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  // send TCP packets from S_nodes to D_nodes
  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < N; i++)
    {
      AddressValue remoteAddress (InetSocketAddress (D_interfaces.GetAddress (i), port));
      client.SetAttribute ("Remote", remoteAddress);
      clientApps.Add (client.Install (S_nodes.Get (i)));
    }

  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (5.0));

  // setup tracing
  p2p.EnablePcapAll ("tcp-star-server");
  csma.EnablePcapAll ("tcp-star-server");

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}

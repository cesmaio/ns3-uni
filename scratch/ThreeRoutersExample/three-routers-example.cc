#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

/**
 * Network topology
 * 
 * Device1---|LAN1                                       LAN2|--- Device1
 *           |                                               |
 * Device2---|                                               |--- Device2
 *           |----- Router1 ------ Router3 ----- Router2 ----|
 * Device3---|                                               |--- Device3
 *           |                                               |
 * ...    ---|                                               |--- ...
 **/

//For colorful console printing
/*
 * Usage example :
 *    std::cout << BOLD_CODE << "some bold text << END_CODE << std::endl;
 *
 *    std::cout << YELLOW_CODE << BOLD_CODE << "some bold yellow text << END_CODE << std::endl;
 *
 */
#define YELLOW_CODE "\033[33m"
#define TEAL_CODE "\033[36m"
#define BOLD_CODE "\033[1m"
#define RED_CODE "\033[91m"
#define END_CODE "\033[0m"

uint32_t total_client_tx = 0;
uint32_t total_client_rx = 0;
uint32_t total_server_tx = 0;
uint32_t total_server_rx = 0;

void
CheckQueueSize (std::string context, uint32_t before, uint32_t after)
{
  std::cout << YELLOW_CODE << context << END_CODE << std::endl;
  std::cout << "\tTxQueue Size = " << after << std::endl;
}

void
BackoffTrace (std::string context, Ptr<const Packet> packet)
{
  std::cout << YELLOW_CODE << context << END_CODE << std::endl;
  EthernetHeader hdr;
  if (packet->PeekHeader (hdr))
    {
      std::cout << "\t" << Now () << " Packet from " << hdr.GetSource () << " to "
                << hdr.GetDestination () << " is experiencing backoff" << std::endl;
    }
}

void
ClientTx (std::string context, Ptr<const Packet> packet)
{
  total_client_tx++;
}
void
ClientRx (std::string context, Ptr<const Packet> packet)
{
  total_client_rx++;
}
void
ServerTx (std::string context, Ptr<const Packet> packet)
{
  total_server_tx++;
}
void
ServerRx (std::string context, Ptr<const Packet> packet)
{
  total_server_rx++;
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;

  uint32_t n1 = 3;
  uint32_t n2 = 3;

  cmd.AddValue ("n1", "Number of LAN 1 nodes", n1);
  cmd.AddValue ("n2", "Number of LAN 2 nodes", n2);

  cmd.Parse (argc, argv);

  //LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  //LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // ## Create Simulation nodes
  //For the first network
  NodeContainer lan1_nodes;
  lan1_nodes.Create (n1);

  //For the second network
  NodeContainer lan2_nodes;
  lan2_nodes.Create (n2);

  //for the nodes in the middle. (routers)
  NodeContainer router_nodes;
  router_nodes.Create (3);
  // or directly:
  // Ptr<Node> r1 = CreateObject<Node> ();
  // Ptr<Node> r2 = CreateObject<Node> ();
  // Ptr<Node> r3 = CreateObject<Node> ();

  //Let's create LAN 1 by attaching a CsmaNetDevice to all the nodes on the LAN
  CsmaHelper csma1;
  csma1.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma1.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  //Router 1 is accessible on LAN 1, so we add it to the list nodes.
  lan1_nodes.Add (router_nodes.Get (0)); // or r1
  //Actually attaching CsmaNetDevice to all LAN 1 nodes.
  NetDeviceContainer lan1Devices = csma1.Install (lan1_nodes);

  //Doing the same for LAN 2
  CsmaHelper csma2;
  csma2.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma2.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  //Router 2 is on LAN 2, so we add it to the node container
  lan2_nodes.Add (router_nodes.Get (1));

  NetDeviceContainer lan2Devices = csma2.Install (lan2_nodes);

  /* So far our two LANs are disjoint, r1 and r2 need to be connected */
  //A PointToPoint connection between the two routers
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // connect r1 and r2 to r3
  NetDeviceContainer firstHopLinkDevs =
      pointToPoint.Install (router_nodes.Get (0), router_nodes.Get (2));

  NetDeviceContainer secondHopLinkDevs =
      pointToPoint.Install (router_nodes.Get (2), router_nodes.Get (1));

  // Setup comunication stack
  InternetStackHelper stack;
  // stack.InstallAll ();
  stack.Install (lan1_nodes);
  stack.Install (lan2_nodes);
  stack.Install (router_nodes.Get (2));
  //Setting IP addresses. Notice that router 1 & 2 are in LAN 1 & 2 respectively.

  Ipv4AddressHelper address;
  //For LAN 1 (10.1.1.*)
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer lan1interfaces;
  lan1interfaces = address.Assign (lan1Devices);
  //For LAN 2 (10.1.2.*)
  address.SetBase ("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer lan2interfaces;
  lan2interfaces = address.Assign (lan2Devices);
  //For PointToPoint
  address.SetBase ("10.1.100.0", "255.255.255.0");
  Ipv4InterfaceContainer routerInterfaces;
  routerInterfaces = address.Assign (firstHopLinkDevs);

  address.SetBase ("10.1.200.0", "255.255.255.0");
  Ipv4InterfaceContainer routerInterfaces2;
  routerInterfaces = address.Assign (secondHopLinkDevs);

  //Let's install a UdpEchoServer on all nodes of LAN2
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (lan2_nodes);
  serverApps.Start (Seconds (0));
  serverApps.Stop (Seconds (10));

  //Let's create UdpEchoClients in all LAN1 nodes.
  UdpEchoClientHelper echoClient (lan2interfaces.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (200)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  //We'll install UdpEchoClient on two nodes in lan1 nodes
  NodeContainer clientNodes (lan1_nodes.Get (0), lan1_nodes.Get (1));

  ApplicationContainer clientApps = echoClient.Install (clientNodes);
  clientApps.Start (Seconds (1));
  clientApps.Stop (Seconds (10));

  //For routers to be able to forward packets, they need to have routing rules.
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Enable data transmission
  csma1.EnablePcap ("lan1", lan1Devices);
  csma2.EnablePcap ("lan2", lan2Devices);
  pointToPoint.EnablePcapAll ("routers");
  pointToPoint.EnableAscii ("ascii-p2p", router_nodes);

  // ## Traceroutes
  //Config::Connect("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/PacketsInQueue", MakeCallback(&CheckQueueSize));
  //Config::Connect("/NodeList/*/DeviceList/*/$ns3::CsmaNetDevice/TxQueue/PacketsInQueue", MakeCallback(&CheckQueueSize));

  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::CsmaNetDevice/MacTxBackoff",
                   MakeCallback (&BackoffTrace));

  Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::UdpEchoClient/Tx",
                   MakeCallback (&ClientTx));
  Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::UdpEchoClient/Rx",
                   MakeCallback (&ClientRx));

  // Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::UdpEchoServer/Tx",
  //                  MakeCallback (&ServerTx));
  Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::UdpEchoServer/Rx",
                   MakeCallback (&ServerRx));

  Simulator::Stop (Seconds (20));
  Simulator::Run ();

  // just print totals
  std::cout << "Client Tx: " << total_client_tx << "\tClient Rx: " << total_client_rx << std::endl;
  std::cout << "Server Rx: " << total_server_rx << std::endl;

  Simulator::Destroy ();
  return 0;
}
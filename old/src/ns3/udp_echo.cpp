/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

NS_LOG_COMPONENT_DEFINE ("FirstScriptExample");

int main (int argc, char *argv[])
{
  // TODO: comment here
  ns3::CommandLine cmd;
  cmd.Parse (argc, argv);
  
  // set the time resolution of the simulator to nanoseconds
  ns3::Time::SetResolution (ns3::Time::NS);
  
  // enable two logging components in the built-in UdpEchoClient and
  // UdpEchoServer ns3 applications
  ns3::LogComponentEnable ("UdpEchoClientApplication", ns3::LOG_LEVEL_INFO);
  ns3::LogComponentEnable ("UdpEchoServerApplication", ns3::LOG_LEVEL_INFO);

  // create an ns3 NodeContainer object and then create two ns3 Node objects
  // in that container
  ns3::NodeContainer nodes;
  nodes.Create (2);

  // create an ns3 p2pHelper object that will help to
  // 1) create ns3 PointToPoint NetDevice objects
  // 2) create ns3 PointToPoint Channel objects between NetDevice objects
  // 3) "install" the connected NetDevice objects in the Node objects
  ns3::PointToPointHelper pointToPoint;

  // set data rate of NetDevice objects
  pointToPoint.SetDeviceAttribute ("DataRate", ns3::StringValue ("5Mbps"));

  // set delay in Channel objects
  pointToPoint.SetChannelAttribute ("Delay", ns3::StringValue ("2ms"));

  // create NetDeviceContainer object hold "installed" NetDevice objects
  ns3::NetDeviceContainer devices;

  // "install" NetDevice object in each Node object in NodeContainer nodes
  devices = pointToPoint.Install (nodes);

  // create ns3 InternetStackHelper to install an internet protocol stack
  // on each Node object
  ns3::InternetStackHelper stack;
  stack.Install (nodes);

  // create ns3 Ipv4AddressHelper that will assign IP addresses starting with
  // base IP 10.1.1.0 and network mask 255.255.255.0
  ns3::Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  // create ns3 Ipv4InterfaceContainer to hold internet addresses "assigned"
  // to "installed" NetDevice objects (Ipv4Interface object represents
  // the association between NetDevice and IP address)
  ns3::Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // create ns3 UdpEchoServerHelper object that will create a UdpEchoServer
  // application object that utilizes port 9
  ns3::UdpEchoServerHelper echoServer (9);

  // create and "install" the UdpEchoServer application on Node 1 and hold the 
  // resulting Application object in ns3 ApplicationContainer
  ns3::ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));

  // set time the applications in serverApps will start and stop running
  serverApps.Start (ns3::Seconds (1.0));
  serverApps.Stop (ns3::Seconds (10.0));

  // create ns3 UdpEchoClientHelper object that will create a UdpEchoClient
  // application object that semds traffic to Node 1 on port 9
  ns3::UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);

  // set the packet attributes for any created UdpEchoClient applications
  echoClient.SetAttribute ("MaxPackets", ns3::UintegerValue (1));
  echoClient.SetAttribute ("Interval", ns3::TimeValue (ns3::Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", ns3::UintegerValue (1024));

  // create and "install" the UdpEchoClient application on Node 0 and hold the
  // resulting Application object in ns3 Application Container
  ns3::ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));

  // set time the applications in clientApps will start and stop running
  clientApps.Start (ns3::Seconds (2.0));
  clientApps.Stop (ns3::Seconds (10.0));

  // let simulator run until there are no more events... alternatively use
  // ns3::Simulator::Stop (ns3::Seconds(VAL));

  // start simulator and destroy/free the simulator when simulation is complete
  ns3::Simulator::Run (); 
  ns3::Simulator::Destroy ();
  return 0;
}

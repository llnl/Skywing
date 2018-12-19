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
#include "HeartHelper.hpp"

NS_LOG_COMPONENT_DEFINE ("SkynetStartup");

int main (int argc, char *argv[])
{
  // TODO: comment here
  ns3::CommandLine cmd;
  cmd.Parse (argc, argv);
  
  // set the time resolution of the simulator to nanoseconds
  ns3::Time::SetResolution (ns3::Time::NS);
  
  ns3::LogComponentEnable ("HeartApplication", ns3::LOG_LEVEL_INFO);

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

  ns3::HeartHelper heartFactory;

  ns3::ApplicationContainer hearts = heartFactory.Install (nodes);

  ns3::Simulator::Stop (ns3::Seconds(10.0));

  // start simulator and destroy/free the simulator when simulation is complete
  ns3::Simulator::Run (); 
  ns3::Simulator::Destroy ();
  return 0;
}

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

#include <cassert>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"
#include "ns3/csma-module.h"
#include "parameter-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CS268Simulation");

NetDeviceContainer datacenter_connect(Ptr<Node> a, Ptr<Node> b) {
    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", StringValue ("10Mbps"));
    csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (15)));
    return csma.Install(NodeContainer(a, b));

    // PointToPointHelper pointToPoint;
    // pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    // pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));
    // return pointToPoint.Install(NodeContainer(a, b));
}

class Rack {
public:
    Rack(int numhosts, Ipv4Address network, Ipv4Mask mask);

    void AddEgress(Ptr<NetDevice> egress, Ptr<NetDevice> nextlayer);
    void Init();

    NodeContainer hosts;
    Ptr<Node> topOfRack;
    Ipv4InterfaceContainer hostIPs;

private:
    NetDeviceContainer tordevs;
    NetDeviceContainer hostdevs;
    Ipv4Address network;
    Ipv4Mask mask;
};

Rack::Rack(int numhosts, Ipv4Address netmask_addr, Ipv4Mask netmask_mask) : network(netmask_addr), mask(netmask_mask) {
    this->hosts.Create(numhosts);
    this->topOfRack = CreateObject<Node>();

    for (int i = 0; i != numhosts; i++) {
        NetDeviceContainer netdevs = datacenter_connect(this->topOfRack, this->hosts.Get(i));
        this->tordevs.Add(netdevs.Get(0));
        this->hostdevs.Add(netdevs.Get(1));
    }

    InternetStackHelper stack;
    stack.Install(this->hosts);
}

void Rack::AddEgress(Ptr<NetDevice> egress, Ptr<NetDevice> nextlayer) {
    this->tordevs.Add(egress);
    this->hostdevs.Add(nextlayer);
}

void Rack::Init() {
    BridgeHelper bridge;
    bridge.Install(this->topOfRack, this->tordevs);

    Ipv4AddressHelper addresses(this->network, this->mask);
    this->hostIPs = addresses.Assign(this->hostdevs);

    // InternetStackHelper stack;
    // stack.Install(NodeContainer(this->topOfRack));
    // addresses.Assign(this->tordevs);
}

class Topology {
public:
    Topology(int numRacks, int rackSize);

    void setColocate();
    void setCluster();
    void setStride();
    void setRandom();

    int numRacks;
    int rackSize;
    Rack** racks;
    Ptr<Node> topSwitch;
};

Topology::Topology(int numRacks, int rackSize) {
    this->numRacks = numRacks;
    this->rackSize = rackSize;

    this->racks = new Rack*[numRacks];
    this->topSwitch = CreateObject<Node>();
    for (int i = 0; i != numRacks; i++) {
        char subnet[20];
        sprintf(subnet, "10.1.%d.0", i+1);
        Rack* rack = new Rack(rackSize, subnet, "255.255.255.0");
        this->racks[i] = rack;

        NetDeviceContainer link = datacenter_connect(this->topSwitch, rack->topOfRack);
        rack->AddEgress(link.Get(1), link.Get(0));
    }

    InternetStackHelper stack;
    stack.Install(NodeContainer(this->topSwitch));

    for (int i = 0; i != numRacks; i++) {
        this->racks[i]->Init();
    }
}

void Topology::setColocate() {

    for (int i = 0; i != this->numRacks; i++) {
        Rack* rack = this->racks[i];
        ParameterServerHelper paramServer (9);
        paramServer.SetAttribute ("NumWorkers", UintegerValue (this->rackSize - 1));
        paramServer.SetAttribute ("ServerNum", UintegerValue(i));
        ApplicationContainer serverApps = paramServer.Install (rack->hosts.Get (0));
        serverApps.Start(Seconds(1.0));

        for (int j = 1; j != this->rackSize; j++) {
            ParameterClientHelper paramClient (rack->hostIPs.GetAddress (0), 9);
            paramClient.SetAttribute ("ClientNum", UintegerValue(j - 1));
            paramClient.SetAttribute ("ServerNum", UintegerValue(i));
            ApplicationContainer clientApps = paramClient.Install (rack->hosts.Get (j));
            clientApps.Start(Seconds(1.0));
        }
    }
}

void Topology::setCluster() {
    assert(this->numRacks == this->rackSize);


    Rack* rack0 = this->racks[0];
    for (int j = 0; j != this->rackSize; j++) {
        ParameterServerHelper paramServer (9);
        paramServer.SetAttribute ("NumWorkers", UintegerValue (this->rackSize - 1));
        paramServer.SetAttribute ("ServerNum", UintegerValue(j));
        ApplicationContainer serverApps = paramServer.Install (rack0->hosts.Get (j));
        serverApps.Start(Seconds(1.0));
    }
    for (int i = 1; i != this->numRacks; i++) {
        Rack* rack = this->racks[i];
        for (int j = 0; j != this->rackSize; j++) {
            ParameterClientHelper paramClient (rack0->hostIPs.GetAddress (j), 9);
            paramClient.SetAttribute("ServerNum", UintegerValue(j));
            paramClient.SetAttribute("ClientNum", UintegerValue(i-1));
            ApplicationContainer clientApps = paramClient.Install (rack->hosts.Get (j));
            clientApps.Start(Seconds(1.0));
        }
    }
}

void Topology::setStride() {

    for (int i = 0; i != this->numRacks; i++) {
        Rack* rack = this->racks[i];
        ParameterServerHelper paramServer (9);
        paramServer.SetAttribute ("NumWorkers", UintegerValue (this->rackSize - 1));
        paramServer.SetAttribute ("ServerNum", UintegerValue(i));
        ApplicationContainer serverApps = paramServer.Install (rack->hosts.Get (0));
        serverApps.Start(Seconds(1.0));

        int rotatedi = (i+1)%numRacks;
        Rack* serverRack = this->racks[rotatedi];
        // if (i == 0) {
        //     serverRack = this->racks[this->numRacks - 1];
        // } else {
        //     serverRack = this->racks[i - 1];
        // }

        for (int j = 1; j != this->rackSize; j++) {
            ParameterClientHelper paramClient (serverRack->hostIPs.GetAddress (0), 9);
            paramClient.SetAttribute("ServerNum", UintegerValue(rotatedi));
            paramClient.SetAttribute("ClientNum", UintegerValue(j - 1));
            ApplicationContainer clientApps = paramClient.Install (rack->hosts.Get (j));
            clientApps.Start(Seconds(1.0));
        }
    }
}

void Topology::setRandom() {
    std::vector<int> locations(this->numRacks * this->rackSize);
    for (int i = 0; i != (int) locations.size(); i++) {
        locations[i] = i;
    }
    NS_LOG_INFO(locations.size());
    std::random_shuffle(locations.begin(), locations.end());


    int servernum = 0;
    int i = 0;
    for (int j = 0; j != this->numRacks; j++) {
        int location = locations[i++];
        Rack* serverRack = this->racks[location / this->rackSize];
        int hostIndex = location % this->rackSize;

        ParameterServerHelper paramServer (9);
        paramServer.SetAttribute ("NumWorkers", UintegerValue (this->rackSize - 1));
        paramServer.SetAttribute ("ServerNum", UintegerValue (servernum));
        ApplicationContainer serverApps = paramServer.Install (serverRack->hosts.Get (hostIndex));
        serverApps.Start(Seconds(1.0));

        int clientnum = 0;
        for (int k = 1; k != this->rackSize; k++) {
            int clientLocation = locations[i++];
            Rack* clientRack = this->racks[clientLocation / this->rackSize];
            int clientHostIndex = clientLocation % this->rackSize;

            ParameterClientHelper paramClient (serverRack->hostIPs.GetAddress (hostIndex), 9);
            paramClient.SetAttribute("ClientNum", UintegerValue(clientnum++));
            paramClient.SetAttribute("ServerNum", UintegerValue(servernum));

            ApplicationContainer clientApps = paramClient.Install (clientRack->hosts.Get (clientHostIndex));
            clientApps.Start(Seconds(1.0));
        }
        servernum++;

    }
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("ParameterClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("ParameterServerApplication", LOG_LEVEL_INFO);

  Topology* topology = new Topology(8, 8);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  topology->setRandom();
  //topology->setStride();
  //topology->setCluster();
  //topology->setColocate();


  Simulator::Stop (Seconds(30));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}

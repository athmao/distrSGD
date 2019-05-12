/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 *  Copyright (c) 2007,2008,2009 INRIA, UDCAST
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
 * Author: Amine Ismail <amine.ismail@sophia.inria.fr>
 *                      <amine.ismail@udcast.com>
 */

#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/tcp-socket-base.h"

//#include "seq-ts-header.h"
#include "parameter-server.h"
#include <cassert>
#include <iostream>
#include <chrono>
#include <random>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <iostream>
#include <chrono>
#include <random>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("ParameterServerApplication");

NS_OBJECT_ENSURE_REGISTERED (ParameterServer);

TypeId
ParameterServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ParameterServer")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<ParameterServer> ()
    .AddAttribute ("Port",
                   "Port on which we listen for incoming packets.",
                   UintegerValue (100),
                   MakeUintegerAccessor (&ParameterServer::m_port),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("NumWorkers",
                   "Number of workers",
                   UintegerValue (2),
                   MakeUintegerAccessor (&ParameterServer::m_numWorkers),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("MTU",
                   "MTU",
                   UintegerValue (1500),
                   MakeUintegerAccessor (&ParameterServer::m_mtu),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("ParameterUpdateSize",
                   "Parameter Update Size",
                   UintegerValue (97490),
                   MakeUintegerAccessor (&ParameterServer::m_parameterUpdateSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("GradientUpdateSize",
                   "GradientUpdateSize",
                   UintegerValue (97490),
                   MakeUintegerAccessor (&ParameterServer::m_gradientUpdateSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute("ServerNum",
                  "ServerNum",
                  UintegerValue(0),
                  MakeUintegerAccessor(&ParameterServer::m_serverNum),
                  MakeUintegerChecker<uint32_t>())
  ;
  return tid;
}

ParameterServer::ParameterServer ()
{
  NS_LOG_FUNCTION (this);
  this->workers_left = 0;
  m_sendEvent = EventId ();
}

ParameterServer::~ParameterServer ()
{
  NS_LOG_FUNCTION (this);
}

void
ParameterServer::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void
ParameterServer::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
      m_socket = DynamicCast<TcpSocket> (Socket::CreateSocket (GetNode (), tid));
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (),
                                                   m_port);
       std::ifstream ifile("aggregation_delay_data.txt");

       if (!ifile.is_open()) {
           //NS_LOG_INFO("There was a problem opening the input file!");
           exit(1);
       }

       double num = 0.0;
       while (ifile >> num) {
           this->aggregation_distribution.push_back(num);
       }
      m_socket->SetAcceptCallback (MakeCallback (&ParameterServer::HandleRequest, this), MakeCallback (&ParameterServer::HandleAccept, this));
      if (m_socket->Bind (local) == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }
      m_socket->Listen();
    }

}

bool
ParameterServer::HandleRequest(Ptr<Socket> socket, const Address& address) {
    //NS_LOG_INFO (Simulator::Now ().GetSeconds () << ": Server #" << m_serverNum << " gets a connection request");
    return true;
}

void
ParameterServer::HandleAccept(Ptr<Socket> socket, const Address& address) {
    //NS_LOG_INFO (Simulator::Now ().GetSeconds () << ": Server #" << m_serverNum << " accepts a connection request");

    struct conn_state state;
    state.socket = socket;
    state.bytes_left_recv = 0;
    state.bytes_left_send = 0;

    /* Attach callbacks to the socket. */
    socket->SetSendCallback(MakeCallback(&ParameterServer::ContinueParameterUpdate, this));
    socket->SetRecvCallback(MakeCallback(&ParameterServer::ReceiveGradientUpdate, this));

    this->worker_connections.push_back(state);
    if (this->worker_connections.size() == this->m_numWorkers) {
        this->SendParameterUpdate();
    } else {
        assert (this->worker_connections.size() < this->m_numWorkers);
    }
}

void
ParameterServer::SendParameterUpdate() {
    NS_LOG_INFO (Simulator::Now ().GetSeconds () << ": Server #" << m_serverNum << " broadcasts parameter update");

    this->workers_left = this->m_numWorkers;
    for (size_t i = 0; i != this->worker_connections.size(); i++) {
        struct conn_state& worker = this->worker_connections[i];
        worker.bytes_left_send = this->m_parameterUpdateSize;

        Ptr<Socket> socket = worker.socket;
        //NS_LOG_INFO (Simulator::Now ().GetSeconds () << ": Server #" << m_serverNum << " starts parameter update: " << i);
        this->ContinueParameterUpdate(socket, socket->GetTxAvailable());
    }
}

void
ParameterServer::ContinueParameterUpdate(Ptr<Socket> socket, uint32_t ready) {
    // TODO: fix this inefficient scan
    for (size_t i = 0; i != this->worker_connections.size(); i++) {
        struct conn_state& worker = this->worker_connections[i];

        if (worker.socket == socket) {
            uint32_t to_send;
            int actual;
            do {
                to_send = this->m_mtu - 40;
                if (to_send > ready) {
                    to_send = ready;
                }
                if (to_send > worker.bytes_left_send) {
                    to_send = worker.bytes_left_send;
                }

                if (to_send == 0) {
                    break;
                }

                Ptr<Packet> packet = Create<Packet> (to_send);
                actual = socket->Send(packet);
                if (actual > 0) {
                    worker.bytes_left_send -= actual;
                    if (worker.bytes_left_send == 0) {
                        //NS_LOG_INFO (Simulator::Now ().GetSeconds () << ": Server #" << m_serverNum << " finishes parameter update");
                        worker.bytes_left_recv = this->m_gradientUpdateSize;
                    }
                }
            } while (actual == (int) to_send);
        }
    }
}

void
ParameterServer::ReceiveGradientUpdate(Ptr<Socket> socket) {
    // TODO: fix this inefficient scan
    for (size_t i = 0; i != this->worker_connections.size(); i++) {
        struct conn_state& worker = this->worker_connections[i];

        if (worker.socket == socket) {
            if (worker.bytes_left_recv == 0) {
                return;
            }

            Ptr<Packet> packet;
            while (packet = socket->Recv(worker.bytes_left_recv, 0)) {
                uint32_t size = packet->GetSize();
                if (size == 0) {
                    break;
                }
                worker.bytes_left_recv -= size;
                if (worker.bytes_left_recv == 0) {
                    this->workers_left--;
                    if (this->workers_left == 0) {
                        //NS_LOG_INFO (Simulator::Now ().GetSeconds () << ": Server #" << m_serverNum << " received gradient update");

                        // int rand_index = rand() % this->aggregation_distribution.size();
                        // double rand_delay = this->aggregation_distribution[rand_index];
                        // this->ScheduleParameterUpdate(Seconds(rand_delay));

                        unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
                        std::default_random_engine generator (seed);
                        std::normal_distribution<double> distribution (0.612/4.0,0.0384/4.0);
                        this->ScheduleParameterUpdate(Seconds(distribution(generator)));
                    }
                    return;
                }
            }
        }
    }
}

void
ParameterServer::ScheduleParameterUpdate (Time dt)
{
  NS_LOG_FUNCTION (this << dt);
  m_sendEvent = Simulator::Schedule (dt, &ParameterServer::SendParameterUpdate, this);
}

void
ParameterServer::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0)
    {
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }

  // TODO: properly NULL out the callbacks on all sockets

  Simulator::Cancel (m_sendEvent);
}

} // Namespace ns3

/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2007 University of Washington
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
 */
#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/tcp-socket-base.h"
#include "parameter-client.h"
#include <cassert>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <iostream>
#include <chrono>
#include <random>


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("ParameterClientApplication");

NS_OBJECT_ENSURE_REGISTERED (ParameterClient);

TypeId
ParameterClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ParameterClient")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<ParameterClient> ()
    .AddAttribute ("RemoteAddress",
                   "The destination Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&ParameterClient::m_peerAddress),
                   MakeAddressChecker ())
    .AddAttribute ("RemotePort",
                   "The destination port of the outbound packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&ParameterClient::m_peerPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("MTU",
                   "MTU",
                   UintegerValue (1500),
                   MakeUintegerAccessor (&ParameterClient::m_mtu),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("ParameterUpdateSize",
                   "Parameter Update Size",
                   UintegerValue (97490),
                   MakeUintegerAccessor (&ParameterClient::m_parameterUpdateSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("GradientUpdateSize",
                   "GradientUpdateSize",
                   UintegerValue (97490),
                   MakeUintegerAccessor (&ParameterClient::m_gradientUpdateSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute("ClientNum",
                  "ClientNum",
                  UintegerValue(0),
                  MakeUintegerAccessor(&ParameterClient::m_clientNum),
                  MakeUintegerChecker<uint32_t>())
    .AddAttribute("ServerNum",
                  "ServerNum",
                  UintegerValue(0),
                  MakeUintegerAccessor(&ParameterClient::m_serverNum),
                  MakeUintegerChecker<uint32_t>())
  ;
  return tid;
}

ParameterClient::ParameterClient ()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_sendEvent = EventId ();

  this->recv_bytes_left = 0;
  this->send_bytes_left = 0;
}

ParameterClient::~ParameterClient()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
}

void
ParameterClient::SetRemote (Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = ip;
  m_peerPort = port;
}

void
ParameterClient::SetRemote (Address addr)
{
  NS_LOG_FUNCTION (this << addr);
  m_peerAddress = addr;
}

void
ParameterClient::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void
ParameterClient::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);

      this->recv_bytes_left = this->m_parameterUpdateSize;

      std::ifstream ifile("gradient_delay_data.txt");

      if (!ifile.is_open()) {
          //NS_LOG_INFO("There was a problem opening the input file!");
          exit(1);
      }

      double num = 0.0;
      while (ifile >> num) {
          this->delay_distribution.push_back(num);
      }

      m_socket->SetRecvCallback (MakeCallback (&ParameterClient::ReceiveParameterUpdate, this));
      m_socket->SetSendCallback (MakeCallback (&ParameterClient::ContinueGradientUpdate, this));

      if (Ipv4Address::IsMatchingType(m_peerAddress) == true)
        {
          if (m_socket->Bind () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
          //NS_LOG_INFO (Simulator::Now ().GetSeconds () << ": Client #" << m_clientNum << " made a connection request to Server #" << m_serverNum);
          m_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
      else if (InetSocketAddress::IsMatchingType (m_peerAddress) == true)
        {
          if (m_socket->Bind () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
          m_socket->Connect (m_peerAddress);
        }
      else
        {
          NS_ASSERT_MSG (false, "Incompatible address type: " << m_peerAddress);
        }
    }

}

void
ParameterClient::ReceiveParameterUpdate (Ptr<Socket> socket)
{
    assert (this->recv_bytes_left > 0);

    Ptr<Packet> packet;
    while (packet = m_socket->Recv(this->recv_bytes_left, 0)) {
        uint32_t size = packet->GetSize();
        if (size == 0) {
            break;
        }
        this->recv_bytes_left -= size;
        if (this->recv_bytes_left == 0) {
            //NS_LOG_INFO (Simulator::Now ().GetSeconds () << ": Client #" << m_clientNum << " received parameter update from Server #" << m_serverNum);
            this->send_bytes_left = this->m_gradientUpdateSize;
            // int rand_index = rand() % this->delay_distribution.size();
            // double rand_delay = this->delay_distribution[rand_index];
            // this->ScheduleGradientUpdate(Seconds(rand_delay));

            unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
            std::default_random_engine generator (seed);
            std::normal_distribution<double> distribution (0.6383/4.0,0.2673/4.0);
            double delay = distribution(generator);
            if (delay < 0.05) {
              delay = 0.05;
            }
            this->ScheduleGradientUpdate(Seconds(delay));
            return;
        }
    }
}

void
ParameterClient::ScheduleGradientUpdate (Time dt)
{
  NS_LOG_FUNCTION (this << dt);
  m_sendEvent = Simulator::Schedule (dt, &ParameterClient::SendGradientUpdate, this);
}

void
ParameterClient::SendGradientUpdate ()
{
    //NS_LOG_INFO (Simulator::Now ().GetSeconds () << ": Client #" << m_clientNum << " sends gradient update to Server #" << m_serverNum);
    this->ContinueGradientUpdate(this->m_socket, this->m_socket->GetTxAvailable());
}

void
ParameterClient::ContinueGradientUpdate(Ptr<Socket> socket, uint32_t ready)
{
    uint32_t to_send;
    int actual;
    do {
        to_send = this->m_mtu - 40;
        if (to_send > ready) {
            to_send = ready;
        }
        if (to_send > this->send_bytes_left) {
            to_send = this->send_bytes_left;
        }

        if (to_send == 0 || this->recv_bytes_left > 0) {
            break;
        }

        Ptr<Packet> packet = Create<Packet> (to_send);
        actual = socket->Send(packet);
        if (actual > 0) {
            this->send_bytes_left -= actual;
            if (this->send_bytes_left == 0) {
                //NS_LOG_INFO (Simulator::Now ().GetSeconds () << ": Client #" << m_clientNum << " finishes up gradient update");
                this->recv_bytes_left = this->m_parameterUpdateSize;
            }
        }
    } while (actual == (int) to_send);
}

void
ParameterClient::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0)
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_socket = 0;
    }

  Simulator::Cancel (m_sendEvent);
}

} // Namespace ns3

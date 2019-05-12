/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007,2008,2009 INRIA, UDCAST
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
 *
 */

#ifndef PARAMETER_SERVER_H
#define PARAMETER_SERVER_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/address.h"
#include "ns3/traced-callback.h"
#include "ns3/tcp-socket-base.h"
#include <vector>


namespace ns3 {
/**
 * \ingroup applications
 * \defgroup udpclientserver UdpClientServer
 */

struct conn_state {
    Ptr<Socket> socket;
    uint32_t bytes_left_recv;
    uint32_t bytes_left_send;
};

/**
 * \ingroup udpclientserver
 *
 * \brief A UDP server, receives UDP packets from a remote host.
 *
 * UDP packets carry a 32bits sequence number followed by a 64bits time
 * stamp in their payloads. The application uses the sequence number
 * to determine if a packet is lost, and the time stamp to compute the delay.
 */
class ParameterServer : public Application
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  ParameterServer ();
  virtual ~ParameterServer ();

protected:
  virtual void DoDispose (void);

private:

  virtual void StartApplication (void);
  virtual void StopApplication (void);

  bool HandleRequest(Ptr<Socket> socket, const Address& address);

  void HandleAccept(Ptr<Socket> socket, const Address& address);

  void SendParameterUpdate();

  void ContinueParameterUpdate(Ptr<Socket> socket, uint32_t ready);

  void ReceiveGradientUpdate(Ptr<Socket> socket);


  void ScheduleParameterUpdate (Time dt);

  uint16_t m_port; //!< Port on which we listen for incoming packets.
  Ptr<TcpSocket> m_socket; //!< IPv4 Socket
  //Ptr<Socket> m_socket6; //!< IPv6 Socket

  std::vector<struct conn_state> worker_connections;
  int workers_left;

  EventId m_sendEvent; //!< Event to send the next packet
  uint16_t m_numWorkers;

  uint32_t m_mtu;
  uint32_t m_parameterUpdateSize;
  uint32_t m_gradientUpdateSize;
  uint32_t m_serverNum;

  std::vector<double> aggregation_distribution;

};

} // namespace ns3

#endif /* UDP_SERVER_H */

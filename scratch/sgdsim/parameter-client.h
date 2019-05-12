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

#ifndef PARAMETER_CLIENT_H
#define PARAMETER_CLIENT_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/traced-callback.h"
#include "ns3/tcp-socket-base.h"
#include <vector>

namespace ns3 {

class Socket;
class Packet;

/**
 * \ingroup Parameter
 * \brief A Udp Echo client
 *
 * Every packet sent should be returned by the server and received here.
 */
class ParameterClient : public Application
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  ParameterClient ();

  virtual ~ParameterClient ();

  /**
   * \brief set the remote address and port
   * \param ip remote IP address
   * \param port remote port
   */
  void SetRemote (Address ip, uint16_t port);
  /**
   * \brief set the remote address
   * \param addr remote address
   */
  void SetRemote (Address addr);

protected:
  virtual void DoDispose (void);

private:

  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void SendGradientUpdate();

  void ContinueGradientUpdate(Ptr<Socket> socket, uint32_t ready);

  void ReceiveParameterUpdate (Ptr<Socket> socket);

  void ScheduleGradientUpdate (Time dt);

  uint32_t recv_bytes_left;
  uint32_t send_bytes_left;

  std::vector<double> delay_distribution;

  uint32_t m_sent; //!< Counter for sent packets
  Ptr<Socket> m_socket; //!< Socket
  Address m_peerAddress; //!< Remote peer address
  uint16_t m_peerPort; //!< Remote peer port
  EventId m_sendEvent; //!< Event to send the next packet

  uint32_t m_mtu;
  uint32_t m_parameterUpdateSize;
  uint32_t m_gradientUpdateSize;
  uint32_t m_clientNum;
  uint32_t m_serverNum;
};

} // namespace ns3

#endif /* UDP_ECHO_CLIENT_H */

/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_TCP_PORT_H_
#define P2P_BASE_TCP_PORT_H_

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "api/candidate.h"
#include "api/field_trials_view.h"
#include "api/packet_socket_factory.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/transport/stun.h"
#include "p2p/base/connection.h"
#include "p2p/base/port.h"
#include "p2p/base/port_interface.h"
#include "p2p/base/stun_request.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/containers/flat_map.h"
#include "rtc_base/network.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/weak_ptr.h"

namespace cricket {

class TCPConnection;

// Communicates using a local TCP port.
//
// This class is designed to allow subclasses to take advantage of the
// connection management provided by this class.  A subclass should take of all
// packet sending and preparation, but when a packet is received, it should
// call this TCPPort::OnReadPacket (3 arg) to dispatch to a connection.
class TCPPort : public Port {
 public:
  static std::unique_ptr<TCPPort> Create(const PortParametersRef& args,
                                         uint16_t min_port,
                                         uint16_t max_port,
                                         bool allow_listen) {
    // Using `new` to access a non-public constructor.
    return absl::WrapUnique(
        new TCPPort(args, min_port, max_port, allow_listen));
  }
  [[deprecated("Pass arguments using PortParametersRef")]] static std::
      unique_ptr<TCPPort>
      Create(webrtc::TaskQueueBase* thread,
             webrtc::PacketSocketFactory* factory,
             const rtc::Network* network,
             uint16_t min_port,
             uint16_t max_port,
             absl::string_view username,
             absl::string_view password,
             bool allow_listen,
             const webrtc::FieldTrialsView* field_trials = nullptr) {
    return Create({.network_thread = thread,
                   .socket_factory = factory,
                   .network = network,
                   .ice_username_fragment = username,
                   .ice_password = password,
                   .field_trials = field_trials},
                  min_port, max_port, allow_listen);
  }
  ~TCPPort() override;

  Connection* CreateConnection(const webrtc::Candidate& address,
                               CandidateOrigin origin) override;

  void PrepareAddress() override;

  // Options apply to accepted sockets.
  // TODO(bugs.webrtc.org/13065): Apply also to outgoing and existing
  // connections.
  int GetOption(webrtc::Socket::Option opt, int* value) override;
  int SetOption(webrtc::Socket::Option opt, int value) override;
  int GetError() override;
  bool SupportsProtocol(absl::string_view protocol) const override;
  webrtc::ProtocolType GetProtocol() const override;

 protected:
  TCPPort(const PortParametersRef& args,
          uint16_t min_port,
          uint16_t max_port,
          bool allow_listen);

  // Handles sending using the local TCP socket.
  int SendTo(const void* data,
             size_t size,
             const webrtc::SocketAddress& addr,
             const rtc::PacketOptions& options,
             bool payload) override;

  // Accepts incoming TCP connection.
  void OnNewConnection(webrtc::AsyncListenSocket* socket,
                       webrtc::AsyncPacketSocket* new_socket);

 private:
  struct Incoming {
    webrtc::SocketAddress addr;
    webrtc::AsyncPacketSocket* socket;
  };

  void TryCreateServerSocket();

  webrtc::AsyncPacketSocket* GetIncoming(const webrtc::SocketAddress& addr,
                                         bool remove = false);

  // Receives packet signal from the local TCP Socket.
  void OnReadPacket(webrtc::AsyncPacketSocket* socket,
                    const rtc::ReceivedPacket& packet);

  void OnSentPacket(webrtc::AsyncPacketSocket* socket,
                    const rtc::SentPacket& sent_packet) override;

  void OnReadyToSend(webrtc::AsyncPacketSocket* socket);

  bool allow_listen_;
  std::unique_ptr<webrtc::AsyncListenSocket> listen_socket_;
  // Options to be applied to accepted sockets.
  // TODO(bugs.webrtc:13065): Configure connect/accept in the same way, but
  // currently, setting OPT_NODELAY for client sockets is done (unconditionally)
  // by BasicPacketSocketFactory::CreateClientTcpSocket.
  webrtc::flat_map<webrtc::Socket::Option, int> socket_options_;

  int error_;
  std::list<Incoming> incoming_;

  friend class TCPConnection;
};

class TCPConnection : public Connection, public sigslot::has_slots<> {
 public:
  // Connection is outgoing unless socket is specified
  TCPConnection(rtc::WeakPtr<Port> tcp_port,
                const webrtc::Candidate& candidate,
                webrtc::AsyncPacketSocket* socket = nullptr);
  ~TCPConnection() override;

  int Send(const void* data,
           size_t size,
           const rtc::PacketOptions& options) override;
  int GetError() override;

  webrtc::AsyncPacketSocket* socket() { return socket_.get(); }

  // Allow test cases to overwrite the default timeout period.
  int reconnection_timeout() const { return reconnection_timeout_; }
  void set_reconnection_timeout(int timeout_in_ms) {
    reconnection_timeout_ = timeout_in_ms;
  }

 protected:
  // Set waiting_for_stun_binding_complete_ to false to allow data packets in
  // addition to what Port::OnConnectionRequestResponse does.
  void OnConnectionRequestResponse(StunRequest* req,
                                   StunMessage* response) override;

 private:
  friend class TCPPort;  // For `MaybeReconnect()`.

  // Helper function to handle the case when Ping or Send fails with error
  // related to socket close.
  void MaybeReconnect();

  void CreateOutgoingTcpSocket() RTC_RUN_ON(network_thread());

  void ConnectSocketSignals(webrtc::AsyncPacketSocket* socket)
      RTC_RUN_ON(network_thread());

  void DisconnectSocketSignals(webrtc::AsyncPacketSocket* socket)
      RTC_RUN_ON(network_thread());

  void OnConnect(webrtc::AsyncPacketSocket* socket);
  void OnClose(webrtc::AsyncPacketSocket* socket, int error);
  void OnSentPacket(webrtc::AsyncPacketSocket* socket,
                    const rtc::SentPacket& sent_packet);
  void OnReadPacket(webrtc::AsyncPacketSocket* socket,
                    const rtc::ReceivedPacket& packet);
  void OnReadyToSend(webrtc::AsyncPacketSocket* socket);
  void OnDestroyed(Connection* c);

  TCPPort* tcp_port() {
    RTC_DCHECK_EQ(port()->GetProtocol(), webrtc::PROTO_TCP);
    return static_cast<TCPPort*>(port());
  }

  std::unique_ptr<webrtc::AsyncPacketSocket> socket_;
  int error_;
  const bool outgoing_;

  // Guard against multiple outgoing tcp connection during a reconnect.
  bool connection_pending_;

  // Guard against data packets sent when we reconnect a TCP connection. During
  // reconnecting, when a new tcp connection has being made, we can't send data
  // packets out until the STUN binding is completed (i.e. the write state is
  // set to WRITABLE again by Connection::OnConnectionRequestResponse). IPC
  // socket, when receiving data packets before that, will trigger OnError which
  // will terminate the newly created connection.
  bool pretending_to_be_writable_;

  // Allow test case to overwrite the default timeout period.
  int reconnection_timeout_;

  webrtc::ScopedTaskSafety network_safety_;
};

}  // namespace cricket

#endif  // P2P_BASE_TCP_PORT_H_

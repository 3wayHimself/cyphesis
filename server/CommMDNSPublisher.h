// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2004 Alistair Riddoch
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

// $Id$

#ifndef SERVER_COMM_MDNS_PUBLISHER_H
#define SERVER_COMM_MDNS_PUBLISHER_H

#include "CommSocket.h"
#include "Idle.h"

#include <set>

#include <stdint.h>

/// \brief Handle a socket used to communicate with the MDNS responder daemon.
/// \ingroup ServerSockets
class CommMDNSPublisher : public CommSocket, virtual public Idle {
  protected:
    // Avahi data
    ///
    struct AvahiClient * m_avahiClient;
    ///
    int m_avahiError;

    bool m_immediate;
    /// Check and expire timers
    void checkTimers(time_t);
  public:
    ///
    int m_avahiFd;
    ///
    struct AvahiWatch * m_avahiWatch;
    ///
    struct AvahiEntryGroup * m_group;
    ///
    std::set<struct AvahiTimeout *> m_avahiTimeouts;

    explicit CommMDNSPublisher(CommServer & svr);

    virtual ~CommMDNSPublisher();

    void setImmediate() {
        m_immediate = true;
    }

    int setup();
    void setup_service(struct AvahiClient * );

    virtual void idle(time_t);

    int getFd() const;
    bool isOpen() const;
    bool eof();
    int read();
    void dispatch();
};

#endif // SERVER_COMM_MDNS_PUBLISHER_H

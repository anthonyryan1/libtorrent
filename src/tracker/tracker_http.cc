// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#include <iomanip>
#include <sstream>

#include "torrent/exceptions.h"
#include "torrent/http.h"
#include "tracker_http.h"
#include "tracker_info.h"

// STOPPED is only sent once, if the connections fails then we stop trying.
// START has a retry of [very short]
// COMPLETED/NONE has a retry of [short]

// START success leads to NONE and [interval]

namespace torrent {

TrackerHttp::TrackerHttp(TrackerInfo* info, const std::string& url) :
  TrackerBase(info, url),

  m_get(Http::call_factory()),
  m_data(NULL) {

  m_get->set_user_agent(PACKAGE "/" VERSION);

  m_get->signal_done().connect(sigc::mem_fun(*this, &TrackerHttp::receive_done));
  m_get->signal_failed().connect(sigc::mem_fun(*this, &TrackerHttp::receive_failed));
}

TrackerHttp::~TrackerHttp() {
  delete m_get;
  delete m_data;
}

bool
TrackerHttp::is_busy() const {
  return m_data != NULL;
}

void
TrackerHttp::send_state(TrackerInfo::State state, uint64_t down, uint64_t up, uint64_t left) {
  close();

  if (m_info == NULL || m_info->get_me() == NULL)
    throw internal_error("TrackerHttp::send_state(...) does not have a valid m_info or m_me");

  if (m_info->get_me()->get_id().length() != 20 ||
      m_info->get_me()->get_port() == 0 ||
      m_info->get_hash().length() != 20)
    throw internal_error("Send state with TrackerHttp with bad hash, id or port");

  std::stringstream s;

  s << m_url << "?info_hash=";
  escape_string(m_info->get_hash(), s);

  s << "&peer_id=";
  escape_string(m_info->get_me()->get_id(), s);

  s << "&key=" << std::hex << std::setw(8) << std::setfill('0') << m_info->get_key() << std::dec;

  if (!m_trackerId.empty()) {
    s << "&trackerid=";
    escape_string(m_trackerId, s);
  }

  if (!m_info->get_me()->get_socket_address().is_address_any())
    s << "&ip=" << m_info->get_me()->get_address();

  if (m_info->get_compact())
    s << "&compact=1";

  if (m_info->get_numwant() >= 0)
    s << "&numwant=" << m_info->get_numwant();

  s << "&port=" << m_info->get_me()->get_port()
    << "&uploaded=" << up
    << "&downloaded=" << down
    << "&left=" << left;

  switch(state) {
  case TrackerInfo::STARTED:
    s << "&event=started";
    break;
  case TrackerInfo::STOPPED:
    s << "&event=stopped";
    break;
  case TrackerInfo::COMPLETED:
    s << "&event=completed";
    break;
  default:
    break;
  }

  m_data = new std::stringstream();

  m_get->set_url(s.str());
  m_get->set_stream(m_data);

  m_get->start();
}

void
TrackerHttp::close() {
  if (m_data == NULL)
    return;

  m_get->close();
  m_get->set_stream(NULL);

  delete m_data;
  m_data = NULL;
}

TrackerHttp::Type
TrackerHttp::get_type() const {
  return TRACKER_HTTP;
}

void
TrackerHttp::escape_string(const std::string& src, std::ostream& stream) {
  // TODO: Correct would be to save the state.
  stream << std::hex << std::uppercase;

  for (std::string::const_iterator itr = src.begin(); itr != src.end(); ++itr)
    if ((*itr >= 'A' && *itr <= 'Z') ||
	(*itr >= 'a' && *itr <= 'z') ||
	(*itr >= '0' && *itr <= '9') ||
	*itr == '-')
      stream << *itr;
    else
      stream << '%' << ((unsigned char)*itr >> 4) << ((unsigned char)*itr & 0xf);

  stream << std::dec << std::nouppercase;
}

void
TrackerHttp::receive_done() {
  if (m_data == NULL)
    throw internal_error("TrackerHttp::receive_done() called on an invalid object");

  Bencode b;
  *m_data >> b;

  if (m_data->fail())
    return receive_failed("Could not parse bencoded data");

  if (!b.is_map())
    return receive_failed("Root not a bencoded map");

  if (b.has_key("failure reason"))
    return receive_failed("Failure reason \"" +
			 (b["failure reason"].is_string() ?
			  b["failure reason"].as_string() :
			  std::string("failure reason not a string"))
			 + "\"");

  if (b.has_key("interval") && b["interval"].is_value())
    m_slotSetInterval(b["interval"].as_value());
  
  if (b.has_key("min interval") && b["min interval"].is_value())
    m_slotSetMinInterval(b["min interval"].as_value());

  if (b.has_key("tracker id") && b["tracker id"].is_string())
    m_trackerId = b["tracker id"].as_string();

  PeerList plist;

  try {
    if (b["peers"].is_string())
      parse_peers_compact(plist, b["peers"].as_string());
    else
      parse_peers_normal(plist, b["peers"].as_list());

  } catch (bencode_error& e) {
    return receive_failed(e.what());
  }

  close();
  m_slotSuccess(&plist);
}

void
TrackerHttp::receive_failed(std::string msg) {
  // Does the order matter?
  close();
  m_slotFailed(msg);
}

PeerInfo
TrackerHttp::parse_peer(const Bencode& b) {
  PeerInfo p;
	
  if (!b.is_map())
    return p;

  for (Bencode::Map::const_iterator itr = b.as_map().begin(); itr != b.as_map().end(); ++itr) {
    if (itr->first == "ip" &&
	itr->second.is_string()) {
      p.get_socket_address().set_address(itr->second.as_string());
	    
    } else if (itr->first == "peer id" &&
	       itr->second.is_string()) {
      p.set_id(itr->second.as_string());
	    
    } else if (itr->first == "port" &&
	       itr->second.is_value()) {
      p.get_socket_address().set_port(itr->second.as_value());
    }
  }
	
  return p;
}

void
TrackerHttp::parse_peers_normal(PeerList& l, const Bencode::List& b) {
  for (Bencode::List::const_iterator itr = b.begin(); itr != b.end(); ++itr) {
    PeerInfo p = parse_peer(*itr);
	  
    if (p.get_socket_address().is_valid())
      l.push_back(p);
  }
}  

void
TrackerHttp::parse_peers_compact(PeerList& l, const std::string& s) {
  for (std::string::const_iterator itr = s.begin(); itr + 6 <= s.end();) {
    SocketAddress sa;

    std::copy(itr, itr + sa.sizeof_address(), sa.begin_address());
    itr += sa.sizeof_address();
    std::copy(itr, itr + sa.sizeof_port(), sa.begin_port());
    itr += sa.sizeof_port();

    l.push_back(PeerInfo("", sa));
  }
}

}
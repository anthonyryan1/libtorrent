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

#include <stdlib.h>
#include <sigc++/bind.h>

#include "data/chunk_list.h"
#include "data/content.h"
#include "data/hash_queue.h"
#include "data/file_manager.h"
#include "data/file_meta.h"
#include "data/file_stat.h"
#include "protocol/handshake_manager.h"
#include "protocol/peer_connection_base.h"
#include "torrent/exceptions.h"
#include "tracker/tracker_manager.h"

#include "download_wrapper.h"

namespace torrent {

DownloadWrapper::~DownloadWrapper() {
  if (m_main.is_active())
    stop();

  if (m_main.is_open())
    close();
}

void
DownloadWrapper::initialize(const std::string& hash, const std::string& id, const SocketAddress& sa) {
  m_main.setup_net();
  m_main.setup_delegator();
  m_main.setup_tracker();

  m_main.tracker_manager()->tracker_info()->set_hash(hash);
  m_main.tracker_manager()->tracker_info()->set_local_id(id);
  m_main.tracker_manager()->tracker_info()->set_local_address(sa);
  m_main.tracker_manager()->tracker_info()->set_key(random());

  m_main.chunk_list()->set_max_queue_size((32 << 20) / m_main.content()->chunk_size());

  // Info hash must be calculate from here on.
  m_hash = std::auto_ptr<HashTorrent>(new HashTorrent(get_hash(), m_main.chunk_list()));

  // Connect various signals and slots.
  m_hash->slot_chunk_done(sigc::mem_fun(m_main, &DownloadMain::receive_hash_done));
  m_hash->signal_torrent().connect(sigc::mem_fun(m_main, &DownloadMain::receive_initial_hash));
}

void
DownloadWrapper::hash_resume_load() {
  if (!m_main.is_open() || m_main.is_active() || m_main.is_checked())
    throw client_error("DownloadWrapper::resume_load() called with wrong state");

  if (!m_bencode.has_key("libtorrent resume"))
    return;

  try {
    Bencode& resume  = m_bencode["libtorrent resume"];

    // Load peer addresses.
    if (resume.has_key("peers") && resume["peers"].is_string()) {
      const std::string& peers = resume["peers"].as_string();

      std::list<SocketAddress> l(reinterpret_cast<const SocketAddressCompact*>(peers.c_str()),
				 reinterpret_cast<const SocketAddressCompact*>(peers.c_str() + peers.size() - peers.size() % sizeof(SocketAddressCompact)));

      l.sort();
      m_main.available_list()->insert(&l);
    }

    Bencode& files = resume["files"];

    if (resume["bitfield"].as_string().size() != m_main.content()->get_bitfield().size_bytes() ||
	files.as_list().size() != m_main.content()->entry_list()->get_files_size())
      return;

    // Clear the hash checking ranges, and add the files ranges we must check.
    m_hash->get_ranges().clear();

    std::memcpy(m_main.content()->get_bitfield().begin(), resume["bitfield"].as_string().c_str(), m_main.content()->get_bitfield().size_bytes());

    Bencode::List::iterator bItr = files.as_list().begin();
    EntryList::iterator sItr = m_main.content()->entry_list()->begin();

    // Check the validity of each file, add to the m_hash's ranges if invalid.
    while (sItr != m_main.content()->entry_list()->end()) {
      FileStat fs;

      // Check that the size and modified stamp matches. If not, then
      // add to the hashes to check.

      if (fs.update(sItr->file_meta()->get_path()) ||
	  sItr->get_size() != fs.get_size() ||
	  !bItr->has_key("mtime") ||
	  !(*bItr)["mtime"].is_value() ||
	  (*bItr)["mtime"].as_value() != fs.get_mtime())
	m_hash->get_ranges().insert(sItr->get_range().first, sItr->get_range().second);

      // Update the priority from the fast resume data.
      if (bItr->has_key("priority") &&
	  (*bItr)["priority"].is_value() &&
	  (*bItr)["priority"].as_value() >= 0 &&
	  (*bItr)["priority"].as_value() < 3)
	sItr->set_priority((*bItr)["priority"].as_value());

      ++sItr;
      ++bItr;
    }  

  } catch (bencode_error e) {
    m_hash->get_ranges().insert(0, m_main.content()->chunk_total());
  }

  // Clear bits in invalid regions which will be checked by m_hash.
  for (Ranges::iterator itr = m_hash->get_ranges().begin(); itr != m_hash->get_ranges().end(); ++itr)
    m_main.content()->get_bitfield().set(itr->first, itr->second, false);

  m_main.content()->update_done();
}

// Break this function up into several smaller functions to make it
// easier to read.
void
DownloadWrapper::hash_resume_save() {
  if (!m_main.is_open() || m_main.is_active())
    throw client_error("DownloadWrapper::resume_save() called with wrong state");

  if (!m_main.is_checked())
    // We don't remove the old hash data since it might still be
    // valid, just that the client didn't finish the check this time.
    return;

  // Clear the resume data since if the syncing fails we propably don't
  // want the old resume data.
  Bencode& resume = m_bencode.insert_key("libtorrent resume", Bencode(Bencode::TYPE_MAP));

  // We're guaranteed that file modification time is correctly updated
  // after this. Though won't help if the files have been delete while
  // we had them open.
  //m_main.content()->entry_list()->sync_all();

  // We sync all chunks in DownloadMain::stop(), so we are guaranteed
  // that it has been called when we arrive here.

  resume.insert_key("bitfield", std::string((char*)m_main.content()->get_bitfield().begin(), m_main.content()->get_bitfield().size_bytes()));

  Bencode::List& l = resume.insert_key("files", Bencode(Bencode::TYPE_LIST)).as_list();

  EntryList::iterator sItr = m_main.content()->entry_list()->begin();
  
  // Check the validity of each file, add to the m_hash's ranges if invalid.
  while (sItr != m_main.content()->entry_list()->end()) {
    Bencode& b = *l.insert(l.end(), Bencode(Bencode::TYPE_MAP));

    FileStat fs;

    if (fs.update(sItr->file_meta()->get_path())) {
      l.clear();
      break;
    }

    b.insert_key("mtime", fs.get_mtime());
    b.insert_key("priority", (int)sItr->get_priority());

    ++sItr;
  }

  // Save the available peer list. Since this function is called when
  // the download is stopped, we know that all the previously
  // connected peers have been copied to the available list.
  std::string peers;
  peers.reserve(m_main.available_list()->size() * sizeof(SocketAddressCompact));
  
  for (AvailableList::const_iterator
	 itr = m_main.available_list()->begin(),
	 last = m_main.available_list()->end();
       itr != last; ++itr)
    peers.append(itr->get_address_compact().c_str(), sizeof(SocketAddressCompact));

  resume.insert_key("peers", peers);
}

void
DownloadWrapper::open() {
  if (m_main.is_open())
    return;

  m_main.open();
  m_hash->get_ranges().insert(0, m_main.content()->chunk_total());
}

void
DownloadWrapper::close() {
  // Stop the hashing first as we need to make sure all chunks are
  // released when DownloadMain::close() is called.
  m_hash->clear();

  m_main.close();
}

void
DownloadWrapper::stop() {
  m_main.stop();
}

bool
DownloadWrapper::is_stopped() const {
  return !m_main.tracker_manager()->is_active();
}

const std::string&
DownloadWrapper::get_hash() const {
  return m_main.tracker_manager()->tracker_info()->get_hash();
}

const std::string&
DownloadWrapper::get_local_id() const {
  return m_main.tracker_manager()->tracker_info()->get_local_id();
}

SocketAddress&
DownloadWrapper::get_local_address() {
  return m_main.tracker_manager()->tracker_info()->get_local_address();
}

void
DownloadWrapper::set_file_manager(FileManager* f) {
  m_main.content()->entry_list()->slot_insert_filemeta(sigc::mem_fun(*f, &FileManager::insert));
  m_main.content()->entry_list()->slot_erase_filemeta(sigc::mem_fun(*f, &FileManager::erase));
}

void
DownloadWrapper::set_handshake_manager(HandshakeManager* h) {
  m_main.slot_count_handshakes(sigc::bind(sigc::mem_fun(*h, &HandshakeManager::get_size_hash), get_hash()));
  m_main.slot_start_handshake(sigc::bind(sigc::mem_fun(*h, &HandshakeManager::add_outgoing), get_hash(), get_local_id()));
}

void
DownloadWrapper::set_hash_queue(HashQueue* h) {
  m_hash->set_queue(h);

  m_main.slot_hash_check_add(sigc::bind(sigc::mem_fun(*h, &HashQueue::push_back),
					sigc::mem_fun(m_main, &DownloadMain::receive_hash_done),
					get_hash()));
}

void
DownloadWrapper::receive_keepalive() {
  for (ConnectionList::iterator itr = m_main.connection_list()->begin(); itr != m_main.connection_list()->end(); )
    if (!(*itr)->receive_keepalive())
      itr = m_main.connection_list()->erase(itr);
    else
      itr++;
}

}
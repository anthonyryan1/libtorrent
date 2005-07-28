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

#include <memory>

#include "torrent/exceptions.h"
#include "content.h"
#include "data/file_meta.h"
#include "data/file_stat.h"

namespace torrent {

Content::Content() :
  m_size(0),
  m_completed(0),
  m_rootDir(".") {

  m_delayDownloadDone.set_slot(m_signalDownloadDone.make_slot());
  m_delayDownloadDone.set_iterator(taskScheduler.end());
}

void
Content::add_file(const Path& path, uint64_t size) {
  if (is_open())
    throw internal_error("Tried to add file to Content that is open");

  m_files.push_back(ContentFile(path, size, make_index_range(m_size, size)));
  m_size += size;
}

void
Content::set_complete_hash(const std::string& hash) {
  if (is_open())
    throw internal_error("Tried to set complete hash on Content that is open");

  m_hash = hash;
}

void
Content::set_root_dir(const std::string& dir) {
  if (is_open())
    throw internal_error("Tried to set root directory on Content that is open");

  m_rootDir = dir;
}

std::string
Content::get_hash(unsigned int index) {
  if (!is_open())
    throw internal_error("Tried to get chunk hash from Content that is not open");

  if (index >= m_storage.get_chunk_total())
    throw internal_error("Tried to get chunk hash from Content that is out of range");

  return m_hash.substr(index * 20, 20);
}

uint32_t
Content::get_chunksize(uint32_t index) const {
  if (m_storage.get_chunk_size() == 0 || index >= m_storage.get_chunk_total())
    throw internal_error("Content::get_chunksize(...) called but we borked");

  if (index + 1 != m_storage.get_chunk_total() || m_size % m_storage.get_chunk_size() == 0)
    return m_storage.get_chunk_size();
  else
    return m_size % m_storage.get_chunk_size();
}

uint64_t
Content::get_bytes_completed() {
  if (!is_open())
    return 0;

  uint64_t cs = m_storage.get_chunk_size();

  if (!m_bitfield[m_storage.get_chunk_total() - 1] || m_size % cs == 0)
    // The last chunk is not done, or the last chunk is the same size as the others.
    return m_completed * cs;

  else
    return (m_completed - 1) * cs + m_size % cs;
}

bool
Content::is_correct_size() {
  if (!is_open())
    return false;

  if (m_files.size() != m_storage.get_consolidator().get_files_size())
    throw internal_error("Content::is_correct_size called on an open object with mismatching FileList and Storage::FileList sizes");

  FileList::iterator fItr = m_files.begin();
  Storage::FileList::iterator sItr = m_storage.get_consolidator().begin();
  
  while (fItr != m_files.end()) {
    // TODO: Throw or return false?
    if (!sItr->get_meta()->prepare(MemoryChunk::prot_read))
      return false;

    if (fItr->get_size() != FileStat(sItr->get_meta()->get_file().fd()).get_size())
      return false;

    ++fItr;
    ++sItr;
  }

  return true;
}

bool
Content::is_valid_piece(const Piece& p) const {
  return
    (uint32_t)p.get_index() < m_storage.get_chunk_total() &&

    p.get_length() != 0 &&
    p.get_length() < (1 << 17) &&

    // Make sure offset does not overflow 32 bits.
    p.get_offset() < (1 << 30) &&
    p.get_offset() + p.get_length() <= get_chunksize(p.get_index());
}

void
Content::open(bool wr) {
  close();

  // Make sure root directory exists, Can't make recursively, so the client must
  // make sure the parent dir of 'm_rootDir' exists.
  Path::mkdir(m_rootDir);
  Path lastPath;

  for (FileList::iterator itr = m_files.begin(); itr != m_files.end(); ++itr) {
    std::auto_ptr<FileMeta> f(new FileMeta);

    try {
      open_file(f.get(), itr->get_path(), lastPath);
      m_slotOpenedFile(f.get());

    } catch (base_error& e) {
      f->get_file().close();
      m_storage.clear();

      throw;
    }

    m_storage.get_consolidator().push_back(f.release(), itr->get_size());

    lastPath = itr->get_path();
  }

  m_bitfield = BitField(m_storage.get_chunk_total());

  // Update anchor count in m_storage.
  m_storage.set_chunk_size(m_storage.get_chunk_size());

  if (m_hash.size() / 20 != m_storage.get_chunk_total())
    throw internal_error("Content::open(...): Chunk count does not match hash count");

  if (m_size != m_storage.get_bytes_size())
    throw internal_error("Content::open(...): m_size != m_storage.get_size()");
}

void
Content::close() {
  m_storage.clear();

  m_completed = 0;
  m_bitfield = BitField();
  taskScheduler.erase(&m_delayDownloadDone);

  std::for_each(m_files.begin(), m_files.end(), std::mem_fun_ref(&ContentFile::reset));
}

void
Content::resize() {
  if (!m_storage.get_consolidator().resize_files())
    throw storage_error("Could not resize files");
}

void
Content::mark_done(uint32_t index) {
  if (index >= m_storage.get_chunk_total())
    throw internal_error("Content::mark_done received index out of range");
    
  if (m_bitfield[index])
    throw internal_error("Content::mark_done received index that has already been marked as done");
  
  if (m_completed >= m_storage.get_chunk_total())
    throw internal_error("Content::mark_done called but m_completed >= m_storage.get_chunk_total()");

  m_bitfield.set(index, true);
  m_completed++;

  mark_done_file(m_files.begin(), index);

  // We delay emitting the signal to allow the delegator to clean
  // up. If we do a straight call it would cause problems for
  // clients that wish to close and reopen the torrent, as
  // HashQueue, Delegator etc shouldn't be cleaned up at this point.
  if (m_completed == m_storage.get_chunk_total() &&
      !m_delayDownloadDone.get_slot().blocked() &&
      !taskScheduler.is_scheduled(&m_delayDownloadDone))
    taskScheduler.insert(&m_delayDownloadDone, Timer::cache());
}

// Recalculate done pieces, this function clears the padding bits on the
// bitfield.
void
Content::update_done() {
  m_bitfield.cleanup();
  m_completed = m_bitfield.count();

  FileList::iterator itr = m_files.begin();

  for (BitField::size_t i = 0; i < m_bitfield.size_bits(); ++i)
    if (m_bitfield[i])
      itr = mark_done_file(itr, i);
}

void
Content::open_file(FileMeta* f, Path& p, Path& lastPath) {
  if (p.empty())
    throw internal_error("Tried to open file with empty path");

  Path::mkdir(m_rootDir, p.begin(), --p.end(),
	      lastPath.begin(), lastPath.end());

  if (!f->get_file().open(m_rootDir + p.as_string(), MemoryChunk::prot_read | MemoryChunk::prot_write, File::o_create) &&
      !f->get_file().open(m_rootDir + p.as_string(), MemoryChunk::prot_read, File::o_create))
    throw storage_error("Could not open file \"" + m_rootDir + p.as_string() + "\"");

  f->set_path(m_rootDir + p.as_string());
}

}
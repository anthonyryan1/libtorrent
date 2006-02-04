// rTorrent - BitTorrent client
// Copyright (C) 2005-2006, Jari Sundell
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

#include <cstring>
#include <sstream>
#include <iomanip>
#include <torrent/exceptions.h>
#include <torrent/rate.h>
#include <torrent/tracker.h>

#include "core/download.h"
#include <rak/timer.h>

#include "utils.h"

namespace display {

inline char*
print_buffer(char* first, char* last, const char* format) {
  if (first >= last)
    return first;

  int s = snprintf(first, last - first, format);

  if (s < 0)
    return first;
  else
    return std::min(first + s, last);
}

template <typename Arg1>
inline char*
print_buffer(char* first, char* last, const char* format, const Arg1& arg1) {
  if (first >= last)
    return first;

  int s = snprintf(first, last - first, format, arg1);

  if (s < 0)
    return first;
  else
    return std::min(first + s, last);
}

template <typename Arg1, typename Arg2>
inline char*
print_buffer(char* first, char* last, const char* format, const Arg1& arg1, const Arg2& arg2) {
  if (first >= last)
    return first;

  int s = snprintf(first, last - first, format, arg1, arg2);

  if (s < 0)
    return first;
  else
    return std::min(first + s, last);
}

template <typename Arg1, typename Arg2, typename Arg3>
inline char*
print_buffer(char* first, char* last, const char* format, const Arg1& arg1, const Arg2& arg2, const Arg3& arg3) {
  if (first >= last)
    return first;

  int s = snprintf(first, last - first, format, arg1, arg2, arg3);

  if (s < 0)
    return first;
  else
    return std::min(first + s, last);
}

char*
print_string(char* first, char* last, char* str) {
  // We don't have any nice simple functions for copying strings that
  // return the end address.
  while (first != last && *str != '\0')
    *(first++) = *(str++);

  return first;
}

char*
print_hhmmss(char* first, char* last, time_t t) {
  std::tm *u = std::localtime(&t);
  
  if (u == NULL)
    //return "inv_time";
    throw torrent::internal_error("print_hhmmss(...) failed.");

  return print_buffer(first, last, "%2u:%02u:%02u", u->tm_hour, u->tm_min, u->tm_sec);
}

char*
print_ddhhmm(char* first, char* last, time_t t) {
  if (t / (24 * 3600) < 100)
    return print_buffer(first, last, "%2i:%02i:%02i", (int)t / (24 * 3600), ((int)t / 3600) % 24, ((int)t / 60) % 60);
  else
    return print_buffer(first, last, "--:--:--");
}

char*
print_ddmmyyyy(char* first, char* last, time_t t) {
  std::tm *u = std::gmtime(&t);
  
  if (u == NULL)
    //return "inv_time";
    throw torrent::internal_error("print_ddmmyyyy(...) failed.");

  return print_buffer(first, last, "%02u/%02u/%04u", u->tm_mday, (u->tm_mon + 1), (1900 + u->tm_year));
}

char*
print_download_title(char* first, char* last, core::Download* d) {
  return print_buffer(first, last, "%s", d->get_download().name().c_str());
}

char*
print_download_info(char* first, char* last, core::Download* d) {
  first = print_buffer(first, last, "Torrent: ");

  if (!d->get_download().is_open())
    first = print_buffer(first, last, "closed            ");

  else if (d->is_done())
    first = print_buffer(first, last, "done %10.1f MB", (double)d->get_download().bytes_total() / (double)(1 << 20));
  else
    first = print_buffer(first, last, "%6.1f / %6.1f MB",
		       (double)d->get_download().bytes_done() / (double)(1 << 20),
		       (double)d->get_download().bytes_total() / (double)(1 << 20));
  
  first = print_buffer(first, last, " Rate: %5.1f / %5.1f KB Uploaded: %7.1f MB",
		     (double)d->get_download().up_rate()->rate() / (1 << 10),
		     (double)d->get_download().down_rate()->rate() / (1 << 10),
		     (double)d->get_download().up_rate()->total() / (1 << 20));

  if (d->get_download().is_active() && !d->is_done()) {
    first = print_buffer(first, last, " ");
    first = print_download_percentage_done(first, last, d);

    first = print_buffer(first, last, " ");
    first = print_download_time_left(first, last, d);
  } else {
    first = print_buffer(first, last, "               ");
  }

  if (d->priority() != 2)
    first = print_buffer(first, last, " [%s]", core::Download::priority_to_string(d->priority()));

  if (first > last)
    throw torrent::internal_error("print_download_info(...) wrote past end of the buffer.");

  return first;
}

char*
print_download_status(char* first, char* last, core::Download* d) {
  if (!d->get_download().is_active())
    first = print_buffer(first, last, "Inactive: ");

  if (d->get_download().is_hash_checking())
    first = print_buffer(first, last, "Checking hash [%2i%%]",
		       (d->get_download().chunks_hashed() * 100) / d->get_download().chunks_total());

  else if (d->get_download().is_tracker_busy() &&
	   d->get_download().tracker_focus() < d->get_download().size_trackers())
    first = print_buffer(first, last, "Tracker[%i:%i]: Connecting to %s",
		       d->get_download().tracker(d->get_download().tracker_focus()).group(),
		       d->get_download().tracker_focus(),
		       d->get_download().tracker(d->get_download().tracker_focus()).url().c_str());

  else if (!d->get_message().empty())
    first = print_buffer(first, last, "%s", d->get_message().c_str());

  else
    *first = '\0';

  if (first > last)
    throw torrent::internal_error("print_download_status(...) wrote past end of the buffer.");

  return first;
}

char*
print_download_time_left(char* first, char* last, core::Download* d) {
  uint32_t rate = d->get_download().down_rate()->rate();

  if (rate < 512)
    return print_buffer(first, last, "--:--:--");
  
  time_t remaining = (d->get_download().bytes_total() - d->get_download().bytes_done()) / (rate & ~(uint32_t)(512 - 1));

  return print_ddhhmm(first, last, remaining);
}

char*
print_download_percentage_done(char* first, char* last, core::Download* d) {
  if (!d->is_open() || d->is_done())
    //return print_buffer(first, last, "[--%%]");
    return print_buffer(first, last, "     ");
  else
    return print_buffer(first, last, "[%2u%%]", (d->get_download().chunks_done() * 100) / d->get_download().chunks_total());
}

char*
print_status_info(char* first, char* last) {
  if (torrent::up_throttle() == 0)
    first = print_buffer(first, last, "[Throttle off");
  else
    first = print_buffer(first, last, "[Throttle %3i", torrent::up_throttle() / 1024);

  if (torrent::down_throttle() == 0)
    first = print_buffer(first, last, "/off KB]");
  else
    first = print_buffer(first, last, "/%3i KB]", torrent::down_throttle() / 1024);
  
  first = print_buffer(first, last, " [Rate %5.1f/%5.1f KB]",
		       (double)torrent::up_rate()->rate() / 1024.0,
		       (double)torrent::down_rate()->rate() / 1024.0);

  first = print_buffer(first, last, " [Listen %s:%u]",
		       torrent::local_address().c_str(),
		       (unsigned int)torrent::listen_port());
  
  if (first > last)
    throw torrent::internal_error("print_status_info(...) wrote past end of the buffer.");

  std::string bindAddress = torrent::bind_address();

  if (!bindAddress.empty())
    first = print_buffer(first, last, " [Bind %s]", bindAddress.c_str());

  return first;
}

char*
print_status_extra(char* first, char* last, Control* c) {
  first = print_buffer(first, last, " [U %i/%i]",
		       torrent::currently_unchoked(),
		       torrent::max_unchoked());

  first = print_buffer(first, last, " [S %i/%i/%i]",
		       torrent::total_handshakes(),
		       torrent::open_sockets(),
		       torrent::max_open_sockets());
		       
  first = print_buffer(first, last, " [F %i/%i]",
		       torrent::open_files(),
		       torrent::max_open_files());

  return first;
}

}
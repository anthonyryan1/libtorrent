// rTorrent - BitTorrent client
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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef RTORRENT_INPUT_MANAGER_H
#define RTORRENT_INPUT_MANAGER_H

#include <list>

namespace input {

class Bindings;
class TextInput;

class Manager : private std::list<Bindings*> {
public:
  typedef std::list<Bindings*> Base;

  using Base::iterator;
  using Base::const_iterator;
  using Base::reverse_iterator;
  using Base::const_reverse_iterator;

  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;

  using Base::push_back;
  using Base::push_front;

  Manager() : m_textInput(NULL) {}

  void erase(Bindings* b);

  void pressed(int key);

  void set_text_input(TextInput* input = NULL) { m_textInput = input; }

  // Slot for unreacted keys.

private:
  TextInput* m_textInput;
};

}

#endif
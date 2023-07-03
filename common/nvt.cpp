/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2023, WWIV Software Services               */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
#include "common/remote_io.h"
#include "common/nvt.h"
#include "core/stl.h"
#include "fmt/format.h"
#include <deque>
#include <optional>
#include <string>

namespace wwiv::common {

template <typename T>
static std::optional<T> take(std::deque<T>& q) {
  if (q.empty()) {
    return std::nullopt;
  }
  auto val = q.front();
  q.pop_front();
  return val;
}

template <typename T>
static std::optional<Nvt::Command> take_command(std::deque<T>& q) {
  if (q.empty()) {
    return std::nullopt;
  }
  auto v = static_cast<unsigned char>(q.front());
  auto val = static_cast<Nvt::Command>(v);
  q.pop_front();
  return val;
}

template <typename T>
static std::optional<Nvt::Command> peek_command(std::deque<T>& q) {
  if (q.empty()) {
    return std::nullopt;
  }
  auto v = static_cast<unsigned char>(q.front());
  auto val = static_cast<Nvt::Command>(v);
  return val;
}

template <typename T>
static std::optional<Nvt::Option> take_option(std::deque<T>& q) {
  if (q.empty()) {
    return std::nullopt;
  }
  auto v = static_cast<unsigned char>(q.front());
  auto val = static_cast<Nvt::Option>(v);
  q.pop_front();
  return val;
}

template<typename T>
static char to_char(T cmd) {
  auto ci = static_cast<int>(cmd);
  return static_cast<char>(ci & 0xff);
}

std::string Nvt::create_command(Command cmd, Option option) {
  std::string r;
  r.push_back(to_char(Nvt::Command::IAC));
  r.push_back(to_char(cmd));
  r.push_back(to_char(option));
  return r;
}

std::string Nvt::create_sub(Option option) {
  std::string r;
  r.push_back(to_char(Nvt::Command::IAC));
  r.push_back(to_char(Nvt::Command::SB));
  r.push_back(to_char(option));
  r.push_back(0x01);
  r.push_back(to_char(Nvt::Command::IAC));
  r.push_back(to_char(Nvt::Command::SE));
  return r;
}

std::string Nvt::create_sub(Option option, const std::string_view arg) {
  // TODO add arg
  std::string r;
  r.reserve(arg.size() + 7);
  r.push_back(to_char(Nvt::Command::IAC));
  r.push_back(to_char(Nvt::Command::SB));
  r.push_back(to_char(option));
  r.push_back(0x00);
  for (auto c : arg) {
    r.push_back(c);
  }
  r.push_back(to_char(Nvt::Command::IAC));
  r.push_back(to_char(Nvt::Command::SE));
  return r;
}

// Creates and sends a command, also updating internal mapping of value in options.
bool Nvt::send_command(Command cmd, Option option, RemoteIO* out) {
  auto c = create_command(cmd, option);
  process_command(cmd, option, "");
  LOG(INFO) << "Sending IAC Command: " << to_string(cmd) << "; for option: " << to_string(option);
  return out->write(c.data(), c.size(), true) == c.size();
}

static std::optional<Nvt::Command> ack(Nvt::Command cmd) {
  switch (cmd) {
  case Nvt::Command::DO:
  case Nvt::Command::DONT:
    return Nvt::Command::WILL;
  case Nvt::Command::WILL:
  case Nvt::Command::WONT:
    return Nvt::Command::DO;
  default:
    return std::nullopt;
  }
}

static std::optional<Nvt::Command> nak(Nvt::Command cmd) {
  switch (cmd) {
  case Nvt::Command::DO:
  case Nvt::Command::DONT:
    return Nvt::Command::WONT;
  case Nvt::Command::WILL:
  case Nvt::Command::WONT:
    return Nvt::Command::DONT;
  default:
    return std::nullopt;
  }
}

// Creates and sends a command, also updating internal mapping of value in options.
bool Nvt::send_ack(Command cmd, Option opt, RemoteIO* out) {
  if (auto nakcmd = ack(cmd)) {
    // respond that we agree with the nak.
    return send_command(nakcmd.value(), opt, out);
  }
  LOG(WARNING) << "Do not know how to ACK: " << to_string(cmd);
  return false;
}
// Creates and sends a command, also updating internal mapping of value in options.
bool Nvt::send_nak(Command cmd, Option opt, RemoteIO* out) {
  if (auto nakcmd = nak(cmd)) {
    // respond that we agree with the nak.
    return send_command(nakcmd.value(), opt, out);
  }
  LOG(WARNING) << "Do not know how to NAK: " << to_string(cmd);
  return false;
}

bool Nvt::send_negotiate(Option option, RemoteIO* out) {
  auto c = create_sub(option);
  if (const auto it = options.find(option); it != std::end(options)) {
    it->second.value.clear();
  }
  return out->write(c.data(), c.size(), true) == c.size();
}

bool Nvt::send_negotiate(Option option, const std::string_view arg, RemoteIO* out) {
  auto c = create_sub(option, arg);
  process_option_value(option, arg);
  return out->write(c.data(), c.size(), true) == c.size();
}


static std::string get_sb_value(std::deque<char>& d) {
  std::string arg;
  while (auto c = peek_command(d)) {
    if (c == Nvt::Command::IAC) {
      break;
    }
    arg.push_back(take(d).value());
  }
  return arg;
}


// Subnegotiation
// IAC,SB,[WE ARE HERE] <option code number>,1,IAC,SE
// or IAC, SB, [WE ARE HERE] <option code>, 0, <value>, IAC, SE
std::optional<Nvt::Sequence> Nvt::parse_sb(std::deque<char>& d) {
  Nvt::Sequence r(Nvt::Command::SB);
  if (auto opt = take_option(d)) {
    r.opt = opt.value();
    if (auto v = take(d); *v == 0) {
      // or IAC, SB, <option code>, 0, [WE ARE HERE] <value>, IAC, SE
      r.arg = get_sb_value(d);
      if (auto c = take_command(d); !c || *c != Nvt::Command::IAC) {
        return std::nullopt;
      }
      if (auto c = take_command(d); !c || *c != Nvt::Command::SE) {
        return std::nullopt;
      }
      // Update database for this option value;
      process_option_value(r.opt, r.arg);
      r.length = r.arg.length() + 5;
      return r;
    }
  }
  return std::nullopt;
}

std::optional<Nvt::Sequence> Nvt::parse_sequence(std::string_view s) {
  std::deque<char> d(std::begin(s), std::end(s));
  if (auto cmd = take_command(d)) {
    if (*cmd == Nvt::Command::SB) {
      // This command has subnegoiations
      return parse_sb(d);
    }
    if (auto opt = take_option(d)) {
      // Update internal representation.
      process_command(*cmd, opt.value(), "");
      Nvt::Sequence r(*cmd, *opt, "");
      r.length = 2;
      return r;
    }
  }
  return std::nullopt;
}

// True if both sides agree to an option.
bool Nvt::enabled(Option opt) {
  if (const auto it = options.find(opt); it != std::end(options)) {
    const auto& c = it->second;
    return c.d && c.w;
  }
  return false;
}

// Updates the half-states of each option.
void Nvt::process_command(Command cmd, Option opt, std::string_view a) {
  OptionConfig c{};
  if (const auto it = options.find(opt); it != std::end(options)) {
    c = it->second;
  }
  switch (cmd) {
  case Command::WILL:
    c.w = true;
    break;
  case Command::DO:
    c.d = true;
    break;
  case Command::WONT:
    c.w = false;
    break;
  case Command::DONT:
    c.w = false;
    break;
  default:
    break;
  }

  if (!a.empty()) {
    c.value = a;
  }
  options.insert_or_assign(opt, c);
}

void Nvt::process_option_value(Option opt, std::string_view a) {
  if (const auto it = options.find(opt); it != std::end(options)) {
    it->second.value = a;
  }

}


// True if a sub-negoiated option has a value
std::optional<std::string> Nvt::value(Option opt) {
  if (const auto it = options.find(opt); it != std::end(options)) {
    return it->second.value;
  }
  return std::nullopt;

}

std::string to_string(const Nvt::Option seq) {
  static std::map<Nvt::Option, std::string> MAP = {
      {Nvt::Option::BINARY, "BINARY"},
      {Nvt::Option::ECHO, "ECHO"},
      {Nvt::Option::LINEMODE, "LINEMODE"},
      {Nvt::Option::SEND_LOCATION, "SEND_LOCATION"} ,
      {Nvt::Option::SUPPRESS_GA, "SUPPRESS_GA"} };
  return wwiv::stl::get_or_default(MAP, seq, "???");
}

std::string to_string(const Nvt::Command cmd) {
  static std::map<Nvt::Command, std::string> MAP = {
      {Nvt::Command::NOP, "NOP"},
      {Nvt::Command::BRK, "BRK"},
      {Nvt::Command::WILL, "WILL"},
      {Nvt::Command::WONT, "WONT"},
      {Nvt::Command::DO, "DO"},
      {Nvt::Command::DONT, "DONT"} };
  return wwiv::stl::get_or_default(MAP, cmd, "???");
}

std::string to_string(const Nvt::Sequence& seq) {
  return fmt::format("IAC Seq: Cmd: {} Opt: {}  Arg: '{}'", to_string(seq.cmd), to_string(seq.opt), seq.arg);
}



}; // namespace wwiv::common


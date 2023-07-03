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
#ifndef INCLUDED_COMMON_NVT_H
#define INCLUDED_COMMON_NVT_H

#include <deque>
#include <map>
#include <optional>
#include <string>

namespace wwiv::common {

class RemoteIO;
class RemoteSocketIO;

/*
Commands:
=========

SE	240	End of subnegotiation parameters.
NOP	241	No operation
DM	242	Data mark. Indicates the position of a Synch event within the data stream. This should always be accompanied by a TCP urgent notification.
BRK	243	Break. Indicates that the "break" or "attention" key was hit.
IP	244	Suspend, interrupt or abort the process to which the NVT is connected.
AO	245	Abort output. Allows the current process to run to completion but do not send its output to the user.
AYT	246	Are you there. Send back to the NVT some visible evidence that the AYT was received.
EC	247	Erase character. The receiver should delete the last preceding undeleted character from the data stream.
EL	248	Erase line. Delete characters from the data stream back to but not including the previous CRLF.
GA	249	Go ahead. Used, under certain circumstances, to tell the other end that it can transmit.
SB	250	Subnegotiation of the indicated option follows.
WILL	251	Indicates the desire to begin performing, or confirmation that you are now performing, the indicated option.
WONT	252	Indicates the refusal to perform, or continue performing, the indicated option.
DO	253	Indicates the request that the other party perform, or confirmation that you are expecting the other party to perform, the indicated option.
DONT	254	Indicates the demand that the other party stop performing, or confirmation that you are no longer expecting the other party to perform, the indicated option.
IAC	255	Interpret as command

 */
// Telnet Network Virtual Terminal (NVT)
class Nvt {

public:
  enum class Command {
    SE = 240,   // End of subnegotiation parameters.
    NOP = 241,  // No operation
    DM = 242,   // Data mark. Indicates the position of a Synch event within the data stream. This should always be accompanied by a TCP urgent notification.
    BRK = 243,  // Break. Indicates that the "break" or "attention" key was hit.
    IP = 244,   // Suspend, interrupt or abort the process to which the NVT is connected.
    AO = 245,   // Abort output. Allows the current process to run to completion but do not send its output to the user.
    AYT = 246,  // Are you there. Send back to the NVT some visible evidence that the AYT was received.
    EC = 247,   // Erase character. The receiver should delete the last preceding undeleted character from the data stream.
    EL = 248,   // Erase line. Delete characters from the data stream back to but not including the previous CRLF.
    GA = 249,   // Go ahead. Used, under certain circumstances, to tell the other end that it can transmit.
    SB = 250,   // Subnegotiation of the indicated option follows.
    WILL = 251, // Indicates the desire to begin performing, or confirmation that you are now performing, the indicated option.
    WONT = 252, // Indicates the refusal to perform, or continue performing, the indicated option.
    DO = 253,   // Indicates the request that the other party perform, or confirmation that you are expecting the other party to perform, the indicated option.
    DONT = 254, // Indicates the demand that the other party stop performing, or confirmation that you are no longer expecting the other party to perform, the indicated option.
    IAC = 255,  // Interpret as command
  };

  enum class Option {
    BINARY = 0,
    ECHO = 1,
    SUPPRESS_GA = 3,
    SEND_LOCATION = 23,
    LINEMODE = 34,
  };

  Nvt() {}
  ~Nvt() {}

  struct Sequence {
    Sequence() : cmd(Command::IAC) {}
    Sequence(Command c) : cmd(c) {}
    Sequence(Command c, Option o) : cmd(c), opt(o) {}
    Sequence(Command c, Option o, std::string_view a) : cmd(c), opt(o), arg(a) {}
    Command cmd;
    Option opt;
    std::string arg;
    // Length minus IAC
    int length{ 2 };
  };

  // Creates the string for a command
  std::string create_command(Command cmd, Option option);
  // Creates the string to set a sub-negoiated option
  std::string create_sub(Option option);
  // Creates the string to set a sub-negoiated option with value
  std::string create_sub(Option option, const std::string_view arg);
  // Parses an IAC sequence, s startes at the character after the IAC
  std::optional<Sequence> parse_sequence(std::string_view s);

  // Creates and sends a command, also updating internal mapping of value in options.
  bool send_command(Command cmd, Option option, RemoteIO* out);
  // Creates and sends a command, also updating internal mapping of value in options.
  bool send_negotiate(Option option, RemoteIO* out);
  // Creates and sends a command, also updating internal mapping of value in options.
  bool send_negotiate(Option option, const std::string_view arg, RemoteIO* out);
  // Creates and sends an ack for the given command, also updating internal mapping of value in options.
  bool send_ack(Command cmd, Option option, RemoteIO* out);
  // Creates and sends a nak for the given command, also updating internal mapping of value in options.
  bool send_nak(Command cmd, Option option, RemoteIO* out);

  // True if both sides agree to an option.
  bool enabled(Option opt);

  // Updates the half-states of each option.
  void process_command(Command cmd, Option opt, std::string_view a);
  // Updates the half-states of each option.
  void process_option_value(Option opt, std::string_view a);

  // True if a sub-negoiated option has a value
  std::optional<std::string> value(Option opt);

private:
  std::optional<Nvt::Sequence> parse_sb(std::deque<char>& d);

  struct OptionConfig {
    bool w; // True if will
    bool d; // True if do
    std::string value; // arg
  };
  std::map<Option, OptionConfig> options;
};

std::string to_string(const Nvt::Option seq);
std::string to_string(const Nvt::Command seq);
std::string to_string(const Nvt::Sequence& seq);

}; // namespace wwiv::common

#endif // INCLUDED_COMMON_NVT_H
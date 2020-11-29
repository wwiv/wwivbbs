/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
#include "bbs/qwk/qwk_reply.h"

// ReSharper disable once CppUnusedIncludeDirective
#include <fcntl.h>
#ifdef _WIN32
// ReSharper disable once CppUnusedIncludeDirective
#include <io.h> // needed for lseek, etc
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "qwk_config.h"
#include "qwk_email.h"
#include "qwk_ui.h"
#include "bbs/acs.h"
#include "bbs/application.h"
#include "bbs/archivers.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/conf.h"
#include "bbs/confutil.h"
#include "bbs/connect1.h"
#include "bbs/execexternal.h"
#include "bbs/message_file.h"
#include "bbs/msgbase1.h"
#include "bbs/shortmsg.h"
#include "bbs/sr.h"
#include "bbs/stuffin.h"
#include "bbs/subacc.h"
#include "bbs/sublist.h"
#include "bbs/sysoplog.h"
#include "bbs/qwk/qwk_text.h"
#include "common/com.h"
#include "common/input.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "local_io/wconstants.h"
#include "sdk/names.h"
#include "sdk/status.h"
#include "sdk/subxtr.h"
#include "sdk/vardec.h"
#include <chrono>
#include <memory>
#include <optional>
#include <sstream>

using std::string;
using std::unique_ptr;
using std::chrono::milliseconds;

using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;

#define qwk_iscan_literal(x) (iscan1(x))

namespace wwiv::bbs::qwk {



static void qwk_receive_file(const std::string& fn, bool* received, int prot_num) {
  if (prot_num <= 1 || prot_num == 5) {
    prot_num = get_protocol(xfertype::xf_up_temp);
  }

  switch (prot_num) {
  case -1:
  case 0:
  case WWIV_INTERNAL_PROT_ASCII:
  case WWIV_INTERNAL_PROT_BATCH:
    *received = false;
    break;
  case WWIV_INTERNAL_PROT_XMODEM:
  case WWIV_INTERNAL_PROT_XMODEMCRC:
  case WWIV_INTERNAL_PROT_YMODEM:
  case WWIV_INTERNAL_PROT_ZMODEM:
    maybe_internal(fn, received, nullptr, false, prot_num);
    break;
  default:
    if (a()->sess().incom()) {
      extern_prot(prot_num - WWIV_NUM_INTERNAL_PROTOCOLS, fn, 0);
      *received = File::Exists(fn);
    }
    break;
  }
}

static std::filesystem::path ready_reply_packet(const std::string& packet_name, const std::string& msg_name) {
  const auto archiver = match_archiver(a()->arcs, packet_name).value_or(a()->arcs[0]);
  const auto command = stuff_in(archiver.arce, packet_name, msg_name, "", "", "");

  ExecuteExternalProgram(command, EFLAG_QWK_DIR);
  return FilePath(a()->sess().dirs().qwk_directory(), msg_name);
}

static std::string make_text_file(int filenumber, int curpos, int blocks) {
  std::string text;
  text.resize(sizeof(qwk_record) * blocks);

  lseek(filenumber, static_cast<long>(curpos) * static_cast<long>(sizeof(qwk_record)), SEEK_SET);
  read(filenumber, &text[0], sizeof(qwk_record) * blocks);

  return make_text_ready(text, static_cast<int>(sizeof(qwk_record) * blocks));
}

static void qwk_post_text(std::string text, std::string to, const std::string& title,
                          int16_t sub) {
  messagerec m{};
  postrec p{};

  int dm, done = 0, pass = 0;
  slrec ss{};
  char user_name[101];

  while (!done && !a()->sess().hangup()) {
    if (pass > 0) {
      int done5 = 0;
      char substr[5];

      while (!done5 && !a()->sess().hangup()) {
        bout.nl();
        bout << "Then which sub?  ?=List  Q=Don't Post :";
        bin.input(substr, 3);

        StringTrim(substr);

        if (substr[0] == 'Q') {
          return;
        }
        if (substr[0] == '?') {
          SubList();
        } else {
          sub = a()->usub[to_number<int>(substr) - 1].subnum;
          done5 = 1;
        }
      }
    }

    if (sub >= ssize(a()->usub) || sub < 0) {
      bout.Color(5);
      bout.bputs("Sub out of range");

      ++pass;
      continue;
    }
    a()->set_current_user_sub_num(static_cast<uint16_t>(sub));

    // Busy files... allow to retry
    while (!a()->sess().hangup()) {
      if (!qwk_iscan_literal(a()->current_user_sub_num())) {
        bout.nl();
        bout << "MSG file is busy on another instance, try again?";
        if (!bin.noyes()) {
          ++pass;
          continue;
        }
      } else {
        break;
      }
    }

    if (a()->sess().GetCurrentReadMessageArea() < 0) {
      bout.Color(5);
      bout.bputs("Sub out of range");

      ++pass;
      continue;
    }

    ss = a()->effective_slrec();

    int xa = 0;
    // User is restricted from posting
    if ((restrict_post & a()->user()->data.restrict) || (a()->user()->data.posttoday >= ss.posts)) {
      bout.nl();
      bout.bputs("Too many messages posted today.");
      bout.nl();

      ++pass;
      continue;
    }

    // User doesn't have enough sl to post on sub
    if (!wwiv::bbs::check_acs(a()->current_sub().post_acs)) {
      bout.nl();
      bout.bputs("You can't post here.");
      bout.nl();
      ++pass;
      continue;
    }

    m.storage_type = static_cast<uint8_t>(a()->current_sub().storage_type);

    if (!a()->current_sub().nets.empty()) {
      xa &= (anony_real_name);

      if (a()->user()->data.restrict & restrict_net) {
        bout.nl();
        bout.bputs("You can't post on networked sub-boards.");
        bout.nl();
        ++pass;
        continue;
      }
    }

    bout.cls();
    bout << "|#5Posting New Message.\r\n\n";
    bout.format("|#9Title      : |#2{}\r\n", title);
    bout.format("|#9To         : |#2{}\r\n", to.empty() ? "All" : to);
    bout.format("|#9Area       : |#2{}\r\n", stripcolors(a()->current_sub().name));

    if (!a()->current_sub().nets.empty()) {
      bout << "|#9On Networks: |#5";
      for (const auto& xnp : a()->current_sub().nets) {
        bout << a()->nets()[xnp.net_num].name << " ";
      }
      bout.nl();
    }

    bout.nl();
    bout << "|#5Correct? ";

    if (bin.noyes()) {
      done = true;
    } else {
      ++pass;
    }
  }
  bout.nl();

  if (a()->current_sub().anony & anony_real_name) {
    strcpy(user_name, a()->user()->GetRealName());
    properize(user_name);
  } else {
    const string name = a()->names()->UserName(a()->sess().user_num(), a()->current_net().sysnum);
    strcpy(user_name, name.c_str());
  }

  if (!to.empty() && !iequals(to, "ALL")) {
    const auto buf = fmt::sprintf("%c0FidoAddr: %s\r\n", CD, to);
    text = StrCat(buf, text);
  }
  qwk_inmsg(text.c_str(), &m, a()->current_sub().filename.c_str(), user_name, DateTime::now());

  if (m.stored_as != 0xffffffff) {
    while (!a()->sess().hangup()) {
      const int f = qwk_iscan_literal(a()->sess().GetCurrentReadMessageArea());

      if (f == -1) {
        bout.nl();
        bout << "MSG file is busy on another instance, try again?";
        if (!bin.noyes()) {
          return;
        }
      } else {
        break;
      }
    }

    // Anonymous
    uint8_t an = 0;
    if (an) {
      bout.Color(1);
      bout << "Anonymous?";
      an = bin.yesno() ? 1 : 0;
    }
    bout.nl();

    to_char_array(p.title, title);
    p.anony = an;
    p.msg = m;
    p.ownersys = 0;
    p.owneruser = static_cast<uint16_t>(a()->sess().user_num());
    {
      a()->status_manager()->Run([&](WStatus& s) { p.qscan = s.IncrementQScanPointer(); });
    }
    p.daten = daten_t_now();
    if (a()->user()->data.restrict & restrict_validate) {
      p.status = status_unvalidated;
    } else {
      p.status = 0;
    }

    open_sub(true);

    if (!a()->current_sub().nets.empty() && a()->current_sub().anony & anony_val_net &&
        (!lcs() || !a()->sess().irt().empty())) {
      p.status |= status_pending_net;
      dm = 1;

      for (auto i = a()->GetNumMessagesInCurrentMessageArea();
           (i >= 1) && (i > (a()->GetNumMessagesInCurrentMessageArea() - 28)); i--) {
        if (get_post(i)->status & status_pending_net) {
          dm = 0;
          break;
        }
      }
      if (dm) {
        ssm(1) << "Unvalidated net posts on " << a()->current_sub().name << ".";
      }
    }

    if (a()->GetNumMessagesInCurrentMessageArea() >= a()->current_sub().maxmsgs) {
      int i = 1;
      dm = 0;
      while (dm == 0 && i <= a()->GetNumMessagesInCurrentMessageArea() && !a()->sess().hangup()) {
        if ((get_post(i)->status & status_no_delete) == 0) {
          dm = i;
        }
        ++i;
      }
      if (dm == 0) {
        dm = 1;
      }
      delete_message(dm);
    }

    add_post(&p);

    ++a()->user()->data.msgpost;
    ++a()->user()->data.posttoday;

    a()->status_manager()->Run([](WStatus& s) {
      s.IncrementNumLocalPosts();
      s.IncrementNumMessagesPostedToday();
    });

    close_sub();

    sysoplog() << "+ '" << p.title << "' posted on '" << a()->current_sub().name;

    if (!a()->current_sub().nets.empty()) {
      ++a()->user()->data.postnet;
      if (!(p.status & status_pending_net)) {
        send_net_post(&p, a()->current_sub());
      }
    }
  }
}


static void process_reply_dat(const std::string& name) {
  qwk_record qwk{};
  int curpos = 0;
  int done = 0;

  int repfile = open(name.c_str(), O_RDONLY | O_BINARY);

  if (repfile < 0) {
    bout.nl();
    bout.Color(3);
    bout.bputs("Can't open packet.");
    bout.pausescr();
    return;
  }

  lseek(repfile, static_cast<long>(curpos) * static_cast<long>(sizeof(qwk_record)), SEEK_SET);
  read(repfile, &qwk, sizeof(qwk_record));

  // Should check to make sure first block contains our bbs id
  ++curpos;

  bout.cls();

  while (!done && !a()->sess().hangup()) {
    bool to_email = false;

    lseek(repfile, static_cast<long>(curpos) * static_cast<long>(sizeof(qwk_record)), SEEK_SET);
    ++curpos;

    if (read(repfile, &qwk, sizeof(qwk_record)) < 1) {
      done = 1;
    } else {
      char blocks[7];
      char to[201];
      char title[26];
      char tosub[8];

      strncpy(blocks, qwk.amount_blocks, 6);
      blocks[6] = 0;

      strncpy(tosub, qwk.msgnum, 7);
      tosub[7] = 0;

      strncpy(title, qwk.subject, 25);
      title[25] = 0;

      strncpy(to, qwk.to, 25);
      to[25] = 0;
      strupr(to);
      StringTrim(to);

      // If in sub 0 or not public, possibly route into email
      if (to_number<int>(tosub) == 0) {
        to_email = true;
      } else if (qwk.status != ' ' && qwk.status != '-') { // if not public
        bout.cls();
        bout.Color(1);
        bout.bprintf("Message '2%s1' is marked 3PRIVATE", title);
        bout.nl();
        bout.Color(1);
        bout.bprintf("It is addressed to 2%s", to);
        bout.nl(2);
        bout.Color(7);
        bout << "Route into E-Mail?";
        if (bin.noyes()) {
          to_email = true;
        }
      }

      auto text = make_text_file(repfile, curpos, to_number<int>(blocks) - 1);
      if (text.empty()) {
        curpos += to_number<int>(blocks) - 1;
        continue;
      }

      if (to_email) {
        auto to_from_msg_opt = get_qwk_from_message(text);
        if (to_from_msg_opt.has_value()) {
          bout.nl();
          bout.Color(3);
          bout.bprintf("1) %s", to);
          bout.nl();
          bout.Color(3);
          bout.bprintf("2) %s", to_from_msg_opt.value());
          bout.nl(2);

          bout << "Which address is correct?";
          bout.mpl(1);

          const int x = onek("12");

          if (x == '2') {
            to_char_array(to, to_from_msg_opt.value());
          }
        }
      }

      if (to_email) {
        qwk_email_text(text.c_str(), title, to);
      } else if (File::freespace_for_path(a()->config()->msgsdir()) < 10) {
        // Not enough disk space
        bout.nl();
        bout.bputs("Sorry, not enough disk space left.");
        bout.pausescr();
      } else {
        qwk_post_text(text, to, title, to_number<int16_t>(tosub) - 1);
      }
      curpos += to_number<int>(blocks) - 1;
    }
  }
  close(repfile);
}


void upload_reply_packet() {
  bool rec = true;
  int save_conf = 0;
  auto qwk_cfg = read_qwk_cfg();

  if (!qwk_cfg.fu) {
    qwk_cfg.fu = daten_t_now();
  }

  ++qwk_cfg.timesu;
  write_qwk_cfg(qwk_cfg);

  const auto save_sub = a()->current_user_sub_num();
  if (ok_multiple_conf(a()->user(), a()->uconfsub)) {
    save_conf = 1;
    tmp_disable_conf(true);
  }

  const auto rep_name = StrCat(qwk_system_name(qwk_cfg), ".REP");
  bout.litebar("Upload QWK Reply Packet");
  bout.nl();
  bout.format("|#9QWK Reply Packet must be named: \"|#2{}|#9\"\r\n", rep_name);
  bout.bputs("|#5Would you like to upload a QWK Reply Packet? ");
  const auto rep_path = FilePath(a()->sess().dirs().qwk_directory(), rep_name);

  const auto do_it = bin.yesno();

  if (do_it) {
    if (a()->sess().incom()) {
      qwk_receive_file(rep_path.string(), &rec, a()->user()->data.qwk_protocol);
      sleep_for(milliseconds(500));
    } else {
      bout << "|#5Please copy the REP file to the following directory: " << wwiv::endl;
      bout << "|#2" << a()->sess().dirs().qwk_directory() << wwiv::endl;
      bout.pausescr();
    }

    if (rec) {
      const auto msg_name = StrCat(qwk_system_name(qwk_cfg), ".MSG");
      auto msg_path = ready_reply_packet(rep_path.string(), msg_name);
      process_reply_dat(msg_path.string());
    } else {
      sysoplog() << "Aborted";
      bout.nl();
      bout.format("|#6Reply Packet: |#2{} |#6not found.\r\n", rep_name);
      bout.nl();
    }
  }
  if (save_conf) {
    tmp_disable_conf(false);
  }

  a()->set_current_user_sub_num(save_sub);
}


void qwk_inmsg(const char* text, messagerec* m1, const char* aux, const char* name,
               const DateTime& dt) {
  ScopeExit at_exit([=]() {
    // Might not need to do this anymore since quoting
    // isn't so convoluted.
    bin.charbufferpointer_ = 0;
    bin.charbuffer[0] = 0;
  });

  auto m = *m1;
  std::ostringstream ss;
  ss << name << "\r\n";
  ss << dt.to_string() << "\r\n";
  ss << text << "\r\n";

  auto message_text = ss.str();
  if (message_text.back() != CZ) {
    message_text.push_back(CZ);
  }
  savefile(message_text, &m, aux);
  *m1 = m;
}




}

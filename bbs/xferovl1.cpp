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

#include "bbs/xferovl1.h"
#include "bbs/batch.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/com.h"
#include "bbs/conf.h"
#include "bbs/datetime.h"
#include "bbs/execexternal.h"
#include "bbs/external_edit.h"
#include "bbs/input.h"
#include "bbs/instmsg.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/sr.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "bbs/xfer_common.h"
#include "bbs/xferovl.h"
#include "bbs/xfertmp.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/numbers.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "local_io/wconstants.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/files/diz.h"
#include "sdk/files/files.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

// How far to indent extended descriptions
static const int INDENTION = 24;

extern int foundany;
static const char* invalid_chars = "ڿ��ĳô��ɻȼͺ̹��ոԾͳƵ��ַӽĺǶ�����װ�������";

using std::string;
using wwiv::sdk::files::FileName;
using namespace wwiv::bbs;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::files;
using namespace wwiv::stl;
using namespace wwiv::strings;

void modify_extended_description(std::string* sss, const std::string& dest) {
  char s[255], s1[255];
  int i, i2;

  bool ii = !sss->empty();
  int i4 = 0;
  bout << "File Name: " << dest << wwiv::endl;
  do {
    if (ii) {
      bout.nl();
      if (okfsed() && a()->HasConfigFlag(OP_FLAGS_FSED_EXT_DESC)) {
        bout << "|#5Modify the extended description? ";
      } else {
        bout << "|#5Enter a new extended description? ";
      }
      if (!yesno()) {
        return;
      }
    } else {
      bout.nl();
      bout << "|#5Enter an extended description? ";
      if (!yesno()) {
        return;
      }
    }
    if (okfsed() && a()->HasConfigFlag(OP_FLAGS_FSED_EXT_DESC)) {
      if (!sss->empty()) {
        TextFile file(FilePath(a()->temp_directory(), "extended.dsc"), "w");
        file.Write(*sss);
      } else {
        File::Remove(FilePath(a()->temp_directory(), "extended.dsc"));
      }

      const auto saved_screen_chars = a()->user()->GetScreenChars();
      if (a()->user()->GetScreenChars() > (76 - INDENTION)) {
        a()->user()->SetScreenChars(76 - INDENTION);
      }

      const auto edit_ok = external_text_edit("extended.dsc", a()->temp_directory(),
                                              a()->max_extend_lines, MSGED_FLAG_NO_TAGLINE);
      a()->user()->SetScreenChars(saved_screen_chars);
      if (edit_ok) {
        TextFile file(FilePath(a()->temp_directory(), "extended.dsc"), "r");
        *sss = file.ReadFileIntoString();

        for (auto i3 = wwiv::stl::ssize(*sss) - 1; i3 >= 0; i3--) {
          if ((*sss)[i3] == 1) {
            (*sss)[i3] = ' ';
          }
        }
      }
    } else {
      sss->clear();
      i = 1;
      bout.nl();
      bout << "Enter up to  " << a()->max_extend_lines << " lines, "
          << 78 - INDENTION << " chars each.\r\n";
      bout.nl();
      s[0] = '\0';
      const int nSavedScreenSize = a()->user()->GetScreenChars();
      if (a()->user()->GetScreenChars() > (76 - INDENTION)) {
        a()->user()->SetScreenChars(76 - INDENTION);
      }
      do {
        bout << "|#2" << i << ": |#0";
        s1[0] = '\0';
        const auto allow_previous = i4 > 0;
        while (inli(s1, s, 90, true, allow_previous, false, true)) {
          if (i > 1) {
            --i;
          }
          sprintf(s1, "%d:", i);
          bout << "|#2" << s1;
          i2 = 0;
          i4 -= 2;
          do {
            s[i2] = sss->at(i4 - 1);
            ++i2;
            --i4;
          }
          while (sss->at(i4) != 10 && i4 != 0);
          if (i4) {
            ++i4;
          }
          sss->resize(i4);
          s[i2] = 0;
          strrev(s);
          if (strlen(s) > static_cast<unsigned int>(a()->user()->GetScreenChars() - 1)) {
            s[a()->user()->GetScreenChars() - 2] = '\0';
          }
        }
        i2 = strlen(s1);
        if (i2 && (s1[i2 - 1] == 1)) {
          s1[i2 - 1] = '\0';
        }
        if (s1[0]) {
          strcat(s1, "\r\n");
          *sss += s1;
          i4 += strlen(s1);
        }
      }
      while (i++ < a()->max_extend_lines && s1[0]);
      a()->user()->SetScreenChars(nSavedScreenSize);
    }
    bout << "|#5Is this what you want? ";
    i = !yesno();
    if (i) {
      sss->clear();
    }
  }
  while (i);
}

static bool has_arc_cmd_for_ext(const std::string& ext) {
  for (auto i = 0; i < MAX_ARCS; i++) {
    if (iequals(ext, a()->arcs[i].extension)) {
      return true;
    }
  }
  return false;
}

static std::optional<std::filesystem::path> PathToTempdDiz(const std::filesystem::path& p) {
  File::Remove(FilePath(a()->temp_directory(), FILE_ID_DIZ));
  File::Remove(FilePath(a()->temp_directory(), DESC_SDI));

  if (!p.has_extension() || !has_arc_cmd_for_ext(p.extension().string().substr(1))) {
    return std::nullopt;
  }

  const auto cmd = get_arc_cmd(p.string(), 1, "FILE_ID.DIZ DESC.SDI");

  ExecuteExternalProgram(cmd, EFLAG_NOHUP | EFLAG_TEMP_DIR);
  auto diz_fn = FilePath(a()->temp_directory(), FILE_ID_DIZ);
  if (!File::Exists(diz_fn)) {
    diz_fn = FilePath(a()->temp_directory(), DESC_SDI);
  }
  if (!File::Exists(diz_fn)) {
    return std::nullopt;
  }
  return {diz_fn};
}

bool get_file_idz(FileRecord& fr, const directory_t& dir) {
  ScopeExit at_exit([] {
    File::Remove(FilePath(a()->temp_directory(), FILE_ID_DIZ));
    File::Remove(FilePath(a()->temp_directory(), DESC_SDI));
  });

  if (!a()->HasConfigFlag(OP_FLAGS_READ_CD_IDZ) && dir.mask & mask_cdrom) {
    return false;
  }

  fr.set_date(DateTime::from_time_t(File::creation_time(FilePath(dir.path, fr))));
  auto o = PathToTempdDiz(FilePath(dir.path, fr));
  if (!o) {
    return true;
  }
  const auto diz_fn = o.value();
  bout.nl();
  bout << "|#9Reading in |#2" << diz_fn.filename() << "|#9 as extended description...";
  const auto old_ext = a()->current_file_area()->ReadExtendedDescriptionAsString(fr).value_or("");

  DizParser dp(a()->HasConfigFlag(OP_FLAGS_IDZ_DESC));
  auto odiz = dp.parse(diz_fn);

  if (odiz.has_value()) {
    auto& diz = odiz.value();
    fr.set_description(diz.description());
    const auto ext_desc = diz.extended_description();
    if (!ext_desc.empty()) {
      if (!old_ext.empty()) {
        a()->current_file_area()->DeleteExtendedDescription(fr.filename());
      }
      a()->current_file_area()->AddExtendedDescription(fr.filename(), ext_desc);
      fr.set_extended_description(true);
    }
  }

  bout << "Done!\r\n";
  return true;
}

int read_idz_all() {
  int count = 0;

  tmp_disable_conf(true);
  TempDisablePause disable_pause;
  a()->ClearTopScreenProtection();
  for (auto i = 0;
       i < a()->dirs().size() && a()->udir[i].subnum != -1 && !a()->localIO()->KeyPressed(); i++) {
    count += read_idz(false, i);
  }
  tmp_disable_conf(false);
  a()->UpdateTopScreen();
  return count;
}

int read_idz(bool prompt_for_mask, int tempdir) {
  int count = 0;
  bool abort = false;
  const auto dir_num = a()->udir[tempdir].subnum;
  const auto dir = a()->dirs()[dir_num];

  std::unique_ptr<TempDisablePause> disable_pause;
  std::string s = "*.*";
  if (prompt_for_mask) {
    disable_pause.reset(new TempDisablePause);
    a()->ClearTopScreenProtection();
    dliscan();
    s = file_mask();
  } else {
    s = aligns(s);
    dliscan1(dir_num);
  }
  bout << fmt::sprintf("|#9Checking for external description files in |#2%-25.25s #%s...\r\n",
                       dir.name, a()->udir[tempdir].keys);
  auto* area = a()->current_file_area();
  for (auto i = 1; i <= area->number_of_files() && !a()->hangup_ && !abort; i++) {
    auto f = area->ReadFile(i);
    const auto fn = f.aligned_filename();
    if (aligned_wildcard_match(s, fn) && !ends_with(fn, ".COM") && !ends_with(fn, ".EXE")) {
      if (File::Exists(FilePath(dir.path, f))) {
        if (get_file_idz(f, dir)) {
          count++;
        }
        if (area->UpdateFile(f, i)) {
          area->Save();
        }
      }
    }
    checka(&abort);
  }
  if (prompt_for_mask) {
    a()->UpdateTopScreen();
  }
  return count;
}

void tag_it() {
  long fs = 0;

  if (a()->batch().size() >= a()->max_batch) {
    bout << "|#6No room left in batch queue.";
    bout.getkey();
    return;
  }
  bout << "|#2Which file(s) (1-" << a()->filelist.size()
      << ", *=All, 0=Quit)? ";
  auto s3 = input(30, true);
  if (!s3.empty() && s3.front() == '*') {
    s3.clear();
    for (size_t i2 = 0; i2 < a()->filelist.size() && i2 < 78; i2++) {
      auto s2 = fmt::format("{} ", i2 + 1);
      s3 += s2;
      if (s3.size() > 250 /* was sizeof(s3)-10 */) {
        break;
      }
    }
    bout << "\r\n|#2Tagging: |#4" << s3 << wwiv::endl;
  }
  for (int i2 = 0; i2 < wwiv::strings::ssize(s3); i2++) {
    auto s1 = s3.substr(i2);
    int i4 = 0;
    bool bad = false;
    for (int i3 = 0; i3 < wwiv::strings::ssize(s1); i3++) {
      if (s1[i3] == ' ' || s1[i3] == ',' || s1[i3] == ';') {
        s1 = s1.substr(0, i3);
        i4 = 1;
      } else {
        if (i4 == 0) {
          i2++;
        }
      }
    }
    int i = to_number<int>(s1);
    if (i == 0) {
      break;
    }
    i--;
    if (!s1.empty() && i >= 0 && i < ssize(a()->filelist)) {
      auto& f = a()->filelist[i];
      if (a()->batch().contains_file(f.u.filename)) {
        bout << "|#6" << f.u.filename << " is already in the batch queue.\r\n";
        bad = true;
      }
      if (a()->batch().size() >= a()->max_batch) {
        bout << "|#6Batch file limit of " << a()->max_batch << " has been reached.\r\n";
        bad = true;
      }
      if (a()->config()->req_ratio() > 0.0001 && ratio() < a()->config()->req_ratio() &&
          !a()->user()->IsExemptRatio() && !bad) {
        bout << fmt::sprintf(
            "|#2Your up/download ratio is %-5.3f.  You need a ratio of %-5.3f to download.\r\n",
            ratio(), a()->config()->req_ratio());
        bad = true;
      }
      if (!bad) {
        auto s = FilePath(a()->dirs()[f.directory].path, FileName(f.u.filename));
        if (f.dir_mask & mask_cdrom) {
          auto s2 = FilePath(a()->dirs()[f.directory].path, FileName(f.u.filename));
          s = FilePath(a()->temp_directory(), FileName(f.u.filename));
          if (!File::Exists(s)) {
            File::Copy(s2, s);
          }
        }
        File fp(s);
        if (!fp.Open(File::modeBinary | File::modeReadOnly)) {
          bout << "|#6The file " << FileName(f.u.filename) << " is not there.\r\n";
          bad = true;
        } else {
          fs = fp.length();
          fp.Close();
        }
      }
      if (!bad) {
        const auto t = time_to_transfer(a()->modem_speed_,fs).count();
        if (nsl() <= a()->batch().dl_time_in_secs() + t) {
          bout << "|#6Not enough time left in queue for " << f.u.filename << ".\r\n";
          bad = true;
        }
      }
      if (!bad) {
        BatchEntry b(f.u.filename, f.directory, fs, true);
        a()->batch().AddBatch(std::move(b));
        bout << "|#1" << f.u.filename << " added to batch queue.\r\n";
      }
    } else {
      bout << "|#6Bad file number " << i + 1 << wwiv::endl;
    }
    bout.clear_lines_listed();
  }
}

static char fancy_prompt(const char* pszPrompt, const char* pszAcceptChars) {
  char ch;

  a()->tleft(true);
  const auto s1 = fmt::format("\r|#2{} (|#1{}|#2)? |#0", pszPrompt, pszAcceptChars);
  const auto s2 = fmt::format("{} ({})? ", pszPrompt, pszAcceptChars);
  const int i1 = ssize(s2);
  const auto s3 = StrCat(pszAcceptChars," \r");
  a()->tleft(true);
  if (okansi()) {
    bout << s1;
    ch = onek_ncr(s3);
    bout.Left(i1);
    for (int i = 0; i < i1; i++) {
      bout.bputch(' ');
    }
    bout.Left(i1);
  } else {
    bout << s2;
    ch = onek_ncr(s3);
    for (auto i = 0; i < i1; i++) {
      bout.bs();
    }
  }
  return ch;
}

void tag_files(bool& need_title) {
  if (bout.lines_listed() == 0) {
    return;
  }
  bool abort = false;
  a()->tleft(true);
  if (a()->hangup_) {
    return;
  }
  bout.clear_lines_listed();
  bout.Color(FRAME_COLOR);
  bout << "\r" << std::string(78, '-') << wwiv::endl;

  bool done = false;
  while (!done && !a()->hangup_) {
    bout.clear_lines_listed();
    char ch = fancy_prompt("File Tagging", "CDEMQRTV?");
    bout.clear_lines_listed();
    switch (ch) {
    case '?': {
      print_help_file(TTAGGING_NOEXT);
      pausescr();
      relist();
    }
    break;
    case 'C':
    case SPACE:
    case RETURN:
      bout.clear_lines_listed();
      a()->filelist.clear();
      need_title = true;
      bout.cls();
      done = true;
      break;
    case 'D':
      batchdl(1);
      bout.nl();
      pausescr();
      bout.cls();
      done = true;
      break;
    case 'E': {
      bout.clear_lines_listed();
      bout << "|#9Which file (1-" << a()->filelist.size() << ")? ";
      auto s = input(2, true);
      int i = to_number<int>(s) - 1;
      if (!s.empty() && i >= 0 && i < ssize(a()->filelist)) {
        auto& f = a()->filelist[i];
        auto d = XFER_TIME(f.u.numbytes);
        bout.nl();
        int i2;
        for (i2 = 0; i2 < a()->dirs().size(); i2++) {
          if (a()->udir[i2].subnum == f.directory) {
            break;
          }
        }
        if (i2 < a()->dirs().size()) {
          bout << "|#1Directory  : |#2#" << a()->udir[i2].keys << ", " << a()->dirs()[f.
                directory].name <<
              wwiv::endl;
        } else {
          bout << "|#1Directory  : |#2#" << "??" << ", " << a()->dirs()[f.directory].name <<
              wwiv::endl;
        }
        bout << "|#1Filename   : |#2" << f.u.filename << wwiv::endl;
        bout << "|#1Description: |#2" << f.u.description << wwiv::endl;
        if (f.u.mask & mask_extended) {
          bout << "|#1Ext. Desc. : |#2";
          print_extended(f.u.filename, &abort, a()->max_extend_lines, 2);
        }
        bout << "|#1File size  : |#2" << bytes_to_k(f.u.numbytes) << wwiv::endl;
        bout << "|#1Apprx. time: |#2" << ctim(std::lround(d)) << wwiv::endl;
        bout << "|#1Uploaded on: |#2" << f.u.date << wwiv::endl;
        bout << "|#1Uploaded by: |#2" << f.u.upby << wwiv::endl;
        bout << "|#1Times D/L'd: |#2" << f.u.numdloads << wwiv::endl;
        if (a()->dirs()[f.directory].mask & mask_cdrom) {
          bout.nl();
          bout << "|#3CD ROM DRIVE\r\n";
        } else {
          auto fn = FilePath(a()->dirs()[f.directory].path, f.u.filename);
          if (!File::Exists(fn)) {
            bout.nl();
            bout << "|#6-=>FILE NOT THERE<=-\r\n";
          }
        }
        bout.nl();
        pausescr();
        relist();

      }
    }
    break;
    case 'M':
      if (dcs()) {
        move_file_t();
        if (a()->filelist.empty()) {
          return;
        }
        relist();
      }
      break;
    case 'Q':
      a()->filelist.clear();
      bout.clear_lines_listed();
      need_title = false;
      return;
    case 'R':
      relist();
      break;
    case 'T':
      tag_it();
      break;
    case 'V': {
      bout << "|#2Which file (1-|#2" << a()->filelist.size() << ")? ";
      auto s = input(2, true);
      int i = to_number<int>(s) - 1;
      if (!s.empty() && i >= 0 && i < ssize(a()->filelist)) {
        auto& f = a()->filelist[i];
        auto s1 = FilePath(a()->dirs()[f.directory].path, FileName(f.u.filename));
        if (a()->dirs()[f.directory].mask & mask_cdrom) {
          auto s2 = FilePath(a()->dirs()[f.directory].path, FileName(f.u.filename));
          s1 = FilePath(a()->temp_directory().c_str(), FileName(f.u.filename));
          if (!File::Exists(s1)) {
            File::Copy(s2, s1);
          }
        }
        if (!File::Exists(s1)) {
          bout << "|#6File not there.\r\n";
          pausescr();
          break;
        }
        auto arc_cmd = get_arc_cmd(s1.string(), 0, "");
        if (!okfn(FileName(f.u.filename).unaligned_filename())) {
          arc_cmd.clear();
        }
        if (!arc_cmd.empty()) {
          bout.nl();
          ExecuteExternalProgram(arc_cmd, a()->spawn_option(SPAWNOPT_ARCH_L));
          bout.nl();
          pausescr();
          a()->UpdateTopScreen();
          bout.cls();
          relist();
        } else {
          bout << "|#6Unknown archive.\r\n";
          pausescr();
          break;
        }
      }
    }
    break;
    default:
      bout.cls();
      done = true;
      break;
    }
  }
  a()->filelist.clear();
  bout.clear_lines_listed();
}


int add_batch(std::string& description, const std::string& aligned_file_name, int dn, long fs) {
  if (a()->batch().FindBatch(aligned_file_name) > -1) {
    return 0;
  }

  const auto t = time_to_transfer(a()->modem_speed_,fs).count();

  if (nsl() <= a()->batch().dl_time_in_secs() + t) {
    bout << "|#6 Insufficient time remaining... press any key.";
    bout.getkey();
  } else {
    if (dn == -1) {
      return 0;
    }
    for (auto& c : description) {
      if (c == '\r')
        c = ' ';
    }
    bout.backline();
    bout << fmt::sprintf(" |#6? |#1%s %3luK |#5%-43.43s |#7[|#2Y/N/Q|#7] |#0", aligned_file_name,
                         bytes_to_k(fs), stripcolors(description));
    auto ch = onek_ncr("QYN\r");
    bout.backline();
    const auto ufn = wwiv::sdk::files::unalign(aligned_file_name);
    const auto dir = a()->dirs()[dn];
    if (to_upper_case<char>(ch) == 'Y') {
      if (dir.mask & mask_cdrom) {
        const auto src = FilePath(dir.path, ufn);
        const auto dest = FilePath(a()->temp_directory(), ufn);
        if (!File::Exists(dest)) {
          if (!File::Copy(src, dest)) {
            bout << "|#6 file unavailable... press any key.";
            bout.getkey();
          }
          bout.backline();
          bout.clreol();
        }
      } else {
        auto f = FilePath(a()->dirs()[dn].path, ufn);
        if (!File::Exists(f) && !so()) {
          bout << "\r";
          bout.clreol();
          bout << "|#6 file unavailable... press any key.";
          bout.getkey();
          bout << "\r";
          bout.clreol();
          return 0;
        }
      }
      const BatchEntry b(aligned_file_name, dn, fs, true);
      bout << "\r";
      const auto bt = ctim(b.time(a()->modem_speed_));
      bout << fmt::sprintf("|#2%3d |#1%s |#2%-7ld |#1%s  |#2%s\r\n", 
                           a()->batch().size() + 1,
                           b.aligned_filename(), b.len(), bt, a()->dirs()[b.dir()].name);
      a()->batch().AddBatch(b);
      bout << "\r";
      bout << "|#5    Continue search? ";
      ch = onek_ncr("YN\r");
      if (to_upper_case<char>(ch) == 'N') {
        return -3;
      }
      return 1;
    }
    if (ch == 'Q') {
      bout.backline();
      return -3;
    }
    bout.backline();
  }
  return 0;
}

int try_to_download(const std::string& file_mask, int dn) {
  int rtn;
  bool abort = false;

  dliscan1(dn);
  int i = recno(file_mask);
  if (i <= 0) {
    checka(&abort);
    return abort ? -1 : 0;
  }
  bool ok = true;
  foundany = 1;
  do {
    a()->tleft(true);
    auto* area = a()->current_file_area();
    auto f = area->ReadFile(i);

    if (!(f.u().mask & mask_no_ratio) && !ratio_ok()) {
      return -2;
    }

    write_inst(INST_LOC_DOWNLOAD, a()->current_user_dir().subnum, INST_FLAGS_ONLINE);
    auto d = fmt::sprintf("%-40.40s", f.description());
    abort = false;
    rtn = add_batch(d, f.aligned_filename(), dn, f.numbytes());

    if (abort || rtn == -3) {
      ok = false;
    } else {
      i = nrecno(file_mask, i);
    }
  } while (i > 0 && ok && !a()->hangup_);

  if (rtn == -2) {
    return -2;
  }
  if (abort || rtn == -3) {
    return -1;
  }
  return 1;
}

void download() {
  int i = 0, color = 0;
  bool ok;
  int rtn = 0;
  bool done = false;

  int useconf = 0;

  bout.cls();
  bout.litebar(StrCat(a()->config()->system_name(), " Batch Downloads"));
  do {
    if (!i) {
      bout << "|#2Enter files, one per line, wildcards okay.  [Space] aborts a search.\r\n";
      bout.nl();
      bout << "|#1 #  File Name    Size    Time      Directory\r\n";
      bout <<
          "|#7\xC4\xC4\xC4 \xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4 \xC4\xC4\xC4\xC4\xC4\xC4\xC4 \xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4 \xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\r\n";
    }
    if (i < ssize(a()->batch().entry)) {
      const auto& b = a()->batch().entry[i];
      if (b.sending()) {
        const auto t = ctim(b.time(a()->modem_speed_));
        bout << fmt::sprintf("|#2%3d |#1%s |#2%-7ld |#1%s  |#2%s\r\n", 
                             i + 1, b.aligned_filename(), b.len(), t, a()->dirs()[b.dir()].name);
      }
    } else {
      do {
        int count = 0;
        ok = true;
        bout.backline();
        bout << fmt::sprintf("|#2%3d ", a()->batch().size() + 1);
        bout.Color(1);
        const bool onl = bout.newline;
        bout.newline = false;
        auto s = input(12);
        bout.newline = onl;
        if (!s.empty() && s.front() != ' ') {
          if (strchr(s.c_str(), '.') == nullptr) {
            s += ".*";
          }
          s = aligns(s);
          rtn = try_to_download(s, a()->current_user_dir().subnum);
          if (rtn == 0) {
            if (a()->uconfdir[1].confnum != -10 && okconf(a()->user())) {
              bout.backline();
              bout << " |#5Search all conferences? ";
              const auto ch = onek_ncr("YN\r");
              if (ch == '\r' || to_upper_case<char>(ch) == 'Y') {
                tmp_disable_conf(true);
                useconf = 1;
              }
            }
            bout.backline();
            auto s1 = fmt::sprintf("%3d %s", a()->batch().size() + 1, s);
            bout.Color(1);
            bout << s1;
            foundany = 0;
            int dn = 0;
            while (dn < a()->dirs().size() && a()->udir[dn].subnum != -1) {
              count++;
              bout.Color(color);
              if (count == NUM_DOTS) {
                bout << "\r";
                bout.Color(color);
                bout << s1;
                color++;
                count = 0;
                if (color == 4) {
                  color++;
                }
                if (color == 10) {
                  color = 0;
                }
              }
              rtn = try_to_download(s, a()->udir[dn].subnum);
              if (rtn < 0) {
                break;
              }
              dn++;
            }
            if (useconf) {
              tmp_disable_conf(false);
            }
            if (!foundany) {
              bout << "|#6 File not found... press any key.";
              bout.getkey();
              bout.backline();
              ok = false;
            }
          }
        } else {
          bout.backline();
          done = true;
        }
      }
      while (!ok && !a()->hangup_);
    }
    i++;
    if (rtn == -2) {
      rtn = 0;
      i = 0;
    }
  } while (!done && !a()->hangup_ && i <= ssize(a()->batch().entry));

  if (!a()->batch().numbatchdl()) {
    return;
  }

  bout.nl();
  if (!ratio_ok()) {
    bout << "\r\nSorry, your ratio is too low.\r\n\n";
    return;
  }
  bout.nl();
  bout << "|#1Files in Batch Queue   : |#2" << a()->batch().size() << wwiv::endl;
  bout << "|#1Estimated Download Time: |#2" << ctim(a()->batch().dl_time_in_secs()) << wwiv::endl;
  bout.nl();
  rtn = batchdl(3);
  if (rtn) {
    return;
  }
  bout.nl();
  if (a()->batch().empty()) {
    return;
  }
  bout << "|#5Hang up after transfer? ";
  const bool had = yesno();
  const int ip = get_protocol(xfertype::xf_down_batch);
  if (ip > 0) {
    switch (ip) {
    case WWIV_INTERNAL_PROT_YMODEM: {
      if (!a()->over_intern.empty()
          && a()->over_intern[2].othr & othr_override_internal
          && a()->over_intern[2].sendbatchfn[0]) {
        dszbatchdl(had, a()->over_intern[2].sendbatchfn, prot_name(WWIV_INTERNAL_PROT_YMODEM));
      } else {
        ymbatchdl(had);
      }
    }
    break;
    case WWIV_INTERNAL_PROT_ZMODEM: {
      zmbatchdl(had);
    }
    break;
    default: {
      dszbatchdl(had, a()->externs[ip - WWIV_NUM_INTERNAL_PROTOCOLS].sendbatchfn,
                 a()->externs[ip - WWIV_NUM_INTERNAL_PROTOCOLS].description);
    }
    }
    if (!had) {
      bout.nl();
      bout << "Your ratio is now: " << ratio() << wwiv::endl;
    }
  }
}

void endlist(int mode) {
  // if mode == 1, list files
  // if mode == 2, new files
  if (a()->filelist.empty()) {
    bout << "\r";
    if (mode == 1) {
      bout << "|#3No matching files found.\r\n\n";
    } else {
      bout << "\r|#1No new files found.\r\n\n";
    }
    return;
  }
  bool need_title = false;
  if (!a()->filelist.empty()) {
    tag_files(need_title);
    return;
  }
  bout.Color(FRAME_COLOR);
  bout << "\r" << std::string(78, '-') << wwiv::endl;
  bout << "\r|#9Files listed: |#2 " << a()->filelist.size();
}

void SetNewFileScanDate() {
  char ag[10];
  bool ok = true;

  bout.nl();
  bout << "|#9Current limiting date: |#2" << DateTime::from_daten(a()->context().nscandate()).
      to_string("%m/%d/%y") << "\r\n";
  bout.nl();
  bout << "|#9Enter new limiting date in the following format: \r\n";
  bout << "|#1 MM/DD/YY\r\n|#7:";
  bout.mpl(8);
  int i = 0;
  char ch = 0;
  do {
    if (i == 2 || i == 5) {
      ag[i++] = '/';
      bout.bputch('/');
    } else {
      switch (i) {
      case 0:
        ch = onek_ncr("01\r");
        break;
      case 3:
        ch = onek_ncr("0123\b");
        break;
      case 8:
        ch = onek_ncr("\b\r");
        break;
      default:
        ch = onek_ncr("0123456789\b");
        break;
      }
      if (a()->hangup_) {
        ok = false;
        ag[0] = '\0';
        break;
      }
      switch (ch) {
      case '\r':
        switch (i) {
        case 0:
          ok = false;
          break;
        case 8:
          ag[8] = '\0';
          break;
        default:
          ch = '\0';
          break;
        }
        break;
      case BACKSPACE:
        bout << " \b";
        --i;
        if (i == 2 || i == 5) {
          bout.bs();
          --i;
        }
        break;
      default:
        ag[i++] = ch;
        break;
      }
    }
  }
  while (ch != '\r' && !a()->hangup_);

  bout.nl();
  if (ok) {
    int m = atoi(ag);
    int dd = atoi(&(ag[3]));
    int y = atoi(&(ag[6])) + 1900;
    if (y < 1950) {
      y += 100;
    }
    tm newTime{};
    if ((m == 2 || m == 9 || m == 4 || m == 6 || m == 11) && dd >= 31 ||
        m == 2 && (y % 4 != 0 && dd == 29 || dd == 30) ||
        dd > 31 || (m == 0 || y == 0 || dd == 0) ||
        (m > 12 || dd > 31)) {
      bout.nl();
      bout << fmt::sprintf("|#6%02d/%02d/%02d is invalid... date not changed!\r\n", m, dd,
                           (y % 100));
      bout.nl();
    } else {
      // Rushfan - Note, this needs a better fix, this whole routine should be replaced.
      newTime.tm_min = 0;
      newTime.tm_hour = 1;
      newTime.tm_sec = 0;
      newTime.tm_year = y - 1900;
      newTime.tm_mday = dd;
      newTime.tm_mon = m - 1;
    }
    bout.nl();
    auto dt = DateTime::from_time_t(mktime(&newTime));
    a()->context().nscandate(dt.to_daten_t());

    // Display the new nscan date
    bout << "|#9New Limiting Date: |#2" << dt.to_string("%m/%d/%Y") << "\r\n";

    // Hack to make sure the date covers everything since we had to increment the hour by one
    // to show the right date on some versions of MSVC
    a()->context().nscandate(a()->context().nscandate() - SECONDS_PER_HOUR);
  } else {
    bout.nl();
  }
}


void removefilesnotthere(int dn, int* autodel) {
  char ch;
  dliscan1(dn);
  const auto all_files = aligns("*.*");
  int i = recno(all_files);
  bool abort = false;
  while (!a()->hangup_ && i > 0 && !abort) {
    auto* area = a()->current_file_area();
    auto f = area->ReadFile(i);
    auto candidate_fn = FilePath(a()->dirs()[dn].path, f);
    if (!File::Exists(candidate_fn)) {
      StringTrim(f.u().description);
      candidate_fn = fmt::sprintf("|#2%s :|#1 %-40.40s", f.aligned_filename(), f.description());
      if (!*autodel) {
        bout.backline();
        bout << candidate_fn;
        bout.nl();
        bout << "|#5Remove Entry (Yes/No/Quit/All) : ";
        ch = onek_ncr("QYNA");
      } else {
        bout.nl();
        bout << "|#1Removing entry " << candidate_fn;
        ch = 'Y';
      }
      if (ch == 'Y' || ch == 'A') {
        if (ch == 'A') {
          bout << "ll";
          *autodel = 1;
        }
        sysoplog() << "- '" << f << "' Removed from " << a()->dirs()[dn].name;
        if (area->DeleteFile(f, i)) {
          area->Save();
        }
        --i;
      } else if (ch == 'Q') {
        abort = true;
      }
    }
    i = nrecno(all_files, i);
    bool next = true;
    checka(&abort, &next);
    if (!next) {
      i = 0;
    }
  }
}

void removenotthere() {
  if (!so()) {
    return;
  }

  tmp_disable_conf(true);
  TempDisablePause disable_pause;
  int autodel = 0;
  bout.nl();
  bout << "|#5Remove N/A files in all directories? ";
  if (yesno()) {
    for (auto i = 0; i < a()->dirs().size() && a()->udir[i].subnum != -1 &&
                     !a()->localIO()->KeyPressed(); i++) {
      bout.nl();
      bout << "|#1Removing N/A|#0 in " << a()->dirs()[a()->udir[i].subnum].name;
      bout.nl(2);
      removefilesnotthere(a()->udir[i].subnum, &autodel);
    }
  } else {
    bout.nl();
    bout << "Removing N/A|#0 in " << a()->dirs()[a()->current_user_dir().subnum].name;
    bout.nl(2);
    removefilesnotthere(a()->current_user_dir().subnum, &autodel);
  }
  tmp_disable_conf(false);
  a()->UpdateTopScreen();
}

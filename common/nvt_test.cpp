/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5.x                            */
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
/**************************************************************************/
#include "common/input_range.h"

#include "fmt/format.h"
#include "gtest/gtest.h"
#include "common/nvt.h"

#include <deque>
#include <string>

using namespace wwiv::common;
using namespace testing;

TEST(NvtTest, ParseSeq_WillEcho) {
  Nvt nvt;
  auto seq = nvt.parse_sequence("\xFB\x01");
  ASSERT_TRUE(seq.has_value());

  auto v = seq.value();
  EXPECT_EQ(v.cmd, Nvt::Command::WILL);
  EXPECT_EQ(v.opt, Nvt::Option::ECHO);
}

TEST(NvtTest, ProcessCommand_EnableSmoke) {
  Nvt nvt;
  nvt.process_command(Nvt::Command::WILL, Nvt::Option::ECHO, "");
  EXPECT_FALSE(nvt.enabled(Nvt::Option::ECHO));
  nvt.process_command(Nvt::Command::DO, Nvt::Option::ECHO, "");
  EXPECT_TRUE(nvt.enabled(Nvt::Option::ECHO));

  nvt.process_command(Nvt::Command::DONT, Nvt::Option::ECHO, "");
  EXPECT_FALSE(nvt.enabled(Nvt::Option::ECHO));
}

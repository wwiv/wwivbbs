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
#ifndef INCLUDED_BBS_MENUS_CONFIG_MENUS_H
#define INCLUDED_BBS_MENUS_CONFIG_MENUS_H

#include "core/stl.h"
#include "core/textfile.h"
#include <map>
#include <string>

namespace wwiv::bbs::menus {


class MenuDescriptions {
public:
  explicit MenuDescriptions(const std::filesystem::path& menupath);
  ~MenuDescriptions();
  [[nodiscard]] std::string description(const std::string& name) const;
  bool set_description(const std::string& name, const std::string& description);

private:
  const std::filesystem::path menupath_;
  std::map<std::string, std::string, wwiv::stl::ci_less> descriptions_;
};

// Functions used b bbs.cpp and defaults.cpp
void ConfigUserMenuSet();

// Functions used by menu-edit and menu
void MenuSysopLog(const std::string& pszMsg);

}  // namespace

#endif
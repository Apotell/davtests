/*
 Copyright 2020 Apotell

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

// Definition location of a module, interface, program or package
// (tests/InstanceDefLocation/dut.sv):
//
//   IEEE 1800-2023 Sec 37.10  instance -> int vpiDefLineNo, str vpiDefFile
//                             ("definition location").
//
// NOT CHECKED: the `line remapping of detail 8 (Sec 22.12).

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/program.h>

#include <gtest/gtest.h>

#include <string_view>

namespace hlc {

class InstanceDefLocationTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "InstanceDefLocation.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static bool isDutFile(std::string_view path) {
    const std::string_view suffix = "/dut.sv";
    return (path.size() >= suffix.size()) && (path.substr(path.size() - suffix.size()) == suffix);
  }
};

TEST_F(InstanceDefLocationTest, ModuleDefLocation) {
  const hldb::Module *const m = hldb::findByName<hldb::Module>("mod_def", m_design->getAllModules());
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(isDutFile(m->getDefFile())) << m->getDefFile();
  EXPECT_EQ(m->getDefLineNo(), 4);
}

TEST_F(InstanceDefLocationTest, InterfaceDefLocation) {
  const hldb::Interface *const i = hldb::findByName<hldb::Interface>("ifc_def", m_design->getAllInterfaces());
  ASSERT_NE(i, nullptr);
  EXPECT_TRUE(isDutFile(i->getDefFile())) << i->getDefFile();
  EXPECT_EQ(i->getDefLineNo(), 7);
}

TEST_F(InstanceDefLocationTest, ProgramDefLocation) {
  const hldb::Program *const p = hldb::findByName<hldb::Program>("prg_def", m_design->getAllPrograms());
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(isDutFile(p->getDefFile())) << p->getDefFile();
  EXPECT_EQ(p->getDefLineNo(), 11);
}

TEST_F(InstanceDefLocationTest, PackageDefLocation) {
  const hldb::Package *const p = hldb::findByName<hldb::Package>("pkg_def", m_design->getAllPackages());
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(isDutFile(p->getDefFile())) << p->getDefFile();
  EXPECT_EQ(p->getDefLineNo(), 14);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

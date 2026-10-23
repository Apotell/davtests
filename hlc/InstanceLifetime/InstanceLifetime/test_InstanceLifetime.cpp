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

// Default lifetime of an interface or program (tests/InstanceLifetime/dut.sv):
//
//   IEEE 1800-2023 Sec 37.10  instance -> bool vpiAutomatic
//   IEEE 1800-2023 Sec 6.21   the lifetime in a module, interface or program header sets
//                             the default lifetime of its subroutines; static when omitted.
//
// NOT CHECKED: packages. Sec 26.2 allows "package automatic p;", but this parser's
// package_declaration rule has no lifetime, so there is nothing to build a check from.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/program.h>

#include <gtest/gtest.h>

namespace hlc {

class InstanceLifetimeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "InstanceLifetime.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::Interface *findInterface(std::string_view name) const {
    return hldb::findByName<hldb::Interface>(name, m_design->getAllInterfaces());
  }
  const hldb::Program *findProgram(std::string_view name) const {
    return hldb::findByName<hldb::Program>(name, m_design->getAllPrograms());
  }
};

TEST_F(InstanceLifetimeTest, AutomaticInterfaceIsAutomatic) {
  const hldb::Interface *const i = findInterface("ifc_auto");
  ASSERT_NE(i, nullptr);
  EXPECT_TRUE(i->getAutomatic());
}

TEST_F(InstanceLifetimeTest, StaticAndDefaultInterfacesAreNotAutomatic) {
  const hldb::Interface *const s = findInterface("ifc_static");
  ASSERT_NE(s, nullptr);
  EXPECT_FALSE(s->getAutomatic());
  const hldb::Interface *const d = findInterface("ifc_default");
  ASSERT_NE(d, nullptr);
  EXPECT_FALSE(d->getAutomatic());
}

TEST_F(InstanceLifetimeTest, AutomaticProgramIsAutomatic) {
  const hldb::Program *const p = findProgram("prg_auto");
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->getAutomatic());
}

TEST_F(InstanceLifetimeTest, StaticAndDefaultProgramsAreNotAutomatic) {
  const hldb::Program *const s = findProgram("prg_static");
  ASSERT_NE(s, nullptr);
  EXPECT_FALSE(s->getAutomatic());
  const hldb::Program *const d = findProgram("prg_default");
  ASSERT_NE(d, nullptr);
  EXPECT_FALSE(d->getAutomatic());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

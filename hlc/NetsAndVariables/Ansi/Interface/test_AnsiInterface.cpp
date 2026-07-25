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

// Validates the UHDM graph produced for tests/NetsAndVariables/Ansi/Interface.sv,
// split out of the combined NetsAndVariablesAnsi.sv suite so the file-scope
// interface testing point stands on its own.
//
// Checked:
//   - work@nets_and_variables_if exists
//   - it has a single modport "mp"

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/modport.h>

namespace hlc {

class AnsiInterfaceTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "AnsiInterface.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(AnsiInterfaceTest, InterfaceExists) {
  ASSERT_NE(m_design->getAllInterfaces(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Interface>("work@nets_and_variables_if", m_design->getAllInterfaces()), nullptr);
}

TEST_F(AnsiInterfaceTest, InterfaceHasOneModport) {
  const hldb::Interface *const iface =
      hldb::findByName<hldb::Interface>("work@nets_and_variables_if", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getModports(), nullptr);
  ASSERT_EQ(iface->getModports()->size(), 1u);
  EXPECT_EQ(iface->getModports()->at(0)->getName(), "mp");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

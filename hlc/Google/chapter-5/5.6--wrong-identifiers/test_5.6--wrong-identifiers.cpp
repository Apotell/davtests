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

// Validates that identifiers with illegal first characters are rejected.
// The SV file deliberately uses two invalid identifiers:
//   reg $dollar;   — '$' is not a legal first character (only legal mid-name)
//   reg 0number;   — digit is not a legal first character
//
// Expected compiler behaviour (confirmed from UHDM log):
//   - 4 syntax errors emitted (SNT:PA0207)
//   - The module declaration fails to parse cleanly; UHDM contains 2 nameless
//     stub Module nodes (fragments from the broken parse), neither with nets.
//   - 'identifiers' does NOT appear — the module name was never captured.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>

namespace hlc {

class WrongIdentifiers : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.6--wrong-identifiers.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// The module name 'identifiers' was never captured due to parse failure.
// ---------------------------------------------------------------------------
TEST_F(WrongIdentifiers, NoModuleNamedIdentifiers) {
  EXPECT_EQ(hldb::findByName<hldb::Module>("identifiers", m_design->getAllModules()), nullptr)
      << "'identifiers' should not exist — parse failure swallowed the name";
}

// ---------------------------------------------------------------------------
// UHDM contains 2 nameless stub modules produced by the broken parse.
// ---------------------------------------------------------------------------
TEST_F(WrongIdentifiers, TwoStubModulesExist) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  EXPECT_EQ(m_design->getAllModules()->size(), 2u) << "broken parse should produce exactly 2 nameless stub modules";
}

TEST_F(WrongIdentifiers, StubModulesHaveNoName) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  for (const hldb::Module *const m : *m_design->getAllModules()) {
    EXPECT_TRUE(m->getName().empty()) << "stub module should have an empty name, got: " << m->getName();
  }
}

// ---------------------------------------------------------------------------
// Neither stub module contains nets — the invalid reg declarations were
// rejected along with the rest of the broken module body.
// ---------------------------------------------------------------------------
TEST_F(WrongIdentifiers, NoNetsInStubModules) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  for (const hldb::Module *const m : *m_design->getAllModules()) {
    EXPECT_TRUE(!m->getNets() || m->getNets()->empty()) << "stub module should have no nets";
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

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

// Validates the parse behaviour for an illegal negative decimal literal.
//
// SV source (module top):
//   logic [7:0] a;
//   initial begin
//     a = 8'd-6;   // ILLEGAL: minus must precede the literal: -8'd6
//   end
//
// The '-' in '8'd-6' is not valid SV syntax; the token '8'd' is a size+base
// specifier and cannot be followed by a minus sign.  The correct negation
// form is '-8'd6' (unary minus applied to the whole sized constant).
//
// Surelog issues 2 syntax errors and the parse collapses: the module body is
// never completed, producing 2 nameless stub modules — the same broken-parse
// pattern seen in other illegal-syntax tests.
//
// UHDM:
//   vpiAllModules: 2 nameless stub modules (no name, no nets, no processes)
//   No module named "work@top"

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/design.h>
#include <uhdm/module.h>

namespace SURELOG {

class IntegersSignedIllegal : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.7.1--integers-signed-illegal.hlc"});

    // Compilation runs despite the syntax errors; m_design is still populated.
    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

// ---------------------------------------------------------------------------
// '8'd-6' is illegal: the module body cannot be parsed, so no named module
// reaches UHDM.
// ---------------------------------------------------------------------------
TEST_F(IntegersSignedIllegal, NoModuleNamedTop) {
  EXPECT_EQ(
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()),
      nullptr)
      << "'work@top' should not exist — syntax error broke the parse";
}

// ---------------------------------------------------------------------------
// The broken parse leaves 2 nameless stub module fragments in UHDM.
// ---------------------------------------------------------------------------
TEST_F(IntegersSignedIllegal, TwoStubModulesExist) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  EXPECT_EQ(m_design->getAllModules()->size(), 2u)
      << "broken parse should produce exactly 2 nameless stub modules";
}

TEST_F(IntegersSignedIllegal, StubModulesHaveNoName) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  for (const uhdm::Module *const m : *m_design->getAllModules()) {
    EXPECT_TRUE(m->getName().empty())
        << "stub module should have an empty name, got: " << m->getName();
  }
}

// ---------------------------------------------------------------------------
// Neither stub module contains nets — the variable declaration for 'a' was
// parsed into the global scope, not into any module.
// ---------------------------------------------------------------------------
TEST_F(IntegersSignedIllegal, NoNetsInStubModules) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  for (const uhdm::Module *const m : *m_design->getAllModules()) {
    EXPECT_TRUE(!m->getNets() || m->getNets()->empty())
        << "stub module should have no nets";
  }
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

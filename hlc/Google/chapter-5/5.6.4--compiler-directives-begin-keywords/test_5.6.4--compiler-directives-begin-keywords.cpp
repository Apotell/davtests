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

// Validates that `begin_keywords / `end_keywords compiler directives correctly
// change the active keyword set.
//
// SV source:
//   `begin_keywords "1364-2001"
//   module b_kw();
//     reg logic;  // OK: "logic" is not a keyword in IEEE 1364-2001
//   endmodule
//   `end_keywords
//
// UHDM structure:
//   Module name:work@b_kw
//     vpiNet (1 item): Net "logic"  ← legal identifier under 1364-2001 rules
//
// Key assertion: the compiler accepted "logic" as a plain identifier name
// (not a type keyword), confirming that `begin_keywords switched the ruleset.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>

namespace hlc {

class CompilerDirectivesBeginKeywords : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__,
            {"-f", "5.6.4--compiler-directives-begin-keywords.hlc"});

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

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@b_kw", d->getAllModules());
}

// ---------------------------------------------------------------------------
// Module
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesBeginKeywords, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@b_kw' not found";
}

// ---------------------------------------------------------------------------
// Net named "logic"
// Under `begin_keywords "1364-2001", "logic" is not a reserved keyword, so
// it is accepted as an ordinary identifier and becomes a net name.
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesBeginKeywords, OneNetExists) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 1u);
}

TEST_F(CompilerDirectivesBeginKeywords, NetIsNamedLogic) {
  // "logic" would be a type keyword under SystemVerilog rules, but the
  // `begin_keywords "1364-2001" directive reverts to Verilog-2001 keywords
  // where "logic" has no special meaning.
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  ASSERT_EQ(m->getNets()->size(), 1u);
  EXPECT_EQ((*m->getNets())[0]->getName(), "logic");
}

// ---------------------------------------------------------------------------
// No processes — the module body contains only the one net declaration
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesBeginKeywords, NoProcesses) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getProcesses() || m->getProcesses()->empty());
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

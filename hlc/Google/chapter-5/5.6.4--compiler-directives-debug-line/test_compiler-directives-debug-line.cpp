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

// Validates that the `line compiler directive is silently consumed by the
// preprocessor and does not corrupt the module parse.
//
// SV source:
//   module directives();
//     `line 1 "5.6.4--compiler-directives-debug-line.sv" 1
//   endmodule
//
// The `line directive resets the compiler's internal source-location counter
// (line number + filename) used for diagnostics.  It produces no UHDM nodes.
//
// UHDM structure:
//   Module name:work@directives  — one empty module, no nets, no processes

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/design.h>
#include <uhdm/module.h>

namespace SURELOG {

class CompilerDirectivesDebugLine : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__,
            {"-f", "5.6.4--compiler-directives-debug-line.hlc"});

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

static const uhdm::Module *getTop(const uhdm::Design *d) {
  return uhdm::findByName<uhdm::Module>("work@directives", d->getAllModules());
}

// ---------------------------------------------------------------------------
// The `line directive is consumed without error; the module compiles cleanly.
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesDebugLine, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr)
      << "module 'work@directives' not found";
}

TEST_F(CompilerDirectivesDebugLine, NoNets) {
  const uhdm::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getNets() || m->getNets()->empty())
      << "`line directive should produce no net declarations";
}

TEST_F(CompilerDirectivesDebugLine, NoProcesses) {
  const uhdm::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getProcesses() || m->getProcesses()->empty())
      << "`line directive should produce no processes";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

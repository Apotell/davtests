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

// Validates that `default_nettype directives are recognised and that their
// effect is visible in UHDM via the vpiDefNetType property.
//
// SV source:
//   `default_nettype wire   // directive 1: switch to wire
//   `default_nettype none   // directive 2: disable implicit nets
//   module dn();
//   endmodule
//
// UHDM observations:
//   SourceFile::getDefNetType() == 1  (wire) — first directive in the file
//   Module::getDefNetType()    == 12 (none) — last active directive at
//                                              module compile time
//
// vpiDefNetType integer values (from VPI standard):
//   wire = 1,  none = 12

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/source_file.h>

namespace hlc {

class CompilerDirectivesDefaultNettype : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.6.4--compiler-directives-default-nettype.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@dn", d->getAllModules());
}

// ---------------------------------------------------------------------------
// Module
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesDefaultNettype, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@dn' not found";
}

TEST_F(CompilerDirectivesDefaultNettype, ModuleIsEmpty) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getNets() || m->getNets()->empty());
  EXPECT_TRUE(!m->getProcesses() || m->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// Module defNetType — reflects `default_nettype none (last active directive)
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesDefaultNettype, ModuleDefNetTypeIsNone) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  // vpiNone = 12; `default_nettype none was active when 'dn' was compiled.
  EXPECT_EQ(m->getDefNetType(), 12) << "module defNetType should be none (12) — last active directive";
}

// ---------------------------------------------------------------------------
// SourceFile defNetType — reflects `default_nettype wire (first directive)
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesDefaultNettype, SourceFileDefNetTypeIsWire) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  ASSERT_FALSE(m_design->getSourceFiles()->empty());
  const hldb::SourceFile *const sf = (*m_design->getSourceFiles())[0];
  ASSERT_NE(sf, nullptr);
  // vpiWire = 1; the first `default_nettype wire is captured on the SourceFile.
  EXPECT_EQ(sf->getDefNetType(), 1) << "source file defNetType should be wire (1) — first directive";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

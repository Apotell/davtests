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

// Validates that the `timescale directive is parsed and its values are
// propagated to both the SourceFile and the compiled Module in UHDM.
//
// SV source:
//   `timescale 1 ns / 1 ps
//   module ts();
//   endmodule
//
// UHDM encodes time values as powers-of-10 exponents (SI notation):
//   1 ns = 10^-9  → vpiTimeUnit      = -9
//   1 ps = 10^-12 → vpiTimePrecision = -12
//
// Both the SourceFile and the Module receive the same timescale values.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/source_file.h>

namespace hlc {

class CompilerDirectivesTimescale : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.6.4--compiler-directives-timescale.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("ts", d->getAllModules());
}

static const hldb::SourceFile *getSourceFile(const hldb::Design *d) {
  if (!d->getSourceFiles() || d->getSourceFiles()->empty()) return nullptr;
  return (*d->getSourceFiles())[0];
}

// ---------------------------------------------------------------------------
// Module
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesTimescale, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'ts' not found";
}

TEST_F(CompilerDirectivesTimescale, ModuleIsEmpty) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getNets() || m->getNets()->empty());
  EXPECT_TRUE(!m->getProcesses() || m->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// Module timescale — `timescale 1 ns / 1 ps
// Time values are stored as powers-of-10 exponents:
//   1 ns → -9   (10^-9 seconds)
//   1 ps → -12  (10^-12 seconds)
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesTimescale, ModuleTimeUnitIsNanosecond) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getTimeUnit(), -9) << "time unit should be -9 (1 ns = 10^-9 s)";
}

TEST_F(CompilerDirectivesTimescale, ModuleTimePrecisionIsPicosecond) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getTimePrecision(), -12) << "time precision should be -12 (1 ps = 10^-12 s)";
}

// ---------------------------------------------------------------------------
// SourceFile timescale — same values propagated to the file scope
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesTimescale, SourceFileTimeUnitIsNanosecond) {
  const hldb::SourceFile *const sf = getSourceFile(m_design);
  ASSERT_NE(sf, nullptr);
  EXPECT_EQ(sf->getTimeUnit(), -9) << "source file time unit should be -9 (1 ns = 10^-9 s)";
}

TEST_F(CompilerDirectivesTimescale, SourceFileTimePrecisionIsPicosecond) {
  const hldb::SourceFile *const sf = getSourceFile(m_design);
  ASSERT_NE(sf, nullptr);
  EXPECT_EQ(sf->getTimePrecision(), -12) << "source file time precision should be -12 (1 ps = 10^-12 s)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

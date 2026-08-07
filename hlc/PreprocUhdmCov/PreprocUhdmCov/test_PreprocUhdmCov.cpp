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

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hlc/ErrorReporting/ErrorContainer.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/identifier.h>
#include <hldb/module.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/source_file.h>

namespace hlc {
class PreprocUhdmCovTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PreprocUhdmCov.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.4: prim_assert.sv is included from dut.sv and must be recorded.
TEST_F(PreprocUhdmCovTest, PrimAssertIncluded) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("prim_assert.sv", sf->getIncludes());
  EXPECT_NE(inc, nullptr) << "prim_assert.sv must be recorded as an include of dut.sv";
}

// LRM 22.5.1: PRIM_ASSERT_SV is a flag macro (include guard) defined in
// prim_assert.sv. Its tokens must be null or empty.
TEST_F(PreprocUhdmCovTest, PrimAssertSvFlagMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("prim_assert.sv", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("PRIM_ASSERT_SV", inc->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "PRIM_ASSERT_SV flag macro must be defined in prim_assert.sv";
  EXPECT_TRUE(macro->getTokens() == nullptr || macro->getTokens()->empty())
      << "PRIM_ASSERT_SV is a flag macro; tokens must be null or empty";
}

// LRM 22.5.1: INC_ASSERT is a flag macro defined in prim_assert.sv.
TEST_F(PreprocUhdmCovTest, IncAssertFlagMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("prim_assert.sv", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("INC_ASSERT", inc->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "INC_ASSERT flag macro must be defined in prim_assert.sv";
  EXPECT_TRUE(macro->getTokens() == nullptr || macro->getTokens()->empty())
      << "INC_ASSERT is a flag macro; tokens must be null or empty";
}

// LRM 22.5.1: ASSERT is a function-like macro defined in prim_assert.sv.
// Its body starts at column 56 (wide arg list).
TEST_F(PreprocUhdmCovTest, AssertFunctionLikeMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("prim_assert.sv", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("ASSERT", inc->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "ASSERT macro must be defined in prim_assert.sv";
}

TEST_F(PreprocUhdmCovTest, AssertMacroBodyColumn) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("prim_assert.sv", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("ASSERT", inc->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getTokens(), nullptr);
  ASSERT_FALSE(macro->getTokens()->empty());
  ASSERT_NE(macro->getTokens()->front(), nullptr);
  EXPECT_EQ(macro->getTokens()->front()->getStartColumn(), 56u) << "ASSERT body starts at column 56";
}

// LRM 22.5.1: _N is defined in dut.sv (two definitions, last wins in
// standard LRM semantics). nameStartColumn=11, bodyStartColumn=19.
TEST_F(PreprocUhdmCovTest, NMacroDefinedInDut) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("_N", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "_N macro must be defined in dut.sv";
}

// LRM 22.5.1(c): redefinition without an intervening `undef is legal (the
// latest definition prevails); dut.sv's two `define _N(stg) ... occurrences
// (lines 18 and 22, identical bodies) must both be recorded, not merged or
// rejected as an error.
TEST_F(PreprocUhdmCovTest, NMacroDefinedTwiceInDut) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getPreprocMacroDefinitions(), nullptr);
  size_t count = 0;
  for (const hldb::PreprocMacroDefinition *const md : *sf->getPreprocMacroDefinitions()) {
    if (md != nullptr && md->getName() == "_N") ++count;
  }
  EXPECT_EQ(count, 2u) << "_N is legally redefined once (no `undef); both definitions must be recorded";
}

// LRM 22.5.1: PRIM_ASSERT_SV and INC_ASSERT are flag macros (no argument list).
TEST_F(PreprocUhdmCovTest, PrimAssertSvHasNoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("prim_assert.sv", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("PRIM_ASSERT_SV", inc->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getArguments() == nullptr || macro->getArguments()->empty())
      << "PRIM_ASSERT_SV is a flag macro; getArguments() must be null or empty";
}

TEST_F(PreprocUhdmCovTest, IncAssertHasNoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("prim_assert.sv", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("INC_ASSERT", inc->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getArguments() == nullptr || macro->getArguments()->empty())
      << "INC_ASSERT is a flag macro; getArguments() must be null or empty";
}

// LRM 22.5.1: ASSERT has four formal parameters:
// __name, __prop, __clk (default cl), __rst (default rs).
TEST_F(PreprocUhdmCovTest, AssertMacroHasFourArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("prim_assert.sv", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("ASSERT", inc->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getArguments(), nullptr);
  EXPECT_EQ(macro->getArguments()->size(), 4u) << "ASSERT has four formal parameters: __name, __prop, __clk, __rst";
}

TEST_F(PreprocUhdmCovTest, AssertMacroHasTokens) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("prim_assert.sv", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("ASSERT", inc->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getTokens(), nullptr);
  EXPECT_FALSE(macro->getTokens()->empty()) << "ASSERT body must have tokens";
}

// ----
// 2. Mismatched end label ("module top ... endmodule : toto")
//
// IEEE 1800-2023 Sec 23.2.1: "If an end label is present, it shall repeat
// the module identifier lexically." dut.sv declares "module top (...)"
// but closes with "endmodule : toto" -- a genuine, deliberate mismatch that
// must be flagged as an error, not silently accepted.
// ----

TEST_F(PreprocUhdmCovTest, MismatchedEndLabelIsReportedAsError) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 1) << "'module top ... endmodule : toto' must be reported as exactly one error "
                                 "(mismatched end label)";
}

TEST_F(PreprocUhdmCovTest, TopModuleEndLabelIsRecordedDespiteMismatch) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' must still compile despite the end-label error";
  EXPECT_EQ(top->getEndLabel(), "toto") << "the (mismatched) end label text must still be recorded verbatim";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

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
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/identifier.h>
#include <hldb/module.h>
#include <hldb/param_assign.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/ref_obj.h>
#include <hldb/source_file.h>

namespace hlc {
class ParamFileMacroTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamFileMacro.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.5.1: both modules that rely on the PARAM_FILE macro must compile.
TEST_F(ParamFileMacroTest, DutModuleCompiles) {
  const hldb::Module *const module = hldb::findByDefName<hldb::Module>("dut", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'dut' must compile";
}

TEST_F(ParamFileMacroTest, Ram1pModuleCompiles) {
  const hldb::Module *const module = hldb::findByDefName<hldb::Module>("ram_1p", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'ram_1p' must compile";
}

// LRM 22.5.1: PARAM_FILE is an object-like macro that expands to a path
// string. nameStartColumn=9, bodyStartColumn=20 (after `PARAM_FILE ').
TEST_F(ParamFileMacroTest, ParamFileMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("PARAM_FILE", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "PARAM_FILE must be defined in dut.sv";
}

TEST_F(ParamFileMacroTest, ParamFileMacroHasBody) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("PARAM_FILE", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getNameObj(), nullptr);
  EXPECT_EQ(macro->getNameObj()->getStartColumn(), 9u) << "PARAM_FILE name starts at column 9";
  ASSERT_NE(macro->getTokens(), nullptr);
  ASSERT_FALSE(macro->getTokens()->empty());
  ASSERT_NE(macro->getTokens()->front(), nullptr);
  EXPECT_EQ(macro->getTokens()->front()->getStartColumn(), 20u) << "PARAM_FILE body starts at column 20";
}

// LRM 22.5.1: PARAM_FILE is object-like (no argument list).
TEST_F(ParamFileMacroTest, ParamFileMacroHasNoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("PARAM_FILE", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getArguments() == nullptr || macro->getArguments()->empty())
      << "PARAM_FILE is object-like; getArguments() must be null or empty";
}

TEST_F(ParamFileMacroTest, ParamFileMacroHasTokens) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("PARAM_FILE", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getTokens(), nullptr);
  EXPECT_FALSE(macro->getTokens()->empty()) << "PARAM_FILE body must have at least one token";
}

// ----
// 2. Interaction between the command-line -DPARAM_FILE=/toto/blah
//    pre-definition and dut.sv's own `define PARAM_FILE "" (line 1).
// ----

// IEEE 1800-2023 Sec 22.5.1(c): "Redefinition of text macros is allowed;
// the latest definition of a particular text macro read by the compiler
// prevails". Redefining a command-line -D macro from within the file --
// even with a different body and no intervening `undef -- is legal and
// must not be reported as an error.
TEST_F(ParamFileMacroTest, RedefinitionOfCommandLineMacroIsNotAnError) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "redefining a command-line -D macro from within the file is legal per "
                                 "IEEE 1800-2023 Sec 22.5.1(c) -- it must not be reported as an error";
}

// Only the in-file `define is recorded against dut.sv's PreprocMacroDefinition
// collection; the command-line pre-definition is not a second entry.
TEST_F(ParamFileMacroTest, ExactlyOnePreprocMacroDefinitionRecorded) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getPreprocMacroDefinitions(), nullptr);
  size_t count = 0;
  for (const hldb::PreprocMacroDefinition *const md : *sf->getPreprocMacroDefinitions()) {
    if (md != nullptr && md->getName() == "PARAM_FILE") ++count;
  }
  EXPECT_EQ(count, 1u);
}

// dut's SRAMInitFile parameter is initialized from `PARAM_FILE, which must
// expand using the LATEST definition (the in-file `define PARAM_FILE ""),
// not the earlier command-line value (-DPARAM_FILE=/toto/blah).
TEST_F(ParamFileMacroTest, LatestDefinitionPrevailsOverCommandLineValue) {
  const hldb::Module *const dut = hldb::findByDefName<hldb::Module>("dut", m_design->getAllModules());
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getParamAssigns(), nullptr);
  ASSERT_FALSE(dut->getParamAssigns()->empty());
  const hldb::ParamAssign *const pa = (*dut->getParamAssigns())[0];
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(pa->getRhs());
  ASSERT_NE(rhs, nullptr) << "SRAMInitFile's initializer should be a Constant";
  EXPECT_EQ(rhs->getDecompile(), "\"\"") << "expected the in-file redefinition's empty string, not the "
                                            "command-line value '/toto/blah'";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

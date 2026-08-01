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

// Validates that all legal escaped-identifier forms are accepted and appear
// in UHDM.  In SV, an escaped identifier begins with '\' and ends at the
// first whitespace; the backslash is NOT part of the identifier name.
//
//   SV source           -> UHDM variable name
//   \busa+index         -> "busa+index"
//   \-clock             -> "-clock"
//   \***error-condition*** -> "***error-condition***"
//   \net1/\net2         -> "net1/\net2"
//   \{a,b}              -> "{a,b}"
//   \a*(b+c)            -> "a*(b+c)"
//
// UHDM: Module name:identifiers with 6 Variable nodes, all LogicTypespec.
// No syntax errors -- escaped identifiers may contain any printable character.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/variable.h>

namespace hlc {

class EscapedIdentifiers : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.6.1--escaped-identifiers.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("identifiers", d->getAllModules());
}

static bool hasVariable(const hldb::Module *m, std::string_view name) {
  return hldb::findByName<hldb::Variable>(name, m->getVariables()) != nullptr;
}

// ----
// Module
// ----
TEST_F(EscapedIdentifiers, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'identifiers' not found"; }

TEST_F(EscapedIdentifiers, SixVariablesExist) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getVariables(), nullptr);
  EXPECT_EQ(m->getVariables()->size(), 6u);
}

// None of the 6 declarations use a net-type keyword (all bare `logic`), so
// per IEEE 1800-2023 Sec 6.7/6.8 the module must have no nets.
TEST_F(EscapedIdentifiers, ModuleHasNoNets) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getNets() || m->getNets()->empty()) << "bare 'logic' declarations must not appear as Nets";
}

// ----
// Each escaped identifier stored without the leading backslash
// ----
TEST_F(EscapedIdentifiers, VariableBusaPlusIndex) {
  // \busa+index  ->  "busa+index"
  EXPECT_TRUE(hasVariable(getTop(m_design), "busa+index")) << "Variable 'busa+index' not found";
}

TEST_F(EscapedIdentifiers, VariableMinusClock) {
  // \-clock  ->  "-clock"
  EXPECT_TRUE(hasVariable(getTop(m_design), "-clock")) << "Variable '-clock' not found";
}

TEST_F(EscapedIdentifiers, VariableErrorConditionWithStars) {
  // \***error-condition***  ->  "***error-condition***"
  EXPECT_TRUE(hasVariable(getTop(m_design), "***error-condition***")) << "Variable '***error-condition***' not found";
}

TEST_F(EscapedIdentifiers, VariableVariable1SlashVariable2) {
  // \net1/\net2  ->  "net1/\net2"
  EXPECT_TRUE(hasVariable(getTop(m_design), "net1/\\net2")) << "Variable 'net1/\\net2' not found";
}

TEST_F(EscapedIdentifiers, VariableCurlyAB) {
  // \{a,b}  ->  "{a,b}"
  EXPECT_TRUE(hasVariable(getTop(m_design), "{a,b}")) << "Variable '{a,b}' not found";
}

TEST_F(EscapedIdentifiers, VariableAStarBPlusC) {
  // \a*(b+c)  ->  "a*(b+c)"
  EXPECT_TRUE(hasVariable(getTop(m_design), "a*(b+c)")) << "Variable 'a*(b+c)' not found";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

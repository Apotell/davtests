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

// Validates that all legal SV identifier forms are accepted and appear in UHDM:
//   reg shiftreg_a;      -- underscore in middle
//   reg busa_index;      -- underscore in middle
//   reg error_condition; -- underscore in middle
//   reg merge_ab;        -- underscore in middle
//   reg _bus3;           -- leading underscore
//   reg n$657;           -- dollar sign in identifier
//   reg sensitive;       -- lowercase
//   reg Sensitive;       -- uppercase start (case-distinct from 'sensitive')
//
// UHDM: Module name:identifiers with 8 Variable nodes, all LogicTypespec.
// Case sensitivity: 'sensitive' and 'Sensitive' are distinct variables.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/variable.h>

namespace hlc {

class Identifiers : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.6--identifiers.hlc"}); }
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
TEST_F(Identifiers, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'identifiers' not found"; }

TEST_F(Identifiers, EightVariablesExist) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getVariables(), nullptr);
  EXPECT_EQ(m->getVariables()->size(), 8u);
}

// `reg` is a variable keyword, not a net-type keyword (IEEE 1800-2023 Sec
// 6.7/6.8), so none of these 8 declarations should appear as Nets.
TEST_F(Identifiers, ModuleHasNoNets) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getNets() || m->getNets()->empty()) << "'reg' declarations must not appear as Nets";
}

// ----
// Standard identifiers with underscores
// ----
TEST_F(Identifiers, VariableShiftregA) {
  EXPECT_TRUE(hasVariable(getTop(m_design), "shiftreg_a")) << "Variable 'shiftreg_a' not found";
}

TEST_F(Identifiers, VariableBusaIndex) {
  EXPECT_TRUE(hasVariable(getTop(m_design), "busa_index")) << "Variable 'busa_index' not found";
}

TEST_F(Identifiers, VariableErrorCondition) {
  EXPECT_TRUE(hasVariable(getTop(m_design), "error_condition")) << "Variable 'error_condition' not found";
}

TEST_F(Identifiers, VariableMergeAb) {
  EXPECT_TRUE(hasVariable(getTop(m_design), "merge_ab")) << "Variable 'merge_ab' not found";
}

// ----
// Leading-underscore identifier
// ----
TEST_F(Identifiers, VariableBus3WithLeadingUnderscore) {
  EXPECT_TRUE(hasVariable(getTop(m_design), "_bus3")) << "Variable '_bus3' (leading underscore) not found";
}

// ----
// Dollar-sign identifier
// ----
TEST_F(Identifiers, VariableN657WithDollarSign) {
  EXPECT_TRUE(hasVariable(getTop(m_design), "n$657")) << "Variable 'n$657' (dollar sign) not found";
}

// ----
// Case sensitivity: 'sensitive' and 'Sensitive' are distinct Variables
// ----
TEST_F(Identifiers, VariableSensitiveLowercase) {
  EXPECT_TRUE(hasVariable(getTop(m_design), "sensitive")) << "Variable 'sensitive' (lowercase) not found";
}

TEST_F(Identifiers, VariableSensitiveUppercase) {
  EXPECT_TRUE(hasVariable(getTop(m_design), "Sensitive")) << "Variable 'Sensitive' (uppercase) not found";
}

TEST_F(Identifiers, SensitiveAndSensitiveAreDistinct) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getVariables(), nullptr);

  const hldb::Variable *lower = nullptr;
  const hldb::Variable *upper = nullptr;
  for (const hldb::Variable *const n : *m->getVariables()) {
    if (n->getName() == "sensitive") lower = n;
    if (n->getName() == "Sensitive") upper = n;
  }
  ASSERT_NE(lower, nullptr) << "'sensitive' not found";
  ASSERT_NE(upper, nullptr) << "'Sensitive' not found";
  EXPECT_NE(lower, upper) << "'sensitive' and 'Sensitive' must be distinct variables";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

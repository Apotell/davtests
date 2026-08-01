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

// Validates that the C-like flat struct-array initializer (ms_t ms[1:0] = '{0,0,1,1})
// is accepted by the parser without errors but produces a flat assign-pattern with
// 4 raw Constants rather than 2 nested patterns.
// `ms_t ms[1:0]` has no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8 it
// is a variable_declaration, not a net_declaration.
// :should_fail_because: C-like assignment is illegal (simulation-time failure only).
// Grammar: assignment_pattern with expression_list (4 items) -- no inner '{} per element.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct_typespec.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>

namespace hlc {

class StructuredArraysIllegal : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.10-structure-arrays-illegal.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(StructuredArraysIllegal, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ----
// Typedef and struct -- same structure as the legal variant
// ----
TEST_F(StructuredArraysIllegal, TypedefMsTExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr) << "module has no typespecs";

  const hldb::TypedefTypespec *msT = nullptr;
  for (const hldb::Typespec *const ts : *top->getTypespecs()) {
    if (const hldb::TypedefTypespec *const tdt = any_cast<hldb::TypedefTypespec>(ts)) {
      if (tdt->getName() == "ms_t") {
        msT = tdt;
        break;
      }
    }
  }
  ASSERT_NE(msT, nullptr) << "TypedefTypespec 'ms_t' not found in module";
}

TEST_F(StructuredArraysIllegal, StructHasTwoMembers) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);

  const hldb::TypedefTypespec *msT = nullptr;
  for (const hldb::Typespec *const ts : *top->getTypespecs()) {
    if (const hldb::TypedefTypespec *const tdt = any_cast<hldb::TypedefTypespec>(ts)) {
      if (tdt->getName() == "ms_t") {
        msT = tdt;
        break;
      }
    }
  }
  ASSERT_NE(msT, nullptr);

  const hldb::StructTypespec *const st = any_cast<hldb::StructTypespec>(msT->getTypedefAlias()->getActual());
  ASSERT_NE(st, nullptr) << "ms_t alias does not resolve to a StructTypespec";
  ASSERT_NE(st->getMembers(), nullptr) << "StructTypespec has no members";
  EXPECT_EQ(st->getMembers()->size(), 2u) << "expected 2 struct members (a, b)";
}

// ----
// Variable ms -- still present and array-typed despite the illegal initializer.
// `ms_t ms[1:0]` has no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8 it
// is a variable_declaration, not a net_declaration.
// ----
TEST_F(StructuredArraysIllegal, VariableExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr) << "module has no variables";

  const hldb::Variable *const ms = hldb::findByName<hldb::Variable>("ms", top->getVariables());
  ASSERT_NE(ms, nullptr) << "variable 'ms' not found in module";
}

TEST_F(StructuredArraysIllegal, VariableIsNotDuplicatedAsNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  if (top->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("ms", top->getNets()), nullptr)
        << "'ms' has no net-type keyword and must not also appear as a Net";
  }
}

TEST_F(StructuredArraysIllegal, ArrayVariableTypespecIsArray) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const hldb::Variable *const ms = hldb::findByName<hldb::Variable>("ms", top->getVariables());
  ASSERT_NE(ms, nullptr);
  ASSERT_NE(ms->getTypespec(), nullptr) << "variable 'ms' has no typespec";

  const hldb::ArrayTypespec *const at = any_cast<hldb::ArrayTypespec>(ms->getTypespec()->getActual());
  ASSERT_NE(at, nullptr) << "variable 'ms' typespec is not an ArrayTypespec";
}

// ----
// Initializer shape -- the key distinction from the legal variant:
// '{0, 0, 1, 1} produces a flat assign-pattern with 4 raw Constants,
// not 2 nested patterns. No inner '{} per struct element.
// ----
TEST_F(StructuredArraysIllegal, InitializerIsFlatAssignPattern) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const hldb::Variable *const ms = hldb::findByName<hldb::Variable>("ms", top->getVariables());
  ASSERT_NE(ms, nullptr);
  ASSERT_NE(ms->getValue(), nullptr) << "variable 'ms' has no initializer";

  const hldb::Operation *const init = ms->getValue<hldb::Operation>();
  ASSERT_NE(init, nullptr) << "initializer is not an Operation";
  EXPECT_EQ(init->getOpType(), vpiAssignmentPatternOp) << "initializer op is not vpiAssignmentPatternOp";
}

TEST_F(StructuredArraysIllegal, InitializerHasFourOperands) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const hldb::Variable *const ms = hldb::findByName<hldb::Variable>("ms", top->getVariables());
  ASSERT_NE(ms, nullptr);

  const hldb::Operation *const init = ms->getValue<hldb::Operation>();
  ASSERT_NE(init, nullptr);
  ASSERT_NE(init->getOperands(), nullptr) << "initializer has no operands";
  // '{0, 0, 1, 1} -- flat C-like list: 4 Constants, not 2 nested patterns
  EXPECT_EQ(init->getOperands()->size(), 4u) << "expected 4 flat operands for illegal C-like '{0, 0, 1, 1}";
}

TEST_F(StructuredArraysIllegal, OperandsAreAllConstants) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const hldb::Variable *const ms = hldb::findByName<hldb::Variable>("ms", top->getVariables());
  ASSERT_NE(ms, nullptr);

  const hldb::Operation *const init = ms->getValue<hldb::Operation>();
  ASSERT_NE(init, nullptr);
  ASSERT_NE(init->getOperands(), nullptr);
  ASSERT_EQ(init->getOperands()->size(), 4u);

  for (size_t i = 0; i < 4u; ++i) {
    const hldb::Constant *const c = any_cast<hldb::Constant>((*init->getOperands())[i]);
    EXPECT_NE(c, nullptr) << "operand [" << i << "] is not a Constant -- illegal flat pattern was not preserved";
  }
}

TEST_F(StructuredArraysIllegal, NoNestedAssignPatterns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const hldb::Variable *const ms = hldb::findByName<hldb::Variable>("ms", top->getVariables());
  ASSERT_NE(ms, nullptr);

  const hldb::Operation *const init = ms->getValue<hldb::Operation>();
  ASSERT_NE(init, nullptr);
  ASSERT_NE(init->getOperands(), nullptr);

  for (size_t i = 0; i < init->getOperands()->size(); ++i) {
    const hldb::Operation *const nested = any_cast<hldb::Operation>((*init->getOperands())[i]);
    EXPECT_EQ(nested, nullptr) << "operand [" << i << "] is a nested Operation -- expected a flat list of Constants";
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

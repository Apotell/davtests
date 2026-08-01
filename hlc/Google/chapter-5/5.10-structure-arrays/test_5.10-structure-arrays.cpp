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

// Validates that an array of structs (ms_t ms[1:0]) with an assignment-pattern
// initializer is correctly represented in the UHDM graph.
// SV: typedef struct { int a; int b; } ms_t;  ms_t ms[1:0] = '{'{0,0}, '{1,1}};
// `ms_t ms[1:0]` has no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8 it
// is a variable_declaration, not a net_declaration.
// Maps to: Variable -> ArrayTypespec(elem=ms_t), vpiValue = AssignPatternOp(2 x AssignPatternOp)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
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

class StructuredArrays : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.10-structure-arrays.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(StructuredArrays, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ----
// Typedef ms_t
// ----
TEST_F(StructuredArrays, TypedefMsTExists) {
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

// ----
// Struct members (a, b) via ms_t alias
// ----
TEST_F(StructuredArrays, StructHasTwoMembers) {
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

TEST_F(StructuredArrays, StructMemberNamesAreAandB) {
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
  ASSERT_NE(st, nullptr);
  ASSERT_NE(st->getMembers(), nullptr);
  ASSERT_EQ(st->getMembers()->size(), 2u);

  EXPECT_EQ((*st->getMembers())[0]->getName(), "a") << "first member should be 'a'";
  EXPECT_EQ((*st->getMembers())[1]->getName(), "b") << "second member should be 'b'";
}

// ----
// Variable ms
// ----
// `ms_t ms[1:0]` has no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8 it
// is a variable_declaration, not a net_declaration. It must not also appear
// in the module's net collection.
TEST_F(StructuredArrays, ArrayVariableExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr) << "module has no variables";

  const hldb::Variable *const ms = hldb::findByName<hldb::Variable>("ms", top->getVariables());
  ASSERT_NE(ms, nullptr) << "variable 'ms' not found in module";
}

TEST_F(StructuredArrays, ArrayVariableIsNotDuplicatedAsNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  if (top->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("ms", top->getNets()), nullptr)
        << "'ms' has no net-type keyword and must not also appear as a Net";
  }
}

TEST_F(StructuredArrays, ArrayVariableTypespecIsArray) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const hldb::Variable *const ms = hldb::findByName<hldb::Variable>("ms", top->getVariables());
  ASSERT_NE(ms, nullptr);
  ASSERT_NE(ms->getTypespec(), nullptr) << "variable 'ms' has no typespec";

  const hldb::ArrayTypespec *const at = any_cast<hldb::ArrayTypespec>(ms->getTypespec()->getActual());
  ASSERT_NE(at, nullptr) << "variable 'ms' typespec is not an ArrayTypespec";
}

TEST_F(StructuredArrays, ArrayElemTypespecReferencesMsT) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const hldb::Variable *const ms = hldb::findByName<hldb::Variable>("ms", top->getVariables());
  ASSERT_NE(ms, nullptr);

  const hldb::ArrayTypespec *const at = any_cast<hldb::ArrayTypespec>(ms->getTypespec()->getActual());
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr) << "ArrayTypespec has no element typespec";
  EXPECT_EQ(at->getElemTypespec()->getName(), "ms_t") << "array element typespec does not reference 'ms_t'";
}

TEST_F(StructuredArrays, ArrayHasRange) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const hldb::Variable *const ms = hldb::findByName<hldb::Variable>("ms", top->getVariables());
  ASSERT_NE(ms, nullptr);

  const hldb::ArrayTypespec *const at = any_cast<hldb::ArrayTypespec>(ms->getTypespec()->getActual());
  ASSERT_NE(at, nullptr);
  EXPECT_NE(at->getRange(), nullptr) << "ArrayTypespec has no range (expected [1:0])";
}

// ----
// Initializer: '{'{0,0}, '{1,1}}
// ----
TEST_F(StructuredArrays, ArrayVariableHasInitializer) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const hldb::Variable *const ms = hldb::findByName<hldb::Variable>("ms", top->getVariables());
  ASSERT_NE(ms, nullptr);
  EXPECT_NE(ms->getValue(), nullptr) << "variable 'ms' has no initializer";
}

TEST_F(StructuredArrays, InitializerIsAssignPattern) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const hldb::Variable *const ms = hldb::findByName<hldb::Variable>("ms", top->getVariables());
  ASSERT_NE(ms, nullptr);

  const hldb::Operation *const init = ms->getValue<hldb::Operation>();
  ASSERT_NE(init, nullptr) << "initializer is not an Operation";
  EXPECT_EQ(init->getOpType(), vpiAssignmentPatternOp)
      << "initializer op is not vpiAssignmentPatternOp (expected '{...})";
}

TEST_F(StructuredArrays, InitializerHasTwoElements) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const hldb::Variable *const ms = hldb::findByName<hldb::Variable>("ms", top->getVariables());
  ASSERT_NE(ms, nullptr);

  const hldb::Operation *const init = ms->getValue<hldb::Operation>();
  ASSERT_NE(init, nullptr);
  ASSERT_NE(init->getOperands(), nullptr) << "initializer has no operands";
  EXPECT_EQ(init->getOperands()->size(), 2u) << "expected 2 elements in outer assign pattern ('{0,0} and '{1,1})";
}

TEST_F(StructuredArrays, EachElementIsAssignPattern) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const hldb::Variable *const ms = hldb::findByName<hldb::Variable>("ms", top->getVariables());
  ASSERT_NE(ms, nullptr);

  const hldb::Operation *const init = ms->getValue<hldb::Operation>();
  ASSERT_NE(init, nullptr);
  ASSERT_NE(init->getOperands(), nullptr);
  ASSERT_EQ(init->getOperands()->size(), 2u);

  for (size_t i = 0; i < 2u; ++i) {
    const hldb::Operation *const elem = any_cast<hldb::Operation>((*init->getOperands())[i]);
    ASSERT_NE(elem, nullptr) << "element [" << i << "] is not an Operation";
    EXPECT_EQ(elem->getOpType(), vpiAssignmentPatternOp)
        << "element [" << i << "] is not vpiAssignmentPatternOp (expected '{a, b})";
  }
}

TEST_F(StructuredArrays, EachElementHasTwoFields) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const hldb::Variable *const ms = hldb::findByName<hldb::Variable>("ms", top->getVariables());
  ASSERT_NE(ms, nullptr);

  const hldb::Operation *const init = ms->getValue<hldb::Operation>();
  ASSERT_NE(init, nullptr);
  ASSERT_NE(init->getOperands(), nullptr);
  ASSERT_EQ(init->getOperands()->size(), 2u);

  for (size_t i = 0; i < 2u; ++i) {
    const hldb::Operation *const elem = any_cast<hldb::Operation>((*init->getOperands())[i]);
    ASSERT_NE(elem, nullptr);
    ASSERT_NE(elem->getOperands(), nullptr) << "element [" << i << "] has no operands";
    EXPECT_EQ(elem->getOperands()->size(), 2u) << "element [" << i << "] should have 2 field values (for 'a' and 'b')";
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

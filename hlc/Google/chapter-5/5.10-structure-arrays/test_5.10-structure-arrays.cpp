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
// Maps to: Variable → ArrayTypespec(elem=ms_t), vpiValue = AssignPatternOp(2 x AssignPatternOp)

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/array_typespec.h>
#include <uhdm/design.h>
#include <uhdm/module.h>
#include <uhdm/operation.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/struct_typespec.h>
#include <uhdm/typedef_typespec.h>
#include <uhdm/typespec_member.h>
#include <uhdm/variable.h>

namespace SURELOG {

class StructuredArrays : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.10-structure-arrays.hlc"});

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

TEST_F(StructuredArrays, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Typedef ms_t
// ---------------------------------------------------------------------------
TEST_F(StructuredArrays, TypedefMsTExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr) << "module has no typespecs";

  const uhdm::TypedefTypespec *msT = nullptr;
  for (const uhdm::Typespec *const ts : *top->getTypespecs()) {
    if (const uhdm::TypedefTypespec *const tdt = any_cast<uhdm::TypedefTypespec>(ts)) {
      if (tdt->getName() == "ms_t") { msT = tdt; break; }
    }
  }
  ASSERT_NE(msT, nullptr) << "TypedefTypespec 'ms_t' not found in module";
}

// ---------------------------------------------------------------------------
// Struct members (a, b) via ms_t alias
// ---------------------------------------------------------------------------
TEST_F(StructuredArrays, StructHasTwoMembers) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);

  const uhdm::TypedefTypespec *msT = nullptr;
  for (const uhdm::Typespec *const ts : *top->getTypespecs()) {
    if (const uhdm::TypedefTypespec *const tdt = any_cast<uhdm::TypedefTypespec>(ts)) {
      if (tdt->getName() == "ms_t") { msT = tdt; break; }
    }
  }
  ASSERT_NE(msT, nullptr);

  const uhdm::StructTypespec *const st =
      any_cast<uhdm::StructTypespec>(msT->getTypedefAlias()->getActual());
  ASSERT_NE(st, nullptr) << "ms_t alias does not resolve to a StructTypespec";
  ASSERT_NE(st->getMembers(), nullptr) << "StructTypespec has no members";
  EXPECT_EQ(st->getMembers()->size(), 2u) << "expected 2 struct members (a, b)";
}

TEST_F(StructuredArrays, StructMemberNamesAreAandB) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);

  const uhdm::TypedefTypespec *msT = nullptr;
  for (const uhdm::Typespec *const ts : *top->getTypespecs()) {
    if (const uhdm::TypedefTypespec *const tdt = any_cast<uhdm::TypedefTypespec>(ts)) {
      if (tdt->getName() == "ms_t") { msT = tdt; break; }
    }
  }
  ASSERT_NE(msT, nullptr);

  const uhdm::StructTypespec *const st =
      any_cast<uhdm::StructTypespec>(msT->getTypedefAlias()->getActual());
  ASSERT_NE(st, nullptr);
  ASSERT_NE(st->getMembers(), nullptr);
  ASSERT_EQ(st->getMembers()->size(), 2u);

  EXPECT_EQ((*st->getMembers())[0]->getName(), "a") << "first member should be 'a'";
  EXPECT_EQ((*st->getMembers())[1]->getName(), "b") << "second member should be 'b'";
}

// ---------------------------------------------------------------------------
// Variable ms
// ---------------------------------------------------------------------------
TEST_F(StructuredArrays, ArrayVarExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr) << "module has no variables";

  const uhdm::Variable *ms = nullptr;
  for (const uhdm::Variable *const v : *top->getVariables()) {
    if (v->getName() == "ms") { ms = v; break; }
  }
  ASSERT_NE(ms, nullptr) << "variable 'ms' not found in module";
}

TEST_F(StructuredArrays, ArrayVarTypespecIsArray) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const uhdm::Variable *ms = nullptr;
  for (const uhdm::Variable *const v : *top->getVariables()) {
    if (v->getName() == "ms") { ms = v; break; }
  }
  ASSERT_NE(ms, nullptr);
  ASSERT_NE(ms->getTypespec(), nullptr) << "variable 'ms' has no typespec";

  const uhdm::ArrayTypespec *const at =
      any_cast<uhdm::ArrayTypespec>(ms->getTypespec()->getActual());
  ASSERT_NE(at, nullptr) << "variable 'ms' typespec is not an ArrayTypespec";
}

TEST_F(StructuredArrays, ArrayElemTypespecReferencesMsT) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const uhdm::Variable *ms = nullptr;
  for (const uhdm::Variable *const v : *top->getVariables()) {
    if (v->getName() == "ms") { ms = v; break; }
  }
  ASSERT_NE(ms, nullptr);

  const uhdm::ArrayTypespec *const at =
      any_cast<uhdm::ArrayTypespec>(ms->getTypespec()->getActual());
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr) << "ArrayTypespec has no element typespec";
  EXPECT_EQ(at->getElemTypespec()->getName(), "ms_t")
      << "array element typespec does not reference 'ms_t'";
}

TEST_F(StructuredArrays, ArrayHasRange) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const uhdm::Variable *ms = nullptr;
  for (const uhdm::Variable *const v : *top->getVariables()) {
    if (v->getName() == "ms") { ms = v; break; }
  }
  ASSERT_NE(ms, nullptr);

  const uhdm::ArrayTypespec *const at =
      any_cast<uhdm::ArrayTypespec>(ms->getTypespec()->getActual());
  ASSERT_NE(at, nullptr);
  EXPECT_NE(at->getRange(), nullptr) << "ArrayTypespec has no range (expected [1:0])";
}

// ---------------------------------------------------------------------------
// Initializer: '{'{0,0}, '{1,1}}
// ---------------------------------------------------------------------------
TEST_F(StructuredArrays, ArrayVarHasInitializer) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const uhdm::Variable *ms = nullptr;
  for (const uhdm::Variable *const v : *top->getVariables()) {
    if (v->getName() == "ms") { ms = v; break; }
  }
  ASSERT_NE(ms, nullptr);
  EXPECT_NE(ms->getValue(), nullptr) << "variable 'ms' has no initializer";
}

TEST_F(StructuredArrays, InitializerIsAssignPattern) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const uhdm::Variable *ms = nullptr;
  for (const uhdm::Variable *const v : *top->getVariables()) {
    if (v->getName() == "ms") { ms = v; break; }
  }
  ASSERT_NE(ms, nullptr);

  const uhdm::Operation *const init = ms->getValue<uhdm::Operation>();
  ASSERT_NE(init, nullptr) << "initializer is not an Operation";
  EXPECT_EQ(init->getOpType(), vpiAssignmentPatternOp)
      << "initializer op is not vpiAssignmentPatternOp (expected '{...})";
}

TEST_F(StructuredArrays, InitializerHasTwoElements) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const uhdm::Variable *ms = nullptr;
  for (const uhdm::Variable *const v : *top->getVariables()) {
    if (v->getName() == "ms") { ms = v; break; }
  }
  ASSERT_NE(ms, nullptr);

  const uhdm::Operation *const init = ms->getValue<uhdm::Operation>();
  ASSERT_NE(init, nullptr);
  ASSERT_NE(init->getOperands(), nullptr) << "initializer has no operands";
  EXPECT_EQ(init->getOperands()->size(), 2u)
      << "expected 2 elements in outer assign pattern ('{0,0} and '{1,1})";
}

TEST_F(StructuredArrays, EachElementIsAssignPattern) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const uhdm::Variable *ms = nullptr;
  for (const uhdm::Variable *const v : *top->getVariables()) {
    if (v->getName() == "ms") { ms = v; break; }
  }
  ASSERT_NE(ms, nullptr);

  const uhdm::Operation *const init = ms->getValue<uhdm::Operation>();
  ASSERT_NE(init, nullptr);
  ASSERT_NE(init->getOperands(), nullptr);
  ASSERT_EQ(init->getOperands()->size(), 2u);

  for (size_t i = 0; i < 2u; ++i) {
    const uhdm::Operation *const elem =
        any_cast<uhdm::Operation>((*init->getOperands())[i]);
    ASSERT_NE(elem, nullptr) << "element [" << i << "] is not an Operation";
    EXPECT_EQ(elem->getOpType(), vpiAssignmentPatternOp)
        << "element [" << i << "] is not vpiAssignmentPatternOp (expected '{a, b})";
  }
}

TEST_F(StructuredArrays, EachElementHasTwoFields) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const uhdm::Variable *ms = nullptr;
  for (const uhdm::Variable *const v : *top->getVariables()) {
    if (v->getName() == "ms") { ms = v; break; }
  }
  ASSERT_NE(ms, nullptr);

  const uhdm::Operation *const init = ms->getValue<uhdm::Operation>();
  ASSERT_NE(init, nullptr);
  ASSERT_NE(init->getOperands(), nullptr);
  ASSERT_EQ(init->getOperands()->size(), 2u);

  for (size_t i = 0; i < 2u; ++i) {
    const uhdm::Operation *const elem =
        any_cast<uhdm::Operation>((*init->getOperands())[i]);
    ASSERT_NE(elem, nullptr);
    ASSERT_NE(elem->getOperands(), nullptr) << "element [" << i << "] has no operands";
    EXPECT_EQ(elem->getOperands()->size(), 2u)
        << "element [" << i << "] should have 2 field values (for 'a' and 'b')";
  }
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

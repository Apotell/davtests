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
// :should_fail_because: C-like assignment is illegal (simulation-time failure only).
// Grammar: assignment_pattern with expression_list (4 items) — no inner '{} per element.

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/array_typespec.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/module.h>
#include <uhdm/operation.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/struct_typespec.h>
#include <uhdm/typedef_typespec.h>
#include <uhdm/typespec_member.h>
#include <uhdm/variable.h>

namespace SURELOG {

class StructuredArraysIllegal : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.10-structure-arrays-illegal.hlc"});

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

TEST_F(StructuredArraysIllegal, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Typedef and struct — same structure as the legal variant
// ---------------------------------------------------------------------------
TEST_F(StructuredArraysIllegal, TypedefMsTExists) {
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

TEST_F(StructuredArraysIllegal, StructHasTwoMembers) {
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

// ---------------------------------------------------------------------------
// Variable ms — still present and array-typed despite the illegal initializer
// ---------------------------------------------------------------------------
TEST_F(StructuredArraysIllegal, ArrayVarExists) {
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

TEST_F(StructuredArraysIllegal, ArrayVarTypespecIsArray) {
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

// ---------------------------------------------------------------------------
// Initializer shape — the key distinction from the legal variant:
// '{0, 0, 1, 1} produces a flat assign-pattern with 4 raw Constants,
// not 2 nested patterns. No inner '{} per struct element.
// ---------------------------------------------------------------------------
TEST_F(StructuredArraysIllegal, InitializerIsFlatAssignPattern) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);

  const uhdm::Variable *ms = nullptr;
  for (const uhdm::Variable *const v : *top->getVariables()) {
    if (v->getName() == "ms") { ms = v; break; }
  }
  ASSERT_NE(ms, nullptr);
  ASSERT_NE(ms->getValue(), nullptr) << "variable 'ms' has no initializer";

  const uhdm::Operation *const init = ms->getValue<uhdm::Operation>();
  ASSERT_NE(init, nullptr) << "initializer is not an Operation";
  EXPECT_EQ(init->getOpType(), vpiAssignmentPatternOp)
      << "initializer op is not vpiAssignmentPatternOp";
}

TEST_F(StructuredArraysIllegal, InitializerHasFourOperands) {
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
  // '{0, 0, 1, 1} — flat C-like list: 4 Constants, not 2 nested patterns
  EXPECT_EQ(init->getOperands()->size(), 4u)
      << "expected 4 flat operands for illegal C-like '{0, 0, 1, 1}";
}

TEST_F(StructuredArraysIllegal, OperandsAreAllConstants) {
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
  ASSERT_EQ(init->getOperands()->size(), 4u);

  for (size_t i = 0; i < 4u; ++i) {
    const uhdm::Constant *const c = any_cast<uhdm::Constant>((*init->getOperands())[i]);
    EXPECT_NE(c, nullptr)
        << "operand [" << i << "] is not a Constant — illegal flat pattern was not preserved";
  }
}

TEST_F(StructuredArraysIllegal, NoNestedAssignPatterns) {
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

  for (size_t i = 0; i < init->getOperands()->size(); ++i) {
    const uhdm::Operation *const nested =
        any_cast<uhdm::Operation>((*init->getOperands())[i]);
    EXPECT_EQ(nested, nullptr)
        << "operand [" << i << "] is a nested Operation — expected a flat list of Constants";
  }
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

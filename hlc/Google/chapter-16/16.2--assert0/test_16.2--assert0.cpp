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

// Spec-based validation of IEEE 1800-2023 sec. 16.4 deferred immediate
// assertion.
//
// All expected values are derived from sec. 16.4 of the spec and the SV
// source. No expected value is taken from the UHDM log.
//
// -- sec. 16.4 rules under test ----
//
// sec. 16.4 defines two forms of deferred immediate assertion:
//
//   assert #0 (expr)    -- deferred:  isDeferred=true,  isFinal=false
//   assert final (expr) -- final:     isDeferred=true,  isFinal=true
//
// Rule 1 -- 'assert #0 (expr)' sets isDeferred=true.
//   -> ImmediateAssert::isDeferred must be true.
//   -> ImmediateAssert::isFinal   must be false.
//   (Contrast with sec. 16.2 'assert (expr)': isDeferred=false, isFinal=false.)
//
// Rule 2 -- The '#0' specifies that evaluation is deferred to the
//   Observed/Reactive region at the end of the time step. This tool models
//   the deferral purely via the isDeferred flag; ImmediateAssert::getExpr()
//   returns the assertion expression itself (an Operation for 'a != 0'),
//   not a DelayControl wrapper -- there is no separate node for the '#0'
//   value.
//   -> ImmediateAssert::getExpr<Operation>() is the expression 'a != 0'.
//
// Rule 3 -- sec. 16.4 allows deferred assertions at module scope as a
//   module_common_item (unlike sec. 16.2 simple assertions which are
//   procedural-only).
//   -> The ImmediateAssert is in module::getAssertions(), not getProcesses().
//
// Rule 4 -- No action blocks. The SV source uses the bare form.
//   -> ImmediateAssert::getStmt()     must be null.
//   -> ImmediateAssert::getElseStmt() must be null.
//
// -- Declaration 'logic a = 1;' ----
// No net-type keyword (wire/tri/etc.), not 'interconnect', not a
// user-defined nettype -- per sec. 6.7/6.8 this is a Variable, not a Net,
// regardless of the module's default nettype.
//
// -- SV source ----
//   logic a = 1;
//   assert #0 (a != 0);      <- module-level deferred immediate assertion
//
// -- Spec-correct UHDM ----
//   Variable 'a': LogicTypespec, inline value = 1
//   Module::getAssertions()[0] = ImmediateAssert {    // Rule 3
//     isDeferred = true,            // Rule 1 -- '#0' form
//     isFinal    = false,           // Rule 1 -- not 'final'
//     expr = Operation {            // Rule 2 -- the assertion expression
//       opType:      vpiNeqOp       // sec. 11.4.5: '!='
//       operands[0]: RefObj -> 'a'
//       operands[1]: Constant -> "0"
//     },
//     stmt     = null,              // Rule 4 -- no pass action block
//     elseStmt = null,              // Rule 4 -- no fail action block
//   }

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/immediate_assert.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>

#include <string>

namespace hlc {

class DeferredAssertTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.2--assert0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("top", d->getAllModules());
}

static const hldb::Variable *getVariableA(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getVariables()) return nullptr;
  return hldb::findByName<hldb::Variable>("a", m->getVariables());
}

static const hldb::ImmediateAssert *getAssert(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m) return nullptr;
  // sec. 16.4 Rule 3: module-level 'assert #0' is stored in getAssertions(),
  // NOT in getProcesses(). This differs from procedural assertions (sec.
  // 16.2) which are inside Initial processes.
  const auto *assertions = m->getAssertions();
  if (!assertions || assertions->empty()) return nullptr;
  return any_cast<const hldb::ImmediateAssert *>((*assertions)[0]);
}

static const hldb::Operation *getOperation(const hldb::Design *d) {
  const auto *ia = getAssert(d);
  if (!ia) return nullptr;
  // Rule 2: getExpr() is the assertion expression itself (no DelayControl
  // wrapper is used to model the '#0').
  return ia->getExpr<hldb::Operation>();
}

// ----
// Module and variable
// ----
TEST_F(DeferredAssertTest, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'top' not found"; }

TEST_F(DeferredAssertTest, VariableA_HasLogicTypespec) {
  // SV source: 'logic a' -- sec. 6.3 declares a 4-state single-bit type.
  // No net-type keyword is present, so per sec. 6.7/6.8 this is a Variable.
  const hldb::Variable *const var = getVariableA(m_design);
  ASSERT_NE(var, nullptr) << "variable 'a' not found";
  ASSERT_NE(var->getTypespec(), nullptr) << "variable 'a' has no typespec";
  EXPECT_NE(var->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr) << "'logic a' must produce a LogicTypespec";
}

TEST_F(DeferredAssertTest, VariableA_InlineValue_Is1) {
  // SV source: 'logic a = 1' -- inline initializer literal 1.
  const hldb::Variable *const var = getVariableA(m_design);
  ASSERT_NE(var, nullptr);
  const auto *c = var->getValue<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "variable 'a' has no inline initializer value";
  EXPECT_EQ(std::string(c->getValue()), "1") << "'logic a = 1' -- inline initializer must be 1";
}

TEST_F(DeferredAssertTest, VariableA_NotAlsoInNets) {
  // sec. 6.7/6.8: 'logic a' with no net-type keyword is a variable, never a
  // net -- it must not also appear in the module's net collection.
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  if (m->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("a", m->getNets()), nullptr)
        << "'a' is a variable (no net-type keyword) and must not also appear in getNets()";
  }
}

// ----
// sec. 16.4 Rule 3: module-level deferred assertion is in getAssertions().
// Unlike sec. 16.2 simple assertions (procedural-only), sec. 16.4 deferred
// assertions may appear directly in module scope as a module_common_item.
// ----
TEST_F(DeferredAssertTest, ModuleLevelAssertInAssertionsCollection) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getAssertions(), nullptr) << "sec. 16.4 Rule 3: module-level 'assert #0 (...)' must populate "
                                            "getAssertions() -- module-scope deferred assertions are not "
                                            "processes, they are stored in the assertions collection";
  EXPECT_FALSE(m->getAssertions()->empty()) << "sec. 16.4 Rule 3: getAssertions() must be non-empty";
}

TEST_F(DeferredAssertTest, AssertionsCollection_FirstIsImmediateAssert) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getAssertions(), nullptr);
  ASSERT_FALSE(m->getAssertions()->empty());
  EXPECT_NE(any_cast<const hldb::ImmediateAssert *>((*m->getAssertions())[0]), nullptr)
      << "sec. 16.4: first entry in getAssertions() must be an ImmediateAssert";
}

// ----
// sec. 16.4 Rule 1: 'assert #0' -- isDeferred=true, isFinal=false.
// ----
TEST_F(DeferredAssertTest, Assert_IsDeferred) {
  // sec. 16.4: the '#0' keyword marks this as a deferred immediate assertion.
  // If the tool sets isDeferred=false, it has misclassified this as a
  // simple immediate assertion (sec. 16.2), losing deferred timing
  // semantics.
  const hldb::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  EXPECT_TRUE(ia->getIsDeferred()) << "sec. 16.4: 'assert #0 (expr)' must have isDeferred=true; "
                                      "simple 'assert (expr)' (sec. 16.2) has isDeferred=false";
}

TEST_F(DeferredAssertTest, Assert_IsNotFinal) {
  // sec. 16.4: 'assert #0' evaluates in the Observed region, not the final
  // region. Only 'assert final (expr)' sets isFinal=true.
  const hldb::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  EXPECT_FALSE(ia->getIsFinal()) << "sec. 16.4: 'assert #0 (expr)' must have isFinal=false; "
                                    "only 'assert final (expr)' sets isFinal=true";
}

// ----
// sec. 16.4 Rule 2: getExpr() returns the assertion expression itself. The
// '#0' deferral is captured solely by the isDeferred flag (see above) --
// there is no separate DelayControl node wrapping the expression.
// ----
TEST_F(DeferredAssertTest, Assert_ExprIsOperation) {
  const hldb::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  EXPECT_NE(ia->getExpr<hldb::Operation>(), nullptr)
      << "sec. 16.4: 'assert #0 (expr)' -- getExpr() must be the assertion expression 'a != 0'";
}

// ----
// The assertion expression 'a != 0'.
// sec. 11.4.5 defines '!=' as the logical inequality operator (vpiNeqOp).
// ----
TEST_F(DeferredAssertTest, Assert_ExprStmt_IsOperation) {
  const hldb::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  EXPECT_NE(ia->getExpr<hldb::Operation>(), nullptr) << "assertion expression 'a != 0' must be an Operation";
}

TEST_F(DeferredAssertTest, Assert_ExprStmt_IsNotEqualOperator) {
  // sec. 11.4.5: '!=' is the logical inequality operator -> vpiNeqOp.
  const auto *op = getOperation(m_design);
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiNeqOp) << "sec. 11.4.5: '!=' must be represented as vpiNeqOp";
}

TEST_F(DeferredAssertTest, Assert_ExprStmt_HasTwoOperands) {
  // '!=' is a binary operator -- exactly 2 operands.
  const auto *op = getOperation(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u) << "binary '!=' must have exactly 2 operands";
}

TEST_F(DeferredAssertTest, Assert_LeftOperand_IsRefObj) {
  // A signal reference in an expression is a RefObj in UHDM.
  const auto *op = getOperation(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  EXPECT_NE(any_cast<const hldb::RefObj *>((*op->getOperands())[0]), nullptr)
      << "left operand of 'a != 0' must be a RefObj";
}

TEST_F(DeferredAssertTest, Assert_LeftOperand_RefersToSignalA) {
  // The RefObj name must match the declared variable 'a'.
  const auto *op = getOperation(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a") << "left operand of 'a != 0' must reference signal 'a'";
}

TEST_F(DeferredAssertTest, Assert_RightOperand_IsConstant) {
  // The right side of 'a != 0' is the integer literal 0 -- a Constant node.
  const auto *op = getOperation(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  EXPECT_NE(any_cast<const hldb::Constant *>((*op->getOperands())[1]), nullptr)
      << "right operand of 'a != 0' must be a Constant";
}

TEST_F(DeferredAssertTest, Assert_RightOperand_ValueIsZero) {
  // The literal 0 in the source must be stored as value "0".
  const auto *op = getOperation(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const auto *c = any_cast<const hldb::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getValue()), "0") << "right operand of 'a != 0' must be the constant 0";
}

// ----
// sec. 16.4 Rule 4: no action blocks.
// 'assert #0 (a != 0)' has no explicit pass or fail statement.
// ----
TEST_F(DeferredAssertTest, Assert_NoPassActionBlock) {
  const hldb::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  EXPECT_EQ(ia->getStmt(), nullptr) << "sec. 16.4: 'assert #0 (a != 0)' has no explicit pass action block -- "
                                       "getStmt() must be null";
}

TEST_F(DeferredAssertTest, Assert_NoFailActionBlock) {
  const hldb::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  EXPECT_EQ(ia->getElseStmt(), nullptr) << "sec. 16.4: 'assert #0 (a != 0)' has no explicit fail action block -- "
                                           "getElseStmt() must be null";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

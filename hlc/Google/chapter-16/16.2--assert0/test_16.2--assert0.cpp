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

// Spec-based validation of IEEE 1800-2017 §16.4 deferred immediate assertion.
//
// All expected values are derived from §16.4 of the spec and the SV source.
// No expected value is taken from the UHDM log. Failing tests document
// Surelog bugs.
//
// ── §16.4 rules under test ─────────────────────────────────────────────────
//
// §16.4 defines two forms of deferred immediate assertion:
//
//   assert #0 (expr)    — deferred:  isDeferred=true,  isFinal=false
//   assert final (expr) — final:     isDeferred=true,  isFinal=true
//
// Rule 1 — 'assert #0 (expr)' sets isDeferred=true.
//   → ImmediateAssert::isDeferred must be true.
//   → ImmediateAssert::isFinal   must be false.
//   (Contrast with §16.2 'assert (expr)': isDeferred=false, isFinal=false.)
//
// Rule 2 — The '#0' specifies that evaluation is deferred to the
//   Observed/Reactive region at the end of the time step.
//   UHDM represents this deferral as a DelayControl wrapping the expression.
//   → ImmediateAssert::getExpr() returns a DelayControl.
//   → DelayControl::getDelay() is the constant 0 (the '#0' value).
//   → DelayControl::getStmt()  is the assertion expression (a != 0).
//
// Rule 3 — §16.4 allows deferred assertions at module scope as a
//   module_common_item (unlike §16.2 simple assertions which are
//   procedural-only).
//   → The ImmediateAssert is in module::getAssertions(), not getProcesses().
//
// Rule 4 — No action blocks. The SV source uses the bare form.
//   → ImmediateAssert::getStmt()     must be null.
//   → ImmediateAssert::getElseStmt() must be null.
//
// ── SV source ──────────────────────────────────────────────────────────────
//   logic a = 1;
//   assert #0 (a != 0);      ← module-level deferred immediate assertion
//
// ── Spec-correct UHDM ──────────────────────────────────────────────────────
//   Net 'a': LogicTypespec, inline value = 1
//   Module::getAssertions()[0] = ImmediateAssert {    // Rule 3
//     isDeferred = true,            // Rule 1 — '#0' form
//     isFinal    = false,           // Rule 1 — not 'final'
//     expr = DelayControl {         // Rule 2 — '#0' → DelayControl
//       delay = Constant(0),        // Rule 2 — the '#0' value
//       stmt  = Operation {         // Rule 2 — the assertion expression
//         opType:      vpiNeqOp     // §11.4.5: '!='
//         operands[0]: RefObj → 'a'
//         operands[1]: Constant → "0"
//       }
//     },
//     stmt     = null,              // Rule 4 — no pass action block
//     elseStmt = null,              // Rule 4 — no fail action block
//   }

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/constant.h>
#include <uhdm/delay_control.h>
#include <uhdm/design.h>
#include <uhdm/immediate_assert.h>
#include <uhdm/logic_typespec.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/operation.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>

#include <string>

namespace SURELOG {

class DeferredAssertTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "16.2--assert0.hlc"});

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

static const uhdm::Module *getTop(const uhdm::Design *d) {
  return uhdm::findByName<uhdm::Module>("work@top", d->getAllModules());
}

static const uhdm::Net *getNetA(const uhdm::Design *d) {
  const uhdm::Module *m = getTop(d);
  if (!m || !m->getNets()) return nullptr;
  return uhdm::findByName<uhdm::Net>("a", m->getNets());
}

static const uhdm::ImmediateAssert *getAssert(const uhdm::Design *d) {
  const uhdm::Module *m = getTop(d);
  if (!m) return nullptr;
  // §16.4 Rule 3: module-level 'assert #0' is stored in getAssertions(),
  // NOT in getProcesses(). This differs from procedural assertions (§16.2)
  // which are inside Initial processes.
  const auto *assertions = m->getAssertions();
  if (!assertions || assertions->empty()) return nullptr;
  return any_cast<const uhdm::ImmediateAssert *>((*assertions)[0]);
}

static const uhdm::DelayControl *getDelayControl(const uhdm::Design *d) {
  const auto *ia = getAssert(d);
  if (!ia) return nullptr;
  // §16.4 Rule 2: the '#0' is represented as a DelayControl wrapping the
  // assertion expression.
  return ia->getExpr<uhdm::DelayControl>();
}

static const uhdm::Operation *getOperation(const uhdm::Design *d) {
  const auto *dc = getDelayControl(d);
  if (!dc) return nullptr;
  // The assertion expression 'a != 0' is the statement inside DelayControl.
  return dc->getStmt<uhdm::Operation>();
}

// ---------------------------------------------------------------------------
// Module and net
// ---------------------------------------------------------------------------
TEST_F(DeferredAssertTest, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found";
}

TEST_F(DeferredAssertTest, NetA_HasLogicTypespec) {
  // SV source: 'logic a' — §6.3 declares a 4-state single-bit variable.
  const uhdm::Net *const net = getNetA(m_design);
  ASSERT_NE(net, nullptr) << "net 'a' not found";
  ASSERT_NE(net->getTypespec(), nullptr) << "net 'a' has no typespec";
  EXPECT_NE(net->getTypespec()->getActual<uhdm::LogicTypespec>(), nullptr)
      << "'logic a' must produce a LogicTypespec";
}

TEST_F(DeferredAssertTest, NetA_InlineValue_Is1) {
  // SV source: 'logic a = 1' — inline initializer literal 1.
  const uhdm::Net *const net = getNetA(m_design);
  ASSERT_NE(net, nullptr);
  const auto *c = net->getValue<uhdm::Constant>();
  ASSERT_NE(c, nullptr) << "net 'a' has no inline initializer value";
  EXPECT_EQ(std::string(c->getValue()), "1")
      << "'logic a = 1' — inline initializer must be 1";
}

// ---------------------------------------------------------------------------
// §16.4 Rule 3: module-level deferred assertion is in getAssertions().
// Unlike §16.2 simple assertions (procedural-only), §16.4 deferred
// assertions may appear directly in module scope as a module_common_item.
// ---------------------------------------------------------------------------
TEST_F(DeferredAssertTest, ModuleLevelAssertInAssertionsCollection) {
  const uhdm::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getAssertions(), nullptr)
      << "§16.4 Rule 3: module-level 'assert #0 (...)' must populate "
         "getAssertions() — module-scope deferred assertions are not "
         "processes, they are stored in the assertions collection";
  EXPECT_FALSE(m->getAssertions()->empty())
      << "§16.4 Rule 3: getAssertions() must be non-empty";
}

TEST_F(DeferredAssertTest, AssertionsCollection_FirstIsImmediateAssert) {
  const uhdm::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getAssertions(), nullptr);
  ASSERT_FALSE(m->getAssertions()->empty());
  EXPECT_NE(
      any_cast<const uhdm::ImmediateAssert *>((*m->getAssertions())[0]),
      nullptr)
      << "§16.4: first entry in getAssertions() must be an ImmediateAssert";
}

// ---------------------------------------------------------------------------
// §16.4 Rule 1: 'assert #0' — isDeferred=true, isFinal=false.
// ---------------------------------------------------------------------------
TEST_F(DeferredAssertTest, Assert_IsDeferred) {
  // §16.4: the '#0' keyword marks this as a deferred immediate assertion.
  // If Surelog sets isDeferred=false, it has misclassified this as a
  // simple immediate assertion (§16.2), losing deferred timing semantics.
  const uhdm::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  EXPECT_TRUE(ia->getIsDeferred())
      << "§16.4: 'assert #0 (expr)' must have isDeferred=true; "
         "simple 'assert (expr)' (§16.2) has isDeferred=false";
}

TEST_F(DeferredAssertTest, Assert_IsNotFinal) {
  // §16.4: 'assert #0' evaluates in the Observed region, not the final
  // region. Only 'assert final (expr)' sets isFinal=true.
  const uhdm::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  EXPECT_FALSE(ia->getIsFinal())
      << "§16.4: 'assert #0 (expr)' must have isFinal=false; "
         "only 'assert final (expr)' sets isFinal=true";
}

// ---------------------------------------------------------------------------
// §16.4 Rule 2: the '#0' delay is represented as a DelayControl wrapping
// the assertion expression. The deferral to the Observed region is modelled
// as a zero-delay timing control.
// ---------------------------------------------------------------------------
TEST_F(DeferredAssertTest, Assert_ExprIsDelayControl) {
  // §16.4: 'assert #0 (expr)' — the '#0' timing control is represented in
  // UHDM as a DelayControl node. getExpr() returns the DelayControl,
  // not the assertion expression directly.
  const uhdm::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  EXPECT_NE(ia->getExpr<uhdm::DelayControl>(), nullptr)
      << "§16.4: 'assert #0 (expr)' — the '#0' is modelled as a "
         "DelayControl in UHDM; getExpr() must return a DelayControl";
}

TEST_F(DeferredAssertTest, Assert_DelayValue_IsZero) {
  // §16.4: the '#0' specifies a zero-delay deferral.
  // The DelayControl's delay constant must have value "0".
  const auto *dc = getDelayControl(m_design);
  ASSERT_NE(dc, nullptr);
  const auto *delay = dc->getDelay<uhdm::Constant>();
  ASSERT_NE(delay, nullptr)
      << "§16.4: DelayControl must have a Constant delay node for '#0'";
  EXPECT_EQ(std::string(delay->getValue()), "0")
      << "§16.4: '#0' delay value must be 0";
}

// ---------------------------------------------------------------------------
// The assertion expression 'a != 0' is inside DelayControl::getStmt().
// §11.4.5 defines '!=' as the logical inequality operator (vpiNeqOp).
// ---------------------------------------------------------------------------
TEST_F(DeferredAssertTest, Assert_ExprStmt_IsOperation) {
  // The expression 'a != 0' is accessed via DelayControl::getStmt(),
  // not ImmediateAssert::getExpr() directly.
  const auto *dc = getDelayControl(m_design);
  ASSERT_NE(dc, nullptr);
  EXPECT_NE(dc->getStmt<uhdm::Operation>(), nullptr)
      << "assertion expression 'a != 0' must be an Operation inside "
         "the DelayControl's statement";
}

TEST_F(DeferredAssertTest, Assert_ExprStmt_IsNotEqualOperator) {
  // §11.4.5: '!=' is the logical inequality operator → vpiNeqOp.
  const auto *op = getOperation(m_design);
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiNeqOp)
      << "§11.4.5: '!=' must be represented as vpiNeqOp";
}

TEST_F(DeferredAssertTest, Assert_ExprStmt_HasTwoOperands) {
  // '!=' is a binary operator — exactly 2 operands.
  const auto *op = getOperation(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u)
      << "binary '!=' must have exactly 2 operands";
}

TEST_F(DeferredAssertTest, Assert_LeftOperand_IsRefObj) {
  // A signal reference in an expression is a RefObj in UHDM.
  const auto *op = getOperation(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  EXPECT_NE(any_cast<const uhdm::RefObj *>((*op->getOperands())[0]), nullptr)
      << "left operand of 'a != 0' must be a RefObj";
}

TEST_F(DeferredAssertTest, Assert_LeftOperand_RefersToSignalA) {
  // The RefObj name must match the declared net 'a'.
  const auto *op = getOperation(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref = any_cast<const uhdm::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a")
      << "left operand of 'a != 0' must reference signal 'a'";
}

TEST_F(DeferredAssertTest, Assert_RightOperand_IsConstant) {
  // The right side of 'a != 0' is the integer literal 0 — a Constant node.
  const auto *op = getOperation(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  EXPECT_NE(any_cast<const uhdm::Constant *>((*op->getOperands())[1]), nullptr)
      << "right operand of 'a != 0' must be a Constant";
}

TEST_F(DeferredAssertTest, Assert_RightOperand_ValueIsZero) {
  // The literal 0 in the source must be stored as value "0".
  const auto *op = getOperation(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const auto *c = any_cast<const uhdm::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getValue()), "0")
      << "right operand of 'a != 0' must be the constant 0";
}

// ---------------------------------------------------------------------------
// §16.4 Rule 4: no action blocks.
// 'assert #0 (a != 0)' has no explicit pass or fail statement.
// ---------------------------------------------------------------------------
TEST_F(DeferredAssertTest, Assert_NoPassActionBlock) {
  const uhdm::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  EXPECT_EQ(ia->getStmt(), nullptr)
      << "§16.4: 'assert #0 (a != 0)' has no explicit pass action block — "
         "getStmt() must be null";
}

TEST_F(DeferredAssertTest, Assert_NoFailActionBlock) {
  const uhdm::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  EXPECT_EQ(ia->getElseStmt(), nullptr)
      << "§16.4: 'assert #0 (a != 0)' has no explicit fail action block — "
         "getElseStmt() must be null";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

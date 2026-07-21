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

// Spec-based validation of IEEE 1800-2017 sec. 16.2 simple immediate assertion.
//
// All expected values are derived from sec. 16.2 of the spec and the SV source.
// No expected value is taken from the UHDM log. Failing tests document
// Surelog bugs.
//
// -- sec. 16.2 rules under test -------------------------------------------------
//
// sec. 16.2 defines three forms of immediate assertion:
//   simple:   assert (expr) [pass_stmt] [else fail_stmt]
//   deferred: assert #0 (expr) ...       -> isDeferred = true
//   final:    assert final (expr) ...    -> isFinal    = true
//
// Rule 1 -- 'assert (expr)' without '#0' or 'final' is a SIMPLE immediate
//   assertion: it evaluates in the active simulation region.
//   -> ImmediateAssert::isDeferred must be false.
//   -> ImmediateAssert::isFinal   must be false.
//
// Rule 2 -- The expression is evaluated as a boolean (false = 0/X/Z -> fail).
//   'a != 0' is a binary inequality (sec. 11.4.5: vpiNeqOp).
//
// Rule 3 -- Action blocks are optional single statements; no begin...end needed.
//   The SV source provides both:
//     pass block: $display("pass")
//     fail block: $display("fail")
//   -> getStmt()     must be non-null (SysTaskCall for $display)
//   -> getElseStmt() must be non-null (SysTaskCall for $display)
//
// Rule 4 -- 'initial assert (expr)' is a single statement. No begin...end
//   wrapper. ImmediateAssert is the direct stmt of the Initial process.
//
// -- SV source --------------------------------------------------------------
//   logic a = 1;
//   initial assert (a != 0) $display("pass") else $display("fail");
//
// -- Spec-correct UHDM ------------------------------------------------------
//   Net 'a': LogicTypespec, inline value = 1
//   Initial:
//     stmt = ImmediateAssert {
//       isDeferred = false,            // Rule 1
//       isFinal    = false,            // Rule 1
//       expr = Operation {             // Rule 2 -- 'a != 0'
//         opType:      vpiNeqOp
//         operands[0]: RefObj -> 'a'
//         operands[1]: Constant -> "0"
//       },
//       stmt = SysTaskCall {           // Rule 3 -- pass block
//         name: "$display"
//         arguments[0]: Constant {
//           value:     "pass"
//           size:      32   // sec. 5.9: 4 chars x 8 bits
//           constType: vpiStringConst
//         }
//       },
//       elseStmt = SysTaskCall {       // Rule 3 -- fail block
//         name: "$display"
//         arguments[0]: Constant {
//           value:     "fail"
//           size:      32   // sec. 5.9: 4 chars x 8 bits
//           constType: vpiStringConst
//         }
//       }
//     }

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/immediate_assert.h>
#include <hldb/initial.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_func_call.h>

#include <string>

namespace hlc {

class ImmediateAssertTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.2--assert.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@top", d->getAllModules());
}

static const hldb::Net *getNetA(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getNets()) return nullptr;
  return hldb::findByName<hldb::Net>("a", m->getNets());
}

static const hldb::ImmediateAssert *getAssert(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getProcesses() || m->getProcesses()->empty()) return nullptr;
  const auto *initial = any_cast<const hldb::Initial *>((*m->getProcesses())[0]);
  if (!initial) return nullptr;
  // sec. 16.2 Rule 4: assert is the direct statement of Initial -- no Begin.
  return initial->getStmt<hldb::ImmediateAssert>();
}

static const hldb::TFCall *getPassCall(const hldb::Design *d) {
  const auto *ia = getAssert(d);
  if (!ia) return nullptr;
  return ia->getStmt<hldb::TFCall>();
}

static const hldb::TFCall *getFailCall(const hldb::Design *d) {
  const auto *ia = getAssert(d);
  if (!ia) return nullptr;
  return ia->getElseStmt<hldb::TFCall>();
}

static const hldb::Constant *getFirstArg(const hldb::TFCall *call) {
  if (!call || !call->getArguments() || call->getArguments()->empty()) return nullptr;
  return any_cast<const hldb::Constant *>((*call->getArguments())[0]);
}

// ---------------------------------------------------------------------------
// Module and net
// ---------------------------------------------------------------------------
TEST_F(ImmediateAssertTest, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found"; }

TEST_F(ImmediateAssertTest, NetA_HasLogicTypespec) {
  // SV source: 'logic a' -- sec. 6.3 declares a 4-state single-bit net.
  // UHDM must represent it as LogicTypespec.
  const hldb::Net *const net = getNetA(m_design);
  ASSERT_NE(net, nullptr) << "net 'a' not found";
  ASSERT_NE(net->getTypespec(), nullptr) << "net 'a' has no typespec";
  EXPECT_NE(net->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr) << "'logic a' must produce a LogicTypespec";
}

TEST_F(ImmediateAssertTest, NetA_InlineValue_Is1) {
  // SV source: 'logic a = 1' -- inline initializer literal 1.
  const hldb::Net *const net = getNetA(m_design);
  ASSERT_NE(net, nullptr);
  const auto *c = net->getValue<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "net 'a' has no inline initializer value";
  EXPECT_EQ(std::string(c->getValue()), "1") << "'logic a = 1' -- inline initializer must be 1";
}

// ---------------------------------------------------------------------------
// sec. 16.2 Rule 4: ImmediateAssert is the direct statement of the Initial
// process. 'initial assert (expr)' does not need begin...end.
// ---------------------------------------------------------------------------
TEST_F(ImmediateAssertTest, InitialHasDirectImmediateAssert) {
  ASSERT_NE(getAssert(m_design), nullptr) << "sec. 16.2 Rule 4: 'initial assert (...)' must produce an ImmediateAssert "
                                             "as the direct statement of the Initial -- no Begin wrapper needed "
                                             "for a single immediate assert statement";
}

// ---------------------------------------------------------------------------
// sec. 16.2 Rule 1: simple immediate assertion -- not deferred, not final.
// ---------------------------------------------------------------------------
TEST_F(ImmediateAssertTest, Assert_IsNotDeferred) {
  // sec. 16.2: 'assert (expr)' without '#0' is simple. Deferred form requires
  // '#0'. If Surelog sets isDeferred=true, it misclassified the assert.
  const hldb::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  EXPECT_FALSE(ia->getIsDeferred()) << "sec. 16.2 Rule 1: simple 'assert (expr)' must have isDeferred=false; "
                                       "deferred form requires '#0' keyword";
}

TEST_F(ImmediateAssertTest, Assert_IsNotFinal) {
  // sec. 16.2: 'assert (expr)' without 'final' evaluates in the active region.
  // If Surelog sets isFinal=true, it misclassified the timing region.
  const hldb::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  EXPECT_FALSE(ia->getIsFinal()) << "sec. 16.2 Rule 1: 'assert (expr)' without 'final' must have isFinal=false";
}

// ---------------------------------------------------------------------------
// sec. 16.2 Rule 2: assertion expression 'a != 0'.
// sec. 11.4.5 defines '!=' as the logical inequality operator (vpiNeqOp).
// ---------------------------------------------------------------------------
TEST_F(ImmediateAssertTest, Assert_ExpressionIsOperation) {
  const hldb::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  EXPECT_NE(ia->getExpr<hldb::Operation>(), nullptr)
      << "sec. 16.2 Rule 2: assertion expression 'a != 0' must be an Operation node";
}

TEST_F(ImmediateAssertTest, Assert_ExpressionIsNotEqualOperator) {
  // sec. 11.4.5: '!=' is the logical inequality operator -- vpiNeqOp.
  const hldb::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  const auto *op = ia->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiNeqOp) << "sec. 11.4.5: '!=' must be represented as vpiNeqOp";
}

TEST_F(ImmediateAssertTest, Assert_ExpressionHasTwoOperands) {
  // '!=' is a binary operator -- exactly 2 operands.
  const hldb::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  const auto *op = ia->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u) << "binary '!=' must have exactly 2 operands";
}

TEST_F(ImmediateAssertTest, Assert_LeftOperand_IsRefObj) {
  // A signal reference in an expression is a RefObj in UHDM.
  const hldb::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  const auto *op = ia->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  EXPECT_NE(any_cast<const hldb::RefObj *>((*op->getOperands())[0]), nullptr)
      << "left operand of 'a != 0' must be a RefObj";
}

TEST_F(ImmediateAssertTest, Assert_LeftOperand_RefersToSignalA) {
  const hldb::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  const auto *op = ia->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a") << "left operand of 'a != 0' must reference signal 'a'";
}

TEST_F(ImmediateAssertTest, Assert_RightOperand_IsConstant) {
  const hldb::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  const auto *op = ia->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  EXPECT_NE(any_cast<const hldb::Constant *>((*op->getOperands())[1]), nullptr)
      << "right operand of 'a != 0' must be a Constant";
}

TEST_F(ImmediateAssertTest, Assert_RightOperand_ValueIsZero) {
  const hldb::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  const auto *op = ia->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const auto *c = any_cast<const hldb::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getValue()), "0") << "right operand of 'a != 0' must be the constant 0";
}

// ---------------------------------------------------------------------------
// sec. 16.2 Rule 3: pass action block -- $display("pass").
// The pass action executes when the assertion expression evaluates to true.
// sec. 16.2 allows a single statement without begin...end as the action block.
// $display is a system task call represented as SysTaskCall in UHDM.
// ---------------------------------------------------------------------------
TEST_F(ImmediateAssertTest, Assert_PassActionBlock_Exists) {
  // sec. 16.2: 'assert (expr) stmt' -- the statement after the expression is
  // the pass action block. It must be non-null.
  const hldb::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  EXPECT_NE(ia->getStmt(), nullptr) << "sec. 16.2 Rule 3: 'assert (...) $display(\"pass\")' must have a "
                                       "non-null pass action block";
}

TEST_F(ImmediateAssertTest, Assert_PassActionBlock_IsSysTaskcCall) {
  // $display is a system call -- represented as SysTaskCall in UHDM.
  const hldb::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  EXPECT_NE(ia->getStmt<hldb::SysTaskCall>(), nullptr)
      << "sec. 16.2: pass action '$display(...)' must be a SysTaskCall";
}

TEST_F(ImmediateAssertTest, Assert_PassActionBlock_Name) {
  // The system task name must be "$display".
  const auto *call = getPassCall(m_design);
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$display") << "pass action block must be a call to '$display'";
}

TEST_F(ImmediateAssertTest, Assert_PassActionBlock_HasOneArgument) {
  // $display("pass") is called with exactly one argument.
  const auto *call = getPassCall(m_design);
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  EXPECT_EQ(call->getArguments()->size(), 1u) << "$display(\"pass\") must have exactly one argument";
}

TEST_F(ImmediateAssertTest, Assert_PassActionBlock_Arg_IsConstant) {
  const auto *c = getFirstArg(getPassCall(m_design));
  EXPECT_NE(c, nullptr) << "argument to $display(\"pass\") must be a Constant";
}

TEST_F(ImmediateAssertTest, Assert_PassActionBlock_Arg_Value) {
  // SV source: $display("pass") -- the literal string is "pass".
  const auto *c = getFirstArg(getPassCall(m_design));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getValue()), "pass") << "$display(\"pass\"): argument value must be \"pass\"";
}

TEST_F(ImmediateAssertTest, Assert_PassActionBlock_Arg_Size) {
  // sec. 5.9: string size = number of characters x 8 bits.
  // "pass" has 4 characters -> 4 x 8 = 32 bits.
  const auto *c = getFirstArg(getPassCall(m_design));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 32) << "sec. 5.9: \"pass\" is 4 chars x 8 bits = 32 bits";
}

TEST_F(ImmediateAssertTest, Assert_PassActionBlock_Arg_ConstType) {
  // sec. 5.9: string literals have string constant type -> vpiStringConst.
  const auto *c = getFirstArg(getPassCall(m_design));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiStringConst)
      << "sec. 5.9: string literal \"pass\" must have constType vpiStringConst";
}

// ---------------------------------------------------------------------------
// sec. 16.2 Rule 3: fail action block (else clause) -- $display("fail").
// The fail action executes when the assertion expression evaluates to false
// (0, X, or Z). The else clause is optional; when present it is non-null.
// ---------------------------------------------------------------------------
TEST_F(ImmediateAssertTest, Assert_FailActionBlock_Exists) {
  // sec. 16.2: '... else fail_stmt' -- the else clause is the fail action block.
  // It must be non-null because the SV source includes 'else $display("fail")'.
  const hldb::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  EXPECT_NE(ia->getElseStmt(), nullptr) << "sec. 16.2 Rule 3: '... else $display(\"fail\")' must have a "
                                           "non-null fail action block";
}

TEST_F(ImmediateAssertTest, Assert_FailActionBlock_IsSysTaskCall) {
  const hldb::ImmediateAssert *const ia = getAssert(m_design);
  ASSERT_NE(ia, nullptr);
  EXPECT_NE(ia->getElseStmt<hldb::SysTaskCall>(), nullptr)
      << "sec. 16.2: fail action '$display(...)' must be a SysTaskCall";
}

TEST_F(ImmediateAssertTest, Assert_FailActionBlock_Name) {
  const auto *call = getFailCall(m_design);
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$display") << "fail action block must be a call to '$display'";
}

TEST_F(ImmediateAssertTest, Assert_FailActionBlock_HasOneArgument) {
  const auto *call = getFailCall(m_design);
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  EXPECT_EQ(call->getArguments()->size(), 1u) << "$display(\"fail\") must have exactly one argument";
}

TEST_F(ImmediateAssertTest, Assert_FailActionBlock_Arg_IsConstant) {
  const auto *c = getFirstArg(getFailCall(m_design));
  EXPECT_NE(c, nullptr) << "argument to $display(\"fail\") must be a Constant";
}

TEST_F(ImmediateAssertTest, Assert_FailActionBlock_Arg_Value) {
  // SV source: $display("fail") -- the literal string is "fail".
  const auto *c = getFirstArg(getFailCall(m_design));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getValue()), "fail") << "$display(\"fail\"): argument value must be \"fail\"";
}

TEST_F(ImmediateAssertTest, Assert_FailActionBlock_Arg_Size) {
  // sec. 5.9: "fail" has 4 characters -> 4 x 8 = 32 bits.
  const auto *c = getFirstArg(getFailCall(m_design));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 32) << "sec. 5.9: \"fail\" is 4 chars x 8 bits = 32 bits";
}

TEST_F(ImmediateAssertTest, Assert_FailActionBlock_Arg_ConstType) {
  // sec. 5.9: string literal "fail" must be vpiStringConst.
  const auto *c = getFirstArg(getFailCall(m_design));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiStringConst)
      << "sec. 5.9: string literal \"fail\" must have constType vpiStringConst";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

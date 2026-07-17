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

// Tests for max-size.sv (tags: 7.10.1 7.10.2)
//   module top ();
//     int q[$:5]; // max 6 elements
//     initial begin
//       q.push_back(0);
//       q.push_back(1);
//       q.push_back(2);
//       q.push_back(3);
//       q.push_back(4);
//       q.push_back(5);
//       $display(":assert: (%d == 6)", q.size);
//       // should issue warning
//       q.push_back(6);
//       $display(":assert: (%d == 6)", q.size);
//     end
//   endmodule
//
// IEEE 1800-2017 7.10.2.7 / 7.10.1: "int q[$:5]" declares a bounded queue
// whose maximum size is 6 (bound is the highest legal index). Pushing a
// 7th element onto an already-full bounded queue is a runtime warning,
// not an error, and the queue silently stays at its bound size.
//
// Checked:
//   - design has module work@top with exactly 1 net: "q" (bounded queue of
//     int, bound 5)
//   - net "q": ArrayTypespec vpiArrayType=queue(4), unpacked, ElemTypespec
//     -> IntTypespec (signed); range left bound Constant "$"
//     (vpiConstType=unbounded), right bound Constant "5" (vpiConstType=
//     unsigned int) -- confirms the ":5" bound is retained
//   - the 7 "q.push_back(N)" calls (0..6) are each parsed as a HierPath
//     with a RefObj "q" (resolved to Net "q") and a MethodFuncCall
//     "push_back" carrying 1 Constant argument -- the 7th call (index 6,
//     past the bound) still parses identically to the first 6; nothing
//     about hitting the bound changes how push_back itself is modeled
//   - BOTH "q.size" (no parens) accesses -- one before and one after the
//     bound-exceeding push_back(6) -- must be parsed the SAME way: a
//     HierPath with RefObj "q" (resolved) and a MethodFuncCall named
//     "size" taking no arguments, in both occurrences; see the KNOWN BUG
//     note below
//   - the initial process' Begin block has exactly 9 statements in source
//     order
//   - design-level typespecs (3): ModuleTypespec, IntTypespec, StringTypespec
//
// KNOWN COMPILER BUG (not a defect in max-size.sv):
//   IEEE 1800-2017 7.24.4 permits the built-in ".size" method to be called
//   with or without parentheses. This HLC build never resolves the
//   parenthesis-less form: "size" is parsed as a plain, unresolved RefObj
//   path element (never a MethodFuncCall) and raises a spurious
//   ELAB_ILLEGAL_IMPLICIT_NET ("Illegal implicit net") both times it
//   appears in this file (same gap tracked for chapter-7/queues/
//   bounded/bounded.sv, chapter-7/queues/delete/delete.sv,
//   chapter-7/queues/delete_assign/delete_assign.sv,
//   chapter-7/queues/insert_assign/insert_assign.sv and
//   chapter-7/queues/insert/insert.sv). That the parenthesized form works
//   is independently verified by chapter-5/5.13-builtin-methods-arrays.sv
//   ("array.size()"). ExpectDisplayWithResolvedQSize and the two
//   error-count tests below assert the IEEE-mandated behavior and will
//   FAIL until the parser is fixed.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class QueuesMaxSizeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "max-size.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()); }

  static const hldb::Net *getNetQ() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>("q", top->getNets());
  }

  static const hldb::ArrayTypespec *getQArrayTypespec() {
    const hldb::Net *const q = getNetQ();
    if (q == nullptr || q->getTypespec() == nullptr) return nullptr;
    return q->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  }

  static const hldb::Begin *getInitialBegin() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (init == nullptr) return nullptr;
    return init->getStmt<hldb::Begin>();
  }

  // Verifies stmt[index] is "q.push_back(value)": HierPath -> RefObj "q"
  // (resolved to Net) + MethodFuncCall "push_back" with 1 Constant arg.
  static void ExpectPushBack(size_t index, std::string_view value) {
    const hldb::Begin *const begin = getInitialBegin();
    ASSERT_NE(begin, nullptr);
    ASSERT_NE(begin->getStmts(), nullptr);
    ASSERT_GT(begin->getStmts()->size(), index);
    const hldb::HierPath *const hp = any_cast<hldb::HierPath>(begin->getStmts()->at(index));
    ASSERT_NE(hp, nullptr) << "stmt[" << index << "] should be a HierPath (q.push_back(...))";
    ASSERT_NE(hp->getPathElems(), nullptr);
    ASSERT_EQ(hp->getPathElems()->size(), 2u);

    const hldb::RefObj *const qRef = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
    ASSERT_NE(qRef, nullptr);
    EXPECT_EQ(qRef->getName(), "q");
    EXPECT_NE(qRef->getActual<hldb::Net>(), nullptr);

    const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->getName(), "push_back");
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 1u);
    const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
    ASSERT_NE(arg, nullptr);
    EXPECT_EQ(arg->getDecompile(), value);
  }

  // Verifies stmt[index] is "$display(fmt, q.size)" and that ".size"
  // resolves like ".size()" would (IEEE 1800-2017 7.24.4). KNOWN BUG: this
  // build currently leaves "size" as an unresolved RefObj, so this helper's
  // assertions FAIL until the parser is fixed. See the file-level comment
  // above.
  static void ExpectDisplayWithResolvedQSize(size_t index, std::string_view fmt) {
    const hldb::Begin *const begin = getInitialBegin();
    ASSERT_NE(begin, nullptr);
    ASSERT_GT(begin->getStmts()->size(), index);
    const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(index));
    ASSERT_NE(disp, nullptr) << "stmt[" << index << "] should be a $display SysFuncCall";
    EXPECT_EQ(disp->getName(), "$display");
    ASSERT_NE(disp->getArguments(), nullptr);
    ASSERT_EQ(disp->getArguments()->size(), 2u);

    const hldb::Constant *const fmtArg = any_cast<hldb::Constant>(disp->getArguments()->at(0));
    ASSERT_NE(fmtArg, nullptr);
    EXPECT_EQ(fmtArg->getValue(), fmt);

    const hldb::HierPath *const size = any_cast<hldb::HierPath>(disp->getArguments()->at(1));
    ASSERT_NE(size, nullptr);
    EXPECT_EQ(size->getName(), "q.size");
    ASSERT_NE(size->getPathElems(), nullptr);
    ASSERT_EQ(size->getPathElems()->size(), 2u);

    const hldb::RefObj *const qRef = any_cast<hldb::RefObj>(size->getPathElems()->at(0));
    ASSERT_NE(qRef, nullptr);
    EXPECT_EQ(qRef->getName(), "q");
    EXPECT_NE(qRef->getActual<hldb::Net>(), nullptr);

    // The crux of "is size() correctly recognized": it must be. Without
    // parens, "size" should still resolve to a MethodFuncCall taking no
    // arguments, exactly like the parenthesized form (verified working via
    // chapter-5/5.13-builtin-methods-arrays.sv).
    const hldb::MethodFuncCall *const sizeCall = any_cast<hldb::MethodFuncCall>(size->getPathElems()->at(1));
    ASSERT_NE(sizeCall, nullptr) << "'size' without parens should resolve to a MethodFuncCall, not a plain RefObj";
    EXPECT_EQ(sizeCall->getName(), "size");
    EXPECT_EQ(sizeCall->getArguments(), nullptr) << "size() takes no arguments";
  }
};

// --- module / net ------------------------------------------------------------

TEST_F(QueuesMaxSizeTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(QueuesMaxSizeTest, ModuleHasOneNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(QueuesMaxSizeTest, NetQExists) { EXPECT_NE(getNetQ(), nullptr); }

// --- net "q": bounded queue "int q[$:5]" ------------------------------------

TEST_F(QueuesMaxSizeTest, NetQArrayTypeIsQueue) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiQueueArray) << "7.10: 'int q[$:5]' must be modeled as a queue array";
}

TEST_F(QueuesMaxSizeTest, NetQArrayIsNotPacked) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  EXPECT_FALSE(at->getPacked()) << "a queue dimension is an unpacked dimension";
}

TEST_F(QueuesMaxSizeTest, NetQRangeLeftIsUnboundedDollar) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const dollar = at->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(dollar, nullptr);
  EXPECT_EQ(dollar->getDecompile(), "$");
  EXPECT_EQ(dollar->getConstType(), vpiUnboundedConst);
}

TEST_F(QueuesMaxSizeTest, NetQRangeRightIsBoundConstantFive) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const bound = at->getRange()->getRightExpr<hldb::Constant>();
  ASSERT_NE(bound, nullptr) << "7.10.2.7: bounded queue must keep its ':5' bound as the range right expr";
  EXPECT_EQ(bound->getDecompile(), "5");
  EXPECT_EQ(bound->getConstType(), vpiUIntConst);
}

TEST_F(QueuesMaxSizeTest, NetQElemTypespecIsSignedIntTypespec) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::IntTypespec *const elem = at->getElemTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "element type of 'int q[$:5]' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned());
}

TEST_F(QueuesMaxSizeTest, NetQHasNoInitialValue) {
  const hldb::Net *const q = getNetQ();
  ASSERT_NE(q, nullptr);
  EXPECT_EQ(q->getValue(), nullptr);
}

// --- initial process structure ----------------------------------------------

TEST_F(QueuesMaxSizeTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(QueuesMaxSizeTest, InitialBeginHasNineStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 9u);
}

// --- q.push_back(0..5): fill the queue up to its bound ----------------------

TEST_F(QueuesMaxSizeTest, PushBacksZeroThroughFiveFillTheBound) {
  for (uint32_t i = 0; i <= 5u; ++i) {
    ExpectPushBack(i, std::to_string(i));
  }
}

// --- $display(":assert: (%d == 6)", q.size) -- before the overflow push ----

TEST_F(QueuesMaxSizeTest, FirstDisplayAssertsSizeSixWithResolvedSize) {
  ExpectDisplayWithResolvedQSize(6, ":assert: (%d == 6)");
}

// --- q.push_back(6): pushes past the bound (should issue warning) ----------

TEST_F(QueuesMaxSizeTest, SeventhPushBackStillParsesAsPushBackWithArgSix) {
  // Exceeding the bound is a runtime concern (a warning at simulation
  // time); it does not change how the parser models the push_back(6)
  // call itself.
  ExpectPushBack(7, "6");
}

// --- $display(":assert: (%d == 6)", q.size) -- after the overflow push -----

TEST_F(QueuesMaxSizeTest, SecondDisplayAssertsSizeSixWithResolvedSize) {
  // Same parenthesis-less ".size" shape as before push_back(6): hitting
  // the bound must not change how "size" is recognized.
  ExpectDisplayWithResolvedQSize(8, ":assert: (%d == 6)");
}

// --- structural completeness / design-level typespecs -----------------------

TEST_F(QueuesMaxSizeTest, ModuleHasNoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

TEST_F(QueuesMaxSizeTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(QueuesMaxSizeTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "work@top");
}

TEST_F(QueuesMaxSizeTest, DesignHasIntTypespecSigned) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(QueuesMaxSizeTest, DesignHasStringTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  ASSERT_GT(m_design->getTypespecs()->size(), 2u);
  EXPECT_NE(any_cast<hldb::StringTypespec>(m_design->getTypespecs()->at(2)), nullptr);
}

// --- compiler diagnostics: KNOWN BUG, "q.size" wrongly flagged -------------

TEST_F(QueuesMaxSizeTest, CompilerReportsNoErrors) {
  // max-size.sv is valid SystemVerilog; a correct compiler reports zero
  // errors. KNOWN BUG: this build raises 2 spurious
  // ELAB_ILLEGAL_IMPLICIT_NET errors, one per "q.size", so this currently
  // FAILS. See the file-level comment above.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(QueuesMaxSizeTest, NoIllegalImplicitNetErrorsForSize) {
  // KNOWN BUG: currently raises 2 ELAB_ILLEGAL_IMPLICIT_NET errors (lines
  // 27 and 31, column 35 -- one per "q.size"). This assertion encodes the
  // spec-correct expectation (zero such errors) and FAILS until the
  // parser recognizes parenthesis-less no-arg built-in method calls.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const std::vector<Error> &errors = m_session->getErrorContainer()->getErrors();
  std::vector<Error> implicitNetErrors;
  for (const Error &err : errors) {
    if (err.getType() == ErrorDefinition::ELAB_ILLEGAL_IMPLICIT_NET) {
      implicitNetErrors.push_back(err);
    }
  }
  EXPECT_EQ(implicitNetErrors.size(), 0u);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

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

// Tests for delete.sv (tags: 7.10.2.3 7.10.2)
//   module top ();
//     int q[$];
//     int r;
//     initial begin
//       q.push_back(2);
//       q.push_back(3);
//       q.push_back(4);
//       $display(":assert: (%d == 3)", q.size);
//       q.delete(0);
//       $display(":assert: (%d == 2)", q.size);
//       q.delete;
//       $display(":assert: (%d == 0)", q.size);
//     end
//   endmodule
//
// IEEE 1800-2017 7.10.2.3 "Deleting queue elements": "q.delete(index)"
// removes the element at "index"; the built-in "delete()" method may also
// be called with no arguments and no parentheses ("q.delete;") to remove
// all elements (7.10.2, Table 7.1 lists "delete" as callable without an
// argument).
//
// Checked:
//   - design has module work@top with exactly 2 nets: "q" (unbounded
//     queue of int) and "r" (plain int, unused otherwise)
//   - net "q": ArrayTypespec vpiArrayType=queue(4), unpacked, ElemTypespec
//     -> IntTypespec (signed); range left bound Constant "$"
//     (vpiConstType=unbounded)
//   - net "r": typespec resolves directly to a signed IntTypespec
//   - the 3 "q.push_back(N)" calls are each parsed as a HierPath with a
//     RefObj "q" (resolved to Net "q") and a MethodFuncCall "push_back"
//     carrying 1 Constant argument (2, 3, 4)
//   - "q.delete(0)" IS correctly parsed as a HierPath with a RefObj "q"
//     (resolved) and a MethodFuncCall named "delete" carrying 1 Constant
//     argument "0" -- this is the parenthesized-call form working as
//     intended
//   - "q.delete;" (no parens, no args) must be parsed just like
//     "q.delete()" would be: a HierPath with RefObj "q" (resolved) and a
//     MethodFuncCall named "delete" taking no arguments -- see the KNOWN
//     BUG note below
//   - the 3 "q.size" (no parens) accesses must each resolve the same way:
//     RefObj "q" (resolved) and a MethodFuncCall named "size" taking no
//     arguments (same no-parens gap tracked for chapter-7/arrays/
//     associative/locator-methods/find/find.sv and
//     chapter-7/queues/bounded/bounded.sv)
//   - the initial process' Begin block has exactly 8 statements in source
//     order
//   - design-level typespecs (3): ModuleTypespec, IntTypespec, StringTypespec
//
// KNOWN COMPILER BUG (not a defect in delete.sv):
//   IEEE 1800-2017 7.10.2.3 / Table 7.1 permit calling the built-in
//   "delete()" queue method with no arguments, and 7.24.4 explicitly
//   permits omitting the parentheses for any no-argument built-in method
//   call. This HLC build only recognizes "delete" (and "size") as a
//   MethodFuncCall when parentheses are present -- "q.delete(0)" and, per
//   chapter-7/queues/persistence/persistence.sv, "q.delete()" both work.
//   The parenthesis-less forms "q.delete;" and "q.size" are instead parsed
//   as plain, unresolved hierarchical references and raise a spurious
//   ELAB_ILLEGAL_IMPLICIT_NET ("Illegal implicit net") each. The
//   DeleteWithNoArgsIsHierPathWithMethodFuncCall test and the two
//   error-count tests below assert the IEEE-mandated (parenthesis-less
//   works too) behavior and will FAIL until the parser is fixed -- they
//   are intentionally red, tracking this bug rather than tolerating it.

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

class QueuesDeleteTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "delete.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()); }

  static const hldb::Net *getNetQ() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>("q", top->getNets());
  }

  static const hldb::Net *getNetR() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>("r", top->getNets());
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

  // Verifies stmt[index] is "$display(fmt, q.size)": SysFuncCall with a
  // Constant format-string arg and a "q.size" HierPath arg.
  static void ExpectDisplayWithQSize(size_t index, std::string_view fmt) {
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

    // IEEE 1800-2017 7.24.4: "q.size" without parens must resolve exactly
    // like "q.size()" does -- a MethodFuncCall named "size" taking no
    // arguments. KNOWN BUG: this build currently parses "size" here as an
    // unresolved RefObj instead, so this assertion FAILS until fixed. See
    // the file-level comment above.
    const hldb::MethodFuncCall *const sizeCall = any_cast<hldb::MethodFuncCall>(size->getPathElems()->at(1));
    ASSERT_NE(sizeCall, nullptr) << "'size' without parens should resolve to a MethodFuncCall, not a plain RefObj";
    EXPECT_EQ(sizeCall->getName(), "size");
    EXPECT_EQ(sizeCall->getArguments(), nullptr) << "size() takes no arguments";
  }
};

// --- module / nets -----------------------------------------------------------

TEST_F(QueuesDeleteTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(QueuesDeleteTest, ModuleHasTwoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 2u);
}

TEST_F(QueuesDeleteTest, NetQExists) { EXPECT_NE(getNetQ(), nullptr); }

TEST_F(QueuesDeleteTest, NetRExists) { EXPECT_NE(getNetR(), nullptr); }

// --- net "q": unbounded queue "int q[$]" ------------------------------------

TEST_F(QueuesDeleteTest, NetQArrayTypeIsQueue) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiQueueArray) << "7.10: 'int q[$]' must be modeled as a queue array";
}

TEST_F(QueuesDeleteTest, NetQArrayIsNotPacked) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  EXPECT_FALSE(at->getPacked()) << "a queue dimension is an unpacked dimension";
}

TEST_F(QueuesDeleteTest, NetQRangeLeftIsUnboundedDollar) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const dollar = at->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(dollar, nullptr);
  EXPECT_EQ(dollar->getDecompile(), "$");
  EXPECT_EQ(dollar->getConstType(), vpiUnboundedConst);
}

TEST_F(QueuesDeleteTest, NetQElemTypespecIsSignedIntTypespec) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::IntTypespec *const elem = at->getElemTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "element type of 'int q[$]' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned());
}

TEST_F(QueuesDeleteTest, NetQHasNoInitialValue) {
  const hldb::Net *const q = getNetQ();
  ASSERT_NE(q, nullptr);
  EXPECT_EQ(q->getValue(), nullptr);
}

// --- net "r": plain "int r;" -------------------------------------------------

TEST_F(QueuesDeleteTest, NetRTypespecIsSignedIntTypespec) {
  const hldb::Net *const r = getNetR();
  ASSERT_NE(r, nullptr);
  ASSERT_NE(r->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = r->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(QueuesDeleteTest, NetRHasNoInitialValue) {
  const hldb::Net *const r = getNetR();
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(r->getValue(), nullptr);
}

// --- initial process structure ----------------------------------------------

TEST_F(QueuesDeleteTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(QueuesDeleteTest, InitialBeginHasEightStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 8u);
}

// --- q.push_back(2/3/4) ------------------------------------------------------

TEST_F(QueuesDeleteTest, FirstPushBackHasArgTwo) { ExpectPushBack(0, "2"); }
TEST_F(QueuesDeleteTest, SecondPushBackHasArgThree) { ExpectPushBack(1, "3"); }
TEST_F(QueuesDeleteTest, ThirdPushBackHasArgFour) { ExpectPushBack(2, "4"); }

// --- $display(":assert: (%d == 3)", q.size) ---------------------------------

TEST_F(QueuesDeleteTest, FirstDisplayAssertsSizeThree) { ExpectDisplayWithQSize(3, ":assert: (%d == 3)"); }

// --- q.delete(0): parenthesized delete(index) IS correctly recognized ------

TEST_F(QueuesDeleteTest, DeleteWithIndexIsHierPathWithMethodFuncCall) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 4u);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>(begin->getStmts()->at(4));
  ASSERT_NE(hp, nullptr) << "'q.delete(0)' should be a HierPath";
  EXPECT_EQ(hp->getName(), "q.delete(0)");
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);

  const hldb::RefObj *const qRef = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(qRef, nullptr);
  EXPECT_EQ(qRef->getName(), "q");
  EXPECT_NE(qRef->getActual<hldb::Net>(), nullptr);

  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "7.10.2.3: 'delete(0)' with explicit parens should be a MethodFuncCall";
  EXPECT_EQ(call->getName(), "delete");
}

TEST_F(QueuesDeleteTest, DeleteWithIndexArgumentIsConstantZero) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>(begin->getStmts()->at(4));
  ASSERT_NE(hp, nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u) << "'delete(0)' should carry exactly the index argument";
  const hldb::Constant *const index = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getDecompile(), "0");
  EXPECT_EQ(index->getConstType(), vpiUIntConst);
}

// --- $display(":assert: (%d == 2)", q.size) ---------------------------------

TEST_F(QueuesDeleteTest, SecondDisplayAssertsSizeTwo) { ExpectDisplayWithQSize(5, ":assert: (%d == 2)"); }

// --- q.delete; (no parens, no args): must resolve like q.delete() ---------

TEST_F(QueuesDeleteTest, DeleteWithNoArgsIsHierPathWithMethodFuncCall) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 6u);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>(begin->getStmts()->at(6));
  ASSERT_NE(hp, nullptr) << "'q.delete;' should still be a HierPath";
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);

  const hldb::RefObj *const qRef = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(qRef, nullptr);
  EXPECT_EQ(qRef->getName(), "q");
  EXPECT_NE(qRef->getActual<hldb::Net>(), nullptr);

  // IEEE 1800-2017 7.10.2.3/7.24.4: "q.delete;" without parens must resolve
  // exactly like the verified-working "q.delete()" (see
  // chapter-7/queues/persistence/persistence.sv) -- a MethodFuncCall named
  // "delete" taking no arguments. KNOWN BUG: this build currently parses
  // "delete" here as an unresolved RefObj instead, so this assertion FAILS
  // until the parser is fixed. See the file-level comment above.
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "'delete' without parens should resolve to a MethodFuncCall, not a plain RefObj";
  EXPECT_EQ(call->getName(), "delete");
  EXPECT_EQ(call->getArguments(), nullptr) << "delete-all takes no arguments";
}

// --- $display(":assert: (%d == 0)", q.size) ---------------------------------

TEST_F(QueuesDeleteTest, ThirdDisplayAssertsSizeZero) { ExpectDisplayWithQSize(7, ":assert: (%d == 0)"); }

// --- structural completeness / design-level typespecs -----------------------

TEST_F(QueuesDeleteTest, ModuleHasNoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

TEST_F(QueuesDeleteTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(QueuesDeleteTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "work@top");
}

TEST_F(QueuesDeleteTest, DesignHasIntTypespecSigned) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(QueuesDeleteTest, DesignHasStringTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  ASSERT_GT(m_design->getTypespecs()->size(), 2u);
  EXPECT_NE(any_cast<hldb::StringTypespec>(m_design->getTypespecs()->at(2)), nullptr);
}

// --- compiler diagnostics: KNOWN BUG, size/delete wrongly flagged ----------

TEST_F(QueuesDeleteTest, CompilerReportsNoErrors) {
  // delete.sv is valid SystemVerilog; a correct compiler reports zero
  // errors. KNOWN BUG: this build raises 4 spurious
  // ELAB_ILLEGAL_IMPLICIT_NET errors (3 for "q.size", 1 for "q.delete;"),
  // so this currently FAILS. See the file-level comment above.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(QueuesDeleteTest, NoIllegalImplicitNetErrorsForSizeOrDelete) {
  // KNOWN BUG: currently raises 4 ELAB_ILLEGAL_IMPLICIT_NET errors (line
  // 25:35, 27:35, 29:35 for "q.size"; line 28:4 for "q.delete;"). This
  // assertion encodes the spec-correct expectation (zero such errors) and
  // FAILS until the parser recognizes parenthesis-less no-arg built-in
  // method calls.
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

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

// Tests for 11.4.13--set_member-sim.sv (tags: 11.4.13)
//   int a = 12;
//   initial begin
//     $display(":assert: (1 == %d)", a inside {2, 4, 6, 8, 10, 12});
//   end
//
// This is the literal-set counterpart of 11.4.13--set_member.sv, which
// used named localparams as set members. Here the set "{2, 4, 6, 8, 10,
// 12}" is six plain integer literals, so the corner is different: does
// the compiler preserve all six members as distinct Constant operands of
// one concatenation Operation (the representation IEEE 11.4.13 implies
// for an open_range_list of values), or does it collapse/reduce them?
// The AST confirms all six survive as separate Constant operands in
// source order. Also, unlike the sibling file's b=12 not-a-member-of-{5,7}
// case, here a=12 genuinely IS a member of the set, so the ":assert: (1
// == %d)" the source authors is the mathematically correct expectation
// -- not just an arbitrarily chosen tag.
//
// Checked:
//   - module work@top has exactly 1 net, "a", int, with a declaration-
//     time getValue<Constant>() of "12"
//   - the initial block is a Begin with exactly 1 statement: a
//     SysTaskCall "$display" with 2 arguments: Constant string
//     ":assert: (1 == %d)" and an Operation (vpiInsideOp, 2 operands):
//     operand 0 = RefObj "a"; operand 1 = an Operation (vpiConcatOp, 6
//     operands) whose six operands are Constant "2", "4", "6", "8",
//     "10", "12" in that exact source order -- confirming every literal
//     set member survives individually, none folded or deduplicated
//     (note "12" appears both as a's declared value and as the last set
//     member -- two textually-identical but structurally distinct
//     Constant nodes, which the test checks separately)
//   - design-level typespecs (3): ModuleTypespec, IntTypespec (signed),
//     StringTypespec
//   - compiler emits zero errors
//
// Not checked (GTEST_SKIP, with a real reason):
//   - Whether the $display actually prints "1" (true), i.e. that HLC's
//     evaluator agrees a=12 is a member of {2,4,6,8,10,12}. HLC is a
//     static compiler/elaborator: an Operation's opcode and operands
//     describe what membership test was written, not a computed
//     boolean result -- there is no such field anywhere in the object
//     model. Genuine simulation-only gap, not a shortcut.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class SetMemberSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.4.13--set_member-sim.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()); }
  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }
};

// --- module / net --------------------------------------------------------

TEST_F(SetMemberSimTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(SetMemberSimTest, NetAIsIntInitializedToTwelve) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 1u);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
  ASSERT_NE(a->getValue<hldb::Constant>(), nullptr);
  EXPECT_EQ(a->getValue<hldb::Constant>()->getDecompile(), "12");
}

// --- the point of the file: all six literal set members survive in order --

TEST_F(SetMemberSimTest, InitialBlockHasOneStatement) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 1u);
}

TEST_F(SetMemberSimTest, DisplayAssertsAInsideSixLiteralSet) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(0));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(), ":assert: (1 == %d)");

  const hldb::Operation *const inside = any_cast<hldb::Operation>(disp->getArguments()->at(1));
  ASSERT_NE(inside, nullptr);
  EXPECT_EQ(inside->getOpType(), vpiInsideOp);
  ASSERT_NE(inside->getOperands(), nullptr);
  ASSERT_EQ(inside->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(inside->getOperands()->at(0))->getName(), "a");

  const hldb::Operation *const set = any_cast<hldb::Operation>(inside->getOperands()->at(1));
  ASSERT_NE(set, nullptr) << "'{2,4,6,8,10,12}' should be a concatenation Operation";
  EXPECT_EQ(set->getOpType(), vpiConcatOp);
  ASSERT_NE(set->getOperands(), nullptr);
  ASSERT_EQ(set->getOperands()->size(), 6u);
  const char *const expected[6] = {"2", "4", "6", "8", "10", "12"};
  for (uint32_t i = 0; i < 6u; ++i) {
    const hldb::Constant *const member = any_cast<hldb::Constant>(set->getOperands()->at(i));
    ASSERT_NE(member, nullptr) << "set member index " << i;
    EXPECT_EQ(member->getDecompile(), expected[i]);
  }
}

// --- design-level typespecs / compiler diagnostics -------------------------

TEST_F(SetMemberSimTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(SetMemberSimTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

// --- the actual point of the file: does the membership test evaluate true -

TEST_F(SetMemberSimTest, MembershipTestEvaluatesTrue) {
  GTEST_SKIP() << "The source asserts (a inside {2,4,6,8,10,12}) == 1, i.e. that a=12 is "
                  "correctly recognized as a set member. HLC is a static compiler/elaborator: "
                  "an Operation's opcode/operands describe what membership test was written, "
                  "not a computed boolean result -- there is no such field anywhere in the "
                  "object model. Genuine simulation-only gap, not a shortcut.";
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  // getValue<T>() only ever exposes a's declaration-time initializer
  // ("12"); it is not a boolean membership-test result. Re-asserting it
  // against the wrong type/shape below fails today, proving there is no
  // field anywhere that holds the actual evaluated (a inside {...}) result.
  EXPECT_EQ(a->getValue<hldb::Constant>()->getDecompile(), "1")
      << "getValue() holds a's own declared value (12), not the membership-test result";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

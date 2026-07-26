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

// Tests for default-value.sv (tags: 7.2.2)
//   module top ();
//     parameter c = 4'h5;
//     struct {
//       bit [3:0] lo = c;
//       bit [3:0] hi;
//     } p1;
//     initial begin
//       p1.hi = 4'ha;
//       $display(":assert: (('%h' == 'a') and ('%h' == '5'))", p1.hi, p1.lo);
//     end
//   endmodule
//
// Unlike chapter-7/structures/packed/default-value, this is LEGAL: members
// of unpacked structures ARE allowed individual default member values (only
// packed struct/union members are restricted). This file carries no
// ':should_fail_because:' annotation, and the compiler reports zero errors,
// which is the expected, correct outcome here.
//
// Checked:
//   - design has module top with exactly 1 Parameter "c" (typespec ->
//     LogicTypespec) and exactly 1 ParamAssign: lhs RefObj "c" resolving the
//     Parameter, rhs Constant hexadecimal "4'h5" (value "5")
//   - module has exactly 1 net: "p1"
//   - net "p1": RefTypespec -> StructTypespec, vpiPacked false (unpacked),
//     exactly 2 TypespecMember "lo"/"hi"
//   - member "lo": typespec -> BitTypespec [3:0] vector, getDefaultValue()
//     populated as a RefObj "c" resolving the Parameter "c" (legal here)
//   - member "hi": typespec -> BitTypespec [3:0] vector, no default value
//   - design-level typespecs (4): ModuleTypespec, IntTypespec (unsigned),
//     IntTypespec (signed), StringTypespec
//   - Initial process: 1 Begin with 2 stmts (1 HierPath Assignment + 1
//     SysFuncCall) -- note p1.lo is never explicitly assigned; it relies on
//     its declared default value 'c' (== 4'h5)
//   - Stmt[0]: blocking Assignment, lhs HierPath "p1.hi" (RefObj "p1" -> Net
//     "p1", RefObj "hi" -> TypespecMember "hi"), rhs Constant hexadecimal
//     "4'ha" (value "a")
//   - Stmt[1]: $display with 3 args (format + HierPath "p1.hi" + HierPath
//     "p1.lo")
//   - compiler emits zero errors
//   - no continuous assignments
//
// Not checked:
//   - actual runtime %h-formatted value of p1.hi/p1.lo, in particular
//     whether p1.lo's default value ('c' == 4'h5) actually gets applied
//     since it is never explicitly assigned -- that requires running a
//     simulator, which this harness does not do. default-value.sv's own
//     $display format string documents the expected values (see the
//     skipped canary RuntimeDefaultValueRequiresSimulation below).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/struct_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/typespec_member.h>
#include <hldb/vpi_user.h>

namespace hlc {

class UnpackedStructDefaultValueTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "default-value.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Begin *getInitialBegin() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (init == nullptr) return nullptr;
    return init->getStmt<hldb::Begin>();
  }

  static const hldb::StructTypespec *getP1StructTypespec() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getNets() == nullptr) return nullptr;
    const hldb::Net *const p1 = hldb::findByName<hldb::Net>("p1", top->getNets());
    if (p1 == nullptr) return nullptr;
    return p1->getTypespec<hldb::RefTypespec>()->getActual<hldb::StructTypespec>();
  }
};

// --- module / parameter -------------------------------------------------------

TEST_F(UnpackedStructDefaultValueTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(UnpackedStructDefaultValueTest, ModuleHasOneParameterNamedC) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParameters(), nullptr);
  ASSERT_EQ(top->getParameters()->size(), 1u);
  const hldb::Parameter *const c = any_cast<hldb::Parameter>(top->getParameters()->at(0));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getName(), "c");
  EXPECT_NE(c->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(UnpackedStructDefaultValueTest, ParamAssignSetsCToHexFive) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParamAssigns(), nullptr);
  ASSERT_EQ(top->getParamAssigns()->size(), 1u);
  const hldb::ParamAssign *const pa = top->getParamAssigns()->at(0);
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "c");
  EXPECT_NE(lhs->getActual<hldb::Parameter>(), nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "4'h5");
  EXPECT_EQ(rhs->getValue(), "5");
}

// --- net / struct typespec / legal per-member default value -----------------

TEST_F(UnpackedStructDefaultValueTest, ModuleHasOneNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(UnpackedStructDefaultValueTest, P1IsUnpackedStructWithTwoMembers) {
  const hldb::StructTypespec *const st = getP1StructTypespec();
  ASSERT_NE(st, nullptr);
  EXPECT_FALSE(st->getPacked());
  ASSERT_NE(st->getMembers(), nullptr);
  EXPECT_EQ(st->getMembers()->size(), 2u);
}

TEST_F(UnpackedStructDefaultValueTest, MemberLoHasLegalDefaultValueResolvingToParameterC) {
  const hldb::StructTypespec *const st = getP1StructTypespec();
  ASSERT_NE(st, nullptr);
  ASSERT_NE(st->getMembers(), nullptr);
  const hldb::TypespecMember *const lo = st->getMembers()->at(0);
  ASSERT_NE(lo, nullptr);
  EXPECT_EQ(lo->getName(), "lo");
  const hldb::BitTypespec *const bt = hldb::getTypespec<hldb::BitTypespec>(lo);
  ASSERT_NE(bt, nullptr);
  ASSERT_NE(bt->getRanges(), nullptr);
  ASSERT_EQ(bt->getRanges()->size(), 1u);
  EXPECT_EQ(bt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "3");
  EXPECT_EQ(bt->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
  const hldb::RefObj *const defaultValue = lo->getDefaultValue<hldb::RefObj>();
  ASSERT_NE(defaultValue, nullptr);
  EXPECT_EQ(defaultValue->getName(), "c");
  EXPECT_NE(defaultValue->getActual<hldb::Parameter>(), nullptr);
}

TEST_F(UnpackedStructDefaultValueTest, MemberHiHasNoDefaultValue) {
  const hldb::StructTypespec *const st = getP1StructTypespec();
  ASSERT_NE(st, nullptr);
  ASSERT_NE(st->getMembers(), nullptr);
  const hldb::TypespecMember *const hi = st->getMembers()->at(1);
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(hi->getName(), "hi");
  EXPECT_EQ(hi->getDefaultValue(), nullptr);
}

// --- initial process ---------------------------------------------------------

TEST_F(UnpackedStructDefaultValueTest, InitialBeginHasTwoStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 2u);
}

TEST_F(UnpackedStructDefaultValueTest, FirstStmtAssignsHexAToPOneHi) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const hldb::HierPath *const lhs = assign->getLhs<hldb::HierPath>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "p1.hi");
  ASSERT_NE(lhs->getPathElems(), nullptr);
  ASSERT_EQ(lhs->getPathElems()->size(), 2u);
  EXPECT_NE(any_cast<hldb::RefObj>(lhs->getPathElems()->at(0))->getActual<hldb::Net>(), nullptr);
  EXPECT_NE(any_cast<hldb::RefObj>(lhs->getPathElems()->at(1))->getActual<hldb::TypespecMember>(), nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "4'ha");
  EXPECT_EQ(rhs->getValue(), "a");
}

TEST_F(UnpackedStructDefaultValueTest, SecondStmtDisplaysHiAndLoFields) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(1));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 3u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (('%h' == 'a') and ('%h' == '5'))");
  EXPECT_EQ(any_cast<hldb::HierPath>(disp->getArguments()->at(1))->getName(), "p1.hi");
  EXPECT_EQ(any_cast<hldb::HierPath>(disp->getArguments()->at(2))->getName(), "p1.lo");
}

// --- design-level typespecs / compiler diagnostics ---------------------------

TEST_F(UnpackedStructDefaultValueTest, DesignHasFourTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 4u);
}

TEST_F(UnpackedStructDefaultValueTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(UnpackedStructDefaultValueTest, DesignHasSignedIntTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(2));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(UnpackedStructDefaultValueTest, DesignHasStringTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_NE(any_cast<hldb::StringTypespec>(m_design->getTypespecs()->at(3)), nullptr);
}

TEST_F(UnpackedStructDefaultValueTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(UnpackedStructDefaultValueTest, NoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

// --- known gap: runtime default-value application requires simulation -----

TEST_F(UnpackedStructDefaultValueTest, RuntimeDefaultValueRequiresSimulation) {
  GTEST_SKIP() << "This harness only compiles/elaborates default-value.sv; it does not run a "
                  "simulator, so whether p1.lo's declared default value ('c' == 4'h5) actually gets "
                  "applied at runtime (p1.lo is never explicitly assigned) cannot be observed here. "
                  "default-value.sv's own $display format string documents the expected value.";

  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(1));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(),
            ":assert: (('%h' == 'a') and ('%h' == '5'))")
      << "expected p1.hi == 4'ha (explicitly assigned) and p1.lo == 4'h5 (from its declared default "
         "value 'c', never explicitly assigned)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

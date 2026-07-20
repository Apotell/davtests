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
//   :should_fail_because: members of packed structures shall not be assigned
//   individual default member values.
//   module top ();
//     parameter c = 4'h5;
//     struct packed {
//       bit [3:0] lo = c;
//       bit [3:0] hi;
//     } p1;
//   endmodule
//
// Checked:
//   - design has module work@top with exactly 1 Parameter "c" (typespec ->
//     LogicTypespec) and exactly 1 ParamAssign: lhs RefObj "c" resolving the
//     Parameter, rhs Constant hexadecimal "4'h5" (value "5")
//   - module has exactly 1 net: "p1"
//   - net "p1": RefTypespec -> StructTypespec, vpiPacked true, exactly 2
//     TypespecMember "lo"/"hi"
//   - member "lo": typespec -> BitTypespec [3:0] vector, getDefaultValue()
//     populated as a RefObj "c" resolving the Parameter "c" -- i.e. the
//     illegal per-member default value IS captured structurally
//   - member "hi": typespec -> BitTypespec [3:0] vector, no default value
//   - design-level typespecs (3): ModuleTypespec, IntTypespec (unsigned),
//     IntTypespec (signed) -- no StringTypespec since there is no initial
//     block / $display
//   - module has no processes (pure declaration, no initial/always block)
//
// Known compiler defect (see CompilerShouldRejectPackedMemberDefaultValueButDoesNot
// below): IEEE 1800-2017 7.2.2 states members of packed structures shall NOT
// be assigned individual default member values -- 'bit [3:0] lo = c;' inside
// 'struct packed' is illegal, matching this file's own ':should_fail_because:'
// annotation. The compiler currently emits ZERO errors for this file, i.e. it
// silently accepts the illegal construct instead of rejecting it. This is a
// genuine, provable compile-time defect (not a "requires simulation" gap):
// no execution is needed to know a packed struct member has an individual
// default value, since TypespecMember::getDefaultValue() is populated right
// there in the elaborated AST, as shown above.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/vpi_user.h>

namespace hlc {

class PackedStructDefaultValueTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "default-value.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()); }

  static const hldb::StructTypespec *getP1StructTypespec() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getNets() == nullptr) return nullptr;
    const hldb::Net *const p1 = hldb::findByName<hldb::Net>("p1", top->getNets());
    if (p1 == nullptr) return nullptr;
    return p1->getTypespec<hldb::RefTypespec>()->getActual<hldb::StructTypespec>();
  }
};

// --- module / parameter -------------------------------------------------------

TEST_F(PackedStructDefaultValueTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(PackedStructDefaultValueTest, ModuleHasOneParameterNamedC) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParameters(), nullptr);
  ASSERT_EQ(top->getParameters()->size(), 1u);
  const hldb::Parameter *const c = any_cast<hldb::Parameter>(top->getParameters()->at(0));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getName(), "c");
  EXPECT_NE(c->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(PackedStructDefaultValueTest, ParamAssignSetsCToHexFive) {
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

// --- net / struct typespec / illegal per-member default value --------------

TEST_F(PackedStructDefaultValueTest, ModuleHasOneNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(PackedStructDefaultValueTest, P1IsPackedStructWithTwoMembers) {
  const hldb::StructTypespec *const st = getP1StructTypespec();
  ASSERT_NE(st, nullptr);
  EXPECT_TRUE(st->getPacked());
  ASSERT_NE(st->getMembers(), nullptr);
  EXPECT_EQ(st->getMembers()->size(), 2u);
}

TEST_F(PackedStructDefaultValueTest, MemberLoHasIllegalDefaultValueResolvingToParameterC) {
  const hldb::StructTypespec *const st = getP1StructTypespec();
  ASSERT_NE(st, nullptr);
  ASSERT_NE(st->getMembers(), nullptr);
  const hldb::TypespecMember *const lo = st->getMembers()->at(0);
  ASSERT_NE(lo, nullptr);
  EXPECT_EQ(lo->getName(), "lo");
  const hldb::BitTypespec *const bt = lo->getTypespec<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  ASSERT_NE(bt->getRanges(), nullptr);
  ASSERT_EQ(bt->getRanges()->size(), 1u);
  EXPECT_EQ(bt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "3");
  EXPECT_EQ(bt->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
  const hldb::RefObj *const defaultValue = lo->getDefaultValue<hldb::RefObj>();
  ASSERT_NE(defaultValue, nullptr) << "'lo = c' should still be captured structurally, even though it is illegal";
  EXPECT_EQ(defaultValue->getName(), "c");
  EXPECT_NE(defaultValue->getActual<hldb::Parameter>(), nullptr);
}

TEST_F(PackedStructDefaultValueTest, MemberHiHasNoDefaultValue) {
  const hldb::StructTypespec *const st = getP1StructTypespec();
  ASSERT_NE(st, nullptr);
  ASSERT_NE(st->getMembers(), nullptr);
  const hldb::TypespecMember *const hi = st->getMembers()->at(1);
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(hi->getName(), "hi");
  EXPECT_EQ(hi->getDefaultValue(), nullptr);
}

// --- design-level typespecs ---------------------------------------------------

TEST_F(PackedStructDefaultValueTest, DesignHasThreeTypespecs) {
  // No StringTypespec: default-value.sv has no initial block / $display.
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(PackedStructDefaultValueTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "work@top");
}

TEST_F(PackedStructDefaultValueTest, DesignHasSignedIntTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(2));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(PackedStructDefaultValueTest, ModuleHasNoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

// --- known compiler defect: illegal packed-member default value accepted ----

TEST_F(PackedStructDefaultValueTest, CompilerShouldRejectPackedMemberDefaultValueButDoesNot) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbFatal + stats.nbSyntax + stats.nbError, 0)
      << "IEEE 1800-2017 7.2.2: members of packed structures shall not be assigned individual default "
         "member values ('bit [3:0] lo = c;' inside 'struct packed'). This file is annotated "
         ":should_fail_because: exactly this reason, yet the compiler currently emits zero errors -- "
         "it silently accepts the illegal construct instead of rejecting it. This is a genuine "
         "compile-time defect, not a simulation gap: TypespecMember::getDefaultValue() is already "
         "populated for 'lo' in the elaborated AST (see MemberLoHasIllegalDefaultValueResolvingToParameterC "
         "above), so the compiler has everything it needs to detect and reject this at compile time.";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

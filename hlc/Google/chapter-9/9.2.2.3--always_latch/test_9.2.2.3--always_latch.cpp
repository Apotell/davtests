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

// Tests for 9.2.2.3--always_latch.sv (tags: 9.2.2.3)
//   module always_tb ();
//     wire a = 0;
//     wire b = 0;
//     reg q = 0;
//     always_latch
//       if(a) q <= b;
//   endmodule
//
// IEEE 1800-2017 Sec 9.2.2.3 "always_latch procedure": another refined
// always form (see 9.2.2.2--always_comb.sv for always_comb) -- its Always
// node's AlwaysType must be exactly vpiAlwaysLatch.
//
// "wire a = 0" / "wire b = 0" -- same reasoning as 9.2.2.2--always_comb.sv:
// both are Nets (net-type keyword present), each a 1-bit unsigned
// LogicTypespec with no declared Ranges, and each net_decl_assignment
// value resolves via getValue() to Constant "0" (vpiUIntConst). As
// established there, getNetDeclAssign() is asserted true per Sec 6.7.1 but
// is a CONFIRMED compiler bug (never set) -- see 9.2.2.2--always_comb.cpp
// for the cross-checked evidence; asserted anyway, intentionally failing.
//
// "reg q = 0" is an ordinary variable, same reasoning as the prior files
// in this chapter: single-bit, unsigned LogicTypespec, no declared Ranges,
// plain-value "0" initializer -> vpiUIntConst.
//
// "if(a) q <= b;" -- Sec 12.4 "Conditional if-else statement": with no
// "else" clause, this must be an IfStmt (not IfElse), whose getCondition()
// holds the bare written condition "a" (a scalar signal used directly as
// a boolean condition is not rewritten into an explicit comparison node --
// only its runtime truth value is implicitly "!= 0") and whose getStmt()
// holds the single then-branch statement, not wrapped in a Begin (no
// begin/end around it in source).
//
// "q <= b" uses the NON-BLOCKING assignment operator "<=" as written --
// unlike 9.2.2.1/9.2.2.2's blocking "=", this Assignment's getBlocking()
// must be false, reflecting the source exactly regardless of the fact
// that Sec 9.2.2.3 recommends blocking-style assignments in
// always_latch/always_comb (a style guideline, not a syntax restriction --
// the language does not forbid "<=" here).
//
// Checked:
//   - design has module "always_tb" with exactly 2 nets ("a", "b") and 1
//     variable ("q")
//   - net "a" / net "b": LogicTypespec, unsigned, no declared ranges; net
//     type vpiWire; getValue() resolves to Constant "0" (vpiUIntConst);
//     not duplicated in the variable collection. getNetDeclAssign() is a
//     confirmed bug (see above), asserted anyway
//   - variable "q": LogicTypespec, unsigned, no declared ranges; not
//     duplicated in the net collection; initial value resolves to
//     Constant "0" (vpiUIntConst)
//   - module has exactly 1 process, and it is an Always whose AlwaysType
//     is exactly vpiAlwaysLatch
//   - the Always' body is directly an IfStmt (no enclosing Begin)
//   - the IfStmt's condition is a RefObj resolving to Net "a"; its
//     then-branch is directly an Assignment (no enclosing Begin), with no
//     else branch
//   - that Assignment is non-blocking ("<="); lhs is a RefObj resolving to
//     Variable "q"; rhs is a RefObj resolving to Net "b"

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/if_stmt.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AlwaysLatchTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.2.2.3--always_latch.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() {
    return hldb::findByName<hldb::Module>("always_tb", m_design->getAllModules());
  }

  static const hldb::Net *getNet(std::string_view name) {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>(name, mod->getNets());
  }

  static const hldb::LogicTypespec *getNetTypespec(std::string_view name) {
    const hldb::Net *const n = getNet(name);
    if (n == nullptr || n->getTypespec() == nullptr) return nullptr;
    return n->getTypespec()->getActual<hldb::LogicTypespec>();
  }

  static const hldb::Variable *getVariableQ() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("q", mod->getVariables());
  }

  static const hldb::LogicTypespec *getVariableQTypespec() {
    const hldb::Variable *const q = getVariableQ();
    if (q == nullptr || q->getTypespec() == nullptr) return nullptr;
    return q->getTypespec()->getActual<hldb::LogicTypespec>();
  }

  static const hldb::Always *getAlwaysProcess() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getProcesses() == nullptr || mod->getProcesses()->empty()) return nullptr;
    return any_cast<hldb::Always>(mod->getProcesses()->at(0));
  }

  static const hldb::IfStmt *getIfStmt() {
    const hldb::Always *const alw = getAlwaysProcess();
    if (alw == nullptr) return nullptr;
    return alw->getStmt<hldb::IfStmt>();
  }

  static const hldb::Assignment *getAssignment() {
    const hldb::IfStmt *const ifStmt = getIfStmt();
    if (ifStmt == nullptr) return nullptr;
    return ifStmt->getStmt<hldb::Assignment>();
  }
};

// --- nets "a" and "b" (wire) --------------------------------------------------

TEST_F(AlwaysLatchTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(AlwaysLatchTest, ModuleHasTwoNets) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getNets(), nullptr);
  EXPECT_EQ(mod->getNets()->size(), 2u);
}

TEST_F(AlwaysLatchTest, NetAExists) { EXPECT_NE(getNet("a"), nullptr); }

TEST_F(AlwaysLatchTest, NetBExists) { EXPECT_NE(getNet("b"), nullptr); }

// "wire a"/"wire b" have a net-type keyword, so per IEEE 1800-2023 Sec
// 6.7/6.8 neither must also appear in the module's variable collection.
TEST_F(AlwaysLatchTest, NetsAreNotDuplicatedAsVariables) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  if (mod->getVariables() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Variable>("a", mod->getVariables()), nullptr)
        << "'wire a' has a net-type keyword and must not also appear as a Variable";
    EXPECT_EQ(hldb::findByName<hldb::Variable>("b", mod->getVariables()), nullptr)
        << "'wire b' has a net-type keyword and must not also appear as a Variable";
  }
}

TEST_F(AlwaysLatchTest, NetANetTypeIsWire) {
  const hldb::Net *const a = getNet("a");
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getNetType(), vpiWire) << "'wire a' must have net type vpiWire";
}

TEST_F(AlwaysLatchTest, NetBNetTypeIsWire) {
  const hldb::Net *const b = getNet("b");
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getNetType(), vpiWire) << "'wire b' must have net type vpiWire";
}

TEST_F(AlwaysLatchTest, NetATypespecIsLogicTypespec) { EXPECT_NE(getNetTypespec("a"), nullptr); }

TEST_F(AlwaysLatchTest, NetBTypespecIsLogicTypespec) { EXPECT_NE(getNetTypespec("b"), nullptr); }

TEST_F(AlwaysLatchTest, NetATypespecIsUnsigned) {
  const hldb::LogicTypespec *const ts = getNetTypespec("a");
  ASSERT_NE(ts, nullptr);
  EXPECT_FALSE(ts->getSigned()) << "6.7.2: an undecorated 'wire' defaults to unsigned";
}

TEST_F(AlwaysLatchTest, NetBTypespecIsUnsigned) {
  const hldb::LogicTypespec *const ts = getNetTypespec("b");
  ASSERT_NE(ts, nullptr);
  EXPECT_FALSE(ts->getSigned()) << "6.7.2: an undecorated 'wire' defaults to unsigned";
}

TEST_F(AlwaysLatchTest, NetATypespecHasNoDeclaredRanges) {
  const hldb::LogicTypespec *const ts = getNetTypespec("a");
  ASSERT_NE(ts, nullptr);
  EXPECT_TRUE(ts->getRanges() == nullptr || ts->getRanges()->empty())
      << "'wire a' declares no '[msb:lsb]' -- it is an implicit scalar bit";
}

TEST_F(AlwaysLatchTest, NetBTypespecHasNoDeclaredRanges) {
  const hldb::LogicTypespec *const ts = getNetTypespec("b");
  ASSERT_NE(ts, nullptr);
  EXPECT_TRUE(ts->getRanges() == nullptr || ts->getRanges()->empty())
      << "'wire b' declares no '[msb:lsb]' -- it is an implicit scalar bit";
}

// CONFIRMED COMPILER BUG (not a defect in always_latch.sv): see
// 9.2.2.2--always_comb.cpp's NetAIsMarkedAsNetDeclAssign for the
// cross-checked evidence (chapter-6/6.9.2--vector_scalared.sv's identical
// "net = value" shape also never sets this flag). Sec 6.7.1 requires it
// for "wire a = 0"/"wire b = 0"; asserted anyway, intentionally failing.
TEST_F(AlwaysLatchTest, NetsAreMarkedAsNetDeclAssign) {
  const hldb::Net *const a = getNet("a");
  const hldb::Net *const b = getNet("b");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  EXPECT_TRUE(a->getNetDeclAssign()) << "6.7.1: 'wire a = 0' is a net_decl_assignment";
  EXPECT_TRUE(b->getNetDeclAssign()) << "6.7.1: 'wire b = 0' is a net_decl_assignment";
}

TEST_F(AlwaysLatchTest, NetAValueIsConstantZero) {
  const hldb::Net *const a = getNet("a");
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const val = a->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr) << "'wire a = 0' must carry its net_decl_assignment value";
  EXPECT_EQ(val->getDecompile(), "0");
  EXPECT_EQ(val->getConstType(), vpiUIntConst) << "bare decimal literal -> constType unsigned int (9)";
}

TEST_F(AlwaysLatchTest, NetBValueIsConstantZero) {
  const hldb::Net *const b = getNet("b");
  ASSERT_NE(b, nullptr);
  const hldb::Constant *const val = b->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr) << "'wire b = 0' must carry its net_decl_assignment value";
  EXPECT_EQ(val->getDecompile(), "0");
  EXPECT_EQ(val->getConstType(), vpiUIntConst) << "bare decimal literal -> constType unsigned int (9)";
}

// --- variable "q" (reg) --------------------------------------------------------

TEST_F(AlwaysLatchTest, ModuleHasOneVariable) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  EXPECT_EQ(mod->getVariables()->size(), 1u);
}

TEST_F(AlwaysLatchTest, VariableQExists) { EXPECT_NE(getVariableQ(), nullptr); }

// "reg q" has no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8 it
// must not also appear in the module's net collection.
TEST_F(AlwaysLatchTest, VariableQIsNotDuplicatedAsNet) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  if (mod->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("q", mod->getNets()), nullptr)
        << "'reg q' has no net-type keyword and must not also appear as a Net";
  }
}

TEST_F(AlwaysLatchTest, VariableQTypespecIsLogicTypespec) { EXPECT_NE(getVariableQTypespec(), nullptr); }

TEST_F(AlwaysLatchTest, VariableQTypespecIsUnsigned) {
  const hldb::LogicTypespec *const ts = getVariableQTypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_FALSE(ts->getSigned()) << "6.8: 'reg' with no 'signed' keyword defaults to unsigned";
}

TEST_F(AlwaysLatchTest, VariableQTypespecHasNoDeclaredRanges) {
  const hldb::LogicTypespec *const ts = getVariableQTypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_TRUE(ts->getRanges() == nullptr || ts->getRanges()->empty())
      << "'reg q' declares no '[msb:lsb]' -- it is an implicit scalar bit";
}

TEST_F(AlwaysLatchTest, VariableQInitialValueIsConstantZero) {
  const hldb::Variable *const q = getVariableQ();
  ASSERT_NE(q, nullptr);
  const hldb::Constant *const val = q->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr) << "'reg q = 0' must carry an initial value";
  EXPECT_EQ(val->getDecompile(), "0");
  EXPECT_EQ(val->getConstType(), vpiUIntConst) << "bare decimal literal -> constType unsigned int (9)";
}

// --- always_latch process structure --------------------------------------------

TEST_F(AlwaysLatchTest, ModuleHasOneProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
}

TEST_F(AlwaysLatchTest, TheOneProcessIsAlways) { EXPECT_NE(getAlwaysProcess(), nullptr); }

TEST_F(AlwaysLatchTest, AlwaysTypeIsAlwaysLatch) {
  const hldb::Always *const alw = getAlwaysProcess();
  ASSERT_NE(alw, nullptr);
  EXPECT_EQ(alw->getAlwaysType(), vpiAlwaysLatch) << "9.2.2.3: 'always_latch' must have AlwaysType vpiAlwaysLatch";
}

// 9.2.2.3: "always_latch" with no begin/end wraps a single statement_or_null
// -- the process' stmt must be the IfStmt itself, not a Begin block.
TEST_F(AlwaysLatchTest, AlwaysStmtIsDirectlyAnIfStmt) {
  EXPECT_NE(getIfStmt(), nullptr)
      << "9.2.2.3: a single-statement 'always_latch' body (no begin/end) must not be wrapped in a Begin block";
}

// --- if(a) q <= b; -----------------------------------------------------------

TEST_F(AlwaysLatchTest, IfConditionIsRefObjResolvingToNetA) {
  const hldb::IfStmt *const ifStmt = getIfStmt();
  ASSERT_NE(ifStmt, nullptr);
  const hldb::RefObj *const cond = ifStmt->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr) << "12.4: 'if(a)' condition should be a bare RefObj, not rewritten as a comparison";
  EXPECT_EQ(cond->getName(), "a");
  EXPECT_EQ(cond->getActual<hldb::Net>(), getNet("a"));
}

// 12.4: with no "else" clause, the then-branch statement (no begin/end) is
// held directly on the IfStmt, not wrapped in a Begin.
TEST_F(AlwaysLatchTest, IfThenBranchIsDirectlyAnAssignment) {
  EXPECT_NE(getAssignment(), nullptr)
      << "12.4: a single-statement then-branch (no begin/end) must not be wrapped in a Begin block";
}

TEST_F(AlwaysLatchTest, AssignmentIsNonBlocking) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  EXPECT_FALSE(assign->getBlocking()) << "'q <= b' uses the non-blocking assignment operator '<='";
}

TEST_F(AlwaysLatchTest, AssignmentLhsResolvesToVariableQ) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "assignment lhs should be a RefObj";
  EXPECT_EQ(lhs->getName(), "q");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariableQ());
}

TEST_F(AlwaysLatchTest, AssignmentRhsResolvesToNetB) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "assignment rhs should be a RefObj";
  EXPECT_EQ(rhs->getName(), "b");
  EXPECT_EQ(rhs->getActual<hldb::Net>(), getNet("b"));
}

// --- compiler diagnostics ----------------------------------------------------

// The construct with the most real risk of a binding failure is any of
// "a"/"b"/"q" resolving back to their declarations.
TEST_F(AlwaysLatchTest, ReferencesAreNotFailedBinds) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr)
      << "'a', 'b', and 'q' in 'if(a) q <= b;' must bind to their declarations";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

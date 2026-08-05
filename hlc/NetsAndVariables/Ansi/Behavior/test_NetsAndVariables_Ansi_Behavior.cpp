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

// Validates the UHDM graph produced for tests/NetsAndVariables/Ansi/Behavior.sv,
// split out of the combined NetsAndVariablesAnsi.sv suite so continuous
// assignments and process coverage stand on their own.
//
// Checked:
//   - explicit nets (w0, uwire_net, w_bus, implicit_wire -- despite its name,
//     it is declared 'wire implicit_wire;') are hldb::Net with vpiNetType ==
//     vpiWire, and are absent from getVariables() (no duplicate)
//   - explicit variables assigned via blocking assignment (var_logic,
//     var_bit in always_comb; var_integer in always_latch; var_real,
//     var_string in initial), via nonblocking assignment (var_int,
//     var_reg_vector in always_ff; y, the output port, in a plain always),
//     and the one assigned via continuous assignment among the truly
//     implicit identifiers (none here -- implicit_wire is explicit) are all
//     hldb::Variable, absent from getNets() (no duplicate)
//   - per IEEE 1800 6.10, there is no implicit variable: implicit_var_a /
//     implicit_var_b (procedural blocking assignment in always_comb) and
//     implicit_var_c (procedural nonblocking assignment in a separate
//     always_ff) are all undeclared identifiers assigned procedurally, so
//     none of them should appear in either getNets() or getVariables()
//   - continuous assignments -- 6 total, in source order: implicit_wire = a |
//     b; implicit_net_a = a & b; implicit_net_b = implicit_net_a | a; w0 = a
//     & b; w_bus[0] = a; uwire_net = a ^ b -- implicit_net_a / implicit_net_b
//     are the only truly implicit identifiers here (undeclared, driven only
//     by continuous assignment). HLC is a compiler, not an elaborator: per
//     6.10 an implicit net is presumed, but materializing that Net object is
//     an elaboration-time concern, so the non-elaborated HLDB graph records
//     only a RefObj with a null vpiActual -- neither a Net nor a Variable
//     for implicit_net_a / implicit_net_b should exist here
//   - processes -- always_comb(x2), always_ff(x2), always_latch, initial,
//     always = 7 total

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/any.h>
#include <hldb/assignment.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/initial.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AnsiBehaviorTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Behavior.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("nets_and_variables_test", m_design->getAllModules());
  }

  // Asserts 'name' is a hldb::Net in top->getNets() with the given
  // vpiNetType, and that no hldb::Variable with the same name exists (no
  // duplicate).
  static const hldb::Net *getNetOfType(const hldb::Module *top, std::string_view name, int32_t expectedNetType) {
    const hldb::Net *const n = hldb::findByName<hldb::Net>(name, top->getNets());
    EXPECT_NE(n, nullptr) << "net '" << name << "' not found";
    if (n != nullptr) {
      EXPECT_EQ(n->getNetType(), expectedNetType);
    }
    EXPECT_EQ(hldb::findByName<hldb::Variable>(name, top->getVariables()), nullptr)
        << "'" << name << "' is net-declared -- it must not also appear in vpiVariables";
    return n;
  }

  // Asserts 'name' is a hldb::Variable in top->getVariables(), and that no
  // hldb::Net with the same name exists (no duplicate).
  static const hldb::Variable *getVarNoNetDuplicate(const hldb::Module *top, std::string_view name) {
    const hldb::Variable *const v = hldb::findByName<hldb::Variable>(name, top->getVariables());
    EXPECT_NE(v, nullptr) << "'" << name << "' should appear in vpiVariables";
    EXPECT_EQ(hldb::findByName<hldb::Net>(name, top->getNets()), nullptr)
        << "'" << name << "' has no net-type keyword -- it must not also appear in vpiNet";
    return v;
  }

  // Per IEEE 1800 6.10, there is no implicit variable: an undeclared
  // identifier assigned only via procedural assignment must not appear in
  // either container.
  static void expectNeitherNetNorVariable(const hldb::Module *top, std::string_view name) {
    EXPECT_EQ(hldb::findByName<hldb::Net>(name, top->getNets()), nullptr)
        << "'" << name << "' is only assigned procedurally -- there is no implicit net for it";
    EXPECT_EQ(hldb::findByName<hldb::Variable>(name, top->getVariables()), nullptr)
        << "'" << name << "' is only assigned procedurally -- there is no implicit variable in SystemVerilog";
  }
};

// ---------------------------------------------------------------------------
// Explicit nets: w0, uwire_net, w_bus, implicit_wire (despite the name, it
// is declared 'wire implicit_wire;').
// ---------------------------------------------------------------------------
TEST_F(AnsiBehaviorTest, W0IsWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(getNetOfType(top, "w0", vpiWire), nullptr);
}

TEST_F(AnsiBehaviorTest, UwireNetIsWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(getNetOfType(top, "uwire_net", vpiWire), nullptr);
}

TEST_F(AnsiBehaviorTest, ImplicitWireIsActuallyExplicitWire) {
  // Despite its name, 'wire implicit_wire;' is an explicit net declaration.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(getNetOfType(top, "implicit_wire", vpiWire), nullptr);
}

TEST_F(AnsiBehaviorTest, WBusIsWireVectorSevenToZero) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const wBus = getNetOfType(top, "w_bus", vpiWire);
  ASSERT_NE(wBus, nullptr);
  const hldb::RefTypespec *const rts = wBus->getTypespec();
  ASSERT_NE(rts, nullptr);
  const hldb::LogicTypespec *const ls = rts->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_TRUE(ls->getVector());
}

// ---------------------------------------------------------------------------
// Explicit variables, exercised by every assignment kind: blocking
// (always_comb / always_latch / initial), nonblocking (always_ff / plain
// always).
// ---------------------------------------------------------------------------
TEST_F(AnsiBehaviorTest, VarLogicAssignedByBlockingIsVariable) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(getVarNoNetDuplicate(top, "var_logic"), nullptr);
}

TEST_F(AnsiBehaviorTest, VarBitAssignedByBlockingIsVariable) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(getVarNoNetDuplicate(top, "var_bit"), nullptr);
}

TEST_F(AnsiBehaviorTest, VarIntAssignedByNonblockingIsVariable) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(getVarNoNetDuplicate(top, "var_int"), nullptr);
}

TEST_F(AnsiBehaviorTest, VarRegVectorAssignedByNonblockingIsVariable) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(getVarNoNetDuplicate(top, "var_reg_vector"), nullptr);
}

TEST_F(AnsiBehaviorTest, VarIntegerAssignedInAlwaysLatchIsVariable) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(getVarNoNetDuplicate(top, "var_integer"), nullptr);
}

TEST_F(AnsiBehaviorTest, VarRealAssignedInInitialIsVariable) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(getVarNoNetDuplicate(top, "var_real"), nullptr);
}

TEST_F(AnsiBehaviorTest, VarStringAssignedInInitialIsVariable) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(getVarNoNetDuplicate(top, "var_string"), nullptr);
}

TEST_F(AnsiBehaviorTest, YPortAssignedByNonblockingIsVariable) {
  // 'output logic y' is an explicit ANSI variable port; the plain
  // 'always @(posedge clk) y <= var_logic;' nonblocking assignment must not
  // change that.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(getVarNoNetDuplicate(top, "y"), nullptr);
}

// ---------------------------------------------------------------------------
// Truly implicit identifiers: per 6.10, only continuous assignment can
// implicitly declare a net; procedural (blocking or nonblocking) assignment
// to an undeclared identifier declares nothing.
// ---------------------------------------------------------------------------
TEST_F(AnsiBehaviorTest, ImplicitNetAAndBAreNotMaterializedPreElaboration) {
  // HLC is a compiler, not an elaborator: the non-elaborated HLDB graph
  // records only what the source explicitly states. An implicitly-declared
  // net (6.10) is represented solely by a RefObj with a null vpiActual (see
  // SecondContAssignDrivesImplicitNetAWithNoActual below) -- materializing
  // the implicit Net object itself is an elaboration-time task, not this
  // compiler's job, so neither a Net nor a Variable should exist here.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("implicit_net_a", top->getNets()), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("implicit_net_b", top->getNets()), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("implicit_net_a", top->getVariables()), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("implicit_net_b", top->getVariables()), nullptr);
}

TEST_F(AnsiBehaviorTest, ImplicitVarABlockingAssignedIsNeitherNetNorVariable) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  expectNeitherNetNorVariable(top, "implicit_var_a");
}

TEST_F(AnsiBehaviorTest, ImplicitVarBBlockingAssignedIsNeitherNetNorVariable) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  expectNeitherNetNorVariable(top, "implicit_var_b");
}

TEST_F(AnsiBehaviorTest, ImplicitVarCNonblockingAssignedIsNeitherNetNorVariable) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  expectNeitherNetNorVariable(top, "implicit_var_c");
}

// ---------------------------------------------------------------------------
// Continuous assignments -- 6 total, in source order:
//   implicit_wire = a | b; implicit_net_a = a & b;
//   implicit_net_b = implicit_net_a | a; w0 = a & b; w_bus[0] = a;
//   uwire_net = a ^ b
// ---------------------------------------------------------------------------
TEST_F(AnsiBehaviorTest, SixContAssignsExist) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getContAssigns()->size(), 6u);
}

TEST_F(AnsiBehaviorTest, FirstContAssignDrivesImplicitWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_FALSE(top->getContAssigns()->empty());
  const hldb::RefObj *const lhs = top->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "implicit_wire");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr) << "'implicit_wire' is formally declared";
}

TEST_F(AnsiBehaviorTest, SecondContAssignDrivesImplicitNetAWithNoActual) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_GE(top->getContAssigns()->size(), 2u);
  const hldb::RefObj *const lhs = top->getContAssigns()->at(1)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "implicit_net_a");
  EXPECT_EQ(lhs->getActual(), nullptr) << "'implicit_net_a' is implicitly declared -- no vpiActual";
}

TEST_F(AnsiBehaviorTest, LastContAssignDrivesUwireNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_FALSE(top->getContAssigns()->empty());
  const hldb::RefObj *const lhs = top->getContAssigns()->back()->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "uwire_net");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr);
}

// ---------------------------------------------------------------------------
// Processes -- always_comb(x2), always_ff(x2), always_latch, initial, always
// = 7 total.
// ---------------------------------------------------------------------------
TEST_F(AnsiBehaviorTest, SevenProcessesExist) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 5u);
}

TEST_F(AnsiBehaviorTest, ProcessTypeCounts) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  int initialCount = 0;
  int alwaysCount = 0;
  for (const hldb::Process *const p : *top->getProcesses()) {
    if (any_cast<hldb::Initial>(p) != nullptr) initialCount++;
    if (any_cast<hldb::Always>(p) != nullptr) alwaysCount++;
  }
  EXPECT_EQ(initialCount, 1) << "one initial block (var_real/var_string)";
  EXPECT_EQ(alwaysCount, 4) << "always_comb x2, always_ff x2, always_latch, always";
}

TEST_F(AnsiBehaviorTest, PlainAlwaysDrivesYFromVarLogic) {
  // Identify the final "always @(posedge clk) y <= var_logic;" block
  // structurally (by its Assignment LHS), rather than assuming process order.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  const hldb::Always *found = nullptr;
  for (const hldb::Process *const p : *top->getProcesses()) {
    const hldb::Always *const a = any_cast<hldb::Always>(p);
    if (a == nullptr) continue;
    const hldb::EventControl *const ec = a->getStmt<hldb::EventControl>();
    if (ec == nullptr) continue;
    const hldb::Begin *const b = ec->getStmt<hldb::Begin>();
    if (b == nullptr) continue;
    const hldb::AnyCollection *const stmts = b->getStmts();
    if (stmts == nullptr) continue;
    for (const hldb::Any *s : *stmts) {
      const hldb::Assignment *const assign = any_cast<hldb::Assignment>(s);
      if (assign == nullptr) continue;
      const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
      if (lhs != nullptr && lhs->getName() == "y") {
        found = a;
        break;
      }
    }
    if (found != nullptr) break;
  }
  ASSERT_NE(found, nullptr) << "could not find 'always @(posedge clk) y <= var_logic;'";
  EXPECT_EQ(found->getAlwaysType(), vpiAlways);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

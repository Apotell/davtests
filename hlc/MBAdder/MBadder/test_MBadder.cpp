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

// Tests for tests/MBAdder/dut.sv (tags: MBAdder)
//
//   module MultibitAdder(a,b,cin,sum,cout);
//     input [3:0] a,b;
//     input cin;
//     output [3:0]sum;
//     output cout;
//     assign {cout,sum}=a+b+cin;
//   endmodule
//
// What is checked:
//   - module "MultibitAdder" exists
//   - Sec 23.2.2.1 (non-ANSI style port declarations): the port list
//     declares 5 ports in order a, b, cin, sum, cout; their directions come
//     from the port declarations: a, b, cin are input, sum and cout output
//   - Sec 23.2.2.1 / 6.10: the port declarations carry no net or variable
//     type, so each port is a net of the default net type (wire, no
//     `default_nettype in this file); a, b, sum are 4-bit vectors with
//     packed range [3:0], cin and cout are scalar
//   - Sec 10.3.2: there is exactly one continuous assignment
//   - Sec 11.4.12: its LHS "{cout,sum}" is Operation(vpiConcatOp) with
//     operands RefObj "cout" then RefObj "sum" (source order)
//   - Sec 11.3.2 (operator precedence and associativity): binary "+" is
//     left-associative, so "a+b+cin" is ((a+b)+cin): Operation(vpiAddOp)
//     whose first operand is Operation(vpiAddOp) over a, b and whose second
//     operand is RefObj "cin"
//   - the references in the assignment bind to the port nets (no
//     COMP_FAILED_TO_BIND for any of them)
//
// What is NOT checked and why:
//   - Bit-width of the addition result / carry extension (11.6): an
//     expression-sizing result computed at elaboration; this compile
//     stops before elaboration.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

#include <string>
#include <vector>

namespace hlc {

class MBadderTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "MBadder.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getAdder() {
    return hldb::findByDefName<hldb::Module>("MultibitAdder", m_design->getAllModules());
  }

  static const hldb::Net *getNet(std::string_view name) {
    const hldb::Module *const m = getAdder();
    if (m == nullptr || m->getNets() == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>(name, m->getNets());
  }

  static const hldb::ContAssign *getAssign() {
    const hldb::Module *const m = getAdder();
    if (m == nullptr || m->getContAssigns() == nullptr || m->getContAssigns()->empty()) return nullptr;
    return m->getContAssigns()->at(0);
  }

  // Returns the single packed range of a net's logic typespec, or nullptr.
  static const hldb::Range *getPackedRange(const hldb::Net *net) {
    if (net == nullptr || net->getTypespec() == nullptr) return nullptr;
    const hldb::LogicTypespec *const lt = net->getTypespec()->getActual<hldb::LogicTypespec>();
    if (lt == nullptr || lt->getRanges() == nullptr || lt->getRanges()->size() != 1) return nullptr;
    return lt->getRanges()->at(0);
  }
};

TEST_F(MBadderTest, ModuleExists) { EXPECT_NE(getAdder(), nullptr) << "module 'MultibitAdder' not found"; }

// ---------------------------------------------------------------------------
// Sec 23.2.2.1: (a,b,cin,sum,cout) with input/output declarations
// ---------------------------------------------------------------------------

TEST_F(MBadderTest, FivePortsInOrderWithDirections) {
  const hldb::Module *const m = getAdder();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getPorts(), nullptr);
  ASSERT_EQ(m->getPorts()->size(), 5u);
  const std::vector<std::string> names = {"a", "b", "cin", "sum", "cout"};
  const std::vector<int32_t> dirs = {vpiInput, vpiInput, vpiInput, vpiOutput, vpiOutput};
  for (size_t i = 0; i < names.size(); ++i) {
    const hldb::Port *const p = m->getPorts()->at(i);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->getName(), names[i]) << "port #" << i;
    EXPECT_EQ(p->getDirection(), dirs[i]) << "port '" << names[i] << "'";
  }
}

TEST_F(MBadderTest, PortsAreWireNets) {
  for (const char *const name : {"a", "b", "cin", "sum", "cout"}) {
    const hldb::Net *const net = getNet(name);
    ASSERT_NE(net, nullptr) << "Sec 23.2.2.1: port '" << name << "' implies a net of the default net type";
    EXPECT_EQ(net->getNetType(), vpiWire) << "'" << name << "': default net type is wire";
  }
}

TEST_F(MBadderTest, VectorPortsHaveRangeThreeDownToZero) {
  for (const char *const name : {"a", "b", "sum"}) {
    const hldb::Net *const net = getNet(name);
    ASSERT_NE(net, nullptr) << name;
    const hldb::Range *const r = getPackedRange(net);
    ASSERT_NE(r, nullptr) << "'" << name << "' is declared with [3:0]";
    const hldb::Constant *const left = r->getLeftExpr<hldb::Constant>();
    const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(left->getDecompile(), "3") << name;
    EXPECT_EQ(right->getDecompile(), "0") << name;
  }
}

TEST_F(MBadderTest, CinAndCoutAreScalar) {
  for (const char *const name : {"cin", "cout"}) {
    const hldb::Net *const net = getNet(name);
    ASSERT_NE(net, nullptr) << name;
    EXPECT_EQ(getPackedRange(net), nullptr) << "'" << name << "' has no packed dimension";
  }
}

// ---------------------------------------------------------------------------
// Sec 10.3.2 / 11.4.12 / 11.3.2: assign {cout,sum}=a+b+cin;
// ---------------------------------------------------------------------------

TEST_F(MBadderTest, OneContinuousAssignment) {
  const hldb::Module *const m = getAdder();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getContAssigns(), nullptr);
  EXPECT_EQ(m->getContAssigns()->size(), 1u);
}

TEST_F(MBadderTest, LhsIsConcatOfCoutAndSum) {
  const hldb::ContAssign *const ca = getAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::Operation *const lhs = ca->getLhs<hldb::Operation>();
  ASSERT_NE(lhs, nullptr) << "'{cout,sum}' must be an Operation";
  EXPECT_EQ(lhs->getOpType(), vpiConcatOp);
  ASSERT_NE(lhs->getOperands(), nullptr);
  ASSERT_EQ(lhs->getOperands()->size(), 2u);
  const hldb::RefObj *const first = any_cast<hldb::RefObj>(lhs->getOperands()->at(0));
  const hldb::RefObj *const second = any_cast<hldb::RefObj>(lhs->getOperands()->at(1));
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(first->getName(), "cout");
  EXPECT_EQ(second->getName(), "sum");
}

TEST_F(MBadderTest, RhsIsLeftAssociativeAddition) {
  const hldb::ContAssign *const ca = getAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::Operation *const outer = ca->getRhs<hldb::Operation>();
  ASSERT_NE(outer, nullptr) << "'a+b+cin' must be an Operation";
  EXPECT_EQ(outer->getOpType(), vpiAddOp);
  ASSERT_NE(outer->getOperands(), nullptr);
  ASSERT_EQ(outer->getOperands()->size(), 2u) << "Sec 11.3.2: binary '+' -> ((a+b)+cin)";

  const hldb::Operation *const inner = any_cast<hldb::Operation>(outer->getOperands()->at(0));
  ASSERT_NE(inner, nullptr) << "first operand must be the nested '(a+b)'";
  EXPECT_EQ(inner->getOpType(), vpiAddOp);
  ASSERT_NE(inner->getOperands(), nullptr);
  ASSERT_EQ(inner->getOperands()->size(), 2u);
  const hldb::RefObj *const a = any_cast<hldb::RefObj>(inner->getOperands()->at(0));
  const hldb::RefObj *const b = any_cast<hldb::RefObj>(inner->getOperands()->at(1));
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(a->getName(), "a");
  EXPECT_EQ(b->getName(), "b");

  const hldb::RefObj *const cin = any_cast<hldb::RefObj>(outer->getOperands()->at(1));
  ASSERT_NE(cin, nullptr);
  EXPECT_EQ(cin->getName(), "cin");
}

TEST_F(MBadderTest, AssignmentReferencesBindToPortNets) {
  const hldb::ContAssign *const ca = getAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::Operation *const lhs = ca->getLhs<hldb::Operation>();
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(lhs->getOperands(), nullptr);
  for (const hldb::Any *const op : *lhs->getOperands()) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(op);
    ASSERT_NE(ref, nullptr);
    ASSERT_NE(getNet(ref->getName()), nullptr);
    EXPECT_EQ(ref->getActual(), getNet(ref->getName())) << "'" << ref->getName() << "' must bind to its port net";
  }
  for (const char *const name : {"a", "b", "cin", "sum", "cout"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, name), nullptr) << name;
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

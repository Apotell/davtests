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

// Tests for tests/ModPortHighConn/dut.sv (tags: ModPortHighConn)
//   module moduleA (inout port0, port1);
//   endmodule
//
//   module top;
//       wire [1:0] topA, topB;
//       moduleA instanceA(.port0(topA[0]), .port1(topB[0]));
//       moduleA instanceB(topA[1], topB[0]);
//       moduleA instanceC(topA[1], topB[0]||topB[1]);
//       moduleA instanceD(topA[1]||topA[0], topB[0]);
//   endmodule
//
// The file exercises the "high connection" (the instantiating side) of
// module port connections, both by name and by ordered list, including
// expressions that are not simple net references.
//
// What is checked (IEEE 1800-2023):
//   - 23.2.2.3: in moduleA's ANSI port list, port0 is explicitly "inout";
//     port1 is a subsequent port with no direction/kind/type, so it inherits
//     the direction (inout) of the previous port. Both are nets of the
//     default net type (wire), 1-bit (scalar), since the data type is
//     omitted (defaults to logic).
//   - 6.7: "wire [1:0] topA, topB;" declares two 2-bit vector wire nets with
//     no net_decl_assignment (no "= expr"), so vpiNetDeclAssign is false.
//   - 23.3.1/23.3.2: top contains four instances of moduleA, each with two
//     port connections.
//   - 23.3.2.2: instanceA connects by name -- each connection's low side
//     names the formal port (port0/port1) and is flagged as connected by name
//     (vpiConnByName); the high side is the bit-select topA[0] / topB[0].
//   - 23.3.2.1: instanceB connects by ordered list -- connections bind
//     positionally to port0 then port1 (low side resolves to moduleA's Port
//     objects); the high side is topA[1] / topB[0]; not connected by name.
//   - 11.4.7: instanceC's second and instanceD's first high connections are
//     the logical-OR expressions "topB[0]||topB[1]" and "topA[1]||topA[0]",
//     each an Operation(vpiLogOrOp) with two bit-select operands, in source
//     order.
//
// What is NOT checked and why:
//   - 23.3.3.3: "An inout can be connected to a net (or a concatenation of
//     nets) ... but cannot be connected to a variable." The "||" expressions
//     connected to the inout ports of instanceC/instanceD are neither nets
//     nor concatenations of nets, so those two connections are illegal.
//     There is no dedicated ErrorDefinition entry for "non-net expression
//     connected to an inout port" (COMP_ILLEGAL_VARIABLE_ON_INOUT_PORT is
//     specifically about variables), so no findError() assertion is made;
//     this is flagged here instead of guessed at.
//   - 23.3.2.1: the ordered connections reuse topA[1] for port0 of three
//     instances; multiple connections of the same net to inout ports is
//     legal (nets may have multiple drivers, 6.5), so nothing is asserted.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class ModPortHighConnTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ModPortHighConn.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::RefInstance *getInstance(std::string_view name) {
    const hldb::Module *const top = getModule("top");
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>(name, top->getRefInstances());
  }

  // Returns the idx-th port connection of the named instance in top.
  static const hldb::Port *getConn(std::string_view inst, size_t idx) {
    const hldb::RefInstance *const ri = getInstance(inst);
    if ((ri == nullptr) || (ri->getPorts() == nullptr) || (ri->getPorts()->size() <= idx)) return nullptr;
    return any_cast<hldb::Port>(ri->getPorts()->at(idx));
  }

  // Checks that expr is a BitSelect "<prefix>[<index>]" with a constant index.
  static void expectBitSelect(const hldb::Any *expr, std::string_view prefix, std::string_view index) {
    ASSERT_NE(expr, nullptr);
    ASSERT_EQ(expr->getAnyType(), hldb::AnyType::BitSelect);
    const hldb::BitSelect *const bs = any_cast<hldb::BitSelect>(expr);
    const hldb::RefObj *const pfx = bs->getPrefix<hldb::RefObj>();
    ASSERT_NE(pfx, nullptr);
    EXPECT_EQ(pfx->getName(), prefix);
    const hldb::Constant *const idx = bs->getIndex<hldb::Constant>();
    ASSERT_NE(idx, nullptr);
    EXPECT_EQ(idx->getDecompile(), index);
  }
};

// ===========================================================================
// moduleA: (inout port0, port1)
// ===========================================================================

TEST_F(ModPortHighConnTest, ModuleAHasTwoInoutPorts) {
  const hldb::Module *const modA = getModule("moduleA");
  ASSERT_NE(modA, nullptr);
  ASSERT_NE(modA->getPorts(), nullptr);
  ASSERT_EQ(modA->getPorts()->size(), 2u);
  EXPECT_EQ(modA->getPorts()->at(0)->getName(), "port0");
  EXPECT_EQ(modA->getPorts()->at(1)->getName(), "port1");
  EXPECT_EQ(modA->getPorts()->at(0)->getDirection(), vpiInout) << "23.2.2.3: port0 is declared inout";
  EXPECT_EQ(modA->getPorts()->at(1)->getDirection(), vpiInout)
      << "23.2.2.3: port1 inherits the direction of the preceding port";
}

TEST_F(ModPortHighConnTest, ModuleAPortsAreScalarWireNets) {
  const hldb::Module *const modA = getModule("moduleA");
  ASSERT_NE(modA, nullptr);
  ASSERT_NE(modA->getNets(), nullptr);
  for (std::string_view name : {"port0", "port1"}) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(name, modA->getNets());
    ASSERT_NE(net, nullptr) << name;
    EXPECT_EQ(net->getNetType(), vpiWire) << "23.2.2.3: inout port defaults to a net of default net type";
    EXPECT_TRUE(net->getScalar()) << name << ": implicit logic data type is 1 bit";
    EXPECT_FALSE(net->getVector()) << name;
  }
}

// ===========================================================================
// top: wire [1:0] topA, topB;
// ===========================================================================

TEST_F(ModPortHighConnTest, TopHasTwoVectorWireNetsWithoutDeclAssign) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  for (std::string_view name : {"topA", "topB"}) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(name, top->getNets());
    ASSERT_NE(net, nullptr) << name;
    EXPECT_EQ(net->getNetType(), vpiWire) << name;
    EXPECT_TRUE(net->getVector()) << name << ": declared with packed range [1:0]";
    EXPECT_FALSE(net->getScalar()) << name;
    EXPECT_FALSE(net->getNetDeclAssign()) << name << ": 6.7 declaration has no net_decl_assignment";
  }
}

TEST_F(ModPortHighConnTest, TopHasFourInstancesOfModuleA) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  ASSERT_EQ(top->getRefInstances()->size(), 4u);
  for (std::string_view name : {"instanceA", "instanceB", "instanceC", "instanceD"}) {
    const hldb::RefInstance *const ri = getInstance(name);
    ASSERT_NE(ri, nullptr) << name;
    ASSERT_NE(ri->getTypespec(), nullptr) << name;
    const hldb::ModuleTypespec *const mt = ri->getTypespec()->getActual<hldb::ModuleTypespec>();
    ASSERT_NE(mt, nullptr) << name;
    EXPECT_EQ(mt->getModule(), getModule("moduleA")) << name;
    ASSERT_NE(ri->getPorts(), nullptr) << name;
    EXPECT_EQ(ri->getPorts()->size(), 2u) << name;
  }
}

// ===========================================================================
// 23.3.2.2: instanceA(.port0(topA[0]), .port1(topB[0]))
// ===========================================================================

TEST_F(ModPortHighConnTest, InstanceANamedConnections) {
  const hldb::Port *const c0 = getConn("instanceA", 0);
  ASSERT_NE(c0, nullptr);
  const hldb::RefObj *const lo0 = c0->getLowConn<hldb::RefObj>();
  ASSERT_NE(lo0, nullptr);
  EXPECT_EQ(lo0->getName(), "port0");
  expectBitSelect(c0->getHighConn(), "topA", "0");

  const hldb::Port *const c1 = getConn("instanceA", 1);
  ASSERT_NE(c1, nullptr);
  const hldb::RefObj *const lo1 = c1->getLowConn<hldb::RefObj>();
  ASSERT_NE(lo1, nullptr);
  EXPECT_EQ(lo1->getName(), "port1");
  expectBitSelect(c1->getHighConn(), "topB", "0");
}

TEST_F(ModPortHighConnTest, InstanceAConnectionsAreFlaggedConnByName) {
  for (size_t i = 0; i < 2; ++i) {
    const hldb::Port *const c = getConn("instanceA", i);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->getConnByName()) << "23.3.2.2: '.port(expr)' connection " << i << " is by name";
  }
}

TEST_F(ModPortHighConnTest, InstanceANamedConnectionsBindToFormalPorts) {
  const hldb::Module *const modA = getModule("moduleA");
  ASSERT_NE(modA, nullptr);
  ASSERT_NE(modA->getPorts(), nullptr);
  ASSERT_EQ(modA->getPorts()->size(), 2u);
  for (size_t i = 0; i < 2; ++i) {
    const hldb::Port *const c = getConn("instanceA", i);
    ASSERT_NE(c, nullptr);
    const hldb::RefObj *const lo = c->getLowConn<hldb::RefObj>();
    ASSERT_NE(lo, nullptr);
    EXPECT_EQ(lo->getActual(), modA->getPorts()->at(i)) << "named connection " << i;
  }
}

// ===========================================================================
// 23.3.2.1: instanceB(topA[1], topB[0])
// ===========================================================================

TEST_F(ModPortHighConnTest, InstanceBOrderedConnectionsBindPositionally) {
  const hldb::Module *const modA = getModule("moduleA");
  ASSERT_NE(modA, nullptr);
  ASSERT_NE(modA->getPorts(), nullptr);
  ASSERT_EQ(modA->getPorts()->size(), 2u);

  const hldb::Port *const c0 = getConn("instanceB", 0);
  ASSERT_NE(c0, nullptr);
  const hldb::RefObj *const lo0 = c0->getLowConn<hldb::RefObj>();
  ASSERT_NE(lo0, nullptr);
  EXPECT_EQ(lo0->getActual(), modA->getPorts()->at(0)) << "first ordered connection binds to port0";
  expectBitSelect(c0->getHighConn(), "topA", "1");
  EXPECT_FALSE(c0->getConnByName());

  const hldb::Port *const c1 = getConn("instanceB", 1);
  ASSERT_NE(c1, nullptr);
  const hldb::RefObj *const lo1 = c1->getLowConn<hldb::RefObj>();
  ASSERT_NE(lo1, nullptr);
  EXPECT_EQ(lo1->getActual(), modA->getPorts()->at(1)) << "second ordered connection binds to port1";
  expectBitSelect(c1->getHighConn(), "topB", "0");
  EXPECT_FALSE(c1->getConnByName());
}

TEST_F(ModPortHighConnTest, HighConnBitSelectPrefixesBindToTopNets) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  const hldb::Net *const topA = hldb::findByName<hldb::Net>("topA", top->getNets());
  const hldb::Net *const topB = hldb::findByName<hldb::Net>("topB", top->getNets());
  ASSERT_NE(topA, nullptr);
  ASSERT_NE(topB, nullptr);

  const hldb::Port *const c0 = getConn("instanceB", 0);
  ASSERT_NE(c0, nullptr);
  const hldb::BitSelect *const bs0 = c0->getHighConn<hldb::BitSelect>();
  ASSERT_NE(bs0, nullptr);
  const hldb::RefObj *const p0 = bs0->getPrefix<hldb::RefObj>();
  ASSERT_NE(p0, nullptr);
  EXPECT_EQ(p0->getActual(), topA);

  const hldb::Port *const c1 = getConn("instanceB", 1);
  ASSERT_NE(c1, nullptr);
  const hldb::BitSelect *const bs1 = c1->getHighConn<hldb::BitSelect>();
  ASSERT_NE(bs1, nullptr);
  const hldb::RefObj *const p1 = bs1->getPrefix<hldb::RefObj>();
  ASSERT_NE(p1, nullptr);
  EXPECT_EQ(p1->getActual(), topB);
}

// ===========================================================================
// 11.4.7: expression high connections
// ===========================================================================

TEST_F(ModPortHighConnTest, InstanceCSecondHighConnIsLogicalOr) {
  const hldb::Port *const c0 = getConn("instanceC", 0);
  ASSERT_NE(c0, nullptr);
  expectBitSelect(c0->getHighConn(), "topA", "1");

  const hldb::Port *const c1 = getConn("instanceC", 1);
  ASSERT_NE(c1, nullptr);
  const hldb::Operation *const op = c1->getHighConn<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "'topB[0]||topB[1]' should be an Operation";
  EXPECT_EQ(op->getOpType(), vpiLogOrOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  expectBitSelect(op->getOperands()->at(0), "topB", "0");
  expectBitSelect(op->getOperands()->at(1), "topB", "1");
}

TEST_F(ModPortHighConnTest, InstanceDFirstHighConnIsLogicalOr) {
  const hldb::Port *const c0 = getConn("instanceD", 0);
  ASSERT_NE(c0, nullptr);
  const hldb::Operation *const op = c0->getHighConn<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "'topA[1]||topA[0]' should be an Operation";
  EXPECT_EQ(op->getOpType(), vpiLogOrOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  expectBitSelect(op->getOperands()->at(0), "topA", "1");
  expectBitSelect(op->getOperands()->at(1), "topA", "0");

  const hldb::Port *const c1 = getConn("instanceD", 1);
  ASSERT_NE(c1, nullptr);
  expectBitSelect(c1->getHighConn(), "topB", "0");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

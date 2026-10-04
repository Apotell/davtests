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

// Tests for tests/HighConnPart/dut.sv:
//
//   `default_nettype none
//
//   module Device(
//       input wire [7:0] doubleNibble,
//       output wire [3:0] sum
//   );
//       Helper instance1(doubleNibble[7:4], doubleNibble[3:0], sum);
//
//       wire [3:0] ignored;
//       Helper instance2(
//           .a(doubleNibble[7:4]),
//           .b(doubleNibble[3:0]),
//           .result(ignored)
//       );
//   endmodule
//
//   module Helper(
//       input wire [3:0] a, b,
//       output wire [3:0] result
//   );
//       assign result = a + b;
//   endmodule
//
// HighConnPart.hlc compiles at "-d db -d ast" (no "-d inst"), so instance
// port connections stay in their unelaborated RefInstance::getPorts() form
// (each connection a Port object per IEEE 1800-2023 Annex A.1.3
// "list_of_port_connections"), rather than being resolved against Helper's
// formal ports.
//
// What is under test: a constant part-select ("doubleNibble[7:4]",
// "doubleNibble[3:0]") used as the actual/high-connection expression on an
// instance port (IEEE 1800-2023 Sec 11.5.1 "Vector bit-select and
// part-select addressing", Sec 23.3.2 "Port connection rules"), in both the
// ordered-connection style (instance1) and the named-connection style
// (instance2). A part-select is a PartSelect whose getPrefix() is a RefObj
// to the base signal and whose getRange() carries the constant bit bounds.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/part_select.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class HighConnPartTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HighConnPart.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::RefInstance *findRefInst(std::string_view instName, const hldb::Module *parent) {
    if (parent == nullptr || parent->getRefInstances() == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>(instName, parent->getRefInstances());
  }

  // RefInstance::getPorts() is a generic AnyCollection (each element a
  // Port), so it cannot go through the T::getName()-based findByName<>()
  // helper -- named-connection lookup is done by hand here.
  static const hldb::Port *findNamedPort(const hldb::RefInstance *inst, std::string_view formalName) {
    if (inst == nullptr || inst->getPorts() == nullptr) return nullptr;
    for (const hldb::Any *const p : *inst->getPorts()) {
      const hldb::Port *const port = any_cast<hldb::Port>(p);
      if (port != nullptr && port->getName() == formalName) return port;
    }
    return nullptr;
  }

  // Verifies that 'conn's high-connection is "base[left:right]" -- a
  // PartSelect whose prefix is a RefObj named 'base' and whose range is the
  // constant pair (left, right).
  static void expectPartSelectHighConn(const hldb::Port *conn, std::string_view base, std::string_view left,
                                        std::string_view right) {
    ASSERT_NE(conn, nullptr);
    const hldb::PartSelect *const sel = conn->getHighConn<hldb::PartSelect>();
    ASSERT_NE(conn->getHighConn(), nullptr) << "port connection has no high-conn";
    ASSERT_NE(sel, nullptr) << "high-conn must be a PartSelect (constant part-select)";

    const hldb::RefObj *const prefix = sel->getPrefix<hldb::RefObj>();
    ASSERT_NE(sel->getPrefix(), nullptr);
    ASSERT_NE(prefix, nullptr) << "part-select prefix must be a RefObj";
    EXPECT_EQ(prefix->getName(), base);

    ASSERT_NE(sel->getRange(), nullptr);
    const hldb::Constant *const leftExpr = sel->getRange()->getLeftExpr<hldb::Constant>();
    const hldb::Constant *const rightExpr = sel->getRange()->getRightExpr<hldb::Constant>();
    ASSERT_NE(leftExpr, nullptr) << "range left bound must be a Constant";
    ASSERT_NE(rightExpr, nullptr) << "range right bound must be a Constant";
    EXPECT_EQ(std::string(leftExpr->getDecompile()), left);
    EXPECT_EQ(std::string(rightExpr->getDecompile()), right);
  }
};

// ---------------------------------------------------------------------------
// Module existence
// ---------------------------------------------------------------------------

TEST_F(HighConnPartTest, ModulesExist) {
  EXPECT_NE(getModule("Device"), nullptr) << "module 'Device' not found";
  EXPECT_NE(getModule("Helper"), nullptr) << "module 'Helper' not found";
}

TEST_F(HighConnPartTest, HelperFormalPortsExistWithCorrectDirections) {
  const hldb::Module *const helper = getModule("Helper");
  ASSERT_NE(helper, nullptr);
  ASSERT_NE(helper->getPorts(), nullptr);
  const hldb::Port *const a = hldb::findByName<hldb::Port>("a", helper->getPorts());
  const hldb::Port *const b = hldb::findByName<hldb::Port>("b", helper->getPorts());
  const hldb::Port *const result = hldb::findByName<hldb::Port>("result", helper->getPorts());
  ASSERT_NE(a, nullptr) << "formal port 'a' not found";
  ASSERT_NE(b, nullptr) << "formal port 'b' not found";
  ASSERT_NE(result, nullptr) << "formal port 'result' not found";
  EXPECT_EQ(a->getDirection(), vpiInput);
  EXPECT_EQ(b->getDirection(), vpiInput);
  EXPECT_EQ(result->getDirection(), vpiOutput);
}

// ---------------------------------------------------------------------------
// instance1: 'Helper instance1(doubleNibble[7:4], doubleNibble[3:0], sum);'
// -- ordered (positional) port connections, Sec 23.3.2.
// ---------------------------------------------------------------------------

TEST_F(HighConnPartTest, Instance1ExistsWithThreeOrderedPortConnections) {
  const hldb::Module *const device = getModule("Device");
  ASSERT_NE(device, nullptr);
  const hldb::RefInstance *const inst1 = findRefInst("instance1", device);
  ASSERT_NE(inst1, nullptr) << "'Helper instance1(...)' RefInstance not found";
  ASSERT_NE(inst1->getPorts(), nullptr);
  ASSERT_EQ(inst1->getPorts()->size(), 3u);

  for (size_t i = 0u; i < 3u; ++i) {
    const hldb::Port *const conn = any_cast<hldb::Port>(inst1->getPorts()->at(i));
    ASSERT_NE(conn, nullptr) << "position " << i;
    EXPECT_FALSE(conn->getConnByName()) << "ordered connection at position " << i << " must not be by-name";
  }
}

TEST_F(HighConnPartTest, Instance1FirstPositionIsDoubleNibbleHighNibble) {
  const hldb::Module *const device = getModule("Device");
  ASSERT_NE(device, nullptr);
  const hldb::RefInstance *const inst1 = findRefInst("instance1", device);
  ASSERT_NE(inst1, nullptr);
  ASSERT_NE(inst1->getPorts(), nullptr);
  ASSERT_EQ(inst1->getPorts()->size(), 3u);

  const hldb::Port *const conn = any_cast<hldb::Port>(inst1->getPorts()->at(0));
  expectPartSelectHighConn(conn, "doubleNibble", "7", "4");
}

TEST_F(HighConnPartTest, Instance1SecondPositionIsDoubleNibbleLowNibble) {
  const hldb::Module *const device = getModule("Device");
  ASSERT_NE(device, nullptr);
  const hldb::RefInstance *const inst1 = findRefInst("instance1", device);
  ASSERT_NE(inst1, nullptr);
  ASSERT_NE(inst1->getPorts(), nullptr);
  ASSERT_EQ(inst1->getPorts()->size(), 3u);

  const hldb::Port *const conn = any_cast<hldb::Port>(inst1->getPorts()->at(1));
  expectPartSelectHighConn(conn, "doubleNibble", "3", "0");
}

TEST_F(HighConnPartTest, Instance1ThirdPositionIsBareSumReference) {
  const hldb::Module *const device = getModule("Device");
  ASSERT_NE(device, nullptr);
  const hldb::RefInstance *const inst1 = findRefInst("instance1", device);
  ASSERT_NE(inst1, nullptr);
  ASSERT_NE(inst1->getPorts(), nullptr);
  ASSERT_EQ(inst1->getPorts()->size(), 3u);

  const hldb::Port *const conn = any_cast<hldb::Port>(inst1->getPorts()->at(2));
  ASSERT_NE(conn, nullptr);
  const hldb::RefObj *const highConn = conn->getHighConn<hldb::RefObj>();
  ASSERT_NE(conn->getHighConn(), nullptr);
  ASSERT_NE(highConn, nullptr) << "'sum' is a bare reference, not a part-select";
  EXPECT_EQ(highConn->getName(), std::string_view{"sum"});
}

// ---------------------------------------------------------------------------
// instance2: named port connections '.a(...)', '.b(...)', '.result(...)' --
// Sec 23.3.2.
// ---------------------------------------------------------------------------

TEST_F(HighConnPartTest, Instance2ExistsWithThreeNamedPortConnections) {
  const hldb::Module *const device = getModule("Device");
  ASSERT_NE(device, nullptr);
  const hldb::RefInstance *const inst2 = findRefInst("instance2", device);
  ASSERT_NE(inst2, nullptr) << "'Helper instance2(...)' RefInstance not found";
  ASSERT_NE(inst2->getPorts(), nullptr);
  ASSERT_EQ(inst2->getPorts()->size(), 3u);

  for (const hldb::Any *const p : *inst2->getPorts()) {
    const hldb::Port *const conn = any_cast<hldb::Port>(p);
    ASSERT_NE(conn, nullptr);
    EXPECT_TRUE(conn->getConnByName()) << "instance2 uses only named connections";
  }
}

TEST_F(HighConnPartTest, Instance2NamedPortAIsDoubleNibbleHighNibble) {
  const hldb::Module *const device = getModule("Device");
  ASSERT_NE(device, nullptr);
  const hldb::RefInstance *const inst2 = findRefInst("instance2", device);
  ASSERT_NE(inst2, nullptr);
  const hldb::Port *const conn = findNamedPort(inst2, "a");
  ASSERT_NE(conn, nullptr) << "'.a(doubleNibble[7:4])' not found";
  expectPartSelectHighConn(conn, "doubleNibble", "7", "4");
}

TEST_F(HighConnPartTest, Instance2NamedPortBIsDoubleNibbleLowNibble) {
  const hldb::Module *const device = getModule("Device");
  ASSERT_NE(device, nullptr);
  const hldb::RefInstance *const inst2 = findRefInst("instance2", device);
  ASSERT_NE(inst2, nullptr);
  const hldb::Port *const conn = findNamedPort(inst2, "b");
  ASSERT_NE(conn, nullptr) << "'.b(doubleNibble[3:0])' not found";
  expectPartSelectHighConn(conn, "doubleNibble", "3", "0");
}

TEST_F(HighConnPartTest, Instance2NamedPortResultIsBareIgnoredReference) {
  const hldb::Module *const device = getModule("Device");
  ASSERT_NE(device, nullptr);
  const hldb::RefInstance *const inst2 = findRefInst("instance2", device);
  ASSERT_NE(inst2, nullptr);
  const hldb::Port *const conn = findNamedPort(inst2, "result");
  ASSERT_NE(conn, nullptr) << "'.result(ignored)' not found";
  const hldb::RefObj *const highConn = conn->getHighConn<hldb::RefObj>();
  ASSERT_NE(conn->getHighConn(), nullptr);
  ASSERT_NE(highConn, nullptr) << "'ignored' is a bare reference, not a part-select";
  EXPECT_EQ(highConn->getName(), std::string_view{"ignored"});
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

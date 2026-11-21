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

// IEEE 1800-2023 Sec 25.3 with Sec 23.3.2: interfaces are instantiated like modules, including
// arrays of instances ("bus_if u_bus[3:0]();" -- four bus_if instances, range [3:0]).
// Source: tests/Google/chapter-25/ch25_suite2/25.3--if_instance_array.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/module.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class IfInstanceArrayTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3--if_instance_array.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::RefInstance *getArrayInstance() {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>("u_bus", top->getRefInstances());
  }

  static const hldb::ArrayTypespec *getArrayTypespec() {
    const hldb::RefInstance *const inst = getArrayInstance();
    if (inst == nullptr || inst->getTypespec() == nullptr) return nullptr;
    return inst->getTypespec()->getActual<hldb::ArrayTypespec>();
  }
};

TEST_F(IfInstanceArrayTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfInstanceArrayTest, BusIfHasVariableReq) {
  // Lines 23-25: "interface bus_if; logic req; endinterface".
  const hldb::Interface *const iface = hldb::findByDefName<hldb::Interface>("bus_if", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("req", iface->getVariables()), nullptr);
}

TEST_F(IfInstanceArrayTest, TopHasOneInstanceUBus) {
  // Lines 27-29: u_bus is the single instance in top.
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  EXPECT_EQ(top->getRefInstances()->size(), 1u);
  EXPECT_NE(getArrayInstance(), nullptr);
}

TEST_F(IfInstanceArrayTest, UBusIsAnArray) {
  // Line 28: "[3:0]" makes u_bus an array of instances.
  EXPECT_NE(getArrayTypespec(), nullptr) << "u_bus is not typed by an array typespec";
}

TEST_F(IfInstanceArrayTest, ArrayRangeIs3To0) {
  // Line 28: the declared range is [3:0].
  const hldb::ArrayTypespec *const ats = getArrayTypespec();
  ASSERT_NE(ats, nullptr);
  const hldb::Range *const range = ats->getRange();
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const left = range->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const right = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->getDecompile(), "3");
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(IfInstanceArrayTest, ElementTypeIsBusIf) {
  // Line 28: each element is a bus_if instance.
  const hldb::ArrayTypespec *const ats = getArrayTypespec();
  ASSERT_NE(ats, nullptr);
  ASSERT_NE(ats->getElemTypespec(), nullptr);
  const hldb::InterfaceTypespec *const its = ats->getElemTypespec()->getActual<hldb::InterfaceTypespec>();
  ASSERT_NE(its, nullptr);
  EXPECT_EQ(its->getInterface(), hldb::findByDefName<hldb::Interface>("bus_if", m_design->getAllInterfaces()));
}

TEST_F(IfInstanceArrayTest, InstanceHasNoConnections) {
  // Line 28: "()" connects nothing.
  const hldb::RefInstance *const inst = getArrayInstance();
  ASSERT_NE(inst, nullptr);
  EXPECT_TRUE(inst->getArguments() == nullptr || inst->getArguments()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

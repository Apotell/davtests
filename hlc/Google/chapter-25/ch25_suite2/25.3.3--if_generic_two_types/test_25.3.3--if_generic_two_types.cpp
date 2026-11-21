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

// IEEE 1800-2023 Sec 25.3.3: a generic interface port accepts an instance of any interface, so the
// same module can be instantiated with an a_if (s1) and with a b_if (s2) without error.
// Source: tests/Google/chapter-25/ch25_suite2/25.3.3--if_generic_two_types.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class IfGenericTwoTypesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3.3--if_generic_two_types.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Interface *getInterface(std::string_view name) {
    return hldb::findByDefName<hldb::Interface>(name, m_design->getAllInterfaces());
  }

  static const hldb::RefInstance *getTopInstance(std::string_view name) {
    const hldb::Module *const top = getModule("top");
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>(name, top->getRefInstances());
  }

  // The single connection of a sink instance (s1 or s2).
  static const hldb::NamedArgument *getConnection(std::string_view instName) {
    const hldb::RefInstance *const inst = getTopInstance(instName);
    if (inst == nullptr || inst->getArguments() == nullptr || inst->getArguments()->size() != 1) return nullptr;
    return inst->getArguments()->at(0);
  }

  static void expectInstanceOf(std::string_view instName, std::string_view ifName) {
    const hldb::RefInstance *const inst = getTopInstance(instName);
    ASSERT_NE(inst, nullptr) << instName;
    ASSERT_NE(inst->getTypespec(), nullptr) << instName;
    const hldb::InterfaceTypespec *const its = inst->getTypespec()->getActual<hldb::InterfaceTypespec>();
    ASSERT_NE(its, nullptr) << instName;
    EXPECT_EQ(its->getInterface(), getInterface(ifName)) << instName;
  }

  static void expectConnection(std::string_view instName, std::string_view actualName) {
    const hldb::NamedArgument *const arg = getConnection(instName);
    ASSERT_NE(arg, nullptr) << instName << " has exactly one connection";
    EXPECT_TRUE(arg->getConnByName()) << instName;
    const hldb::RefObj *const low = arg->getLowConn<hldb::RefObj>();
    ASSERT_NE(low, nullptr) << instName;
    const hldb::Any *const expected = hldb::findByName<hldb::Port>("p", getModule("sink")->getPorts());
    ASSERT_NE(expected, nullptr) << instName;
    EXPECT_EQ(low->getActual(), expected) << instName;
    const hldb::RefObj *const high = arg->getHighConn<hldb::RefObj>();
    ASSERT_NE(high, nullptr) << instName;
    EXPECT_EQ(high->getActual(), getTopInstance(actualName)) << instName;
  }
};

TEST_F(IfGenericTwoTypesTest, CompilerReportsZeroErrors) {
  // Sec 25.3.3: both connections are legal.
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfGenericTwoTypesTest, TwoDifferentInterfaces) {
  // Lines 23-29: a_if holds x, b_if holds y.
  const hldb::Interface *const a = getInterface("a_if");
  const hldb::Interface *const b = getInterface("b_if");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("x", a->getVariables()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("y", b->getVariables()), nullptr);
}

TEST_F(IfGenericTwoTypesTest, SinkHasOneGenericPortP) {
  // Line 31: "module sink(interface p);".
  const hldb::Module *const sink = getModule("sink");
  ASSERT_NE(sink, nullptr);
  ASSERT_NE(sink->getPorts(), nullptr);
  ASSERT_EQ(sink->getPorts()->size(), 1u);
  const hldb::Port *const p = sink->getPorts()->at(0);
  EXPECT_EQ(p->getName(), "p");
  EXPECT_EQ(p->getPortType(), vpiInterfacePort);
}

TEST_F(IfGenericTwoTypesTest, TopHasFourInstances) {
  // Lines 35-38: ua, ub, s1, s2.
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  EXPECT_EQ(top->getRefInstances()->size(), 4u);
}

TEST_F(IfGenericTwoTypesTest, UaIsAIfAndUbIsBIf) {
  // Lines 35-36: "a_if ua();" and "b_if ub();".
  expectInstanceOf("ua", "a_if");
  expectInstanceOf("ub", "b_if");
}

TEST_F(IfGenericTwoTypesTest, S1ConnectsPToUa) {
  // Line 37: "sink s1(.p(ua));".
  expectConnection("s1", "ua");
}

TEST_F(IfGenericTwoTypesTest, S2ConnectsPToUb) {
  // Line 38: "sink s2(.p(ub));".
  expectConnection("s2", "ub");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

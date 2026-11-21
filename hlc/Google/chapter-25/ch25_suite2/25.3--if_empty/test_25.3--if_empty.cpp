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

// IEEE 1800-2023 Sec 25.3: interface_declaration allows an empty item list, and an interface is
// instantiated like a module ("empty_if u_empty();").
// Source: tests/Google/chapter-25/ch25_suite2/25.3--if_empty.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/module.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_typespec.h>

namespace hlc {

class IfEmptyTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3--if_empty.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getInterface() {
    return hldb::findByDefName<hldb::Interface>("empty_if", m_design->getAllInterfaces());
  }
};

TEST_F(IfEmptyTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
  EXPECT_EQ(stats.m_warnings, 0);
}

TEST_F(IfEmptyTest, InterfaceIsDeclared) { EXPECT_NE(getInterface(), nullptr); }

TEST_F(IfEmptyTest, InterfaceHasNoItems) {
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  EXPECT_TRUE(iface->getPorts() == nullptr || iface->getPorts()->empty());
  EXPECT_TRUE(iface->getNets() == nullptr || iface->getNets()->empty());
  EXPECT_TRUE(iface->getVariables() == nullptr || iface->getVariables()->empty());
  EXPECT_TRUE(iface->getModports() == nullptr || iface->getModports()->empty());
}

TEST_F(IfEmptyTest, InterfaceHasNoEndLabel) {
  // Line 24: a bare "endinterface".
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  EXPECT_TRUE(iface->getEndLabel().empty());
}

TEST_F(IfEmptyTest, TopHasOneInterfaceInstance) {
  // Lines 26-28: top contains only "empty_if u_empty();".
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  EXPECT_EQ(top->getRefInstances()->size(), 1u);
}

TEST_F(IfEmptyTest, InstanceHasNoConnections) {
  // Line 27: "()" -- empty_if has no ports, so nothing is connected.
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>("u_empty", top->getRefInstances());
  ASSERT_NE(inst, nullptr);
  EXPECT_TRUE(inst->getArguments() == nullptr || inst->getArguments()->empty());
}

TEST_F(IfEmptyTest, TopInstantiatesEmptyIf) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>("u_empty", top->getRefInstances());
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::InterfaceTypespec *const its = inst->getTypespec()->getActual<hldb::InterfaceTypespec>();
  ASSERT_NE(its, nullptr) << "u_empty is not typed by an interface";
  EXPECT_EQ(its->getInterface(), getInterface());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

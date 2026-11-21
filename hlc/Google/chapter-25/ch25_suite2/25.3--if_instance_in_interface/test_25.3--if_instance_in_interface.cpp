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

// IEEE 1800-2023 Sec 25.3: an interface can instantiate other interfaces
// ("inner_if u_inner();" inside outer_if).
// Source: tests/Google/chapter-25/ch25_suite2/25.3--if_instance_in_interface.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/net.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class IfInstanceInInterfaceTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3--if_instance_in_interface.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getInterface(std::string_view name) {
    return hldb::findByDefName<hldb::Interface>(name, m_design->getAllInterfaces());
  }
};

TEST_F(IfInstanceInInterfaceTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfInstanceInInterfaceTest, BothInterfacesDeclared) {
  EXPECT_NE(getInterface("inner_if"), nullptr);
  EXPECT_NE(getInterface("outer_if"), nullptr);
}

TEST_F(IfInstanceInInterfaceTest, OuterHoldsInstanceOfInner) {
  const hldb::Interface *const outer = getInterface("outer_if");
  ASSERT_NE(outer, nullptr);
  const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>("u_inner", outer->getRefInstances());
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::InterfaceTypespec *const its = inst->getTypespec()->getActual<hldb::InterfaceTypespec>();
  ASSERT_NE(its, nullptr) << "u_inner is not typed by an interface";
  EXPECT_EQ(its->getInterface(), getInterface("inner_if"));
}

TEST_F(IfInstanceInInterfaceTest, InnerHasVariableS) {
  // Line 24: "logic s;" in inner_if.
  const hldb::Interface *const inner = getInterface("inner_if");
  ASSERT_NE(inner, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("s", inner->getVariables()), nullptr);
}

TEST_F(IfInstanceInInterfaceTest, OuterHasOnlyOneInstanceWithoutConnections) {
  // Line 28: "inner_if u_inner();" is the single item of outer_if; "()" connects nothing.
  const hldb::Interface *const outer = getInterface("outer_if");
  ASSERT_NE(outer, nullptr);
  ASSERT_NE(outer->getRefInstances(), nullptr);
  ASSERT_EQ(outer->getRefInstances()->size(), 1u);
  const hldb::RefInstance *const inst = outer->getRefInstances()->at(0);
  EXPECT_EQ(inst->getName(), "u_inner");
  EXPECT_TRUE(inst->getArguments() == nullptr || inst->getArguments()->empty());
}

TEST_F(IfInstanceInInterfaceTest, InstanceIsNeitherNetNorVariable) {
  // Line 28: an interface instance is not a data object of outer_if.
  const hldb::Interface *const outer = getInterface("outer_if");
  ASSERT_NE(outer, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("u_inner", outer->getNets()), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("u_inner", outer->getVariables()), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

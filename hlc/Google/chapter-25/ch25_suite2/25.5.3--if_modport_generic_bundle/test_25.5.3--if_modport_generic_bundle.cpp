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

// IEEE 1800-2023 Sec 25.5.3 (connecting port bundle to generic interface): memMod and cpuMod use
// the "interface" keyword in their headers; the actual interface and modport are specified in the
// module instantiation ("memMod mem(sb_intf.target);").
// Source: tests/Google/chapter-25/ch25_suite2/25.5.3--if_modport_generic_bundle.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/modport.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class IfModportGenericBundleTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.5.3--if_modport_generic_bundle.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Modport *getModport(std::string_view name) {
    const hldb::Interface *const bus = hldb::findByDefName<hldb::Interface>("simple_bus", m_design->getAllInterfaces());
    return (bus == nullptr) ? nullptr : hldb::findByName<hldb::Modport>(name, bus->getModports());
  }
  static const hldb::RefObj *getHighConn(std::string_view instName) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
    if (top == nullptr) return nullptr;
    const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>(instName, top->getRefInstances());
    if ((inst == nullptr) || (inst->getArguments() == nullptr) || (inst->getArguments()->size() != 1)) return nullptr;
    return inst->getArguments()->at(0)->getHighConn<hldb::RefObj>();
  }
};

TEST_F(IfModportGenericBundleTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfModportGenericBundleTest, HeadersAreGenericInterfacePorts) {
  // Lines 36 and 42: "module memMod (interface a);" and "module cpuMod (interface b);" name
  // neither an interface nor a modport (Sec 25.3.3).
  for (auto [module, port] : {std::pair<std::string_view, std::string_view>{"memMod", "a"}, {"cpuMod", "b"}}) {
    const hldb::Module *const m = hldb::findByDefName<hldb::Module>(module, m_design->getAllModules());
    ASSERT_NE(m, nullptr) << module;
    const hldb::Port *const p = hldb::findByName<hldb::Port>(port, m->getPorts());
    ASSERT_NE(p, nullptr) << module;
    EXPECT_EQ(p->getPortType(), vpiInterfacePort) << module;
    ASSERT_NE(p->getTypespec(), nullptr) << module;
    const hldb::InterfaceTypespec *const its = p->getTypespec()->getActual<hldb::InterfaceTypespec>();
    ASSERT_NE(its, nullptr) << module;
    EXPECT_EQ(its->getInterface(), nullptr) << module;
    EXPECT_EQ(its->getModport(), nullptr) << module;
  }
}

TEST_F(IfModportGenericBundleTest, MemConnectsModportTarget) {
  // Line 49: "memMod mem(sb_intf.target);"
  const hldb::RefObj *const high = getHighConn("mem");
  ASSERT_NE(high, nullptr);
  ASSERT_NE(getModport("target"), nullptr);
  EXPECT_EQ(high->getActual(), getModport("target"));
}

TEST_F(IfModportGenericBundleTest, CpuConnectsModportInitiator) {
  // Line 50: "cpuMod cpu(sb_intf.initiator);"
  const hldb::RefObj *const high = getHighConn("cpu");
  ASSERT_NE(high, nullptr);
  ASSERT_NE(getModport("initiator"), nullptr);
  EXPECT_EQ(high->getActual(), getModport("initiator"));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

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

// IEEE 1800-2023 Sec 25.3.2: a module port declared with an interface name ("bus_if b") is an
// interface port bound to that interface; no modport is selected. An interface port is neither a
// net nor a variable (Sec 23.2.2.3).
// Source: tests/Google/chapter-25/ch25_suite2/25.3.2--if_named_bundle_port.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class IfNamedBundlePortTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3.2--if_named_bundle_port.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getSink() { return hldb::findByDefName<hldb::Module>("sink", m_design->getAllModules()); }
};

TEST_F(IfNamedBundlePortTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfNamedBundlePortTest, PortIsInterfacePort) {
  const hldb::Module *const sink = getSink();
  ASSERT_NE(sink, nullptr);
  ASSERT_NE(sink->getPorts(), nullptr);
  ASSERT_EQ(sink->getPorts()->size(), 1u);
  const hldb::Port *const p = sink->getPorts()->at(0);
  EXPECT_EQ(p->getName(), "b");
  EXPECT_EQ(p->getPortType(), vpiInterfacePort);
}

TEST_F(IfNamedBundlePortTest, PortTypedByBusIfWithoutModport) {
  const hldb::Module *const sink = getSink();
  ASSERT_NE(sink, nullptr);
  const hldb::Port *const p = hldb::findByName<hldb::Port>("b", sink->getPorts());
  ASSERT_NE(p, nullptr);
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::InterfaceTypespec *const its = p->getTypespec()->getActual<hldb::InterfaceTypespec>();
  ASSERT_NE(its, nullptr) << "port b is not typed by an interface";
  const hldb::Any *const expected = hldb::findByDefName<hldb::Interface>("bus_if", m_design->getAllInterfaces());
  ASSERT_NE(expected, nullptr);
  EXPECT_EQ(its->getInterface(), expected);
  EXPECT_EQ(its->getModport(), nullptr) << "no modport is named in 'bus_if b'";
}

TEST_F(IfNamedBundlePortTest, PortIsNeitherNetNorVariable) {
  const hldb::Module *const sink = getSink();
  ASSERT_NE(sink, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("b", sink->getNets()), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("b", sink->getVariables()), nullptr);
}

TEST_F(IfNamedBundlePortTest, BusIfHasVariableReq) {
  // Line 24: "logic req;" in bus_if.
  const hldb::Interface *const iface = hldb::findByDefName<hldb::Interface>("bus_if", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("req", iface->getVariables()), nullptr);
}

TEST_F(IfNamedBundlePortTest, PortHasNoDirection) {
  // Line 27: an interface port is declared without a direction (Sec 23.2.2.1).
  const hldb::Module *const sink = getSink();
  ASSERT_NE(sink, nullptr);
  const hldb::Port *const p = hldb::findByName<hldb::Port>("b", sink->getPorts());
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getDirection(), vpiNoDirection);
}

TEST_F(IfNamedBundlePortTest, IODeclTypedByBusIf) {
  // Line 27: the port declaration of b (Sec 37.13) is typed by bus_if.
  const hldb::Module *const sink = getSink();
  ASSERT_NE(sink, nullptr);
  const hldb::IODecl *const io = hldb::findByName<hldb::IODecl>("b", sink->getIODecls());
  ASSERT_NE(io, nullptr);
  ASSERT_NE(io->getTypespec(), nullptr);
  const hldb::InterfaceTypespec *const its = io->getTypespec()->getActual<hldb::InterfaceTypespec>();
  ASSERT_NE(its, nullptr);
  const hldb::Any *const expected = hldb::findByDefName<hldb::Interface>("bus_if", m_design->getAllInterfaces());
  ASSERT_NE(expected, nullptr);
  EXPECT_EQ(its->getInterface(), expected);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

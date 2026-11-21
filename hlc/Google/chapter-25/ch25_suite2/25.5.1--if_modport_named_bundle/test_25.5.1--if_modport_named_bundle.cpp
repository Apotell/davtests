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

// IEEE 1800-2023 Sec 25.5.1 (named port bundle): modports target and initiator of simple_bus list
// input, output and ref ports, including the interface port clk; memMod and cpuMod select a
// modport in their headers and are connected to the plain interface instance.
// Source: tests/Google/chapter-25/ch25_suite2/25.5.1--if_modport_named_bundle.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/modport.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>
#include <utility>

namespace hlc {

class IfModportNamedBundleTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.5.1--if_modport_named_bundle.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  using PortDir = std::pair<std::string_view, int32_t>;

  static const hldb::Interface *getBus() {
    return hldb::findByDefName<hldb::Interface>("simple_bus", m_design->getAllInterfaces());
  }
  static const hldb::Modport *getModport(std::string_view name) {
    const hldb::Interface *const bus = getBus();
    return (bus == nullptr) ? nullptr : hldb::findByName<hldb::Modport>(name, bus->getModports());
  }
  template <size_t N>
  static void expectPorts(std::string_view mpName, const PortDir (&ports)[N]) {
    const hldb::Modport *const mp = getModport(mpName);
    ASSERT_NE(mp, nullptr) << mpName;
    ASSERT_NE(mp->getIODecls(), nullptr) << mpName;
    ASSERT_EQ(mp->getIODecls()->size(), N) << mpName;
    for (size_t i = 0; i < N; ++i) {
      const hldb::IODecl *const io = mp->getIODecls()->at(i);
      EXPECT_EQ(io->getName(), ports[i].first) << mpName << " port #" << i;
      EXPECT_EQ(io->getDirection(), ports[i].second) << mpName << "." << ports[i].first;
    }
  }
  static const hldb::InterfaceTypespec *getHeaderTypespec(std::string_view module, std::string_view port) {
    const hldb::Module *const m = hldb::findByDefName<hldb::Module>(module, m_design->getAllModules());
    if (m == nullptr) return nullptr;
    const hldb::Port *const p = hldb::findByName<hldb::Port>(port, m->getPorts());
    if ((p == nullptr) || (p->getTypespec() == nullptr)) return nullptr;
    return p->getTypespec()->getActual<hldb::InterfaceTypespec>();
  }
};

TEST_F(IfModportNamedBundleTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfModportNamedBundleTest, TargetPortDirections) {
  // Lines 28-30: "modport target (input req, addr, mode, start, clk, output gnt, rdy, ref data);"
  expectPorts("target", {{"req", vpiInput},
                         {"addr", vpiInput},
                         {"mode", vpiInput},
                         {"start", vpiInput},
                         {"clk", vpiInput},
                         {"gnt", vpiOutput},
                         {"rdy", vpiOutput},
                         {"data", vpiRef}});
}

TEST_F(IfModportNamedBundleTest, InitiatorPortDirections) {
  // Lines 31-33: "modport initiator (input gnt, rdy, clk, output req, addr, mode, start, ref data);"
  expectPorts("initiator", {{"gnt", vpiInput},
                            {"rdy", vpiInput},
                            {"clk", vpiInput},
                            {"req", vpiOutput},
                            {"addr", vpiOutput},
                            {"mode", vpiOutput},
                            {"start", vpiOutput},
                            {"data", vpiRef}});
}

TEST_F(IfModportNamedBundleTest, RefDataReferencesBusVariableData) {
  // Line 25: "logic [7:0] addr, data;" -- "ref data" references this variable in both modports.
  const hldb::Interface *const bus = getBus();
  ASSERT_NE(bus, nullptr);
  const hldb::Variable *const data = hldb::findByName<hldb::Variable>("data", bus->getVariables());
  ASSERT_NE(data, nullptr);
  for (std::string_view mpName : {"target", "initiator"}) {
    const hldb::Modport *const mp = getModport(mpName);
    ASSERT_NE(mp, nullptr) << mpName;
    const hldb::IODecl *const io = hldb::findByName<hldb::IODecl>("data", mp->getIODecls());
    ASSERT_NE(io, nullptr) << mpName;
    const hldb::RefObj *const ref = io->getExpr<hldb::RefObj>();
    ASSERT_NE(ref, nullptr) << mpName;
    EXPECT_EQ(ref->getActual(), data) << mpName;
  }
}

TEST_F(IfModportNamedBundleTest, ClkReferencesInterfacePortClk) {
  // Line 23: clk is declared by simple_bus as its port "input logic clk"; the modport item clk
  // references that declaration (the port or the net it declares).
  const hldb::Interface *const bus = getBus();
  ASSERT_NE(bus, nullptr);
  const hldb::Any *const port = hldb::findByName<hldb::Port>("clk", bus->getPorts());
  const hldb::Any *const net = hldb::findByName<hldb::Net>("clk", bus->getNets());
  ASSERT_TRUE((port != nullptr) || (net != nullptr)) << "simple_bus declares no clk";
  for (std::string_view mpName : {"target", "initiator"}) {
    const hldb::Modport *const mp = getModport(mpName);
    ASSERT_NE(mp, nullptr) << mpName;
    const hldb::IODecl *const io = hldb::findByName<hldb::IODecl>("clk", mp->getIODecls());
    ASSERT_NE(io, nullptr) << mpName;
    const hldb::RefObj *const ref = io->getExpr<hldb::RefObj>();
    ASSERT_NE(ref, nullptr) << mpName;
    ASSERT_NE(ref->getActual(), nullptr) << mpName << ".clk is unbound";
    EXPECT_TRUE((ref->getActual() == port) || (ref->getActual() == net)) << mpName;
  }
}

TEST_F(IfModportNamedBundleTest, MemModSelectsTarget) {
  // Line 36: "module memMod (simple_bus.target a);"
  const hldb::InterfaceTypespec *const its = getHeaderTypespec("memMod", "a");
  ASSERT_NE(its, nullptr);
  EXPECT_EQ(its->getInterface(), getBus());
  ASSERT_NE(getModport("target"), nullptr);
  EXPECT_EQ(its->getModport(), getModport("target"));
}

TEST_F(IfModportNamedBundleTest, CpuModSelectsInitiator) {
  // Line 42: "module cpuMod (simple_bus.initiator b);"
  const hldb::InterfaceTypespec *const its = getHeaderTypespec("cpuMod", "b");
  ASSERT_NE(its, nullptr);
  EXPECT_EQ(its->getInterface(), getBus());
  ASSERT_NE(getModport("initiator"), nullptr);
  EXPECT_EQ(its->getModport(), getModport("initiator"));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

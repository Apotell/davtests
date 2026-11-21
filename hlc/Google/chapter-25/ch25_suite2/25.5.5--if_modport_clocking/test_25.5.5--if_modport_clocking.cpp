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

// IEEE 1800-2023 Sec 25.5.5 (clocking blocks and modports): "modport STB (clocking sb);" lists the
// clocking block sb declared by the same interface (synchronous port), while DUT and TB list plain
// signals (asynchronous ports). dev1 selects DUT and program T selects STB in their headers.
// Source: tests/Google/chapter-25/ch25_suite2/25.5.5--if_modport_clocking.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/clocking_block.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/modport.h>
#include <hldb/module.h>
#include <hldb/port.h>
#include <hldb/program.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class IfModportClockingTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.5.5--if_modport_clocking.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getBus() {
    return hldb::findByDefName<hldb::Interface>("A_Bus", m_design->getAllInterfaces());
  }
  static const hldb::Modport *getModport(std::string_view name) {
    const hldb::Interface *const bus = getBus();
    return (bus == nullptr) ? nullptr : hldb::findByName<hldb::Modport>(name, bus->getModports());
  }
  static const hldb::InterfaceTypespec *asInterfaceTypespec(const hldb::Port *p) {
    if ((p == nullptr) || (p->getTypespec() == nullptr)) return nullptr;
    return p->getTypespec()->getActual<hldb::InterfaceTypespec>();
  }
};

TEST_F(IfModportClockingTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfModportClockingTest, BusHasModportsDutStbTb) {
  // Lines 31-33.
  const hldb::Interface *const bus = getBus();
  ASSERT_NE(bus, nullptr);
  ASSERT_NE(bus->getModports(), nullptr);
  ASSERT_EQ(bus->getModports()->size(), 3u);
  EXPECT_NE(getModport("DUT"), nullptr);
  EXPECT_NE(getModport("STB"), nullptr);
  EXPECT_NE(getModport("TB"), nullptr);
}

TEST_F(IfModportClockingTest, StbListsOnlyClockingSb) {
  // Line 32: "modport STB (clocking sb);" -- a modport_clocking_declaration.
  const hldb::Modport *const stb = getModport("STB");
  ASSERT_NE(stb, nullptr);
  ASSERT_NE(stb->getIODecls(), nullptr);
  ASSERT_EQ(stb->getIODecls()->size(), 1u);
  const hldb::IODecl *const io = stb->getIODecls()->at(0);
  EXPECT_EQ(io->getName(), std::string_view("sb"));
  EXPECT_TRUE(io->getClocking());
}

TEST_F(IfModportClockingTest, StbClockingRefersToBusClockingBlockSb) {
  // Lines 26-30: clocking block sb is declared by A_Bus, the interface of modport STB.
  const hldb::Interface *const bus = getBus();
  ASSERT_NE(bus, nullptr);
  const hldb::ClockingBlock *const sb = hldb::findByName<hldb::ClockingBlock>("sb", bus->getClockingBlocks());
  ASSERT_NE(sb, nullptr);
  const hldb::Modport *const stb = getModport("STB");
  ASSERT_NE(stb, nullptr);
  const hldb::IODecl *const io = hldb::findByName<hldb::IODecl>("sb", stb->getIODecls());
  ASSERT_NE(io, nullptr);
  const hldb::RefObj *const ref = io->getExpr<hldb::RefObj>();
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getActual(), sb);
}

TEST_F(IfModportClockingTest, DutIsAsynchronous) {
  // Line 31: "modport DUT (input clk, req, addr, output gnt, inout data);"
  const hldb::Modport *const dut = getModport("DUT");
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getIODecls(), nullptr);
  ASSERT_EQ(dut->getIODecls()->size(), 5u);
  constexpr std::string_view kNames[5] = {"clk", "req", "addr", "gnt", "data"};
  constexpr int32_t kDirs[5] = {vpiInput, vpiInput, vpiInput, vpiOutput, vpiInout};
  for (size_t i = 0; i < 5; ++i) {
    const hldb::IODecl *const io = dut->getIODecls()->at(i);
    EXPECT_EQ(io->getName(), kNames[i]);
    EXPECT_EQ(io->getDirection(), kDirs[i]) << kNames[i];
    EXPECT_FALSE(io->getClocking()) << kNames[i];
  }
}

TEST_F(IfModportClockingTest, Dev1SelectsDut) {
  // Line 36: "module dev1 (A_Bus.DUT b);"
  const hldb::Module *const dev1 = hldb::findByDefName<hldb::Module>("dev1", m_design->getAllModules());
  ASSERT_NE(dev1, nullptr);
  const hldb::InterfaceTypespec *const its = asInterfaceTypespec(hldb::findByName<hldb::Port>("b", dev1->getPorts()));
  ASSERT_NE(its, nullptr);
  ASSERT_NE(getModport("DUT"), nullptr);
  EXPECT_EQ(its->getModport(), getModport("DUT"));
}

TEST_F(IfModportClockingTest, ProgramTSelectsStb) {
  // Line 39: "program T (A_Bus.STB b1);"
  const hldb::Program *const t = hldb::findByDefName<hldb::Program>("T", m_design->getAllPrograms());
  ASSERT_NE(t, nullptr);
  const hldb::InterfaceTypespec *const its = asInterfaceTypespec(hldb::findByName<hldb::Port>("b1", t->getPorts()));
  ASSERT_NE(its, nullptr);
  ASSERT_NE(getModport("STB"), nullptr);
  EXPECT_EQ(its->getModport(), getModport("STB"));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

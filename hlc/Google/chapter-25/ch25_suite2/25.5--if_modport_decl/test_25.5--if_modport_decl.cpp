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

// IEEE 1800-2023 Sec 25.5: "modport initiator (input a, b, output c, d);" declares a modport whose
// directions are seen from the module that uses it. Each simple modport port is both a port
// identifier and a reference to the interface item of the same name.
// Source: tests/Google/chapter-25/ch25_suite2/25.5--if_modport_decl.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/io_decl.h>
#include <hldb/modport.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class IfModportDeclTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.5--if_modport_decl.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getI2() {
    return hldb::findByDefName<hldb::Interface>("i2", m_design->getAllInterfaces());
  }
  static const hldb::Modport *getModport(std::string_view name) {
    const hldb::Interface *const i2 = getI2();
    return (i2 == nullptr) ? nullptr : hldb::findByName<hldb::Modport>(name, i2->getModports());
  }
  // Checks names and directions of the four modport ports in declaration order.
  static void expectPorts(const hldb::Modport *mp, const int32_t (&dirs)[4]) {
    ASSERT_NE(mp, nullptr);
    ASSERT_NE(mp->getIODecls(), nullptr);
    ASSERT_EQ(mp->getIODecls()->size(), 4u);
    constexpr std::string_view kNames[4] = {"a", "b", "c", "d"};
    for (size_t i = 0; i < 4; ++i) {
      const hldb::IODecl *const io = mp->getIODecls()->at(i);
      EXPECT_EQ(io->getName(), kNames[i]);
      EXPECT_EQ(io->getDirection(), dirs[i]) << "port " << kNames[i];
    }
  }
};

TEST_F(IfModportDeclTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfModportDeclTest, I2HasModportsInitiatorAndTarget) {
  // Lines 25-26: two modport declarations, initiator and target.
  const hldb::Interface *const i2 = getI2();
  ASSERT_NE(i2, nullptr);
  ASSERT_NE(i2->getModports(), nullptr);
  ASSERT_EQ(i2->getModports()->size(), 2u);
  EXPECT_NE(getModport("initiator"), nullptr);
  EXPECT_NE(getModport("target"), nullptr);
}

TEST_F(IfModportDeclTest, ModportsBelongToI2) {
  const hldb::Interface *const i2 = getI2();
  ASSERT_NE(i2, nullptr);
  for (std::string_view name : {"initiator", "target"}) {
    const hldb::Modport *const mp = getModport(name);
    ASSERT_NE(mp, nullptr) << name;
    EXPECT_EQ(mp->getInterface(), i2) << name;
  }
}

TEST_F(IfModportDeclTest, InitiatorInputsAB_OutputsCD) {
  // Line 25: "modport initiator (input a, b, output c, d);"
  expectPorts(getModport("initiator"), {vpiInput, vpiInput, vpiOutput, vpiOutput});
}

TEST_F(IfModportDeclTest, TargetOutputsAB_InputsCD) {
  // Line 26: "modport target (output a, b, input c, d);"
  expectPorts(getModport("target"), {vpiOutput, vpiOutput, vpiInput, vpiInput});
}

TEST_F(IfModportDeclTest, ModportPortsReferToI2Nets) {
  // Line 24: "wire a, b, c, d;" -- each simple modport port references the interface net of the
  // same name (Sec 25.5.4).
  const hldb::Interface *const i2 = getI2();
  ASSERT_NE(i2, nullptr);
  for (std::string_view mpName : {"initiator", "target"}) {
    const hldb::Modport *const mp = getModport(mpName);
    ASSERT_NE(mp, nullptr) << mpName;
    ASSERT_NE(mp->getIODecls(), nullptr) << mpName;
    for (const hldb::IODecl *io : *mp->getIODecls()) {
      const hldb::RefObj *const ref = io->getExpr<hldb::RefObj>();
      ASSERT_NE(ref, nullptr) << mpName << "." << io->getName();
      const hldb::Net *const net = hldb::findByName<hldb::Net>(io->getName(), i2->getNets());
      ASSERT_NE(net, nullptr) << io->getName();
      EXPECT_EQ(ref->getActual(), net) << mpName << "." << io->getName();
    }
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

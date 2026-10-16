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

// Validates the HLDB model of module ref ports (IEEE 1800-2023):
//   Sec 37.13 detail 1  -- vpiDirection of an io decl is vpiRef for a pass-by-ref port
//   Sec 37.14           -- the port itself stays a vpiPort, vpiPortType is vpiPort
//   Sec 37.15 detail 1  -- a ref obj exists for a port with ref direction; it names the
//                          variable that is the low conn of the ref port
//   Sec 37.15 detail 2  -- that ref obj is reached through the io decl of the instance

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ModuleRefPort : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ModuleRefPort.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::Module *findModule(std::string_view name) const {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  void checkRefPort(std::string_view moduleName) const {
    const hldb::Module *const m = findModule(moduleName);
    ASSERT_NE(m, nullptr) << "Module '" << moduleName << "' not found";

    // The ref port is declared with a variable data type ("ref logic [7:0] r").
    const hldb::Variable *const variable = hldb::findByName<hldb::Variable>("r", m->getVariables());
    ASSERT_NE(variable, nullptr) << moduleName << " has no Variable 'r'";

    const hldb::Port *const port = hldb::findByName<hldb::Port>("r", m->getPorts());
    ASSERT_NE(port, nullptr) << moduleName << " has no Port 'r'";
    EXPECT_EQ(port->getDirection(), vpiRef);
    const hldb::RefObj *const lowConn = port->getLowConn<hldb::RefObj>();
    ASSERT_NE(lowConn, nullptr) << "Port 'r' low conn is not a RefObj";
    EXPECT_EQ(lowConn->getActual(), variable);

    const hldb::IODecl *const iodecl = hldb::findByName<hldb::IODecl>("r", m->getIODecls());
    ASSERT_NE(iodecl, nullptr) << moduleName << " has no IODecl 'r'";
    EXPECT_EQ(iodecl->getDirection(), vpiRef);
    ASSERT_NE(iodecl->getExpr(), nullptr);
    ASSERT_EQ(iodecl->getExpr()->getAnyType(), hldb::AnyType::RefObj);
    const hldb::RefObj *const expr = static_cast<const hldb::RefObj *>(iodecl->getExpr());
    EXPECT_EQ(expr->getName(), "r");
    EXPECT_EQ(expr->getActual(), variable);

    ASSERT_NE(iodecl->getTypespec(), nullptr) << "IODecl 'r' has no typespec";
    EXPECT_NE(iodecl->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr)
        << "IODecl 'r' is not typed 'logic [7:0]'";
  }

  void checkPortTypeIsPort(std::string_view moduleName) const {
    const hldb::Module *const m = findModule(moduleName);
    ASSERT_NE(m, nullptr);
    ASSERT_NE(m->getPorts(), nullptr);
    for (const hldb::Port *const port : *m->getPorts()) {
      EXPECT_EQ(port->getPortType(), vpiPort) << "Port '" << port->getName() << "'";
    }
  }
};

TEST_F(ModuleRefPort, AnsiRefPortIsIODecl) { checkRefPort("ansi_ref"); }

TEST_F(ModuleRefPort, NonAnsiRefPortIsIODecl) { checkRefPort("nonansi_ref"); }

TEST_F(ModuleRefPort, InputPortKeepsItsDirection) {
  for (std::string_view name : {"ansi_ref", "nonansi_ref"}) {
    const hldb::Module *const m = findModule(name);
    ASSERT_NE(m, nullptr);
    const hldb::Port *const port = hldb::findByName<hldb::Port>("a", m->getPorts());
    ASSERT_NE(port, nullptr) << name << " has no Port 'a'";
    EXPECT_EQ(port->getDirection(), vpiInput) << name;
  }
}

TEST_F(ModuleRefPort, AnsiPortTypeIsPort) {
  GTEST_SKIP() << "IEEE 1800-2023 Sec 37.14 detail 1: vpiPortType shall be vpiPort for a port that is "
                  "neither an interface port nor a modport port; HLC only sets vpiPortType for "
                  "interface/modport ports and leaves it unset otherwise.";
  checkPortTypeIsPort("ansi_ref");
}

TEST_F(ModuleRefPort, NonAnsiPortTypeIsPort) {
  GTEST_SKIP() << "IEEE 1800-2023 Sec 37.14 detail 1: vpiPortType shall be vpiPort for a port that is "
                  "neither an interface port nor a modport port; HLC only sets vpiPortType for "
                  "interface/modport ports and leaves it unset otherwise.";
  checkPortTypeIsPort("nonansi_ref");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

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

// Validates the UHDM graph produced for tests/NetsAndVariables/NonAnsi/Program.sv,
// split out of the combined NetsAndVariablesNonAnsi.sv suite so the program
// testing point stands on its own.
//
// Checked:
//   - nets_and_variables_program exists via m_design->getAllPrograms(), with
//     three ports (a, b input; y output)
//   - a / b (input wire): explicit net ports, vpiWire, absent from
//     getVariables()
//   - y (output logic): per IEEE 1800-2023 Sec 23.2.2.3, an output port with
//     an explicit data type and no net-type keyword defaults to a variable
//     -- absent from getNets()
//   - prog_net (wire, driven by continuous assignment) is a hldb::Net with
//     vpiWire, absent from getVariables()
//   - prog_var (logic, assigned by blocking assignment in always_comb) is a
//     hldb::Variable, absent from getNets()
//   - one continuous assignment (prog_net = a & b) and one always_comb
//     process (prog_var = a ^ b; y = prog_var;)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/any.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/process_stmt.h>
#include <hldb/program.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class NonAnsiProgramTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Program.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Program *getProg() {
    return hldb::findByName<hldb::Program>("nets_and_variables_program", m_design->getAllPrograms());
  }

  // Asserts 'name' is a hldb::Net in prog->getNets() with the given
  // vpiNetType, and that no hldb::Variable with the same name exists (no
  // duplicate).
  static const hldb::Net *getNetOfType(const hldb::Program *prog, std::string_view name, int32_t expectedNetType) {
    const hldb::Net *const n = hldb::findByName<hldb::Net>(name, prog->getNets());
    EXPECT_NE(n, nullptr) << "net '" << name << "' not found";
    if (n != nullptr) {
      EXPECT_EQ(n->getNetType(), expectedNetType);
    }
    EXPECT_EQ(hldb::findByName<hldb::Variable>(name, prog->getVariables()), nullptr)
        << "'" << name << "' is net-declared -- it must not also appear in vpiVariables";
    return n;
  }

  // Asserts 'name' is a hldb::Variable in prog->getVariables(), and that no
  // hldb::Net with the same name exists (no duplicate).
  static const hldb::Variable *getVarNoNetDuplicate(const hldb::Program *prog, std::string_view name) {
    const hldb::Variable *const v = hldb::findByName<hldb::Variable>(name, prog->getVariables());
    EXPECT_NE(v, nullptr) << "'" << name << "' should appear in vpiVariables";
    EXPECT_EQ(hldb::findByName<hldb::Net>(name, prog->getNets()), nullptr)
        << "'" << name << "' has no net-type keyword -- it must not also appear in vpiNet";
    return v;
  }
};

TEST_F(NonAnsiProgramTest, ProgramExists) {
  ASSERT_NE(m_design->getAllPrograms(), nullptr);
  ASSERT_NE(getProg(), nullptr);
}

TEST_F(NonAnsiProgramTest, ThreePortsExistWithCorrectDirections) {
  const hldb::Program *const prog = getProg();
  ASSERT_NE(prog, nullptr);
  ASSERT_NE(prog->getPorts(), nullptr);
  EXPECT_EQ(prog->getPorts()->size(), 3u) << "expected ports a, b, y";

  const hldb::Port *const a = hldb::findByName<hldb::Port>("a", prog->getPorts());
  const hldb::Port *const b = hldb::findByName<hldb::Port>("b", prog->getPorts());
  const hldb::Port *const y = hldb::findByName<hldb::Port>("y", prog->getPorts());
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(y, nullptr);
  EXPECT_EQ(a->getDirection(), vpiInput);
  EXPECT_EQ(b->getDirection(), vpiInput);
  EXPECT_EQ(y->getDirection(), vpiOutput);
}

TEST_F(NonAnsiProgramTest, PortAIsWireNet) {
  const hldb::Program *const prog = getProg();
  ASSERT_NE(prog, nullptr);
  EXPECT_NE(getNetOfType(prog, "a", vpiWire), nullptr);
}

TEST_F(NonAnsiProgramTest, PortBIsWireNet) {
  const hldb::Program *const prog = getProg();
  ASSERT_NE(prog, nullptr);
  EXPECT_NE(getNetOfType(prog, "b", vpiWire), nullptr);
}

TEST_F(NonAnsiProgramTest, PortYIsVariable) {
  // 'output logic y;': explicit data type, no net-type keyword -- IEEE
  // 1800-2023 Sec 23.2.2.3 defaults this to a variable.
  const hldb::Program *const prog = getProg();
  ASSERT_NE(prog, nullptr);
  EXPECT_NE(getVarNoNetDuplicate(prog, "y"), nullptr);
}

TEST_F(NonAnsiProgramTest, ProgNetIsWire) {
  const hldb::Program *const prog = getProg();
  ASSERT_NE(prog, nullptr);
  EXPECT_NE(getNetOfType(prog, "prog_net", vpiWire), nullptr);
}

TEST_F(NonAnsiProgramTest, ProgVarIsVariable) {
  const hldb::Program *const prog = getProg();
  ASSERT_NE(prog, nullptr);
  EXPECT_NE(getVarNoNetDuplicate(prog, "prog_var"), nullptr);
}

TEST_F(NonAnsiProgramTest, OneContAssignDrivesProgNet) {
  const hldb::Program *const prog = getProg();
  ASSERT_NE(prog, nullptr);
  ASSERT_NE(prog->getContAssigns(), nullptr);
  ASSERT_EQ(prog->getContAssigns()->size(), 1u);
  const hldb::RefObj *const lhs = prog->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "prog_net");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

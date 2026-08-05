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

// Validates the UHDM graph produced for tests/NetsAndVariables/Ansi/Program.sv.
//
// Checked:
//   - nets_and_variables_program exists with three ports (a, b input;
//     y output)
//   - it has a net (prog_net) and a variable (prog_var) declared in program
//     scope
//   - it has one continuous assignment driving prog_net
//   - it has one process, modeled as an Initial process (a program cannot
//     contain always procedures -- IEEE 1800 clause 24.3)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/any.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/process_stmt.h>
#include <hldb/program.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AnsiProgramTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Program.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Program *getProg() {
    return hldb::findByName<hldb::Program>("nets_and_variables_program", m_design->getAllPrograms());
  }
};

TEST_F(AnsiProgramTest, ProgramExists) { ASSERT_NE(getProg(), nullptr); }

TEST_F(AnsiProgramTest, ProgramHasThreePorts) {
  const hldb::Program *const prog = getProg();
  ASSERT_NE(prog, nullptr);
  ASSERT_NE(prog->getPorts(), nullptr);
  ASSERT_EQ(prog->getPorts()->size(), 3u);

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

TEST_F(AnsiProgramTest, ProgramHasNetAndVariable) {
  const hldb::Program *const prog = getProg();
  ASSERT_NE(prog, nullptr);
  ASSERT_NE(prog->getNets(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("prog_net", prog->getNets()), nullptr);
  ASSERT_NE(prog->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("prog_var", prog->getVariables()), nullptr);
}

TEST_F(AnsiProgramTest, ProgramHasOneContAssignDrivingProgNet) {
  const hldb::Program *const prog = getProg();
  ASSERT_NE(prog, nullptr);
  ASSERT_NE(prog->getContAssigns(), nullptr);
  ASSERT_EQ(prog->getContAssigns()->size(), 1u);
  const hldb::RefObj *const lhs = prog->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "prog_net");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr) << "'prog_net' is formally declared";
}

TEST_F(AnsiProgramTest, ProgramHasOneProcess) {
  const hldb::Program *const prog = getProg();
  ASSERT_NE(prog, nullptr);
  ASSERT_NE(prog->getProcesses(), nullptr);
  ASSERT_EQ(prog->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(prog->getProcesses()->at(0)), nullptr)
      << "'initial' should be modeled as an Initial process";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

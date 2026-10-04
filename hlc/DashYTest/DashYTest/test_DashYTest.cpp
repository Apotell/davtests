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

// Tests for tests/DashYTest/dut.sv:
//
//   module top();
//      AND and1();
//   endmodule
//
// The "AND" module is not declared anywhere in dut.sv. DashYTest.hlc passes
// "-y lib", pointing HLC at tests/DashYTest/lib as a library directory
// (IEEE 1800-2023 Clause 33, "Configuring the compilation process" / library
// search). That directory contains AND.v (module AND), OR.v (module OR),
// SIM.v (module SIM) and BAD.v (module MODULE_NAME_DOES_NOT_MATCH_FILE_NAME
// -- a decoy whose module name does not match its file name, and which is
// not named by any instantiation in dut.sv).
//
// Per library binding (Clause 33), the elaborator resolves the unresolved
// module reference "AND" (from "AND and1();") by searching the library
// directories for a file that, once compiled, provides a module definition
// named "AND" -- AND.v qualifies, so module "AND" must be pulled into the
// design and "and1" must bind to it.
//
// Checked:
//   - module "top" exists
//   - "top" instantiates "and1" as a RefInstance whose typespec resolves
//     (ModuleTypespec) to "AND"
//   - the design's module list contains a definition of module "AND",
//     pulled in via the "-y lib" library search
//   - module "AND" is empty (no ports, no nets/variables, no processes),
//     matching "module AND(); endmodule" in lib/AND.v
//   - no COMP_FAILED_TO_BIND error is reported for "AND" (i.e. the library
//     search actually resolved the reference)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_typespec.h>

namespace hlc {

class DashYTestTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "DashYTest.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Module *getAnd() {
    return hldb::findByDefName<hldb::Module>("AND", m_design->getAllModules());
  }
};

// ---------------------------------------------------------------------------
// top
// ---------------------------------------------------------------------------

TEST_F(DashYTestTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr) << "module 'top' not found"; }

TEST_F(DashYTestTest, TopInstantiatesAnd1) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  const hldb::RefInstance *const and1 = hldb::findByName<hldb::RefInstance>("and1", top->getRefInstances());
  ASSERT_NE(and1, nullptr) << "'AND and1();' RefInstance not found in 'top'";
  ASSERT_NE(and1->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = and1->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr) << "'and1's typespec is not ModuleTypespec";
  EXPECT_EQ(mt->getName(), std::string_view("AND"));
}

// ---------------------------------------------------------------------------
// Library-resolved module "AND"
// ---------------------------------------------------------------------------

TEST_F(DashYTestTest, LibraryModuleAndWasPulledIntoDesign) {
  EXPECT_NE(getAnd(), nullptr) << "'-y lib' should resolve 'AND' via lib/AND.v (IEEE 1800-2023 Clause 33)";
}

TEST_F(DashYTestTest, LibraryModuleAndIsEmpty) {
  const hldb::Module *const mAnd = getAnd();
  ASSERT_NE(mAnd, nullptr);
  EXPECT_TRUE(!mAnd->getPorts() || mAnd->getPorts()->empty()) << "'module AND();' declares no ports";
  EXPECT_TRUE(!mAnd->getNets() || mAnd->getNets()->empty()) << "'module AND();' declares no nets";
  EXPECT_TRUE(!mAnd->getVariables() || mAnd->getVariables()->empty()) << "'module AND();' declares no variables";
  EXPECT_TRUE(!mAnd->getProcesses() || mAnd->getProcesses()->empty()) << "'module AND();' has no processes";
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(DashYTestTest, NoFailedToBindErrorForAnd) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "AND"), nullptr)
      << "'-y lib' should have resolved 'AND' without a binding failure";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

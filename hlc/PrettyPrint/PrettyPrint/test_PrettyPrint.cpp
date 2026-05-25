/*
 Copyright 2021 Alain Dargelas

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

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/design.h>
#include <uhdm/module.h>

namespace SURELOG {
class PrettyPrint : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "PrettyPrint.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

TEST_F(PrettyPrint, MinusOp) {
  const uhdm::Module *const module = uhdm::findByName<uhdm::Module>("work@dut", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "Module is null";

  const uhdm::Parameter *const parameter = uhdm::findByName<uhdm::Parameter>("a", module->getParameters());
  ASSERT_NE(parameter, nullptr) << "Module/Parameter is null";

  const uhdm::LogicTypespec *const typespec = uhdm::getTypespec<uhdm::LogicTypespec>(parameter);
  ASSERT_NE(typespec, nullptr) << "Module/Parameter/Typespec is null";

  const uhdm::RangeCollection *const ranges = typespec->getRanges();
  ASSERT_NE(ranges, nullptr) << "Module/Parameter/Typespec/Ranges is null";
  ASSERT_FALSE(ranges->empty()) << "Module/Parameter/Typespec/Ranges is empty";

  const uhdm::Range *const range = ranges->front();
  ASSERT_NE(range, nullptr) << "Module/Parameter/Typespec/Ranges::front is null";

  const uhdm::Expr *const expr = range->getLeftExpr();
  ASSERT_NE(expr, nullptr) << "Module/Parameter/Typespec/Range/Left is null";

  ASSERT_EQ(uhdm::prettyPrint(expr), "SIZE-1");
}

TEST_F(PrettyPrint, Select) {
  const uhdm::Module *const module = uhdm::findByName<uhdm::Module>("work@dut", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "Module is null";

  const uhdm::ParamAssign *const paramAssign = uhdm::findByName<uhdm::ParamAssign>("b", module->getParamAssigns());
  ASSERT_NE(paramAssign, nullptr) << "Module/ParamAssign is null";

  const uhdm::Expr *const rhs = paramAssign->getRhs<uhdm::Expr>();
  ASSERT_NE(rhs, nullptr) << "Module/ParamAssign/Rhs is null";

  ASSERT_EQ(uhdm::prettyPrint(rhs), "c[3][2][1:0]");
}

TEST_F(PrettyPrint, ConditionOp) {
  const uhdm::Module *const module = uhdm::findByName<uhdm::Module>("work@dut", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "Module is null";

  const uhdm::ParamAssign *const paramAssign = uhdm::findByName<uhdm::ParamAssign>("d", module->getParamAssigns());
  ASSERT_NE(paramAssign, nullptr) << "Module/ParamAssign is null";

  const uhdm::Expr *const rhs = paramAssign->getRhs<uhdm::Expr>();
  ASSERT_NE(rhs, nullptr) << "Module/ParamAssign/Rhs is null";

  ASSERT_EQ(uhdm::prettyPrint(rhs), "e ? 1 : 3");
}

TEST_F(PrettyPrint, SysFuncCall) {
  const uhdm::Module *const module = uhdm::findByName<uhdm::Module>("work@dut", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "Module is null";

  const uhdm::ParamAssign *const paramAssign = uhdm::findByName<uhdm::ParamAssign>("f", module->getParamAssigns());
  ASSERT_NE(paramAssign, nullptr) << "Module/ParamAssign is null";

  const uhdm::Expr *const rhs = paramAssign->getRhs<uhdm::Expr>();
  ASSERT_NE(rhs, nullptr) << "Module/ParamAssign/Rhs is null";

  ASSERT_EQ(uhdm::prettyPrint(rhs), "$sformatf(\"%d\", g)");
}

TEST_F(PrettyPrint, AssignmentPatternOp) {
  const uhdm::Module *const module = uhdm::findByName<uhdm::Module>("work@dut", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "Module is null";

  const uhdm::ParamAssign *const paramAssign = uhdm::findByName<uhdm::ParamAssign>("h", module->getParamAssigns());
  ASSERT_NE(paramAssign, nullptr) << "Module/ParamAssign is null";

  const uhdm::Expr *const rhs = paramAssign->getRhs<uhdm::Expr>();
  ASSERT_NE(rhs, nullptr) << "Module/ParamAssign/Rhs is null";

  ASSERT_EQ(uhdm::prettyPrint(rhs), "'{1, 2, 3}");
}

TEST_F(PrettyPrint, PackedArrayTypespec) {
  const uhdm::Module *const module = uhdm::findByName<uhdm::Module>("work@dut", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "Module is null";

  const uhdm::Parameter *const parameter = uhdm::findByName<uhdm::Parameter>("i", module->getParameters());
  ASSERT_NE(parameter, nullptr) << "Module/Parameter is null";

  const uhdm::BitTypespec *const typespec = uhdm::getTypespec<uhdm::BitTypespec>(parameter);
  ASSERT_NE(typespec, nullptr) << "Module/Parameter/Typespec is null";

  ASSERT_EQ(uhdm::prettyPrint(typespec), "bit [10:20][30:40]");
}

TEST_F(PrettyPrint, UnpackedArrayTypespec) {
  const uhdm::Module *const module = uhdm::findByName<uhdm::Module>("work@dut", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "Module is null";

  const uhdm::Parameter *const parameter = uhdm::findByName<uhdm::Parameter>("j", module->getParameters());
  ASSERT_NE(parameter, nullptr) << "Module/Parameter is null";

  const uhdm::ArrayTypespec *const typespec = uhdm::getTypespec<uhdm::ArrayTypespec>(parameter);
  ASSERT_NE(typespec, nullptr) << "Module/Parameter/Typespec is null";

  ASSERT_EQ(uhdm::prettyPrint(typespec), "int [10:20][30:40]");
}

TEST_F(PrettyPrint, UnpackedArrayTypespecOfPackedArrayTypespec_1) {
  const uhdm::Module *const module = uhdm::findByName<uhdm::Module>("work@dut", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "Module is null";

  const uhdm::Parameter *const parameter = uhdm::findByName<uhdm::Parameter>("k", module->getParameters());
  ASSERT_NE(parameter, nullptr) << "Module/Parameter is null";

  const uhdm::ArrayTypespec *const typespec = uhdm::getTypespec<uhdm::ArrayTypespec>(parameter);
  ASSERT_NE(typespec, nullptr) << "Module/Parameter/Typespec is null";

  ASSERT_EQ(uhdm::prettyPrint(typespec), "logic [10:20] [30:40]");
}

TEST_F(PrettyPrint, UnpackedArrayTypespecOfPackedArrayTypespec_2) {
  const uhdm::Module *const module = uhdm::findByName<uhdm::Module>("work@dut", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "Module is null";

  const uhdm::Parameter *const parameter = uhdm::findByName<uhdm::Parameter>("m", module->getParameters());
  ASSERT_NE(parameter, nullptr) << "Module/Parameter is null";

  const uhdm::ArrayTypespec *const typespec = uhdm::getTypespec<uhdm::ArrayTypespec>(parameter);
  ASSERT_NE(typespec, nullptr) << "Module/Parameter/Typespec is null";

  ASSERT_EQ(uhdm::prettyPrint(typespec), "l [10:20] [30:40]");
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

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

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>

namespace hlc {
class PrettyPrint : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PrettyPrint.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(PrettyPrint, MinusOp) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("dut", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "Module is null";

  const hldb::Parameter *const parameter = hldb::findByName<hldb::Parameter>("a", module->getParameters());
  ASSERT_NE(parameter, nullptr) << "Module/Parameter is null";

  const hldb::LogicTypespec *const typespec = hldb::getTypespec<hldb::LogicTypespec>(parameter);
  ASSERT_NE(typespec, nullptr) << "Module/Parameter/Typespec is null";

  const hldb::RangeCollection *const ranges = typespec->getRanges();
  ASSERT_NE(ranges, nullptr) << "Module/Parameter/Typespec/Ranges is null";
  ASSERT_FALSE(ranges->empty()) << "Module/Parameter/Typespec/Ranges is empty";

  const hldb::Range *const range = ranges->front();
  ASSERT_NE(range, nullptr) << "Module/Parameter/Typespec/Ranges::front is null";

  const hldb::Expr *const expr = range->getLeftExpr();
  ASSERT_NE(expr, nullptr) << "Module/Parameter/Typespec/Range/Left is null";

  ASSERT_EQ(hldb::prettyPrint(expr), "SIZE-1");
}

TEST_F(PrettyPrint, Select) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("dut", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "Module is null";

  const hldb::ParamAssign *const paramAssign = hldb::findByName<hldb::ParamAssign>("b", module->getParamAssigns());
  ASSERT_NE(paramAssign, nullptr) << "Module/ParamAssign is null";

  const hldb::Expr *const rhs = paramAssign->getRhs<hldb::Expr>();
  ASSERT_NE(rhs, nullptr) << "Module/ParamAssign/Rhs is null";

  ASSERT_EQ(hldb::prettyPrint(rhs), "c[3][2][1:0]");
}

TEST_F(PrettyPrint, ConditionOp) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("dut", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "Module is null";

  const hldb::ParamAssign *const paramAssign = hldb::findByName<hldb::ParamAssign>("d", module->getParamAssigns());
  ASSERT_NE(paramAssign, nullptr) << "Module/ParamAssign is null";

  const hldb::Expr *const rhs = paramAssign->getRhs<hldb::Expr>();
  ASSERT_NE(rhs, nullptr) << "Module/ParamAssign/Rhs is null";

  ASSERT_EQ(hldb::prettyPrint(rhs), "e ? 1 : 3");
}

TEST_F(PrettyPrint, SysFuncCall) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("dut", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "Module is null";

  const hldb::ParamAssign *const paramAssign = hldb::findByName<hldb::ParamAssign>("f", module->getParamAssigns());
  ASSERT_NE(paramAssign, nullptr) << "Module/ParamAssign is null";

  const hldb::Expr *const rhs = paramAssign->getRhs<hldb::Expr>();
  ASSERT_NE(rhs, nullptr) << "Module/ParamAssign/Rhs is null";

  ASSERT_EQ(hldb::prettyPrint(rhs), "$sformatf(\"%d\", g)");
}

TEST_F(PrettyPrint, AssignmentPatternOp) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("dut", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "Module is null";

  const hldb::ParamAssign *const paramAssign = hldb::findByName<hldb::ParamAssign>("h", module->getParamAssigns());
  ASSERT_NE(paramAssign, nullptr) << "Module/ParamAssign is null";

  const hldb::Expr *const rhs = paramAssign->getRhs<hldb::Expr>();
  ASSERT_NE(rhs, nullptr) << "Module/ParamAssign/Rhs is null";

  ASSERT_EQ(hldb::prettyPrint(rhs), "'{1, 2, 3}");
}

TEST_F(PrettyPrint, PackedArrayTypespec) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("dut", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "Module is null";

  const hldb::Parameter *const parameter = hldb::findByName<hldb::Parameter>("i", module->getParameters());
  ASSERT_NE(parameter, nullptr) << "Module/Parameter is null";

  const hldb::BitTypespec *const typespec = hldb::getTypespec<hldb::BitTypespec>(parameter);
  ASSERT_NE(typespec, nullptr) << "Module/Parameter/Typespec is null";

  ASSERT_EQ(hldb::prettyPrint(typespec), "bit [10:20][30:40]");
}

TEST_F(PrettyPrint, UnpackedArrayTypespec) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("dut", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "Module is null";

  const hldb::Parameter *const parameter = hldb::findByName<hldb::Parameter>("j", module->getParameters());
  ASSERT_NE(parameter, nullptr) << "Module/Parameter is null";

  const hldb::ArrayTypespec *const typespec = hldb::getTypespec<hldb::ArrayTypespec>(parameter);
  ASSERT_NE(typespec, nullptr) << "Module/Parameter/Typespec is null";

  ASSERT_EQ(hldb::prettyPrint(typespec), "int [10:20][30:40]");
}

TEST_F(PrettyPrint, UnpackedArrayTypespecOfPackedArrayTypespec_1) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("dut", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "Module is null";

  const hldb::Parameter *const parameter = hldb::findByName<hldb::Parameter>("k", module->getParameters());
  ASSERT_NE(parameter, nullptr) << "Module/Parameter is null";

  const hldb::ArrayTypespec *const typespec = hldb::getTypespec<hldb::ArrayTypespec>(parameter);
  ASSERT_NE(typespec, nullptr) << "Module/Parameter/Typespec is null";

  ASSERT_EQ(hldb::prettyPrint(typespec), "logic [10:20] [30:40]");
}

TEST_F(PrettyPrint, UnpackedArrayTypespecOfPackedArrayTypespec_2) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("dut", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "Module is null";

  const hldb::Parameter *const parameter = hldb::findByName<hldb::Parameter>("m", module->getParameters());
  ASSERT_NE(parameter, nullptr) << "Module/Parameter is null";

  const hldb::ArrayTypespec *const typespec = hldb::getTypespec<hldb::ArrayTypespec>(parameter);
  ASSERT_NE(typespec, nullptr) << "Module/Parameter/Typespec is null";

  ASSERT_EQ(hldb::prettyPrint(typespec), "l [10:20] [30:40]");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

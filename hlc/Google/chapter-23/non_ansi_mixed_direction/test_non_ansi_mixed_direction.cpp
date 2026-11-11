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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.1--non-ansi-mixed-direction.sv
//
// THE SOURCE
//
//   19: module mixed_direction(.p({a, e}));
//   20:   input a;
//   21:   output e;
//   22: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// Example 7 of 23.2.2.1: "Explicitly named port with mix of input and output
// directions". The LRM annotates it: "p contains both input and output
// directions." The object model's vpiDirection has a value for exactly this
// case, vpiMixedIO ("mixed input-output", vpi_user.h, Annex K). p is an
// explicit port (vpiExplicitName, 37.14), its port expression {a, e} is its
// low connection (37.14 detail 4), and as mixed_direction is never
// instantiated it has no high connection (37.14 detail 10).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error diagnostic
//   OneExplicitPortP           one port, p, vpiExplicitName TRUE
//   PortPIsMixedIO             vpiDirection vpiMixedIO
//   PortPExpressionIsLowConn   p has a vpiLowConn and no vpiHighConn
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The shape of the concatenation {a, e}. That is the expression model,
//     outside 23.2.2.1.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/port.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.2.1--non-ansi-mixed-direction.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

template <typename T>
const T *findFromSourceFile(const std::vector<T *> *collection) {
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (endsWith(object->getFile(), kSourceFileName)) return object;
    }
  }
  return nullptr;
}

int64_t integerProperty(const hldb::Any *object, int32_t property) {
  const hldb::Any::vpi_property_value_t value = object->getVpiPropertyValue(property);
  if (const int64_t *const number = std::get_if<int64_t>(&value)) return *number;
  return vpiUndefined;
}

size_t relationSize(const hldb::Any *object, int32_t relation) {
  const hldb::Any::get_by_vpi_type_return_t result = object->getByVpiType(relation);
  const std::vector<const hldb::Any *> *const objects = std::get<2>(result);
  if (objects != nullptr) return objects->size();
  return (std::get<1>(result) != nullptr) ? 1 : 0;
}

uint32_t lineOf(const Event &error) { return error.getArguments().empty() ? 0 : error.getArguments().front().m_line; }

bool isErrorClass(const Event &error) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
  if (errorInfo == nullptr) return false;
  const EventSeverity severity = errorInfo->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}

std::string describeError(const Event &error) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
  std::string description = " [" + std::to_string(static_cast<int32_t>(error.getType()));
  description += " line " + std::to_string(lineOf(error));
  if (errorInfo != nullptr) description += ": " + std::string(errorInfo->m_lines[0].m_text);
  return description + "]";
}
}  // namespace

class NonAnsiMixedDirectionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "non_ansi_mixed_direction.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string allErrors() {
    if ((m_session == nullptr) || (m_session->getEventListener() == nullptr)) return "no error container";
    std::string description;
    for (const Event &error : m_session->getEventListener()->getEvents()) {
      if (isErrorClass(error)) description += describeError(error);
    }
    return description;
  }

  static const hldb::Port *findPortP() {
    if (m_design == nullptr) return nullptr;
    const hldb::Module *const module = findFromSourceFile(m_design->getAllModules());
    if ((module == nullptr) || (module->getPorts() == nullptr) || (module->getPorts()->size() != 1)) return nullptr;
    return module->getPorts()->front();
  }
};

TEST_F(NonAnsiMixedDirectionTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(NonAnsiMixedDirectionTest, OneExplicitPortP) {
  const hldb::Port *const port = findPortP();
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "p");
  EXPECT_EQ(integerProperty(port, vpiExplicitName), 1);
}

TEST_F(NonAnsiMixedDirectionTest, PortPIsMixedIO) {
  const hldb::Port *const port = findPortP();
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(integerProperty(port, vpiDirection), static_cast<int64_t>(vpiMixedIO));
}

TEST_F(NonAnsiMixedDirectionTest, PortPExpressionIsLowConn) {
  const hldb::Port *const port = findPortP();
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(relationSize(port, vpiLowConn), 1u);
  EXPECT_EQ(relationSize(port, vpiHighConn), 0u);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

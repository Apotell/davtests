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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.1--non-ansi-same-input.sv
//
// THE SOURCE
//
//   19: module same_input(a, a);
//   20:   input a;
//   21: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// Example 6 of 23.2.2.1: "Two implicitly named ports connected to same
// internal net". The LRM annotates "input a;" with "This is legal. The
// inputs are tied together." Both ports are implicit ports whose
// port_expression is the simple identifier a, so a "shall be used as the
// port name" for each (23.2.2.1). The single "input a;" gives the one
// internal net a its direction, so both ports are inputs connected to that
// same net. Port indices start at zero (37.14 detail 9); the low connection
// is the port expression inside the module (37.14 detail 4).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error diagnostic
//   TwoPortsBothNamedA         two ports, both named a, indices 0 and 1
//   BothPortsAreInputs         vpiDirection vpiInput for both
//   OneInternalNetA            the module holds exactly one net, a
//   BothPortsConnectToNetA     each port's vpiLowConn resolves to that net
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - How an instance connects the two ports. The fixture has no instance;
//     connections are 23.3.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.2.1--non-ansi-same-input.sv";

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

const hldb::Any *relatedObject(const hldb::Any *object, int32_t relation) {
  const hldb::Any::get_by_vpi_type_return_t result = object->getByVpiType(relation);
  if (const hldb::Any *const single = std::get<1>(result)) return single;
  const std::vector<const hldb::Any *> *const objects = std::get<2>(result);
  return ((objects != nullptr) && (objects->size() == 1)) ? objects->front() : nullptr;
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

class NonAnsiSameInputTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "non_ansi_same_input.hlc"}); }
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

  static const hldb::Module *findModule() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllModules());
  }
};

TEST_F(NonAnsiSameInputTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(NonAnsiSameInputTest, TwoPortsBothNamedA) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getName(), "same_input");
  const hldb::PortCollection *const ports = module->getPorts();
  ASSERT_NE(ports, nullptr);
  ASSERT_EQ(ports->size(), 2u);
  for (size_t index = 0; index < ports->size(); ++index) {
    EXPECT_EQ((*ports)[index]->getName(), "a") << index;
    EXPECT_EQ(integerProperty((*ports)[index], vpiPortIndex), static_cast<int64_t>(index));
  }
}

TEST_F(NonAnsiSameInputTest, BothPortsAreInputs) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  const hldb::PortCollection *const ports = module->getPorts();
  ASSERT_NE(ports, nullptr);
  ASSERT_EQ(ports->size(), 2u);
  for (size_t index = 0; index < ports->size(); ++index) {
    EXPECT_EQ(integerProperty((*ports)[index], vpiDirection), static_cast<int64_t>(vpiInput)) << index;
  }
}

TEST_F(NonAnsiSameInputTest, OneInternalNetA) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  const hldb::NetCollection *const nets = module->getNets();
  ASSERT_NE(nets, nullptr);
  ASSERT_EQ(nets->size(), 1u);
  EXPECT_EQ(nets->front()->getName(), "a");
}

TEST_F(NonAnsiSameInputTest, BothPortsConnectToNetA) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  const hldb::PortCollection *const ports = module->getPorts();
  const hldb::NetCollection *const nets = module->getNets();
  ASSERT_NE(ports, nullptr);
  ASSERT_NE(nets, nullptr);
  ASSERT_EQ(ports->size(), 2u);
  ASSERT_EQ(nets->size(), 1u);
  for (size_t index = 0; index < ports->size(); ++index) {
    const hldb::Any *const connection = relatedObject((*ports)[index], vpiLowConn);
    ASSERT_NE(connection, nullptr) << index;
    const hldb::RefObj *const lowConn = any_cast<hldb::RefObj>(connection);
    ASSERT_NE(lowConn, nullptr) << index;
    EXPECT_EQ(lowConn->getActual(), nets->front()) << index;
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

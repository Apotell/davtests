#ifndef UHDM_EVENTLISTENER_H
#define UHDM_EVENTLISTENER_H
#pragma once

#include <uhdm/RTTI.h>

#include <initializer_list>
#include <string>

namespace uhdm {
class Any;

enum class EventType : uint16_t {
  UnsupportedExpr = 801,
  UnsupportedStmt,
  WrongObjectType,
  UndefinedPatternKey,
  UnmatchedFieldInPatternAssign,
  RealTypeAsSelect,
  ReturnValueVoidFunction,
  IllegalDefaultValue,
  MultipleContAssign,
  IllegalWireLhs,
  IllegalPackedDimension,
  NonSynthesizable,
  EnumConstSizeMismatch,
  DivideByZero,
  InternalErrorOutOfBound,
  UndefinedUserFunction,
  UnresolvedHierPath,
  UndefinedVariable,
  InvalidCaseStmtValue,
  UnsupportedTypespec,
  UnresolvedProperty,
  NonTemporalSequenceUse,
  NonPositiveValue,
  SignedUnsignedPortConn,
  ForcingUnsignedType,
  CollectionHasDuplicates,
  IllegalPropertyValue,
};

class Event : public RTTI {
 public:
  virtual ~Event() = default;
};

class EventListener {
 protected:
  EventListener() = default;

 public:
  virtual ~EventListener() = default;
  EventListener(const EventListener &) = delete;
  EventListener &operator=(const EventListener &) = delete;

  virtual void onEvent(EventType type, const std::initializer_list<std::string> &args,
                       const std::initializer_list<const Any *> &objects) = 0;
  virtual void onEvent(EventType type, Event *event) = 0;
};

class StreamEventListener : public EventListener {
 public:
  StreamEventListener();
  explicit StreamEventListener(std::ostream &out) : m_out(out) {}
  ~StreamEventListener() override = default;

  static EventListener *getDefaultInstance();

  void onEvent(EventType type, const std::initializer_list<std::string> &args,
               const std::initializer_list<const Any *> &objects) override;
  void onEvent(EventType type, Event *event) override;

 protected:
  std::ostream &m_out;
};
}  // namespace uhdm

UHDM_IMPLEMENT_RTTI_CAST_FUNCTIONS(event_cast, uhdm::Event)

#endif  // UHDM_EVENTLISTENER_H

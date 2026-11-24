#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/events/Events.h"

namespace lex::core::events {

// Bus de eventos tipado. Los listeners se dan de alta/baja con un Subscription
// RAII: al destruirlo se elimina automaticamente del bus.
class EventBus {
 public:
  using ListenerId = std::uint64_t;

 private:
  struct Entry {
    ListenerId id = 0;
    std::function<void(Event&)> call;
  };

  struct State {
    std::unordered_map<std::type_index, std::vector<Entry>> entries;
    ListenerId nextId = 1;
    std::mutex mutex;
  };

 public:
  class Subscription {
   public:
    Subscription() = default;
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

    Subscription(Subscription&& other) noexcept
        : state_(std::move(other.state_)), type_(other.type_), id_(other.id_) {
      other.type_ = nullptr;
      other.id_ = 0;
    }

    Subscription& operator=(Subscription&& other) noexcept {
      if (this != &other) {
        reset();
        state_ = std::move(other.state_);
        type_ = other.type_;
        id_ = other.id_;
        other.type_ = nullptr;
        other.id_ = 0;
      }
      return *this;
    }

    ~Subscription() { reset(); }

    explicit operator bool() const noexcept { return id_ != 0; }

    void reset() {
      if (id_ == 0 || type_ == nullptr) {
        state_.reset();
        return;
      }
      if (std::shared_ptr<State> state = state_.lock()) {
        std::lock_guard<std::mutex> lock(state->mutex);
        auto it = state->entries.find(std::type_index(*type_));
        if (it != state->entries.end()) {
          std::vector<Entry>& list = it->second;
          for (auto entry = list.begin(); entry != list.end(); ++entry) {
            if (entry->id == id_) {
              list.erase(entry);
              break;
            }
          }
        }
      }
      state_.reset();
      type_ = nullptr;
      id_ = 0;
    }

   private:
    friend class EventBus;

    Subscription(std::weak_ptr<State> state, const std::type_info* type,
                 ListenerId id)
        : state_(std::move(state)), type_(type), id_(id) {}

    std::weak_ptr<State> state_;
    const std::type_info* type_ = nullptr;
    ListenerId id_ = 0;
  };

  EventBus() = default;
  EventBus(const EventBus&) = delete;
  EventBus& operator=(const EventBus&) = delete;

  template <typename E, typename Fn>
  Subscription subscribe(Fn&& callback) {
    static_assert(std::is_base_of_v<Event, E>, "E debe derivar de Event");
    std::function<void(E&)> typed(std::forward<Fn>(callback));
    std::shared_ptr<State> state = state_;

    ListenerId id = 0;
    {
      std::lock_guard<std::mutex> lock(state->mutex);
      id = state->nextId++;
      Entry entry;
      entry.id = id;
      entry.call = [typed = std::move(typed)](Event& base) {
        typed(static_cast<E&>(base));
      };
      state->entries[std::type_index(typeid(E))].push_back(std::move(entry));
    }
    return Subscription(state, &typeid(E), id);
  }

  template <typename E>
  void publish(E& event) {
    static_assert(std::is_base_of_v<Event, E>, "E debe derivar de Event");
    std::vector<std::function<void(Event&)>> snapshot;

    {
      std::shared_ptr<State> state = state_;
      std::lock_guard<std::mutex> lock(state->mutex);
      auto it = state->entries.find(std::type_index(typeid(E)));
      if (it == state->entries.end()) return;
      snapshot.reserve(it->second.size());
      for (const Entry& entry : it->second) snapshot.push_back(entry.call);
    }

    // Se llama fuera del lock: un listener puede darse de baja durante publish.
    for (const std::function<void(Event&)>& call : snapshot) {
      if (event.cancelled) break;
      call(event);
    }
  }

  std::size_t listenerCount() const {
    std::shared_ptr<State> state = state_;
    std::lock_guard<std::mutex> lock(state->mutex);
    std::size_t total = 0;
    for (const auto& pair : state->entries) total += pair.second.size();
    return total;
  }

 private:
  std::shared_ptr<State> state_ = std::make_shared<State>();
};

}  // namespace lex::core::events

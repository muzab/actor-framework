/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <atomic>
#include <string>
#include <memory>
#include <typeindex>
#include <functional>
#include <type_traits>
#include <unordered_map>

#include "caf/fwd.hpp"
#include "caf/stream.hpp"
#include "caf/thread_hook.hpp"
#include "caf/config_value.hpp"
#include "caf/config_option.hpp"
#include "caf/actor_factory.hpp"
#include "caf/is_typed_actor.hpp"
#include "caf/type_erased_value.hpp"
#include "caf/named_actor_config.hpp"

#include "caf/detail/safe_equal.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {

/// Configures an `actor_system` on startup.
class actor_system_config {
public:
  // -- member types -----------------------------------------------------------

  using hook_factory = std::function<io::hook* (actor_system&)>;

  using hook_factory_vector = std::vector<hook_factory>;

  using thread_hooks = std::vector<std::unique_ptr<thread_hook>>;

  template <class K, class V>
  using hash_map = std::unordered_map<K, V>;

  using module_factory = std::function<actor_system::module* (actor_system&)>;

  using module_factory_vector = std::vector<module_factory>;

  using value_factory = std::function<type_erased_value_ptr ()>;

  using value_factory_string_map = hash_map<std::string, value_factory>;

  using value_factory_rtti_map = hash_map<std::type_index, value_factory>;

  using actor_factory_map = hash_map<std::string, actor_factory>;

  using portable_name_map = hash_map<std::type_index, std::string>;

  using error_renderer = std::function<std::string (uint8_t, atom_value,
                                                    const message&)>;

  using error_renderer_map = hash_map<atom_value, error_renderer>;

  using option_ptr = std::unique_ptr<config_option>;

  using option_vector = std::vector<option_ptr>;

  using group_module_factory = std::function<group_module* ()>;

  using group_module_factory_vector = std::vector<group_module_factory>;

  // -- nested classes ---------------------------------------------------------

  using named_actor_config_map = hash_map<std::string, named_actor_config>;

  class opt_group {
  public:
    opt_group(option_vector& xs, const char* category);

    template <class T>
    opt_group& add(T& storage, const char* name, const char* explanation) {
      xs_.emplace_back(make_config_option(storage, cat_, name, explanation));
      return *this;
    }

  private:
    option_vector& xs_;
    const char* cat_;
  };

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~actor_system_config();

  actor_system_config();

  actor_system_config(actor_system_config&&) = default;

  actor_system_config(const actor_system_config&) = delete;
  actor_system_config& operator=(const actor_system_config&) = delete;

  // -- modifiers --------------------------------------------------------------

  /// Parses `args` as tuple of strings containing CLI options and tries to
  /// open `ini_file_cstr` as INI formatted config file. The parsers tries to
  /// open `caf-application.ini` if `ini_file_cstr` is `nullptr`.
  actor_system_config& parse(message& args,
                             const char* ini_file_cstr = nullptr);

  /// Parses `args` as tuple of strings containing CLI options
  /// and `ini_stream` as INI formatted input stream.
  actor_system_config& parse(message& args, std::istream& ini);

  /// Parses the CLI options `{argc, argv}` and
  /// `ini_stream` as INI formatted input stream.
  actor_system_config& parse(int argc, char** argv, std::istream& ini);

  /// Parses the CLI options `{argc, argv}` and tries to open `ini_file_cstr`
  /// as INI formatted config file. The parsers tries to open
  /// `caf-application.ini` if `ini_file_cstr` is `nullptr`.
  actor_system_config& parse(int argc, char** argv,
                             const char* ini_file_cstr = nullptr);

  /// Allows other nodes to spawn actors created by `fun`
  /// dynamically by using `name` as identifier.
  /// @experimental
  actor_system_config& add_actor_factory(std::string name, actor_factory fun);

  /// Allows other nodes to spawn actors of type `T`
  /// dynamically by using `name` as identifier.
  /// @experimental
  template <class T, class... Ts>
  actor_system_config& add_actor_type(std::string name) {
    using handle = typename infer_handle_from_class<T>::type;
    if (!std::is_same<handle, actor>::value)
      add_message_type<handle>(name);
    return add_actor_factory(std::move(name), make_actor_factory<T, Ts...>());
  }

  /// Allows other nodes to spawn actors implemented by function `f`
  /// dynamically by using `name` as identifier.
  /// @experimental
  template <class F>
  actor_system_config& add_actor_type(std::string name, F f) {
    using handle = typename infer_handle_from_fun<F>::type;
    if (!std::is_same<handle, actor>::value)
      add_message_type<handle>(name);
    return add_actor_factory(std::move(name), make_actor_factory(std::move(f)));
  }

  /// Adds message type `T` with runtime type info `name`.
  template <class T>
  actor_system_config& add_message_type(std::string name) {
    static_assert(std::is_empty<T>::value
                  || std::is_same<T, actor>::value // silence add_actor_type err
                  || is_typed_actor<T>::value
                  || (std::is_default_constructible<T>::value
                      && std::is_copy_constructible<T>::value),
                  "T must provide default and copy constructors");
    std::string stream_name = "stream<";
    stream_name += name;
    stream_name += ">";
    add_message_type_impl<stream<T>>(std::move(stream_name));
    std::string vec_name = "std::vector<";
    vec_name += name;
    vec_name += ">";
    add_message_type_impl<std::vector<T>>(std::move(vec_name));
    add_message_type_impl<T>(std::move(name));
    return *this;
  }

  /// Enables the actor system to convert errors of this error category
  /// to human-readable strings via `renderer`.
  actor_system_config& add_error_category(atom_value x,
                                          error_renderer y);

  /// Enables the actor system to convert errors of this error category
  /// to human-readable strings via `to_string(T)`.
  template <class T>
  actor_system_config& add_error_category(atom_value category) {
    auto f = [=](uint8_t val, const std::string& ctx) -> std::string {
      std::string result;
      result = to_string(category);
      result += ": ";
      result += to_string(static_cast<T>(val));
      if (!ctx.empty()) {
        result += " (";
        result += ctx;
        result += ")";
      }
      return result;
    };
    return add_error_category(category, f);
  }

  /// Loads module `T` with optional template parameters `Ts...`.
  template <class T, class... Ts>
  actor_system_config& load() {
    module_factories.push_back([](actor_system& sys) -> actor_system::module* {
      return T::make(sys, detail::type_list<Ts...>{});
    });
    return *this;
  }

  /// Adds a factory for a new hook type to the middleman (if loaded).
  template <class Factory>
  actor_system_config& add_hook_factory(Factory f) {
    hook_factories.push_back(f);
    return *this;
  }

  /// Adds a hook type to the middleman (if loaded).
  template <class Hook>
  actor_system_config& add_hook_type() {
    return add_hook_factory([](actor_system& sys) -> Hook* {
      return new Hook(sys);
    });
  }

  /// Adds a hook type to the scheduler.
  template <class Hook, class... Ts>
  actor_system_config& add_thread_hook(Ts&&... ts) {
    thread_hooks_.emplace_back(new Hook(std::forward<Ts>(ts)...));
    return *this;
  }

  /// Sets a config by using its INI name `config_name` to `config_value`.
  actor_system_config& set(const char* cn, config_value cv);

  // -- parser and CLI state ---------------------------------------------------

  /// Stores whether the help text was printed. If set to `true`, the
  /// application should not use this config to initialize an `actor_system`
  /// and instead return from `main` immediately.
  bool cli_helptext_printed;

  /// Stores CLI arguments that were not consumed by CAF.
  message args_remainder;

  // -- caf-run parameters -----------------------------------------------------

  /// Stores whether this node was started in slave mode.
  bool slave_mode;

  /// Name of this node when started in slave mode.
  std::string slave_name;

  /// Credentials for connecting to the bootstrap node.
  std::string bootstrap_node;

  // -- streaming parameters ---------------------------------------------------

  /// @private
  size_t streaming_desired_batch_complexity_us;

  /// @private
  size_t streaming_max_batch_delay_us;

  /// @private
  size_t streaming_credit_round_interval_us;

  /// @private
  size_t streaming_tick_duration_us() const noexcept;

  /// Returns the greatest common divisor of `streaming_max_batch_delay` and
  /// `streaming_credit_round_interval`.
  inline timespan streaming_tick_duration() const noexcept {
    using us_t = std::chrono::microseconds;
    return timespan{us_t{streaming_tick_duration_us()}};
  }

  /// Returns the desired timespan in a sink or stage for processing a single
  /// batch.
  inline timespan streaming_desired_batch_complexity() const noexcept {
    using us_t = std::chrono::microseconds;
    return timespan{us_t{streaming_desired_batch_complexity_us}};
  }
  /// Returns the maximum delay for sending underfull batches.
  inline timespan streaming_max_batch_delay() const noexcept {
    using us_t = std::chrono::microseconds;
    return timespan{us_t{streaming_max_batch_delay_us}};
  }

  // Returns the desired timespan between two credit rounds.
  inline timespan streaming_credit_round_interval() const noexcept {
    using us_t = std::chrono::microseconds;
    return timespan{us_t{streaming_credit_round_interval_us}};
  }

  // -- scheduling parameters --------------------------------------------------

  atom_value scheduler_policy;
  size_t scheduler_max_threads;
  size_t scheduler_max_throughput;
  bool scheduler_enable_profiling;
  size_t scheduler_profiling_ms_resolution;
  std::string scheduler_profiling_output_file;

  // -- work-stealing parameters -----------------------------------------------

  size_t work_stealing_aggressive_poll_attempts;
  size_t work_stealing_aggressive_steal_interval;
  size_t work_stealing_moderate_poll_attempts;
  size_t work_stealing_moderate_steal_interval;
  size_t work_stealing_moderate_sleep_duration_us;
  size_t work_stealing_relaxed_steal_interval;
  size_t work_stealing_relaxed_sleep_duration_us;

  // -- logger parameters ------------------------------------------------------

  std::string logger_file_name;
  std::string logger_file_format;
  atom_value logger_console;
  std::string logger_console_format;
  std::string logger_component_filter;
  atom_value logger_verbosity;
  bool logger_inline_output;

  // -- middleman parameters ---------------------------------------------------

  atom_value middleman_network_backend;
  std::string middleman_app_identifier;
  bool middleman_enable_automatic_connections;
  size_t middleman_max_consecutive_reads;
  size_t middleman_heartbeat_interval;
  bool middleman_detach_utility_actors;
  bool middleman_detach_multiplexer;
  bool middleman_enable_tcp;
  bool middleman_enable_udp;
  size_t middleman_cached_udp_buffers;
  size_t middleman_max_pending_msgs;

  // -- OpenCL parameters ------------------------------------------------------

  std::string opencl_device_ids;

  // -- OpenSSL parameters -----------------------------------------------------

  std::string openssl_certificate;
  std::string openssl_key;
  std::string openssl_passphrase;
  std::string openssl_capath;
  std::string openssl_cafile;

  // -- factories --------------------------------------------------------------

  value_factory_string_map value_factories_by_name;
  value_factory_rtti_map value_factories_by_rtti;
  actor_factory_map actor_factories;
  module_factory_vector module_factories;
  hook_factory_vector hook_factories;
  group_module_factory_vector group_module_factories;

  // -- hooks ------------------------------------------------------------------

  thread_hooks thread_hooks_;

  // -- run-time type information ----------------------------------------------

  portable_name_map type_names_by_rtti;

  // -- rendering of user-defined types ----------------------------------------

  error_renderer_map error_renderers;

  // -- parsing parameters -----------------------------------------------------

  /// Configures the file path for the INI file, `caf-application.ini` per
  /// default.
  std::string config_file_path;

  // -- convenience functions --------------------------------------------------

  template <class F>
  void for_each_option(F f) const {
    const option_vector* all_options[] = { &options_, &custom_options_ };
    for (auto& opt_vec : all_options)
      for (auto& opt : *opt_vec)
        f(*opt);
  }

  // -- utility for caf-run ----------------------------------------------------

  // Config parameter for individual actor types.
  named_actor_config_map named_actor_configs;

  int (*slave_mode_fun)(actor_system&, const actor_system_config&);

protected:
  virtual std::string make_help_text(const std::vector<message::cli_arg>&);

  option_vector custom_options_;

private:
  template <class T>
  void add_message_type_impl(std::string name) {
    type_names_by_rtti.emplace(std::type_index(typeid(T)), name);
    value_factories_by_name.emplace(std::move(name), &make_type_erased_value<T>);
    value_factories_by_rtti.emplace(std::type_index(typeid(T)),
                                     &make_type_erased_value<T>);
  }

  static std::string render_sec(uint8_t, atom_value, const message&);

  static std::string render_exit_reason(uint8_t, atom_value, const message&);

  void extract_config_file_path(message& args);

  option_vector options_;
};

} // namespace caf


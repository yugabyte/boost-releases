/* Tells C++ coroutines about Outcome's result
(C) 2019-2021 Niall Douglas <http://www.nedproductions.biz/> (12 commits)
File Created: Oct 2019


Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#ifndef BOOST_OUTCOME_COROUTINE_SUPPORT_NAMESPACE_BEGIN
#error This header must only be included by outcome/coroutine_support.hpp or outcome/experimental/coroutine_support.hpp
#endif

#ifndef BOOST_OUTCOME_DETAIL_COROUTINE_SUPPORT_HPP
#define BOOST_OUTCOME_DETAIL_COROUTINE_SUPPORT_HPP

#include <atomic>
#include <cassert>

#if __cpp_impl_coroutine || (defined(_MSC_VER) && __cpp_coroutines) || (defined(__clang__) && __cpp_coroutines)
#ifndef BOOST_OUTCOME_HAVE_NOOP_COROUTINE
#if defined(__has_builtin)
#if __has_builtin(__builtin_coro_noop) || (!defined(__clang__) && __GNUC__ >= 10)
#define BOOST_OUTCOME_HAVE_NOOP_COROUTINE 1
#endif
#endif
#endif
#ifndef BOOST_OUTCOME_HAVE_NOOP_COROUTINE
#if _MSC_VER >= 1928 || (!defined(__clang__) && __GNUC__ >= 10)
#define BOOST_OUTCOME_HAVE_NOOP_COROUTINE 1
#else
#define BOOST_OUTCOME_HAVE_NOOP_COROUTINE 0
#endif
#endif
#if __has_include(<coroutine>)
#include <coroutine>
BOOST_OUTCOME_V2_NAMESPACE_BEGIN
namespace awaitables
{
  template <class Promise = void> using coroutine_handle = std::coroutine_handle<Promise>;
  template <class... Args> using coroutine_traits = std::coroutine_traits<Args...>;
  using std::suspend_always;
  using std::suspend_never;
#if BOOST_OUTCOME_HAVE_NOOP_COROUTINE
  using std::noop_coroutine;
#endif
}  // namespace awaitables
BOOST_OUTCOME_V2_NAMESPACE_END
#define BOOST_OUTCOME_FOUND_COROUTINE_HEADER 1
#elif __has_include(<experimental/coroutine>)
#include <experimental/coroutine>
BOOST_OUTCOME_V2_NAMESPACE_BEGIN
namespace awaitables
{
  template <class Promise = void> using coroutine_handle = std::experimental::coroutine_handle<Promise>;
  template <class... Args> using coroutine_traits = std::experimental::coroutine_traits<Args...>;
  using std::experimental::suspend_always;
  using std::experimental::suspend_never;
#if BOOST_OUTCOME_HAVE_NOOP_COROUTINE
  using std::experimental::noop_coroutine;
#endif
}  // namespace awaitables
BOOST_OUTCOME_V2_NAMESPACE_END
#define BOOST_OUTCOME_FOUND_COROUTINE_HEADER 1
#endif
#endif

BOOST_OUTCOME_V2_NAMESPACE_EXPORT_BEGIN
namespace awaitables
{
  namespace detail
  {
    struct error_type_not_found
    {
    };
    struct exception_type_not_found
    {
    };
    template <class T> struct type_found
    {
      using type = T;
    };
    template <class T, class U = typename T::error_type> constexpr inline type_found<U> extract_error_type(int /*unused*/) { return {}; }
    template <class T> constexpr inline type_found<error_type_not_found> extract_error_type(...) { return {}; }
    template <class T, class U = typename T::exception_type> constexpr inline type_found<U> extract_exception_type(int /*unused*/) { return {}; }
    template <class T> constexpr inline type_found<exception_type_not_found> extract_exception_type(...) { return {}; }

    BOOST_OUTCOME_TEMPLATE(class T, class U)
    BOOST_OUTCOME_TREQUIRES(BOOST_OUTCOME_TPRED(BOOST_OUTCOME_V2_NAMESPACE::detail::is_constructible<U, T>))
    inline bool try_set_error(T &&e, U *result)
    {
      new(result) U(static_cast<T &&>(e));
      return true;
    }
    template <class T> inline bool try_set_error(T && /*unused*/, ...) { return false; }
    BOOST_OUTCOME_TEMPLATE(class T, class U)
    BOOST_OUTCOME_TREQUIRES(BOOST_OUTCOME_TPRED(BOOST_OUTCOME_V2_NAMESPACE::detail::is_constructible<U, T>))
    inline void set_or_rethrow(T &e, U *result) { new(result) U(e); }
    template <class T> inline void set_or_rethrow(T &e, ...) { rethrow_exception(e); }
    template <class T> class fake_atomic
    {
      T _v;

    public:
      constexpr fake_atomic(T v)
          : _v(v)
      {
      }
      T load(std::memory_order /*unused*/) { return _v; }
      void store(T v, std::memory_order /*unused*/) { _v = v; }
    };

#ifdef BOOST_OUTCOME_FOUND_COROUTINE_HEADER
    template <class Awaitable, bool suspend_initial, bool use_atomic, bool is_void> struct outcome_promise_type
    {
      using container_type = typename Awaitable::container_type;
      using result_set_type = std::conditional_t<use_atomic, std::atomic<bool>, fake_atomic<bool>>;
      union
      {
        BOOST_OUTCOME_V2_NAMESPACE::detail::empty_type _default{};
        container_type result;
      };
      result_set_type result_set{false};
      coroutine_handle<> continuation;

      outcome_promise_type() noexcept {}
      outcome_promise_type(const outcome_promise_type &) = delete;
      outcome_promise_type(outcome_promise_type &&) = delete;
      outcome_promise_type &operator=(const outcome_promise_type &) = delete;
      outcome_promise_type &operator=(outcome_promise_type &&) = delete;
      ~outcome_promise_type()
      {
        if(result_set.load(std::memory_order_acquire))
        {
          result.~container_type();  // could throw
        }
      }
      auto get_return_object()
      {
        return Awaitable{*this};  // could throw bad_alloc
      }
      void return_value(container_type &&value)
      {
        assert(!result_set.load(std::memory_order_acquire));
        if(result_set.load(std::memory_order_acquire))
        {
          result.~container_type();  // could throw
        }
        new(&result) container_type(static_cast<container_type &&>(value));  // could throw
        result_set.store(true, std::memory_order_release);
      }
      void return_value(const container_type &value)
      {
        assert(!result_set.load(std::memory_order_acquire));
        if(result_set.load(std::memory_order_acquire))
        {
          result.~container_type();  // could throw
        }
        new(&result) container_type(value);  // could throw
        result_set.store(true, std::memory_order_release);
      }
      void unhandled_exception()
      {
        assert(!result_set.load(std::memory_order_acquire));
        if(result_set.load(std::memory_order_acquire))
        {
          result.~container_type();
        }
#ifndef BOOST_NO_EXCEPTIONS
        auto e = std::current_exception();
        auto ec = detail::error_from_exception(static_cast<decltype(e) &&>(e), {});
        // Try to set error code first
        if(!detail::error_is_set(ec) || !detail::try_set_error(static_cast<decltype(ec) &&>(ec), &result))
        {
          detail::set_or_rethrow(e, &result);  // could throw
        }
#else
        std::terminate();
#endif
        result_set.store(true, std::memory_order_release);
      }
      auto initial_suspend() noexcept
      {
        struct awaiter
        {
          bool await_ready() noexcept { return !suspend_initial; }
          void await_resume() noexcept {}
          void await_suspend(coroutine_handle<> /*unused*/) noexcept {}
        };
        return awaiter{};
      }
      auto final_suspend() noexcept
      {
        struct awaiter
        {
          bool await_ready() noexcept { return false; }
          void await_resume() noexcept {}
#if BOOST_OUTCOME_HAVE_NOOP_COROUTINE
          coroutine_handle<> await_suspend(coroutine_handle<outcome_promise_type> self) noexcept
          {
            return self.promise().continuation ? self.promise().continuation : noop_coroutine();
          }
#else
          void await_suspend(coroutine_handle<outcome_promise_type> self)
          {
            if(self.promise().continuation)
            {
              return self.promise().continuation.resume();
            }
          }
#endif
        };
        return awaiter{};
      }
    };
    template <class Awaitable, bool suspend_initial, bool use_atomic> struct outcome_promise_type<Awaitable, suspend_initial, use_atomic, true>
    {
      using container_type = void;
      using result_set_type = std::conditional_t<use_atomic, std::atomic<bool>, fake_atomic<bool>>;
      result_set_type result_set{false};
      coroutine_handle<> continuation;

      outcome_promise_type() {}
      outcome_promise_type(const outcome_promise_type &) = delete;
      outcome_promise_type(outcome_promise_type &&) = delete;
      outcome_promise_type &operator=(const outcome_promise_type &) = delete;
      outcome_promise_type &operator=(outcome_promise_type &&) = delete;
      ~outcome_promise_type() = default;
      auto get_return_object()
      {
        return Awaitable{*this};  // could throw bad_alloc
      }
      void return_void() noexcept
      {
        assert(!result_set.load(std::memory_order_acquire));
        result_set.store(true, std::memory_order_release);
      }
      void unhandled_exception()
      {
        assert(!result_set.load(std::memory_order_acquire));
        std::rethrow_exception(std::current_exception());  // throws
      }
      auto initial_suspend() noexcept
      {
        struct awaiter
        {
          bool await_ready() noexcept { return !suspend_initial; }
          void await_resume() noexcept {}
          void await_suspend(coroutine_handle<> /*unused*/) noexcept {}
        };
        return awaiter{};
      }
      auto final_suspend() noexcept
      {
        struct awaiter
        {
          bool await_ready() noexcept { return false; }
          void await_resume() noexcept {}
#if BOOST_OUTCOME_HAVE_NOOP_COROUTINE
          coroutine_handle<> await_suspend(coroutine_handle<outcome_promise_type> self) noexcept
          {
            return self.promise().continuation ? self.promise().continuation : noop_coroutine();
          }
#else
          void await_suspend(coroutine_handle<outcome_promise_type> self)
          {
            if(self.promise().continuation)
            {
              return self.promise().continuation.resume();
            }
          }
#endif
        };
        return awaiter{};
      }
    };
    template <class Awaitable, bool suspend_initial, bool use_atomic>
    constexpr inline auto move_result_from_promise_if_not_void(outcome_promise_type<Awaitable, suspend_initial, use_atomic, false> &p)
    {
      return static_cast<typename Awaitable::container_type &&>(p.result);
    }
    template <class Awaitable, bool suspend_initial, bool use_atomic>
    constexpr inline void move_result_from_promise_if_not_void(outcome_promise_type<Awaitable, suspend_initial, use_atomic, true> & /*unused*/)
    {
    }

    template <class Cont, bool suspend_initial, bool use_atomic> struct BOOST_OUTCOME_NODISCARD awaitable
    {
      using container_type = Cont;
      using promise_type = outcome_promise_type<awaitable, suspend_initial, use_atomic, std::is_void<container_type>::value>;
      coroutine_handle<promise_type> _h;

      awaitable(awaitable &&o) noexcept
          : _h(static_cast<coroutine_handle<promise_type> &&>(o._h))
      {
        o._h = nullptr;
      }
      awaitable(const awaitable &o) = delete;
      awaitable &operator=(awaitable &&) = delete;  // as per P1056
      awaitable &operator=(const awaitable &) = delete;
      ~awaitable()
      {
        if(_h)
        {
          _h.destroy();
        }
      }
      explicit awaitable(promise_type &p)  // could throw
          : _h(coroutine_handle<promise_type>::from_promise(p))
      {
      }
      bool await_ready() noexcept { return _h.promise().result_set.load(std::memory_order_acquire); }
      container_type await_resume()
      {
        assert(_h.promise().result_set.load(std::memory_order_acquire));
        if(!_h.promise().result_set.load(std::memory_order_acquire))
        {
          std::terminate();
        }
        return detail::move_result_from_promise_if_not_void(_h.promise());
      }
#if BOOST_OUTCOME_HAVE_NOOP_COROUTINE
      coroutine_handle<> await_suspend(coroutine_handle<> cont) noexcept
      {
        _h.promise().continuation = cont;
        return _h;
      }
#else
      void await_suspend(coroutine_handle<> cont)
      {
        _h.promise().continuation = cont;
        _h.resume();
      }
#endif
    };
#endif
  }  // namespace detail

}  // namespace awaitables

BOOST_OUTCOME_V2_NAMESPACE_END

#endif

#ifdef BOOST_OUTCOME_FOUND_COROUTINE_HEADER
BOOST_OUTCOME_COROUTINE_SUPPORT_NAMESPACE_EXPORT_BEGIN
/*! AWAITING HUGO JSON CONVERSION TOOL
SIGNATURE NOT RECOGNISED
*/
template <class T> using eager = BOOST_OUTCOME_V2_NAMESPACE::awaitables::detail::awaitable<T, false, false>;

/*! AWAITING HUGO JSON CONVERSION TOOL
SIGNATURE NOT RECOGNISED
*/
template <class T> using atomic_eager = BOOST_OUTCOME_V2_NAMESPACE::awaitables::detail::awaitable<T, false, true>;

/*! AWAITING HUGO JSON CONVERSION TOOL
SIGNATURE NOT RECOGNISED
*/
template <class T> using lazy = BOOST_OUTCOME_V2_NAMESPACE::awaitables::detail::awaitable<T, true, false>;

/*! AWAITING HUGO JSON CONVERSION TOOL
SIGNATURE NOT RECOGNISED
*/
template <class T> using atomic_lazy = BOOST_OUTCOME_V2_NAMESPACE::awaitables::detail::awaitable<T, true, true>;

BOOST_OUTCOME_COROUTINE_SUPPORT_NAMESPACE_END
#endif

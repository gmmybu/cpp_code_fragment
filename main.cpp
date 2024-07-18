#include <execution>
#include <concepts>
#include <vector>
#include <string>
#include <iostream>
using namespace std;

#include <coroutine>
#include <optional>
#include <functional>
#include <memory>

#include "coro.h"

static void run_async_task1(int i, std::function<void(std::error_code, int)> callback) {
    std::thread([=] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        callback({}, i + 100);
    }).detach();
}

static void run_async_task2(int i, std::function<void(std::error_code, std::string)> callback) {
    std::thread([=] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        callback({}, std::to_string(i) + " ack");
    }).detach();
}

static void run_async_task3(int i, std::function<void(std::error_code, int)> callback) {
    std::thread([=] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        callback({}, i + 300);
    }).detach();
}

static void check_stop_token(std::stop_token token) {
    if (token.stop_requested()) {
        throw std::runtime_error{"cancelled"};
    }
}

static co::task<std::string> test_async_to_coro(int pp, std::stop_token token) {
    std::cout << __FUNCTION__ << "[" << std::this_thread::get_id() << "]" << " start" << std::endl;
    check_stop_token(token);

    auto&& [ec, x] = co_await co::call_async(
        run_async_task1,
        50
    );
    
    if (ec) {
        co_return "error1";
    }

    std::cout << __FUNCTION__ << "[" << std::this_thread::get_id() << "]" << " " << x << std::endl;

    check_stop_token(token);

    auto&& [ec2, y] = co_await co::call_async(
        run_async_task2,
        50
    );

    if (ec2) {
        co_return "error2";
    }

    std::cout << __FUNCTION__ << "[" << std::this_thread::get_id() << "]" << " " << y << std::endl;

    check_stop_token(token);

    if (pp == 100) {
        co_return "got 100";
    }

    co_return "ok";
}

static co::task<std::string> test_coro_wit_exception(int pp) {
    std::cout << __FUNCTION__ << "[" << std::this_thread::get_id() << "]" << " start" << std::endl;

    co_await co::call_async(
        run_async_task1,
        50
    );

    throw std::runtime_error{ "throw runtime error" };

    co_return "ok";
}

static void run_async_void(std::function<void()> cb) {
    std::thread([=] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        cb();
    }).detach();
}

static void run_immediately(std::function<void()> cb) {
    cb();
}

static co::task<void> test_coro_void() {

    co_await co::call_async(
        run_async_void
    );

    std::cout << __FUNCTION__ << "[" << std::this_thread::get_id() << "]" << " test_coro_void " << std::endl;
}

static co::task<int> test_coro_nest(std::stop_token token) {
    auto&& [ec, a] = co_await co::call_async(
        run_async_task2,
        1030
    );
    if (ec) {
        std::cout << __FUNCTION__ << " got error: " << ec.message() << std::endl;
        co_return 0;
    }

    std::cout << __FUNCTION__ << "[" << std::this_thread::get_id() << "]" << " a = " << a << std::endl;


    co_await co::call_async(
        run_async_void
    );

    std::cout << __FUNCTION__ << "[" << std::this_thread::get_id() << "]" << " run_async_void " << std::endl;


    co_await co::call_async(
        run_immediately
    );

    std::cout << __FUNCTION__ << "[" << std::this_thread::get_id() << "]" << " run_immediately " << std::endl;


    try {
        co_await call_coro(
            test_coro_wit_exception,
            23
        );
    }
    catch (std::runtime_error& ex) {
        std::cout << __FUNCTION__ << " got exception 1: " << ex.what() << std::endl;
    }

    try {
        std::string ret = co_await call_coro(
            test_async_to_coro,
            100,
            token
        );
        std::cout << __FUNCTION__ << " got result: " << ret << std::endl;
    } catch (std::runtime_error& ex) {
        std::cout << __FUNCTION__ << " got exception: " << ex.what() << std::endl;
    }

    co_return 5;
}

static void run_async_task_refer(std::function<void(const std::string&, std::string)> callback) {
    std::string sss = "hello";
    std::string bbb = "abc";
    callback(sss, bbb);
}

static co::task<void> run_coro_async_refer() {
    auto&& [str, str2] = co_await co::call_async(
        run_async_task_refer
    );
    std::cout << str << ", " << str2 << std::endl;
}

static co::task<const std::string&> run_coro_with_refer() {
    static std::string xxx = "xxxxxxx";
    co_return xxx;
}

int main() {
    std::stop_source source;
    auto tt = co::run_coro(
        test_coro_nest,
        source.get_token()
    );

    std::cout << "final result = " << tt.get() << std::endl;

    auto xx = co::run_coro(
        test_coro_void
    );
    xx.get();

    auto yy = co::run_coro(
        run_coro_async_refer
    );
    yy.get();


    auto zz = co::run_coro(
        run_coro_with_refer
    );
    std::cout << zz.get() << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return 0;
}

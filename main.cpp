#include <vector>
#include <string>
#include <iostream>
using namespace std;

#include <coroutine>
#include <optional>
#include <functional>
#include <memory>

#include "coro.h"

static void call_async_task1(int i, std::function<void(std::error_code, int)> callback) {
    std::thread([=] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        callback({}, i + 100);
    }).detach();
}

static void call_async_task2(int i, std::function<void(std::error_code, std::string)> callback) {
    std::thread([=] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        callback({}, std::to_string(i) + " ack");
    }).detach();
}

static void call_async_task3(int i, std::function<void(std::error_code, int)> callback) {
    std::thread([=] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        callback({}, i + 300);
    }).detach();
}

static co::task<std::string> test_async_to_coro(int pp) {
    std::cout << __FUNCTION__ << "[" << std::this_thread::get_id() << "]" << " start" << std::endl;

    auto&& [ec, x] = co_await co::call_async<std::error_code, int>(
        call_async_task1,
        50
    );

    if (ec) {
        co_return "error1";
    }

    std::cout << __FUNCTION__ << "[" << std::this_thread::get_id() << "]" << " " << x << std::endl;


    auto&& [ec2, y] = co_await co::call_async<std::error_code, std::string>(
        call_async_task2,
        50
    );

    if (ec2) {
        co_return "error2";
    }

    std::cout << __FUNCTION__ << "[" << std::this_thread::get_id() << "]" << " " << y << std::endl;

    if (pp == 100) {
        co_return "got 100";
    }

    co_return "ok";
}

static co::task<std::string> test_coro_wit_exception(int pp) {
    std::cout << __FUNCTION__ << "[" << std::this_thread::get_id() << "]" << " start" << std::endl;

    co_await co::call_async<std::error_code, int>(
        call_async_task1,
        50
    );

    throw std::runtime_error{ "throw runtime error" };
    co_return "ok";
}

static void call_async_void(std::function<void()> cb) {
    std::thread([=] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        cb();
    }).detach();
}

static void run_immediately(std::function<void()> cb) {
    cb();
}

static co::task<void> test_coro_void() {
    co_await co::call_async<>(
        call_async_void
    );

    std::cout << __FUNCTION__ << "[" << std::this_thread::get_id() << "]" << " test_coro_void " << std::endl;
}

static co::task<int> test_coro_nest() {
    auto&& [ec, a] = co_await co::call_async<std::error_code, std::string>(
        call_async_task2,
        1030
    );
    if (ec) {
        std::cout << __FUNCTION__ << " got error: " << ec.message() << std::endl;
        co_return 0;
    }

    std::cout << __FUNCTION__ << "[" << std::this_thread::get_id() << "]" << " a = " << a << std::endl;

    co_await co::call_async<>(
        call_async_void
    );

    std::cout << __FUNCTION__ << "[" << std::this_thread::get_id() << "]" << " call_async_void " << std::endl;

    co_await co::call_async<>(
        run_immediately
    );

    std::cout << __FUNCTION__ << "[" << std::this_thread::get_id() << "]" << " run_immediately " << std::endl;

    try {
        co_await co::call_coro(
            test_coro_wit_exception,
            23
        );
    }
    catch (std::runtime_error& ex) {
        std::cout << __FUNCTION__ << " got exception 1: " << ex.what() << std::endl;
    }

    try {
        std::string ret = co_await co::call_coro(
            test_async_to_coro,
            100
        );
        std::cout << __FUNCTION__ << " got result: " << ret << std::endl;
    } catch (std::runtime_error& ex) {
        std::cout << __FUNCTION__ << " got exception: " << ex.what() << std::endl;
    }

    co_return 5;
}

static void call_async_task_refer(std::function<void(const std::string&, std::string)> callback) {
    std::string sss = "hello";
    std::string bbb = "abc";
    callback(sss, bbb);
}

static co::task<void> run_coro_async_refer() {
    auto&& [str, str2] = co_await co::call_async<std::string, std::string>(
        call_async_task_refer
    );
    std::cout << str << ", " << str2 << std::endl;
}

static co::task<const std::string&> run_coro_with_refer() {
    static std::string xxx = "xxxxxxx";
    co_return xxx;
}


class my_class {
public:
    co::task<std::string> run_coro(int y) {
        co_await co::call_coro(
            run_coro_async_refer
        );

        co_return "returned " + std::to_string(y);
    }

    co::task<std::string> run_coro_const(int y) const {
        co_await co::call_coro(
            run_coro_async_refer
        );

        co_return "returned " + std::to_string(y + x);
    }

    void call_async_task_refer(std::function<void(const std::string&, std::string)> callback) {
        std::string sss = "hello";
        std::string bbb = "abc";
        callback(sss, bbb);
    }

private:
    int x = 5;
};

int main() {
    auto tt = co::run_coro(
        test_coro_nest
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

    my_class cls;
    auto kk = co::run_coro(
        &my_class::run_coro,
        &cls,
        20
    );
    std::cout << kk.get() << std::endl;

    std::shared_ptr<my_class> mc =
        std::make_shared<my_class>();

    auto jj = co::run_coro(
        &my_class::run_coro_const,
        mc.get(),
        20
    );
    std::cout << jj.get() << std::endl;
    std::cout << "cls = " << mc.get() << std::endl;


    auto lam = [mc](int y) -> co::task<std::string> {
        std::cout << "cls = " << mc.get() << std::endl;
        std::string ss = co_await co::call_coro(
            &my_class::run_coro_const,
            mc.get(),
            30
        );

        auto&& [s3, s4] = co_await co::call_async<std::string, std::string>(
            &my_class::call_async_task_refer,
            mc.get()
        );

        co_return "returned " + std::to_string(y) + " " + ss + " " + s3 + " " + s4;
    };

    auto ll = co::run_coro(
        lam,
        8
    );
    std::cout << ll.get() << std::endl;
       
    std::cout << "cls = " << mc.get() << std::endl;


    auto lam2 = [mc](this auto self, int y) {
        std::cout << "cls = " << self.mc.get() << std::endl;
        std::string ss = co::run_coro(
            &my_class::run_coro_const,
            self.mc.get(),
            30
        ).get();

        return "returned " + std::to_string(y) + " " + ss;
    };

    lam2(23);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return 0;
}

/* SPDX-License-Identifier: Apache-2.0 */

#include <platform/windows/TerminalOwner.hpp>

#include <atomic>
#include <thread>
#include <vector>

int main()
{
    rasterm::TerminalOwner first;
    rasterm::TerminalOwner second;
    if (!first.acquire() || second.acquire()) return 1;
    first.release();
    if (!second.acquire()) return 2;
    second.release();

    std::atomic<bool> start = false;
    std::atomic<int> active = 0;
    std::atomic<int> maximum = 0;
    std::vector<std::thread> workers;
    for (int index = 0; index < 16; ++index) {
        workers.emplace_back([&] {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            rasterm::TerminalOwner owner;
            if (owner.acquire()) {
                const int count = active.fetch_add(1) + 1;
                int observed = maximum.load();
                while (observed < count && !maximum.compare_exchange_weak(observed, count)) {}
                std::this_thread::yield();
                active.fetch_sub(1);
            }
        });
    }
    start.store(true, std::memory_order_release);
    for (auto& worker : workers) worker.join();
    return maximum.load() == 1 ? 0 : 3;
}
/* SPDX-License-Identifier: Apache-2.0 */

#include <platform/windows/TerminalOwner.hpp>

#include <mutex>

namespace rasterm {
namespace {

std::mutex ownerMutex;
const TerminalOwner* activeOwner = nullptr;

}

TerminalOwner::~TerminalOwner()
{
    release();
}

bool TerminalOwner::acquire() noexcept
{
    std::lock_guard lock(ownerMutex);
    if (owned) {
        return true;
    }
    if (activeOwner != nullptr) {
        return false;
    }
    activeOwner = this;
    owned = true;
    return true;
}

void TerminalOwner::release() noexcept
{
    std::lock_guard lock(ownerMutex);
    if (activeOwner == this) {
        activeOwner = nullptr;
    }
    owned = false;
}

}
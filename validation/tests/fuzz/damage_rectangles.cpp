/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/Damage.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size)
{
    if (size < sizeof(std::int32_t) * 6) return 0;
    std::int32_t values[6]{};
    std::memcpy(values, data, sizeof(values));
    const rasterm::DamageRect rectangle{ values[0], values[1], values[2], values[3] };
    const rasterm::DamageView damage{ &rectangle, 1, true };
    (void)damage.isValidFor(values[4], values[5]);
    return 0;
}

/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/capi.h>

int main(void)
{
    rasterm_engine_options options;
    rasterm_engine_options_init(&options);
    return options.struct_size == sizeof(options) ? 0 : 1;
}
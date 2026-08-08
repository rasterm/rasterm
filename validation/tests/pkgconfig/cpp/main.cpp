/* SPDX-License-Identifier: Apache-2.0 */

#include <rasterm/rasterm.hpp>

int main()
{
    rasterm::Presenter presenter;
    return presenter.isInitialized() ? 1 : 0;
}
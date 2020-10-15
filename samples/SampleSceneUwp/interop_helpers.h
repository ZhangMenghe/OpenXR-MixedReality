#ifndef INTEROP_HELPER_H
#define INTEROP_HELPER_H

#include "pch.h"
template <typename T>
T from_cx(Platform::Object ^ from) {
    T to{nullptr};

    winrt::check_hresult(reinterpret_cast<::IUnknown*>(from)->QueryInterface(winrt::guid_of<T>(), winrt::put_abi(to)));

    return to;
}

template <typename T>
    T ^ to_cx(winrt::Windows::Foundation::IUnknown const& from) {
        return safe_cast<T ^>(reinterpret_cast<Platform::Object ^>(winrt::get_abi(from)));
    }

#endif // !INTEROP_HELPER_H

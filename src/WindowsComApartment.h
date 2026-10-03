#pragma once
#include <windows.h>
#include <objbase.h>

// Mantener el apartamento COM hasta DESPUÉS del destructor de QApplication:
// Qt y los servicios nativos pueden conservar objetos WinRT durante el cierre.
class WindowsComApartment final {
public:
    WindowsComApartment() noexcept
        : result_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)) {}
    ~WindowsComApartment() { if (SUCCEEDED(result_)) CoUninitialize(); }
    WindowsComApartment(const WindowsComApartment&) = delete;
    WindowsComApartment& operator=(const WindowsComApartment&) = delete;
private:
    HRESULT result_;
};

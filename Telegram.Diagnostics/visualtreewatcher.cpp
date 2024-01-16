#include "stdafx.h"

#include "visualtreewatcher.hpp"

#include <winrt/Windows.Storage.h>
#include <fstream>

#define EXTRA_DEBUG 0

const std::wstring TO_MASTER_LONG =
    L"RootPage/LayoutRoot (Grid)[0]/Navigation (SplitView)/Grid[1]/ContentRoot "
    L"(Grid)[0]/Border/Frame/ContentPresenter/";
const std::wstring TO_MASTER_SHORT = L"RootPage/.../";

const std::wstring TO_DETAIL_LONG =
    L"MainPage/Grid[4]/MasterDetail (MasterDetailView)/AdaptivePanel "
    L"(MasterDetailPanel)[1]/DetailHeaderPresenter2 (Grid)[3]/DetailPresenter "
    L"(Grid)/Frame/ContentPresenter/";
const std::wstring TO_DETAIL_SHORT = L"MainPage/.../";

const std::wstring TO_PIVOTITEM_LONG =
    L"MainPage/Grid[4]/MasterDetail (MasterDetailView)/AdaptivePanel "
    L"(MasterDetailPanel)[2]/MasterFrame "
    L"(ContentControl)/ContentPresenter/Grid[1]/rpMasterTitlebar "
    L"(Pivot)/RootElement (Grid)[1]/Grid/ScrollViewer "
    L"(ScrollViewer)/Grid/ScrollContentPresenter "
    L"(ScrollContentPresenter)/Panel (PivotPanel)/PivotLayoutElement "
    L"(Grid)[5]/PivotItemPresenter (ItemsPresenter)/Grid/PivotItem";
const std::wstring TO_PIVOTITEM_SHORT = L"MainPage/.../PivotItem";

inline void OutputDebugStringFormat(LPCWSTR pwhFormat, ...) {
    va_list args;
    va_start(args, pwhFormat);
    WCHAR buffer[1024];
    vswprintf_s(buffer, 1024, pwhFormat, args);
    OutputDebugStringW(buffer);
    va_end(args);
}

inline bool Replace(std::wstring& str,
                    const std::wstring& from,
                    const std::wstring& to) {
    size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

VisualTreeWatcher::VisualTreeWatcher(winrt::com_ptr<IUnknown> site)
    : m_xamlDiagnostics(site.as<IXamlDiagnostics>()) {
    m_unhandledException = wux::Application::Current().UnhandledException(
        winrt::auto_revoke, [this](wf::IInspectable const& sender,
                                   wux::UnhandledExceptionEventArgs const& e) {
            auto exception = e.Exception();
            if (exception == 0x802B0014) {
                std::wstring path =
                    winrt::Windows::Storage::ApplicationData::Current()
                        .LocalFolder()
                        .Path()
                        .c_str();
                std::wofstream f(path + L"\\LayoutCycle.txt",
                                 std::wofstream::out | std::wofstream::trunc);

                for (auto& item : m_history) {
                    auto path = std::wstring(item.path);
                    Replace(path, TO_MASTER_LONG, TO_MASTER_SHORT);
                    Replace(path, TO_DETAIL_LONG, TO_DETAIL_SHORT);
                    Replace(path, TO_PIVOTITEM_LONG, TO_PIVOTITEM_SHORT);
                    f << path << L" 0x" << std::hex << item.handle
                      << L"\n";
                }

                f.close();
            }
        });
    // const auto treeService = m_xamlDiagnostics.as<IVisualTreeService3>();
    // winrt::check_hresult(treeService->AdviseVisualTreeChange(this));

    // Calling AdviseVisualTreeChange from the current thread causes the app to
    // hang on Windows 10 in Advising::RunOnUIThread. Creating a new thread and
    // calling it from there fixes it.
    HANDLE thread = CreateThread(
        nullptr, 0,
        [](LPVOID lpParam) -> DWORD {
            auto watcher = reinterpret_cast<VisualTreeWatcher*>(lpParam);
            const auto treeService =
                watcher->m_xamlDiagnostics.as<IVisualTreeService3>();
            winrt::check_hresult(treeService->AdviseVisualTreeChange(watcher));
            return 0;
        },
        this, 0, nullptr);
    if (thread) {
        CloseHandle(thread);
    }
}

void VisualTreeWatcher::Activate() {
    std::shared_lock lock(m_dlgMainMutex);
}

VisualTreeWatcher::~VisualTreeWatcher() {
    std::shared_lock lock(m_dlgMainMutex);
}

HRESULT VisualTreeWatcher::OnVisualTreeChange(
    ParentChildRelation parentChildRelation,
    VisualElement element,
    VisualMutationType mutationType) try {
#if EXTRA_DEBUG
    try {
        WCHAR szBuffer[1025];
        OutputDebugString(L"========================================\n");

        wsprintf(szBuffer, L"Parent: %p\n", (void*)parentChildRelation.Parent);
        OutputDebugString(szBuffer);
        wsprintf(szBuffer, L"Child: %p\n", (void*)parentChildRelation.Child);
        OutputDebugString(szBuffer);
        wsprintf(szBuffer, L"Child index: %u\n",
                 parentChildRelation.ChildIndex);
        OutputDebugString(szBuffer);
        wsprintf(szBuffer, L"Handle: %p\n", (void*)element.Handle);
        OutputDebugString(szBuffer);
        wsprintf(szBuffer, L"Type: %s\n", element.Type);
        OutputDebugString(szBuffer);
        wsprintf(szBuffer, L"Name: %s\n", element.Name);
        OutputDebugString(szBuffer);
        wsprintf(szBuffer, L"NumChildren: %u\n", element.NumChildren);
        OutputDebugString(szBuffer);
        OutputDebugString(L"~~~~~~~~~~\n");

        switch (mutationType) {
            case Add:
                OutputDebugString(L"Mutation type: Add\n");
                break;

            case Remove:
                OutputDebugString(L"Mutation type: Remove\n");
                break;

            default:
                wsprintf(szBuffer, L"Mutation type: %d\n",
                         static_cast<int>(mutationType));
                OutputDebugString(szBuffer);
                break;
        }

        wux::FrameworkElement frameworkElement = nullptr;

        const auto inspectable = FromHandle<wf::IInspectable>(element.Handle);
        frameworkElement = inspectable.try_as<wux::FrameworkElement>();

        if (frameworkElement) {
            wsprintf(szBuffer, L"FrameworkElement address: %p\n",
                     winrt::get_abi(frameworkElement));
            OutputDebugString(szBuffer);
            wsprintf(szBuffer, L"FrameworkElement class: %s\n",
                     winrt::get_class_name(frameworkElement).c_str());
            OutputDebugString(szBuffer);
            wsprintf(szBuffer, L"FrameworkElement name: %s\n",
                     frameworkElement.Name().c_str());
            OutputDebugString(szBuffer);
        }

        if (parentChildRelation.Parent) {
            if (frameworkElement) {
                wsprintf(szBuffer, L"Real parent address: %p\n",
                         winrt::get_abi(
                             frameworkElement.Parent().as<wf::IInspectable>()));
                OutputDebugString(szBuffer);
            }

            const auto inspectable =
                FromHandle<wf::IInspectable>(parentChildRelation.Parent);
            const auto frameworkElement =
                inspectable.try_as<wux::FrameworkElement>();
            if (frameworkElement) {
                wsprintf(szBuffer, L"Parent FrameworkElement address: %p\n",
                         winrt::get_abi(frameworkElement));
                OutputDebugString(szBuffer);
                wsprintf(szBuffer, L"Parent FrameworkElement class: %s\n",
                         winrt::get_class_name(frameworkElement).c_str());
                OutputDebugString(szBuffer);
                wsprintf(szBuffer, L"Parent FrameworkElement name: %s\n",
                         frameworkElement.Name().c_str());
                OutputDebugString(szBuffer);
            }
        }
    } catch (...) {
        HRESULT hr = winrt::to_hresult();
        WCHAR szBuffer[1025];
        wsprintf(szBuffer, L"Error %X\n", hr);
        OutputDebugString(szBuffer);
    }
#endif  // EXTRA_DEBUG

    switch (mutationType) {
        case Add:
            ElementAdded(parentChildRelation, element);
            break;

        case Remove:
            ElementRemoved(element.Handle);
            break;

        default:
            ATLASSERT(FALSE);
            break;
    }

    return S_OK;
} catch (...) {
    ATLASSERT(FALSE);
    // Returning an error prevents (some?) further messages, always return
    // success.
    // return winrt::to_hresult();
    return S_OK;
}

HRESULT VisualTreeWatcher::OnElementStateChanged(InstanceHandle,
                                                 VisualElementState,
                                                 LPCWSTR) noexcept {
    return S_OK;
}

void VisualTreeWatcher::ElementAdded(
    const ParentChildRelation& parentChildRelation,
    const VisualElement& element) {
    wux::FrameworkElement frameworkElement = nullptr;

    const auto inspectable = FromHandle<wf::IInspectable>(element.Handle);
    frameworkElement = inspectable.try_as<wux::FrameworkElement>();

    if (frameworkElement) {
        const std::wstring_view elementType{
            element.Type, element.Type ? SysStringLen(element.Type) : 0};
        const std::wstring_view elementName{
            element.Name, element.Name ? SysStringLen(element.Name) : 0};

        std::wstring path;

        if (!elementName.empty()) {
            path += elementName;
            path += L" (";
        }

        size_t index = elementType.find_last_of('.');
        if (index >= 0) {
            path += elementType.substr(index + 1);
        } else {
            path += elementType;
        }

        if (!elementName.empty()) {
            path += L")";
        }

        m_elements[element.Handle] =
            ElementItem{.parent = parentChildRelation.Parent,
                        .name = path,
                        .numChildren = element.NumChildren,
                        .childIndex = parentChildRelation.ChildIndex};

        if (!parentChildRelation.Parent) {
            return;
        }

        auto handle = element.Handle;

        m_sizeChangedTokens[element.Handle] = frameworkElement.SizeChanged(
            winrt::auto_revoke,
            [this, handle](IInspectable const& sender,
                           wux::SizeChangedEventArgs const& args) {
                auto previous = args.PreviousSize();
                if (previous.Width > 0 || previous.Height > 0) {
                    auto path = FindPathToRoot(handle);

#if EXTRA_DEBUG
                    OutputDebugStringFormat(L"SizeChanged for %s\n",
                                            path.c_str());
#endif

                    if (!m_history.empty()) {
                        const auto& peek = m_history.back();
                        size_t index = path.find(peek.path);
                        if (index == 0 && path.size() > peek.path.size()) {
                            m_history.pop_back();
                        }
                    }

                    m_history.push_back({.handle = handle, .path = path});

                    while (200 < m_history.size()) {
                        m_history.pop_front();
                    }
                }
            });
    }
}

void VisualTreeWatcher::ElementRemoved(InstanceHandle handle) {
    m_elements.erase(handle);
    m_sizeChangedTokens.erase(handle);
}

std::wstring VisualTreeWatcher::FindPathToRoot(InstanceHandle parent) {
    unsigned int numChildren;
    auto path = FindPathToRootImpl(parent, numChildren);
    return path;
}

std::wstring VisualTreeWatcher::FindPathToRootImpl(InstanceHandle handle,
                                                   unsigned int& numChildren) {
    auto find = m_elements.find(handle);
    if (find != m_elements.end()) {
        std::wstring path = find->second.name;

        if (find->second.parent) {
            auto parent = FindPathToRootImpl(find->second.parent, numChildren);

            if (numChildren > 1) {
                parent +=
                    L"[" + std::to_wstring(find->second.childIndex) + L"]";
            }

            path = parent + L"/" + path;
        }

        numChildren = find->second.numChildren;
        return path;
    }

    numChildren = 0;
    return L"";
}

#pragma once

#include "winrt.hpp"

#include <deque>

struct VisualTreeWatcher : winrt::implements<VisualTreeWatcher,
                                             IVisualTreeServiceCallback2,
                                             winrt::non_agile> {
    VisualTreeWatcher(winrt::com_ptr<IUnknown> site);

    void Activate();

    VisualTreeWatcher(const VisualTreeWatcher&) = delete;
    VisualTreeWatcher& operator=(const VisualTreeWatcher&) = delete;

    VisualTreeWatcher(VisualTreeWatcher&&) = delete;
    VisualTreeWatcher& operator=(VisualTreeWatcher&&) = delete;

    ~VisualTreeWatcher();

   private:
    HRESULT STDMETHODCALLTYPE
    OnVisualTreeChange(ParentChildRelation relation,
                       VisualElement element,
                       VisualMutationType mutationType) override;
    HRESULT STDMETHODCALLTYPE
    OnElementStateChanged(InstanceHandle element,
                          VisualElementState elementState,
                          LPCWSTR context) noexcept override;

    template <typename T>
    T FromHandle(InstanceHandle handle) {
        wf::IInspectable obj;
        winrt::check_hresult(m_xamlDiagnostics->GetIInspectableFromHandle(
            handle, reinterpret_cast<::IInspectable**>(winrt::put_abi(obj))));

        return obj.as<T>();
    }

    void ElementAdded(const ParentChildRelation& parentChildRelation,
                      const VisualElement& element);
    void ElementRemoved(InstanceHandle handle);

    std::wstring FindPathToRoot(InstanceHandle parent);
    std::wstring FindPathToRootImpl(InstanceHandle parent,
                                    unsigned int& numChildren);

    winrt::com_ptr<IXamlDiagnostics> m_xamlDiagnostics;

    struct HistoryItem {
        InstanceHandle handle;
        std::wstring path;
    };

    struct ElementItem {
        InstanceHandle parent;
        std::wstring name;
        unsigned int numChildren;
        unsigned int childIndex;
    };

    std::shared_mutex m_dlgMainMutex;
    wux::Application::UnhandledException_revoker m_unhandledException;
    std::unordered_map<InstanceHandle,
                       wux::FrameworkElement::SizeChanged_revoker>
        m_sizeChangedTokens;
    std::unordered_map<InstanceHandle, ElementItem> m_elements;
    std::deque<HistoryItem> m_history;
};

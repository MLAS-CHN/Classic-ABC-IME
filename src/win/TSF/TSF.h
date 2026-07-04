#pragma once

#include "Globals.h"
#include "Compartment.h"
#include "proto_core.h"

class TSF : public ITfTextInputProcessorEx,
                  public ITfThreadMgrEventSink,
                  public ITfTextEditSink,
                  public ITfTextLayoutSink,
                  public ITfKeyEventSink,
                  public ITfCompositionSink,
                  public ITfThreadFocusSink,
                  public ITfActiveLanguageProfileNotifySink,
                  public ITfEditSession {
 public:
  TSF();
  ~TSF();

  STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject);
  STDMETHODIMP_(ULONG) AddRef();
  STDMETHODIMP_(ULONG) Release();

  STDMETHODIMP Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId);
  STDMETHODIMP Deactivate();
  STDMETHODIMP ActivateEx(ITfThreadMgr* pThreadMgr,
                           TfClientId tfClientId,
                           DWORD dwFlags);

  STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr* pDocMgr);
  STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr* pDocMgr);
  STDMETHODIMP OnSetFocus(ITfDocumentMgr* pDocMgrFocus,
                          ITfDocumentMgr* pDocMgrPrevFocus);
  STDMETHODIMP OnPushContext(ITfContext* pContext);
  STDMETHODIMP OnPopContext(ITfContext* pContext);

  STDMETHODIMP OnEndEdit(ITfContext* pic,
                         TfEditCookie ecReadOnly,
                         ITfEditRecord* pEditRecord);

  STDMETHODIMP OnLayoutChange(ITfContext* pContext,
                              TfLayoutCode lcode,
                              ITfContextView* pContextView);

  STDMETHODIMP OnSetFocus(BOOL fForeground);
  STDMETHODIMP OnTestKeyDown(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
  STDMETHODIMP OnKeyDown(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
  STDMETHODIMP OnTestKeyUp(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
  STDMETHODIMP OnKeyUp(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
  STDMETHODIMP OnPreservedKey(ITfContext* pContext, REFGUID rguid, BOOL* pfEaten);

  STDMETHODIMP OnSetThreadFocus();
  STDMETHODIMP OnKillThreadFocus();

  STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition* pComposition);
  STDMETHODIMP DoEditSession(TfEditCookie ec);

  STDMETHODIMP OnActivated(REFCLSID clsid, REFGUID guidProfile, BOOL isActivated);

  BOOL _IsKeyboardDisabled();
  BOOL _IsKeyboardOpen();
  HRESULT _SetKeyboardOpen(BOOL fOpen);
  void _AbortComposition(bool clear = true);

 private:
  BOOL _InitThreadMgrEventSink();
  void _UninitThreadMgrEventSink();
  BOOL _InitThreadFocusSink();
  void _UninitThreadFocusSink();
  DWORD _dwThreadFocusSinkCookie;

  BOOL _InitTextEditSink(com_ptr<ITfDocumentMgr> pDocMgr);

  BOOL _InitKeyEventSink();
  void _UninitKeyEventSink();
  void _ProcessKeyEvent(WPARAM wParam, LPARAM lParam, BOOL* pfEaten);

  BOOL _InitCompartment();
  void _UninitCompartment();
  HRESULT _HandleCompartment(REFGUID guidCompartment);

  com_ptr<CCompartmentEventSink> _pKeyboardCompartmentSink;

  com_ptr<ITfThreadMgr> _pThreadMgr;
  TfClientId _tfClientId;
  DWORD _dwThreadMgrEventSinkCookie;

  com_ptr<ITfContext> _pTextEditSinkContext;
  DWORD _dwTextEditSinkCookie, _dwTextLayoutSinkCookie;
  BOOL _fTestKeyDownPending, _fTestKeyUpPending;

  LONG _cRef;

  void _InitEngine();
  void _ShutdownEngine();
  std::wstring _dllDir;
  bool _engineReady = false;
  ProtoIME::NinePatchSkin _skin;
  ProtoIME::NinePatchSkin _settingsSkin;
  ProtoIME::NinePatchSkin _btnSkin;
};

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <mmsystem.h>
#include <objbase.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/constants.h"
#include "core/logger.h"
#include "core/types.h"
#include "win_platform.h"

namespace adhan {
namespace {

void format_hr(char* buf, size_t n, const char* op, HRESULT hr) {
  std::snprintf(buf, n, "%s hr=0x%08lX", op, static_cast<unsigned long>(hr));
}

class WasapiVolume : public VolumeController {
 public:
  explicit WasapiVolume(Logger* log)
      : log_(log),
        enumerator_(0),
        device_(0),
        volume_(0),
        notify_(this),
        notify_registered_(false),
        com_inited_here_(false),
        com_checked_(false),
        stale_(0),
        last_hr_(S_OK) {
    last_err_[0] = 0;
    std::strcpy(backend_, "none");
  }
  ~WasapiVolume() { shutdown(); }

  bool get_master_volume(float* out) override {
    if (!out) return false;
    if (ensure_wasapi(false) && volume_) {
      HRESULT hr = volume_->GetMasterVolumeLevelScalar(out);
      if (SUCCEEDED(hr)) {
        std::strcpy(backend_, "wasapi");
        return true;
      }
      set_err("IAudioEndpointVolume::GetMasterVolumeLevelScalar", hr);
      mark_stale();
      if (ensure_wasapi(true) && volume_) {
        hr = volume_->GetMasterVolumeLevelScalar(out);
        if (SUCCEEDED(hr)) {
          std::strcpy(backend_, "wasapi");
          return true;
        }
        set_err("IAudioEndpointVolume::GetMasterVolumeLevelScalar(retry)", hr);
      }
    }
    if (mixer_get(out)) {
      std::strcpy(backend_, "winmm-mixer");
      last_err_[0] = 0;
      return true;
    }
    if (last_err_[0] == 0) std::strcpy(last_err_, "get_master_volume: WASAPI and mixer failed");
    return false;
  }

  bool set_master_volume(float volume) override {
    if (volume < 0) volume = 0;
    if (volume > 1) volume = 1;
    if (ensure_wasapi(false) && volume_) {
      if (wasapi_set_verify(volume)) {
        std::strcpy(backend_, "wasapi");
        return true;
      }
      mark_stale();
      if (ensure_wasapi(true) && volume_ && wasapi_set_verify(volume)) {
        std::strcpy(backend_, "wasapi");
        return true;
      }
    }
    if (mixer_set(volume)) {
      std::strcpy(backend_, "winmm-mixer");
      float got = -1.0f;
      if (mixer_get(&got) && std::fabs(got - volume) > kVolumeVerifyEpsilon) {
        std::snprintf(last_err_, sizeof(last_err_), "mixer verify mismatch want=%.3f got=%.3f",
                      volume, got);
        return false;
      }
      last_err_[0] = 0;
      return true;
    }
    if (last_err_[0] == 0) std::strcpy(last_err_, "set_master_volume: WASAPI and mixer failed");
    return false;
  }

  bool get_mute(bool* muted) override {
    if (!muted) return false;
    *muted = false;
    if (!ensure_wasapi(false) || !volume_) return true;
    BOOL m = FALSE;
    HRESULT hr = volume_->GetMute(&m);
    if (FAILED(hr)) {
      set_err("IAudioEndpointVolume::GetMute", hr);
      return false;
    }
    *muted = m ? true : false;
    return true;
  }

  bool set_mute(bool muted) override {
    if (!ensure_wasapi(false) || !volume_) return false;
    HRESULT hr = volume_->SetMute(muted ? TRUE : FALSE, NULL);
    if (FAILED(hr)) {
      set_err("IAudioEndpointVolume::SetMute", hr);
      return false;
    }
    return true;
  }

  void refresh_endpoint() override { mark_stale(); }

  void mark_stale() { InterlockedExchange(&stale_, 1); }

  const char* backend_name() const override { return backend_; }
  const char* last_error() const override { return last_err_; }

 private:
  class EndpointNotify : public IMMNotificationClient {
   public:
    explicit EndpointNotify(WasapiVolume* owner) : ref_(1), owner_(owner) {}
    ULONG STDMETHODCALLTYPE AddRef() { return static_cast<ULONG>(InterlockedIncrement(&ref_)); }
    ULONG STDMETHODCALLTYPE Release() { return static_cast<ULONG>(InterlockedDecrement(&ref_)); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) {
      if (!ppv) return E_POINTER;
      if (riid == IID_IUnknown || riid == __uuidof(IMMNotificationClient)) {
        *ppv = static_cast<IMMNotificationClient*>(this);
        AddRef();
        return S_OK;
      }
      *ppv = NULL;
      return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) {
      if (owner_) owner_->mark_stale();
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) {
      if (owner_) owner_->mark_stale();
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole, LPCWSTR) {
      if (flow == eRender && owner_) {
        owner_->mark_stale();
        if (owner_->log_) owner_->log_->info("Default audio render endpoint changed; will rebind");
      }
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) { return S_OK; }

   private:
    LONG ref_;
    WasapiVolume* owner_;
  };

  Logger* log_;
  IMMDeviceEnumerator* enumerator_;
  IMMDevice* device_;
  IAudioEndpointVolume* volume_;
  EndpointNotify notify_;
  bool notify_registered_;
  bool com_inited_here_;
  bool com_checked_;
  volatile LONG stale_;
  HRESULT last_hr_;
  char last_err_[192];
  char backend_[32];

  void set_err(const char* op, HRESULT hr) {
    last_hr_ = hr;
    format_hr(last_err_, sizeof(last_err_), op, hr);
    if (log_) log_->warn(last_err_);
  }

  void ensure_com() {
    if (com_checked_) return;
    com_checked_ = true;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (hr == S_OK || hr == S_FALSE) {
      com_inited_here_ = true;
    }
  }

  void release_endpoint() {
    if (volume_) {
      volume_->Release();
      volume_ = 0;
    }
    if (device_) {
      device_->Release();
      device_ = 0;
    }
  }

  void shutdown() {
    if (enumerator_ && notify_registered_) {
      enumerator_->UnregisterEndpointNotificationCallback(&notify_);
      notify_registered_ = false;
    }
    release_endpoint();
    if (enumerator_) {
      enumerator_->Release();
      enumerator_ = 0;
    }
    if (com_inited_here_) {
      CoUninitialize();
      com_inited_here_ = false;
    }
  }

  bool ensure_enumerator() {
    ensure_com();
    if (enumerator_) return true;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), (void**)&enumerator_);
    if (FAILED(hr) || !enumerator_) {
      set_err("CoCreateInstance(MMDeviceEnumerator)", hr);
      enumerator_ = 0;
      return false;
    }
    HRESULT nr = enumerator_->RegisterEndpointNotificationCallback(&notify_);
    notify_registered_ = SUCCEEDED(nr);
    if (FAILED(nr) && log_) {
      char buf[96];
      format_hr(buf, sizeof(buf), "RegisterEndpointNotificationCallback", nr);
      log_->warn(buf);
    }
    return true;
  }

  bool ensure_wasapi(bool force) {
    if (InterlockedCompareExchange(&stale_, 0, 1) == 1) force = true;
    if (force) release_endpoint();
    if (volume_) return true;
    if (!ensure_enumerator()) return false;

    HRESULT hr = enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &device_);
    if (FAILED(hr) || !device_) {
      if (device_) {
        device_->Release();
        device_ = 0;
      }
      hr = enumerator_->GetDefaultAudioEndpoint(eRender, eMultimedia, &device_);
    }
    if (FAILED(hr) || !device_) {
      set_err("IMMDeviceEnumerator::GetDefaultAudioEndpoint", hr);
      device_ = 0;
      return false;
    }
    hr = device_->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&volume_);
    if (FAILED(hr) || !volume_) {
      set_err("IMMDevice::Activate(IAudioEndpointVolume)", hr);
      volume_ = 0;
      release_endpoint();
      return false;
    }
    std::strcpy(backend_, "wasapi");
    last_err_[0] = 0;
    return true;
  }

  bool wasapi_set_verify(float volume) {
    HRESULT hr = volume_->SetMasterVolumeLevelScalar(volume, NULL);
    if (FAILED(hr)) {
      set_err("IAudioEndpointVolume::SetMasterVolumeLevelScalar", hr);
      return false;
    }
    float got = -1.0f;
    hr = volume_->GetMasterVolumeLevelScalar(&got);
    if (FAILED(hr)) {
      set_err("IAudioEndpointVolume::GetMasterVolumeLevelScalar(verify)", hr);
      return false;
    }
    if (std::fabs(got - volume) > kVolumeVerifyEpsilon) {
      std::snprintf(last_err_, sizeof(last_err_),
                    "WASAPI verify mismatch want=%.3f got=%.3f", volume, got);
      if (log_) log_->warn(last_err_);
      return false;
    }
    last_err_[0] = 0;
    return true;
  }

  static bool mixer_get(float* volume) {
    HMIXER mixer = 0;
    if (mixerOpen(&mixer, 0, 0, 0, MIXER_OBJECTF_MIXER) != MMSYSERR_NOERROR) return false;
    MIXERLINE line;
    ZeroMemory(&line, sizeof(line));
    line.cbStruct = sizeof(line);
    line.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
    if (mixerGetLineInfo(reinterpret_cast<HMIXEROBJ>(mixer), &line, MIXER_GETLINEINFOF_COMPONENTTYPE) !=
        MMSYSERR_NOERROR) {
      mixerClose(mixer);
      return false;
    }
    MIXERLINECONTROLS ctrls;
    MIXERCONTROL ctrl;
    ZeroMemory(&ctrls, sizeof(ctrls));
    ZeroMemory(&ctrl, sizeof(ctrl));
    ctrls.cbStruct = sizeof(ctrls);
    ctrls.dwLineID = line.dwLineID;
    ctrls.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
    ctrls.cControls = 1;
    ctrls.cbmxctrl = sizeof(ctrl);
    ctrls.pamxctrl = &ctrl;
    if (mixerGetLineControls(reinterpret_cast<HMIXEROBJ>(mixer), &ctrls,
                             MIXER_GETLINECONTROLSF_ONEBYTYPE) != MMSYSERR_NOERROR) {
      mixerClose(mixer);
      return false;
    }
    MIXERCONTROLDETAILS details;
    MIXERCONTROLDETAILS_UNSIGNED value;
    ZeroMemory(&details, sizeof(details));
    ZeroMemory(&value, sizeof(value));
    details.cbStruct = sizeof(details);
    details.dwControlID = ctrl.dwControlID;
    details.cChannels = 1;
    details.cbDetails = sizeof(value);
    details.paDetails = &value;
    MMRESULT mr = mixerGetControlDetails(reinterpret_cast<HMIXEROBJ>(mixer), &details,
                                         MIXER_GETCONTROLDETAILSF_VALUE);
    mixerClose(mixer);
    if (mr != MMSYSERR_NOERROR) return false;
    DWORD maxv = ctrl.Bounds.dwMaximum ? ctrl.Bounds.dwMaximum : 65535;
    *volume = static_cast<float>(value.dwValue) / static_cast<float>(maxv);
    return true;
  }

  static bool mixer_set(float volume) {
    HMIXER mixer = 0;
    if (mixerOpen(&mixer, 0, 0, 0, MIXER_OBJECTF_MIXER) != MMSYSERR_NOERROR) return false;
    MIXERLINE line;
    ZeroMemory(&line, sizeof(line));
    line.cbStruct = sizeof(line);
    line.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
    if (mixerGetLineInfo(reinterpret_cast<HMIXEROBJ>(mixer), &line, MIXER_GETLINEINFOF_COMPONENTTYPE) !=
        MMSYSERR_NOERROR) {
      mixerClose(mixer);
      return false;
    }
    MIXERLINECONTROLS ctrls;
    MIXERCONTROL ctrl;
    ZeroMemory(&ctrls, sizeof(ctrls));
    ZeroMemory(&ctrl, sizeof(ctrl));
    ctrls.cbStruct = sizeof(ctrls);
    ctrls.dwLineID = line.dwLineID;
    ctrls.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
    ctrls.cControls = 1;
    ctrls.cbmxctrl = sizeof(ctrl);
    ctrls.pamxctrl = &ctrl;
    if (mixerGetLineControls(reinterpret_cast<HMIXEROBJ>(mixer), &ctrls,
                             MIXER_GETLINECONTROLSF_ONEBYTYPE) != MMSYSERR_NOERROR) {
      mixerClose(mixer);
      return false;
    }
    MIXERCONTROLDETAILS details;
    MIXERCONTROLDETAILS_UNSIGNED value;
    ZeroMemory(&details, sizeof(details));
    ZeroMemory(&value, sizeof(value));
    details.cbStruct = sizeof(details);
    details.dwControlID = ctrl.dwControlID;
    details.cChannels = 1;
    details.cbDetails = sizeof(value);
    details.paDetails = &value;
    DWORD maxv = ctrl.Bounds.dwMaximum ? ctrl.Bounds.dwMaximum : 65535;
    value.dwValue = static_cast<DWORD>(volume * maxv + 0.5f);
    MMRESULT mr = mixerSetControlDetails(reinterpret_cast<HMIXEROBJ>(mixer), &details,
                                         MIXER_SETCONTROLDETAILSF_VALUE);
    mixerClose(mixer);
    return mr == MMSYSERR_NOERROR;
  }
};

}  // namespace

VolumeController* create_win_volume_controller(Logger* log) { return new WasapiVolume(log); }

}  // namespace adhan

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <mmsystem.h>

#include "core/types.h"

namespace adhan {
namespace {

class WasapiVolume : public VolumeController {
 public:
  WasapiVolume() : enumerator_(0), device_(0), volume_(0), inited_(false) {}
  ~WasapiVolume() { release(); }

  bool get_master_volume(float* volume) override {
    if (!ensure() || !volume_) return mixer_get(volume);
    HRESULT hr = volume_->GetMasterVolumeLevelScalar(volume);
    return SUCCEEDED(hr);
  }
  bool set_master_volume(float volume) override {
    if (volume < 0) volume = 0;
    if (volume > 1) volume = 1;
    if (!ensure() || !volume_) return mixer_set(volume);
    HRESULT hr = volume_->SetMasterVolumeLevelScalar(volume, NULL);
    return SUCCEEDED(hr);
  }
  bool get_mute(bool* muted) override {
    if (!ensure() || !volume_) {
      *muted = false;
      return true;
    }
    BOOL m = FALSE;
    HRESULT hr = volume_->GetMute(&m);
    if (FAILED(hr)) return false;
    *muted = m ? true : false;
    return true;
  }

 private:
  IMMDeviceEnumerator* enumerator_;
  IMMDevice* device_;
  IAudioEndpointVolume* volume_;
  bool inited_;

  void release() {
    if (volume_) {
      volume_->Release();
      volume_ = 0;
    }
    if (device_) {
      device_->Release();
      device_ = 0;
    }
    if (enumerator_) {
      enumerator_->Release();
      enumerator_ = 0;
    }
  }

  bool ensure() {
    if (volume_) return true;
    if (inited_ && !volume_) return false;
    inited_ = true;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), (void**)&enumerator_);
    if (FAILED(hr) || !enumerator_) return false;
    hr = enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &device_);
    if (FAILED(hr) || !device_) return false;
    hr = device_->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&volume_);
    return SUCCEEDED(hr) && volume_;
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

VolumeController* create_win_volume_controller() { return new WasapiVolume(); }

}  // namespace adhan

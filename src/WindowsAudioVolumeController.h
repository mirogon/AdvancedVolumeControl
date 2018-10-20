#pragma once
#include "stdlib.h"
#include <iostream>
#include <windows.h>
#include <stdio.h>
#include <sstream>
#include <map>
#include <utility>
#include <vector>
#include <mmdeviceapi.h>
#include <appmodel.h>
#include <memory>
#include <ShlObj.h>
#include <propkey.h>
#include <PathCch.h>
#include <atlbase.h>
#include <atlstr.h>
#include <strsafe.h>
#include <Psapi.h>
#include <Audioclient.h>
#include <Audiopolicy.h>

using namespace std;

extern const CLSID CLSID_MMDeviceEnumerator;
extern const IID IID_IMMDeviceEnumerator;

#define SAFE_RELEASE(x) if(x != nullptr){x->Release(); x = nullptr;}

#define ERROR_HANDLE(x) if(FAILED(x)){ HRESULT hr = x; std::stringstream s; s << std::hex << hr; std::cout<<"HRESULT CODE: "<<s.str(); }

struct S_AudioSession
{
	std::string displayName;
	float currentVolumeLevel;
	bool isMuted;
    unsigned int originalSessionIndex;
	DWORD processID;
	GUID groupingID;
};

class C_WindowsAudioVolumeController
{
public:

	C_WindowsAudioVolumeController();
	~C_WindowsAudioVolumeController();

	void Initialize();
	void UpdateAudioSessions();
	void SetVolume(uint8_t sessionIndex, const float& newVolume);
	void SetMute(uint8_t sessionIndex, bool newMuteState);
    void GetAudioSessions(std::vector<S_AudioSession>& audioSessionsP);

private:

	HRESULT hResult;

	int numAudioSessions;

	IMMDevice* defaultAudioEndpointDevice;
	IMMDeviceEnumerator* deviceEnumerator;

	IAudioSessionManager* audioSessionManager;
	IAudioSessionManager2* audioSessionManager2;
	
	IAudioSessionEnumerator* audioSessionEnumerator;

	std::vector<IAudioSessionControl*> audioSessionControls;
	std::vector<IAudioSessionControl2*> audioSession2Controls;
	std::vector<ISimpleAudioVolume*> audioSessionVolumes;

	std::vector<S_AudioSession> audioSessions;
    bool similarSessions[128][128];

	//FUNCTIONS

	std::string GetSessionExecutableName(DWORD PID);

	std::string GetAppnameFromPath(const std::string& path);

    void InitSimilarSessions();

};

 inline void C_WindowsAudioVolumeController::GetAudioSessions(std::vector<S_AudioSession>& audioSessionsP)
 {
    audioSessionsP = audioSessions;
 }

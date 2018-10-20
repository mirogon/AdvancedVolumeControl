#include "WindowsAudioVolumeController.h"

extern const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
extern const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

C_WindowsAudioVolumeController::C_WindowsAudioVolumeController()
{
	hResult = S_OK;
	numAudioSessions = 0;
	defaultAudioEndpointDevice = nullptr;
	deviceEnumerator = nullptr;
	audioSessionManager = nullptr;
	audioSessionManager2 = nullptr;
	audioSessionEnumerator = nullptr;

	audioSessionControls = *new std::vector<IAudioSessionControl*>();
	audioSession2Controls = *new std::vector<IAudioSessionControl2*>();
	audioSessionVolumes = *new std::vector<ISimpleAudioVolume*>();
	audioSessions = *new std::vector<S_AudioSession>();

}

C_WindowsAudioVolumeController::~C_WindowsAudioVolumeController()
{
	SAFE_RELEASE(defaultAudioEndpointDevice);
	SAFE_RELEASE(deviceEnumerator);
	SAFE_RELEASE(audioSessionManager);
	SAFE_RELEASE(audioSessionManager2);
	SAFE_RELEASE(audioSessionEnumerator);

	for (int i = 0; i < audioSessionControls.size(); ++i)
	{
		SAFE_RELEASE(audioSessionControls.at(i));
	}

	audioSessionControls.clear();

	for (int i = 0; i < audioSession2Controls.size(); ++i)
	{
		SAFE_RELEASE(audioSession2Controls.at(i));
	}

	audioSession2Controls.clear();

	for (int i = 0; i < audioSessionVolumes.size(); ++i)
	{
		SAFE_RELEASE(audioSessionVolumes.at(i));
	}

	audioSessionVolumes.clear();

	audioSessions.clear();

}

void C_WindowsAudioVolumeController::Initialize()
{
	CoInitialize(nullptr);

	//Initialize deviceEnumerator
	hResult = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&deviceEnumerator);
	ERROR_HANDLE(hResult);
	
	//Initialize defaultAudioEnpointDevice
	hResult = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultAudioEndpointDevice);
	ERROR_HANDLE(hResult);

	//Initialize the audioSessionManager
	hResult = defaultAudioEndpointDevice->Activate(__uuidof(IAudioSessionManager), CLSCTX_ALL, NULL, (void**)&audioSessionManager);
	ERROR_HANDLE(hResult);

	//Initialize the audioSessionManager2
	hResult = defaultAudioEndpointDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**)&audioSessionManager2);
	ERROR_HANDLE(hResult);

	//Initialize the audioSessionEnumerator
	hResult = audioSessionManager2->GetSessionEnumerator(&audioSessionEnumerator);
	ERROR_HANDLE(hResult);

	//Get the count of audio sessions and store them in numAudioSessions
	hResult = audioSessionEnumerator->GetCount(&numAudioSessions);

	//cout num of sessions
	std::cout << "SessionNum: " << numAudioSessions << std::endl;

	//Intialize a IAudioSessionControl and IAudioSessionControl2 for every audio session
	for (int i = 0; i < numAudioSessions; ++i)
	{
		audioSessionControls.push_back(nullptr);
		hResult = audioSessionEnumerator->GetSession(i, &audioSessionControls.at(i));
		ERROR_HANDLE(hResult);

		audioSession2Controls.push_back(nullptr);
		hResult = audioSessionControls.at(i)->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&audioSession2Controls.at(i));
		ERROR_HANDLE(hResult);

	}

	//Create and intialize the audio sessions
	for (int i = 0; i < numAudioSessions; ++i)
	{
		static DWORD cachePID;
		static GUID cacheGUID; 
		static std::string cacheExecutableName;
		static float cacheVolumeLevel;
		static BOOL cacheIsMuted;

		hResult = audioSession2Controls.at(i)->GetProcessId(&cachePID);
		ERROR_HANDLE(hResult);

		hResult = audioSessionControls.at(i)->GetGroupingParam(&cacheGUID);
		ERROR_HANDLE(hResult);

		audioSessionVolumes.push_back(nullptr);
		hResult = audioSessionControls.at(i)->QueryInterface(&audioSessionVolumes.at(i));
		ERROR_HANDLE(hResult);

		cacheExecutableName = GetSessionExecutableName(cachePID);

        if(audioSession2Controls.at(i)->IsSystemSoundsSession() == S_OK)
        {
            cacheExecutableName = "\\SystemSounds";
        }
		
		hResult = audioSessionVolumes.at(i)->GetMasterVolume(&cacheVolumeLevel);
		ERROR_HANDLE(hResult);
	
		hResult = audioSessionVolumes.at(i)->GetMute(&cacheIsMuted);
		ERROR_HANDLE(hResult);

        audioSessions.push_back(S_AudioSession());
        audioSessions.at(i).processID = cachePID;
        audioSessions.at(i).groupingID = cacheGUID;
        audioSessions.at(i).displayName = GetAppnameFromPath(cacheExecutableName);
        audioSessions.at(i).currentVolumeLevel = cacheVolumeLevel;
        audioSessions.at(i).isMuted = cacheIsMuted;
        audioSessions.at(i).originalSessionIndex = i;

	}

    InitSimilarSessions();

	for (int i = 0; i < audioSessions.size(); ++i)
	{
		std::cout << "Session name: " << audioSessions.at(i).displayName << std::endl;
		std::cout << "Current volume: " << audioSessions.at(i).currentVolumeLevel * 100 << "%" << std::endl;
		std::cout << "Muted: " << audioSessions.at(i).isMuted << std::endl;
	}

}

void C_WindowsAudioVolumeController::UpdateAudioSessions()
{
	static HRESULT hResult;

	for (int i = 0; i < audioSessionControls.size(); ++i)
	{
		SAFE_RELEASE(audioSessionControls.at(i));
	}

	audioSessionControls.clear();
	audioSessionControls.shrink_to_fit();

	for (int i = 0; i < audioSession2Controls.size(); ++i)
	{
		SAFE_RELEASE(audioSession2Controls.at(i));
	}

	audioSession2Controls.clear();
	audioSession2Controls.shrink_to_fit();

	for (int i = 0; i < audioSessionVolumes.size(); ++i)
	{
		SAFE_RELEASE(audioSessionVolumes.at(i));
	}

	audioSessionVolumes.clear();
	audioSessionVolumes.shrink_to_fit();

	audioSessions.clear();
	audioSessions.shrink_to_fit();

	numAudioSessions = 0;
	
	SAFE_RELEASE(audioSessionEnumerator);

	while (audioSessionEnumerator == nullptr)
	{
		static short tries = 0;

		hResult = audioSessionManager2->GetSessionEnumerator(&audioSessionEnumerator);
		ERROR_HANDLE(hResult)

		++tries;
		if (tries >= 10 && audioSessionEnumerator == nullptr)
		{
			std::cout << "10 Tries of initializing the session enumerator failed!!" << std::endl;
			return;
		}
	}

	hResult = audioSessionEnumerator->GetCount(&numAudioSessions);
    ERROR_HANDLE(hResult);

    //std::cout<<"numAudioSessions: "<<numAudioSessions<<std::endl;

	//Intialize a IAudioSessionControl and IAudioSessionControl2 for every audio session
	for (int i = 0; i < numAudioSessions; ++i)
	{
		audioSessionControls.push_back(nullptr);
		hResult = audioSessionEnumerator->GetSession(i, &audioSessionControls.at(i));
		ERROR_HANDLE(hResult);

		audioSession2Controls.push_back(nullptr);
		hResult = audioSessionControls.at(i)->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&audioSession2Controls.at(i));
		ERROR_HANDLE(hResult);

	}

	//Create and intialize the audio sessions
	for (int i = 0; i < numAudioSessions; ++i)
	{
		static DWORD cachePID;
		static GUID cacheGUID;
		static std::string cacheExecutableName;
		static float cacheVolumeLevel;
		static BOOL cacheIsMuted;

        hResult = audioSession2Controls.at(i)->GetProcessId(&cachePID);
        ERROR_HANDLE(hResult);

        hResult = audioSessionControls.at(i)->GetGroupingParam(&cacheGUID);
        ERROR_HANDLE(hResult);

        audioSessionVolumes.push_back(nullptr);
        hResult = audioSessionControls.at(i)->QueryInterface(&audioSessionVolumes.at(i));
        ERROR_HANDLE(hResult);

        cacheExecutableName = GetSessionExecutableName(cachePID);

        if(audioSession2Controls.at(i)->IsSystemSoundsSession() == S_OK)
        {
            cacheExecutableName = "\\SystemSounds";
        }

        hResult = audioSessionVolumes.at(i)->GetMasterVolume(&cacheVolumeLevel);
        ERROR_HANDLE(hResult);

        hResult = audioSessionVolumes.at(i)->GetMute(&cacheIsMuted);
        ERROR_HANDLE(hResult);

        audioSessions.push_back(S_AudioSession());
        audioSessions.at(i).processID = cachePID;
        audioSessions.at(i).groupingID = cacheGUID;
        audioSessions.at(i).displayName = GetAppnameFromPath(cacheExecutableName);
        audioSessions.at(i).currentVolumeLevel = cacheVolumeLevel;
        audioSessions.at(i).isMuted = cacheIsMuted;
        audioSessions.at(i).originalSessionIndex = i;

	}

    InitSimilarSessions();


}

void C_WindowsAudioVolumeController::SetVolume(uint8_t sessionIndex,const float& newVolume)
{
    if (sessionIndex >= audioSessions.size())
    {
        return;
    }

    unsigned int originalSessionIndex = audioSessions.at(sessionIndex).originalSessionIndex;

    audioSessionVolumes.at(originalSessionIndex)->SetMasterVolume(newVolume, NULL);
	audioSessions.at(sessionIndex).currentVolumeLevel = newVolume;

    for(int i = 0; i < 128;++i)
    {

       if( similarSessions[originalSessionIndex][i] == true)
       {
            audioSessionVolumes.at(i)->SetMasterVolume(newVolume, NULL);
       }

    }

}

void C_WindowsAudioVolumeController::SetMute(uint8_t sessionIndex, bool newMuteState)
{

    if (sessionIndex >= audioSessions.size())
    {
        return;
    }

    unsigned int originalSessionIndex = audioSessions.at(sessionIndex).originalSessionIndex;

    audioSessionVolumes.at(originalSessionIndex)->SetMute(newMuteState, NULL);
	audioSessions.at(sessionIndex).isMuted = newMuteState;

    for(int i = 0; i < 128;++i)
    {

       if( similarSessions[originalSessionIndex][i] == true)
       {
            audioSessionVolumes.at(i)->SetMute(newMuteState, NULL);
       }

    }

}

std::string C_WindowsAudioVolumeController::GetAppnameFromPath(const std::string& path)
{
    if(path.size() <= 0)
    {
        return std::string("");
    }

    static std::string appName;
    static int nameStartIndex;
    appName.clear();
    appName.shrink_to_fit();
    nameStartIndex = -1;

    for(int i = path.size() - 1; i >= 0; --i)
    {
        if(path.at(i) == '\\' || path.at(i) == '/')
        {
            nameStartIndex = i+1;
            break;
        }
    }

    if(nameStartIndex == -1)
    {
        return std::string("");
    }

    for(int i = nameStartIndex; i < path.size() ; ++i)
    {
        appName.push_back(path.at(i));
    }

    return appName;

}

std::string C_WindowsAudioVolumeController::GetSessionExecutableName(DWORD PID)
{
	
	DWORD maxPath = 128;
    wchar_t name[128];

	HANDLE processHandle(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, PID));
	if (processHandle != NULL)
	{
        QueryFullProcessImageNameW(processHandle, 0, name, &maxPath);
	}
	
	CloseHandle(processHandle);

    std::wstring ws(name);
    std::string s(ws.begin(), ws.end());
    return s;

}

void C_WindowsAudioVolumeController::InitSimilarSessions()
{
    for(int i = 0; i < 128; ++i)
    {
        for(int o = 0; o < 128; ++o)
        {
            similarSessions[i][o] = false;
        }
    }

    //Initialize similar session list and delte unnecessary audioSessions
    const size_t shouldDeleteSize = (const size_t)(audioSessions.size());
    std::vector<bool> shouldDelete;
    shouldDelete.resize(audioSessions.size());

    for(int b = 0; b < audioSessions.size(); b++)
    {
        shouldDelete[b] = false;
    }

    for(int i = 0; i < audioSessions.size(); ++i)
    {

        if(shouldDelete[i] == true)
        {
            continue;
        }

        for(int i2 = 0; i2 < audioSessions.size();++i2)
        {
            if(i != i2)
            {
                if(audioSessions.at(i).displayName == audioSessions.at(i2).displayName)
                {
                    similarSessions[i][i2] = true;
                    shouldDelete[i2] = true;
                }
            }
        }
    }

    static std::vector<S_AudioSession> audioSessions_NEW;
    audioSessions_NEW.clear();
    audioSessions_NEW.shrink_to_fit();

    for(int i = 0; i < audioSessions.size(); i++)
    {
        if(shouldDelete[i] == false)
        {
            audioSessions_NEW.push_back(audioSessions.at(i));
        }
    }

    audioSessions.clear();
    audioSessions.shrink_to_fit();
    audioSessions = audioSessions_NEW;

}

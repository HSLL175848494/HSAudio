#include "HSAudio.h"

// 初始化静态成员
BOOL HSLL::HSAudio::Initialized = false;
DWORD HSLL::HSAudio::Ref = 0;
IXAudio2* HSLL::HSAudio::pXAudio2 = nullptr;
IXAudio2MasteringVoice* HSLL::HSAudio::pMasterVoice = nullptr;


//初始化com组件
BOOL HSLL::HSAudio::CoInitialize()
{
	if (!Initialized)
	{
		Initialized = true;
		if (FAILED(::CoInitializeEx(NULL, COINIT_MULTITHREADED)))
			return false;
		else
			return true;
	}
	return false;
}


//卸载com组件
void  HSLL::HSAudio::CoUninitialize()
{
	if (Initialized)
	::CoUninitialize();
}


//初始化
BOOL HSLL::HSAudio::Initialize()
{
	if (!Ref)//初次创建
	{
		if (FAILED(XAudio2Create(&pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR)))
			return false;

		if (FAILED(pXAudio2->CreateMasteringVoice(&pMasterVoice)))
			return false;

		Ref++;
	}

	return true;
}


BOOL HSLL::HSAudio::CreateAudio(BYTE* Data, DWORD Size, BOOL Loop, AudioContoller* pController)
{
	return pController->AudioContollerInit(Data, Size, Loop);
}


BOOL HSLL::HSAudio::CreateAudio(LPCSTR FileName, DWORD Offset, DWORD Size, BOOL Loop, AudioContoller* pController)
{
	HANDLE Handle = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (Handle == INVALID_HANDLE_VALUE)
		return false;
	else
		return pController->AudioContollerInit(Handle, Offset, Size, Loop);
}


BOOL HSLL::HSAudio::CreateAudio(LPCSTR FileName, BOOL Loop, AudioContoller* pController)
{
	HANDLE Handle = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (Handle == INVALID_HANDLE_VALUE)
		return false;
	else
		return pController->AudioContollerInit(Handle, 0, GetFileSize(Handle, 0), Loop);
}


BOOL HSLL::HSAudio::CreateAudio(WAVEFORMATEXTENSIBLE Format, AudioCustom* pDecoder, AudioContoller* pController)
{
	return pController->AudioContollerInit(Format, pDecoder);
}



BOOL HSLL::AudioContoller::SetVolume(float Volume)//设置音量
{
	if (Volume < 0 || Volume >= 2)
		return false;

	return !FAILED(pSourceVoice->SetVolume(Volume));
}


void HSLL::AudioContoller::ClearUp()//清理资源
{
	for (DWORD i = 0; i < HSAUDIO_BUFFERSIZE; i++)
	{
		if (Buffers[i].pContext)
		{
			if(PlayType==AudioPlayType_Custom)
			pCustom->FreeBuffer((BYTE*) Buffers[i].pAudioData);
			else
			delete[] Buffers[i].pAudioData;

			Buffers[i].pContext = nullptr;
		}
	}
}

DWORD GetDefaultChannelMask(ma_uint32 channelCount)
{
	switch (channelCount)
	{
	case 1: return SPEAKER_FRONT_CENTER;
	case 2: return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
	case 3: return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER;
	case 4: return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
	case 5: return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
	case 6: return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
	case 8: return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT | SPEAKER_FRONT_LEFT_OF_CENTER | SPEAKER_FRONT_RIGHT_OF_CENTER;
	default: return 0;
	}
}

BOOL HSLL::AudioContoller::InitDecoder() // 初始化解码器
{
	ma_decoder_config config = ma_decoder_config_init(ma_format_unknown, 0, 0);
	ma_result result;

	if (PlayType == AudioPlayType_File)
		result = ma_decoder_init(on_read_f, on_seek_f, pPlayInfo, &config, &pPlayInfo->Decoder);
	else
		result = ma_decoder_init(on_read_b, on_seek_b, pPlayInfo, &config, &pPlayInfo->Decoder);

	if (result != MA_SUCCESS)
		return false;

	WaveFormat.Format.nChannels = pPlayInfo->Decoder.outputChannels;
	WaveFormat.Format.nSamplesPerSec = pPlayInfo->Decoder.outputSampleRate;
	WaveFormat.Format.wBitsPerSample = ma_get_bytes_per_sample(pPlayInfo->Decoder.outputFormat) * 8;
	WaveFormat.Format.nBlockAlign = (WaveFormat.Format.nChannels * WaveFormat.Format.wBitsPerSample) / 8;
	WaveFormat.Format.nAvgBytesPerSec = WaveFormat.Format.nSamplesPerSec * WaveFormat.Format.nBlockAlign;

	switch (pPlayInfo->Decoder.outputFormat)
	{
	case ma_format_u8:
	case ma_format_s16:
		WaveFormat.Format.wFormatTag = WAVE_FORMAT_PCM;
		WaveFormat.Format.cbSize = 0;
		break;
	case ma_format_s24:
	case ma_format_s32:
		WaveFormat.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
		WaveFormat.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
		WaveFormat.Samples.wValidBitsPerSample = WaveFormat.Format.wBitsPerSample;
		WaveFormat.dwChannelMask = GetDefaultChannelMask(WaveFormat.Format.nChannels);
		WaveFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
		break;
	case ma_format_f32:
		WaveFormat.Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
		WaveFormat.Format.cbSize = 0;
		break;
	default:
		return false;
	}

	ma_uint64 totalFrames;
    ma_decoder_get_length_in_pcm_frames(&pPlayInfo->Decoder, &totalFrames);
	pPlayInfo->Length = totalFrames/ pPlayInfo->Decoder.outputSampleRate;

	return true;
}


void HSLL::AudioContoller::SubmitBuffer()//提交buffer
{
	AudioBuffer Buffer;

	if (PlayType == AudioPlayType_Custom)
		Buffer = pCustom->GetBuffer();
	else
		Buffer = ReadBuffer();

	Buffers[NowIndex].AudioBytes = Buffer.Size;
	Buffers[NowIndex].pAudioData = Buffer.Data;
	Buffers[NowIndex].pContext = &Buffers[NowIndex];
	Buffers[NowIndex].Flags = 0;

	if (Buffer.Size < WaveFormat.Format.nBlockAlign * HSAUDIO_FRAMESREAD)
	{
		Buffers[NowIndex].Flags = XAUDIO2_END_OF_STREAM;
		pSourceVoice->SubmitSourceBuffer(&Buffers[NowIndex]);
	}
	else
	{
		pSourceVoice->SubmitSourceBuffer(&Buffers[NowIndex]);
		NowIndex = (NowIndex + 1) % HSAUDIO_BUFFERSIZE;
	}
}


void HSLL::AudioContoller::ReCreateSourceVoice(DWORD StartFrame)
{
	if (State != PlayState_End)
	{
		pSourceVoice->DestroyVoice();
		HSAudio::pXAudio2->CreateSourceVoice(&pSourceVoice, (WAVEFORMATEX*)&WaveFormat, 0, XAUDIO2_DEFAULT_FREQ_RATIO, this);
	}
	else
	State = PlayState_Suspended;

	ma_decoder_seek_to_pcm_frame(&pPlayInfo->Decoder, StartFrame);

	ClearUp();

	for (DWORD i = 0; i < HSAUDIO_BUFFERSIZE; i++)
		SubmitBuffer();
}


HSLL::AudioBuffer HSLL::AudioContoller::ReadBuffer()
{
	AudioBuffer Buffer; ma_uint64 FramesRead;
	ma_uint64 FramesCount = HSAUDIO_FRAMESREAD;
	ma_uint32 bytesPerFrame = WaveFormat.Format.nBlockAlign;
	Buffer.Size = FramesCount * bytesPerFrame;
	Buffer.Data = new BYTE[Buffer.Size];
	BYTE* pBuffer = Buffer.Data;

	while (FramesCount)
	{
		ma_decoder_read_pcm_frames(&pPlayInfo->Decoder, pBuffer, FramesCount, &FramesRead);

		FramesCount -= FramesRead;
		pBuffer += FramesRead * bytesPerFrame;

		if (FramesCount && pPlayInfo->Loop)
		{
			ma_decoder_seek_to_pcm_frame(&pPlayInfo->Decoder, 0);
			continue;
		}
		break;
	}

	Buffer.Size = (HSAUDIO_FRAMESREAD - FramesCount) * bytesPerFrame;
	return Buffer;
}


HSLL::AudioContoller::~AudioContoller()//析构
{
	DeleteCriticalSection(&CriticalSection);

	CloseHandle(hEndEvent);
}


HSLL::AudioContoller::AudioContoller() :NowIndex(0), State(PlayState_Default), pPlayInfo(nullptr), pSourceVoice(nullptr), pCustom(nullptr)
{
	InitializeCriticalSection(&CriticalSection);
	hEndEvent = CreateEventA(0, false, false, 0);
}


BOOL HSLL::AudioContoller::AudioContollerInit(BYTE* Data, DWORD TotalSize, BOOL Loop)//读取载入内存的音乐(格式要求:)
{
	pPlayInfo = new PlayInfo();
	pPlayInfo->Data = Data;
	pPlayInfo->Loop = Loop;
	pPlayInfo->TotalSize = TotalSize;
	PlayType = AudioPlayType_Buffer;

	return InitDecoder() && !FAILED(HSAudio::pXAudio2->CreateSourceVoice(&pSourceVoice, (WAVEFORMATEX*)&WaveFormat, 0, XAUDIO2_DEFAULT_FREQ_RATIO, this));
}


BOOL HSLL::AudioContoller::AudioContollerInit(HANDLE hFile, DWORD Offset, DWORD TotalSize, BOOL Loop)//读取文件中的音乐(格式要求:)
{
	pPlayInfo = new PlayInfo();
	pPlayInfo->hFile = hFile;
	pPlayInfo->Offset = Offset;
	pPlayInfo->Loop = Loop;
	pPlayInfo->TotalSize = TotalSize;
	PlayType = AudioPlayType_File;

	return InitDecoder() && !FAILED(HSAudio::pXAudio2->CreateSourceVoice(&pSourceVoice, (WAVEFORMATEX*)&WaveFormat, 0, XAUDIO2_DEFAULT_FREQ_RATIO, this));
}


BOOL HSLL::AudioContoller::AudioContollerInit(WAVEFORMATEXTENSIBLE Format, AudioCustom* pDecoder)//自定义读取
{
	this->WaveFormat = Format;
	this->pCustom = pDecoder;
	PlayType = AudioPlayType_Custom;

	return	!FAILED(HSAudio::pXAudio2->CreateSourceVoice(&pSourceVoice, (WAVEFORMATEX*)&Format, 0, XAUDIO2_DEFAULT_FREQ_RATIO, this));
}


void HSLL::AudioContoller::WaitForEnd()//等待播放结束
{
	EnterCriticalSection(&CriticalSection);

	if (State == PlayState_End)
		return;

	LeaveCriticalSection(&CriticalSection);

	WaitForSingleObject(hEndEvent, INFINITE);
}


void HSLL::AudioContoller::Start()
{
	EnterCriticalSection(&CriticalSection);

	if (State == PlayState_Default)
	{
		for (DWORD i = 0; i < HSAUDIO_BUFFERSIZE; i++)
			SubmitBuffer();
	}

	if (State != PlayState_End)
	{
		pSourceVoice->Start(0);
		State = PlayState_Playing;
	}

	LeaveCriticalSection(&CriticalSection);
}


void HSLL::AudioContoller::Stop()//停止播放
{
	EnterCriticalSection(&CriticalSection);

	if (State != PlayState_End)
	{
		pSourceVoice->Stop(0);
		State = PlayState_Suspended;
	}

	LeaveCriticalSection(&CriticalSection);
}


void HSLL::AudioContoller::Release()//数据重置
{
	if (pSourceVoice)
	{
		pSourceVoice->Stop(0);
		pSourceVoice->DestroyVoice();
		ClearUp();
	}

	if (PlayType == AudioPlayType_File)
		CloseHandle(pPlayInfo->hFile);

	if (PlayType != AudioPlayType_Custom)
	{
		ma_decoder_uninit(&pPlayInfo->Decoder);
		delete pPlayInfo;
	}

	State = PlayState_Default;
}


BOOL HSLL::AudioContoller::SetPlaySecond(DWORD Second)//设置播放位置
{
	if (Second >= pPlayInfo->Length)
		return false;

	EnterCriticalSection(&CriticalSection);

	pSourceVoice->Stop(0);

	ReCreateSourceVoice(pPlayInfo->Decoder.outputSampleRate * Second);

	if (State == PlayState_Playing)
		pSourceVoice->Start(0);

	LeaveCriticalSection(&CriticalSection);

	return true;
}


void __stdcall HSLL::AudioContoller::OnBufferEnd(void* pBufferContext) noexcept//buffer播放结束时
{
	XAUDIO2_BUFFER* pBuffer = (XAUDIO2_BUFFER*)pBufferContext;

	if (PlayType == AudioPlayType_Custom)
		pCustom->FreeBuffer((BYTE*)pBuffer->pAudioData);
	else
		delete[] pBuffer->pAudioData;

	pBuffer->pContext = nullptr;

	if (pBuffer->Flags!= XAUDIO2_END_OF_STREAM)
		SubmitBuffer();
}


void __stdcall HSLL::AudioContoller::OnStreamEnd() noexcept //音频播放结束时
{
	EnterCriticalSection(&CriticalSection);
	State=PlayState_End;
	LeaveCriticalSection(&CriticalSection);
	SetEvent(hEndEvent);
}


void __stdcall HSLL::AudioContoller::OnVoiceProcessingPassEnd()noexcept {}
void __stdcall HSLL::AudioContoller::OnVoiceProcessingPassStart(UINT32 SamplesRequired)noexcept {}
void __stdcall HSLL::AudioContoller::OnBufferStart(void* pBufferContext) noexcept {}
void __stdcall HSLL::AudioContoller::OnLoopEnd(void* pBufferContext) noexcept {}
void __stdcall HSLL::AudioContoller::OnVoiceError(void* pBufferContext, HRESULT Error)noexcept {}


ma_result HSLL::AudioContoller::on_read_f(ma_decoder* pDecoder, void* pBufferOut, size_t bytesToRead, size_t* pBytesRead)
{
	PlayInfo* userData = (PlayInfo*)pDecoder->pUserData;

	DWORD hasRead = 0;
	if (!ReadFile(userData->hFile, pBufferOut, bytesToRead, &hasRead, NULL))
		return MA_ERROR;

	userData->NowSize += hasRead;
	*pBytesRead = hasRead;
	return MA_SUCCESS;
}


ma_result HSLL::AudioContoller::on_read_b(ma_decoder* pDecoder, void* pBufferOut, size_t bytesToRead, size_t* pBytesRead)
{
	PlayInfo* userData = (PlayInfo*)pDecoder->pUserData;
	memcpy(pBufferOut, userData->Data + userData->NowSize, bytesToRead);
	*pBytesRead = bytesToRead;
	userData->NowSize += bytesToRead;
	return MA_SUCCESS;
}


ma_result HSLL::AudioContoller::on_seek_f(ma_decoder* pDecoder, ma_int64 byteOffset, ma_seek_origin origin)
{
	PlayInfo* userData = (PlayInfo*)pDecoder->pUserData;

	switch (origin)
	{
	case ma_seek_origin_start:

		SetFilePointer(userData->hFile, (LONG)(byteOffset + userData->Offset), NULL, FILE_BEGIN);
		userData->NowSize = byteOffset;

		break;
	case ma_seek_origin_current:

		SetFilePointer(userData->hFile, (LONG)(byteOffset), NULL, FILE_CURRENT);
		userData->NowSize += byteOffset;

		break;
	case ma_seek_origin_end:

		SetFilePointer(userData->hFile, (LONG)(byteOffset + userData->TotalSize + userData->Offset), NULL, FILE_BEGIN);
		userData->NowSize = userData->TotalSize + byteOffset;

		break;
	}

	return MA_SUCCESS;
}


ma_result HSLL::AudioContoller::on_seek_b(ma_decoder* pDecoder, ma_int64 byteOffset, ma_seek_origin origin)
{
	PlayInfo* userData = (PlayInfo*)pDecoder->pUserData;

	switch (origin)
	{
	case ma_seek_origin_start:

		userData->NowSize = byteOffset;

		break;
	case ma_seek_origin_current:

		userData->NowSize += byteOffset;

		break;
	case ma_seek_origin_end:

		userData->NowSize = userData->TotalSize + byteOffset;

		break;
	}

	return MA_SUCCESS;
}


HSLL::HSAudio::~HSAudio()//析构
{
	if (--Ref == 0)
	{
		pMasterVoice->DestroyVoice();
		pXAudio2->Release();
		pXAudio2 = nullptr;
		pMasterVoice = nullptr;
	}
}
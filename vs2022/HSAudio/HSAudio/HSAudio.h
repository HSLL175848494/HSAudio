#ifndef HS_AUDIO

//inc
#include "MiniAudio.h"
#include<Windows.h>
#include<xaudio2.h>
#include<wrl.h>

//lib
#pragma comment(lib, "winmm.lib")

namespace HSLL
{

#define HSAUDIO_BUFFERSIZE 3
#define HSAUDIO_FRAMESREAD 512

	enum AudioPlayType
	{
		AudioPlayType_Buffer,
		AudioPlayType_File,
		AudioPlayType_Custom
	};

	enum PlayState
	{
		PlayState_Default,
		PlayState_Playing,
		PlayState_Suspended,
		PlayState_End
	};

	struct AudioBuffer
	{
		ma_uint64 Size;
		BYTE* Data;
	};

	class AudioCustom
	{
	public:

		virtual AudioBuffer GetBuffer() = 0;//获取Buffer
		virtual void SetNowPlay(DWORD Second) = 0;//设置当前播放的时间(在之后的GetBuffer中，以Second秒为起始读取帧)
		virtual void FreeBuffer(BYTE* Buffer) = 0;//销毁Buffer
	};

	class AudioContoller : public IXAudio2VoiceCallback
	{
	private:

		friend class  AudioCustom;
		friend class HSAudio;

		struct PlayInfo
		{
			BOOL Loop;//是否循环
			HANDLE hFile;//文件句柄(从文件播放时有效)
			DWORD Offset;//文件中音乐数据的偏移位置(从文件播放时有效)
			BYTE* Data;//音乐数据(从buffer播放时使用)
			ma_uint64 TotalSize;//总共大小
			ma_uint64 NowSize;//已读取大小
			ma_int64 Length;//总时长
			ma_decoder Decoder;//解码器
		};

		WAVEFORMATEXTENSIBLE WaveFormat;//格式
		AudioPlayType PlayType;//播放模式

		PlayInfo* pPlayInfo;//音乐
		AudioCustom* pCustom;//自定义解码器(自定义播放时采用)
		IXAudio2SourceVoice* pSourceVoice;	//音源

		WORD NowIndex;//当前索引
		XAUDIO2_BUFFER Buffers[HSAUDIO_BUFFERSIZE];//数据buffers

		PlayState State;//播放状态
		HANDLE hEndEvent;//播放结束事件
		CRITICAL_SECTION CriticalSection;//临界区

	public:

		~AudioContoller();

		AudioContoller();

		void WaitForEnd();//等待播放结束

		void Start();//开始播放

		void Stop();//停止播放

		void Release();//释放数据(之后再次通过HSAudio的CreateAudio设置上下文)

		BOOL SetVolume(float Volume);//设置音量

		BOOL SetPlaySecond(DWORD Second);//设置播放位置(对自定义读取方式无效)

	private:

		void ClearUp();//清理资源

		BOOL InitDecoder();//初始化解码器

		void SubmitBuffer();//提交buffer

		void ReCreateSourceVoice(DWORD StartFrame);//重新创建SourceVoice

		AudioBuffer ReadBuffer();//从文件中获取buffer

		BOOL AudioContollerInit(BYTE* Data, DWORD Size, BOOL Loop);//读取载入内存的音乐(格式要求:)

		BOOL AudioContollerInit(HANDLE hFile, DWORD Offset, DWORD Size, BOOL Loop);//读取文件中的音乐(格式要求:)

		BOOL AudioContollerInit(WAVEFORMATEXTENSIBLE Format, AudioCustom* pDecoder);//自定义读取

		void STDMETHODCALLTYPE OnBufferEnd(void* pBufferContext)noexcept override;
		void STDMETHODCALLTYPE OnStreamEnd()noexcept override;
		void STDMETHODCALLTYPE  OnVoiceProcessingPassEnd() noexcept override;
		void STDMETHODCALLTYPE  OnVoiceProcessingPassStart(UINT32 SamplesRequired) noexcept override;
		void STDMETHODCALLTYPE  OnBufferStart(void* pBufferContext) noexcept override;
		void STDMETHODCALLTYPE  OnLoopEnd(void* pBufferContext)noexcept override;
		void STDMETHODCALLTYPE  OnVoiceError(void* pBufferContext, HRESULT Error)noexcept override;

		static ma_result on_seek_f(ma_decoder* pDecoder, ma_int64 byteOffset, ma_seek_origin origin);//seek回调
		static ma_result on_seek_b(ma_decoder* pDecoder, ma_int64 byteOffset, ma_seek_origin origin);//seek回调
		static ma_result on_read_f(ma_decoder* pDecoder, void* pBufferOut, size_t bytesToRead, size_t* pBytesRead);//read回调
		static ma_result on_read_b(ma_decoder* pDecoder, void* pBufferOut, size_t bytesToRead, size_t* pBytesRead);//read回调

	};

	class HSAudio
	{
	private:

		friend class AudioContoller;

		static BOOL Initialized;//是否已经初始化
		static DWORD Ref;//引用数目
		static IXAudio2* pXAudio2;//引擎
		static IXAudio2MasteringVoice* pMasterVoice;//主音源

	public:

		BOOL Initialize();//初始化

		 BOOL CreateAudio(BYTE* Data, DWORD Size, BOOL Loop, AudioContoller* pController);//读取载入内存的音乐(格式要求:)

		 BOOL CreateAudio(LPCSTR FileName, DWORD Offset, DWORD Size, BOOL Loop, AudioContoller* pController);//读取文件中的音乐(格式要求:)

		 BOOL CreateAudio(LPCSTR FileName,BOOL Loop, AudioContoller* pController);//读取文件中的音乐(格式要求:)

		 BOOL CreateAudio(WAVEFORMATEXTENSIBLE Format, AudioCustom* pDecoder, AudioContoller* pController);//自定义读取

		static BOOL CoInitialize();//初始化com组件

		static void CoUninitialize();//卸载com组件（请保证当前线程不再使用com组件）

		~HSAudio();//析构
	};
}

#define HS_AUDIO
#endif // !HS_AUDIO

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

		virtual AudioBuffer GetBuffer() = 0;//��ȡBuffer
		virtual void SetNowPlay(DWORD Second) = 0;//���õ�ǰ���ŵ�ʱ��(��֮���GetBuffer�У���Second��Ϊ��ʼ��ȡ֡)
		virtual void FreeBuffer(BYTE* Buffer) = 0;//����Buffer
	};

	class AudioContoller : public IXAudio2VoiceCallback
	{
	private:

		friend class  AudioCustom;
		friend class HSAudio;

		struct PlayInfo
		{
			BOOL Loop;//�Ƿ�ѭ��
			HANDLE hFile;//�ļ����(���ļ�����ʱ��Ч)
			DWORD Offset;//�ļ����������ݵ�ƫ��λ��(���ļ�����ʱ��Ч)
			BYTE* Data;//��������(��buffer����ʱʹ��)
			ma_uint64 TotalSize;//�ܹ���С
			ma_uint64 NowSize;//�Ѷ�ȡ��С
			ma_int64 Length;//��ʱ��
			ma_decoder Decoder;//������
		};

		WAVEFORMATEXTENSIBLE WaveFormat;//��ʽ
		AudioPlayType PlayType;//����ģʽ

		PlayInfo* pPlayInfo;//����
		AudioCustom* pCustom;//�Զ��������(�Զ��岥��ʱ����)
		IXAudio2SourceVoice* pSourceVoice;	//��Դ

		WORD NowIndex;//��ǰ����
		XAUDIO2_BUFFER Buffers[HSAUDIO_BUFFERSIZE];//����buffers

		PlayState State;//����״̬
		HANDLE hEndEvent;//���Ž����¼�
		CRITICAL_SECTION CriticalSection;//�ٽ���

	public:

		~AudioContoller();

		AudioContoller();

		void WaitForEnd();//�ȴ����Ž���

		void Start();//��ʼ����

		void Stop();//ֹͣ����

		void Release();//�ͷ�����(֮���ٴ�ͨ��HSAudio��CreateAudio����������)

		BOOL SetVolume(float Volume);//��������

		BOOL SetPlaySecond(DWORD Second);//���ò���λ��(���Զ����ȡ��ʽ��Ч)

	private:

		void ClearUp();//������Դ

		BOOL InitDecoder();//��ʼ��������

		void SubmitBuffer();//�ύbuffer

		void ReCreateSourceVoice(DWORD StartFrame);//���´���SourceVoice

		AudioBuffer ReadBuffer();//���ļ��л�ȡbuffer

		BOOL AudioContollerInit(BYTE* Data, DWORD Size, BOOL Loop);//��ȡ�����ڴ������(��ʽҪ��:)

		BOOL AudioContollerInit(HANDLE hFile, DWORD Offset, DWORD Size, BOOL Loop);//��ȡ�ļ��е�����(��ʽҪ��:)

		BOOL AudioContollerInit(WAVEFORMATEXTENSIBLE Format, AudioCustom* pDecoder);//�Զ����ȡ

		void STDMETHODCALLTYPE OnBufferEnd(void* pBufferContext)noexcept override;
		void STDMETHODCALLTYPE OnStreamEnd()noexcept override;
		void STDMETHODCALLTYPE  OnVoiceProcessingPassEnd() noexcept override;
		void STDMETHODCALLTYPE  OnVoiceProcessingPassStart(UINT32 SamplesRequired) noexcept override;
		void STDMETHODCALLTYPE  OnBufferStart(void* pBufferContext) noexcept override;
		void STDMETHODCALLTYPE  OnLoopEnd(void* pBufferContext)noexcept override;
		void STDMETHODCALLTYPE  OnVoiceError(void* pBufferContext, HRESULT Error)noexcept override;

		static ma_result on_seek_f(ma_decoder* pDecoder, ma_int64 byteOffset, ma_seek_origin origin);//seek�ص�
		static ma_result on_seek_b(ma_decoder* pDecoder, ma_int64 byteOffset, ma_seek_origin origin);//seek�ص�
		static ma_result on_read_f(ma_decoder* pDecoder, void* pBufferOut, size_t bytesToRead, size_t* pBytesRead);//read�ص�
		static ma_result on_read_b(ma_decoder* pDecoder, void* pBufferOut, size_t bytesToRead, size_t* pBytesRead);//read�ص�

	};

	class HSAudio
	{
	private:

		friend class AudioContoller;

		static BOOL Initialized;//�Ƿ��Ѿ���ʼ��
		static DWORD Ref;//������Ŀ
		static IXAudio2* pXAudio2;//����
		static IXAudio2MasteringVoice* pMasterVoice;//����Դ

	public:

		BOOL Initialize();//��ʼ��

		 BOOL CreateAudio(BYTE* Data, DWORD Size, BOOL Loop, AudioContoller* pController);//��ȡ�����ڴ������(��ʽҪ��:)

		 BOOL CreateAudio(LPCSTR FileName, DWORD Offset, DWORD Size, BOOL Loop, AudioContoller* pController);//��ȡ�ļ��е�����(��ʽҪ��:)

		 BOOL CreateAudio(LPCSTR FileName,BOOL Loop, AudioContoller* pController);//��ȡ�ļ��е�����(��ʽҪ��:)

		 BOOL CreateAudio(WAVEFORMATEXTENSIBLE Format, AudioCustom* pDecoder, AudioContoller* pController);//�Զ����ȡ

		static BOOL CoInitialize();//��ʼ��com���

		static void CoUninitialize();//ж��com������뱣֤��ǰ�̲߳���ʹ��com�����

		~HSAudio();//����
	};
}

#define HS_AUDIO
#endif // !HS_AUDIO

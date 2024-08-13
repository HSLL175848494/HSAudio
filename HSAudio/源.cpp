#include"HSAudio.h"
#pragma comment(lib, "winmm.lib")

using namespace HSLL;


int main()
{

	{
		HSAudio::CoInitialize();
		HSAudio audio;
		bool result = audio.Initialize();

		AudioContoller controller;
		BOOL re = audio.CreateAudio("D:\\音乐\\1_亚托莉Official - ひだまりの場所｜向阳之处 ED_(Instrumental).wav", false, &controller);
		controller.Start();
		controller.WaitForEnd();
	}

	return 0;
}
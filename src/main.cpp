#include "game.h"
#include "messaging.h"

#include "GL/gl3w.h"
#include "GL/glcorearb.h"
#include "GLFW/glfw3.h"

#include <Windows.h>

int main()
{
	run_game();
}

#ifdef _WIN32
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	main();
}
#endif

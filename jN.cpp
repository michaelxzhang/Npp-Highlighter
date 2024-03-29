/*
This file is part of jN, a plugin for Notepad++
Copyright (C)2013 Eugen Kremer <eugen DOT kremer AT gmail DOT com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <atlstr.h>
#include <tchar.h>
#include "common\System.h"
#include "common\Statics.h"


#include "editor\Editor.h"


NppData nppData;

CEditor* g_editor;
bool     g_editor_ready = false;
bool     g_fileopen = false;    //use this to identify if we open the new file

//System*  g_system;

const TCHAR START_SCRIPT_FILE_NAME[] = TEXT("jN\\start.js");


HHOOK hook;

extern FuncItem funcItem[nbFunc];

//
// Initialize your plugin data here
// It will be called while plugin loading   
void pluginInit(HANDLE hModule)
{

	//HRESULT res = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	HRESULT res = OleInitialize(NULL);
	
	Statics::instance().hInstance = (HMODULE)hModule;
}

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  reasonForCall, LPVOID lpReserved ){
    switch (reasonForCall)
    {
      case DLL_PROCESS_ATTACH:
        pluginInit(hModule);

        break;

      case DLL_PROCESS_DETACH:
		commandMenuCleanUp();
		pluginCleanUp();

        break;

      case DLL_THREAD_ATTACH:
        break;

      case DLL_THREAD_DETACH:
        break;
    }

    return TRUE;
}

void loadInitScript(){

	g_editor = new CEditor(nppData._nppHandle);
	
	/*TCHAR scriptfile[MAX_PATH];

	GetModuleFileName(Statics::instance().hInstance, scriptfile, MAX_PATH);
	
	BOOL res = PathRemoveFileSpec((LPWSTR)scriptfile);
	PathAppend((LPWSTR)scriptfile, START_SCRIPT_FILE_NAME); 

	if (PathFileExists(scriptfile)== TRUE){

		HANDLE hFile = CreateFile(
			scriptfile, // name of the write
			GENERIC_READ,			// open for writing
			0,                      // do not share
			NULL,                   // default security
			OPEN_ALWAYS,			// overwrite existing
			FILE_ATTRIBUTE_NORMAL,  // normal file
			NULL
		);                  // no attr. template

		if (hFile == INVALID_HANDLE_VALUE) 
		{ 
			::MessageBox(0,TEXT("Could not open file (error %d)\n"),TEXT("Error"), MB_OK);
			return;
		}

		char* buf = NULL;
		DWORD fSize, fRead;
		fSize = GetFileSize(hFile, NULL);

		if (fSize == INVALID_FILE_SIZE){
			::MessageBox(0,TEXT("Could not open file (error %d)\n"),TEXT("Error"), MB_OK);
			CloseHandle(hFile);
			return;
		}

		buf = new char[fSize+1]; 
		buf[fSize] = 0;

		ReadFile(hFile, buf, fSize,& fRead, NULL);
		CloseHandle(hFile);


		// assume file is UTF8 encoded
		int newlen = MultiByteToWideChar(CP_UTF8, 0, buf, fSize, NULL,0);
		BSTR wbuf = SysAllocStringLen(NULL, newlen);
		int res = MultiByteToWideChar(CP_UTF8, 0, buf, fSize, wbuf, newlen);

		if (buf!=NULL)	
		  delete[] buf;

		g_editor = new CEditor(nppData._nppHandle);
		LocRef<System> system = new System(scriptfile, *MyActiveSite::getInstance());

		MyActiveSite::getInstance()->addNamedItem(TEXT("Editor"), static_cast<IUnknown*>(g_editor));
		MyActiveSite::getInstance()->addNamedItem(TEXT("System"), static_cast<IUnknown*>(system));

		MyActiveSite::getInstance()->Connect();

		MyActiveSite::getInstance()->runScript(wbuf, scriptfile);

		SysFreeString(wbuf);
	}
	*/
}

extern "C" __declspec(dllexport) void setInfo(NppData notpadPlusData){
	nppData = notpadPlusData;
	
	Statics::instance().hWindow = nppData._nppHandle;

	commandMenuInit();
}

extern "C" __declspec(dllexport) const TCHAR * getName(){
	return NPP_PLUGIN_NAME;
}

extern "C" __declspec(dllexport) FuncItem * getFuncsArray(int *nbF)
{
	*nbF = nbFunc;
	return funcItem;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification *scnNotification)
{
	switch (scnNotification->nmhdr.code){
		case NPPN_READY:{

			::OutputDebugString(_T("NPPN Ready"));
			loadInitScript();

			break;
		}
		case NPPN_SHUTDOWN:{

			if (g_editor != NULL){
				g_editor->doOnMessage(scnNotification);

				g_editor->Release();
				g_editor = NULL;
			}

			//MyActiveSite::getInstance()->CloseScript();//Release();

			//OleUninitialize();
			break;
		}
		
		case NPPN_BUFFERACTIVATED:
		{
			::OutputDebugString(_T("NPPN_BUFFERACTIVATED"));
			g_fileopen = false;

			break;
		}

		case NPPN_FILEBEFORELOAD:
		{
			::OutputDebugString(_T("NPPN_FILEBEFORELOAD"));
			g_fileopen = true;

			break;
		}

		case SCN_MODIFIED:
		{
			if (g_editor != NULL) 
			{
				int ModifyType = scnNotification->modificationType;

				CString s;
				s.Format(_T("SCN_MODIFIED=%lx\n"), ModifyType);
				::OutputDebugString(s);
				
				if (g_fileopen == false)
				{
					if (g_editor_ready && (scnNotification->linesAdded != 0))
					{
						//if multiple lines added or deleted at the same time
						//the notification will send the each position, so we'll pass the pos and use it to get the linenum
						int pos = scnNotification->position;

						g_editor->AddRef();
						g_editor->doOnFileLinesAddedDeleted(pos, scnNotification->linesAdded);
						g_editor->Release();
					}
					//on some machine, we receive SCN_MODIFIED with modificationType=0x11, so need to use g_editor_ready to protect
					else if (g_editor_ready && ((ModifyType & SC_MOD_INSERTTEXT) || (ModifyType & SC_MOD_DELETETEXT)))
					{
							//if multiple lines changed at the same time (column mode)
							//the notification will send the each position, so we'll pass the pos and use it to get the linenum
							int pos = scnNotification->position;

							g_editor->AddRef();
							g_editor->doOnFileModified(pos);
							g_editor->Release();
					}
				}

				g_editor->AddRef();
				g_editor->doOnMessage(scnNotification);
				g_editor->Release();
			}
			break;
		}
		case SCN_UPDATEUI:
		{
			if ((g_editor != NULL)&&(scnNotification->updated != 0x3)&&(scnNotification->updated & SC_UPDATE_SELECTION))
			{
				::OutputDebugString(_T("SCN_UPDATEUI\n"));
				g_editor_ready = true;
				g_editor->AddRef();
				g_editor->doOnClick();
				g_editor->Release();

				g_editor->AddRef();
				g_editor->doOnMessage(scnNotification);
				g_editor->Release();
			}
			break;
		}
		case SCN_DOUBLECLICK:
		{
			if (g_editor != NULL) 
			{

				g_editor->AddRef();
				g_editor->doOnDoubleClick();
				g_editor->Release();

				g_editor->AddRef();
				g_editor->doOnMessage(scnNotification);
				g_editor->Release();
			}
			break;
		}
		default:
			if (g_editor != NULL){
				g_editor->AddRef();
				g_editor->doOnMessage(scnNotification);
				g_editor->Release();
			}
			
		break;
	}
}


// Here you can process the Npp Messages 
// I will make the messages accessible little by little, according to the need of plugin development.
// Please let me know if you need to access to some messages :
// http://sourceforge.net/forum/forum.php?forum_id=482781
//
extern "C" __declspec(dllexport) LRESULT messageProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
	return TRUE;
}

#ifdef UNICODE
extern "C" __declspec(dllexport) BOOL isUnicode() 
{
    return TRUE;
}
#endif //UNICODE


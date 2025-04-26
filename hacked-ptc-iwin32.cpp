/////////////////////
// Win32 interface //
/////////////////////
#include "iwin32.h"

#ifdef __WIN32__

volatile char keyboard_buffer[128];
volatile int keyboard_head;
volatile int keyboard_tail;
volatile int my_mouseX;
volatile int my_mouseY;
volatile bool my_lbutton;
volatile bool my_rbutton;
volatile bool windowed;
//volatile bool altDn = false;
volatile bool switchModeWhenPossible=false;


// static window critical section
CRITICAL_SECTION IWin32::StaticCriticalSection;
int IWin32::StaticCriticalSectionReferenceCount=0;







IWin32::IWin32(WINDOW window)
{
    // initialize static critical section
    InitStaticCriticalSection();

    // initialize local critical sections
    InitLocalCriticalSections();

    // clear status
    Status=1;

    // defaults
    XResolution=0;
    YResolution=0;
    Recent=NULL;
    WindowKeyboardEvent=NULL;
    WindowThread=NULL;
    WindowTitle=NULL;
    WindowShowCommand=SW_HIDE;

    // setup window
    Window=window;                 
    if (Window)
    {
        // setup user window
        WindowDC=GetDC(Window);
        if (!WindowDC) Status=0;
        else
        {
            // non-managed
            Managed=0;

            // get window res
            RECT rect;
            GetClientRect(Window,&rect);
            XResolution=rect.right-rect.left;
            YResolution=rect.bottom-rect.top;
        }
    }
    else
    {
        // managed window
        WindowDC=NULL;
        Managed=1;
    }
}


IWin32::~IWin32()
{
    // close non-managed window dc
    if (!Managed && Window && WindowDC) ReleaseDC(Window,WindowDC);

    // close local critical sections
    CloseLocalCriticalSections();

    // close static critical section
    CloseStaticCriticalSection();
}







int IWin32::GetModeList(List<MODE> &modelist)
{
    // return modelist
    modelist=ModeList;
    return 1;
}







int IWin32::getch()
{
    // check getch type
    if (Window && Managed)
    {
        // wait for keyboard event
        WaitForSingleObject(WindowKeyboardEvent,INFINITE);

        // enter critical section
        EnterInputCriticalSection();

        // get key
        int key; 
        WindowKeyboard.get(key);

        // update input signal
        if (!WindowKeyboard.check())
        {
            // no keys waiting: reset keyboard event
            ResetEvent(WindowKeyboardEvent);
        }

        // leave critical section
        LeaveInputCriticalSection();

        // return key
        return key;
    }
    else
    {
        // console getch
        return ::getch();
    }
}


int IWin32::kbhit()
{
    // check kbhit type
    if (Window && Managed)
    {
        // enter critical section
        EnterInputCriticalSection();

        // windowed kbhit check
        int result=WindowKeyboard.check();

        // leave critical section
        LeaveInputCriticalSection();

        // return result
        return result;
    }
    else
    {   
        // console kbhit
        return ::kbhit();
    }
}







int IWin32::SetTitle(char title[])
{
    // update title data
    delete[] WindowTitle;
    WindowTitle=new char[strlen(title)+1];
    strcpy(WindowTitle,title);

    // set window title
    if (Window)
    {
        uint result=SetWindowText(Window,title);
        if (result==strlen(title)) return 1;
    }
    
    // failure
    return 0;
}


int IWin32::GetTitle(char title[])
{
    // return window title
    strcpy(title,WindowTitle);
    return 1;
}







int IWin32::NativeType()
{                         
    return NATIVE_WIN32_HDC;
}


void* IWin32::GetNative()
{
    return (void*)&WindowDC;
}









int IWin32::GetXResolution() const
{
    return XResolution;
}
        

int IWin32::GetYResolution() const
{
    return YResolution;
}
        

WINDOW IWin32::GetWindow() const
{
    return Window;
}







int IWin32::ok() const
{
    return Status;
}







Surface* IWin32::GetRecent()
{
    // get recent surface
    return Recent;
}


void IWin32::SetRecent(Surface &surface,RECTANGLE const &src,RECTANGLE const &dest)
{
    // enter critical section
    EnterUpdateCriticalSection();

    // setup recent data
    Recent=&surface;
    RecentSrcRect=src;
    RecentDestRect=dest;

    // leave critical section
    LeaveUpdateCriticalSection();
}


void IWin32::ClearRecent()
{
    // enter critical section
    EnterUpdateCriticalSection();

    // clear recent
    Recent=NULL;

    // leave critical section
    LeaveUpdateCriticalSection();
}







int IWin32::CreateManagedWindow(char wndclass[],uint style,int show,int width,int height)
{
    // fail if not managed
    if (!Managed) return 0;

    // close old window
    CloseManagedWindow();

    // setup window title
    char *title="";
    if (WindowTitle) title=WindowTitle;

    // setup window thread info
    WINDOWTHREADINFO info;
    info.i=this;
    info.wndclass=wndclass;
    info.title=title;
    info.width=width;
    info.height=height;
    info.style=style;
    info.show=show;
    info.event=CreateEvent(NULL,0,0,NULL);
    if (!info.event) return 0;

    // create window thread
    WindowThread=new Thread((Thread::FUNCTION)WindowThreadFunction,&info);
    if (!WindowThread || !WindowThread->ok())
    {
        // failure
        WindowThread=NULL;
        CloseManagedWindow();
        return 0;
    }

    // wait for window initialization
    WaitForSingleObject(info.event,INFINITE);
        
    // check for valid window
    if (!IsWindow(Window)) 
    {
        // failure
        CloseManagedWindow();
        return 0;
    }

    // create window keyboard event
    WindowKeyboardEvent=CreateEvent(NULL,1,0,NULL);
    if (!WindowKeyboardEvent)
    {
        // failure
        CloseManagedWindow();
        return 0;
    }

    // setup window dc
    WindowDC=GetDC(Window);
    if (!WindowDC) 
    {
        // failure
        CloseManagedWindow();
        return 0;
    }
    
    // close window thread info event
    if (info.event) CloseHandle(info.event);
    
    // success
    return 1;
}


int IWin32::ResizeManagedWindow(int width,int height)
{
    // enter critical section
    EnterWindowCriticalSection();

    // clear recent
    ClearRecent();

    // leave critical section
    LeaveWindowCriticalSection();

    // resize window
    SetWindowPos(Window,0,0,0,width,height,SWP_NOMOVE | SWP_NOZORDER);

    // done
    return 1;
}


void IWin32::CloseManagedWindow()
{
    // check if managed
    if (!Managed) return;

    // close window thread
    if (WindowThread && IsWindow(Window))
    {
        // enter critical section
        EnterWindowCriticalSection();

        // release window dc
        if (WindowDC) 
        {
            ReleaseDC(Window,WindowDC);
            WindowDC=NULL;
        }

        // clear recent
        Recent=NULL;

        // close window thread
        PostMessage(Window,WM_DESTROY,0,0);

        // leave critical section
        LeaveWindowCriticalSection();

        //printf("waiting for window thread to exit...");

        // wait for window thread to exit
        WindowThread->WaitForExit(INFINITE);

        //printf("done.\n");

        // cleanup
        delete WindowThread;
        WindowThread=NULL;
    }
    
    // release window dc
    if (WindowDC) 
    {
        ReleaseDC(Window,WindowDC);
        WindowDC=NULL;
    }

    // cleanup
    Window=NULL;

    // close keyboard event
    if (WindowKeyboardEvent) 
    {
        CloseHandle(WindowKeyboardEvent);
        WindowKeyboardEvent=NULL;
    }
}







HWND IWin32::GetManagedWindow()
{
    // get window
    return Window;
}


HDC IWin32::GetManagedWindowDC()
{
    // get window dc
    return WindowDC;
}


int IWin32::ManagedWindow()
{
    // managed flag
    return Managed;
}







void IWin32::AddMode(MODE const &mode)
{
    // add mode to modelist
    MODE *storage=new MODE;
    *storage=mode;
    ModeList.add(storage);
}


void IWin32::ClearModeList()
{
    // clear modelist
    ModeList.free();
}


List<MODE>* IWin32::GetModeList()
{
    // return modelist
    return &ModeList;
}







void IWin32::EnterInputCriticalSection()
{
    EnterCriticalSection(&InputCriticalSection);
}


void IWin32::EnterUpdateCriticalSection()
{
    EnterCriticalSection(&UpdateCriticalSection);
}


void IWin32::EnterWindowCriticalSection()
{
    EnterCriticalSection(&WindowCriticalSection);
}


void IWin32::EnterPaletteCriticalSection()
{
    EnterCriticalSection(&PaletteCriticalSection);
}


void IWin32::LeaveInputCriticalSection()
{
    LeaveCriticalSection(&InputCriticalSection);
}


void IWin32::LeaveUpdateCriticalSection()
{
    LeaveCriticalSection(&UpdateCriticalSection);
}


void IWin32::LeaveWindowCriticalSection()
{
    LeaveCriticalSection(&WindowCriticalSection);
}


void IWin32::LeavePaletteCriticalSection()
{
    LeaveCriticalSection(&PaletteCriticalSection);
}







void IWin32::EnterStaticCriticalSection()
{
    EnterCriticalSection(&StaticCriticalSection);
}


void IWin32::LeaveStaticCriticalSection()
{
    LeaveCriticalSection(&StaticCriticalSection);
}







int IWin32::Detach(Surface *surface)
{
    // update recent
    if (surface==Recent) ClearRecent();
    return ISoftware::Detach(surface);
}







void IWin32::WindowThreadFunction(WINDOWTHREADINFO *info)
{
    // create window
    info->i->Window=CreateWindow(info->wndclass,info->title,info->style,
                                 CW_USEDEFAULT,CW_USEDEFAULT,
                                 info->width,info->height,
                                 NULL,NULL,NULL,NULL);
    
    // check window
    int result=IsWindow(info->i->Window);

    // store data locally
    HWND window=info->i->Window;
    int show=info->show;

    // setup window
    if (result)
    {
        // show window
        ShowWindow(window,show);
    }

    // signal window event
    SetEvent(info->event);

    // check result
    if (!result) return;

    // set focus
    SetFocus(window);

    // force window to front if not hidden (DX)
    if (show!=SW_HIDE) SetForegroundWindow(window);

    // message loop
    MSG msg;
    while (GetMessage(&msg,NULL,0,0)==TRUE)
    {       
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        Sleep(0);
    }
}


LRESULT CALLBACK IWin32::WndProc(IWin32 *i,HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam)
{
    // enter critical section
    if (i) i->EnterWindowCriticalSection();

    // result
    int result=0;

    // message handler
    if (i)
    {
        switch (message) 
        {
            case WM_MOUSEMOVE:
            {
                my_mouseX = LOWORD(lParam);
                my_mouseY = HIWORD(lParam);
                break;
            }
            case WM_LBUTTONDOWN:
			{
			  my_lbutton = true;
			}
			break;
            case WM_LBUTTONUP:
			{
			  my_lbutton = false;
			}
			break;
            case WM_RBUTTONDOWN:
			{
			  my_rbutton = true;
			}
			break;
            case WM_RBUTTONUP:
			{
			  my_rbutton = false;
			}
			break;


            case WM_KEYDOWN:
            {   
                /* // enter critical section
                i->EnterInputCriticalSection();

                // todo: translate virtual key
                i->WindowKeyboard.insert(1);

                // set keyboard event
                SetEvent(i->WindowKeyboardEvent);

                // leave input critical section
                i->LeaveInputCriticalSection(); */

                unsigned char keycode = (unsigned char) wParam;

                //char str[50];
                //sprintf(str,"%d %d",(int)wParam,(int)lParam);
                //MessageBox(NULL,str,str,MB_OK);

                //if(keycode == 18)       // alt down
                //{
                //  altDn = true;
                //}
                //else if(keycode == 116)
                //{
                //  altDn = false;
                //}
                if((keycode == 13))
                {
                  switchModeWhenPossible = true;
                }
                else
                {
                  keyboard_buffer[keyboard_tail] = keycode;
                  keyboard_tail++;
                  if(keyboard_tail >= 128)
                  {
                     keyboard_tail = 0;
                  }
                }

                break;
            }

            case WM_PAINT:
            {
                // enter critical section
                i->EnterUpdateCriticalSection();

                // need clear flag
                int clear=1;

                // redraw window
                if (i->Recent)
                {
                    // update most recent surface
                    if (i->Recent->Update(i)) clear=0;
                }
        
                // check clear
                if (clear)
                {
                    // fill window with black
                    RECT rect;
                    GetClientRect(hWnd,&rect);
                    HBRUSH black=GetStockObject(BLACK_BRUSH);
                    HDC dc=GetDC(hWnd);
                    HBRUSH old=SelectObject(dc,black);
                    Rectangle(dc,rect.left,rect.top,rect.right,rect.bottom);
                    SelectObject(dc,old);
                    ReleaseDC(hWnd,dc);
                }

                // validate window
                ValidateRect(hWnd,NULL);

                // leave critical section
                i->LeaveUpdateCriticalSection();
                break;
            }

            case WM_CLOSE:
            {
                // exit
                exit(0);
            }
        
            case WM_DESTROY: 
            {
                // shutdown window
                ShowWindow(hWnd,SW_HIDE);
                PostQuitMessage(0);
                break;
            }

            default:
            {
                // unhandled messages
                result=DefWindowProc(hWnd,message,wParam,lParam);
            }
        }
    }
    else
    {
        switch (message)
        {
            case WM_PAINT:
            {
                // fill window with black
                RECT rect;
                GetClientRect(hWnd,&rect);
                HBRUSH black=GetStockObject(BLACK_BRUSH);
                HDC dc=GetDC(hWnd);
                HBRUSH old=SelectObject(dc,black);
                Rectangle(dc,rect.left,rect.top,rect.right,rect.bottom);
                SelectObject(dc,old);
                ReleaseDC(hWnd,dc);
            
                // validate window
                ValidateRect(hWnd,NULL);
                break;
            }

            case WM_CLOSE:
            {
                // exit
                exit(0);
            }

            case WM_DESTROY: 
            {
                // shutdown window
                ShowWindow(hWnd,SW_HIDE);
                PostQuitMessage(0);
                break;
            }

            default:
        
                // unhandled messages
                result=DefWindowProc(hWnd,message,wParam,lParam);
        }
    }

    // leave critical section
    if (i) i->LeaveWindowCriticalSection();
    return result;
}







void IWin32::InitLocalCriticalSections()
{
    // initialize local critical sections
    InitializeCriticalSection(&InputCriticalSection);
    InitializeCriticalSection(&UpdateCriticalSection);
    InitializeCriticalSection(&WindowCriticalSection);
    InitializeCriticalSection(&PaletteCriticalSection);
}


void IWin32::CloseLocalCriticalSections()
{
    // delete local critical sections
    DeleteCriticalSection(&InputCriticalSection);
    DeleteCriticalSection(&UpdateCriticalSection);
    DeleteCriticalSection(&WindowCriticalSection);
    DeleteCriticalSection(&PaletteCriticalSection);
}







void IWin32::InitStaticCriticalSection()
{
    // check reference count...
    if (StaticCriticalSectionReferenceCount==0)
    {
        // initialize critical section
        InitializeCriticalSection(&StaticCriticalSection);
    }

    // incremement reference count
    StaticCriticalSectionReferenceCount++;
}


void IWin32::CloseStaticCriticalSection()
{
    // check reference count
    if (StaticCriticalSectionReferenceCount==1)
    {
        // delete critical section
        DeleteCriticalSection(&StaticCriticalSection);
    }

    // decrement reference count
    if (StaticCriticalSectionReferenceCount>0) StaticCriticalSectionReferenceCount--;
}







#endif

#include<oleacc.h>
#include<iostream>
#include<fstream>
#include <string>

// #include <stdio.h>
// #include <assert.h>
// #include <windows.h>
#include <comdef.h>

//#include <afxwin.h>
#pragma comment(lib,"oleacc.lib")
using namespace std;
#include <thread>

#define MAGIC_CODE 0x1603ABD9

#define IDC_NEW_TAB                     34014
#define IDC_CLOSE_TAB                   34015
#define IDC_SELECT_NEXT_TAB             34016
#define IDC_SELECT_PREVIOUS_TAB         34017
#define IDC_SELECT_TAB_0                34018
#define IDC_SELECT_TAB_1                34019
#define IDC_SELECT_TAB_2                34020
#define IDC_SELECT_TAB_3                34021
#define IDC_SELECT_TAB_4                34022
#define IDC_SELECT_TAB_5                34023
#define IDC_SELECT_TAB_6                34024
#define IDC_SELECT_TAB_7                34025
#define IDC_SELECT_LAST_TAB             34026

#define IDC_UPGRADE_DIALOG              40024





HWND GetTopWnd(HWND hwnd)
{
    while (::GetParent(hwnd) && ::IsWindowVisible(::GetParent(hwnd)))
    {
        hwnd = ::GetParent(hwnd);
    }
    return hwnd;
}

//http://cn.voidcc.com/question/p-xpkiiqzl-rw.html
void write_text(std::string str)
{
    std::ofstream ofs;
    ofs.open("D:\\test1.txt");
    ofs << str;
    ofs.close();
}



void ExecuteCommand(int id, HWND hwnd = 0)
{
    if (hwnd == 0) hwnd = GetForegroundWindow();
    //hwnd = GetTopWnd(hwnd);
    //hwnd = GetForegroundWindow();
    //PostMessage(hwnd, WM_SYSCOMMAND, id, 0);
    ::SendMessageTimeoutW(hwnd, WM_SYSCOMMAND, id, 0, 0, 1000, 0);
}

HHOOK mouse_hook = NULL;

#define KEY_PRESSED 0x8000
bool IsPressed(int key)
{
    return key && (::GetKeyState(key) & KEY_PRESSED) != 0;
}

long GetAccessibleRole(IAccessible *node)
{
    VARIANT self;
    self.vt = VT_I4;
    self.lVal = CHILDID_SELF;

    VARIANT role;
    if (S_OK == node->get_accRole(self, &role))
    {
        if (role.vt == VT_I4)
        {
            return role.lVal;
        }
    }
    return 0;
}

long GetAccessibleState(IAccessible *node)
{
    VARIANT self;
    self.vt = VT_I4;
    self.lVal = CHILDID_SELF;

    VARIANT state;
    if (S_OK == node->get_accState(self, &state))
    {
        if (state.vt == VT_I4)
        {
            return state.lVal;
        }
    }
    return 0;
}

template<typename Function>
void TraversalAccessible(IAccessible *node, Function f)
{
    long childCount = 0;
    if (node && S_OK == node->get_accChildCount(&childCount) && childCount)
    {
        VARIANT *varChildren = (VARIANT *)malloc(sizeof(VARIANT) * childCount);
        if (S_OK == AccessibleChildren(node, 0, childCount, varChildren, &childCount))
        {
            for (int i = 0; i < childCount; i++)
            {
                if (varChildren[i].vt == VT_DISPATCH)
                {
                    IDispatch *dispatch = varChildren[i].pdispVal;
                    IAccessible *child = NULL;
                    if (S_OK == dispatch->QueryInterface(IID_IAccessible, (void **)&child))
                    {
                        if ((GetAccessibleState(child) & STATE_SYSTEM_INVISIBLE) == 0) // 只遍历可见节点
                        {
                            if (f(child))
                            {
                                dispatch->Release();
                                break;
                            }
                            child->Release();
                        }
                    }
                    dispatch->Release();
                }
            }
        }
        free(varChildren);
    }
}

template<typename Function>
void TraversalRawAccessible(IAccessible *node, Function f)
{
    long childCount = 0;
    if (node && S_OK == node->get_accChildCount(&childCount) && childCount)
    {
        VARIANT *varChildren = (VARIANT *)malloc(sizeof(VARIANT) * childCount);
        if (S_OK == AccessibleChildren(node, 0, childCount, varChildren, &childCount))
        {
            for (int i = 0; i < childCount; i++)
            {
                if (varChildren[i].vt == VT_DISPATCH)
                {
                    IDispatch *dispatch = varChildren[i].pdispVal;
                    IAccessible *child = NULL;
                    if (S_OK == dispatch->QueryInterface(IID_IAccessible, (void **)&child))
                    {
                        //if ((GetAccessibleState(child) & STATE_SYSTEM_INVISIBLE) == 0) // 只遍历可见节点
                        {
                            if (f(child))
                            {
                                dispatch->Release();
                                break;
                            }
                            child->Release();
                        }
                    }
                    dispatch->Release();
                }
            }
        }
        free(varChildren);
    }
}


// 地址栏回车

template<typename Function>
void GetAccessibleValue(IAccessible *node, Function f)
{
    VARIANT self;
    self.vt = VT_I4;
    self.lVal = CHILDID_SELF;

    BSTR bstr = NULL;
    if( S_OK == node->get_accValue(self, &bstr) )
    {
        f(bstr);
        SysFreeString(bstr);
    }
}

IAccessible* FindChildElement(IAccessible *parent, long role)
{
    IAccessible* element = NULL;
    if (parent)
    {
        TraversalAccessible(parent, [&element, &role]
        (IAccessible* child) {
            if (GetAccessibleRole(child) == role)
            {
                element = child;
            }
            return element != NULL;
        });
    }
    return element;
}

// #pragma comment(lib,"legacy_stdio_definitions.lib")
// #pragma comment(lib,"psapi.lib")
// 是否焦点在地址栏
bool IsOmniboxViewFocus(IAccessible* top)
{
    bool flag = false;

    // 寻找地址栏
    IAccessible *LocationBarView = NULL;
    if (top)
    {
        TraversalAccessible(top, [&LocationBarView]
        (IAccessible* child) {
            if (GetAccessibleRole(child) == ROLE_SYSTEM_TOOLBAR)
            {
                IAccessible *group = FindChildElement(child, ROLE_SYSTEM_GROUPING);
                if (group)
                {
                    LocationBarView = group;
                    child->Release();
                    return true;
                }
            }
            return false;
        });
    }

    if (LocationBarView)
    {



              IAccessible *OmniboxViewViews = FindChildElement(LocationBarView, ROLE_SYSTEM_TEXT);
        if(OmniboxViewViews)
        {

         TraversalAccessible(LocationBarView,  [&OmniboxViewViews, &flag]
            (IAccessible* child){
                if(GetAccessibleRole(child)==ROLE_SYSTEM_TEXT)
                {
	                 if( (GetAccessibleState(OmniboxViewViews) & STATE_SYSTEM_FOCUSED) == STATE_SYSTEM_FOCUSED)
			                        {
			                            flag = true;
			                        }
                    // GetAccessibleValue(OmniboxViewViews,  [&OmniboxViewViews, &flag]
                    //     (BSTR bstr){	                        
                    //    //      if(bstr[0]!=0) // 地址栏为空直接跳过
			                 //    // {
			                 //        if( (GetAccessibleState(OmniboxViewViews) & STATE_SYSTEM_FOCUSED) == STATE_SYSTEM_FOCUSED)
			                 //        {
			                 //            flag = true;
			                 //        }
			                 //    // }                           
                    //     });
                }
                if(flag) child->Release();
                return flag;
            });
        LocationBarView->Release();

             }

	    
        // IAccessible *OmniboxViewViews = FindChildElement(LocationBarView, ROLE_SYSTEM_TEXT);
        // if(OmniboxViewViews)
        // {
        //     GetAccessibleValue(OmniboxViewViews, [&OmniboxViewViews, &flag]
        //         (BSTR bstr){
        //             if(bstr[0]!=0) // 地址栏为空直接跳过
        //             {
        //                 if( (GetAccessibleState(OmniboxViewViews) & STATE_SYSTEM_FOCUSED) == STATE_SYSTEM_FOCUSED)
        //                 {
        //                     flag = true;
        //                 }
        //             }
        //     });
        //     OmniboxViewViews->Release();
        // }
        // LocationBarView->Release();











        
    }
    else
    {
        // if (top) DebugLog(L"IsOmniboxViewFocus failed");
    }
    return flag;
}

// 地址栏回车





IAccessible *FindPageTabList(IAccessible *node)
{
    IAccessible *PageTabList = NULL;
    if (node)
    {
        TraversalAccessible(node, [&]
                            (IAccessible * child)
        {
            long role = GetAccessibleRole(child);
            if (role == ROLE_SYSTEM_PAGETABLIST)
            {
                PageTabList = child;
            }
            else if (role == ROLE_SYSTEM_PANE || role == ROLE_SYSTEM_TOOLBAR)
            {
                PageTabList = FindPageTabList(child);
            }
            return PageTabList;
        });
    }
    return PageTabList;
}

IAccessible *FindPageTab(IAccessible *node)
{
    IAccessible *PageTab = NULL;
    if (node)
    {
        TraversalAccessible(node, [&]
                            (IAccessible * child)
        {
            long role = GetAccessibleRole(child);
            if (role == ROLE_SYSTEM_PAGETAB)
            {
                PageTab = child;
            }
            else if (role == ROLE_SYSTEM_PANE || role == ROLE_SYSTEM_TOOLBAR)
            {
                PageTab = FindPageTab(child);
            }
            return PageTab;
        });
    }
    return PageTab;
}

IAccessible *GetParentElement(IAccessible *child)
{
    IAccessible *element = NULL;
    IDispatch *dispatch = NULL;
    if (S_OK == child->get_accParent(&dispatch) && dispatch)
    {
        IAccessible *parent = NULL;
        if (S_OK == dispatch->QueryInterface(IID_IAccessible, (void **)&parent))
        {
            element = parent;
        }
        dispatch->Release();
    }
    return element;
}

IAccessible *GetTopContainerView(HWND hwnd)
{
    IAccessible *TopContainerView = NULL;
    wchar_t name[MAX_PATH];
    if (GetClassName(hwnd, name, MAX_PATH) && wcscmp(name, L"Chrome_WidgetWin_1") == 0)
    {
        IAccessible *paccMainWindow = NULL;
        if (S_OK == AccessibleObjectFromWindow(hwnd, OBJID_WINDOW, IID_IAccessible, (void **)&paccMainWindow))
        {
            IAccessible *PageTabList = FindPageTabList(paccMainWindow);
            if (PageTabList)
            {
                TopContainerView = GetParentElement(PageTabList);
                PageTabList->Release();
            }
            if (!TopContainerView)
            {
                // DebugLog(L"FindTopContainerView failed");
            }
        }
    }
    return TopContainerView;
}



template<typename Function>
void GetAccessibleSize(IAccessible *node, Function f)
{
    VARIANT self;
    self.vt = VT_I4;
    self.lVal = CHILDID_SELF;

    RECT rect;
    if (S_OK == node->accLocation(&rect.left, &rect.top, &rect.right, &rect.bottom, self))
    {
        //rect.left = (int)((float)rect.left);
        //rect.top = (int)((float)rect.top);
        //rect.right = (int)((float)rect.right);
        //rect.bottom = (int)((float)rect.bottom);

        rect.right += rect.left;
        rect.bottom += rect.top;

        f(rect);
    }
}





// 鼠标是否在某个标签上
bool IsOnOneTab(IAccessible *top, POINT pt)
{
    bool flag = false;
    IAccessible *PageTabList = FindPageTabList(top);
    if (PageTabList)
    {
        IAccessible *PageTab = FindPageTab(PageTabList);
        if(PageTab)
        {
            IAccessible *PageTabPane = GetParentElement(PageTab);
            if (PageTabPane)
            {
                TraversalAccessible(PageTabPane, [&flag, &pt]
                                    (IAccessible * child)
                {
                    if (GetAccessibleRole(child) == ROLE_SYSTEM_PAGETAB)
                    {
                        GetAccessibleSize(child, [&flag, &pt]
                                          (RECT rect)
                        {
                            if (PtInRect(&rect, pt))
                            {
                                flag = true;
                            }
                        });
                    }
                    if (flag) child->Release();
                    return flag;
                });
                PageTabPane->Release();
            }
            PageTab->Release();
        }
        PageTabList->Release();
    }
    else
    {
        // if (top) DebugLog(L"IsOnOneTab failed");
    }
    return flag;
}

int GetRawChildCount(IAccessible *node)
{
    int count = 0;
    TraversalRawAccessible(node, [&count]
                           (IAccessible * child)
    {
        count++;
        return false;
    });
    return count;
}

// 是否只有一个标签
bool IsOnlyOneTab(IAccessible *top)
{
    IAccessible *PageTabList = FindPageTabList(top);
    if (PageTabList)
    {
        //DebugLog(L"IsOnlyOneTab");
        long tab_count = 0;
        bool closing = false;
        int last_count = 0;

        IAccessible *PageTab = FindPageTab(PageTabList);
        if(PageTab)
        {
            IAccessible *PageTabPane = GetParentElement(PageTab);
            if (PageTabPane)
            {
                TraversalAccessible(PageTabPane, [&tab_count, &closing, &last_count]
                                    (IAccessible * child)
                {
                    //if (GetAccessibleRole(child) == ROLE_SYSTEM_PAGETAB && GetChildCount(child))
                    if (GetAccessibleRole(child) == ROLE_SYSTEM_PAGETAB)
                    {
                        //DebugChildCount(child);
                        if (last_count == 0)
                        {
                            last_count = GetRawChildCount(child);
                        }
                        else
                        {
                            if (last_count != GetRawChildCount(child))
                            {
                                closing = true;
                            }
                        }
                        tab_count++;
                    }
                    return false;
                });
                PageTabPane->Release();
            }
            PageTab->Release();
        }
        //DebugLog(L"closing %d,%d", closing, tab_count);
        PageTabList->Release();
        return tab_count <= 1 || (closing && tab_count == 2);
    }
    else
    {
        // if (top) DebugLog(L"IsOnlyOneTab failed");
    }
    return false;
}

// 鼠标是否在标签栏上
bool IsOnTheTab(IAccessible *top, POINT pt)
{
    bool flag = false;
    IAccessible *PageTabList = FindPageTabList(top);
    if (PageTabList)
    {
        GetAccessibleSize(PageTabList, [&flag, &pt]
                          (RECT rect)
        {
            if (PtInRect(&rect, pt))
            {
                flag = true;
            }
        });
        PageTabList->Release();
    }
    else
    {
        // if (top) DebugLog(L"IsOnTheTab failed");
    }
    return flag;
}


//EditByJN20220413   鼠标是否在Pane上
IAccessible *FindPagePane(IAccessible *node)
{
    IAccessible *PageTabList = NULL;
    if (node)
    {
        TraversalAccessible(node, [&]
                            (IAccessible * child)
        {
            long role = GetAccessibleRole(child);
            if (role == ROLE_SYSTEM_PAGETAB)
            {
                PageTabList = child;
            }
            else if (role == ROLE_SYSTEM_PANE || role == ROLE_SYSTEM_TOOLBAR)
            {
                PageTabList = FindPageTab(child);
            }
            return PageTabList;
            
        });
    }
    return PageTabList;
}



bool IsOnThePane(IAccessible* top, POINT pt)
{
	return false;
    bool flag = false;
    IAccessible *PageTabList = FindPageTab(top);
    if (PageTabList)
    {
        GetAccessibleSize(PageTabList, [&flag, &pt]
                          (RECT rect)
        {
            if (PtInRect(&rect, pt))
            {
                flag = true;
            }
        });
        PageTabList->Release();
    }
    else
    {
        // if (top) DebugLog(L"IsOnTheTab failed");
    }
    return flag;
}
//EditByJN20220413    鼠标是否在Pane上


//EditByJN20220414 
template<typename Function>
void GetAccessibleName(IAccessible *node, Function f)
{
    VARIANT self;
    self.vt = VT_I4;
    self.lVal = CHILDID_SELF;

    BSTR bstr = NULL;
    if( S_OK == node->get_accName(self, &bstr) )
    {
        f(bstr);
        // SysFreeString(bstr);
    }
}


//转化std::wstring为std::string
std::string stows(std::wstring& ws)
{
	std::string curLocale = setlocale(LC_ALL, NULL); // curLocale = "C";
	setlocale(LC_ALL, "chs");
	const wchar_t* _Source = ws.c_str();
	size_t _Dsize = 2 * ws.size() + 1;
	char *_Dest = new char[_Dsize];
	memset(_Dest, 0, _Dsize);
	wcstombs(_Dest, _Source, _Dsize);
	std::string result = _Dest;
	delete[]_Dest;
	setlocale(LC_ALL, curLocale.c_str());
	return result;
}


bool IsBlankTab(IAccessible *top)
{
    bool flag = false;
    IAccessible *PageTabList = FindPageTabList(top);
    if (PageTabList)
    {
        IAccessible *PageTab = FindPageTab(PageTabList);
        if(PageTab)
        {
            IAccessible *PageTabPane = GetParentElement(PageTab);
            if (PageTabPane)
            {
                TraversalAccessible(PageTabPane, [&flag]
                                    (IAccessible * child)
                {

	                    if (GetAccessibleState(child) & STATE_SYSTEM_SELECTED) // 只遍历可见节点
                        {
                        GetAccessibleName(child, [&flag]
                            (BSTR bstr){	                            
                            std::wstring bstr_String = _bstr_t(bstr, false);  //转化BSTR为wstring
						    if(!(stows(bstr_String).find("标签页") == string::npos))//判断当前标签页标题是否包含标签页文字
						    {
							    flag = true;
						    }
                        });

                        }
                    if (flag) child->Release();
                    return flag;
                });
                PageTabPane->Release();
            }
            PageTab->Release();
        }
        PageTabList->Release();
    }

    return flag;
}
//EditByJN20220414 




bool IsOnOneBookmarkInner(IAccessible* parent, POINT pt)
{
    bool flag = false;

    // 寻找书签栏
    IAccessible *BookmarkBarView = NULL;
    if (parent)
    {
        TraversalAccessible(parent, [&BookmarkBarView]
        (IAccessible* child) {
            if (GetAccessibleRole(child) == ROLE_SYSTEM_TOOLBAR)
            {
                IAccessible *group = FindChildElement(child, ROLE_SYSTEM_GROUPING);
                if (group==NULL)
                {
                    BookmarkBarView = child;
                    return true;
                }
                group->Release();
            }
            return false;
        });
    }

    if(BookmarkBarView)
    {
        TraversalAccessible(BookmarkBarView, [&flag, &pt]
            (IAccessible* child){
	                
                    GetAccessibleSize(child, [&flag, &pt]
                        (RECT rect){
                            if(PtInRect(&rect, pt))
                            {
                                flag = true;
                            }
                        });
 
                if(flag) child->Release();
                return flag;
            });
        BookmarkBarView->Release();
    }
    return flag;
}



// 是否点击书签栏
bool IsOnOneBookmark(IAccessible* top, POINT pt)
{
    bool flag = false;
    if(top)
    {
        // 开启了书签栏长显
        if(IsOnOneBookmarkInner(top, pt)) return true;


        // 未开启书签栏长显
        IDispatch* dispatch = NULL;
        if( S_OK == top->get_accParent(&dispatch) && dispatch)
        {
            IAccessible* parent = NULL;
            if( S_OK == dispatch->QueryInterface(IID_IAccessible, (void**)&parent))
            {
                flag = IsOnOneBookmarkInner(parent, pt);
                parent->Release();
            }
            dispatch->Release();
        }
    }
    return flag;
}



#include <map>
std::map <HWND, bool> tracking_hwnd;

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    static bool wheel_tab_ing = false;
    static bool right_close_tab_ing = false;
    bool bookmark_new_tab = false;
    if (nCode != HC_ACTION)
    {
        return CallNextHookEx(mouse_hook, nCode, wParam, lParam);
    }

    if (nCode == HC_ACTION)
    {
        PMOUSEHOOKSTRUCT pmouse = (PMOUSEHOOKSTRUCT)lParam;

        if (wParam == WM_MOUSEMOVE || wParam == WM_NCMOUSEMOVE)
        {
            return CallNextHookEx(mouse_hook, nCode, wParam, lParam);
        }

        if (pmouse->dwExtraInfo == MAGIC_CODE)
        {
            goto next;
        }

        if (wParam == WM_RBUTTONUP && wheel_tab_ing)
        {
            wheel_tab_ing = false;
            return 1;
        }

        if (wParam == WM_MOUSEWHEEL)
        {
            HWND hwnd = WindowFromPoint(pmouse->pt);
            IAccessible *TopContainerView = GetTopContainerView(hwnd);

            PMOUSEHOOKSTRUCTEX pwheel = (PMOUSEHOOKSTRUCTEX)lParam;
            int zDelta = GET_WHEEL_DELTA_WPARAM(pwheel->mouseData);

            if (IsOnTheTab(TopContainerView, pmouse->pt) || IsPressed(VK_RBUTTON))
            {
                hwnd = GetTopWnd(hwnd);
                if (zDelta > 0)
                {
                    ExecuteCommand(IDC_SELECT_PREVIOUS_TAB, hwnd);
                }
                else
                {
                    ExecuteCommand(IDC_SELECT_NEXT_TAB, hwnd);
                }

                wheel_tab_ing = true;
                if (TopContainerView)
                {
                    TopContainerView->Release();
                }
                return 1;
            }
        }
        //EditByJN20220405
        if (wParam == WM_RBUTTONUP)
        {
            //write_text("ee");
            //DebugLog("www");
            //MessageBox("这是一个有标题的消息框！");
           if (!IsPressed(VK_SHIFT))
           {
            HWND hwnd = WindowFromPoint(pmouse->pt);
            IAccessible *TopContainerView = GetTopContainerView(hwnd);

            bool isOnOneTab = IsOnOneTab(TopContainerView, pmouse->pt);
            bool isOnlyOneTab = IsOnlyOneTab(TopContainerView);
            bool isOnTheTab = IsOnTheTab(TopContainerView, pmouse->pt);
            if (TopContainerView)
            {
                TopContainerView->Release();
            }
            

			//EditByJN20220413    鼠标在Pane上右击打开新标签页




        

            if (isOnTheTab && !isOnOneTab)
            {
	            	std::thread th([]() {
                       //SendKey(VK_LBUTTON);
                       ExecuteCommand(IDC_NEW_TAB);
						// char *url,*pData;
						// size_t length;
						// OpenClipboard(NULL);
						// HANDLE hData=GetClipboardData(CF_TEXT);
						// assert(hData!=NULL);
						// length=GlobalSize(hData);
						// url=(char*)malloc(length+1);
						// pData=(char*)GlobalLock(hData);
						// strcpy(url,pData);
						// GlobalUnlock(hData);
						// CloseClipboard();
						// url[length]=0;
						// printf("%s\n",url);
                        Sleep(50);
						keybd_event(VK_CONTROL, 0x1D, KEYEVENTF_EXTENDEDKEY | 0, 0);
						keybd_event('V', 0x2F, KEYEVENTF_EXTENDEDKEY | 0, 0);
						keybd_event('V', 0x2F, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
						keybd_event(VK_CONTROL, 0x1D, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);

                        Sleep(50);
						keybd_event(VK_CONTROL, 0x1D, KEYEVENTF_EXTENDEDKEY | 0, 0);
						keybd_event('A', 0x2F, KEYEVENTF_EXTENDEDKEY | 0, 0);
						keybd_event('A', 0x2F, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
						keybd_event(VK_CONTROL, 0x1D, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);	
                   });
                   th.detach();
                   return 1;             
            }
            //EditByJN20220413    鼠标在Pane上右击打开新标签页


            if (isOnOneTab)
            {
                if (isOnlyOneTab)
                {
                    // 最后一个标签页要关闭，新建一个标签
                    //DebugLog(L"keep_tab");
                    ExecuteCommand(IDC_NEW_TAB);
                    ExecuteCommand(IDC_SELECT_PREVIOUS_TAB);
                    ExecuteCommand(IDC_CLOSE_TAB);
                }
                else
                {
                   std::thread th([]() {
                       //SendKey(VK_LBUTTON);
                       mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,0);
                       Sleep(10);
                       mouse_event(MOUSEEVENTF_LEFTUP,0,0,0,0);
                       Sleep(50);
                       ExecuteCommand(IDC_CLOSE_TAB);
                   });
                   th.detach();
                    //ExecuteCommand(IDC_SELECT_PREVIOUS_TAB, hwnd);
                    //ExecuteCommand(IDC_CLOSE_TAB, hwnd);
                }
                return 1;
            }
           }
        }
        //EditByJN20220405




 if(wParam==WM_LBUTTONUP)
        {	        
            IAccessible* TopContainerView = GetTopContainerView(WindowFromPoint(pmouse->pt));
	        bool isOnTheBookmark = IsOnOneBookmark(TopContainerView, pmouse->pt);
	        IAccessible* TopContainerView1 = GetTopContainerView(GetForegroundWindow());
	        bool isBlank = IsBlankTab(TopContainerView1);
	        if (TopContainerView)
	        {
	            TopContainerView->Release();
	        }
	        if (TopContainerView1)
		    {
		        TopContainerView1->Release();
		    }

		    
			if(isOnTheBookmark && !isBlank)
			{
				bookmark_new_tab = true;
			}
        }




        if (wParam == WM_LBUTTONDBLCLK)
        {
            HWND hwnd = WindowFromPoint(pmouse->pt);
            IAccessible *TopContainerView = GetTopContainerView(hwnd);

            bool isOnOneTab = IsOnOneTab(TopContainerView, pmouse->pt);
            bool isOnlyOneTab = IsOnlyOneTab(TopContainerView);

            if (TopContainerView)
            {
                TopContainerView->Release();
            }

            if (isOnOneTab)
            {
                if (isOnlyOneTab)
                {
                    // 最后一个标签页要关闭，新建一个标签
                    //DebugLog(L"keep_tab");
                    ExecuteCommand(IDC_NEW_TAB);
                    ExecuteCommand(IDC_SELECT_PREVIOUS_TAB);
                    ExecuteCommand(IDC_CLOSE_TAB);
                }
                else
                {
                    ExecuteCommand(IDC_CLOSE_TAB);
                    //ExecuteCommand(IDC_SELECT_PREVIOUS_TAB, hwnd);
                    //ExecuteCommand(IDC_CLOSE_TAB, hwnd);
                }
            }
        }

        if (wParam == WM_MBUTTONUP)
        {
            HWND hwnd = WindowFromPoint(pmouse->pt);
            IAccessible *TopContainerView = GetTopContainerView(hwnd);
            // 只有一个标签
            bool isOnOneTab = IsOnOneTab(TopContainerView, pmouse->pt);
            bool isOnlyOneTab = IsOnlyOneTab(TopContainerView);

            if (TopContainerView)
            {
                TopContainerView->Release();
            }
            // 发送中键消息，关闭标签
            if (isOnOneTab && isOnlyOneTab)
            {
                //DebugLog(L"keep_tab");
                ExecuteCommand(IDC_NEW_TAB);
                // ExecuteCommand(IDC_SELECT_PREVIOUS_TAB);
                //ExecuteCommand(IDC_CLOSE_TAB);
            }
        }
    }


        if(bookmark_new_tab)
    {
            // ExecuteCommand(IDC_NEW_TAB);
            // 发送中键消息，新建标签，前台
            // SendKeys(VK_MBUTTON, VK_SHIFT);


          std::thread th([]() {
			keybd_event(VK_SHIFT, 0x1D, KEYEVENTF_EXTENDEDKEY | 0, 0);
           	mouse_event(MOUSEEVENTF_MIDDLEDOWN,0,0,0,0);
            Sleep(10);
            mouse_event(MOUSEEVENTF_MIDDLEUP,0,0,0,0);
            Sleep(100);
			keybd_event(VK_SHIFT, 0x1D, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
           });
           th.detach();
 
			return 1;
    }


    
next:
    return CallNextHookEx(mouse_hook, nCode, wParam, lParam);
}

bool IsNeedKeep()
{
    bool keep_tab = false;

    IAccessible *TopContainerView = GetTopContainerView(GetForegroundWindow());
    if (IsOnlyOneTab(TopContainerView))
    {
        keep_tab = true;
    }

    if (TopContainerView)
    {
        TopContainerView->Release();
    }

    return keep_tab;
}

HHOOK keyboard_hook = NULL;
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && !(lParam & 0x80000000)) // pressed
    {
        bool keep_tab = false;

        if (wParam == 'W' && IsPressed(VK_CONTROL) && !IsPressed(VK_SHIFT))
        {
            keep_tab = IsNeedKeep();
        }
        if (wParam == VK_F4 && IsPressed(VK_CONTROL))
        {
            keep_tab = IsNeedKeep();
        }

        if (wParam == (char)13)
        {
	        IAccessible* TopContainerView = GetTopContainerView(GetFocus());
	        IAccessible* TopContainerView1 = GetTopContainerView(GetForegroundWindow());
            bool isFocus = IsOmniboxViewFocus(TopContainerView);
            bool isBlank = IsBlankTab(TopContainerView1);
			if (TopContainerView)
		    {
		        TopContainerView->Release();
		    }
		    if (TopContainerView1)
		    {
		        TopContainerView1->Release();
		    }
	        
	        if( !IsPressed(VK_MENU) && isFocus && !isBlank)
            {
                  // SendKeys(VK_MENU, VK_RETURN);
              	keybd_event(VK_MENU, 0x1D, KEYEVENTF_EXTENDEDKEY | 0, 0);
				keybd_event(VK_RETURN, 0x2F, KEYEVENTF_EXTENDEDKEY | 0, 0);
				keybd_event(VK_RETURN, 0x2F, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
				keybd_event(VK_MENU, 0x1D, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
                return 1;
            }
	        //https://github.com/shuax/GreenChrome/blob/master/src/TabBookmark.h
           /* std::CWnd *pControl;
            pControl = this->GetFocus();*/
            //int nId = pWnd->GetDlgCtrlID();
            // HWND pWnd = GetForegroundWindow();
            //CString str;
            // write_text(to_string(static_cast<long long>((int)pWnd)));
            
        }




        if (keep_tab)
        {
            ExecuteCommand(IDC_NEW_TAB);
            ExecuteCommand(IDC_SELECT_PREVIOUS_TAB);
            ExecuteCommand(IDC_CLOSE_TAB);
            return 1;
        }
    }
    return CallNextHookEx(keyboard_hook, nCode, wParam, lParam);
}



void UI()
{
    mouse_hook = SetWindowsHookEx(WH_MOUSE, MouseProc, hInstance, GetCurrentThreadId());
    keyboard_hook = SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, hInstance, GetCurrentThreadId());
}

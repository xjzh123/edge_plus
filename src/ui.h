﻿#include <oleacc.h>
#include <Windows.h>
#pragma comment(lib,"oleacc.lib")

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

IAccessible *FindChildElement(IAccessible *parent, long role, int skipcount = 0)
{
    IAccessible *element = NULL;
    if (parent)
    {
        int i = 0;
        TraversalAccessible(parent, [&element, &role, &i, &skipcount]
                            (IAccessible * child)
        {
            //DebugLog(L"当前 %d,%d", i, skipcount);
            if (GetAccessibleRole(child) == role)
            {
                if (i == skipcount)
                {
                    element = child;
                }
                i++;
            }
            return element != NULL;
        });
    }
    return element;
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

#include <map>
std::map <HWND, bool> tracking_hwnd;

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    static bool wheel_tab_ing = false;
    static bool right_close_tab_ing = false;

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
           if (!IsPressed(VK_SHIFT))
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
                   std::thread th([]() {
                       SendKey(VK_LBUTTON);
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

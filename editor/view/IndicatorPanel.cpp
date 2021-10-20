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
#include <mutex>
#include <atlstr.h>
#include "npp\Scintilla.h"
#include "npp\PluginDefinition.h"
#include "IndicatorPanel.h"

#pragma warning (disable : 4355)

COLORREF color_change = RGB(17, 240, 84);
COLORREF color_current_line = RGB(0, 0, 255);
COLORREF color_search = RGB(212, 157, 6);
std::mutex g_mset_mutex;  // protects m_set_modified_linenum
std::map<ULONG_PTR, std::set<size_t>> map_modified_linenum;

IndicatorPanel::IndicatorPanel(SCIView* view ): m_IndicPixelsUp(this), m_IndicLinesUp(this)
{
	m_IndicatorMask = (~0); // All of indicators are enabled
	m_Disabled = false;
	m_View = view;
	m_linemodified = false;
	m_current_linenum = 1;
	//m_set_modified_linenum.clear();
	m_totallines = m_View->sci(SCI_GETLINECOUNT, 0, 0);
	m_current_bufferid = ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0);
	map_modified_linenum[m_current_bufferid].clear();
	pixelIndicators = 0;

	// let window calculate nc region again, to be able to draw panel first time
	SetWindowPos(view->m_Handle, NULL,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

IndicatorPanel::~IndicatorPanel(void)
{
	ForegroundIdleHook::getInstance()->remove(&m_IndicLinesUp);
	ForegroundIdleHook::getInstance()->remove(&m_IndicPixelsUp);

	if (pixelIndicators)
		delete[] pixelIndicators;
}

void IndicatorPanel::RedrawIndicatorPanel(){
	m_linemodified = true;
	size_t totallines = m_View->sci(SCI_GETLINECOUNT, 0, 0);
	m_totallines = totallines;
	m_current_bufferid = ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0);

	BOOL res;
	res = 0;
	res = RedrawWindow(m_View->m_Handle, &m_PanelRect, NULL, RDW_INVALIDATE | RDW_FRAME);
	res = res;
};

LRESULT IndicatorPanel::OnNCCalcSize(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
	if (wParam && !m_Disabled){
		NCCALCSIZE_PARAMS* ncp = (NCCALCSIZE_PARAMS*)lParam;
		int borderWidth = GetSystemMetrics(SM_CXEDGE);
		int hScrollHeight = GetSystemMetrics(SM_CYVTHUMB);
		int vScrollWidth = GetSystemMetrics(SM_CXHTHUMB);

		m_PanelRect = ncp->rgrc[0];

		m_PanelRect.bottom -= (m_PanelRect.top + borderWidth); 
		m_PanelRect.top = borderWidth;

		m_PanelRect.right -= (m_PanelRect.left + borderWidth); 
		m_PanelRect.left = m_PanelRect.right - m_PanelWidth - borderWidth;

		ncp->rgrc[0].right -= (m_PanelWidth + borderWidth);

		m_UnderScroll.bottom	= m_PanelRect.bottom;
		m_UnderScroll.right		= m_PanelRect.left;
		m_UnderScroll.top		= m_UnderScroll.bottom	- hScrollHeight;
		m_UnderScroll.left		= m_UnderScroll.right	- vScrollWidth;

		ForegroundIdleHook::getInstance()->add( &m_IndicPixelsUp);
	}
	return m_View->CallOldWndProc(message,wParam,lParam);
}

LRESULT IndicatorPanel::OnNCPaint(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){

	if (m_Disabled)
		return m_View->CallOldWndProc(message,wParam,lParam);

	int borderWidth		= GetSystemMetrics(SM_CXFOCUSBORDER);
	int scrollWidth		= GetSystemMetrics(SM_CXVSCROLL);
	int scrollHHeight	= GetSystemMetrics(SM_CXHSCROLL);

	HDC hdc;
	HBRUSH hbr3DFace; 
	int res;

	hbr3DFace = (HBRUSH)GetSysColorBrush(COLOR_3DFACE); 

	HRGN prRG = CreateRectRgnIndirect(&m_PanelRect);

	hdc = GetWindowDC(hwnd);

	bool vscroll = hasStyle(hwnd, WS_VSCROLL);
	bool hscroll = hasStyle(hwnd, WS_HSCROLL);

	if (vscroll && hscroll){
		// fill small rectangle under vertical scrollbar and panel with 
		// frame color
		HRGN combinedRG	= CreateRectRgnIndirect(&m_UnderScroll);
		HRGN underRG	= CreateRectRgnIndirect(&m_UnderScroll);
		res = CombineRgn(combinedRG, underRG, prRG, RGN_OR);

		res = FillRgn(hdc, combinedRG, hbr3DFace);

		DeleteObject(combinedRG);
		DeleteObject(underRG);

	}else{
		res = FillRgn(hdc, prRG, hbr3DFace);
	}

	m_linemodified = true;

	paintIndicators();

	DeleteObject(prRG);

	ReleaseDC(hwnd, hdc);

	return m_View->CallOldWndProc(message,wParam,lParam);
}

void IndicatorPanel::ClearIndicators(int begin, int end){
	if (begin <= 0 && end < 0)
		m_Indicators.clear();

	// document line
	unsigned int beginLine	= (begin <= 0)? 0 : m_View->sci(SCI_LINEFROMPOSITION, begin, 0);
	unsigned int endLine	= (end < 0)? m_View->sci(SCI_GETLINECOUNT, 0, 0) : m_View->sci(SCI_LINEFROMPOSITION, end, 0);

	// view line
	beginLine	= m_View->sci(SCI_VISIBLEFROMDOCLINE, beginLine, 0);
	endLine		= m_View->sci(SCI_VISIBLEFROMDOCLINE, endLine, 0);

	// walk backwards
	for(int i=m_Indicators.size()-1; i >=0  ; i--){
		LineMask& lm = m_Indicators[i];
		if (beginLine > lm.line || endLine < lm.line)
			continue;

		m_Indicators.erase(m_Indicators.begin() + i);
	}
}

void IndicatorPanel::GetIndicatorLines(int begin, int end){ 

	ClearIndicators(-1, -1);

	if (begin < 0)
		begin = 0;

	if (end < 0)
		end = m_View->sci(SCI_GETLENGTH, 0, 0);
	
	DWORD	mask = 0;

	int line = m_View->sci(SCI_LINEFROMPOSITION, begin, 0);
	int lineende = m_View->sci(SCI_GETLINEENDPOSITION, line, 0);;

	for (int p=begin; p < end; p++){

		// move indicator mask for line into the list of masks
		// if end of line or end of file
		if (lineende <= p || p == end-1){ // new line 

			// dont save empty masks
			if (mask){ // save mask from last line
				// get position of line in the view
				DWORD viewPos = m_View->sci(SCI_VISIBLEFROMDOCLINE, line, 0);
				if (viewPos >= 0){
					LineMask lm = {viewPos, mask};
					m_Indicators.push_back(lm);
				}
			}

			line++;
			lineende = m_View->sci(SCI_GETLINEENDPOSITION, line, 0);

			mask = 0;
		}
		//
		DWORD tmp = m_View->sci(SCI_INDICATORALLONFOR, p, 0); 	
		mask = mask | tmp;
	}

}

void IndicatorPanel::GetIndicatorPixels(){
	m_PixelIndicatorsLen = (m_PanelRect.bottom - m_PanelRect.top);

	if (pixelIndicators){
		delete[] pixelIndicators;
		pixelIndicators = NULL;
	}

	int scrollHHeight	= GetSystemMetrics(SM_CYHSCROLL);
	if (hasStyle(m_View->m_Handle, WS_VSCROLL))
		m_PixelIndicatorsLen -= 2*scrollHHeight;
	if (hasStyle(m_View->m_Handle, WS_HSCROLL))
		m_PixelIndicatorsLen -= scrollHHeight;

	if (m_PixelIndicatorsLen > 0){

		int lines = m_View->sci(SCI_GETLINECOUNT, 0,0);

		int lineHeight = m_View->sci(SCI_TEXTHEIGHT, 0, 0);

		if (lineHeight && lines){ // avoid division by NULL

			int visibleLines = m_View->sci(SCI_VISIBLEFROMDOCLINE, lines-1, 0)+1;
			int linesOnPage = (m_PanelRect.bottom - m_PanelRect.top) / lineHeight;

			// maximum pixel per line on panel line height
			float pixelPerLineOnPanel = (linesOnPage > visibleLines)? lineHeight : (float)m_PixelIndicatorsLen / visibleLines; 

			float pixelPerPageOnPanel = ((linesOnPage > visibleLines)?  visibleLines : (float)linesOnPage) * pixelPerLineOnPanel; 


			pixelIndicators = new DWORD[m_PixelIndicatorsLen];

			memset(pixelIndicators, 0, sizeof(DWORD) * m_PixelIndicatorsLen);

			// setup mask
			for(int l=0, c=m_Indicators.size(); l < c; l++){
				LineMask& lm = m_Indicators[l];
				int y = (int)(pixelPerLineOnPanel * (float)(lm.line));
				
				if (y >= m_PixelIndicatorsLen){ 
					// error! view go to switch current buffer
					// we can break here, it will be updated after switching
					break;
				}

				pixelIndicators[y] |= lm.mask;
			}

			// distribute mask to other pixels if possible
			for(int l=0; l < m_PixelIndicatorsLen; l++){
				DWORD pi = pixelIndicators[l];
				
				int next = l+1;
				DWORD indicator = 0x1;
				// if pi contains bits and next pixel does not have bits
				while(pi && next < m_PixelIndicatorsLen && !pixelIndicators[next]){
					
					// seek to next present indicator
					while(!(pi & indicator)){
						indicator = indicator << 1;
					}

					// substract indicator from pi
					pi = pi & ~indicator;
					
					// set indicator at the current position and 
					// move other indicators to the next pixel
					pixelIndicators[l] = indicator;
					pixelIndicators[next] = pi;

					l++;
					next++;

					// seek indicator
					indicator = indicator << 1;
				}
			}


		}
	}
}


void IndicatorPanel::paintIndicators(){
	HDC hdc = GetWindowDC(m_View->m_Handle);

	paintIndicators(hdc);

	ReleaseDC(m_View->m_Handle, hdc);
}

void IndicatorPanel::paintIndicators(HDC hdc){
	
	if ( m_Disabled)
		return;

	bool vscroll = hasStyle(m_View->m_Handle, WS_VSCROLL);
	int scrollHHeight	= GetSystemMetrics(SM_CXHSCROLL);
	HBRUSH hbr3DFace = (HBRUSH)GetSysColorBrush(COLOR_3DFACE); 

	int topOffset = vscroll ? m_PanelRect.top + scrollHHeight : m_PanelRect.top;

	BOOL res;

	//we only clear the search results indicators, keep the change indicators
	RECT t = m_PanelRect;
	t.left += 20;

	res = FillRect(hdc, &t, hbr3DFace);

	if (m_Indicators.size() > 0)
	{
		TCHAR desc[300] = { 0 };
		TCHAR out[350] = { 0 };
		int langid = 0;

		::SendMessage(nppData._nppHandle, NPPM_GETCURRENTLANGTYPE, NULL, (LPARAM)&langid);
		::SendMessage(nppData._nppHandle, NPPM_GETLANGUAGEDESC, langid, NULL);
		::SendMessage(nppData._nppHandle, NPPM_GETLANGUAGEDESC, langid, (LPARAM)desc);
	
		swprintf(out, 350, _T("%s    %d highlighted"), desc, m_Indicators.size());
		::SendMessage(nppData._nppHandle, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, (LPARAM)out);
	}

	COLORREF color = getColorForMask(1);
	HBRUSH brush = CreateSolidBrush(color);
	long curOffset = 0;


	long x = m_PanelRect.left + 2;
	long y = m_PanelRect.top;

	int lineHeight = m_View->sci(SCI_TEXTHEIGHT, 0, 0);
	int visuablelines = (m_PanelRect.bottom - m_PanelRect.top) / lineHeight;

	if (m_totallines < visuablelines)
		m_totallines = visuablelines;

	topOffset = 19;
	int height = m_PanelRect.bottom - m_PanelRect.top - 3 * scrollHHeight;

	DWORD mask = 1;
	DWORD prev_mask = 1;
	color = getColorForMask(mask);
	brush = CreateSolidBrush(color);

	for (auto it = m_Indicators.begin(); it != m_Indicators.end(); it++) 
	{
		mask = it->mask;
		if (mask) 
		{
			if(mask != prev_mask)
			{
				color = getColorForMask(mask);
				brush = CreateSolidBrush(color);
			}
			
			if(brush != NULL)
			{
				prev_mask = mask;
				curOffset = (it->line) * (height) / (long)(m_totallines)+topOffset;
				if (curOffset < m_PanelRect.top)
					curOffset = m_PanelRect.top;
				if (curOffset > m_PanelRect.bottom)
					curOffset = m_PanelRect.bottom;

				t = { m_PanelRect.left + 9, curOffset, m_PanelRect.left + 14, curOffset + 5 };
				FillRect(hdc, &t, brush);
			}
		}
	}

	if(m_linemodified)
	{
		brush = CreateSolidBrush(color_change);

		const std::lock_guard<std::mutex> lock(g_mset_mutex);
		for (auto it = map_modified_linenum[m_current_bufferid].begin(); it != map_modified_linenum[m_current_bufferid].end(); it++)
		{
			y = (*it) * (height) / (long)(m_totallines)+topOffset;

			if (y < m_PanelRect.top)
				y = m_PanelRect.top;
			if (y > m_PanelRect.bottom)
				y = m_PanelRect.bottom;

			t = { x, y, x + 5, y + 5 };
			FillRect(hdc, &t, brush);
		}

		m_linemodified = false;
	}

	size_t linenum = ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTLINE, 0, 0);

	y = linenum * height / (long)m_totallines + topOffset;

	brush = CreateSolidBrush(color_current_line);
	t = { x, y, x + 14, y + 2 };
	FillRect(hdc, &t, brush);

	DeleteObject(brush);
}



bool IndicatorPanel::hasStyle(HWND hwnd, int style) {
	return (GetWindowLongPtr(hwnd, GWL_STYLE) & style) != 0;
}

COLORREF IndicatorPanel::getColorForMask(DWORD mask){
	int indic = 0;
	int r,g,b;
	r=g=b=0;
	int n=0;

	while(mask){
		if (mask & 0x1){
			COLORREF c = m_View->sci(SCI_INDICGETFORE, indic, 0);
			r += GetRValue(c);
			g += GetGValue(c);
			b += GetBValue(c);
			n++;
		}
		indic++;
		mask >>=1;
	}
	if (n)
		return RGB(r / n, g / n, b / n);
	else
		return RGB(0,0,0);
}

void IndicatorPanel::SetDisabled(bool value){
	if (value == m_Disabled)
		return;

	m_Disabled = value;

	// let window calculate nc region again, to be able to draw panel first time
	SetWindowPos(m_View->m_Handle, NULL,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

bool IndicatorPanel::GetDisabled(){
	return m_Disabled;
}

DWORD IndicatorPanel::GetIndicatorMask(){
	return m_IndicatorMask;
}

void  IndicatorPanel::SetIndicatorMask(DWORD value){
	m_IndicatorMask = value;

	paintIndicators();
}

bool IndicatorPanel::fileModified(size_t linenum)
{
	size_t totallines = m_View->sci(SCI_GETLINECOUNT, 0, 0);

	m_current_bufferid = ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0);

	auto it = map_modified_linenum.find(m_current_bufferid);
	if(it != map_modified_linenum.end())
	{
		const std::lock_guard<std::mutex> lock(g_mset_mutex);
		map_modified_linenum[m_current_bufferid].insert(linenum);

		//if line got deleted, need to update the line number in m_map_modified_linenum 
		if (m_totallines > totallines)
		{
			int changednum = m_totallines - totallines;
			std::set<size_t>::iterator& it = map_modified_linenum[m_current_bufferid].end();
			it--;
			while(*it > linenum)
			{
				map_modified_linenum[m_current_bufferid].erase(it);
				map_modified_linenum[m_current_bufferid].insert((*it) - changednum);
				
				it = map_modified_linenum[m_current_bufferid].end();

				if (it == map_modified_linenum[m_current_bufferid].begin())
					break;
				
			}
		}
	}
	else
	{
		map_modified_linenum[m_current_bufferid].clear();
		map_modified_linenum[m_current_bufferid].insert(linenum);
	}

	m_totallines = totallines;
	m_linemodified = true;

	paintIndicators();

	return true;
}

bool IndicatorPanel::fileDoubleClicked()
{
	m_current_bufferid = ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0);

	::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, (LPARAM)43031);    //NPP MENU CMD: SEARCH_UNMARKALLEXT5

	::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, (LPARAM)43030);    //NPP MENU CMD: SEARCH_MARKALLEXT5

	return true;
}

bool IndicatorPanel::fileSingleClicked()
{
	m_current_bufferid = ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0);

	::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, (LPARAM)43031);    //NPP MENU CMD: SEARCH_UNMARKALLEXT5

	RedrawIndicatorPanel();

	return true;
}

bool IndicatorPanel::BufferActivated(ULONG_PTR bufferid)
{
	return true;
}
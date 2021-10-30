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
#pragma once

#include <vector>

#include "common\ForegroundIdleHook.h"
#include "SCIView.h"

#define INDICATOR_MODIFIED  0
#define INDICATOR_SEARCH    1

class IndicatorPanel
{
	struct LineMask{
		DWORD line;
		DWORD mask;
	};
	
	typedef std::vector<LineMask> LineMasks;

	LineMasks m_Indicators;
	LineMasks m_PixelIndicators;

	RECT	m_PanelRect;
	RECT	m_UnderScroll;

	SCIView* m_View;

	const static int m_PanelWidth = 14;


	bool           m_linemodified = false;
	size_t         m_totallines = 0;           //real total lines
	size_t         m_virtual_totallines = 0;   //total visual lines, used for calculate if the real line number less than the line number the current view could hold
	//std::set<size_t> m_set_modified_linenum;
	ULONG_PTR      m_current_bufferid;
	size_t         m_current_linenum = 0;

	long           m_draw_height = 0;
	long           m_topOffset = 0;
public:
	DWORD* pixelIndicators;
	int  m_PixelIndicatorsLen;

	void ClearIndicators(int begin, int end);
	
	void GetIndicatorLines(int begin, int end);

	void GetIndicatorPixels();

	void paintIndicators();

	void paintIndicators(HDC hdc);
	
	COLORREF getColorForMask(DWORD mask);

	bool fileModified(int pos);
	bool fileDoubleClicked();
	bool fileSingleClicked();
	void updateSelectedIndicator(HDC hdc);
	void updateChangedIndicator(HDC hdc);

	static bool hasStyle(HWND hwnd, int style);

	LRESULT OnNCCalcSize(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT OnNCPaint(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

	DWORD m_IndicatorMask; 
	bool  m_Disabled;

public:

	DWORD GetIndicatorMask();
	void SetIndicatorMask(DWORD value);

	void SetDisabled(bool value);
	bool GetDisabled();

	IndicatorPanel(SCIView* m_View);
	~IndicatorPanel(void);
	
	class IndicatorLinesUpdater : public ForegroundIdleHook::IdleHandler{
	public:
		IndicatorPanel* m_IndPanel;
		IndicatorLinesUpdater(IndicatorPanel* indPanel){
			m_IndPanel = indPanel;
			m_Begin = -1;
			m_End = -1;
		};

		int m_Begin;
		int m_End;

		void execute(){
			if (m_IndPanel->m_Disabled)
				return;

			m_IndPanel->GetIndicatorLines(m_Begin, m_End);
			m_IndPanel->RedrawIndicatorPanel();
			ForegroundIdleHook::getInstance()->add( &m_IndPanel->m_IndicPixelsUp);

			m_Begin = -1;
			m_End = -1;
		};
	};

	class IndicatorPixelsUpdater : public ForegroundIdleHook::IdleHandler{
		public:
		IndicatorPanel* m_IndPanel;
		IndicatorPixelsUpdater(IndicatorPanel* indPanel){m_IndPanel = indPanel;};
		void execute(){
			if (m_IndPanel->m_Disabled)
				return;

			/*m_IndPanel->GetIndicatorPixels();
			if (m_IndPanel->m_PixelIndicatorsLen > 0)
			{
				m_IndPanel->RedrawIndicatorPanel(); //m_View->paintIndicators();
			}*/
		};
	};


	IndicatorPixelsUpdater	m_IndicPixelsUp;
	IndicatorLinesUpdater	m_IndicLinesUp;

	void RedrawIndicatorPanel();

};

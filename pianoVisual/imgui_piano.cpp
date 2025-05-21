#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_piano.h>
#include <iostream>
#include <cassert>

namespace ImGuiExt::Piano
{
	void Keyboard
	(
		const char* idName,
		ImVec2 size,
		keyCode_t* prevNoteActive,
		keyCode_t beginOctaveNote,
		keyCode_t endOctaveNote,
		KeyboardCallback callback,
		void* userData,
		KeyboardStyles* keyboardStyles
	)
	{
		assert(beginOctaveNote <= keyCodeMax);
		assert(endOctaveNote <= keyCodeMax);
		assert(beginOctaveNote < endOctaveNote);

		constexpr bool isNoteDark[12] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 };
		constexpr unsigned char noteLightNumber[12] = { 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6, 7 };
		constexpr float noteDarkOffset[12] = { 0.0f,  -2.0f / 3.0f, 0.0f, -1.0f / 3.0f, 0.0f, 0.0f, -2.0f / 3.0f, 0.0f, -0.5f, 0.0f, -1.0f / 3.0f, 0.0f };

		// fix range dark keys
		if (isNoteDark[beginOctaveNote % 12]) beginOctaveNote++;
		if (isNoteDark[endOctaveNote % 12]) endOctaveNote--;

		// bad range
		if (!idName || !callback || beginOctaveNote < 0 || endOctaveNote < 0 || endOctaveNote <= beginOctaveNote) return;

		// style
		static KeyboardStyles ColorsBase;
		if (!keyboardStyles) keyboardStyles = &ColorsBase;

		ImGuiWindow* const window = ImGui::GetCurrentWindow();
		if (window->SkipItems) return;

		const ImGuiID imGuiWindowId = window->GetID(idName);

		ImDrawList* const drawList = window->DrawList;

		const ImVec2 pos = window->DC.CursorPos;
		const ImVec2 mousePos = ImGui::GetIO().MousePos;

		// sizes
		const int countNotesAllign7 = (endOctaveNote / 12 - beginOctaveNote / 12) * 7 + noteLightNumber[endOctaveNote % 12] - (noteLightNumber[beginOctaveNote % 12] - 1);
		const float noteHeight = size.y;
		const float noteWidth = size.x / (float)countNotesAllign7;
		const float noteHeightDark = noteHeight * keyboardStyles->NoteDarkHeight;
		const float noteWidthDark = noteWidth * keyboardStyles->NoteDarkWidth;

		// minimal size draw
		if (noteHeight < 5.0 || noteWidth < 3.0) return;

		// minimal size using mouse
		const bool isMouseInput = (noteHeight >= 10.0 && noteWidth >= 5.0);

		// item
		const ImRect boundingRect(pos, pos + size); // ImVec2(Pos.x + Size.x, Pos.y + Size.y));
		ImGui::ItemSize(size, 0);
		if (!ImGui::ItemAdd(boundingRect, imGuiWindowId)) return;

		// item input
		bool held = false;
		if (isMouseInput)
		{
			ImGui::ButtonBehavior(boundingRect, imGuiWindowId, nullptr, &held, 0);
		}

		keyCode_t keyCodeClicked = keyCodeNone;
		float velocityClicked = 0.0f;

		float offsetX = boundingRect.Min.x;
		float offsetY = boundingRect.Min.y;
		float offsetY2 = offsetY + noteHeight;
		for (keyCode_t keyCode = beginOctaveNote; keyCode <= endOctaveNote; keyCode++)
		{
			if (isNoteDark[keyCode % 12]) continue;

			ImRect NoteRect(roundf(offsetX), offsetY, roundf(offsetX + noteWidth), offsetY2);

			if (held && NoteRect.Contains(mousePos))
			{
				keyCodeClicked = keyCode;
				velocityClicked = (mousePos.y - NoteRect.Min.y) / noteHeight;
			}

			const bool isActive = callback(userData, KeyboardMsgType::NoteGetStatus, keyCode, 0.0f);
			drawList->AddRectFilled(NoteRect.Min, NoteRect.Max, keyboardStyles->Colors[isActive ? 2 : 0], 0.0f);
			drawList->AddRect(NoteRect.Min, NoteRect.Max, keyboardStyles->Colors[4], 0.0f);
			offsetX += noteWidth;
		}

		// draw dark notes
		offsetX = boundingRect.Min.x;
		offsetY = boundingRect.Min.y;
		offsetY2 = offsetY + noteHeightDark;
		for (keyCode_t keyCode = beginOctaveNote; keyCode <= endOctaveNote; keyCode++)
		{
			const auto i = keyCode % 12;
			if (!isNoteDark[i])
			{
				offsetX += noteWidth;
				continue;
			}

			const float offsetDark = noteDarkOffset[i] * noteWidthDark;
			const ImRect noteRect(roundf(offsetX + offsetDark), offsetY, roundf(offsetX + noteWidthDark + offsetDark), offsetY2);

			if (held && noteRect.Contains(mousePos))
			{
				keyCodeClicked = keyCode;
				velocityClicked = (mousePos.y - noteRect.Min.y) / noteHeightDark;
			}

			bool isActive = callback(userData, KeyboardMsgType::NoteGetStatus, keyCode, 0.0f);
			drawList->AddRectFilled(noteRect.Min, noteRect.Max, keyboardStyles->Colors[isActive ? 3 : 1], 0.0f);
			drawList->AddRect(noteRect.Min, noteRect.Max, keyboardStyles->Colors[4], 0.0f);
		}

		// mouse note click
		if (*prevNoteActive != keyCodeClicked)
		{
			if (*prevNoteActive != keyCodeNone)
			{
				callback(userData, KeyboardMsgType::NoteOff, *prevNoteActive, 0.0f);
				*prevNoteActive = keyCodeNone;
			}

			if (held && keyCodeClicked >= 0)
			{
				callback(userData, KeyboardMsgType::NoteOn, keyCodeClicked, velocityClicked);
				*prevNoteActive = keyCodeClicked;
			}
		}
	}
}
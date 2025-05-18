/*
The MIT License (MIT)
by https://github.com/frowrik

Piano keyboard for ImGui v1.1

example:
static int PrevNoteActive = -1;
ImGui_PianoKeyboard("PianoTest", ImVec2(1024, 100), &PrevNoteActive, 21, 108, TestPianoBoardFunct, nullptr, nullptr);

bool TestPianoBoardFunct(void* UserData, int Msg, int Key, float Vel) {
		if (Key >= 128 || Key < 0) return false; // midi max keys
		if (Msg == NoteGetStatus) return KeyPresed[Key];
		if (Msg == NoteOn) { KeyPresed[Key] = true; Send_Midi_NoteOn(Key, Vel*127); }
		if (Msg == NoteOff) { KeyPresed[Key] = false; Send_Midi_NoteOff(Key, Vel*127);}
		return false;
}
*/

#pragma once

#include <../imgui/imgui.h>
#include <../imgui/imgui_internal.h>

namespace ImGuiExt::Piano
{
	using keyCode_t = unsigned char;
	constexpr keyCode_t keyCodeCount{ 128 };
	constexpr keyCode_t keyCodeMin{ 0 };
	constexpr keyCode_t keyCodeMax{ keyCodeCount - 1 };
	constexpr keyCode_t keyCodeNone = 255;

	enum class KeyboardMsgType { NoteGetStatus, NoteOn, NoteOff };

	using KeyboardCallback = bool (*)(void* UserData, KeyboardMsgType msgType, keyCode_t keyCode, float velocity);

	struct KeyboardStyles
	{
		ImU32 Colors[5]
		{
			IM_COL32(255, 255, 255, 255),	// light note
			IM_COL32(0, 0, 0, 255),			// dark note
			IM_COL32(255, 255, 0, 255),		// active light note
			IM_COL32(200, 200, 0, 255),		// active dark note
			IM_COL32(75, 75, 75, 255),		// background
		};
		float NoteDarkHeight = 2.0f / 3.0f; // dark note scale h
		float NoteDarkWidth = 2.0f / 3.0f;	// dark note scale w
	};

	void Keyboard(
		const char* idName,
		ImVec2 size,
		keyCode_t* prevNoteActive,
		keyCode_t beginOctaveNote,
		keyCode_t endOctaveNote,
		KeyboardCallback callback,
		void* userData = nullptr,
		KeyboardStyles* keyboardStyles = nullptr
	);
}

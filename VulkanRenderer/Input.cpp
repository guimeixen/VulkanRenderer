#include "Input.h"

InputManager* Input::inputManager = nullptr;

InputManager::InputManager()
{
	// Clear all the keys
	for (size_t i = 0; i < 512; i++)
	{
		keys[i].state = false;
		keys[i].justReleased = false;
		keys[i].justPressed = false;
	}

	Input::inputManager = this;

	mouseButtonsState[0] = { false, false, false };
	mouseButtonsState[1] = { false, false, false };

	mouseMoved = false;

	mousePosition = glm::vec2();
	charUpdated = false;

	scrollWheelY = 0.0f;

	lastKeyPressed = Keys::KEY_SPACE;
	lastButtons = 0;
	lastChar = 0;

	buttons = 0;
	leftStickX = 0.0f;
	leftStickY = 0.0f;
	rightStickX = 0.0f;
	rightStickY = 0.0f;

	keysToString[Keys::KEY_0] = "0";
	keysToString[Keys::KEY_1] = "1";
	keysToString[Keys::KEY_2] = "2";
	keysToString[Keys::KEY_3] = "3";
	keysToString[Keys::KEY_4] = "4";
	keysToString[Keys::KEY_5] = "5";
	keysToString[Keys::KEY_6] = "6";
	keysToString[Keys::KEY_7] = "7";
	keysToString[Keys::KEY_8] = "8";
	keysToString[Keys::KEY_9] = "9";
	keysToString[Keys::KEY_A] = "A";
	keysToString[Keys::KEY_B] = "B";
	keysToString[Keys::KEY_C] = "C";
	keysToString[Keys::KEY_D] = "D";
	keysToString[Keys::KEY_E] = "E";
	keysToString[Keys::KEY_F] = "F";
	keysToString[Keys::KEY_G] = "G";
	keysToString[Keys::KEY_H] = "H";
	keysToString[Keys::KEY_I] = "I";
	keysToString[Keys::KEY_J] = "J";
	keysToString[Keys::KEY_K] = "K";
	keysToString[Keys::KEY_L] = "L";
	keysToString[Keys::KEY_M] = "M";
	keysToString[Keys::KEY_N] = "N";
	keysToString[Keys::KEY_O] = "O";
	keysToString[Keys::KEY_P] = "P";
	keysToString[Keys::KEY_Q] = "Q";
	keysToString[Keys::KEY_R] = "R";
	keysToString[Keys::KEY_S] = "S";
	keysToString[Keys::KEY_T] = "T";
	keysToString[Keys::KEY_U] = "U";
	keysToString[Keys::KEY_V] = "V";
	keysToString[Keys::KEY_W] = "W";
	keysToString[Keys::KEY_X] = "X";
	keysToString[Keys::KEY_Y] = "Y";
	keysToString[Keys::KEY_Z] = "Z";
	keysToString[Keys::KEY_ESCAPE] = "Escape";
	keysToString[Keys::KEY_ENTER] = "Enter";
	keysToString[Keys::KEY_TAB] = "Tab";
	keysToString[Keys::KEY_BACKSPACE] = "Backspace";
	keysToString[Keys::KEY_DEL] = "Delete";
	keysToString[Keys::KEY_F1] = "F1";
	keysToString[Keys::KEY_F2] = "F2";
	keysToString[Keys::KEY_F3] = "F3";
	keysToString[Keys::KEY_F4] = "F4";
	keysToString[Keys::KEY_F5] = "F5";
	keysToString[Keys::KEY_F6] = "F6";
	keysToString[Keys::KEY_F7] = "F7";
	keysToString[Keys::KEY_F8] = "F8";
	keysToString[Keys::KEY_F9] = "F9";
	keysToString[Keys::KEY_F10] = "F10";
	keysToString[Keys::KEY_F11] = "F11";
	keysToString[Keys::KEY_F12] = "F12";
	keysToString[Keys::KEY_LEFT_SHIFT] = "Left shift";
	keysToString[Keys::KEY_LEFT_CONTROL] = "Left control";

	vitaButtonsToString[VitaButtons::VITA_SELECT] = "Select";
	vitaButtonsToString[VitaButtons::VITA_START] = "Start";
	vitaButtonsToString[VitaButtons::VITA_UP] = "Up";
	vitaButtonsToString[VitaButtons::VITA_RIGHT] = "Right";
	vitaButtonsToString[VitaButtons::VITA_DOWN] = "Down";
	vitaButtonsToString[VitaButtons::VITA_LEFT] = "Left";
	vitaButtonsToString[VitaButtons::VITA_LTRIGGER] = "Left trigger";
	vitaButtonsToString[VitaButtons::VITA_RTRIGGER] = "Right trigger";
	vitaButtonsToString[VitaButtons::VITA_TRIANGLE] = "Triangle";
	vitaButtonsToString[VitaButtons::VITA_CIRCLE] = "Circle";
	vitaButtonsToString[VitaButtons::VITA_CROSS] = "Cross";
	vitaButtonsToString[VitaButtons::VITA_SQUARE] = "Square";
	vitaButtonsToString[VitaButtons::VITA_PSBUTTON] = "PS button";
}

void InputManager::Update()
{
	//charUpdated = false;

	mouseMoved = false;

	for (int i = 0; i < 512; i++)
	{
		keys[i].justReleased = false;
		keys[i].justPressed = false;
	}

	mouseButtonsState[0].justPressed = false;
	mouseButtonsState[1].justPressed = false;
}

void InputManager::Reset()
{
	mouseButtonsState[0].justReleased = false;
	mouseButtonsState[1].justReleased = false;
}

void InputManager::LoadInputMappings(FileManager* fileManager, const std::string& path)
{
	// Not finished
	/*std::ifstream file = fileManager->OpenForReading(path);

	if (file.is_open())
	{

	}
	else
	{*/
	InputMapping horizontal = {};
	horizontal.positiveKey = Keys::KEY_D;
	horizontal.negativeKey = Keys::KEY_A;
	horizontal.useLeftAnalogueStickX = true;

	InputMapping vertical = {};
	vertical.positiveKey = Keys::KEY_W;
	vertical.negativeKey = Keys::KEY_S;
	vertical.useLeftAnalogueStickY = true;

	InputMapping fire = {};
	fire.mouseButton = MouseButtonType::Left;
	fire.positiveVitaButton = VitaButtons::VITA_RTRIGGER;

	inputMappings["Horizontal"] = horizontal;
	inputMappings["Vertical"] = vertical;
	inputMappings["Fire"] = fire;
	//}
}

int InputManager::AnyKeyPressed() const
{
	for (int i = 0; i < 512; i++)
	{
		if (keys[i].justPressed)
			return i;
	}
	return 0;
}

bool InputManager::GetLastChar(unsigned char& c)
{
	c = lastChar;

	if (charUpdated)
	{
		charUpdated = false;
		return true;
	}

	return false;
}

Keys InputManager::GetLastKeyPressed() const
{
	return lastKeyPressed;
}

bool InputManager::IsKeyPressed(int keycode) const
{
	return keys[keycode].state;
}

bool InputManager::WasKeyPressed(int keycode) const
{
	return keys[keycode].justPressed;
}

bool InputManager::WasKeyReleased(int keycode) const
{
	return keys[keycode].justReleased;
}

bool InputManager::WasMouseButtonReleased(int button)
{
	return mouseButtonsState[button].justReleased;
}

bool InputManager::MouseMoved() const
{
	return mouseMoved;
}

bool InputManager::IsMousePressed(int button) const
{
	return mouseButtonsState[button].justPressed;
}

bool InputManager::IsMouseButtonDown(int button) const
{
	return mouseButtonsState[button].state;
}

void InputManager::SetMousePosition(const glm::vec2& pos)
{
	mousePosition = pos;
	mouseMoved = true;
}

void InputManager::UpdateKeys(int key, int scancode, int action, int mods)
{
	if (key >= 0 && key < 512)
	{
		if (action == KEY_PRESSED)
		{
			lastKeyPressed = (Keys)key;
			keys[key].state = true;
			keys[key].justReleased = false;
			keys[key].justPressed = true;
		}
		else if (action == KEY_RELEASED)
		{
			keys[key].state = false;
			keys[key].justReleased = true;
			keys[key].justPressed = false;
		}
	}
}

void InputManager::UpdateChar(unsigned char c)
{
	lastChar = c;
	charUpdated = true;
}

void InputManager::SetMouseButtonState(int button, int action)
{
	if (button < 0 || button > 1)
		return;

	if (button == MOUSE_BUTTON_LEFT)
	{
		if (action == KEY_PRESSED)
		{
			mouseButtonsState[0].state = true;
			mouseButtonsState[0].justReleased = false;
			mouseButtonsState[0].justPressed = true;
		}
		else if (action == KEY_RELEASED)
		{
			mouseButtonsState[0].state = false;
			mouseButtonsState[0].justReleased = true;
			mouseButtonsState[0].justPressed = false;
		}
	}
	else if (button == MOUSE_BUTTON_RIGHT)
	{
		if (action == KEY_PRESSED)
		{
			mouseButtonsState[1].state = true;
			mouseButtonsState[1].justReleased = false;
			mouseButtonsState[1].justPressed = true;
		}
		else if (action == KEY_RELEASED)
		{
			mouseButtonsState[1].state = false;
			mouseButtonsState[1].justReleased = true;
			mouseButtonsState[1].justPressed = false;
		}
	}
}

void InputManager::SetScrollWheelYOffset(float yoffset)
{
	scrollWheelY += yoffset;
}

void InputManager::UpdateVitaButtons(int buttons)
{
	lastButtons = this->buttons;
	this->buttons = buttons;
}

void InputManager::UpdateVitaSticks(unsigned char leftStickX, unsigned char leftStickY, unsigned char rightStickX, unsigned char rightStickY)
{
	this->leftStickX = (float)leftStickX;
	this->leftStickX = (this->leftStickX - 128.0f) / 128.0f;

	this->leftStickY = (float)leftStickY;
	this->leftStickY = (this->leftStickY - 128.0f) / 128.0f;
	this->leftStickY *= -1.0f;			// Flip the Y to -1 on the bottom and 1 on the top because the analogue stick is -1 on the top and 1 on the bottom

	this->rightStickX = (float)rightStickX;
	this->rightStickX = (this->rightStickX - 128.0f) / 128.0f;

	this->rightStickY = (float)rightStickY;
	this->rightStickY = (this->rightStickY - 128.0f) / 128.0f;
}

bool InputManager::IsVitaButtonDown(int button)
{
	return buttons & button;
}

bool InputManager::WasVitaButtonReleased(int button)
{
	if ((lastButtons & button) == true && (buttons & button) == false)
		return true;

	return false;
}

float InputManager::GetAxis(const std::string& name)
{
	// If using strings as the key becomes slows, then we could try ints
	//unsigned int id = SID(name);

	const InputMapping& im = inputMappings[name];

#ifdef VITA
	if (im.useLeftAnalogueStickX && (leftStickX > 0.1f || leftStickX < -0.1f))
		return leftStickX;

	if (im.useLeftAnalogueStickY && (leftStickY > 0.1f || leftStickY < -0.1f))
		return leftStickY;
#else
	if (IsKeyPressed(im.positiveKey))
		return 1.0f;

	if (IsKeyPressed(im.negativeKey))
		return -1.0f;
#endif	
	return 0.0f;
}

bool InputManager::GetAction(const std::string& name)
{
	const InputMapping& im = inputMappings[name];

#ifdef VITA
	if (buttons & im.positiveVitaButton)
		return true;
#else
	if (IsKeyPressed(im.positiveKey))
		return true;
#endif
	return false;
}

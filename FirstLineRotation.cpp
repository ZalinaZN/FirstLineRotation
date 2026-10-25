#include "framework.h"
#include "FirstLineRotation.h"
#include <cmath>            

//  Типы данных для однородных координат и матричных преобразований
struct Vector
{
	double x;   // координата X
	double y;   // координата Y
	double w;   // однородная координата
};

// Матрица 3×3 (преобразование плоскости)
struct Matrix
{
	double element[3][3];
};

//  Умножение вектор-строки на матрицу: result = vector * matrix
Vector MultiplyVectorByMatrix(const Vector& vector, const Matrix& matrix)
{
	Vector result;

	
	result.x = vector.x * matrix.element[0][0]
		+ vector.y * matrix.element[1][0]
		+ vector.w * matrix.element[2][0];

	result.y = vector.x * matrix.element[0][1]
		+ vector.y * matrix.element[1][1]
		+ vector.w * matrix.element[2][1];

	result.w = vector.x * matrix.element[0][2]
		+ vector.y * matrix.element[1][2]
		+ vector.w * matrix.element[2][2];

	return result;
}

//  Умножение матриц: result = first * second
Matrix MultiplyMatrixByMatrix(const Matrix& first, const Matrix& second)
{
	Matrix result = {};   // нулевая матрица

	for (int row = 0; row < 3; ++row)
		for (int column = 0; column < 3; ++column)
			for (int sharedIndex = 0; sharedIndex < 3; ++sharedIndex)
				result.element[row][column] +=
				first.element[row][sharedIndex] * second.element[sharedIndex][column];

	return result;
}

//  Единичная матрица 3×3
Matrix CreateIdentityMatrix()
{
	Matrix result = {};
	result.element[0][0] = 1.0;
	result.element[1][1] = 1.0;
	result.element[2][2] = 1.0;
	return result;
}

//  Матрица переноса T(dx, dy)
//  (для вектор-строки смещение)
Matrix CreateTranslationMatrix(double shiftX, double shiftY)
{
	return { 1,0,0,0,1,0,shiftX,  shiftY ,1 };
}

//  Параметры вращения
const UINT   TIMER_INTERVAL_MS = 23;      // ~40 кадров/с
const double ROTATION_STEP_DEGREES = 1.0;     // шаг поворота Δφ
const double PI_VALUE = 3.14159265358979323846;
const double INITIAL_P1_X = 250, INITIAL_P1_Y = 250;
const double INITIAL_P2_X = 450, INITIAL_P2_Y = 250;
const COLORREF LINE_COLOR = RGB(0, 0, 200);   // цвет отрезка

//  Глобальные переменные
HINSTANCE hInst;                     // текущий экземпляр приложения
WCHAR szTitle[MAX_LOADSTRING];       // строка заголовка окна
WCHAR szWindowClass[MAX_LOADSTRING]; // имя класса окна
Matrix rotationTransform = { 1,0,0, 0,1,0, 0,0,1 };

// Состояние отрезка 
Vector rotationCenterP1 = { INITIAL_P1_X, INITIAL_P1_Y, 1.0 };   //  точка P1 (центр вращения)
Vector movingPointP2 = { INITIAL_P2_X, INITIAL_P2_Y, 1.0 };      // текущее положение точки P2

// Матрица поворота, собранная один раз при нажатии "Rotation" 
Matrix CreateRotationMatrix(double angleRadians)
{
	double cos = std::cos(angleRadians);
	double sin = std::sin(angleRadians);
	return { cos,-sin,0, sin,cos,0, 0,0,1 };
}

//  Состояние таймера 
bool isRotationRunning = false;

//  Объявления функций
ATOM                MyRegisterClass(HINSTANCE);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

// Вспомогательные функции рисования
void DrawSegment(HDC hdc, const Vector& p1, const Vector& p2, COLORREF color);
void DrawCenterMarker(HDC hdc, const Vector& center);

//  Рисование отрезка заданным цветом
void DrawSegment(HDC hdc, const Vector& p1, const Vector& p2, COLORREF color)
{
	HPEN pen = CreatePen(PS_SOLID, 3, color);
	HPEN oldPen = (HPEN)SelectObject(hdc, pen);
	MoveToEx(hdc, (int)p1.x, (int)p1.y, nullptr);
	LineTo(hdc, (int)p2.x, (int)p2.y);
	SelectObject(hdc, oldPen);
	DeleteObject(pen);
}

//  Точка входа
int WINAPI wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ PWSTR pCmdLine,
	_In_ int nCmdShow
)
{
	// Загрузка строковых ресурсов
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_FIRSTLINEROTATION, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_FIRSTLINEROTATION));

	MSG windowMessage;
	while (GetMessage(&windowMessage, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(windowMessage.hwnd, hAccelTable, &windowMessage))
		{
			TranslateMessage(&windowMessage);
			DispatchMessage(&windowMessage);
		}
	}

	return (int)windowMessage.wParam;
}

//  Регистрация класса окна
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex = { sizeof(wcex) };
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_FIRSTLINEROTATION));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_FIRSTLINEROTATION);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

//  Создание главного окна
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance;
	// WS_CLIPCHILDREN — чтобы рисование линии не накладывалось на кнопки
	HWND hWnd = CreateWindowW(szWindowClass, szTitle,
		WS_OVERLAPPED | WS_CLIPCHILDREN,
		CW_USEDEFAULT, 0, 500, 600,
		nullptr, nullptr, hInstance, nullptr);

	if (!hWnd) return FALSE;
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	return TRUE;
}

//  Обработчик сообщений главного окна
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
		CreateWindowW(L"BUTTON", L"Rotation",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			10, 10, 120, 30,
			hWnd, (HMENU)IDC_BUTTON_ROTATION, hInst, nullptr);

		CreateWindowW(L"BUTTON", L"Cancel",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			140, 10, 120, 30,
			hWnd, (HMENU)IDC_BUTTON_CANCEL, hInst, nullptr);
		return 0;

	case WM_COMMAND:
		switch (LOWORD(wParam))          
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			return 0;

		case IDM_EXIT:
		case IDC_BUTTON_CANCEL:          
			DestroyWindow(hWnd);
			return 0;

		case IDC_BUTTON_ROTATION:
		{
			double angleRadians = ROTATION_STEP_DEGREES * PI_VALUE / 180.0;
			rotationTransform = MultiplyMatrixByMatrix(
				MultiplyMatrixByMatrix(
					CreateTranslationMatrix(-rotationCenterP1.x, -rotationCenterP1.y),
					CreateRotationMatrix(angleRadians)),
				CreateTranslationMatrix(rotationCenterP1.x, rotationCenterP1.y));
			SetTimer(hWnd, TIMER_ID, TIMER_INTERVAL_MS, nullptr);
			return 0;
		}
		}
		break;

	case WM_TIMER:
		if (wParam == TIMER_ID)
		{
			HDC hdc = GetDC(hWnd);

			// 1. Стереть текущий отрезок, нарисовав его цветом фона
			DrawSegment(hdc, rotationCenterP1, movingPointP2,
				GetSysColor(COLOR_WINDOW));

			// 2. Повернуть P2 на Δφ:  P2'' = P2' · T(-x1,-y1) · R(Δφ) · T(x1,y1)
			movingPointP2 = MultiplyVectorByMatrix(movingPointP2, rotationTransform);

			// 3. Нарисовать в новом положении
			DrawSegment(hdc, rotationCenterP1, movingPointP2, LINE_COLOR);
			ReleaseDC(hWnd, hdc);
		}
		return 0;

	case WM_PAINT:
	{
		PAINTSTRUCT paintStruct;
		HDC hdc = BeginPaint(hWnd, &paintStruct);
		DrawSegment(hdc, rotationCenterP1, movingPointP2, LINE_COLOR);
		EndPaint(hWnd, &paintStruct);
		return 0;
	}

	case WM_DESTROY:
		KillTimer(hWnd, TIMER_ID);       
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}
//  Диалог "О программе" 
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
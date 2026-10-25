#include "framework.h"
#include "FirstLineRotation.h"
#include <cmath>            

//  Типы данных для однородных координат и матричных преобразований
// Вектор в однородных координатах: (x, y, w)
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

	// Каждая компонента результата — сумма произведений элемента вектора
	// на соответствующий столбец матрицы.
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
	Matrix result = CreateIdentityMatrix();
	result.element[2][0] = shiftX;
	result.element[2][1] = shiftY;
	return result;
}

//  Матрица поворота R(φ), φ в радианах
//  (для вектор-строки: x' = x·cosφ − y·sinφ,  y' = x·sinφ + y·cosφ)
Matrix CreateRotationMatrix(double angleRadians)
{
	Matrix result = CreateIdentityMatrix();

	const double cos = std::cos(angleRadians);
	const double sin = std::sin(angleRadians);

	result.element[0][0] = cos;
	result.element[0][1] = -sin;
	result.element[1][0] = sin;
	result.element[1][1] = cos;

	return result;
}
//  Параметры вращения
const UINT   ROTATION_TIMER_INTERVAL_MS = 23;      // ~40 кадров/с
const double ROTATION_ANGLE_DEGREES = 1.0;     // шаг поворота Δφ
const double PI_VALUE = 3.14159265358979323846;

//  Цвета
const COLORREF LINE_COLOR = RGB(0, 0, 200);   // цвет отрезка
const COLORREF CENTER_MARKER_COLOR = RGB(0, 0, 200);   // цвет метки P1

//  Глобальные переменные
HINSTANCE hInst;                     // текущий экземпляр приложения
WCHAR szTitle[MAX_LOADSTRING];       // строка заголовка окна
WCHAR szWindowClass[MAX_LOADSTRING]; // имя класса окна

// Состояние отрезка 
Vector rotationCenterP1;   // неподвижная точка P1 (центр вращения)
Vector movingPointP2;      // текущее положение точки P2

// Матрица поворота, собранная один раз при нажатии "Rotation" 
Matrix rotationTransform;

//  Состояние таймера 
bool isRotationRunning = false;

//  Объявления функций
ATOM                MyRegisterClass(HINSTANCE hInstance);
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
	HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));

	MoveToEx(hdc, static_cast<int>(p1.x), static_cast<int>(p1.y), nullptr);
	LineTo(hdc, static_cast<int>(p2.x), static_cast<int>(p2.y));

	SelectObject(hdc, oldPen);
	DeleteObject(pen);
}

//  Рисование метки центра вращения 
void DrawCenterMarker(HDC hdc, const Vector& center)
{
	const int radius = 3;

	HBRUSH brush = CreateSolidBrush(CENTER_MARKER_COLOR);
	HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, brush));

	Ellipse(hdc,
		static_cast<int>(center.x) - radius,
		static_cast<int>(center.y) - radius,
		static_cast<int>(center.x) + radius,
		static_cast<int>(center.y) + radius);

	SelectObject(hdc, oldBrush);
	DeleteObject(brush);
}

//  Точка входа
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// Загрузка строковых ресурсов
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_FIRSTLINEROTATION, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_FIRSTLINEROTATION));

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}

//  Регистрация класса окна
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
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
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, 0, 500, 600,
		nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

//  Обработчик сообщений главного окна
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		//  WM_CREATE: инициализация отрезка и создание кнопок
	case WM_CREATE:
	{
		// Начальное положение отрезкаP1 = (250,250), P2 = (400,350)
		rotationCenterP1.x = 250.0;
		rotationCenterP1.y = 250.0;
		rotationCenterP1.w = 1.0;

		movingPointP2.x = 400.0;
		movingPointP2.y = 350.0;
		movingPointP2.w = 1.0;

		// Пока матрица — единичная (композиция будет собрана при нажатии "Rotation")
		rotationTransform = CreateIdentityMatrix();

		// Кнопка "Rotation"
		CreateWindowW(L"BUTTON", L"Rotation",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			10, 10, 120, 30,
			hWnd, (HMENU)IDC_BUTTON_ROTATION, hInst, nullptr);

		// Кнопка "Cancel"
		CreateWindowW(L"BUTTON", L"Cancel",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			140, 10, 120, 30,
			hWnd, (HMENU)IDC_BUTTON_CANCEL, hInst, nullptr);

		return 0;
	}

	//  WM_COMMAND: обработка меню и кнопок
	case WM_COMMAND:
	{
		int controlId = LOWORD(wParam);
		switch (controlId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			return 0;

		case IDM_EXIT:
			DestroyWindow(hWnd);
			return 0;

			//  Кнопка "Rotation" — собрать композицию матриц и стартовать таймер
		case IDC_BUTTON_ROTATION:
		{
			// Δφ в радианах
			// Δφ переводим в радианы, т.к. cos/sin работают с радианами.
			const double angleRadians = ROTATION_ANGLE_DEGREES * PI_VALUE / 180.0;

			// Отдельные матрицы композиции поворота вокруг P1:

			//   T(-x1, -y1) — перенос P1 в начало координат
			//   R(Δφ)       — поворот на малый угол
			//   T( x1,  y1) — возврат начала координат в P1
			Matrix translateToOrigin = CreateTranslationMatrix(-rotationCenterP1.x, -rotationCenterP1.y);
			Matrix rotateByDeltaPhi = CreateRotationMatrix(angleRadians);
			Matrix translateBackToCenter = CreateTranslationMatrix(rotationCenterP1.x, rotationCenterP1.y);

			// Композиция: M = T(-x1,-y1) · R(Δφ) · T(x1,y1)
			Matrix temp = MultiplyMatrixByMatrix(translateToOrigin, rotateByDeltaPhi);
			rotationTransform = MultiplyMatrixByMatrix(temp, translateBackToCenter);

			// Запуск таймера только при первом нажатии
			if (!isRotationRunning)
			{
				isRotationRunning = true;
				SetTimer(hWnd, ROTATION_TIMER_ID, ROTATION_TIMER_INTERVAL_MS, nullptr);
			}
			return 0;
		}

		//  Кнопка "Cancel" — завершение программы
		case IDC_BUTTON_CANCEL:
			DestroyWindow(hWnd);
			return 0;
		}
	}
	break;

	//  WM_TIMER: шаг анимации
	case WM_TIMER:
	{
		if (wParam == ROTATION_TIMER_ID && isRotationRunning)
		{
			HDC hdc = GetDC(hWnd);

			// 1. Стереть текущий отрезок, нарисовав его цветом фона
			DrawSegment(hdc, rotationCenterP1, movingPointP2,
				GetSysColor(COLOR_WINDOW));

			// 2. Повернуть точку P2 на малый угол:
			//    P2'' = P2' · T(-x1,-y1) · R(Δφ) · T(x1,y1)
			movingPointP2 = MultiplyVectorByMatrix(movingPointP2, rotationTransform);

			// 3. Нарисовать отрезок в новом положении + обновить метку P1
			DrawSegment(hdc, rotationCenterP1, movingPointP2, LINE_COLOR);
			DrawCenterMarker(hdc, rotationCenterP1);

			ReleaseDC(hWnd, hdc);
		}
		return 0;
	}

	//  WM_PAINT: перерисовать текущее состояние отрезка
	case WM_PAINT:
	{
		PAINTSTRUCT paintStruct;
		HDC hdc = BeginPaint(hWnd, &paintStruct);

		DrawSegment(hdc, rotationCenterP1, movingPointP2, LINE_COLOR);
		DrawCenterMarker(hdc, rotationCenterP1);

		EndPaint(hWnd, &paintStruct);
		return 0;
	}

	//  WM_DESTROY: остановить таймер и выйти
	case WM_DESTROY:
		if (isRotationRunning)
		{
			KillTimer(hWnd, ROTATION_TIMER_ID);
			isRotationRunning = false;
		}
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
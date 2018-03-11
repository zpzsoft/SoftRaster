/* -------------------------------------------------------------------------------------------------------------------
* Description   :	C++实现的软件光栅化渲染器
*
* Update        :	2018-02-07 => 初步定义好结构.
*					2018-02-14 => 初步把想象中的代码实现完, 等待调试.
*					2018-02-22 => MVP矩阵计算调试通过, 绘制基本的点和直线.
*					2018-02-23 => 实现了ScanLine绘制出完整的三角形面积.
*					2018-02-27 => 实现了zbuffer的支持.
*					2018-03-07 => 实现了简单裁剪及纹理映射.
*
* Author        :	Ryan Zheng
* Detail        :	在win32窗体下实现简单的软件光栅化渲染器.√
*				1,	实现单个像素的绘制.√
*				2,	实现对像素缓冲区的封装.√
*				3,	实现向量, 矩阵的定义及运算支持(向量的运算的封装).√
*				4,	绘制支线函数的实现.√
*				5,	绘制三角形面积的实现.√
*				6,	zbuffer实现和绘制立方体.√
*				7,	空间裁剪.√
*				8,	纹理映射.√
*				9,	简单光照×
* Quote			:
				1, Perspective Texture Mapping : http://chrishecker.com/Miscellaneous_Technical_Articles
				2, About clipping1 : https://fgiesen.wordpress.com/2011/07/05/a-trip-through-the-graphics-pipeline-2011-part-5/
				3, About clipping2 : http://gad.qq.com/article/detail/35738
*
* ----------------------------------------------------------------------------------------------------------------- */
#include <windows.h>
#include <math.h>
#include <vector>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define PIX_BITS 24

#pragma region Color & Matrix4 & Vector4 & Camera & Device

enum DRAW_TYPE
{
	DRAW_POINT = 1,
	DRAW_LINE,
	DRAW_TRIANGLE,
};

class Color
{
public:
	int r, g, b;
public:
	Color(int _r, int _g, int _b) { r = _r; g = _g; b = _b; }

	static Color Black() { return (*(new Color(0, 0, 0))); }
};

class Matrix4
{
public:
	float mm[4][4];
public:
	Matrix4(int value = 0) { memset(mm, value, 16 * sizeof(float)); mm[0][0] = mm[1][1] = mm[2][2] = mm[3][3] = 1; }

	Matrix4 operator*(const Matrix4& ma)
	{
		Matrix4 ret;
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				ret.mm[j][i] = mm[j][0] * ma.mm[0][i] + mm[j][1] * ma.mm[1][i] + mm[j][2] * ma.mm[2][i] + mm[j][3] * ma.mm[3][i];
			}
		}

		return ret;
	}

	void operator=(const Matrix4& ma)
	{
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				mm[i][j] = ma.mm[i][j];
			}
		}
	}

	void SetColumn(int colIndex, float x, float y, float z, float w)
	{
		mm[0][colIndex] = x;
		mm[1][colIndex] = y;
		mm[2][colIndex] = z;
		mm[3][colIndex] = w;
	}

	void SetRow(int rowIndex, float x, float y, float z, float w)
	{
		mm[rowIndex][0] = x;
		mm[rowIndex][1] = y;
		mm[rowIndex][2] = z;
		mm[rowIndex][3] = w;
	}

	void Translate(float dX, float dY, float dZ)
	{
		mm[3][0] += dX;
		mm[3][1] += dY;
		mm[3][2] += dZ;
	}

	/* usually in this order: first Y, then Z, then X(but not necessarily)
	https://msdn.microsoft.com/en-us/library/windows/desktop/bb206269(v=vs.85).aspx */
	void Rotate(float xAngle, float yAngle, float zAngle)
	{
		Matrix4 mat;

		//rotate Y
		mat.SetRow(0, mm[0][0] * cos(yAngle) + mm[0][2]* sin(yAngle), mm[0][1], -mm[0][0]*sin(yAngle) + mm[0][2]*cos(yAngle), mm[0][3]);
		mat.SetRow(1, mm[1][0] * cos(yAngle) + mm[1][2] * sin(yAngle), mm[1][1], -mm[0][0] * sin(yAngle) + mm[1][2] * cos(yAngle), mm[1][3]);
		mat.SetRow(2, mm[2][0] * cos(yAngle) + mm[2][2] * sin(yAngle), mm[2][1], -mm[0][0] * sin(yAngle) + mm[2][2] * cos(yAngle), mm[2][3]);
		mat.SetRow(3, mm[3][0] * cos(yAngle) + mm[3][2] * sin(yAngle), mm[3][1], -mm[0][0] * sin(yAngle) + mm[3][2] * cos(yAngle), mm[3][3]);
		*this = mat; 
		mat.Identity();

		//rotate Z
		mat.SetRow(0, mm[0][0] * cos(zAngle) - mm[0][1]*sin(zAngle), mm[0][0]*sin(zAngle) + mm[0][1]*cos(zAngle), mm[0][2], mm[0][3]);
		mat.SetRow(1, mm[1][0] * cos(zAngle) - mm[1][1] * sin(zAngle), mm[1][0] * sin(zAngle) + mm[1][1] * cos(zAngle), mm[1][2], mm[1][3]);
		mat.SetRow(2, mm[2][0] * cos(zAngle) - mm[2][1] * sin(zAngle), mm[2][0] * sin(zAngle) + mm[2][1] * cos(zAngle), mm[2][2], mm[2][3]);
		mat.SetRow(3, mm[3][0] * cos(zAngle) - mm[3][1] * sin(zAngle), mm[3][0] * sin(zAngle) + mm[3][1] * cos(zAngle), mm[3][2], mm[3][3]);
		*this = mat;
		mat.Identity();

		//rotate X
		mat.SetRow(0, mm[0][0], mm[0][1] * cos(xAngle) - mm[0][2] * sin(xAngle), mm[0][1] * sin(xAngle) + mm[0][2] * cos(xAngle), mm[0][3]);
		mat.SetRow(1, mm[1][0], mm[1][1] * cos(xAngle) - mm[1][2] * sin(xAngle), mm[1][1] * sin(xAngle) + mm[1][2] * cos(xAngle), mm[1][3]);
		mat.SetRow(2, mm[2][0], mm[2][1] * cos(xAngle) - mm[2][2] * sin(xAngle), mm[2][1] * sin(xAngle) + mm[2][2] * cos(xAngle), mm[2][3]);
		mat.SetRow(3, mm[3][0], mm[3][1] * cos(xAngle) - mm[3][2] * sin(xAngle), mm[3][1] * sin(xAngle) + mm[3][2] * cos(xAngle), mm[3][3]);
		*this = mat;
		mat.Identity();

	}

	void Scale(float sX, float sY, float sZ)
	{
		mm[0][0] *= sX;
		mm[0][1] *= sX;
		mm[0][2] *= sX;
		mm[0][3] *= sX;

		mm[1][0] *= sY;
		mm[1][1] *= sY;
		mm[1][2] *= sY;
		mm[1][3] *= sY;

		mm[2][0] *= sZ;
		mm[2][1] *= sZ;
		mm[2][2] *= sZ;
		mm[2][3] *= sZ;
	}

	void Identity()
	{
		mm[0][0] = mm[1][1] = mm[2][2] = mm[3][3] = 1.0f;
		mm[0][1] = mm[0][2] = mm[0][2] = 0;
		mm[1][0] = mm[1][2] = mm[1][3] = 0;
		mm[2][0] = mm[2][1] = mm[2][3] = 0;
	}
};

class Vector4
{
public:
	float x, y, z, w, u, v;
public:
	Vector4(float _x = 0, float _y = 0, float _z = 0, float _w = 1.0f) { x = _x; y = _y; z = _z; w = _w; }

	static Vector4 New(float _x = 0, float _y = 0, float _z = 0, float _w = 1.0f)
	{
		Vector4 vec(_x, _y, _z, _w);

		return vec;
	}

	Vector4 operator+(const Vector4& vec)
	{
		Vector4 ret;
		ret.x = x + vec.x;
		ret.y = y + vec.y;
		ret.z = z + vec.z;
		ret.w = 1;

		return ret;
	}

	Vector4 operator-(Vector4& vec)
	{
		Vector4 ret;
		ret.x = x - vec.x;
		ret.y = y - vec.y;
		ret.z = z - vec.z;

		return ret;
	}

	Vector4 operator*(Matrix4& matrix)
	{
		Vector4 ret;
		ret.x = x*matrix.mm[0][0] + y*matrix.mm[1][0] + z * matrix.mm[2][0] + w * matrix.mm[3][0];
		ret.y = x*matrix.mm[0][1] + y*matrix.mm[1][1] + z * matrix.mm[2][1] + w * matrix.mm[3][1];
		ret.z = x*matrix.mm[0][2] + y*matrix.mm[1][2] + z * matrix.mm[2][2] + w * matrix.mm[3][2];
		ret.w = x*matrix.mm[0][3] + y*matrix.mm[1][3] + z * matrix.mm[2][3] + w * matrix.mm[3][3];

		return ret;
	}

	Vector4 Normalize()
	{
		Vector4 ret;
		float length = sqrt(pow(x, 2) + pow(y, 2) + pow(z, 2));
		ret.x = x / length;
		ret.y = y / length;
		ret.z = z / length;

		return ret;
	}

	float Length()
	{
		return sqrt(x * x + y * y + z * z);
	}

	/* https://www.mathsisfun.com/algebra/vectors-dot-product.html */
	static float Dot(const Vector4& vecA, const Vector4& vecB)
	{
		return vecA.x*vecB.x + vecA.y* vecB.y + vecA.z*vecB.z;
	}

	/* https://www.mathsisfun.com/algebra/vectors-cross-product.html */
	static Vector4 Cross(const Vector4& vecA, const Vector4& vecB)
	{
		Vector4 ret;
		ret.x = vecA.y*vecB.z - vecA.z*vecB.y;
		ret.y = vecA.z*vecB.x - vecA.x*vecB.z;
		ret.z = vecA.x*vecB.y - vecA.y*vecB.x;
		ret.w = 1.0;

		return ret;
	}

	static float Angle(Vector4& vecA, Vector4& vecB)
	{
		return acos(Dot(vecA, vecB) / (vecA.Length() * vecB.Length()));
	}
};

class Rect
{
public:
	float _x1, _y1, _x2, _y2;
	Rect(float x1 = 0 , float y1 = 0, float x2 = 0, float y2 = 0) { _x1 = x1; _y1 = y1; _x2 = x2; _y2 = y2; }
	bool InRegion(float x, float y)
	{
		return x >= _x1 && x <= _x2 && y >= _y1 && y >= _y2;
	}
};

class Math
{
public:
	static float Clamp(float value, float min = 0, float max = 1)
	{
		return max(min, min(value, max));
	}

	static float Interpolate(float min, float max, float gradient)
	{
		return min + (max - min) * Clamp(gradient);
	}
};

class Transform
{
public:
	DRAW_TYPE type;
	Matrix4 worldMatrix;
	Matrix4 viewMatrix;
	Matrix4 projectionMatrix;
	std::vector<float> verticeList;
	std::vector<int> indiceList;
public:
	void SetVertices(float* vertices, int count)
	{
		for (int i = 0; i < count; i++)
			verticeList.push_back(vertices[i]);
	}

	void SetIndices(int* indices, int count)
	{
		for (int i = 0; i < count; i++)
			indiceList.push_back(indices[i]);
	}

	Matrix4 WorldViewProjection()
	{
		return worldMatrix * viewMatrix * projectionMatrix;
	}
};

/*
http://www.opengl-tutorial.org/cn/beginners-tutorials/tutorial-3-matrices/
http://www.songho.ca/opengl/gl_projectionMatrix4.html
https://www.scratchapixel.com/lessons/3d-basic-rendering/perspective-and-orthographic-projection-Matrix4/opengl-perspective-projection-Matrix4
http://www.songho.ca/opengl/gl_transform.html
https://www.cnblogs.com/graphics/archive/2012/07/12/2476413.html */
class Camera
{
private:
	Vector4 mPosition;
	Vector4 mTarget;
	Vector4 mUp;
public:
	//Get view Matrix4.
	Matrix4 LookAt(const Vector4& position, const Vector4& target, const Vector4& up)
	{
		Matrix4 matrix;
		mPosition = position;
		mTarget = target;
		mUp = up;

		//TODO:这里的顺序需要求证.
		Vector4 z = mTarget - mPosition;
		z = z.Normalize();

		Vector4 x = Vector4::Cross(up, z);
		x = x.Normalize();

		Vector4 y = Vector4::Cross(z, x);
		y = y.Normalize();
		 
		float xValue = -Vector4::Dot(position, x);
		float yValue = -Vector4::Dot(position, y);
		float zValue = -Vector4::Dot(position, z);

		matrix.SetRow(0, x.x, y.x, z.x, 0);
		matrix.SetRow(1, x.y, y.y, z.y, 0);
		matrix.SetRow(2, x.z, y.z, z.z, 0);
		matrix.SetRow(3, xValue, yValue, zValue, 1);

		return matrix;
	}

	/* Get projection Matrix4.
	http://www.songho.ca/opengl/gl_projectionMatrix4.html */
	Matrix4 Perspective(float angle, float aspect, float nearClip, float farClip)
	{
		Matrix4 m;

		//Right Hand
		float n = nearClip;
		float f = farClip;
		float t = n * tan(angle / 2);
		float r = t * aspect;

		m.SetRow(0, n / r, 0, 0, 0);
		m.SetRow(1, 0, n / t, 0, 0);
		m.SetRow(2, 0, 0, (f) / (f - n), 1);
		m.SetRow(3, 0, 0, -f * n / (f - n), 0);

		return m;
	}
};

class Device
{
private:
	int mWidth;
	int mHeight;
	int mTextureWidth;
	int mTextureHeight;
	BYTE* mBuf = NULL;
	float* mZBuf = NULL;
	unsigned int* mTexture = NULL;
	BITMAPINFO* mBitmapInfo = NULL;
	HDC mScreenHDC;
	HDC mCompatibleDC;
	HBITMAP mOldBitmap;
	HBITMAP mCompatibleBitmap;
	Matrix4 mViewProjection;

public:
	Device(HWND hwnd, int width = SCREEN_WIDTH, int height = SCREEN_HEIGHT)
	{
		mWidth = width;
		mHeight = height;
		mBuf = new BYTE[mWidth * mHeight * PIX_BITS / 8];
		memset(mBuf, 45, mWidth * mHeight * PIX_BITS / 8);
		mZBuf = new float[mWidth * mHeight];
		for (int i = 0; i < mWidth*mHeight; i++) mZBuf[i] = (float)INT_MAX;

		mBitmapInfo = new BITMAPINFO();
		ZeroMemory(mBitmapInfo, sizeof(BITMAPINFO));

		mBitmapInfo->bmiHeader.biBitCount = PIX_BITS;
		mBitmapInfo->bmiHeader.biCompression = BI_RGB;
		mBitmapInfo->bmiHeader.biHeight = -mHeight;
		mBitmapInfo->bmiHeader.biPlanes = 1;
		mBitmapInfo->bmiHeader.biSizeImage = 0;
		mBitmapInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		mBitmapInfo->bmiHeader.biWidth = mWidth;

		mScreenHDC = GetDC(hwnd);
		mCompatibleDC = CreateCompatibleDC(mScreenHDC);
		mCompatibleBitmap = CreateCompatibleBitmap(mScreenHDC, mWidth, mHeight);
		mOldBitmap = (HBITMAP)SelectObject(mCompatibleDC, mCompatibleBitmap);
	}

	/* 
	https://stackoverflow.com/questions/3792481/how-to-get-screen-coordinates-from-a-3d-point-opengl
	https://stackoverflow.com/questions/724219/how-to-convert-a-3d-point-into-2d-perspective-projection/866749#866749 */
	Vector4 GetScreenPos(Transform& transform, Vector4& worldPos)
	{
		Vector4 pos = worldPos * transform.WorldViewProjection();
		bool inClipping = CheckInclipping(pos);
		pos.x = (pos.x /pos.w + 1)*mWidth/2;
		pos.y = (1 - pos.y / pos.w)*mHeight/2;
		pos.z = pos.z / pos.w;
		pos.w = inClipping ? 1.0f : -1.0f;

		return pos;
	}

	bool CheckInclipping(Vector4 pos)
	{
		if (pos.z < 0) return false;
		if (pos.z > pos.w) return false;
		if (pos.x < -pos.w) return false;
		if (pos.x > pos.w) return false;
		if (pos.y < -pos.w) return false;
		if (pos.y > pos.w) return false;

		return true;
	}

	void DrawArrays(Transform& transform)
	{
		std::vector<Vector4> screenPoints;
		std::vector<Vector4> trianglePoints;
		for (int i = 0; i < transform.indiceList.size(); i++)
		{
			Vector4 worldPos;
			int vericeIndex = transform.indiceList[i];

			worldPos.x = transform.verticeList[vericeIndex * 3];
			worldPos.y = transform.verticeList[vericeIndex * 3 + 1];
			worldPos.z = transform.verticeList[vericeIndex * 3 + 2];
			worldPos = GetScreenPos(transform, worldPos);

			screenPoints.push_back(worldPos);
		}

		//计算面片的UV.
		for (int i = 0; i < screenPoints.size()/4; i++)
		{
			Vector4 pt1 = screenPoints[i*4];
			Vector4 pt2 = screenPoints[i*4 + 1];
			Vector4 pt3 = screenPoints[i*4 + 2];
			Vector4 pt4 = screenPoints[i*4 + 3];

			pt1.u = 0; pt1.v = 0;
			pt2.u = 1; pt2.v = 0;
			pt3.u = 1; pt3.v = 1;
			pt4.u = 0; pt4.v = 1;

			trianglePoints.push_back(pt1);
			trianglePoints.push_back(pt2);
			trianglePoints.push_back(pt3);
			trianglePoints.push_back(pt1);
			trianglePoints.push_back(pt3);
			trianglePoints.push_back(pt4);
		}

		switch (transform.type)
		{
		case DRAW_POINT:
			for (int i = 0; i < trianglePoints.size(); i++)
			{
				if (trianglePoints[i].w < 0) continue;;
				SetPiexel(trianglePoints[i].x, trianglePoints[i].y, trianglePoints[i].z, Color::Black());
			}
			break;
		case DRAW_LINE:
			for (int i = 0; i < trianglePoints.size(); i++)
			{
				if (trianglePoints[i].w < 0) continue;
				if (trianglePoints[(i - 1 + trianglePoints.size()) % trianglePoints.size()].w < 0) continue;
				DrawLine(trianglePoints[(i - 1 + trianglePoints.size()) % trianglePoints.size()], trianglePoints[i], Color::Black());
			}
			break;
		case DRAW_TRIANGLE:
		{
			for (int i = 0; i < trianglePoints.size() / 3; i++)
			{
				//裁剪只做简单做丢弃
				if (trianglePoints[i * 3].w < 0) continue;
				if (trianglePoints[i * 3 + 1].w < 0) continue;
				if (trianglePoints[i * 3 + 2].w < 0) continue;

				DrawArea(trianglePoints[i * 3], trianglePoints[i * 3 + 1], trianglePoints[i * 3 + 2], Color::Black());
			}
			break;
		}
		default:
			break;
		}
	}

	void SetMatrix4(Matrix4& Matrix4)
	{
		mViewProjection = Matrix4;
	}

	void Paint()
	{
		SetDIBits(mScreenHDC, mCompatibleBitmap, 0, mHeight, mBuf, mBitmapInfo, DIB_RGB_COLORS);
		BitBlt(mScreenHDC, -1, -1, mWidth, mHeight, mCompatibleDC, 0, 0, SRCCOPY);
		memset(mBuf, 45, mWidth * mHeight * PIX_BITS / 8);
		for (int i = 0; i < mWidth*mHeight; i++) mZBuf[i] = (float)INT_MAX;
	}

	void InitTexture(int width, int height)
	{
		mTextureWidth = width;
		mTextureHeight = height;
		mTexture = new unsigned int[width * height];

		int i, j;
		for (j = 0; j < width; j++) {
			for (i = 0; i < height; i++) {
				int x = i / 32, y = j / 32;
				mTexture[j*width + i] = ((x + y) & 1) ? 0xffffff : 0x000000;
			}
		}
	}

	void GetTexturePixel(float u, float v, Color& outColor)
	{
		int xPos = mTextureWidth * u;
		int yPos = mTextureHeight * v;

		int value = mTexture[yPos * mTextureWidth + xPos];
		outColor.b = ((value & 0xf000) >> 12)*255/15;
		outColor.g = ((value & 0x0f00) >> 8) * 255 / 15;;
		outColor.r = ((value & 0x00f0) >> 4)*255/15;
	}

private:
	void SetPiexel(int x, int y, float z, const Color& color)
	{
		if (NULL == mBuf) return;
		if (x < 0 || y < 0) return;
		if (y * mWidth * 3 + x * 3 + 3 > mWidth * mHeight * PIX_BITS / 8) return;

		if (z < mZBuf[y * mWidth + x])
		{
			mZBuf[y * mWidth + x] = z;
			mBuf[y * mWidth * 3 + x * 3 + 1] = color.g;
			mBuf[y * mWidth * 3 + x * 3 + 2] = color.r;
			mBuf[y * mWidth * 3 + x * 3 + 3] = color.b;
		}
	}

	void DrawLine(Vector4 start, Vector4 end, Color color, bool readTexture = false)
	{
		Color pixelColor = color;
		if (start.x == end.x && start.y == end.y)
		{
			SetPiexel(start.x, start.y, start.z, pixelColor);
		}
		else if (start.x == end.x)
		{
			for (int y = min(start.y, end.y); y < max(start.y, end.y); y++)
			{
				if (readTexture)
				{
					float u = Math::Interpolate(start.u, end.u, (y - start.y) / (end.y - start.y));
					float v = Math::Interpolate(start.v, end.v, (y - start.y) / (end.y - start.y));
					GetTexturePixel(u, v, pixelColor);
				}
				SetPiexel(start.x, y, Math::Interpolate(start.z, end.z, (y - start.y) / (end.y - start.y)), pixelColor);
			}
		}
		else if (start.y == end.y)
		{
			for (int x = min(start.x, end.x); x < max(start.x, end.x); x++)
			{
				if (readTexture)
				{
					float u = Math::Interpolate(start.u, end.u, (x - start.x) / (end.x - start.x));
					float v = Math::Interpolate(start.v, end.v, (x - start.x) / (end.x - start.x));
					GetTexturePixel(u, v, pixelColor);
				}
				SetPiexel(x, start.y, Math::Interpolate(start.z, end.z, (x - start.x) / (end.x - start.x)), pixelColor);
			}
		}
		else
		{
			bool goX = abs(start.x - start.x) > abs(start.y - end.y);
			float slope = (start.y - end.y) / (start.x - end.x);
			int minValue = goX ? min(start.x, end.x) : min(start.y, end.y);
			int maxValue = goX ? max(start.x, end.x) : max(start.y, end.y);
			
			
			for (int val = minValue; val < maxValue; val++)
			{
				if (readTexture)
				{
					float u = Math::Interpolate(start.u, end.u, (val - start.x) / (end.x - start.x));
					float v = Math::Interpolate(start.v, end.v, (val - start.x) / (end.x - start.x));
					GetTexturePixel(u, v, pixelColor);
				}

				if (goX)
					SetPiexel(val, slope * (val - start.x) + start.y, Math::Interpolate(start.z, end.z, (val - start.x) / (end.x - start.x)), pixelColor);
				else
					SetPiexel((val - start.y) / slope + start.x, val, Math::Interpolate(start.z, end.z, (val - start.y) / (end.y - start.y)), pixelColor);
			}
		}
	}

	void DrawArea(Vector4 point1, Vector4 point2, Vector4 point3, Color color)
	{
		int minX = min(point1.x, min(point2.x, point3.x)) - 0.5f;
		int maxX = max(point1.x, max(point2.x, point3.x)) + 0.5f;
		Vector4 startScan, endScan;

		for (int i = minX; i < maxX; i++)
		{
			if (ScanLineInX(point1, point2, point3, i, startScan, endScan))
				DrawLine(startScan, endScan, color, true);
		}
	}
	
	Vector4 OnLine(Vector4& start, Vector4& end, int x)
	{
		Vector4 point(0, 0, 0, -1);
		if (x < min(start.x, end.x) || x > max(start.x, end.x)) return point;
		if (start.x == end.x == x) return point;

		Vector4 leftPoint = start, rightPoint = end;

		if (leftPoint.x > rightPoint.x)
		{
			leftPoint = end;
			rightPoint = start;
		}
		float slope = (leftPoint.y - rightPoint.y) / (leftPoint.x - rightPoint.x);
		float rate = (x - leftPoint.x) / (rightPoint.x - leftPoint.x);
		point.x = x;
		point.y = slope * (x - rightPoint.x) + rightPoint.y;
		point.z = Math::Interpolate(leftPoint.z, rightPoint.z, rate);
		point.w = 1;
		point.u = Math::Interpolate(leftPoint.u, rightPoint.u, rate);
		point.v = Math::Interpolate(leftPoint.v, rightPoint.v, rate);

		return point;
	}

	//MinY in Vector4.x, MaxY in Vector4.y;
	//https://www.davrous.com/2013/06/21/tutorial-part-4-learning-how-to-write-a-3d-software-engine-in-c-ts-or-js-rasterization-z-buffering/
	bool ScanLineInX(Vector4& point1, Vector4& point2, Vector4& point3, int x, Vector4& scanStart, Vector4& scanEnd)
	{
		if (x < min(point1.x, min(point2.x, point3.x)) || x > max(point1.x, max(point2.x, point3.x))) 
			return  false;

		Vector4 line1Pt = OnLine(point1, point2, x);
		Vector4 line2Pt = OnLine(point2, point3, x);
		Vector4 line3Pt = OnLine(point3, point1, x);

		if (line1Pt.w < 0) { scanStart = line2Pt; scanEnd = line3Pt; }
		else if (line2Pt.w < 0) { scanStart = line3Pt; scanEnd = line1Pt; }
		else if (line3Pt.w < 0) { scanStart = line1Pt; scanEnd = line2Pt; }
		else
		{
			scanStart.x = scanEnd.x = x;
			scanStart.y = min(line1Pt.y, min(line2Pt.y, line3Pt.y));
			scanEnd.y = max(line1Pt.y, max(line2Pt.y, line3Pt.y));
		}
		
		return true;
	}
};
#pragma endregion

#pragma region Init Data
static float xMoveDelta = 0, yMoveDelta = 0, zMoveDelta = 0;
static float xRotateDelta = 0, yRotateDelta = 0, zRotateDelta = 0;
static Device* device = NULL;
static Transform transform;
static float verticeArray[] =
{
	0, 0, 0,
	1, 0, 0,
	1, 0, 1,
	0, 0, 1,
	0, 1, 0,
	1, 1, 0,
	1, 1, 1,
	0, 1, 1,
};

static int indiceArray[] =
{
	0, 1, 2, 3,
	//4, 5, 6, 7,
	//0, 1, 5, 4,
	//3, 2, 6, 7,
	//1, 2, 6, 5,
	0, 3, 7, 4,
};

#pragma endregion

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
	{
	}
	case WM_PAINT:
	{
		break;
	}
	case WM_CHAR:
	{
		char keyCode = (wParam);
		switch (keyCode)
		{
		case 'w':
			zMoveDelta += 0.1f;
		case 'a':
			yMoveDelta += 0.1f;
			break;
		case 's':
			zMoveDelta -= 0.1f;
		case 'd':
			yMoveDelta -= 0.1f;
			break;
		default:
			break;
		}
	}
	case WM_KEYDOWN:
	{
		switch (wParam)
		{
		case VK_LEFT:
			xRotateDelta += 0.05f;
			break;
		case VK_RIGHT:
			xRotateDelta -= 0.05f;
			break;
		case VK_UP:
			zRotateDelta += 0.05f;
			break;
		case VK_DOWN:
			zRotateDelta -= 0.05f;
			break;
		case VK_ESCAPE:
		{
			SendMessage(hwnd, WM_DESTROY, wParam, lParam);
			return 0;
		}
		default:
			break;
		}
		break;
	}
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		return 0;
	}
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR sZCmdLine, int iCmdShow)
{
	static TCHAR sZAppName[] = TEXT("SoftRaster");
	HWND hwnd;
	MSG msg;
	WNDCLASS wndclass;

	wndclass.style = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc = WndProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hInstance = hInstance;
	wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wndclass.lpszMenuName = NULL;
	wndclass.lpszClassName = sZAppName;

	if (!RegisterClass(&wndclass))
	{
		MessageBox(NULL, TEXT("This program requires Windows NT!"), sZAppName, MB_ICONERROR);
		return 0;
	}

	hwnd = CreateWindow(sZAppName, sZAppName, WS_OVERLAPPEDWINDOW, 
		0, GetSystemMetrics(SM_CYSCREEN) - SCREEN_HEIGHT,
		SCREEN_WIDTH, SCREEN_HEIGHT,NULL, NULL, hInstance, NULL);

	ShowWindow(hwnd, iCmdShow);
	UpdateWindow(hwnd);

	//Init Camera
	Camera camera;
	Matrix4 worldMatrix4;
	Matrix4 viewMatrix4 = camera.LookAt(Vector4::New(3.5, 0, 0), Vector4::New(0, 0, 0), Vector4::New(0, 0, 1));
	Matrix4 projectionMatrix4 = camera.Perspective(90 * 3.14159 / 180, (float)SCREEN_WIDTH / SCREEN_HEIGHT, 1, 100);

	device = new Device(hwnd);
	device->InitTexture(256, 256);
	transform.type = DRAW_TRIANGLE;
	transform.worldMatrix = worldMatrix4;
	transform.viewMatrix = viewMatrix4;
	transform.projectionMatrix = projectionMatrix4;
	transform.SetVertices(verticeArray, sizeof(verticeArray) / sizeof(float));
	transform.SetIndices(indiceArray, sizeof(indiceArray) / sizeof(int));

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		transform.worldMatrix.Translate(xMoveDelta, yMoveDelta, zMoveDelta);
		transform.worldMatrix.Rotate(xRotateDelta, yRotateDelta, zRotateDelta);
		device->DrawArrays(transform);
		device->Paint();
		xMoveDelta = yMoveDelta = zMoveDelta = 0;
		xRotateDelta = yRotateDelta = zRotateDelta = 0;
	}

	return 0;
}

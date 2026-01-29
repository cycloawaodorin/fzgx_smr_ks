#include <windows.h>
#include <cmath>
#include <thread>
#include <string>
#include <regex>
#include <format>
#include "output2.hpp"
#include "resource_definition.h"
#include "version.hpp"

bool func_output( OUTPUT_INFO *oip );
bool func_config( HWND hwnd, HINSTANCE hinst );
void load_config(const std::string &path);
void save_config(const std::string &path);
LPCWSTR func_get_config_text();
static std::wstring config_text;
void set_config_text();

constexpr static const DWORD YC48 = mmioFOURCC('Y', 'C', '4', '8');

static std::string
lw2str(const LPCWSTR lpcwstr)
{
	if (!lpcwstr) return {};
	int size_needed = WideCharToMultiByte(932, 0, lpcwstr, -1, nullptr, 0, nullptr, nullptr);
	if (size_needed <= 0) return {};
	std::string out(size_needed, 0);
	int written = WideCharToMultiByte(932, 0, lpcwstr, -1, out.data(), size_needed, nullptr, nullptr);
	if (written <= 0) return {};
	return out;
}

enum class Separator {
	SPACE=0,
	COMMA=1,
	TAB=2,
	NONE
};
static struct {
	int start_x;
	int start_y;
	bool preview;
	bool frame;
	int offset;
	Separator sep_idx;
	bool dialog;
	bool dialog_eval;
	float dialog_eval_limit;
	bool dialog_always;
	int num_th;
} config = {536, 413, true, true, 0, Separator::SPACE, true, true, 0.95f, false, 0};

constexpr static const TCHAR *auo_filename = "fzgx_smr_ks.auo2";
constexpr static const TCHAR *config_filename = "fzgx_smr_ks.config";
#define PLUGIN_NAME L"SMR for F-ZERO GX"
OUTPUT_PLUGIN_TABLE output_plugin_table = {
	0,
	PLUGIN_NAME,
	L"Text File (*.txt)\0*.txt\0CSV File (*.csv)\0*.csv\0All File (*.*)\0*.*\0",
	(PLUGIN_NAME L" " VERSION L" by KAZOON"),
	func_output,
	func_config,
	func_get_config_text
};

constexpr static const int width = 19;
constexpr static const int height = 26;
constexpr static const int window_width = 19*4;
constexpr static const int window_byte_width = 19*4*3;
static int dib_width;
constexpr static const char *separators = " ,\t";
constexpr static const char *digits = "0123456789 ";
static bool cancel;
static OUTPUT_INFO *oip=nullptr;
static int preview_frame=0;
static struct {
	bool cnn_low, unmatch;
} dialog_flags;
static char est_str[5]={0, 0, 0, 0, 0};
static std::size_t n_th=std::thread::hardware_concurrency();
constexpr static const DWORD PMAX = 1023;

template <class T>
static void
parallel_do(void (*f)(T*, const std::size_t&, const std::size_t&), T *p, const std::size_t &n)
{
	std::unique_ptr<std::thread[]> threads(new std::thread[n]);
	for (std::size_t i=0; i<n; i++) {
		threads[i] = std::thread(f, p, i, n);
	}
	for (std::size_t i=0; i<n; i++) {
		threads[i].join();
	}
}

class Cnn {
private:
#include "weights0.cpp"
	void
	conv0(const unsigned char *src)
	{
		for (std::size_t i=0; i<std::size(inter0); i++) {
			for (std::size_t j=0; j<std::size(inter0[i]); j++) {
				for (std::size_t k=0; k<std::size(inter0[i][j]); k++) {
					inter0[i][j][k] = Conv0B[k];
					for (std::size_t di=0; di<std::size(Conv0K); di++) {
						for (std::size_t dj=0; dj<std::size(Conv0K[di]); dj++) {
							for (std::size_t c=0; c<3; c++) {
								inter0[i][j][k] += static_cast<float>(src[(height-i-di)*dib_width+(j+dj)*3+2-static_cast<int>(c)])*Conv0K[di][dj][c][k];
							}
						}
					}
					if ( inter0[i][j][k] < 0.0f ) {
						inter0[i][j][k] = 0.0f;
					}
				}
			}
		}
	}
	void
	conv1()
	{
		for (std::size_t i=0; i<std::size(inter1); i++) {
			for (std::size_t j=0; j<std::size(inter1[i]); j++) {
				for (std::size_t k=0; k<std::size(inter1[i][j]); k++) {
					inter1[i][j][k] = Conv1B[k];
					for (std::size_t di=0; di<std::size(Conv1K); di++) {
						for (std::size_t dj=0; dj<std::size(Conv1K[di]); dj++) {
							for (std::size_t c=0; c<std::size(Conv1K[di][dj]); c++) {
								inter1[i][j][k] += inter0[i+di][j+dj][c]*Conv1K[di][dj][c][k];
							}
						}
					}
				}
			}
		}
	}
	void
	pooling()
	{
		for (std::size_t i=0; i<std::size(inter2); i++) {
			for (std::size_t j=0; j<std::size(inter2[i]); j++) {
				for (std::size_t k=0; k<std::size(inter2[i][j]); k++) {
					inter2[i][j][k] = 0.0f;
					for (std::size_t di=0; di<2; di++) {
						for (std::size_t dj=0; dj<2; dj++) {
							if ( inter2[i][j][k] < inter1[i*2+di][j*2+dj][k] ) {
								inter2[i][j][k] = inter1[i*2+di][j*2+dj][k];
							}
						}
					}
				}
			}
		}
	}
	void
	conv2()
	{
		for (std::size_t i=0; i<std::size(inter3); i++) {
			for (std::size_t j=0; j<std::size(inter3[i]); j++) {
				for (std::size_t k=0; k<std::size(inter3[i][j]); k++) {
					inter3[i][j][k] = Conv2B[k];
					for (std::size_t di=0; di<2; di++) {
						for (std::size_t dj=0; dj<2; dj++) {
							for (std::size_t c=0; c<std::size(Conv2K[di][dj]); c++) {
								inter3[i][j][k] += inter2[i*2+di][j*2+dj][c]*Conv2K[di][dj][c][k];
							}
						}
					}
					if ( inter3[i][j][k] < 0.0f ) {
						inter3[i][j][k] = 0.0f;
					}
				}
			}
		}
	}
	void
	dense0()
	{
		for (std::size_t i=0; i<std::size(Dense0B); i++) {
			inter4[i] = Dense0B[i];
			std::size_t j=0;
			for (std::size_t a=0; a<std::size(inter3); a++) {
				for (std::size_t b=0; b<std::size(inter3[a]); b++) {
					for (std::size_t c=0; c<std::size(inter3[a][b]); c++) {
						inter4[i] += inter3[a][b][c]*Dense0K[j++][i];
					}
				}
			}
			if ( inter4[i] < 0.0f ) {
				inter4[i] = 0.0f;
			}
		}
	}
	void
	dense1()
	{
		float sum = 0.0f;
		for (std::size_t j=0; j<11; j++) {
			output[j] = Dense1B[j];
			for (std::size_t k=0; k<std::size(Dense1K); k++) {
				output[j] += inter4[k]*Dense1K[k][j];
			}
			output[j] = std::exp(output[j]);
			sum += output[j];
		}
		for (auto& e : output) {
			e /= sum;
		}
	}
public:
	float output[11];
	void
	predict(const unsigned char *src)
	{
		conv0(src);
		conv1();
		pooling();
		conv2();
		dense0();
		dense1();
	}
};
class Dnn {
private:
#include "weights1.cpp"
	void
	conv0(const unsigned char *src)
	{
		for (std::size_t i=0; i<std::size(inter0); i++) {
			for (std::size_t j=0; j<std::size(inter0[i]); j++) {
				for (std::size_t k=0; k<std::size(inter0[i][j]); k++) {
					inter0[i][j][k] = Conv0B[k];
					for (std::size_t di=0; di<std::size(Conv0K); di++) {
						for (std::size_t dj=0; dj<std::size(Conv0K[di]); dj++) {
							for (std::size_t c=0; c<3; c++) {
								inter0[i][j][k] += static_cast<float>(src[(height-i-di*2)*dib_width+(j+dj)*3+2-static_cast<int>(c)])*Conv0K[di][dj][c][k];
							}
						}
					}
					if ( inter0[i][j][k] < 0.0f ) {
						inter0[i][j][k] = 0.0f;
					}
				}
			}
		}
	}
	void
	conv1()
	{
		for (std::size_t i=0; i<std::size(inter1); i++) {
			for (std::size_t j=0; j<std::size(inter1[i]); j++) {
				for (std::size_t k=0; k<std::size(inter1[i][j]); k++) {
					inter1[i][j][k] = Conv1B[k];
					for (std::size_t di=0; di<std::size(Conv1K); di++) {
						for (std::size_t dj=0; dj<std::size(Conv1K[di]); dj++) {
							for (std::size_t c=0; c<std::size(Conv1K[di][dj]); c++) {
								inter1[i][j][k] += inter0[i+di*2][j+dj][c]*Conv1K[di][dj][c][k];
							}
						}
					}
				}
			}
		}
	}
	void
	pooling()
	{
		for (std::size_t i=0; i<std::size(inter2); i++) {
			for (std::size_t j=0; j<std::size(inter2[i]); j++) {
				for (std::size_t k=0; k<std::size(inter2[i][j]); k++) {
					inter2[i][j][k] = 0.0f;
					for (std::size_t di=0; di<4; di++) {
						for (std::size_t dj=0; dj<4; dj++) {
							if ( inter2[i][j][k] < inter1[i*4+di][j*4+dj][k] ) {
								inter2[i][j][k] = inter1[i*4+di][j*4+dj][k];
							}
						}
					}
				}
			}
		}
	}
	void
	conv2()
	{
		for (std::size_t i=0; i<std::size(inter3); i++) {
			for (std::size_t j=0; j<std::size(inter3[i]); j++) {
				for (std::size_t k=0; k<std::size(inter3[i][j]); k++) {
					inter3[i][j][k] = Conv2B[k];
					for (std::size_t di=0; di<2; di++) {
						for (std::size_t dj=0; dj<2; dj++) {
							for (std::size_t c=0; c<std::size(Conv2K[di][dj]); c++) {
								inter3[i][j][k] += inter2[i*2+di][j*2+dj][c]*Conv2K[di][dj][c][k];
							}
						}
					}
					if ( inter3[i][j][k] < 0.0f ) {
						inter3[i][j][k] = 0.0f;
					}
				}
			}
		}
	}
	void
	dense0()
	{
		for (std::size_t i=0; i<std::size(Dense0B); i++) {
			inter4[i] = Dense0B[i];
			std::size_t j=0;
			for (std::size_t a=0; a<std::size(inter3); a++) {
				for (std::size_t b=0; b<std::size(inter3[a]); b++) {
					for (std::size_t c=0; c<std::size(inter3[a][b]); c++) {
						inter4[i] += inter3[a][b][c]*Dense0K[j++][i];
					}
				}
			}
			if ( inter4[i] < 0.0f ) {
				inter4[i] = 0.0f;
			}
		}
	}
	void
	dense1()
	{
		float sum = 0.0f;
		for (std::size_t j=0; j<11; j++) {
			output[j] = Dense1B[j];
			for (std::size_t k=0; k<std::size(Dense1K); k++) {
				output[j] += inter4[k]*Dense1K[k][j];
			}
			output[j] = std::exp(output[j]);
			sum += output[j];
		}
		for (auto& e : output) {
			e /= sum;
		}
	}
public:
	float output[11];
	void
	predict(const unsigned char *src)
	{
		conv0(src);
		conv1();
		pooling();
		conv2();
		dense0();
		dense1();
	}
};

class Nets {
public:
	const unsigned char *src;
	Cnn cnn[4];
	Dnn dnn[4];
	static void
	invoke(Nets *p, const std::size_t &i, const std::size_t &n)
	{
		const std::size_t start = (i*8)/n;
		const std::size_t end = ((i+1)*8)/n;
		for (std::size_t j=start; j<end; j++) {
			const std::size_t j_=j%4;
			const unsigned char *s = p->src + (j_*width*3);
			if (j<4) {
				p->cnn[j_].predict(s);
			} else {
				p->dnn[j_].predict(s);
			}
		}
	}
};
static std::unique_ptr<Nets> nn(new Nets());

static void
correct_values()
{
	if ( config.start_x < 0 ) {
		config.start_x = 0;
	} else if ( oip->w - window_width < config.start_x ) {
		config.start_x = oip->w - window_width;
	}
	if ( config.start_y < 0 ) {
		config.start_y = 0;
	} else if ( oip->h - height < config.start_y ) {
		config.start_y = oip->h - height;
	}
	if ( preview_frame < 0 ) {
		preview_frame = 0;
	} else if ( oip->n <= preview_frame ) {
		preview_frame = oip->n-1;
	}
}
// サイズに合わせていろいろ修正．出力不可なら true を返す．
static bool
check_video_size()
{
	if (oip->w < window_width || oip->h < height) {
		std::string str = std::format(
			"動画は{}x{}以上のサイズが必要です(given: {}x{})．\n出力を中止します．",
			window_width, height, oip->w, oip->h
		);
		MessageBox(GetActiveWindow(), str.c_str(), nullptr, MB_OK);
		return true;
	}
	correct_values();
	dib_width = (oip->w*3+3)&(~3);
	return false;
}
static void
set_bmp(unsigned char *bmp, const int &frame)
{
	const unsigned char *org = static_cast<unsigned char *>(oip->func_get_video(frame, YC48));
	correct_values();
	for (int y=0; y<height; y++) {
		memcpy(bmp+y*window_byte_width,
			org+(oip->h-config.start_y-height+y)*dib_width+config.start_x*3, window_byte_width);
	}
}
static LRESULT CALLBACK
func_preview_proc(HWND hdlg, UINT umsg, WPARAM wparam, LPARAM lparam)
{
	static HBITMAP hBitmap, hBitmapD;
	static unsigned char *bmp;
	if (umsg == WM_INITDIALOG) {
		std::string str = std::format("{}", config.start_x).c_str();
		SetDlgItemTextA(hdlg, IDC_X, str.c_str());
		str = std::format("{}", config.start_y);
		SetDlgItemTextA(hdlg, IDC_Y, str.c_str());
		str = std::format("{}", preview_frame);
		SetDlgItemTextA(hdlg, IDC_FRAME, str.c_str());
		hBitmap = LoadBitmap(GetModuleHandleA(auo_filename), "FFCK");
		BITMAPINFO bmi = {
			{sizeof(BITMAPINFOHEADER), window_width, height, 1, 24, BI_RGB, 0, 0, 0, 0, 0},
			{0, 0, 0, 0}
		};
		hBitmapD = CreateDIBSection(GetDC(hdlg), &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&bmp), nullptr, 0);
		return TRUE;
	} else if (umsg==WM_DESTROY) {
		preview_frame = 0;
		DeleteObject(hBitmap);
		DeleteObject(hBitmapD);
		return TRUE;
	} else if (umsg==WM_COMMAND) {
		std::string str(15, 0);
		WORD lwparam = LOWORD(wparam);
		if (lwparam == IDCANCEL) {
			cancel = true;
			EndDialog(hdlg, LOWORD(wparam));
		} else if (lwparam == IDOK) {
			EndDialog(hdlg, LOWORD(wparam));
		} else if (lwparam == IDC_X) {
			GetDlgItemTextA(hdlg, IDC_X, str.data(), static_cast<int>(str.size()));
			config.start_x = std::stoi(str);
			correct_values();
		} else if (lwparam == IDC_XLEFT) {
			config.start_x -= 1;
			correct_values();
			SetDlgItemTextA(hdlg, IDC_X, std::format("{}", config.start_x).c_str());
		} else if (lwparam == IDC_XRIGHT) {
			config.start_x += 1;
			correct_values();
			SetDlgItemTextA(hdlg, IDC_X, std::format("{}", config.start_x).c_str());
		} else if (lwparam == IDC_Y) {
			GetDlgItemTextA(hdlg, IDC_Y, str.data(), static_cast<int>(str.size()));
			config.start_y = std::stoi(str);
		} else if (lwparam == IDC_YLEFT) {
			config.start_y -= 1;
			correct_values();
			SetDlgItemTextA(hdlg, IDC_Y, std::format("{}", config.start_y).c_str());
		} else if (lwparam == IDC_YRIGHT) {
			config.start_y += 1;
			correct_values();
			SetDlgItemTextA(hdlg, IDC_Y, std::format("{}", config.start_y).c_str());
		} else if (lwparam == IDC_FRAME) {
			GetDlgItemTextA(hdlg, IDC_FRAME, str.data(), static_cast<int>(str.size()));
			preview_frame = std::stoi(str);
		} else if (lwparam == IDC_FLEFT) {
			preview_frame -= 1;
			correct_values();
			SetDlgItemTextA(hdlg, IDC_FRAME, std::format("{}", preview_frame).c_str());
		} else if (lwparam == IDC_FRIGHT) {
			preview_frame += 1;
			correct_values();
			SetDlgItemTextA(hdlg, IDC_FRAME, std::format("{}", preview_frame).c_str());
		} else {
			return FALSE;
		}
		InvalidateRect(hdlg, nullptr, TRUE);
		UpdateWindow(hdlg);
		return TRUE;
	} else if (umsg==WM_PAINT) {
		HDC hdc, hBuffer;
		PAINTSTRUCT ps;
		hdc = BeginPaint(hdlg, &ps);
		hBuffer = CreateCompatibleDC(hdc);
		SelectObject(hBuffer, hBitmap);
		BitBlt(hdc, 132, 10, window_width, height, hBuffer, 0, 0, SRCCOPY);
		SelectObject(hBuffer, hBitmapD);
		set_bmp(bmp, preview_frame);
		BitBlt(hdc, 132, 66, window_width, height, hBuffer, 0, 0, SRCCOPY);
		DeleteDC(hBuffer);
		EndPaint(hdlg, &ps);
	}
	return FALSE;
}

static void
set_estimates(const unsigned char *org)
{
	dialog_flags.cnn_low = false;
	dialog_flags.unmatch = false;
	nn->src = org+((oip->h-config.start_y-height)*dib_width+config.start_x*3);
	parallel_do(Nets::invoke, nn.get(), n_th);
	for (int i=0; i<4; i++) {
		int c_max_j=0, d_max_j=0, max_j=0;
		float c_max=nn->cnn[i].output[c_max_j], d_max=nn->dnn[i].output[d_max_j];
		float max=c_max*d_max;
		for (int j=1; j<11; j++) {
			float now = nn->cnn[i].output[j];
			if (c_max<now) {
				c_max_j = j;
				c_max = now;
			}
			now = nn->dnn[i].output[j];
			if (d_max<now) {
				d_max_j = j;
				d_max = now;
			}
			now *= nn->cnn[i].output[j];
			if (max<now) {
				max_j = j;
				max = now;
			}
		}
		if (config.dialog_eval && max < config.dialog_eval_limit) {
			dialog_flags.cnn_low = true;
		}
		if (c_max_j != d_max_j) {
			dialog_flags.unmatch = true;
		}
		est_str[i] = digits[max_j];
	}
}

static LRESULT CALLBACK
func_correct_proc(HWND hdlg, UINT umsg, WPARAM wparam, LPARAM lparam)
{
	static HBITMAP hBitmapD;
	static unsigned char *bmp;
	if (umsg == WM_INITDIALOG) {
		SetDlgItemTextA(hdlg, IDC_EDIT, est_str);
		BITMAPINFO bmi = {
			{sizeof(BITMAPINFOHEADER), window_width, height, 1, 24, BI_RGB, 0, 0, 0, 0, 0},
			{0, 0, 0, 0}
		};
		hBitmapD = CreateDIBSection(GetDC(hdlg), &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&bmp), nullptr, 0);
		return TRUE;
	} else if (umsg==WM_DESTROY) {
		DeleteObject(hBitmapD);
		return TRUE;
	} else if (umsg==WM_COMMAND) {
		WORD lwparam = LOWORD(wparam);
		if (lwparam == IDCANCEL ) {
			cancel = true;
			EndDialog(hdlg, LOWORD(wparam));
		} else if (lwparam == IDOK) {
			GetDlgItemTextA(hdlg, IDC_EDIT, est_str, 5);
			est_str[4] = 0;
			EndDialog(hdlg, LOWORD(wparam));
		} else {
			return FALSE;
		}
		InvalidateRect(hdlg, nullptr, TRUE);
		UpdateWindow(hdlg);
		return TRUE;
	} else if (umsg==WM_PAINT) {
		HDC hdc, hBuffer;
		PAINTSTRUCT ps;
		hdc = BeginPaint(hdlg, &ps);
		hBuffer = CreateCompatibleDC(hdc);
		SelectObject(hBuffer, hBitmapD);
		set_bmp(bmp, preview_frame);
		BitBlt(hdc, 26, 10, window_width, height, hBuffer, 0, 0, SRCCOPY);
		DeleteDC(hBuffer);
		EndPaint(hdlg, &ps);
	}
	return FALSE;
}

bool
func_output(OUTPUT_INFO *oip_org)
{
	oip = oip_org;
	if (check_video_size()) {
		return true;
	}
	
	cancel = true;
	if (config.preview) {
		DialogBoxA(GetModuleHandleA(auo_filename), "PREVIEW", GetActiveWindow(), reinterpret_cast<DLGPROC>(func_preview_proc));
	}
	if (cancel) {
		return true;
	}
	HANDLE fp;
	fp = CreateFileA(lw2str(oip->savefile).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if ( fp == INVALID_HANDLE_VALUE ) { return false; }
	for ( int i=0; i<oip->n; i++ ) {
		if (oip->func_is_abort()) { break; }
		oip->func_rest_time_disp(i, oip->n);
		DWORD dw;
		std::string str;
		set_estimates(static_cast<unsigned char *>(oip->func_get_video(i, YC48)));
		if (config.dialog) {
			if ( config.dialog_always || dialog_flags.unmatch || dialog_flags.cnn_low ) {
				preview_frame = i;
				DialogBoxA(GetModuleHandleA(auo_filename), "CORRECT", GetActiveWindow(), reinterpret_cast<DLGPROC>(func_correct_proc));
				if (cancel) {
					CloseHandle(fp); return true;
				}
			}
		}
		if (config.frame) {
			str = std::format("{}{:c}{:s}\n", i+config.offset, separators[static_cast<int>(config.sep_idx)], est_str);
		} else {
			str = std::format("{:s}\n", est_str);
		}
		WriteFile(fp, str.c_str(), static_cast<DWORD>(str.length()), &dw, nullptr);
	}
	CloseHandle(fp);
	return true;
}

// コンフィグ関係
static Separator sep_now=Separator::NONE;
static void
set_offset_enableness(HWND &hdlg, const LRESULT &val)
{
	BOOL v = static_cast<BOOL>(val);
	EnableWindow(GetDlgItem(hdlg, IDC_OFFSET), v);
	EnableWindow(GetDlgItem(hdlg, IDC_SPACE), v);
	EnableWindow(GetDlgItem(hdlg, IDC_COMMA), v);
	EnableWindow(GetDlgItem(hdlg, IDC_TAB), v);
}
static void
set_dialog_enableness(HWND &hdlg, const LRESULT &val)
{
	BOOL v = static_cast<BOOL>(val);
	EnableWindow(GetDlgItem(hdlg, IDC_DIALOG_EVAL), v);
	EnableWindow(GetDlgItem(hdlg, IDC_DIALOG_EVAL_LIM), v);
}
static void
set_dialog_enableness_ex(HWND &hdlg, const LRESULT &val, const LRESULT &val2)
{
	set_dialog_enableness(hdlg, val&&val2);
	EnableWindow(GetDlgItem(hdlg, IDC_DIALOG_ALWAYS), static_cast<BOOL>(val));
}
static void
init_dialog(HWND &hdlg)
{
	std::string str = std::format("{}", config.start_x);
	SetDlgItemTextA(hdlg, IDC_X, str.c_str());
	str = std::format("{}", config.start_y);
	SetDlgItemTextA(hdlg, IDC_Y, str.c_str());
	SendMessage(GetDlgItem(hdlg, IDC_PREVIEW), BM_SETCHECK, config.preview, 0);
	SendMessage(GetDlgItem(hdlg, IDC_FRAME), BM_SETCHECK, config.frame, 0);
	str = std::format("{}", config.offset);
	SetDlgItemTextA(hdlg, IDC_OFFSET, str.c_str());
	set_offset_enableness(hdlg, config.frame);
	sep_now = config.sep_idx;
	if ( sep_now == Separator::SPACE ) {
		SendMessage(GetDlgItem(hdlg, IDC_SPACE), BM_SETCHECK, TRUE, 0);
	} else if ( sep_now == Separator::COMMA ) {
		SendMessage(GetDlgItem(hdlg, IDC_COMMA), BM_SETCHECK, TRUE, 0);
	} else if ( sep_now == Separator::TAB ) {
		SendMessage(GetDlgItem(hdlg, IDC_TAB), BM_SETCHECK, TRUE, 0);
	}
	SendMessage(GetDlgItem(hdlg, IDC_DIALOG), BM_SETCHECK, config.dialog, 0);
	set_dialog_enableness_ex(hdlg, config.dialog, !config.dialog_always);
	SendMessage(GetDlgItem(hdlg, IDC_DIALOG_EVAL), BM_SETCHECK, config.dialog_eval, 0);
	int delim_dec = static_cast<int>(std::round((config.dialog_eval_limit)*1000.0f));
	int delim_int = delim_dec/1000;
	str = std::format("{}.{:03}", delim_int, delim_dec-(delim_int*1000));
	SetDlgItemTextA(hdlg, IDC_DIALOG_EVAL_LIM, str.c_str());
	EnableWindow(GetDlgItem(hdlg, IDC_DIALOG_EVAL_LIM), config.dialog_eval);
	SendMessage(GetDlgItem(hdlg, IDC_DIALOG_ALWAYS), BM_SETCHECK, config.dialog_always, 0);
	set_dialog_enableness(hdlg, !config.dialog_always);
	str = std::format("{}", config.num_th);
	SetDlgItemTextA(hdlg, IDC_NTH, str.c_str());
}
static void
n_th_correction()
{
	int nt = config.num_th;
	if ( nt <= 0 ) {
		nt += std::thread::hardware_concurrency();
		if ( nt <= 0 ) {
			nt = 1;
		}
	}
	n_th = static_cast<std::size_t>(nt);
}
static void
setup_config(HWND &hdlg)
{
	std::string str(15, 0);
	GetDlgItemTextA(hdlg, IDC_X, str.data(), static_cast<int>(str.size()));
	config.start_x = std::stoi(str);
	GetDlgItemTextA(hdlg, IDC_Y, str.data(), static_cast<int>(str.size()));
	config.start_y = std::stoi(str);
	config.preview = SendDlgItemMessage(hdlg, IDC_PREVIEW, BM_GETCHECK, 0, 0);
	config.frame = SendDlgItemMessage(hdlg, IDC_FRAME, BM_GETCHECK, 0, 0);
	GetDlgItemTextA(hdlg, IDC_OFFSET, str.data(), static_cast<int>(str.size()-1));
	config.offset = std::stoi(str);
	config.sep_idx = sep_now;
	config.dialog = SendDlgItemMessage(hdlg, IDC_DIALOG, BM_GETCHECK, 0, 0);
	config.dialog_eval = SendDlgItemMessage(hdlg, IDC_DIALOG_EVAL, BM_GETCHECK, 0, 0);
	GetDlgItemTextA(hdlg, IDC_DIALOG_EVAL_LIM, str.data(), static_cast<int>(str.size()));
	config.dialog_eval_limit = std::stof(str);
	config.dialog_always = SendDlgItemMessage(hdlg, IDC_DIALOG_ALWAYS, BM_GETCHECK, 0, 0);
	GetDlgItemTextA(hdlg, IDC_NTH, str.data(), static_cast<int>(str.size()));
	config.num_th = std::stoi(str);
	n_th_correction();
}
static LRESULT CALLBACK
func_config_proc(HWND hdlg, UINT umsg, WPARAM wparam, LPARAM lparam)
{
	if (umsg == WM_INITDIALOG) {
		init_dialog(hdlg);
		return TRUE;
	} else if (umsg==WM_DESTROY) {
		sep_now=Separator::NONE;
		return TRUE;
	} else if (umsg==WM_COMMAND) {
		WORD lwparam = LOWORD(wparam);
		if (lwparam == IDCANCEL ) {
			EndDialog(hdlg, LOWORD(wparam));
		} else if (lwparam == IDOK) {
			setup_config(hdlg);
			EndDialog(hdlg, LOWORD(wparam));
		} else if (lwparam == IDC_FRAME) {
			set_offset_enableness(hdlg, SendDlgItemMessage(hdlg, IDC_FRAME, BM_GETCHECK, 0, 0));
		} else if (lwparam == IDC_SPACE) {
			sep_now = Separator::SPACE;
		} else if (lwparam == IDC_COMMA) {
			sep_now = Separator::COMMA;
		} else if (lwparam == IDC_TAB) {
			sep_now = Separator::TAB;
		} else if (lwparam == IDC_DIALOG) {
			set_dialog_enableness_ex(hdlg,
				SendDlgItemMessage(hdlg, IDC_DIALOG, BM_GETCHECK, 0, 0),
				!SendDlgItemMessage(hdlg, IDC_DIALOG_ALWAYS, BM_GETCHECK, 0, 0));
		} else if (lwparam == IDC_DIALOG_EVAL) {
			EnableWindow(GetDlgItem(hdlg, IDC_DIALOG_EVAL_LIM),
				static_cast<BOOL>(SendDlgItemMessage(hdlg, IDC_DIALOG_EVAL, BM_GETCHECK, 0, 0)));
		} else if (lwparam == IDC_DIALOG_ALWAYS) {
			set_dialog_enableness(hdlg, !SendDlgItemMessage(hdlg, IDC_DIALOG_ALWAYS, BM_GETCHECK, 0, 0));
		}
		return TRUE;
	}
	return FALSE;
}
bool
func_config(HWND hwnd, HINSTANCE dll_hinst)
{
	TCHAR p_auo[PMAX+1];
	GetModuleFileNameA(GetModuleHandleA(auo_filename), p_auo, PMAX);
	std::string p_config = std::regex_replace(p_auo, std::regex(auo_filename), config_filename);
	
	load_config(p_config);
	DialogBoxA(dll_hinst, "CONFIG", hwnd, reinterpret_cast<DLGPROC>(func_config_proc));
	save_config(p_config);
	set_config_text();
	
	return true;
}

void
load_config(const std::string &path)
{
	HANDLE fp;
	fp = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if ( fp == INVALID_HANDLE_VALUE ) { return; }
	char buff[sizeof(config)+1];
	DWORD read;
	BOOL flg = ReadFile(fp, buff, sizeof(config), &read, nullptr);
	if ( flg && read==sizeof(config) ) {
		memcpy(&config, buff, read);
		n_th_correction();
	}
	CloseHandle(fp);
}
void
save_config(const std::string &path)
{
	HANDLE fp;
	fp = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if ( fp == INVALID_HANDLE_VALUE ) { return; }
	WriteFile(fp, &config, sizeof(config), nullptr, nullptr);
	CloseHandle(fp);
}

LPCWSTR
func_get_config_text()
{
	return config_text.c_str();
}
void
set_config_text()
{
	config_text = std::format(
		L"プレビュー：{} / 人力確認：{}",
		(config.preview ? L"オン" : L"オフ"), (config.dialog ? L"オン" : L"オフ")
	);
}

extern "C" {

bool
InitializePlugin(const DWORD version)
{
	TCHAR p_auo[PMAX+1];
	GetModuleFileNameA(GetModuleHandleA(auo_filename), p_auo, PMAX);
	std::string p_config = std::regex_replace(p_auo, std::regex(auo_filename), config_filename);
	
	load_config(p_config);
	set_config_text();
	return true;
}

void
UninitializePlugin()
{
	nn.reset(nullptr);
}

OUTPUT_PLUGIN_TABLE*
GetOutputPluginTable()
{
	return &output_plugin_table;
}

}

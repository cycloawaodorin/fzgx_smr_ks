#include <windows.h>
#include <cmath>
#include <iterator>
#include "output.h"
#include "resource_definition.h"
#include "version.h"

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
} config = {536, 413, true, true, 0, Separator::SPACE, true, true, 0.95f, false};

static const TCHAR *auo_filename = "fzgx_smr_ks.auo";
#define PLUGIN_NAME "SMR for F-ZERO GX"
static const char *filefilter = "Text File (*.txt)\0*.txt\0CSV File (*.csv)\0*.csv\0All File (*.*)\0*.*\0";
OUTPUT_PLUGIN_TABLE output_plugin_table = {
	0,
	const_cast<TCHAR *>(PLUGIN_NAME),
	const_cast<TCHAR *>(filefilter),
	const_cast<TCHAR *>(PLUGIN_NAME " " VERSION " by KAZOON"),
	func_init,
	func_exit,
	func_output,
	func_config,
	func_config_get,
	func_config_set,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

EXTERN_C OUTPUT_PLUGIN_TABLE __declspec(dllexport) * __stdcall
GetOutputPluginTable(void)
{
	return &output_plugin_table;
}

constexpr static const int width = 19;
constexpr static const int height = 26;
constexpr static const int window_width = 19*4;
constexpr static const int window_byte_width = 19*4*3;
static int dib_width;
static const char *separators = " ,\t";
static const char *digits = "0123456789 ";
static BOOL cancel;
static OUTPUT_INFO *oip=nullptr;
static int preview_frame=0;
static struct {
	bool cnn_low, unmatch;
} dialog_flags;
static char est_str[5]={0, 0, 0, 0, 0};

class Cnn {
private:
#include "weights0.cpp"
	void
	conv0(const unsigned char *src, int s)
	{
		for (std::size_t i=0; i<std::size(inter0); i++) {
			for (std::size_t j=0; j<std::size(inter0[i]); j++) {
				for (std::size_t k=0; k<std::size(inter0[i][j]); k++) {
					inter0[i][j][k] = Conv0B[k];
					for (std::size_t di=0; di<std::size(Conv0K); di++) {
						for (std::size_t dj=0; dj<std::size(Conv0K[di]); dj++) {
							for (std::size_t c=0; c<3; c++) {
								inter0[i][j][k] += static_cast<float>(src[(height-i-di)*s+(j+dj)*3+2-static_cast<int>(c)])*Conv0K[di][dj][c][k];
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
	dense1(int i)
	{
		float sum = 0.0f;
		for (std::size_t j=0; j<11; j++) {
			output[i][j] = Dense1B[j];
			for (std::size_t k=0; k<std::size(Dense1K); k++) {
				output[i][j] += inter4[k]*Dense1K[k][j];
			}
			output[i][j] = std::exp(output[i][j]);
			sum += output[i][j];
		}
		for (auto& e : output[i]) {
			e /= sum;
		}
	}
public:
	float output[4][11];
	void
	predict(const unsigned char *src, int dibw, int i)
	{
		conv0(src, dibw);
		conv1();
		pooling();
		conv2();
		dense0();
		dense1(i);
	}
};
static Cnn *cnn;
class Dnn {
private:
#include "weights1.cpp"
	void
	conv0(const unsigned char *src, int s)
	{
		for (std::size_t i=0; i<std::size(inter0); i++) {
			for (std::size_t j=0; j<std::size(inter0[i]); j++) {
				for (std::size_t k=0; k<std::size(inter0[i][j]); k++) {
					inter0[i][j][k] = Conv0B[k];
					for (std::size_t di=0; di<std::size(Conv0K); di++) {
						for (std::size_t dj=0; dj<std::size(Conv0K[di]); dj++) {
							for (std::size_t c=0; c<3; c++) {
								inter0[i][j][k] += static_cast<float>(src[(height-i-di*2)*s+(j+dj)*3+2-static_cast<int>(c)])*Conv0K[di][dj][c][k];
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
	dense1(int i)
	{
		float sum = 0.0f;
		for (std::size_t j=0; j<11; j++) {
			output[i][j] = Dense1B[j];
			for (std::size_t k=0; k<std::size(Dense1K); k++) {
				output[i][j] += inter4[k]*Dense1K[k][j];
			}
			output[i][j] = std::exp(output[i][j]);
			sum += output[i][j];
		}
		for (auto& e : output[i]) {
			e /= sum;
		}
	}
public:
	float output[4][11];
	void
	predict(const unsigned char *src, int dibw, int i)
	{
		conv0(src, dibw);
		conv1();
		pooling();
		conv2();
		dense0();
		dense1(i);
	}
};
static Dnn *dnn;

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
// サイズに合わせていろいろ修正．出力不可なら TRUE を返す．
static BOOL
check_video_size()
{
	char buf[256];
	if (oip->w < window_width || oip->h < height) {
		wsprintf(buf, "動画は%dx%d以上のサイズが必要です(given:%dx%d)．\n出力を中止します．", window_width, height, oip->w, oip->h);
		MessageBox(GetActiveWindow(), buf, NULL, MB_OK);
		return TRUE;
	}
	correct_values();
	dib_width = (oip->w*3+3)&(~3);
	return FALSE;
}
static void
set_bmp(unsigned char *bmp, int frame)
{
	const unsigned char *org = static_cast<unsigned char *>(oip->func_get_video(frame));
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
		char buf[16];
		wsprintf(buf, "%d", config.start_x);
		SetDlgItemText(hdlg, IDC_X, buf);
		wsprintf(buf, "%d", config.start_y);
		SetDlgItemText(hdlg, IDC_Y, buf);
		wsprintf(buf, "%d", preview_frame);
		SetDlgItemText(hdlg, IDC_FRAME, buf);
		hBitmap = LoadBitmap(GetModuleHandle(auo_filename), "FFCK");
		BITMAPINFO bmi = {
			{sizeof(BITMAPINFOHEADER), window_width, height, 1, 24, BI_RGB, 0, 0, 0, 0, 0},
			{0, 0, 0, 0}
		};
		hBitmapD = CreateDIBSection(GetDC(hdlg), &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&bmp), NULL, 0);
		return TRUE;
	} else if (umsg==WM_DESTROY) {
		preview_frame = 0;
		DeleteObject(hBitmap);
		DeleteObject(hBitmapD);
		return TRUE;
	} else if (umsg==WM_COMMAND) {
		char buf[16];
		WORD lwparam = LOWORD(wparam);
		if (lwparam == IDCANCEL ) {
			cancel = TRUE;
			EndDialog(hdlg, LOWORD(wparam));
		} else if (lwparam == IDOK) {
			EndDialog(hdlg, LOWORD(wparam));
		} else if (lwparam == IDC_X) {
			GetDlgItemText(hdlg, IDC_X, buf, sizeof(buf)-1);
			config.start_x = atoi(buf);
			correct_values();
		} else if (lwparam == IDC_XLEFT) {
			config.start_x -= 1;
			correct_values();
			wsprintf(buf, "%d", config.start_x);
			SetDlgItemText(hdlg, IDC_X, buf);
		} else if (lwparam == IDC_XRIGHT) {
			config.start_x += 1;
			correct_values();
			wsprintf(buf, "%d", config.start_x);
			SetDlgItemText(hdlg, IDC_X, buf);
		} else if (lwparam == IDC_Y) {
			GetDlgItemText(hdlg, IDC_Y, buf, sizeof(buf)-1);
			config.start_y = atoi(buf);
		} else if (lwparam == IDC_YLEFT) {
			config.start_y -= 1;
			correct_values();
			wsprintf(buf, "%d", config.start_y);
			SetDlgItemText(hdlg, IDC_Y, buf);
		} else if (lwparam == IDC_YRIGHT) {
			config.start_y += 1;
			correct_values();
			wsprintf(buf, "%d", config.start_y);
			SetDlgItemText(hdlg, IDC_Y, buf);
		} else if (lwparam == IDC_FRAME) {
			GetDlgItemText(hdlg, IDC_FRAME, buf, sizeof(buf)-1);
			preview_frame = atoi(buf);
		} else if (lwparam == IDC_FLEFT) {
			preview_frame -= 1;
			correct_values();
			wsprintf(buf, "%d", preview_frame);
			SetDlgItemText(hdlg, IDC_FRAME, buf);
		} else if (lwparam == IDC_FRIGHT) {
			preview_frame += 1;
			correct_values();
			wsprintf(buf, "%d", preview_frame);
			SetDlgItemText(hdlg, IDC_FRAME, buf);
		} else {
			return FALSE;
		}
		InvalidateRect(hdlg, NULL, TRUE);
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
	for (int i=0; i<4; i++) {
		const unsigned char *src = org+(oip->h-config.start_y-height)*dib_width+(config.start_x+i*width)*3;
		cnn->predict(src, dib_width, i);
		dnn->predict(src, dib_width, i);
		int c_max_j=0, d_max_j=0, max_j=0;
		float c_max=cnn->output[i][c_max_j], d_max=dnn->output[i][d_max_j];
		float max=c_max*d_max;
		for (int j=1; j<11; j++) {
			float now = cnn->output[i][j];
			if (c_max<now) {
				c_max_j = j;
				c_max = now;
			}
			now = dnn->output[i][j];
			if (d_max<now) {
				d_max_j = j;
				d_max = now;
			}
			now *= cnn->output[i][j];
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
		SetDlgItemText(hdlg, IDC_EDIT, est_str);
		BITMAPINFO bmi = {
			{sizeof(BITMAPINFOHEADER), window_width, height, 1, 24, BI_RGB, 0, 0, 0, 0, 0},
			{0, 0, 0, 0}
		};
		hBitmapD = CreateDIBSection(GetDC(hdlg), &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&bmp), NULL, 0);
		return TRUE;
	} else if (umsg==WM_DESTROY) {
		DeleteObject(hBitmapD);
		return TRUE;
	} else if (umsg==WM_COMMAND) {
		WORD lwparam = LOWORD(wparam);
		if (lwparam == IDCANCEL ) {
			cancel = TRUE;
			EndDialog(hdlg, LOWORD(wparam));
		} else if (lwparam == IDOK) {
			GetDlgItemText(hdlg, IDC_EDIT, est_str, 5);
			est_str[4] = 0;
			EndDialog(hdlg, LOWORD(wparam));
		} else {
			return FALSE;
		}
		InvalidateRect(hdlg, NULL, TRUE);
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

BOOL
func_init()
{
	cnn = new Cnn();
	if (cnn==nullptr) {
		return FALSE;
	}
	dnn = new Dnn();
	if (dnn==nullptr) {
		return FALSE;
	}
	return TRUE;
}

BOOL
func_exit()
{
	delete cnn;
	delete dnn;
	return TRUE;
}

BOOL
func_output(OUTPUT_INFO *oip_org)
{
	oip = oip_org;
	if (check_video_size()) {
		return TRUE;
	}
	
	cancel = FALSE;
	if (config.preview) {
		DialogBox(GetModuleHandle(auo_filename), "PREVIEW", GetActiveWindow(), reinterpret_cast<DLGPROC>(func_preview_proc));
	}
	if (cancel) {
		return TRUE;
	}
	HANDLE fp;
	fp = CreateFile(oip->savefile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if ( fp == INVALID_HANDLE_VALUE ) { return FALSE; }
	for ( int i=0; i<oip->n; i++ ) {
		if (oip->func_is_abort()) { break; }
		oip->func_rest_time_disp(i, oip->n);
		DWORD dw;
		char str[32];
		set_estimates(static_cast<unsigned char *>(oip->func_get_video(i)));
		if (config.dialog) {
			if ( config.dialog_always || dialog_flags.unmatch || dialog_flags.cnn_low ) {
				preview_frame = i;
				DialogBox(GetModuleHandle(auo_filename), "CORRECT", GetActiveWindow(), reinterpret_cast<DLGPROC>(func_correct_proc));
				if (cancel) {
					CloseHandle(fp); return TRUE;
				}
			}
		}
		if (config.frame) {
			wsprintf(str, "%d%c%s\n", i+config.offset, separators[static_cast<int>(config.sep_idx)], est_str);
		} else {
			wsprintf(str, "%s\n", est_str);
		}
		WriteFile(fp, str, strlen(str), &dw, NULL);
	}
	CloseHandle(fp);
	return TRUE;
}


// コンフィグ関係
static Separator sep_now=Separator::NONE;
static void set_offset_enableness(HWND hdlg, BOOL val) {
	EnableWindow(GetDlgItem(hdlg, IDC_OFFSET), val);
	EnableWindow(GetDlgItem(hdlg, IDC_SPACE), val);
	EnableWindow(GetDlgItem(hdlg, IDC_COMMA), val);
	EnableWindow(GetDlgItem(hdlg, IDC_TAB), val);
}
static void set_dialog_enableness(HWND hdlg, BOOL val) {
	EnableWindow(GetDlgItem(hdlg, IDC_DIALOG_EVAL), val);
	EnableWindow(GetDlgItem(hdlg, IDC_DIALOG_EVAL_LIM), val);
}
static void set_dialog_enableness_ex(HWND hdlg, BOOL val, BOOL val2) {
	set_dialog_enableness(hdlg, val&&val2);
	EnableWindow(GetDlgItem(hdlg, IDC_DIALOG_ALWAYS), val);
}
static void init_dialog(HWND hdlg) {
	TCHAR buf[16];
	wsprintf(buf, "%d", config.start_x);
	SetDlgItemText(hdlg, IDC_X, buf);
	wsprintf(buf, "%d", config.start_y);
	SetDlgItemText(hdlg, IDC_Y, buf);
	SendMessage(GetDlgItem(hdlg, IDC_PREVIEW), BM_SETCHECK, config.preview, 0);
	SendMessage(GetDlgItem(hdlg, IDC_FRAME), BM_SETCHECK, config.frame, 0);
	wsprintf(buf, "%d", config.offset);
	SetDlgItemText(hdlg, IDC_OFFSET, buf);
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
	wsprintf(buf, "%d.%03d", delim_int, delim_dec-(delim_int*1000));
	SetDlgItemText(hdlg, IDC_DIALOG_EVAL_LIM, buf);
	EnableWindow(GetDlgItem(hdlg, IDC_DIALOG_EVAL_LIM), config.dialog_eval);
	SendMessage(GetDlgItem(hdlg, IDC_DIALOG_ALWAYS), BM_SETCHECK, config.dialog_always, 0);
	set_dialog_enableness(hdlg, !config.dialog_always);
}
static void setup_config(HWND hdlg) {
	TCHAR buf[16];
	GetDlgItemText(hdlg, IDC_X, buf, sizeof(buf)-1);
	config.start_x = atoi(buf);
	GetDlgItemText(hdlg, IDC_Y, buf, sizeof(buf)-1);
	config.start_y = atoi(buf);
	config.preview = SendDlgItemMessage(hdlg, IDC_PREVIEW, BM_GETCHECK, 0, 0);
	config.frame = SendDlgItemMessage(hdlg, IDC_FRAME, BM_GETCHECK, 0, 0);
	GetDlgItemText(hdlg, IDC_OFFSET, buf, sizeof(buf)-1);
	config.offset = atoi(buf);
	config.sep_idx = sep_now;
	config.dialog = SendDlgItemMessage(hdlg, IDC_DIALOG, BM_GETCHECK, 0, 0);
	config.dialog_eval = SendDlgItemMessage(hdlg, IDC_DIALOG_EVAL, BM_GETCHECK, 0, 0);
	GetDlgItemText(hdlg, IDC_DIALOG_EVAL_LIM, buf, sizeof(buf)-1);
	config.dialog_eval_limit = static_cast<float>(atof(buf));
	config.dialog_always = SendDlgItemMessage(hdlg, IDC_DIALOG_ALWAYS, BM_GETCHECK, 0, 0);
}
static LRESULT CALLBACK func_config_proc(HWND hdlg, UINT umsg, WPARAM wparam, LPARAM lparam) {
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
			EnableWindow(GetDlgItem(hdlg, IDC_DIALOG_EVAL_LIM), SendDlgItemMessage(hdlg, IDC_DIALOG_EVAL, BM_GETCHECK, 0, 0));
		} else if (lwparam == IDC_DIALOG_ALWAYS) {
			set_dialog_enableness(hdlg, !SendDlgItemMessage(hdlg, IDC_DIALOG_ALWAYS, BM_GETCHECK, 0, 0));
		}
		return TRUE;
	}
	return FALSE;
}
BOOL func_config(HWND hwnd, HINSTANCE dll_hinst) {
	DialogBox(dll_hinst, "CONFIG", hwnd, reinterpret_cast<DLGPROC>(func_config_proc));
	return TRUE;
}
int func_config_get(void *data, int size) {
	if (data) {
		memcpy(data, &config, sizeof(config));
		return sizeof(config);
	}
	return 0;
}
int func_config_set(void *data, int size) {
	if (size != sizeof(config)) {
		return 0;
	}
	memcpy(&config, data, size);
	return size;
}

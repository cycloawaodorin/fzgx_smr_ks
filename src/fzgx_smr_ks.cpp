#include <Windows.h>
#include <cmath>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <exception>
#include <utility>
#include <format>
#include <fstream>
#include "output.hpp"
#include "resource_definition.h"
#include "version.hpp"

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

static std::wstring
Utf8ToUtf16(const std::string &utf8)
{
	if (utf8.empty()) return L"";
	int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
	std::wstring wstr(size - 1, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wstr.data(), size);
	return wstr;
}
static std::string
Utf16ToCp932(const std::wstring &utf16)
{
	if (utf16.empty()) return "";
	int size = WideCharToMultiByte(932, 0, utf16.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::string str(size - 1, '\0');
	WideCharToMultiByte(932, 0, utf16.c_str(), -1, str.data(), size, nullptr, nullptr);
	return str;
}
static std::string
Utf8ToCp932(const std::string &utf8)
{
	return Utf16ToCp932(Utf8ToUtf16(utf8));
}

static const std::wstring auo_filename = L"fzgx_smr_ks.auo";
#define PLUGIN_NAME "SMR for F-ZERO GX"

EXTERN_C OUTPUT_PLUGIN_TABLE *
GetOutputPluginTable()
{
	static CHAR filefilter[] = "Text File (*.txt)\0*.txt\0CSV File (*.csv)\0*.csv\0All File (*.*)\0*.*\0";
	static OUTPUT_PLUGIN_TABLE opt = {
		0,
		const_cast<LPSTR>(Utf8ToCp932(PLUGIN_NAME).c_str()),
		filefilter,
		const_cast<LPSTR>(Utf8ToCp932(PLUGIN_NAME " " VERSION " by KAZOON").c_str()),
		func_init,
		func_exit,
		func_output,
		func_config,
		func_config_get,
		func_config_set,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	};
	return &opt;
}

constexpr static const std::size_t width = 19uz;
constexpr static const std::size_t height = 26uz;
constexpr static const std::size_t window_width = 19uz*4uz;
constexpr static const std::size_t window_byte_width = 19uz*4uz*3uz;
static std::size_t dib_width;
constexpr static const char *separators = " ,\t";
constexpr static const char *digits = "0123456789 ";
static bool cancel;
static OUTPUT_INFO *oip=nullptr;
static int preview_frame=0;
static struct {
	bool cnn_low, unmatch;
} dialog_flags;
static char est_str[5]={0, 0, 0, 0, 0};
static std::size_t n_th=8uz;

class ThreadPool {
private:
	struct Thread {
		std::thread thread;
		std::mutex mx;
		std::condition_variable cv;
		bool ready=false;
	};
	std::size_t size;
	std::unique_ptr<Thread[]> threads;
	std::function<void(std::size_t)> func;
	std::exception_ptr ep;
	std::atomic<std::size_t> current_i=0uz;
	std::size_t max_i=0uz;
	bool alive=true;
	void
	listen(Thread *th)
	{
		while (alive) {
			{ // ジョブが来るまで待機
				auto lk=std::unique_lock(th->mx);
				th->cv.wait(lk, [th] { return th->ready; });
			}
			for ( std::size_t i=max_i; current_i<max_i; ) { // ジョブの取り出しと実行
				i = current_i++;
				try {
					if ( i < max_i ) {
						func(i);
					}
				} catch (...) { // func からの例外を捕捉
					ep = std::current_exception();
					current_i = max_i;
				}
			}
			{ // 全ジョブ完了
				auto lk=std::lock_guard(th->mx);
				th->ready = false;
			}
			th->cv.notify_one();
		}
	}
public:
	explicit ThreadPool(std::size_t s=std::thread::hardware_concurrency()) : size(s)
	{
		threads = std::make_unique<Thread[]>(size);
		for (auto i=0uz; i<size; i++) {
			threads[i].thread = std::thread([this, i](){listen(&threads[i]);});
		}
	}
	~ThreadPool()
	{
		{
			alive = false;
			for (auto i=0uz; i<size; i++) {
				{
					auto lk=std::lock_guard(threads[i].mx);
					threads[i].ready = true;
				}
				threads[i].cv.notify_one();
			}
		}
		for (auto i=0uz; i<size; i++) {
			threads[i].thread.join();
		}
	}
	void
	parallel_do(std::function<void(std::size_t)> f, std::size_t n)
	{
		func = f; // ジョブ関数
		current_i = 0; max_i = n;
		for (auto i=0uz; i<size; i++) { // ワーカー起動
			{
				auto lk=std::lock_guard(threads[i].mx);
				threads[i].ready = true;
			}
			threads[i].cv.notify_one();
		}
		for (auto i=0uz; i<size; i++) { // 全ワーカーの終了を待つ
			auto lk=std::unique_lock(threads[i].mx);
			threads[i].cv.wait(lk, [this, i]{ return !(threads[i].ready); });
		}
		func = nullptr;
		if ( ep ) {
			std::rethrow_exception(std::exchange(ep, nullptr));
		}
	}
	std::size_t
	get_size()
	{
		return size;
	}
};
static std::unique_ptr<ThreadPool> TP;

class Cnn {
private:
#include "weights0.cpp"
	void
	conv0(const unsigned char *src)
	{
		for (auto i=0uz; i<std::size(inter0); i++) {
			for (auto j=0uz; j<std::size(inter0[i]); j++) {
				for (auto k=0uz; k<std::size(inter0[i][j]); k++) {
					inter0[i][j][k] = Conv0B[k];
					for (auto di=0uz; di<std::size(Conv0K); di++) {
						for (auto dj=0uz; dj<std::size(Conv0K[di]); dj++) {
							for (auto c=0uz; c<3uz; c++) {
								inter0[i][j][k] = std::fmaf(src[(height-i-di)*dib_width+(j+dj)*3uz+2uz-c], Conv0K[di][dj][c][k], inter0[i][j][k]);
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
		for (auto i=0uz; i<std::size(inter1); i++) {
			for (auto j=0uz; j<std::size(inter1[i]); j++) {
				for (auto k=0uz; k<std::size(inter1[i][j]); k++) {
					inter1[i][j][k] = Conv1B[k];
					for (auto di=0uz; di<std::size(Conv1K); di++) {
						for (auto dj=0uz; dj<std::size(Conv1K[di]); dj++) {
							for (auto c=0uz; c<std::size(Conv1K[di][dj]); c++) {
								inter1[i][j][k] = std::fma(inter0[i+di][j+dj][c], Conv1K[di][dj][c][k], inter1[i][j][k]);
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
		for (auto i=0uz; i<std::size(inter2); i++) {
			for (auto j=0uz; j<std::size(inter2[i]); j++) {
				for (auto k=0uz; k<std::size(inter2[i][j]); k++) {
					inter2[i][j][k] = 0.0f;
					for (auto di=0uz; di<2uz; di++) {
						for (auto dj=0uz; dj<2uz; dj++) {
							if ( inter2[i][j][k] < inter1[i*2uz+di][j*2uz+dj][k] ) {
								inter2[i][j][k] = inter1[i*2uz+di][j*2uz+dj][k];
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
		for (auto i=0uz; i<std::size(inter3); i++) {
			for (auto j=0uz; j<std::size(inter3[i]); j++) {
				for (auto k=0uz; k<std::size(inter3[i][j]); k++) {
					inter3[i][j][k] = Conv2B[k];
					for (auto di=0uz; di<2uz; di++) {
						for (auto dj=0uz; dj<2uz; dj++) {
							for (auto c=0uz; c<std::size(Conv2K[di][dj]); c++) {
								inter3[i][j][k] = std::fma(inter2[i*2uz+di][j*2uz+dj][c], Conv2K[di][dj][c][k], inter3[i][j][k]);
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
		for (auto i=0uz; i<std::size(Dense0B); i++) {
			inter4[i] = Dense0B[i];
			std::size_t j=0uz;
			for (auto a=0uz; a<std::size(inter3); a++) {
				for (auto b=0uz; b<std::size(inter3[a]); b++) {
					for (auto c=0uz; c<std::size(inter3[a][b]); c++) {
						inter4[i] = std::fma(inter3[a][b][c], Dense0K[j++][i], inter4[i]);
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
		for (auto j=0uz; j<11uz; j++) {
			output[j] = Dense1B[j];
			for (auto k=0uz; k<std::size(Dense1K); k++) {
				output[j] = std::fma(inter4[k], Dense1K[k][j], output[j]);
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
		for (auto i=0uz; i<std::size(inter0); i++) {
			for (auto j=0uz; j<std::size(inter0[i]); j++) {
				for (auto k=0uz; k<std::size(inter0[i][j]); k++) {
					inter0[i][j][k] = Conv0B[k];
					for (auto di=0uz; di<std::size(Conv0K); di++) {
						for (auto dj=0uz; dj<std::size(Conv0K[di]); dj++) {
							for (auto c=0uz; c<3uz; c++) {
								inter0[i][j][k] = std::fmaf(src[(height-i-di*2uz)*dib_width+(j+dj)*3uz+2uz-c], Conv0K[di][dj][c][k], inter0[i][j][k]);
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
		for (auto i=0uz; i<std::size(inter1); i++) {
			for (auto j=0uz; j<std::size(inter1[i]); j++) {
				for (auto k=0uz; k<std::size(inter1[i][j]); k++) {
					inter1[i][j][k] = Conv1B[k];
					for (auto di=0uz; di<std::size(Conv1K); di++) {
						for (auto dj=0uz; dj<std::size(Conv1K[di]); dj++) {
							for (auto c=0uz; c<std::size(Conv1K[di][dj]); c++) {
								inter1[i][j][k] = std::fma(inter0[i+di*2uz][j+dj][c], Conv1K[di][dj][c][k], inter1[i][j][k]);
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
		for (auto i=0uz; i<std::size(inter2); i++) {
			for (auto j=0uz; j<std::size(inter2[i]); j++) {
				for (auto k=0uz; k<std::size(inter2[i][j]); k++) {
					inter2[i][j][k] = 0.0f;
					for (auto di=0uz; di<4uz; di++) {
						for (auto dj=0uz; dj<4uz; dj++) {
							if ( inter2[i][j][k] < inter1[i*4uz+di][j*4uz+dj][k] ) {
								inter2[i][j][k] = inter1[i*4uz+di][j*4uz+dj][k];
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
		for (auto i=0uz; i<std::size(inter3); i++) {
			for (auto j=0uz; j<std::size(inter3[i]); j++) {
				for (auto k=0uz; k<std::size(inter3[i][j]); k++) {
					inter3[i][j][k] = Conv2B[k];
					for (auto di=0uz; di<2uz; di++) {
						for (auto dj=0uz; dj<2uz; dj++) {
							for (auto c=0uz; c<std::size(Conv2K[di][dj]); c++) {
								inter3[i][j][k] = std::fma(inter2[i*2uz+di][j*2uz+dj][c], Conv2K[di][dj][c][k], inter3[i][j][k]);
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
		for (auto i=0uz; i<std::size(Dense0B); i++) {
			inter4[i] = Dense0B[i];
			auto j=0uz;
			for (auto a=0uz; a<std::size(inter3); a++) {
				for (auto b=0uz; b<std::size(inter3[a]); b++) {
					for (auto c=0uz; c<std::size(inter3[a][b]); c++) {
						inter4[i] = std::fma(inter3[a][b][c], Dense0K[j++][i], inter4[i]);
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
		for (auto j=0uz; j<11uz; j++) {
			output[j] = Dense1B[j];
			for (std::size_t k=0; k<std::size(Dense1K); k++) {
				output[j] = std::fma(inter4[k], Dense1K[k][j], output[j]);
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
	void
	invoke(const std::size_t i)
	{
		const std::size_t j=i%4uz;
		const unsigned char *s = &src[j*width*3uz];
		if (i<4) {
			cnn[j].predict(s);
		} else {
			dnn[j].predict(s);
		}
	}
};
static std::unique_ptr<Nets> nn;

static void
correct_values()
{
	if ( config.start_x < 0 ) {
		config.start_x = 0;
	} else if ( oip->w - static_cast<int>(window_width) < config.start_x ) {
		config.start_x = oip->w - static_cast<int>(window_width);
	}
	if ( config.start_y < 0 ) {
		config.start_y = 0;
	} else if ( oip->h - static_cast<int>(height) < config.start_y ) {
		config.start_y = oip->h - static_cast<int>(height);
	}
	if ( preview_frame < 0 ) {
		preview_frame = 0;
	} else if ( oip->n <= preview_frame ) {
		preview_frame = oip->n-1;
	}
}
// サイズに合わせていろいろ修正．出力不可なら TRUE を返す．
static bool
check_video_size()
{
	if (oip->w < static_cast<int>(window_width) || oip->h < static_cast<int>(height)) {
		std::wstring wstr = std::format(
			L"動画は{}x{}以上のサイズが必要です(given: {}x{})．\n出力を中止します．",
			window_width, height, oip->w, oip->h
		);
		MessageBoxW(GetActiveWindow(), wstr.c_str(), nullptr, MB_OK);
		return true;
	}
	correct_values();
	dib_width = (static_cast<std::size_t>(oip->w)*3uz+3uz)&(~3uz);
	return false;
}
static void
set_bmp(unsigned char *bmp, const int frame)
{
	const unsigned char *org = static_cast<unsigned char *>(oip->func_get_video(frame));
	correct_values();
	for (auto y=0uz; y<height; y++) {
		const auto yy = (static_cast<std::size_t>(oip->h-config.start_y)-height+y);
		memcpy(&bmp[y*window_byte_width],
			&org[yy*dib_width+static_cast<std::size_t>(config.start_x)*3uz], window_byte_width);
	}
}
static INT_PTR CALLBACK
func_preview_proc(HWND hdlg, UINT umsg, WPARAM wparam, LPARAM lparam)
{
	static HBITMAP hBitmap, hBitmapD;
	static unsigned char *bmp;
	if (umsg == WM_INITDIALOG) {
		std::wstring wstr = std::format(L"{}", config.start_x).c_str();
		SetDlgItemTextW(hdlg, IDC_X, wstr.c_str());
		wstr = std::format(L"{}", config.start_y);
		SetDlgItemTextW(hdlg, IDC_Y, wstr.c_str());
		wstr = std::format(L"{}", preview_frame);
		SetDlgItemTextW(hdlg, IDC_FRAME, wstr.c_str());
		hBitmap = LoadBitmap(GetModuleHandleW(auo_filename.c_str()), "FFCK");
		BITMAPINFO bmi = {
			{sizeof(BITMAPINFOHEADER), window_width, height, 1, 24, BI_RGB, 0, 0, 0, 0, 0},
			{{0, 0, 0, 0}}
		};
		hBitmapD = CreateDIBSection(GetDC(hdlg), &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&bmp), NULL, 0);
		return TRUE;
	} else if (umsg==WM_DESTROY) {
		preview_frame = 0;
		DeleteObject(hBitmap);
		DeleteObject(hBitmapD);
		return TRUE;
	} else if (umsg==WM_COMMAND) {
		std::wstring wstr(15, L'\0');
		WORD lwparam = LOWORD(wparam);
		if (lwparam == IDCANCEL ) {
			cancel = true;
			EndDialog(hdlg, LOWORD(wparam));
		} else if (lwparam == IDOK) {
			EndDialog(hdlg, LOWORD(wparam));
		} else if (lwparam == IDC_X) {
			GetDlgItemTextW(hdlg, IDC_X, wstr.data(), static_cast<int>(wstr.size()));
			config.start_x = std::stoi(wstr);
			correct_values();
		} else if (lwparam == IDC_XLEFT) {
			config.start_x -= 1;
			correct_values();
			SetDlgItemTextW(hdlg, IDC_X, std::format(L"{}", config.start_x).c_str());
		} else if (lwparam == IDC_XRIGHT) {
			config.start_x += 1;
			correct_values();
			SetDlgItemTextW(hdlg, IDC_X, std::format(L"{}", config.start_x).c_str());
		} else if (lwparam == IDC_Y) {
			GetDlgItemTextW(hdlg, IDC_Y, wstr.data(), static_cast<int>(wstr.size()));
			config.start_y = std::stoi(wstr);
			correct_values();
		} else if (lwparam == IDC_YLEFT) {
			config.start_y -= 1;
			correct_values();
			SetDlgItemTextW(hdlg, IDC_Y, std::format(L"{}", config.start_y).c_str());
		} else if (lwparam == IDC_YRIGHT) {
			config.start_y += 1;
			correct_values();
			SetDlgItemTextW(hdlg, IDC_Y, std::format(L"{}", config.start_y).c_str());
		} else if (lwparam == IDC_FRAME) {
			GetDlgItemTextW(hdlg, IDC_FRAME, wstr.data(), static_cast<int>(wstr.size()));
			preview_frame = std::stoi(wstr);
			correct_values();
		} else if (lwparam == IDC_FLEFT) {
			preview_frame -= 1;
			correct_values();
			SetDlgItemTextW(hdlg, IDC_FRAME, std::format(L"{}", preview_frame).c_str());
		} else if (lwparam == IDC_FRIGHT) {
			preview_frame += 1;
			correct_values();
			SetDlgItemTextW(hdlg, IDC_FRAME, std::format(L"{}", preview_frame).c_str());
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
	auto p = nn.get();
	TP->parallel_do([&p](std::size_t i){p->invoke(i);}, 8uz);
	for (auto i=0uz; i<4uz; i++) {
		std::size_t c_max_j=0uz, d_max_j=0uz, max_j=0uz;
		float c_max=nn->cnn[i].output[c_max_j], d_max=nn->dnn[i].output[d_max_j];
		float max=c_max*d_max;
		for (auto j=1uz; j<11uz; j++) {
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

static INT_PTR CALLBACK
func_correct_proc(HWND hdlg, UINT umsg, WPARAM wparam, LPARAM lparam)
{
	static HBITMAP hBitmapD;
	static unsigned char *bmp;
	if (umsg == WM_INITDIALOG) {
		SetDlgItemTextA(hdlg, IDC_EDIT, est_str);
		BITMAPINFO bmi = {
			{sizeof(BITMAPINFOHEADER), window_width, height, 1, 24, BI_RGB, 0, 0, 0, 0, 0},
			{{0, 0, 0, 0}}
		};
		hBitmapD = CreateDIBSection(GetDC(hdlg), &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&bmp), NULL, 0);
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
	TP = std::make_unique<ThreadPool>(n_th);
	nn = std::make_unique<Nets>();
	return TRUE;
}

BOOL
func_exit()
{
	nn.reset(nullptr);
	TP.reset(nullptr);
	return TRUE;
}

BOOL
func_output(OUTPUT_INFO *oip_org)
{
	oip = oip_org;
	if (check_video_size()) {
		return TRUE;
	}
	
	cancel = false;
	if (config.preview) {
		DialogBoxW(GetModuleHandleW(auo_filename.c_str()), L"PREVIEW", GetActiveWindow(), func_preview_proc);
	}
	if (cancel) {
		return TRUE;
	}
	std::ofstream ofs(oip->savefile, std::ios::binary);
	if (!ofs.is_open()) { return FALSE; }
	for ( int i=0; i<oip->n; i++ ) {
		if (oip->func_is_abort()) { break; }
		oip->func_rest_time_disp(i, oip->n);
		std::string str;
		set_estimates(static_cast<unsigned char *>(oip->func_get_video(i)));
		if (config.dialog) {
			if ( config.dialog_always || dialog_flags.unmatch || dialog_flags.cnn_low ) {
				preview_frame = i;
				DialogBoxW(GetModuleHandleW(auo_filename.c_str()), L"CORRECT", GetActiveWindow(), func_correct_proc);
				if (cancel) {
					ofs.close(); return TRUE;
				}
			}
		}
		if (config.frame) {
			str = std::format("{}{:c}{:s}\n", i+config.offset, separators[static_cast<std::size_t>(config.sep_idx)], est_str);
		} else {
			str = std::format("{:s}\n", est_str);
		}
		ofs << str;
	}
	ofs.close();
	return TRUE;
}

// コンフィグ関係
static Separator sep_now=Separator::NONE;
static void
set_offset_enableness(HWND hdlg, BOOL val)
{
	EnableWindow(GetDlgItem(hdlg, IDC_OFFSET), val);
	EnableWindow(GetDlgItem(hdlg, IDC_SPACE), val);
	EnableWindow(GetDlgItem(hdlg, IDC_COMMA), val);
	EnableWindow(GetDlgItem(hdlg, IDC_TAB), val);
}
static void
set_dialog_enableness(HWND hdlg, BOOL val)
{
	EnableWindow(GetDlgItem(hdlg, IDC_DIALOG_EVAL), val);
	EnableWindow(GetDlgItem(hdlg, IDC_DIALOG_EVAL_LIM), val);
}
static void
set_dialog_enableness_ex(HWND hdlg, BOOL val, BOOL val2)
{
	set_dialog_enableness(hdlg, val&&val2);
	EnableWindow(GetDlgItem(hdlg, IDC_DIALOG_ALWAYS), val);
}
static void
init_dialog(HWND hdlg)
{
	std::wstring wstr = std::format(L"{}", config.start_x);
	SetDlgItemTextW(hdlg, IDC_X, wstr.c_str());
	wstr = std::format(L"{}", config.start_y);
	SetDlgItemTextW(hdlg, IDC_Y, wstr.c_str());
	SendMessage(GetDlgItem(hdlg, IDC_PREVIEW), BM_SETCHECK, config.preview, 0);
	SendMessage(GetDlgItem(hdlg, IDC_FRAME), BM_SETCHECK, config.frame, 0);
	wstr = std::format(L"{}", config.offset);
	SetDlgItemTextW(hdlg, IDC_OFFSET, wstr.c_str());
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
	int delim_dec = std::lrint(config.dialog_eval_limit * 1000.0f);
	int delim_int = delim_dec/1000;
	wstr = std::format(L"{}.{:03}", delim_int, delim_dec-(delim_int*1000));
	SetDlgItemTextW(hdlg, IDC_DIALOG_EVAL_LIM, wstr.c_str());
	EnableWindow(GetDlgItem(hdlg, IDC_DIALOG_EVAL_LIM), config.dialog_eval);
	SendMessage(GetDlgItem(hdlg, IDC_DIALOG_ALWAYS), BM_SETCHECK, config.dialog_always, 0);
	set_dialog_enableness(hdlg, !config.dialog_always);
	wstr = std::format(L"{}", config.num_th);
	SetDlgItemTextW(hdlg, IDC_NTH, wstr.c_str());
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
	if ( 8 < nt ) {
		nt = 8;
	}
	n_th = static_cast<std::size_t>(nt);
	if ( !TP || ( TP->get_size() != n_th ) ) {
		TP = std::make_unique<ThreadPool>(n_th);
	}
}
static void
setup_config(HWND hdlg)
{
	std::wstring wstr(15, L'\0');
	GetDlgItemTextW(hdlg, IDC_X, wstr.data(), static_cast<int>(wstr.size()));
	config.start_x = std::stoi(wstr);
	GetDlgItemTextW(hdlg, IDC_Y, wstr.data(), static_cast<int>(wstr.size()));
	config.start_y = std::stoi(wstr);
	config.preview = SendDlgItemMessageW(hdlg, IDC_PREVIEW, BM_GETCHECK, 0, 0);
	config.frame = SendDlgItemMessageW(hdlg, IDC_FRAME, BM_GETCHECK, 0, 0);
	GetDlgItemTextW(hdlg, IDC_OFFSET, wstr.data(), static_cast<int>(wstr.size()));
	config.offset = std::stoi(wstr);
	config.sep_idx = sep_now;
	config.dialog = SendDlgItemMessageW(hdlg, IDC_DIALOG, BM_GETCHECK, 0, 0);
	config.dialog_eval = SendDlgItemMessageW(hdlg, IDC_DIALOG_EVAL, BM_GETCHECK, 0, 0);
	GetDlgItemTextW(hdlg, IDC_DIALOG_EVAL_LIM, wstr.data(), static_cast<int>(wstr.size()));
	config.dialog_eval_limit = std::stof(wstr);
	config.dialog_always = SendDlgItemMessageW(hdlg, IDC_DIALOG_ALWAYS, BM_GETCHECK, 0, 0);
	GetDlgItemTextW(hdlg, IDC_NTH, wstr.data(), static_cast<int>(wstr.size()));
	config.num_th = std::stoi(wstr);
	n_th_correction();
}
static INT_PTR CALLBACK
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
				SendDlgItemMessageW(hdlg, IDC_DIALOG, BM_GETCHECK, 0, 0),
				!SendDlgItemMessageW(hdlg, IDC_DIALOG_ALWAYS, BM_GETCHECK, 0, 0));
		} else if (lwparam == IDC_DIALOG_EVAL) {
			EnableWindow(GetDlgItem(hdlg, IDC_DIALOG_EVAL_LIM), SendDlgItemMessageW(hdlg, IDC_DIALOG_EVAL, BM_GETCHECK, 0, 0));
		} else if (lwparam == IDC_DIALOG_ALWAYS) {
			set_dialog_enableness(hdlg, !SendDlgItemMessageW(hdlg, IDC_DIALOG_ALWAYS, BM_GETCHECK, 0, 0));
		}
		return TRUE;
	}
	return FALSE;
}
BOOL
func_config(HWND hwnd, HINSTANCE dll_hinst)
{
	DialogBoxW(dll_hinst, L"CONFIG", hwnd, func_config_proc);
	return TRUE;
}
int
func_config_get(void *data, int size)
{
	if (data) {
		memcpy(data, &config, sizeof(config));
		return sizeof(config);
	}
	return 0;
}
int
func_config_set(void *data, int size)
{
	if (size != sizeof(config)) {
		return 0;
	}
	memcpy(&config, data, size);
	n_th_correction();
	return size;
}

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
	invoke(std::size_t i)
	{
		const std::size_t j=i%4uz;
		const unsigned char *s = &src[j*width*3uz];
		if (i<4uz) {
			cnn[j].predict(s);
		} else {
			dnn[j].predict(s);
		}
	}
};

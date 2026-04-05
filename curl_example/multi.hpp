#ifndef TINY_CORO_CURL_EXAMPLE_MULTI_HPP
#define TINY_CORO_CURL_EXAMPLE_MULTI_HPP

#include <curl/curl.h>
#include <tinycoro/tinycoro_all.h>

#include <cassert>

struct CurlMulti
{
    CurlMulti()
    : _multi{curl_multi_init()}
    {
        assert(_multi);
    }

    CurlMulti(CurlMulti&&) = delete;

    ~CurlMulti() { curl_multi_cleanup(_multi); }

    auto AddEasyHandle(CURL* easy) -> tinycoro::Task<>
    {
        // Queue new work from request coroutines; Run() will attach it to libcurl on its next iteration.
        co_await _channel.PushWait(easy);
        Notify();
    }

    void Notify() noexcept { curl_multi_wakeup(_multi); }

    auto Run() -> tinycoro::Task<>
    {
        int easy_left = 0;
        for (;;)
        {
            CURL* easy{};
            while (_channel.TryPop(easy))
                curl_multi_add_handle(_multi, easy);

            // Block until libcurl has network activity, or until Notify() wakes the loop up.
            CURLMcode mc = curl_multi_poll(_multi, nullptr, 0, 1000, nullptr);
            if (mc != CURLM_OK)
            {
                std::cerr << "curl_multi_poll failed: " << curl_multi_strerror(mc) << "\n";
                break;
            }

            // Give TinyCoro a suspend point so cancellation stays responsive while requests are in flight.
            easy_left ? co_await tinycoro::this_coro::yield() : co_await tinycoro::this_coro::yield_cancellable();

            // Let libcurl advance all active requests.
            mc = curl_multi_perform(_multi, &easy_left);
            if (mc != CURLM_OK)
            {
                std::cerr << "curl_multi_perform failed: " << curl_multi_strerror(mc) << "\n";
                break;
            }

            int msgs_left = 0;
            while (CURLMsg* msg = curl_multi_info_read(_multi, &msgs_left))
            {
                if (msg->msg == CURLMSG_DONE)
                {
                    long http_code = 0;
                    curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &http_code);

                    std::cout << "result=" << curl_easy_strerror(msg->data.result) << ", http=" << http_code << "\n";

                    tinycoro::AutoEvent* event{};
                    // Recover the coroutine-specific event stored in CURLOPT_PRIVATE and wake that waiter.
                    curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &event);

                    assert(event);
                    event->Set();

                    curl_multi_remove_handle(_multi, msg->easy_handle);
                }
            }
        }
    }

private:
    CURLM*                           _multi{nullptr};
    tinycoro::BufferedChannel<CURL*> _channel;
};

#endif // TINY_CORO_CURL_EXAMPLE_MULTI_HPP

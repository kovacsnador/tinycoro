#ifndef TINY_CORO_CURL_EXAMPLE_EASY_HPP
#define TINY_CORO_CURL_EXAMPLE_EASY_HPP

#include <curl/curl.h>
#include <tinycoro/tinycoro_all.h>

#include <string_view>
#include <string>
#include <cassert>

struct Easy
{
    explicit Easy(std::string_view url)
    : _easy(curl_easy_init())
    {
        assert(_easy);

        // Configure one HTTP request and tell libcurl where to store the response body.
        curl_easy_setopt(_easy, CURLOPT_URL, url.data());
        curl_easy_setopt(_easy, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(_easy, CURLOPT_WRITEDATA, std::addressof(_body));
        curl_easy_setopt(_easy, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(_easy, CURLOPT_NOSIGNAL, 0L);
        // Keep a pointer to our completion event so the multi loop can wake this coroutine up.
        curl_easy_setopt(_easy, CURLOPT_PRIVATE, std::addressof(_event));
        curl_easy_setopt(_easy, CURLOPT_TIMEOUT_MS, 3000L);
        curl_easy_setopt(_easy, CURLOPT_CONNECTTIMEOUT_MS, 2000L);
    }

    Easy(Easy&&) = delete;

    ~Easy() { curl_easy_cleanup(_easy); }

    auto Fetch(auto& multi) -> tinycoro::Task<const std::string&>
    {
        assert(_easy);

        _body.clear();

        // Hand the easy handle to the shared multi handle, which actually performs the I/O.
        co_await multi.AddEasyHandle(_easy);

        // Suspend until CurlMulti marks this request as finished.
        co_await _event.Wait();
        co_return _body;
    };

private:
    static auto write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t
    {
        // libcurl may deliver the response in chunks, so append each chunk to the output string.
        auto* out = static_cast<std::string*>(userdata);
        out->append(ptr, size * nmemb);
        return size * nmemb;
    }

    std::string         _body{};
    CURL*               _easy{nullptr};
    tinycoro::AutoEvent _event{};
};

auto FetchUrl(std::string_view url, auto& multi) -> tinycoro::Task<std::string>
{
    // Keep the easy handle alive for the whole request, then return the collected response body.
    Easy easy{url};
    std::string result = co_await easy.Fetch(multi);
    co_return result;
}

#endif // TINY_CORO_CURL_EXAMPLE_EASY_HPP
